#include "platform/process.h"
#include <cerrno>

#ifdef _WIN32

#include <windows.h>

namespace {

// Quote one argument according to the rules the CRT uses to split a command
// line back into argv. Backslashes are only special in front of a quote.
std::string quote_argument(const std::string& argument) {
    if (!argument.empty() && argument.find_first_of(" \t\n\v\"") == std::string::npos) {
        return argument;
    }

    std::string quoted = "\"";
    for (size_t i = 0;; i++) {
        int backslashes = 0;
        while (i < argument.size() && argument[i] == '\\') {
            i++;
            backslashes++;
        }

        if (i == argument.size()) {
            quoted.append((size_t)backslashes * 2, '\\');
            break;
        }

        if (argument[i] == '"') {
            quoted.append((size_t)backslashes * 2 + 1, '\\');
        } else {
            quoted.append((size_t)backslashes, '\\');
        }
        quoted.push_back(argument[i]);
    }
    quoted.push_back('"');
    return quoted;
}

class windows_subprocess : public subprocess {
  public:
    windows_subprocess(HANDLE process, HANDLE stdin_write)
        : process_(process),
          stdin_write_(stdin_write) {}

    ~windows_subprocess() override {
        if (stdin_write_ != INVALID_HANDLE_VALUE) {
            CloseHandle(stdin_write_);
        }
        if (process_ != INVALID_HANDLE_VALUE) {
            CloseHandle(process_);
        }
    }

    bool write(const void* data, size_t size) override {
        constexpr size_t MAX_CHUNK = 1024 * 1024;
        const char* bytes = (const char*)data;
        while (size > 0) {
            DWORD chunk = (DWORD)(size > MAX_CHUNK ? MAX_CHUNK : size);
            DWORD written = 0;
            if (!WriteFile(stdin_write_, bytes, chunk, &written, nullptr) || written == 0) {
                return false;
            }
            bytes += written;
            size -= written;
        }
        return true;
    }

    int close_and_wait() override {
        if (stdin_write_ != INVALID_HANDLE_VALUE) {
            CloseHandle(stdin_write_);
            stdin_write_ = INVALID_HANDLE_VALUE;
        }

        WaitForSingleObject(process_, INFINITE);

        DWORD exit_code = 0;
        if (!GetExitCodeProcess(process_, &exit_code)) {
            return -1;
        }
        return (int)exit_code;
    }

  private:
    HANDLE process_;
    HANDLE stdin_write_;
};

} // namespace

std::unique_ptr<subprocess> subprocess::spawn_with_stdin(const std::vector<std::string>& argv) {
    if (argv.empty()) {
        return nullptr;
    }

    SECURITY_ATTRIBUTES attributes{};
    attributes.nLength = sizeof(attributes);
    attributes.bInheritHandle = TRUE;

    HANDLE read_end = INVALID_HANDLE_VALUE;
    HANDLE write_end = INVALID_HANDLE_VALUE;
    if (!CreatePipe(&read_end, &write_end, &attributes, 0)) {
        return nullptr;
    }

    // Our end of the pipe must not leak into the child, or the child would
    // never see end of file.
    if (!SetHandleInformation(write_end, HANDLE_FLAG_INHERIT, 0)) {
        CloseHandle(read_end);
        CloseHandle(write_end);
        return nullptr;
    }

    std::string command_line;
    for (const std::string& argument : argv) {
        if (!command_line.empty()) {
            command_line.push_back(' ');
        }
        command_line += quote_argument(argument);
    }

    STARTUPINFOA startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = read_end;
    startup.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    startup.hStdError = GetStdHandle(STD_ERROR_HANDLE);

    PROCESS_INFORMATION information{};
    BOOL started = CreateProcessA(nullptr, command_line.data(), nullptr, nullptr, TRUE, 0, nullptr,
                                  nullptr, &startup, &information);

    CloseHandle(read_end);

    if (!started) {
        CloseHandle(write_end);
        return nullptr;
    }

    CloseHandle(information.hThread);
    return std::make_unique<windows_subprocess>(information.hProcess, write_end);
}

#else

#include <csignal>
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

namespace {

class posix_subprocess : public subprocess {
  public:
    posix_subprocess(pid_t pid, int stdin_write)
        : pid_(pid),
          stdin_write_(stdin_write) {}

    ~posix_subprocess() override {
        if (stdin_write_ >= 0) {
            close(stdin_write_);
        }
        if (pid_ > 0) {
            int status = 0;
            waitpid(pid_, &status, 0);
        }
    }

    bool write(const void* data, size_t size) override {
        const char* bytes = (const char*)data;
        while (size > 0) {
            ssize_t written = ::write(stdin_write_, bytes, size);
            if (written < 0) {
                if (errno == EINTR) {
                    continue;
                }
                return false;
            }
            bytes += written;
            size -= (size_t)written;
        }
        return true;
    }

    int close_and_wait() override {
        if (stdin_write_ >= 0) {
            close(stdin_write_);
            stdin_write_ = -1;
        }

        int status = 0;
        while (waitpid(pid_, &status, 0) < 0) {
            if (errno != EINTR) {
                pid_ = -1;
                return -1;
            }
        }
        pid_ = -1;

        if (!WIFEXITED(status)) {
            return -1;
        }
        return WEXITSTATUS(status);
    }

  private:
    pid_t pid_;
    int stdin_write_;
};

} // namespace

std::unique_ptr<subprocess> subprocess::spawn_with_stdin(const std::vector<std::string>& argv) {
    if (argv.empty()) {
        return nullptr;
    }

    int pipe_ends[2];
    if (pipe(pipe_ends) != 0) {
        return nullptr;
    }

    // Closed by a successful exec, so the parent can tell whether the program
    // was actually started instead of only finding out from the exit status.
    int exec_status[2];
    if (pipe(exec_status) != 0) {
        close(pipe_ends[0]);
        close(pipe_ends[1]);
        return nullptr;
    }
    fcntl(exec_status[1], F_SETFD, FD_CLOEXEC);

    pid_t pid = fork();
    if (pid < 0) {
        close(pipe_ends[0]);
        close(pipe_ends[1]);
        close(exec_status[0]);
        close(exec_status[1]);
        return nullptr;
    }

    if (pid == 0) {
        // Child: hook the read end up to stdin and hand over to the program.
        close(pipe_ends[1]);
        close(exec_status[0]);
        if (dup2(pipe_ends[0], STDIN_FILENO) < 0) {
            _exit(127);
        }
        close(pipe_ends[0]);

        std::vector<char*> arguments;
        arguments.reserve(argv.size() + 1);
        for (const std::string& argument : argv) {
            arguments.push_back(const_cast<char*>(argument.c_str()));
        }
        arguments.push_back(nullptr);

        execvp(arguments[0], arguments.data());

        int error = errno;
        ssize_t ignored = ::write(exec_status[1], &error, sizeof(error));
        (void)ignored;
        _exit(127);
    }

    close(pipe_ends[0]);
    close(exec_status[1]);

    int exec_error = 0;
    ssize_t read_bytes = 0;
    do {
        read_bytes = read(exec_status[0], &exec_error, sizeof(exec_error));
    } while (read_bytes < 0 && errno == EINTR);
    close(exec_status[0]);

    if (read_bytes > 0) {
        // exec failed, so reap the child and report the failure to start
        close(pipe_ends[1]);
        int status = 0;
        while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {
        }
        return nullptr;
    }

    // A dead child must fail our writes rather than kill us.
    signal(SIGPIPE, SIG_IGN);

    return std::make_unique<posix_subprocess>(pid, pipe_ends[1]);
}

#endif
