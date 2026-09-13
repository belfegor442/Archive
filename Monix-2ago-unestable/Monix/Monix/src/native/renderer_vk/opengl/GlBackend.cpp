#include "GlBackend.hpp"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <GL/gl.h>
#include <GL/glext.h>

#ifndef GL_VERTEX_SHADER
#define GL_VERTEX_SHADER 0x8B31
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_INFO_LOG_LENGTH 0x8B84
#define GL_TEXTURE_2D 0x0DE1
#define GL_RGBA 0x1908
#define GL_RGBA8 0x8058
#define GL_RGBA32F 0x8814
#define GL_UNSIGNED_BYTE 0x1401
#define GL_FLOAT 0x1406
#define GL_LINEAR 0x2601
#define GL_NEAREST 0x2600
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_TEXTURE_WRAP_S 0x2802
#define GL_TEXTURE_WRAP_T 0x2803
#define GL_CLAMP_TO_EDGE 0x812F
#define GL_CLAMP_TO_BORDER 0x812D
#define GL_COLOR_BUFFER_BIT 0x00004000
#define GL_ARRAY_BUFFER 0x8892
#define GL_STATIC_DRAW 0x88E4
#define GL_FRAMEBUFFER 0x8D40
#define GL_COLOR_ATTACHMENT0 0x8CE0
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5
#define GL_TEXTURE0 0x84C0
#define GL_TRIANGLES 0x0004
#define GL_FALSE 0
#define GL_TRUE 1
#endif

#ifndef WGL_CONTEXT_MAJOR_VERSION_ARB
#define WGL_CONTEXT_MAJOR_VERSION_ARB 0x2091
#define WGL_CONTEXT_MINOR_VERSION_ARB 0x2092
#define WGL_CONTEXT_PROFILE_MASK_ARB  0x9126
#define WGL_CONTEXT_CORE_PROFILE_BIT_ARB 0x00000001
#endif

typedef HGLRC(WINAPI* PFNWGLCREATECONTEXTATTRIBSARBPROC)(HDC, HGLRC, const int*);

#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <vector>

#pragma comment(lib, "opengl32.lib")

namespace monix::renderer_vk {

static constexpr const wchar_t* kGlWndClass = L"MonixGlOffscreen";

static LRESULT CALLBACK GlOffscreenWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    return DefWindowProcW(hwnd, msg, wp, lp);
}

GlBackend::~GlBackend() { shutdown(); }

bool GlBackend::createHiddenWindow(HWND parentHwnd) {
    HINSTANCE hInst = GetModuleHandleW(nullptr);
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_OWNDC;
    wc.lpfnWndProc = GlOffscreenWndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = kGlWndClass;
    RegisterClassExW(&wc);

    hiddenHwnd_ = CreateWindowExW(0, kGlWndClass, L"",
        WS_POPUP, 0, 0, 2, 2, parentHwnd, nullptr, hInst, nullptr);
    return hiddenHwnd_ != nullptr;
}

bool GlBackend::createGlContext() {
    glDc_ = GetDC(hiddenHwnd_);

    PIXELFORMATDESCRIPTOR pfd{};
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.iLayerType = PFD_MAIN_PLANE;

    int pf = ChoosePixelFormat(glDc_, &pfd);
    if (!pf) return false;
    SetPixelFormat(glDc_, pf, &pfd);

    HGLRC tempRc = wglCreateContext(glDc_);
    if (!tempRc) return false;
    wglMakeCurrent(glDc_, tempRc);

    auto p_wglCreateContextAttribsARB = (PFNWGLCREATECONTEXTATTRIBSARBPROC)
        wglGetProcAddress("wglCreateContextAttribsARB");

    if (p_wglCreateContextAttribsARB) {
        int attribs[] = {
            WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
            WGL_CONTEXT_MINOR_VERSION_ARB, 3,
            WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
            0
        };
        glRc_ = p_wglCreateContextAttribsARB(glDc_, nullptr, attribs);
        if (glRc_) {
            wglMakeCurrent(nullptr, nullptr);
            wglDeleteContext(tempRc);
            wglMakeCurrent(glDc_, glRc_);
        } else {
            glRc_ = tempRc;
        }
    } else {
        glRc_ = tempRc;
    }

    glContextValid_ = true;
    return true;
}

#define LOAD_GL(name) fn_##name = decltype(fn_##name)(wglGetProcAddress(#name))

void GlBackend::loadGlFunctions() {
    LOAD_GL(glCreateShader);
    LOAD_GL(glShaderSource);
    LOAD_GL(glCompileShader);
    LOAD_GL(glGetShaderiv);
    LOAD_GL(glGetShaderInfoLog);
    LOAD_GL(glDeleteShader);
    LOAD_GL(glCreateProgram);
    LOAD_GL(glAttachShader);
    LOAD_GL(glLinkProgram);
    LOAD_GL(glGetProgramiv);
    LOAD_GL(glGetProgramInfoLog);
    LOAD_GL(glUseProgram);
    LOAD_GL(glDeleteProgram);
    LOAD_GL(glGetUniformLocation);
    LOAD_GL(glBindAttribLocation);
    LOAD_GL(glUniform1i);
    LOAD_GL(glUniform1f);
    LOAD_GL(glUniform2f);
    LOAD_GL(glUniformMatrix4fv);
    LOAD_GL(glActiveTexture);
    LOAD_GL(glGenFramebuffers);
    LOAD_GL(glBindFramebuffer);
    LOAD_GL(glFramebufferTexture2D);
    LOAD_GL(glCheckFramebufferStatus);
    LOAD_GL(glDeleteFramebuffers);
    LOAD_GL(glGenVertexArrays);
    LOAD_GL(glBindVertexArray);
    LOAD_GL(glDeleteVertexArrays);
    LOAD_GL(glGenBuffers);
    LOAD_GL(glBindBuffer);
    LOAD_GL(glBufferData);
    LOAD_GL(glDeleteBuffers);
    LOAD_GL(glEnableVertexAttribArray);
    LOAD_GL(glVertexAttribPointer);

    functionsLoaded_ = fn_glCreateShader && fn_glCompileShader && fn_glCreateProgram &&
                       fn_glLinkProgram && fn_glGenFramebuffers && fn_glBindFramebuffer &&
                       fn_glFramebufferTexture2D && fn_glGenVertexArrays && fn_glGenBuffers;
}
#undef LOAD_GL

bool GlBackend::initialize(HWND parentHwnd) {
    if (initialized_) return true;

    if (!createHiddenWindow(parentHwnd)) return false;
    if (!createGlContext()) return false;
    loadGlFunctions();
    if (!functionsLoaded_) return false;

    wglMakeCurrent(glDc_, glRc_);

    fn_glGenVertexArrays(1, &vao_);
    fn_glBindVertexArray(vao_);

    static const float quad[] = {
        // VertexCoord (vec4), TexCoord (vec4), COLOR (vec4) = 12 floats per vertex
        -1.f, -1.f, 0.f, 1.f,   0.f, 0.f, 0.f, 1.f,   1.f, 1.f, 1.f, 1.f,
         1.f, -1.f, 0.f, 1.f,   1.f, 0.f, 0.f, 1.f,   1.f, 1.f, 1.f, 1.f,
         1.f,  1.f, 0.f, 1.f,   1.f, 1.f, 0.f, 1.f,   1.f, 1.f, 1.f, 1.f,
        -1.f, -1.f, 0.f, 1.f,   0.f, 0.f, 0.f, 1.f,   1.f, 1.f, 1.f, 1.f,
         1.f,  1.f, 0.f, 1.f,   1.f, 1.f, 0.f, 1.f,   1.f, 1.f, 1.f, 1.f,
        -1.f,  1.f, 0.f, 1.f,   0.f, 1.f, 0.f, 1.f,   1.f, 1.f, 1.f, 1.f,
    };
    fn_glGenBuffers(1, &vbo_);
    fn_glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    fn_glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    fn_glEnableVertexAttribArray(0);
    fn_glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 12 * sizeof(float), (void*)0);
    fn_glEnableVertexAttribArray(1);
    fn_glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 12 * sizeof(float), (void*)(4 * sizeof(float)));
    fn_glEnableVertexAttribArray(2);
    fn_glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, 12 * sizeof(float), (void*)(8 * sizeof(float)));

    initialized_ = true;
    return true;
}

void GlBackend::shutdown() {
    if (!initialized_) return;

    for (auto& p : compiledPasses_) {
        if (p.program) fn_glDeleteProgram(p.program);
        if (p.fbo) fn_glDeleteFramebuffers(1, &p.fbo);
        if (p.fboTexture) glDeleteTextures(1, &p.fboTexture);
    }
    compiledPasses_.clear();
    passes_.clear();

    if (sourceTexture_) { glDeleteTextures(1, &sourceTexture_); sourceTexture_ = 0; }
    if (vbo_) { fn_glDeleteBuffers(1, &vbo_); vbo_ = 0; }
    if (vao_) { fn_glDeleteVertexArrays(1, &vao_); vao_ = 0; }

    if (glRc_) { wglMakeCurrent(nullptr, nullptr); wglDeleteContext(glRc_); glRc_ = nullptr; }
    if (glDc_ && hiddenHwnd_) { ReleaseDC(hiddenHwnd_, glDc_); glDc_ = nullptr; }
    if (hiddenHwnd_) { DestroyWindow(hiddenHwnd_); hiddenHwnd_ = nullptr; }

    initialized_ = false;
    glContextValid_ = false;
}

GLuint GlBackend::compileShader(const char* source, GLenum type) {
    wglMakeCurrent(glDc_, glRc_);
    GLuint shader = fn_glCreateShader(type);
    if (!shader) return 0;
    fn_glShaderSource(shader, 1, &source, nullptr);
    fn_glCompileShader(shader);
    GLint ok = 0;
    fn_glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[4096] = {};
        fn_glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        fn_glDeleteShader(shader);
        return 0;
    }
    return shader;
}

GLuint GlBackend::buildProgram(GLuint vertShader, GLuint fragShader) {
    GLuint prog = fn_glCreateProgram();
    fn_glAttachShader(prog, vertShader);
    fn_glAttachShader(prog, fragShader);
    fn_glBindAttribLocation(prog, 0, "VertexCoord");
    fn_glBindAttribLocation(prog, 1, "TexCoord");
    fn_glBindAttribLocation(prog, 2, "COLOR");
    fn_glLinkProgram(prog);
    GLint ok = 0;
    fn_glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[2048];
        fn_glGetProgramInfoLog(prog, sizeof(log), nullptr, log);
        FILE* f = nullptr;
        fopen_s(&f, "build\\gl_frame.log", "w");
        if (f) { fprintf(f, "LINK FAILED\n%s\n", log); fclose(f); }
        fn_glDeleteProgram(prog);
        return 0;
    }
    return prog;
}

GLuint GlBackend::createFboTexture(uint32_t w, uint32_t h, bool isFloat) {
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    GLenum internalFmt = isFloat ? GL_RGBA32F : GL_RGBA8;
    glTexImage2D(GL_TEXTURE_2D, 0, internalFmt, w, h, 0, GL_RGBA, isFloat ? GL_FLOAT : GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return tex;
}

bool GlBackend::splitGlslSource(const std::string& fullSource,
                                std::string& vertexSource,
                                std::string& fragmentSource,
                                std::unordered_map<std::string, float>& paramDefaults) {
    bool inVertex = false;
    bool inFragment = false;
    int depth = 0;
    std::string vertLines;
    std::string fragLines;

    std::istringstream stream(fullSource);
    std::string line;

    while (std::getline(stream, line)) {
        auto trimmed = line;
        auto p = trimmed.find_first_not_of(" \t\r");
        if (p != std::string::npos) trimmed = trimmed.substr(p);

        if (trimmed.rfind("#pragma parameter", 0) == 0) {
            std::istringstream ss(trimmed);
            std::string pragma, parameter, varName, label;
            float defaultVal = 0.f;
            ss >> pragma >> parameter >> varName;
            char next = 0;
            while (ss.get(next) && (next == ' ' || next == '\t')) {}
            if (next == '"') { std::getline(ss, label, '"'); }
            else if (next != 0) ss.putback(next);
            if (!(ss >> defaultVal)) defaultVal = 0.f;
            if (!varName.empty()) {
                paramDefaults[varName] = defaultVal;
            }
            continue;
        }

        if (line.find("#if defined(VERTEX)") != std::string::npos ||
            line.find("#ifdef VERTEX") != std::string::npos) {
            inVertex = true;
            inFragment = false;
            depth = 0;
            continue;
        }
        if (line.find("#elif defined(FRAGMENT)") != std::string::npos ||
            line.find("#ifdef FRAGMENT") != std::string::npos) {
            if (inVertex) {
                inVertex = false;
                inFragment = true;
                depth = 0;
                continue;
            }
        }

        if (!inVertex && !inFragment) {
            continue;
        }

        if (inVertex || inFragment) {
            if (line.find("#if ") == 0 || line.find("#ifdef ") == 0 || line.find("#ifndef ") == 0) {
                depth++;
            }
            if (line.find("#endif") != std::string::npos) {
                if (depth == 0) {
                    inVertex = false;
                    inFragment = false;
                    continue;
                }
                depth--;
            }

            if (inVertex) vertLines += line + "\n";
            else if (inFragment) fragLines += line + "\n";
        }
    }

    if (vertLines.empty() || fragLines.empty()) return false;

    vertexSource =
        "#version 330 core\n"
        "\n"
        "#define PARAMETER_UNIFORM\n"
        "#define COMPAT_PRECISION\n"
        "#define COMPAT_VARYING out\n"
        "#define COMPAT_ATTRIBUTE in\n"
        "#define COMPAT_TEXTURE texture\n"
        "\n"
        + vertLines;

    fragmentSource =
        "#version 330 core\n"
        "\n"
        "#define PARAMETER_UNIFORM\n"
        "#define COMPAT_PRECISION\n"
        "#define COMPAT_VARYING in\n"
        "#define COMPAT_TEXTURE texture\n"
        "#define FINEMASK\n"
        "\n"
        + fragLines;

    return true;
}

bool GlBackend::compilePassShader(size_t passIndex) {
    if (passIndex >= compiledPasses_.size()) return false;
    auto& pass = compiledPasses_[passIndex];
    if (pass.program) return true;

    auto& config = passes_[passIndex];

    std::ifstream file(config.shaderPath);
    if (!file.is_open()) return false;

    std::string fullSource((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());

    std::string vertSrc, fragSrc;
    if (!splitGlslSource(fullSource, vertSrc, fragSrc, pass.paramDefaults)) return false;

    GLuint vert = compileShader(vertSrc.c_str(), GL_VERTEX_SHADER);
    GLuint frag = compileShader(fragSrc.c_str(), GL_FRAGMENT_SHADER);

    if (!vert || !frag) {
        char log[4096] = {};
        FILE* f = nullptr;
        fopen_s(&f, "build\\gl_frame.log", "w");
        if (f) {
            if (!vert) fprintf(f, "VERT FAILED\n%s\n", vertSrc.c_str());
            if (!frag) fprintf(f, "FRAG FAILED\n%s\n", fragSrc.c_str());
            fclose(f);
        }
        fn_glDeleteShader(vert);
        fn_glDeleteShader(frag);
        return false;
    }

    pass.program = buildProgram(vert, frag);
    fn_glDeleteShader(vert);
    fn_glDeleteShader(frag);

    return pass.program != 0;
}

void GlBackend::renderPass(size_t passIndex, GLuint inputTex, GLuint fbo,
                           uint32_t outW, uint32_t outH,
                           uint32_t inW, uint32_t inH) {
    fn_glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, outW, outH);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    auto& pass = compiledPasses_[passIndex];
    fn_glUseProgram(pass.program);

    fn_glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, inputTex);
    fn_glUniform1i(fn_glGetUniformLocation(pass.program, "Texture"), 0);
    fn_glUniform1i(fn_glGetUniformLocation(pass.program, "Texture0"), 0);

    float fw = (float)outW, fh = (float)outH;
    fn_glUniform2f(fn_glGetUniformLocation(pass.program, "OutputSize"), fw, fh);
    fn_glUniform2f(fn_glGetUniformLocation(pass.program, "TextureSize"), (float)inW, (float)inH);
    fn_glUniform2f(fn_glGetUniformLocation(pass.program, "InputSize"), (float)inW, (float)inH);

    GLint loc;
    loc = fn_glGetUniformLocation(pass.program, "FrameDirection");
    if (loc >= 0) fn_glUniform1i(loc, 1);
    loc = fn_glGetUniformLocation(pass.program, "FrameCount");
    if (loc >= 0) fn_glUniform1i(loc, 0);

    loc = fn_glGetUniformLocation(pass.program, "MVPMatrix");
    if (loc >= 0) {
        static const float identity[16] = {
            1.f, 0, 0, 0,
            0, 1.f, 0, 0,
            0, 0, 1.f, 0,
            0, 0, 0, 1.f
        };
        fn_glUniformMatrix4fv(loc, 1, GL_FALSE, identity);
    }

    for (auto& [name, val] : pass.paramDefaults) {
        loc = fn_glGetUniformLocation(pass.program, name.c_str());
        if (loc >= 0) fn_glUniform1f(loc, val);
    }

    fn_glBindVertexArray(vao_);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

bool GlBackend::loadPreset(const GlPresetConfig& config) {
    for (auto& p : compiledPasses_) {
        if (p.program) fn_glDeleteProgram(p.program);
        if (p.fbo) fn_glDeleteFramebuffers(1, &p.fbo);
        if (p.fboTexture) glDeleteTextures(1, &p.fboTexture);
    }
    compiledPasses_.clear();

    passes_ = config.passes;
    compiledPasses_.resize(config.passes.size());

    for (size_t i = 0; i < config.passes.size(); i++) {
        if (!compilePassShader(i)) return false;
    }

    return true;
}

void GlBackend::setSourceTexture(const void* pixels, uint32_t w, uint32_t h) {
    if (!sourceTexture_) {
        glGenTextures(1, &sourceTexture_);
    }
    glBindTexture(GL_TEXTURE_2D, sourceTexture_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

GlReadbackResult GlBackend::execute(const void* sourcePixels, uint32_t srcW, uint32_t srcH) {
    GlReadbackResult result;
    if (!initialized_ || !glContextValid_ || compiledPasses_.empty()) return result;

    wglMakeCurrent(glDc_, glRc_);

    setSourceTexture(sourcePixels, srcW, srcH);

    for (auto& p : compiledPasses_) {
        if (!p.fbo || p.width != srcW || p.height != srcH) {
            if (p.fbo) {
                fn_glDeleteFramebuffers(1, &p.fbo);
                glDeleteTextures(1, &p.fboTexture);
                p.fbo = 0;
                p.fboTexture = 0;
            }
            p.width = srcW;
            p.height = srcH;
            p.fboTexture = createFboTexture(p.width, p.height, false);
            fn_glGenFramebuffers(1, &p.fbo);
            fn_glBindFramebuffer(GL_FRAMEBUFFER, p.fbo);
            fn_glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, p.fboTexture, 0);
        }
    }

    for (size_t i = 0; i < compiledPasses_.size(); i++) {
        GLuint inputTex = (i == 0) ? sourceTexture_ : compiledPasses_[i - 1].fboTexture;
        uint32_t outW = compiledPasses_[i].width;
        uint32_t outH = compiledPasses_[i].height;
        uint32_t inW = (i == 0) ? srcW : compiledPasses_[i - 1].width;
        uint32_t inH = (i == 0) ? srcH : compiledPasses_[i - 1].height;
        renderPass(i, inputTex, compiledPasses_[i].fbo, outW, outH, inW, inH);
    }

    auto& last = compiledPasses_.back();
    fn_glBindFramebuffer(GL_FRAMEBUFFER, last.fbo);
    result.width = last.width;
    result.height = last.height;
    result.pixels.resize(result.width * result.height * 4);
    glReadPixels(0, 0, result.width, result.height, GL_RGBA, GL_UNSIGNED_BYTE, result.pixels.data());

    fn_glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return result;
}

}  // namespace monix::renderer_vk
