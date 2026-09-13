#pragma once
#ifndef GPU_VALIDATOR_HPP
#define GPU_VALIDATOR_HPP
#define GPU_VALIDATOR_HPP_VERSION 1

#include <GL/gl.h>
#include <GL/glext.h>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <sstream>
#include <fstream>
#include <cmath>
#include <ctime>
#include <cstring>
#include <algorithm>
#include <functional>
#include <cstdint>
#include <limits>

struct TextureSnapshot {
    GLuint id;
    GLint  width = 0, height = 0, depth = 0;
    GLint  internalFormat = 0;
    GLint  externalFormat = 0;
    GLint  type = 0;
    GLint  mipLevels = 0;
    GLint  minFilter = 0, magFilter = 0;
    GLint  wrapS = 0, wrapT = 0, wrapR = 0;
    GLfloat borderColor[4] = {0,0,0,0};
    bool   isSRGB = false;
    bool   isRenderTarget = false;
    bool   isFeedback = false;
    std::string origin;
    std::string aliasName;
    GLenum target = GL_TEXTURE_2D;
};

struct UniformSnapshot {
    std::string name;
    GLenum type = 0;
    GLint location = -1;
    GLint arraySize = 1;
    GLfloat floatValue[16] = {};
    GLint intValue[4] = {};
    bool isFloat = false;
    bool hasNaN = false;
    bool hasInf = false;
    bool neverUploaded = true;
    std::string status;
};

struct SamplerSnapshot {
    GLuint unit = 0;
    GLuint textureId = 0;
    TextureSnapshot texture;
    GLenum target = GL_TEXTURE_2D;
    bool hasTexture = true;
    bool isAlias = false;
    bool isFeedback = false;
    std::string aliasName;
};

struct BlendSnapshot {
    bool enabled = false;
    GLenum srcRGB = 0, dstRGB = 0, opRGB = 0;
    GLenum srcAlpha = 0, dstAlpha = 0, opAlpha = 0;
    GLfloat color[4] = {0,0,0,0};
};

struct DepthSnapshot {
    bool enabled = false;
    GLboolean mask = GL_TRUE;
    GLenum func = 0;
};

struct StencilSnapshot {
    bool enabled = false;
    GLenum func = 0;
    GLint ref = 0;
    GLuint mask = 0;
    GLenum fail = 0, zFail = 0, zPass = 0;
};

struct CullSnapshot {
    bool enabled = false;
    GLenum mode = 0;
    GLenum frontFace = 0;
};

struct ScissorSnapshot {
    bool enabled = false;
    GLint x = 0, y = 0;
    GLsizei width = 0, height = 0;
};

struct ViewportSnapshot {
    GLint x = 0, y = 0;
    GLsizei width = 0, height = 0;
};

struct RenderStateSnapshot {
    BlendSnapshot blend;
    DepthSnapshot depth;
    StencilSnapshot stencil;
    CullSnapshot cull;
    ScissorSnapshot scissor;
    ViewportSnapshot viewport;
    GLboolean colorMask[4] = {GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE};
    GLenum frontFace = GL_CCW;
    GLenum polygonMode = GL_FILL;
    GLboolean framebufferSRGB = GL_FALSE;
    GLenum activeTexture = GL_TEXTURE0;
    GLenum drawBuffer = 0;
    GLenum readBuffer = 0;
};

struct FBOAttachment {
    GLenum attachment;
    GLuint textureId;
    GLint level;
    GLint mipLevel;
};

struct GPUSnapshot {
    int passIndex = -1;
    std::string passName;
    GLuint programId = 0;
    bool programValid = false;
    GLuint fboId = 0;
    bool fboComplete = false;
    GLenum fboStatus = 0;
    std::vector<FBOAttachment> fboAttachments;
    RenderStateSnapshot state;
    std::vector<TextureSnapshot> textures;
    std::map<GLuint, size_t> textureIndexById;
    std::map<GLuint, GLuint> textureBindings;
    std::vector<SamplerSnapshot> samplers;
    std::vector<UniformSnapshot> uniforms;
    int uniformCount = 0;
    int uniformCountExpected = 0;
    GLuint64 gpuTimeNs = 0;
    bool hasTimer = false;
    int drawCalls = 0;
    std::vector<std::string> issues;
};

struct ResourceLifetime {
    std::string kind;
    GLuint id;
    std::string label;
    double created = 0;
    double deleted = -1;
    bool leaked = false;
    bool doubleDelete = false;
};

struct RetroArchUniform {
    std::string name;
    GLenum type = 0;
    GLint location = -1;
    GLfloat floatVal[16] = {};
    GLint intVal[4] = {};
};

struct RetroArchTextureInfo {
    GLuint id = 0;
    GLint width = 0, height = 0;
    GLint internalFormat = 0;
    std::string origin;
};

struct RetroArchSampler {
    GLuint unit = 0;
    GLuint textureId = 0;
    std::string origin;
};

struct RetroArchSnapshot {
    std::string label;
    std::vector<RetroArchUniform> uniforms;
    std::vector<RetroArchTextureInfo> textures;
    std::vector<RetroArchSampler> samplers;
    std::map<std::string, std::string> renderState;
    int passCount = 0;
    std::vector<std::string> issues;
    bool loaded = false;
};
struct GpuValidator {
    bool enabled = false;
    bool capturePerPass = true;
    bool validateAfterRender = true;
    bool validateConsistency = true;
    bool enableTimerQueries = true;
    bool enableUniformReadback = true;

    int passCount = 0;
    std::vector<GPUSnapshot> snapshots;
    std::vector<ResourceLifetime> lifetime;
    std::map<GLuint, size_t> activeResources;
    RetroArchSnapshot retroarch;
    std::vector<std::string> globalIssues;
    std::vector<std::string> retroDiffs;
    std::vector<std::string> consistencyIssues;
    std::map<std::string, int> leakSummary;
    int currentPassDrawCalls = 0;

    struct GLFuncs {
        typedef void   (APIENTRY *PFNGLENABLEPROC)(GLenum);
        typedef void   (APIENTRY *PFNGLDISABLEPROC)(GLenum);
        typedef void   (APIENTRY *PFNGLGETINTEGERVPROC)(GLenum, GLint*);
        typedef void   (APIENTRY *PFNGLGETBOOLEANVPROC)(GLenum, GLboolean*);
        typedef void   (APIENTRY *PFNGLGETFLOATVPROC)(GLenum, GLfloat*);
        typedef void   (APIENTRY *PFNGLBINDFRAMEBUFFERPROC)(GLenum, GLuint);
        typedef GLenum (APIENTRY *PFNGLCHECKFRAMEBUFFERSTATUSPROC)(GLenum);
        typedef void   (APIENTRY *PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC)(GLenum, GLenum, GLenum, GLint*);
        typedef void   (APIENTRY *PFNGLGETTEXLEVELPARAMETERIVPROC)(GLenum, GLint, GLenum, GLint*);
        typedef void   (APIENTRY *PFNGLACTIVETEXTUREPROC)(GLenum);
        typedef GLint  (APIENTRY *PFNGLGETUNIFORMLOCATIONPROC)(GLuint, const GLchar*);
        typedef void   (APIENTRY *PFNGLGETUNIFORMFVPROC)(GLuint, GLint, GLfloat*);
        typedef void   (APIENTRY *PFNGLGETUNIFORMIVPROC)(GLuint, GLint, GLint*);
        typedef void   (APIENTRY *PFNGLGETACTIVEUNIFORMPROC)(GLuint, GLuint, GLsizei, GLsizei*, GLint*, GLenum*, GLchar*);
        typedef void   (APIENTRY *PFNGLGETPROGRAMIVPROC)(GLuint, GLenum, GLint*);
        typedef void   (APIENTRY *PFNGLGETTEXPARAMETERIVPROC)(GLenum, GLenum, GLint*);
        typedef void   (APIENTRY *PFNGLGETTEXPARAMETERFVPROC)(GLenum, GLenum, GLfloat*);
        typedef void   (APIENTRY *PFNGLBINDTEXTUREPROC)(GLenum, GLuint);

        PFNGLGETINTEGERVPROC GetIntegerv = nullptr;
        PFNGLGETBOOLEANVPROC GetBooleanv = nullptr;
        PFNGLGETFLOATVPROC GetFloatv = nullptr;
        PFNGLBINDFRAMEBUFFERPROC BindFramebuffer = nullptr;
        PFNGLCHECKFRAMEBUFFERSTATUSPROC CheckFramebufferStatus = nullptr;
        PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC GetFramebufferAttachmentParameteriv = nullptr;
        PFNGLGETTEXLEVELPARAMETERIVPROC GetTexLevelParameteriv = nullptr;
        PFNGLACTIVETEXTUREPROC ActiveTexture = nullptr;
        PFNGLGETUNIFORMLOCATIONPROC GetUniformLocation = nullptr;
        PFNGLGETUNIFORMFVPROC GetUniformfv = nullptr;
        PFNGLGETUNIFORMIVPROC GetUniformiv = nullptr;
        PFNGLGETACTIVEUNIFORMPROC GetActiveUniform = nullptr;
        PFNGLGETPROGRAMIVPROC GetProgramiv = nullptr;
        PFNGLGETTEXPARAMETERIVPROC GetTexParameteriv = nullptr;
        PFNGLGETTEXPARAMETERFVPROC GetTexParameterfv = nullptr;
        PFNGLBINDTEXTUREPROC BindTexture = nullptr;
    } gl;

    bool glLoaded = false;

    bool init() {
        if (!loadGlFunctions()) {
            globalIssues.push_back("CRITICAL: Failed to load GL functions");
            return false;
        }
        enabled = true;
        return true;
    }

    bool loadGlFunctions() {
        #define LOAD(name) gl.name = (decltype(gl.name))wglGetProcAddress(#name)
        LOAD(GetIntegerv);
        LOAD(GetBooleanv);
        LOAD(GetFloatv);
        LOAD(BindFramebuffer);
        LOAD(CheckFramebufferStatus);
        LOAD(GetFramebufferAttachmentParameteriv);
        LOAD(GetTexLevelParameteriv);
        LOAD(ActiveTexture);
        LOAD(GetUniformLocation);
        LOAD(GetUniformfv);
        LOAD(GetUniformiv);
        LOAD(GetActiveUniform);
        LOAD(GetProgramiv);
        LOAD(GetTexParameteriv);
        LOAD(GetTexParameterfv);
        LOAD(BindTexture);
        #undef LOAD
        if (!gl.GetIntegerv || !gl.BindFramebuffer || !gl.CheckFramebufferStatus ||
            !gl.GetActiveUniform || !gl.GetUniformfv || !gl.GetProgramiv) {
            return false;
        }
        glLoaded = true;
        return true;
    }

    static double getTimeSeconds() {
        static LARGE_INTEGER freq = {};
        static BOOL hasFreq = QueryPerformanceFrequency(&freq);
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        return (double)now.QuadPart / (double)freq.QuadPart;
    }

    static std::string toHex(GLenum v) {
        char buf[32];
        snprintf(buf, sizeof(buf), "0x%04X", (unsigned)v);
        return buf;
    }

    static std::string enumName(GLenum v) {
        switch (v) {
            case 0x0000: return "GL_ZERO";
            case 0x0001: return "GL_ONE";
            case 0x0300: return "GL_SRC_COLOR";
            case 0x0301: return "GL_ONE_MINUS_SRC_COLOR";
            case 0x0302: return "GL_SRC_ALPHA";
            case 0x0303: return "GL_ONE_MINUS_SRC_ALPHA";
            case 0x0304: return "GL_DST_ALPHA";
            case 0x0305: return "GL_ONE_MINUS_DST_ALPHA";
            case 0x0306: return "GL_DST_COLOR";
            case 0x0307: return "GL_ONE_MINUS_DST_COLOR";
            case 0x0308: return "GL_SRC_ALPHA_SATURATE";
            case 0x8006: return "GL_FUNC_ADD";
            case 0x8009: return "GL_FUNC_SUBTRACT";
            case 0x800A: return "GL_FUNC_REVERSE_SUBTRACT";
            case 0x0200: return "GL_LEQUAL";
            case 0x0201: return "GL_LESS";
            case 0x0202: return "GL_EQUAL";
            case 0x0203: return "GL_GEQUAL";
            case 0x0204: return "GL_GREATER";
            case 0x0205: return "GL_NOTEQUAL";
            case 0x0206: return "GL_ALWAYS";
            case 0x0207: return "GL_NEVER";
            case 0x0404: return "GL_CULL_FACE";
            case 0x0405: return "GL_BACK";
            case 0x0406: return "GL_FRONT";
            case 0x0407: return "GL_FRONT_AND_BACK";
            case 0x0900: return "GL_CW";
            case 0x0901: return "GL_CCW";
            case 0x0B71: return "GL_DEPTH_TEST";
            case 0x0B44: return "GL_STENCIL_TEST";
            case 0x0BA2: return "GL_VIEWPORT";
            case 0x0C11: return "GL_SCISSOR_TEST";
            case 0x0BD0: return "GL_POLYGON_MODE";
            case 0x0DE1: return "GL_TEXTURE_2D";
            case 0x84C0: return "GL_TEXTURE0";
            case 0x84C1: return "GL_TEXTURE1";
            case 0x84C2: return "GL_TEXTURE2";
            case 0x84C3: return "GL_TEXTURE3";
            case 0x84C4: return "GL_TEXTURE4";
            case 0x84C5: return "GL_TEXTURE5";
            case 0x84C6: return "GL_TEXTURE6";
            case 0x84C7: return "GL_TEXTURE7";
            default: return toHex(v);
        }
    }

    static int getUniformComponentCount(GLenum type) {
        switch (type) {
            case GL_FLOAT: return 1;
            case GL_FLOAT_VEC2: return 2;
            case GL_FLOAT_VEC3: return 3;
            case GL_FLOAT_VEC4: return 4;
            case GL_INT: case GL_BOOL: case GL_SAMPLER_2D: case GL_SAMPLER_CUBE: return 1;
            case GL_INT_VEC2: case GL_BOOL_VEC2: return 2;
            case GL_INT_VEC3: case GL_BOOL_VEC3: return 3;
            case GL_INT_VEC4: case GL_BOOL_VEC4: return 4;
            case GL_FLOAT_MAT2: return 4;
            case GL_FLOAT_MAT3: return 9;
            case GL_FLOAT_MAT4: return 16;
            case GL_FLOAT_MAT2x3: case GL_FLOAT_MAT3x2: return 6;
            case GL_FLOAT_MAT2x4: case GL_FLOAT_MAT4x2: return 8;
            case GL_FLOAT_MAT3x4: case GL_FLOAT_MAT4x3: return 12;
            default: return 1;
        }
    }

    static std::string uniformTypeStr(GLenum type) {
        switch (type) {
            case GL_FLOAT: return "float";
            case GL_FLOAT_VEC2: return "vec2";
            case GL_FLOAT_VEC3: return "vec3";
            case GL_FLOAT_VEC4: return "vec4";
            case GL_INT: return "int";
            case GL_BOOL: return "bool";
            case GL_SAMPLER_2D: return "sampler2D";
            case GL_SAMPLER_CUBE: return "samplerCube";
            case GL_FLOAT_MAT2: return "mat2";
            case GL_FLOAT_MAT3: return "mat3";
            case GL_FLOAT_MAT4: return "mat4";
            default: return toHex(type);
        }
    }

    void trackCreate(const std::string& kind, GLuint id, const std::string& label = "") {
        if (!enabled) return;
        double now = getTimeSeconds();
        ResourceLifetime entry;
        entry.kind = kind;
        entry.id = id;
        entry.label = label;
        entry.created = now;
        entry.deleted = -1;
        lifetime.push_back(entry);
        activeResources[id] = lifetime.size() - 1;
    }

    void trackDelete(const std::string& kind, GLuint id) {
        if (!enabled) return;
        auto it = activeResources.find(id);
        if (it != activeResources.end()) {
            lifetime[it->second].deleted = getTimeSeconds();
            activeResources.erase(it);
        } else {
            globalIssues.push_back("DOUBLE_DELETE: " + kind + " id=" + std::to_string(id));
        }
    }

    void detectLeaks() {
        for (auto& r : lifetime) {
            if (r.deleted < 0 && !r.doubleDelete) {
                r.leaked = true;
                leakSummary[r.kind]++;
            }
        }
        for (auto it = leakSummary.begin(); it != leakSummary.end(); ++it) {
            const std::string& kind = it->first;
            const int count = it->second;
            consistencyIssues.push_back("LEAK: " + std::to_string(count) + " " + kind + " never deleted");
        }
    }

    TextureSnapshot captureTextureInfo(GLuint texId) {
        TextureSnapshot ts;
        ts.id = texId;
        if (!texId || !glLoaded) return ts;
        ts.target = GL_TEXTURE_2D;
        gl.BindTexture(ts.target, texId);
        gl.GetTexLevelParameteriv(ts.target, 0, GL_TEXTURE_WIDTH, &ts.width);
        gl.GetTexLevelParameteriv(ts.target, 0, GL_TEXTURE_HEIGHT, &ts.height);
        gl.GetTexLevelParameteriv(ts.target, 0, GL_TEXTURE_INTERNAL_FORMAT, &ts.internalFormat);
        gl.GetTexLevelParameteriv(ts.target, 0, 0x8C1D, &ts.type);
        GLint levels = 0;
        gl.GetTexParameteriv(ts.target, GL_TEXTURE_MAX_LEVEL, &levels);
        ts.mipLevels = levels + 1;
        gl.GetTexParameteriv(ts.target, GL_TEXTURE_MIN_FILTER, &ts.minFilter);
        gl.GetTexParameteriv(ts.target, GL_TEXTURE_MAG_FILTER, &ts.magFilter);
        gl.GetTexParameteriv(ts.target, GL_TEXTURE_WRAP_S, &ts.wrapS);
        gl.GetTexParameteriv(ts.target, GL_TEXTURE_WRAP_T, &ts.wrapT);
        gl.GetTexParameteriv(ts.target, GL_TEXTURE_WRAP_R, &ts.wrapR);
        GLfloat bc[4] = {};
        gl.GetTexParameterfv(ts.target, GL_TEXTURE_BORDER_COLOR, bc);
        memcpy(ts.borderColor, bc, sizeof(bc));
        ts.isSRGB = (ts.internalFormat == 0x8C41 || ts.internalFormat == 0x8C43 ||
                     ts.internalFormat == 0x8D64 || ts.internalFormat == 0x8C40);
        return ts;
    }

    void captureFBOAttachments(GPUSnapshot& snap) {
        if (!snap.fboId || !glLoaded) return;
        gl.BindFramebuffer(GL_FRAMEBUFFER, snap.fboId);
        GLint maxAttach = 0;
        gl.GetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &maxAttach);
        for (GLint i = 0; i < maxAttach; i++) {
            GLint texId = 0;
            gl.GetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,
                GL_COLOR_ATTACHMENT0 + i,
                0x8CD0, &texId);
            if (texId > 0) {
                FBOAttachment att;
                att.attachment = GL_COLOR_ATTACHMENT0 + i;
                att.textureId = (GLuint)texId;
                GLint mipLevel = 0;
                gl.GetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,
                    GL_COLOR_ATTACHMENT0 + i, 0x8CD1, &mipLevel);
                att.mipLevel = mipLevel;
                att.level = 0;
                snap.fboAttachments.push_back(att);
            }
        }
    }

    void captureRenderState(GPUSnapshot& snap) {
        if (!glLoaded) return;
        auto& s = snap.state;
        GLint v = 0;
        GLboolean bv = GL_FALSE;
        gl.GetBooleanv(GL_BLEND, &bv); s.blend.enabled = (bv != GL_FALSE);
        gl.GetIntegerv(GL_BLEND_SRC_RGB, &v); s.blend.srcRGB = (GLenum)v;
        gl.GetIntegerv(GL_BLEND_DST_RGB, &v); s.blend.dstRGB = (GLenum)v;
        gl.GetIntegerv(GL_BLEND_EQUATION_RGB, &v); s.blend.opRGB = (GLenum)v;
        gl.GetIntegerv(GL_BLEND_SRC_ALPHA, &v); s.blend.srcAlpha = (GLenum)v;
        gl.GetIntegerv(GL_BLEND_DST_ALPHA, &v); s.blend.dstAlpha = (GLenum)v;
        gl.GetIntegerv(GL_BLEND_EQUATION_ALPHA, &v); s.blend.opAlpha = (GLenum)v;
        gl.GetFloatv(GL_BLEND_COLOR, s.blend.color);
        gl.GetBooleanv(GL_DEPTH_TEST, &bv); s.depth.enabled = (bv != GL_FALSE);
        gl.GetBooleanv(GL_DEPTH_WRITEMASK, &s.depth.mask);
        gl.GetIntegerv(GL_DEPTH_FUNC, &v); s.depth.func = (GLenum)v;
        gl.GetBooleanv(GL_STENCIL_TEST, &bv); s.stencil.enabled = (bv != GL_FALSE);
        gl.GetIntegerv(GL_STENCIL_FUNC, &v); s.stencil.func = (GLenum)v;
        gl.GetIntegerv(GL_STENCIL_REF, &s.stencil.ref);
        gl.GetIntegerv(GL_STENCIL_VALUE_MASK, &v); s.stencil.mask = (GLuint)v;
        gl.GetIntegerv(GL_STENCIL_FAIL, &v); s.stencil.fail = (GLenum)v;
        gl.GetIntegerv(GL_STENCIL_PASS_DEPTH_FAIL, &v); s.stencil.zFail = (GLenum)v;
        gl.GetIntegerv(GL_STENCIL_PASS_DEPTH_PASS, &v); s.stencil.zPass = (GLenum)v;
        gl.GetBooleanv(GL_CULL_FACE, &bv); s.cull.enabled = (bv != GL_FALSE);
        gl.GetIntegerv(GL_CULL_FACE_MODE, &v); s.cull.mode = (GLenum)v;
        gl.GetIntegerv(GL_FRONT_FACE, &v); s.cull.frontFace = (GLenum)v;
        s.frontFace = s.cull.frontFace;
        gl.GetBooleanv(GL_SCISSOR_TEST, &bv); s.scissor.enabled = (bv != GL_FALSE);
        GLint scBox[4] = {};
        gl.GetIntegerv(GL_SCISSOR_BOX, scBox);
        s.scissor.x = scBox[0]; s.scissor.y = scBox[1];
        s.scissor.width = scBox[2]; s.scissor.height = scBox[3];
        GLint vpBox[4] = {};
        gl.GetIntegerv(GL_VIEWPORT, vpBox);
        s.viewport.x = vpBox[0]; s.viewport.y = vpBox[1];
        s.viewport.width = vpBox[2]; s.viewport.height = vpBox[3];
        gl.GetBooleanv(GL_COLOR_WRITEMASK, s.colorMask);
        gl.GetIntegerv(GL_POLYGON_MODE, &v); s.polygonMode = (GLenum)v;
        gl.GetBooleanv(GL_FRAMEBUFFER_SRGB, &bv); s.framebufferSRGB = (bv != GL_FALSE);
        gl.GetIntegerv(GL_DRAW_BUFFER, &v); s.drawBuffer = (GLenum)v;
        gl.GetIntegerv(GL_READ_BUFFER, &v); s.readBuffer = (GLenum)v;
    }

    void captureUniforms(GPUSnapshot& snap) {
        if (!snap.programId || !glLoaded) return;
        GLint activeUniforms = 0;
        gl.GetProgramiv(snap.programId, GL_ACTIVE_UNIFORMS, &activeUniforms);
        snap.uniformCount = activeUniforms;
        for (GLint i = 0; i < activeUniforms; i++) {
            UniformSnapshot u;
            char nameBuf[256] = {};
            GLsizei len = 0;
            gl.GetActiveUniform(snap.programId, (GLuint)i, sizeof(nameBuf),
                &len, &u.arraySize, &u.type, nameBuf);
            u.name = std::string(nameBuf, len);
            u.location = gl.GetUniformLocation(snap.programId, nameBuf);
            if (u.location < 0) continue;
            switch (u.type) {
                case GL_FLOAT: case GL_FLOAT_VEC2: case GL_FLOAT_VEC3: case GL_FLOAT_VEC4:
                case GL_FLOAT_MAT2: case GL_FLOAT_MAT3: case GL_FLOAT_MAT4:
                case GL_FLOAT_MAT2x3: case GL_FLOAT_MAT2x4:
                case GL_FLOAT_MAT3x2: case GL_FLOAT_MAT3x4:
                case GL_FLOAT_MAT4x2: case GL_FLOAT_MAT4x3:
                    gl.GetUniformfv(snap.programId, u.location, u.floatValue);
                    u.isFloat = true;
                    break;
                case GL_INT: case GL_BOOL: case GL_SAMPLER_2D: case GL_SAMPLER_CUBE:
                case GL_INT_VEC2: case GL_INT_VEC3: case GL_INT_VEC4:
                case GL_BOOL_VEC2: case GL_BOOL_VEC3: case GL_BOOL_VEC4:
                    gl.GetUniformiv(snap.programId, u.location, u.intValue);
                    break;
                default: break;
            }
            if (u.isFloat) {
                int count = getUniformComponentCount(u.type);
                for (int c = 0; c < count && c < 16; c++) {
                    if (std::isnan(u.floatValue[c])) { u.hasNaN = true; break; }
                    if (std::isinf(u.floatValue[c])) { u.hasInf = true; break; }
                }
            }
            if (u.hasNaN) u.status = "NaN";
            else if (u.hasInf) u.status = "Inf";
            else u.status = "OK";
            snap.uniforms.push_back(u);
        }
    }

    void validateSamplers(GPUSnapshot& snap) {
        for (auto it = snap.textureBindings.begin(); it != snap.textureBindings.end(); ++it) {
            SamplerSnapshot sampler;
            sampler.unit = it->first;
            sampler.textureId = it->second;
            sampler.hasTexture = (it->second > 0);
            auto texIt = snap.textureIndexById.find(it->second);
            if (texIt != snap.textureIndexById.end()) {
                sampler.texture = snap.textures[texIt->second];
            }
            if (!sampler.hasTexture) {
                snap.issues.push_back("SAMPLER_NO_TEXTURE: unit " + std::to_string(it->first));
            }
            snap.samplers.push_back(sampler);
        }
    }

    GPUSnapshot captureSnapshot(int passIndex, const std::string& passName,
                                GLuint program, GLuint fbo) {
        GPUSnapshot snap;
        snap.passIndex = passIndex;
        snap.passName = passName;
        snap.programId = program;
        snap.drawCalls = currentPassDrawCalls;
        currentPassDrawCalls = 0;
        if (!enabled || !glLoaded) return snap;
        if (program) {
            GLint linked = 0;
            gl.GetProgramiv(program, GL_LINK_STATUS, &linked);
            snap.programValid = (linked != 0);
            if (!snap.programValid) {
                snap.issues.push_back("PROGRAM_NOT_LINKED: " + std::to_string(program));
            }
        }
        snap.fboId = fbo;
        if (fbo) {
            gl.BindFramebuffer(GL_FRAMEBUFFER, fbo);
            snap.fboStatus = gl.CheckFramebufferStatus(GL_FRAMEBUFFER);
            snap.fboComplete = (snap.fboStatus == GL_FRAMEBUFFER_COMPLETE);
            if (!snap.fboComplete) {
                snap.issues.push_back("INCOMPLETE_FBO: " + std::to_string(fbo) +
                    " status=" + toHex(snap.fboStatus));
            }
            captureFBOAttachments(snap);
        }
        captureRenderState(snap);
        GLint activeTex = 0;
        gl.GetIntegerv(GL_ACTIVE_TEXTURE, &activeTex);
        snap.state.activeTexture = (GLenum)activeTex;
        for (GLuint unit = 0; unit < 16; unit++) {
            gl.ActiveTexture(GL_TEXTURE0 + unit);
            GLint bound = 0;
            gl.GetIntegerv(GL_TEXTURE_BINDING_2D, &bound);
            if (bound > 0) {
                snap.textureBindings[unit] = (GLuint)bound;
            }
        }
        gl.ActiveTexture(snap.state.activeTexture);
        for (auto it = snap.textureBindings.begin(); it != snap.textureBindings.end(); ++it) {
            const GLuint unit = it->first;
            const GLuint texId = it->second;
            TextureSnapshot ts = captureTextureInfo(texId);
            snap.textureIndexById[texId] = snap.textures.size();
            snap.textures.push_back(ts);
            if (fbo && texId > 0) {
                for (auto& att : snap.fboAttachments) {
                    if (att.textureId == texId) {
                        snap.issues.push_back("SIMULTANEOUS_READ_WRITE: tex " +
                            std::to_string(texId) + " unit " + std::to_string(unit));
                        break;
                    }
                }
            }
        }
        if (program && enableUniformReadback) {
            captureUniforms(snap);
        }
        validateSamplers(snap);
        return snap;
    }

    void runConsistencyChecks(GPUSnapshot& snap) {
        for (auto& s : snap.samplers) {
            if (!s.hasTexture) {
                consistencyIssues.push_back("PASS " + std::to_string(snap.passIndex) +
                    ": Sampler unit " + std::to_string(s.unit) + " has no texture");
            }
        }
        for (auto& t : snap.textures) {
            if (t.internalFormat == 0) {
                consistencyIssues.push_back("PASS " + std::to_string(snap.passIndex) +
                    ": Texture " + std::to_string(t.id) + " has invalid format");
            }
        }
        if (snap.fboId && !snap.fboComplete) {
            consistencyIssues.push_back("PASS " + std::to_string(snap.passIndex) +
                ": FBO " + std::to_string(snap.fboId) + " incomplete");
        }
        for (auto& u : snap.uniforms) {
            if (u.hasNaN) {
                consistencyIssues.push_back("PASS " + std::to_string(snap.passIndex) +
                    ": Uniform '" + u.name + "' contains NaN");
            }
            if (u.hasInf) {
                consistencyIssues.push_back("PASS " + std::to_string(snap.passIndex) +
                    ": Uniform '" + u.name + "' contains Inf");
            }
        }
        for (auto& att : snap.fboAttachments) {
            bool found = false;
            for (auto& t : snap.textures) {
                if (t.id == att.textureId) { found = true; break; }
            }
            if (!found && att.textureId > 0) {
                TextureSnapshot ts = captureTextureInfo(att.textureId);
                snap.textures.push_back(ts);
            }
        }
    }

    bool loadRetroArchSnapshot(const std::string& filePath) {
        std::ifstream f(filePath);
        if (!f.is_open()) return false;
        retroarch.loaded = true;
        retroarch.label = filePath;
        std::string line;
        std::string currentSection;
        while (std::getline(f, line)) {
            if (line.empty() || line[0] == '#') continue;
            if (line[0] == '[') {
                auto bracket = line.find(']');
                currentSection = (bracket != std::string::npos) ? line.substr(1, bracket - 1) : line.substr(1);
                continue;
            }
            if (currentSection == "uniforms") {
                RetroArchUniform u;
                auto eq = line.find('=');
                if (eq != std::string::npos) {
                    u.name = line.substr(0, eq);
                    std::string val = line.substr(eq + 1);
                    std::istringstream iss(val);
                    int idx = 0;
                    while (iss >> u.floatVal[idx] && idx < 16) idx++;
                    u.location = -1;
                    retroarch.uniforms.push_back(u);
                }
            } else if (currentSection == "textures") {
                RetroArchTextureInfo t;
                std::istringstream iss(line);
                iss >> t.id >> t.width >> t.height >> t.internalFormat;
                std::getline(iss, t.origin);
                retroarch.textures.push_back(t);
            } else if (currentSection == "sampler_bindings") {
                RetroArchSampler s;
                std::istringstream iss(line);
                iss >> s.unit >> s.textureId;
                std::getline(iss, s.origin);
                retroarch.samplers.push_back(s);
            } else if (currentSection == "render_state") {
                auto eq = line.find('=');
                if (eq != std::string::npos) {
                    retroarch.renderState[line.substr(0, eq)] = line.substr(eq + 1);
                }
            } else if (currentSection == "info") {
                auto eq = line.find('=');
                if (eq != std::string::npos) {
                    std::string key = line.substr(0, eq);
                    std::string val = line.substr(eq + 1);
                    if (key == "pass_count") retroarch.passCount = std::stoi(val);
                }
            }
        }
        return true;
    }

    void compareWithRetroArch(const GPUSnapshot& snap) {
        if (!retroarch.loaded) return;
        for (auto& ru : retroarch.uniforms) {
            bool found = false;
            for (auto& mu : snap.uniforms) {
                if (mu.name == ru.name) {
                    found = true;
                    if (mu.isFloat) {
                        int count = getUniformComponentCount(mu.type);
                        for (int c = 0; c < count && c < 16; c++) {
                            float diff = std::fabs(mu.floatValue[c] - ru.floatVal[c]);
                            if (diff > 0.01f) {
                                retroDiffs.push_back("PASS " + std::to_string(snap.passIndex) +
                                    ": Uniform '" + mu.name + "'[" + std::to_string(c) +
                                    "] Monix=" + std::to_string(mu.floatValue[c]) +
                                    " RetroArch=" + std::to_string(ru.floatVal[c]));
                                break;
                            }
                        }
                    }
                    break;
                }
            }
            if (!found) {
                retroDiffs.push_back("PASS " + std::to_string(snap.passIndex) +
                    ": Uniform '" + ru.name + "' missing in Monix");
            }
        }
        for (auto& rt : retroarch.textures) {
            bool found = false;
            for (auto& mt : snap.textures) {
                if (mt.width == rt.width && mt.height == rt.height &&
                    mt.internalFormat == rt.internalFormat) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                retroDiffs.push_back("PASS " + std::to_string(snap.passIndex) +
                    ": Texture " + std::to_string(rt.id) + " (" +
                    std::to_string(rt.width) + "x" + std::to_string(rt.height) +
                    ") not found in Monix");
            }
        }
    }

    void runAllChecks() {
        for (auto& snap : snapshots) {
            runConsistencyChecks(snap);
            compareWithRetroArch(snap);
        }
        detectLeaks();
    }

    std::string generateReport() const {
        std::ostringstream os;
        os << "MEGA BEZEL PHASE 12 - RUNTIME STATE VALIDATION REPORT\n";
        os << "=====================================================\n\n";

        int totalIssues = 0;
        for (auto& s : snapshots) totalIssues += (int)s.issues.size();
        totalIssues += (int)globalIssues.size();
        totalIssues += (int)retroDiffs.size();
        totalIssues += (int)consistencyIssues.size();

        bool pass = (totalIssues == 0);
        os << "1. EXECUTIVE SUMMARY\n";
        os << "--------------------\n";
        os << "Verdict: " << (pass ? "PASS" : "FAIL") << "\n";
        os << "Total snapshots: " << snapshots.size() << "\n";
        os << "Total issues: " << totalIssues << "\n";
        os << "  Snapshot issues: " << (totalIssues - globalIssues.size() - retroDiffs.size() - consistencyIssues.size()) << "\n";
        os << "  Global issues: " << globalIssues.size() << "\n";
        os << "  RetroArch diffs: " << retroDiffs.size() << "\n";
        os << "  Consistency issues: " << consistencyIssues.size() << "\n\n";

        os << "2. GPU RESOURCE INVENTORY\n";
        os << "-------------------------\n";
        std::set<GLuint> allPrograms, allTextures, allFBOs;
        for (auto& snap : snapshots) {
            if (snap.programId) allPrograms.insert(snap.programId);
            for (auto& t : snap.textures) allTextures.insert(t.id);
            if (snap.fboId) allFBOs.insert(snap.fboId);
        }
        os << "Programs: " << allPrograms.size() << "\n";
        os << "Textures: " << allTextures.size() << "\n";
        os << "FBOs: " << allFBOs.size() << "\n\n";

        os << "3. PASS STATE SNAPSHOTS\n";
        os << "-----------------------\n";
        for (auto& snap : snapshots) {
            os << "\n--- Pass " << snap.passIndex << ": " << snap.passName << " ---\n";
            os << "  Program: " << snap.programId;
            os << (snap.programValid ? " [VALID]" : " [INVALID]") << "\n";
            os << "  FBO: " << snap.fboId;
            os << (snap.fboComplete ? " [COMPLETE]" : " [INCOMPLETE]") << "\n";
            os << "  Attachments: " << snap.fboAttachments.size() << "\n";
            os << "  Draw calls: " << snap.drawCalls << "\n";
            os << "  Viewport: " << snap.state.viewport.width << "x" << snap.state.viewport.height << "\n";
            os << "  Blend: " << (snap.state.blend.enabled ? "ON" : "OFF");
            if (snap.state.blend.enabled) {
                os << " src=" << enumName(snap.state.blend.srcRGB)
                   << " dst=" << enumName(snap.state.blend.dstRGB)
                   << " eq=" << enumName(snap.state.blend.opRGB);
            }
            os << "\n";
            os << "  Depth: " << (snap.state.depth.enabled ? "ON" : "OFF");
            if (snap.state.depth.enabled) {
                os << " func=" << enumName(snap.state.depth.func);
            }
            os << "\n";
            os << "  Stencil: " << (snap.state.stencil.enabled ? "ON" : "OFF") << "\n";
            os << "  Cull: " << (snap.state.cull.enabled ? "ON" : "OFF");
            if (snap.state.cull.enabled) {
                os << " mode=" << enumName(snap.state.cull.mode);
            }
            os << "\n";
            os << "  Scissor: " << (snap.state.scissor.enabled ? "ON" : "OFF") << "\n";
            os << "  SRGB: " << (snap.state.framebufferSRGB ? "ON" : "OFF") << "\n";
            os << "  Textures bound: " << snap.textureBindings.size() << "\n";
            os << "  Uniforms: " << snap.uniforms.size() << "\n";
            for (auto& u : snap.uniforms) {
                os << "    " << u.name << " [" << uniformTypeStr(u.type) << "] loc=" << u.location;
                if (u.status != "OK") os << " STATUS=" << u.status;
                if (u.isFloat) os << " val=(" << u.floatValue[0] << "," << u.floatValue[1] << ")";
                os << "\n";
            }
            if (!snap.issues.empty()) {
                os << "  ISSUES:\n";
                for (auto& iss : snap.issues) os << "    " << iss << "\n";
            }
        }

        os << "\n4. TEXTURE AUDIT\n";
        os << "----------------\n";
        for (auto& snap : snapshots) {
            for (auto& t : snap.textures) {
                os << "  Pass " << snap.passIndex << " Tex " << t.id
                   << " " << t.width << "x" << t.height
                   << " fmt=" << toHex(t.internalFormat)
                   << " filter=(" << t.minFilter << "," << t.magFilter << ")"
                   << " wrap=(" << t.wrapS << "," << t.wrapT << ")"
                   << (t.isSRGB ? " sRGB" : "") << "\n";
            }
        }

        os << "\n5. UNIFORM AUDIT\n";
        os << "----------------\n";
        for (auto& snap : snapshots) {
            for (auto& u : snap.uniforms) {
                os << "  Pass " << snap.passIndex << " " << u.name
                   << " [" << uniformTypeStr(u.type) << "] loc=" << u.location
                   << " status=" << u.status << "\n";
            }
        }

        os << "\n6. SAMPLER AUDIT\n";
        os << "----------------\n";
        for (auto& snap : snapshots) {
            for (auto& s : snap.samplers) {
                os << "  Pass " << snap.passIndex << " unit=" << s.unit
                   << " tex=" << s.textureId
                   << (s.hasTexture ? " [BOUND]" : " [UNBOUND]")
                   << "\n";
            }
        }

        os << "\n7. RENDER STATE AUDIT\n";
        os << "---------------------\n";
        for (auto& snap : snapshots) {
            auto& rs = snap.state;
            os << "  Pass " << snap.passIndex
               << " blend=" << (rs.blend.enabled ? "ON" : "OFF")
               << " depth=" << (rs.depth.enabled ? "ON" : "OFF")
               << " stencil=" << (rs.stencil.enabled ? "ON" : "OFF")
               << " cull=" << (rs.cull.enabled ? "ON" : "OFF")
               << " scissor=" << (rs.scissor.enabled ? "ON" : "OFF")
               << " srgb=" << (rs.framebufferSRGB ? "ON" : "OFF")
               << "\n";
        }

        os << "\n8. RESOURCE LIFETIME\n";
        os << "--------------------\n";
        int created = 0, deleted = 0, leaked = 0;
        for (auto& r : lifetime) {
            created++;
            if (r.deleted >= 0) deleted++;
            if (r.leaked) leaked++;
        }
        os << "  Created: " << created << "\n";
        os << "  Deleted: " << deleted << "\n";
        os << "  Leaked: " << leaked << "\n";
        for (auto& r : lifetime) {
            if (r.leaked) {
                os << "  LEAKED: " << r.kind << " id=" << r.id;
                if (!r.label.empty()) os << " (" << r.label << ")";
                os << "\n";
            }
        }

        os << "\n9. LEAK DETECTION\n";
        os << "------------------\n";
        if (leakSummary.empty()) {
            os << "  No leaks detected.\n";
        } else {
            for (auto it = leakSummary.begin(); it != leakSummary.end(); ++it) {
                os << "  " << it->first << ": " << it->second << " leaked\n";
            }
        }

        os << "\n10. RETROARCH RUNTIME COMPARISON\n";
        os << "--------------------------------\n";
        if (!retroarch.loaded) {
            os << "  No RetroArch snapshot loaded. Use --retroarch-snapshot <file>.\n";
        } else {
            os << "  RetroArch snapshot: " << retroarch.label << "\n";
            os << "  RetroArch passes: " << retroarch.passCount << "\n";
            os << "  RetroArch uniforms: " << retroarch.uniforms.size() << "\n";
            os << "  RetroArch textures: " << retroarch.textures.size() << "\n";
            if (retroDiffs.empty()) {
                os << "  No mismatches found.\n";
            } else {
                os << "  MISMATCHES (" << retroDiffs.size() << "):\n";
                for (auto& d : retroDiffs) os << "    " << d << "\n";
            }
        }

        os << "\n11. STATE DIFFERENCES\n";
        os << "---------------------\n";
        if (snapshots.size() > 1) {
            auto& first = snapshots[0];
            for (size_t i = 1; i < snapshots.size(); i++) {
                auto& cur = snapshots[i];
                if (cur.state.blend.enabled != first.state.blend.enabled) {
                    os << "  Pass " << first.passIndex << " vs " << cur.passIndex
                       << ": blend state differs\n";
                }
                if (cur.state.depth.enabled != first.state.depth.enabled) {
                    os << "  Pass " << first.passIndex << " vs " << cur.passIndex
                       << ": depth state differs\n";
                }
                if (cur.state.cull.enabled != first.state.cull.enabled) {
                    os << "  Pass " << first.passIndex << " vs " << cur.passIndex
                       << ": cull state differs\n";
                }
                if (cur.state.viewport.width != first.state.viewport.width ||
                    cur.state.viewport.height != first.state.viewport.height) {
                    os << "  Pass " << first.passIndex << " vs " << cur.passIndex
                       << ": viewport size differs ("
                       << first.state.viewport.width << "x" << first.state.viewport.height
                       << " vs " << cur.state.viewport.width << "x" << cur.state.viewport.height
                       << ")\n";
                }
            }
        } else {
            os << "  Only one pass — no cross-pass comparison.\n";
        }

        os << "\n12. CONSISTENCY CHECKS\n";
        os << "----------------------\n";
        if (consistencyIssues.empty()) {
            os << "  All checks passed.\n";
        } else {
            for (auto& c : consistencyIssues) os << "  " << c << "\n";
        }

        os << "\n13. ROOT CAUSE ANALYSIS\n";
        os << "-----------------------\n";
        if (totalIssues == 0) {
            os << "  No issues to analyze.\n";
        } else {
            if (!consistencyIssues.empty()) {
                os << "  Consistency issues detected — review Sampler, Texture, and FBO state.\n";
            }
            if (!retroDiffs.empty()) {
                os << "  RetroArch differences found — review uniform values and texture formats.\n";
            }
            int snapIssues = 0;
            for (auto& s : snapshots) snapIssues += (int)s.issues.size();
            if (snapIssues > 0) {
                os << "  " << snapIssues << " per-pass issues detected — review PASS STATE SNAPSHOTS.\n";
            }
            if (!globalIssues.empty()) {
                os << "  Global issues: " << globalIssues.size() << "\n";
                for (auto& g : globalIssues) os << "    " << g << "\n";
            }
        }

        os << "\n14. GENERIC RECOMMENDATIONS\n";
        os << "---------------------------\n";
        os << "  1. Ensure all FBOs are complete before use.\n";
        os << "  2. Validate uniform uploads against expected values.\n";
        os << "  3. Confirm texture dimensions match render target size.\n";
        os << "  4. Verify feedback textures alternate correctly.\n";
        os << "  5. Track resource lifecycle to detect leaks.\n";
        os << "  6. Compare runtime state against RetroArch reference.\n";
        os << "  7. Maintain per-pass state snapshots for debugging.\n";

        os << "\n15. VERDICT\n";
        os << "----------\n";
        os << "  " << (pass ? "PASS" : "FAIL") << "\n";
        os << "\n";

        for (auto& g : globalIssues) {
            os << "[GLOBAL] " << g << "\n";
        }

        return os.str();
    }

    bool saveReport(const std::string& path) const {
        std::ofstream f(path);
        if (!f.is_open()) return false;
        f << generateReport();
        return true;
    }
};

#endif
