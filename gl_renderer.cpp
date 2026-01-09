#include "gl_renderer.h"
#include "main.h"

#include <SDL.h>
#include <glad/glad.h>
#include <cstring>
#include <cstdio>

static const char* vertexShaderSource = R"(
#version 410 core
layout (location = 0) in vec2 position;
layout (location = 1) in vec2 texCoord;
out vec2 fragTexCoord;

void main() {
    gl_Position = vec4(position, 0.0, 1.0);
    fragTexCoord = texCoord;
}
)";

static const char* fragmentShaderSource = R"(
#version 410 core
in vec2 fragTexCoord;
out vec4 FragColor;
uniform usampler2D indexTexture;
uniform sampler2D paletteTexture;

void main() {
    uint index = texture(indexTexture, fragTexCoord).r;
    // Index is 0-255, normalize to [0,1] for palette lookup
    // Add 0.5 to sample from texel center
    float u = (float(index) + 0.5) / 256.0;
    FragColor = texture(paletteTexture, vec2(u, 0.5));
}
)";

static SDL_GLContext glContext = nullptr;
static GLuint shaderProgram = 0;
static GLuint indexTexture = 0;
static GLuint paletteTexture = 0;
static GLuint vao = 0;
static GLuint vbo = 0;
static GLuint pbo = 0;
static int frameWidth = 0;
static int frameHeight = 0;
static unsigned int currentPalette[256];
static GLint indexTexLoc = -1;
static GLint paletteTexLoc = -1;

static GLuint compileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    if (shader == 0) {
        return 0;
    }
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        internal_error("Shader compilation failed:", infoLog);
        return 0;
    }
    return shader;
}

int gl_init(SDL_Window* sdlWindow, int width, int height) {
    frameWidth = width;
    frameHeight = height;

    glContext = SDL_GL_CreateContext(sdlWindow);
    if (!glContext) {
        internal_error("Failed to create OpenGL context:", SDL_GetError());
        return -1;
    }

    if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) {
        internal_error("Failed to initialize GLAD");
        return -1;
    }

    glViewport(0, 0, width, height);

    SDL_GL_SetSwapInterval(0);

    // Compile shaders
    GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexShaderSource);
    GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);
    if (!vertexShader || !fragmentShader) {
        return -1;
    }

    // Link shader program
    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    GLint success;
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(shaderProgram, 512, nullptr, infoLog);
        internal_error("Shader linking failed:", infoLog);
        return -1;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    glUseProgram(shaderProgram);
    indexTexLoc = glGetUniformLocation(shaderProgram, "indexTexture");
    paletteTexLoc = glGetUniformLocation(shaderProgram, "paletteTexture");
    glUniform1i(indexTexLoc, 0);
    glUniform1i(paletteTexLoc, 1);

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    float vertices[] = {// positions   // texCoords
                        -1.0f, 1.0f, 0.0f, 0.0f, -1.0f, -1.0f, 0.0f, 1.0f, 1.0f, -1.0f, 1.0f, 1.0f,
                        -1.0f, 1.0f, 0.0f, 0.0f, 1.0f,  -1.0f, 1.0f, 1.0f, 1.0f, 1.0f,  1.0f, 0.0f};

    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

    // Create index texture (R8UI) - unsigned integer format to preserve exact indices
    glGenTextures(1, &indexTexture);
    glBindTexture(GL_TEXTURE_2D, indexTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8UI, width, height, 0, GL_RED_INTEGER, GL_UNSIGNED_BYTE,
                 nullptr);

    // Create palette texture (256x1 RGBA)
    glGenTextures(1, &paletteTexture);
    glBindTexture(GL_TEXTURE_2D, paletteTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

#ifdef __APPLE__
    glGenBuffers(1, &pbo);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
    glBufferData(GL_PIXEL_UNPACK_BUFFER, width * height, nullptr, GL_STREAM_DRAW);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
#endif

    memset(currentPalette, 0, sizeof(currentPalette));

    glUseProgram(shaderProgram);
    glBindVertexArray(vao);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, indexTexture);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, paletteTexture);

    return 0;
}

void gl_upload_frame(const unsigned char* indices, int pitch) {
#ifdef __APPLE__
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);

    void* ptr = glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
    if (ptr) {
        memcpy(ptr, indices, frameWidth * frameHeight);
        glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
    }

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, indexTexture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, frameWidth, frameHeight, GL_RED_INTEGER,
                    GL_UNSIGNED_BYTE, 0);

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
#else
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, indexTexture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, frameWidth, frameHeight, GL_RED_INTEGER,
                    GL_UNSIGNED_BYTE, indices);
#endif
}

void gl_update_palette(const unsigned int* palette) {
    unsigned char rgba_palette[256 * 4];
    for (int i = 0; i < 256; i++) {
        unsigned int color = palette[i];
        rgba_palette[i * 4 + 0] = (color >> 16) & 0xFF; // R
        rgba_palette[i * 4 + 1] = (color >> 8) & 0xFF;  // G
        rgba_palette[i * 4 + 2] = color & 0xFF;         // B
        rgba_palette[i * 4 + 3] = (color >> 24) & 0xFF; // A
    }
    memcpy(currentPalette, palette, sizeof(currentPalette));

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, paletteTexture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 1, GL_RGBA, GL_UNSIGNED_BYTE, rgba_palette);
}

void gl_present() { glDrawArrays(GL_TRIANGLES, 0, 6); }

void gl_cleanup() {
    if (pbo) {
        glDeleteBuffers(1, &pbo);
        pbo = 0;
    }
    if (indexTexture) {
        glDeleteTextures(1, &indexTexture);
        indexTexture = 0;
    }
    if (paletteTexture) {
        glDeleteTextures(1, &paletteTexture);
        paletteTexture = 0;
    }
    if (vbo) {
        glDeleteBuffers(1, &vbo);
        vbo = 0;
    }
    if (vao) {
        glDeleteVertexArrays(1, &vao);
        vao = 0;
    }
    if (shaderProgram) {
        glDeleteProgram(shaderProgram);
        shaderProgram = 0;
    }
    if (glContext) {
        SDL_GL_DeleteContext(glContext);
        glContext = nullptr;
    }
}
