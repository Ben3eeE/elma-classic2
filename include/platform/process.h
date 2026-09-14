#ifndef PLATFORM_PROCESS_H
#define PLATFORM_PROCESS_H

#include <memory>
#include <string>
#include <vector>

// A child process with its standard input connected to a pipe.
class subprocess {
  public:
    virtual ~subprocess() = default;

    subprocess(const subprocess&) = delete;
    subprocess& operator=(const subprocess&) = delete;

    // Spawn argv[0] with the remaining entries as its arguments. The arguments
    // are passed through verbatim, without a shell, so they may contain spaces
    // and other characters a shell would interpret. Returns nullptr if the
    // process could not be started.
    static std::unique_ptr<subprocess> spawn_with_stdin(const std::vector<std::string>& argv);

    // Write to the child's standard input. Returns false if the pipe broke.
    virtual bool write(const void* data, size_t size) = 0;

    // Close the pipe, wait for the child to exit and return its exit code.
    // Returns -1 if the child did not exit normally.
    virtual int close_and_wait() = 0;

  protected:
    subprocess() = default;
};

#endif
