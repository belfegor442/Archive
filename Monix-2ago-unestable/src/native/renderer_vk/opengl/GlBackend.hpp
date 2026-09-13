#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <GL/gl.h>
#include <GL/glext.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace monix::renderer_vk {

struct GlPassConfig {
    std::filesystem::path shaderPath;
    float scaleX = 1.0f;
    float scaleY = 1.0f;
    bool linearFilter = true;
    bool floatFramebuffer = false;
};

struct GlPresetConfig {
    std::vector<GlPassConfig> passes;
    struct TextureEntry {
        std::filesystem::path path;
        bool linear = true;
        bool mipmap = false;
        bool repeat = false;
    };
    std::unordered_map<std::string, TextureEntry> textures;
};

struct GlReadbackResult {
    std::vector<uint8_t> pixels;
    uint32_t width = 0;
    uint32_t height = 0;
};

class GlBackend {
public:
    GlBackend() = default;
    ~GlBackend();

    bool initialize(HWND parentHwnd);
    void shutdown();
    bool isInitialized() const { return initialized_; }

    bool loadPreset(const GlPresetConfig& config);
    GlReadbackResult execute(const void* sourcePixels, uint32_t srcW, uint32_t srcH);

    void setSourceTexture(const void* pixels, uint32_t w, uint32_t h);
    bool isValid() const { return initialized_ && glContextValid_; }

private:
    bool createHiddenWindow(HWND parentHwnd);
    bool createGlContext();
    void loadGlFunctions();

    GLuint compileShader(const char* source, GLenum type);
    GLuint buildProgram(GLuint vertShader, GLuint fragShader);
    GLuint createFboTexture(uint32_t w, uint32_t h, bool isFloat);

    bool compilePassShader(size_t passIndex);
    void renderPass(size_t passIndex, GLuint inputTex, GLuint fbo, uint32_t w, uint32_t h);

    static bool splitGlslSource(const std::string& fullSource,
                                std::string& vertexSource,
                                std::string& fragmentSource);

    PFNGLCREATESHADERPROC fn_glCreateShader = nullptr;
    PFNGLSHADERSOURCEPROC fn_glShaderSource = nullptr;
    PFNGLCOMPILESHADERPROC fn_glCompileShader = nullptr;
    PFNGLGETSHADERIVPROC fn_glGetShaderiv = nullptr;
    PFNGLGETSHADERINFOLOGPROC fn_glGetShaderInfoLog = nullptr;
    PFNGLDELETESHADERPROC fn_glDeleteShader = nullptr;
    PFNGLCREATEPROGRAMPROC fn_glCreateProgram = nullptr;
    PFNGLATTACHSHADERPROC fn_glAttachShader = nullptr;
    PFNGLLINKPROGRAMPROC fn_glLinkProgram = nullptr;
    PFNGLGETPROGRAMIVPROC fn_glGetProgramiv = nullptr;
    PFNGLGETPROGRAMINFOLOGPROC fn_glGetProgramInfoLog = nullptr;
    PFNGLUSEPROGRAMPROC fn_glUseProgram = nullptr;
    PFNGLDELETEPROGRAMPROC fn_glDeleteProgram = nullptr;
    PFNGLGETUNIFORMLOCATIONPROC fn_glGetUniformLocation = nullptr;
    PFNGLBINDATTRIBLOCATIONPROC fn_glBindAttribLocation = nullptr;
    PFNGLUNIFORM1IPROC fn_glUniform1i = nullptr;
    PFNGLUNIFORM2FPROC fn_glUniform2f = nullptr;
    PFNGLACTIVETEXTUREPROC fn_glActiveTexture = nullptr;
    PFNGLGENFRAMEBUFFERSPROC fn_glGenFramebuffers = nullptr;
    PFNGLBINDFRAMEBUFFERPROC fn_glBindFramebuffer = nullptr;
    PFNGLFRAMEBUFFERTEXTURE2DPROC fn_glFramebufferTexture2D = nullptr;
    PFNGLCHECKFRAMEBUFFERSTATUSPROC fn_glCheckFramebufferStatus = nullptr;
    PFNGLDELETEFRAMEBUFFERSPROC fn_glDeleteFramebuffers = nullptr;
    PFNGLGENVERTEXARRAYSPROC fn_glGenVertexArrays = nullptr;
    PFNGLBINDVERTEXARRAYPROC fn_glBindVertexArray = nullptr;
    PFNGLDELETEVERTEXARRAYSPROC fn_glDeleteVertexArrays = nullptr;
    PFNGLGENBUFFERSPROC fn_glGenBuffers = nullptr;
    PFNGLBINDBUFFERPROC fn_glBindBuffer = nullptr;
    PFNGLBUFFERDATAPROC fn_glBufferData = nullptr;
    PFNGLDELETEBUFFERSPROC fn_glDeleteBuffers = nullptr;
    PFNGLENABLEVERTEXATTRIBARRAYPROC fn_glEnableVertexAttribArray = nullptr;
    PFNGLVERTEXATTRIBPOINTERPROC fn_glVertexAttribPointer = nullptr;

    HWND hiddenHwnd_ = nullptr;
    HDC glDc_ = nullptr;
    HGLRC glRc_ = nullptr;

    bool initialized_ = false;
    bool glContextValid_ = false;
    bool functionsLoaded_ = false;

    struct CompiledPass {
        GLuint program = 0;
        GLuint fbo = 0;
        GLuint fboTexture = 0;
        uint32_t width = 0;
        uint32_t height = 0;
    };
    std::vector<CompiledPass> compiledPasses_;

    std::vector<GlPassConfig> passes_;

    GLuint sourceTexture_ = 0;
    GLuint vao_ = 0;
    GLuint vbo_ = 0;
};

}  // namespace monix::renderer_vk
