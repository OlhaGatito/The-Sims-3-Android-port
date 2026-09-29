#include "s3e_host_internal.h"

#define GL_WRAP_FLOAT1(name, t1)                                                                   \
    static S3E_SOFTFP void host_##name(t1 a) {                                                     \
        void (*real)(t1) = lookup_gl(#name);                                                       \
        if (real)                                                                                  \
            real(a);                                                                               \
    }
#define GL_WRAP_FLOAT2(name, t1, t2)                                                               \
    static S3E_SOFTFP void host_##name(t1 a, t2 b) {                                               \
        void (*real)(t1, t2) = lookup_gl(#name);                                                   \
        if (real)                                                                                  \
            real(a, b);                                                                            \
    }
#define GL_WRAP_FLOAT3(name, t1, t2, t3)                                                           \
    static S3E_SOFTFP void host_##name(t1 a, t2 b, t3 c) {                                         \
        void (*real)(t1, t2, t3) = lookup_gl(#name);                                               \
        if (real)                                                                                  \
            real(a, b, c);                                                                         \
    }
#define GL_WRAP_FLOAT4(name, t1, t2, t3, t4)                                                       \
    static S3E_SOFTFP void host_##name(t1 a, t2 b, t3 c, t4 d) {                                   \
        void (*real)(t1, t2, t3, t4) = lookup_gl(#name);                                           \
        if (real)                                                                                  \
            real(a, b, c, d);                                                                      \
    }
#define GL_WRAP_FLOAT5(name, t1, t2, t3, t4, t5)                                                   \
    static S3E_SOFTFP void host_##name(t1 a, t2 b, t3 c, t4 d, t5 e) {                             \
        void (*real)(t1, t2, t3, t4, t5) = lookup_gl(#name);                                       \
        if (real)                                                                                  \
            real(a, b, c, d, e);                                                                   \
    }
#define GL_WRAP_FLOAT6(name, t1, t2, t3, t4, t5, t6)                                               \
    static S3E_SOFTFP void host_##name(t1 a, t2 b, t3 c, t4 d, t5 e, t6 f) {                       \
        void (*real)(t1, t2, t3, t4, t5, t6) = lookup_gl(#name);                                   \
        if (real)                                                                                  \
            real(a, b, c, d, e, f);                                                                \
    }
#define GL_WRAP_FLOAT1_ALIAS(name, fallback, t1)                                                    \
    static S3E_SOFTFP void host_##name(t1 a) {                                                     \
        void (*real)(t1) = lookup_gl(#name);                                                       \
        if (!real)                                                                                 \
            real = lookup_gl(#fallback);                                                           \
        if (real)                                                                                  \
            real(a);                                                                               \
    }
#define GL_WRAP_FLOAT2_ALIAS(name, fallback, t1, t2)                                                \
    static S3E_SOFTFP void host_##name(t1 a, t2 b) {                                               \
        void (*real)(t1, t2) = lookup_gl(#name);                                                   \
        if (!real)                                                                                 \
            real = lookup_gl(#fallback);                                                           \
        if (real)                                                                                  \
            real(a, b);                                                                            \
    }
#define GL_WRAP_FLOAT6_ALIAS(name, fallback, t1, t2, t3, t4, t5, t6)                                \
    static S3E_SOFTFP void host_##name(t1 a, t2 b, t3 c, t4 d, t5 e, t6 f) {                       \
        void (*real)(t1, t2, t3, t4, t5, t6) = lookup_gl(#name);                                   \
        if (!real)                                                                                 \
            real = lookup_gl(#fallback);                                                           \
        if (real)                                                                                  \
            real(a, b, c, d, e, f);                                                                \
    }

enum {
    FRAME_INTERVAL_US = 16667,
    FRAME_RESET_US = FRAME_INTERVAL_US * 4,
};

static void sleep_until_us(uint64_t target_us) {
    for (;;) {
        uint64_t now = monotonic_us();
        if (now >= target_us) {
            return;
        }
        uint64_t remaining = target_us - now;
        struct timespec req = {
            .tv_sec = (time_t)(remaining / 1000000u),
            .tv_nsec = (long)(remaining % 1000000u) * 1000L,
        };
        while (nanosleep(&req, &req) != 0 && errno == EINTR) {
        }
    }
}

static void pace_frame(void) {
    static uint64_t next_frame_us;
    uint64_t now = monotonic_us();

    if (!next_frame_us || now > next_frame_us + FRAME_RESET_US) {
        next_frame_us = now + FRAME_INTERVAL_US;
    }

    sleep_until_us(next_frame_us);
    next_frame_us += FRAME_INTERVAL_US;
}

GL_WRAP_FLOAT2(glAlphaFunc, GLenum, GLfloat)
GL_WRAP_FLOAT4(glBlendColor, GLfloat, GLfloat, GLfloat, GLfloat)
GL_WRAP_FLOAT4(glClearColor, GLfloat, GLfloat, GLfloat, GLfloat)
GL_WRAP_FLOAT1(glClearDepthf, GLfloat)
GL_WRAP_FLOAT1_ALIAS(glClearDepthfOES, glClearDepthf, GLfloat)
GL_WRAP_FLOAT4(glColor4f, GLfloat, GLfloat, GLfloat, GLfloat)
GL_WRAP_FLOAT2(glDepthRangef, GLfloat, GLfloat)
GL_WRAP_FLOAT2_ALIAS(glDepthRangefOES, glDepthRangef, GLfloat, GLfloat)
GL_WRAP_FLOAT5(glDrawTexfOES, GLfloat, GLfloat, GLfloat, GLfloat, GLfloat)
GL_WRAP_FLOAT2(glFogf, GLenum, GLfloat)
GL_WRAP_FLOAT6(glFrustumf, GLfloat, GLfloat, GLfloat, GLfloat, GLfloat, GLfloat)
GL_WRAP_FLOAT6_ALIAS(glFrustumfOES, glFrustumf, GLfloat, GLfloat, GLfloat, GLfloat, GLfloat,
                     GLfloat)
GL_WRAP_FLOAT2(glLightModelf, GLenum, GLfloat)
GL_WRAP_FLOAT3(glLightf, GLenum, GLenum, GLfloat)
GL_WRAP_FLOAT1(glLineWidth, GLfloat)
GL_WRAP_FLOAT3(glMaterialf, GLenum, GLenum, GLfloat)
GL_WRAP_FLOAT4(glMultiTexCoord4f, GLenum, GLfloat, GLfloat, GLfloat)
GL_WRAP_FLOAT3(glNormal3f, GLfloat, GLfloat, GLfloat)
GL_WRAP_FLOAT6(glOrthof, GLfloat, GLfloat, GLfloat, GLfloat, GLfloat, GLfloat)
GL_WRAP_FLOAT6_ALIAS(glOrthofOES, glOrthof, GLfloat, GLfloat, GLfloat, GLfloat, GLfloat, GLfloat)
GL_WRAP_FLOAT2(glPointParameterf, GLenum, GLfloat)
GL_WRAP_FLOAT1(glPointSize, GLfloat)
GL_WRAP_FLOAT2(glPolygonOffset, GLfloat, GLfloat)
GL_WRAP_FLOAT4(glRotatef, GLfloat, GLfloat, GLfloat, GLfloat)
GL_WRAP_FLOAT2(glSampleCoverage, GLfloat, GLboolean)
GL_WRAP_FLOAT3(glScalef, GLfloat, GLfloat, GLfloat)
GL_WRAP_FLOAT3(glTexEnvf, GLenum, GLenum, GLfloat)
GL_WRAP_FLOAT3(glTexGenfOES, GLenum, GLenum, GLfloat)
GL_WRAP_FLOAT3(glTexParameterf, GLenum, GLenum, GLfloat)
GL_WRAP_FLOAT3(glTranslatef, GLfloat, GLfloat, GLfloat)
GL_WRAP_FLOAT2(glUniform1f, GLint, GLfloat)
GL_WRAP_FLOAT3(glUniform2f, GLint, GLfloat, GLfloat)
GL_WRAP_FLOAT4(glUniform3f, GLint, GLfloat, GLfloat, GLfloat)
GL_WRAP_FLOAT5(glUniform4f, GLint, GLfloat, GLfloat, GLfloat, GLfloat)
GL_WRAP_FLOAT5(glVertexAttrib4f, GLuint, GLfloat, GLfloat, GLfloat, GLfloat)

static void *resolve_wrapped_host_proc(const char *symbol);

/* NFS Shift (NextOS): instrumentacao de render (SIMS3_GL_LOG=1). Conta
   draws/texturas/clear por frame p/ diagnosticar tela branca (o jogo emite
   draws? sobe texturas? ha erro de GL?). */
static unsigned g_gl_draws, g_gl_tex, g_gl_ctex, g_gl_clears, g_gl_frame;
static int gl_log_on(void) {
    static int v = -1;
    if (v < 0) {
        v = getenv("SIMS3_GL_LOG") ? 1 : 0;
    }
    return v;
}

static int g_gl_draw_err_logged = 0;
static void gl_probe_draw_error(const char *what, GLenum mode, GLsizei count) {
    if (!gl_log_on() || g_gl_draw_err_logged >= 8) {
        return;
    }
    GLenum (*ge)(void) = lookup_gl("glGetError");
    if (!ge) {
        return;
    }
    GLenum e = ge();
    if (e) {
        g_gl_draw_err_logged++;
        fprintf(stderr, "[gl] %s(mode=0x%x count=%d) -> glErr=0x%x\n", what, (unsigned)mode,
                (int)count, (unsigned)e);
    }
}
static unsigned g_tex_null = 0;
static GLuint g_tex_bound0 = 0;
static unsigned g_draw_untex = 0;
/* Quantas TEXTURAS DISTINTAS o quadro usa. E' o que separa duas leituras muito
   diferentes do Create-A-Sim: se o CAS usa tantas texturas quanto o menu, a
   interface E' desenhada e esta colapsada; se usa bem menos, ela nao e'
   desenhada. A contagem de draws sozinha nao distingue (deu 97 nas tres telas). */
#define CUR_TEX_HIST 64
static GLuint g_frame_tex[CUR_TEX_HIST];
static unsigned g_frame_tex_n = 0;
static void tex_hist_note(GLuint t) {
    for (unsigned i = 0; i < g_frame_tex_n; ++i) {
        if (g_frame_tex[i] == t)
            return;
    }
    if (g_frame_tex_n < CUR_TEX_HIST)
        g_frame_tex[g_frame_tex_n++] = t;
}
static void host_glDrawArrays(GLenum mode, GLint first, GLsizei count) {
    g_gl_draws++;
    if (!g_tex_bound0)
        g_draw_untex++;
    tex_hist_note(g_tex_bound0);
    if (gl_log_on() && g_gl_draw_err_logged < 8) {
        GLenum (*ge)(void) = lookup_gl("glGetError");
        if (ge)
            ge(); /* limpa erro pendente */
    }
    void (*real)(GLenum, GLint, GLsizei) = lookup_gl("glDrawArrays");
    if (real)
        real(mode, first, count);
    gl_probe_draw_error("DrawArrays", mode, count);
}
static void host_glDrawElements(GLenum mode, GLsizei count, GLenum type, const void *indices) {
    g_gl_draws++;
    if (!g_tex_bound0)
        g_draw_untex++;
    tex_hist_note(g_tex_bound0);
    if (gl_log_on() && g_gl_draw_err_logged < 8) {
        GLenum (*ge)(void) = lookup_gl("glGetError");
        if (ge)
            ge();
    }
    void (*real)(GLenum, GLsizei, GLenum, const void *) = lookup_gl("glDrawElements");
    if (real)
        real(mode, count, type, indices);
    gl_probe_draw_error("DrawElements", mode, count);
}
static void host_glClear(GLbitfield mask) {
    g_gl_clears++;
    void (*real)(GLbitfield) = lookup_gl("glClear");
    if (real)
        real(mask);
}
/* Sonda de textura (Sims 3/NextOS): a tela "Select Game" desenha os icones e o
   botao de voltar CHAPADOS de azul-marinho, sem erro de GL, sem falha de shader
   e sem falha de descompressao. As duas explicacoes que sobram sao MUDAS:
   (a) a textura e' criada VAZIA (pixels=NULL) e nunca preenchida;
   (b) o desenho sai com a unidade 0 SEM textura ligada (bind 0).
   Contamos as duas, sempre — como o PROGRAM LINK FAIL. */
static void host_glBindTexture(GLenum target, GLuint texture) {
    if (target == 0x0DE1 /* TEXTURE_2D */) {
        g_tex_bound0 = texture;
    }
    void (*real)(GLenum, GLuint) = lookup_gl("glBindTexture");
    if (real)
        real(target, texture);
}
static void host_glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width,
                              GLsizei height, GLint border, GLenum format, GLenum type,
                              const void *pixels) {
    g_gl_tex++;
    if (!pixels && level == 0) {
        g_tex_null++;
        if (g_tex_null <= 20) {
            fprintf(stderr, "[tex] VAZIA #%u: tex=%u %dx%d fmt=0x%x (pixels=NULL)\n", g_tex_null,
                    g_tex_bound0, (int)width, (int)height, (unsigned)format);
            fflush(stderr);
        }
    }
    if (getenv("SIMS3_TEX_LOG")) {
        fprintf(stderr, "[tex] TexImage2D tex=%u nivel=%d %dx%d fmt=0x%x tipo=0x%x pixels=%s\n",
                g_tex_bound0, (int)level, (int)width, (int)height, (unsigned)format,
                (unsigned)type, pixels ? "sim" : "NULL");
    }
    void (*real)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void *) =
        lookup_gl("glTexImage2D");
    if (real)
        real(target, level, internalformat, width, height, border, format, type, pixels);
}
static void host_glCompressedTexImage2D(GLenum target, GLint level, GLenum internalformat,
                                        GLsizei width, GLsizei height, GLint border,
                                        GLsizei imageSize, const void *data) {
    g_gl_ctex++;
    if (gl_log_on() && g_gl_ctex <= 4) {
        fprintf(stderr, "[gl] CompressedTexImage2D fmt=0x%x %dx%d size=%d\n",
                (unsigned)internalformat, (int)width, (int)height, (int)imageSize);
    }
    void (*real)(GLenum, GLint, GLenum, GLsizei, GLsizei, GLint, GLsizei, const void *) =
        lookup_gl("glCompressedTexImage2D");
    if (real)
        real(target, level, internalformat, width, height, border, imageSize, data);
}
static unsigned g_gl_last_fbo = 0xffffffffu;
static void host_glBindFramebuffer(GLenum target, GLuint framebuffer) {
    if (gl_log_on() && framebuffer != g_gl_last_fbo) {
        g_gl_last_fbo = framebuffer;
        fprintf(stderr, "[gl] BindFramebuffer target=0x%x fbo=%u\n", (unsigned)target, framebuffer);
    }
    void (*real)(GLenum, GLuint) = lookup_gl("glBindFramebuffer");
    if (real)
        real(target, framebuffer);
}
static int g_gl_vp_logged = 0;
static void host_glViewport(GLint x, GLint y, GLsizei w, GLsizei h) {
    if (gl_log_on() && g_gl_vp_logged < 6) {
        g_gl_vp_logged++;
        fprintf(stderr, "[gl] Viewport %d,%d %dx%d\n", (int)x, (int)y, (int)w, (int)h);
    }
    void (*real)(GLint, GLint, GLsizei, GLsizei) = lookup_gl("glViewport");
    if (real)
        real(x, y, w, h);
}
static int g_gl_sh_logged = 0;
static void host_glCompileShader(GLuint shader) {
    void (*real)(GLuint) = lookup_gl("glCompileShader");
    if (real)
        real(shader);
    /* NAO gatilhado por SIMS3_GL_LOG: shader que NAO COMPILA e programa que NAO
       LINKA sao MUDOS — glGetError devolve 0x0 e a tela so fica sem o objeto.
       Regua da casa (mali450 L0010): um programa morto nao produz erro nenhum,
       entao esta checagem tem de valer em TODA corrida, inclusive a do jogador. */
    if (g_gl_sh_logged < 20) {
        void (*getiv)(GLuint, GLenum, GLint *) = lookup_gl("glGetShaderiv");
        void (*getlog)(GLuint, GLsizei, GLsizei *, char *) = lookup_gl("glGetShaderInfoLog");
        GLint ok = 1;
        if (getiv)
            getiv(shader, 0x8B81 /*COMPILE_STATUS*/, &ok);
        if (!ok) {
            char buf[512] = "";
            if (getlog)
                getlog(shader, sizeof(buf), NULL, buf);
            g_gl_sh_logged++;
            fprintf(stderr, "[gl] SHADER COMPILE FAIL: %s\n", buf);
        }
    }
}
static void host_glLinkProgram(GLuint program) {
    void (*real)(GLuint) = lookup_gl("glLinkProgram");
    if (real)
        real(program);
    if (g_gl_sh_logged < 20) {
        void (*getiv)(GLuint, GLenum, GLint *) = lookup_gl("glGetProgramiv");
        void (*getlog)(GLuint, GLsizei, GLsizei *, char *) = lookup_gl("glGetProgramInfoLog");
        GLint ok = 1;
        if (getiv)
            getiv(program, 0x8B82 /*LINK_STATUS*/, &ok);
        if (!ok) {
            char buf[512] = "";
            if (getlog)
                getlog(program, sizeof(buf), NULL, buf);
            g_gl_sh_logged++;
            fprintf(stderr, "[gl] PROGRAM LINK FAIL: %s\n", buf);
        }
    }
}

static void *host_eglGetProcAddress(const char *procname) {
    if (!procname) {
        return NULL;
    }
    void *wrapped = resolve_wrapped_host_proc(procname);
    if (wrapped) {
        return wrapped;
    }

    void *(*real)(const char *) = lookup_egl("eglGetProcAddress");
    return real ? real(procname) : NULL;
}

/* ---- ponteiro desenhado por NOS (overlay GLES2) ----
 * O Sims 3 e' um jogo de toque: no telefone o dedo E' o cursor, entao o jogo nao
 * desenha ponteiro nenhum. Sem uma seta visivel o ponteiro virtual do pad e'
 * injogavel.
 * A ARTE NAO E' NOSSA: e' a mesma seta 64x64 com anti-aliasing e sombra de
 * ports/summertimesaga (reaproveitada em masseffect, deadspace e peacedeath),
 * em src/cursor_arrow.h -- textura RGBA com ALFA PREMULTIPLICADO.
 * A versao anterior desenhava uma cruz com glClear+scissor. Trocada por quad
 * texturizado: glClear OBEDECE AO ESTENCIL, e o jogo deixa estencil ligado. */
#include "cursor_arrow.h"

/* Este arquivo nao inclui cabecalho de GL — as constantes usadas aqui, e so aqui. */
#define CUR_GL_TEXTURE_2D 0x0DE1
#define CUR_GL_RGBA 0x1908
#define CUR_GL_UNSIGNED_BYTE 0x1401
#define CUR_GL_FLOAT 0x1406
#define CUR_GL_TEXTURE0 0x84C0
#define CUR_GL_ACTIVE_TEXTURE 0x84E0
#define CUR_GL_TEXTURE_BINDING_2D 0x8069
#define CUR_GL_TEXTURE_MIN_FILTER 0x2801
#define CUR_GL_TEXTURE_MAG_FILTER 0x2800
#define CUR_GL_TEXTURE_WRAP_S 0x2802
#define CUR_GL_TEXTURE_WRAP_T 0x2803
#define CUR_GL_LINEAR 0x2601
#define CUR_GL_LINEAR_MIPMAP_LINEAR 0x2703
#define CUR_GL_CLAMP_TO_EDGE 0x812F
#define CUR_GL_VERTEX_SHADER 0x8B31
#define CUR_GL_FRAGMENT_SHADER 0x8B30
#define CUR_GL_COMPILE_STATUS 0x8B81
#define CUR_GL_LINK_STATUS 0x8B82
#define CUR_GL_CURRENT_PROGRAM 0x8B8D
#define CUR_GL_ARRAY_BUFFER 0x8892
#define CUR_GL_ARRAY_BUFFER_BINDING 0x8894
#define CUR_GL_VIEWPORT 0x0BA2
#define CUR_GL_BLEND 0x0BE2
#define CUR_GL_DEPTH_TEST 0x0B71
#define CUR_GL_CULL_FACE 0x0B44
#define CUR_GL_SCISSOR_TEST 0x0C11
#define CUR_GL_STENCIL_TEST 0x0B90
#define CUR_GL_BLEND_SRC_RGB 0x80C9
#define CUR_GL_BLEND_DST_RGB 0x80C8
#define CUR_GL_BLEND_SRC_ALPHA 0x80CB
#define CUR_GL_BLEND_DST_ALPHA 0x80CA
#define CUR_GL_BLEND_EQUATION_RGB 0x8009
#define CUR_GL_BLEND_EQUATION_ALPHA 0x883D
#define CUR_GL_FUNC_ADD 0x8006
#define CUR_GL_ONE 1
#define CUR_GL_ONE_MINUS_SRC_ALPHA 0x0303
#define CUR_GL_COLOR_WRITEMASK 0x0C23
#define CUR_GL_TRIANGLE_STRIP 5
#define CUR_GL_FALSE 0
#define CUR_GL_TRUE 1
#define CUR_GL_VAA_ENABLED 0x8622
#define CUR_GL_VAA_SIZE 0x8623
#define CUR_GL_VAA_STRIDE 0x8624
#define CUR_GL_VAA_TYPE 0x8625
#define CUR_GL_VAA_NORMALIZED 0x886A
#define CUR_GL_VAA_BUFFER_BINDING 0x889F
#define CUR_GL_VAA_POINTER 0x8645

struct cursor_gl {
    void (*ActiveTexture)(GLenum);
    void (*AttachShader)(GLuint, GLuint);
    void (*BindAttribLocation)(GLuint, GLuint, const char *);
    void (*BindBuffer)(GLenum, GLuint);
    void (*BindTexture)(GLenum, GLuint);
    void (*BlendEquation)(GLenum);
    void (*BlendEquationSeparate)(GLenum, GLenum);
    void (*BlendFuncSeparate)(GLenum, GLenum, GLenum, GLenum);
    void (*ColorMask)(GLboolean, GLboolean, GLboolean, GLboolean);
    void (*CompileShader)(GLuint);
    GLuint (*CreateProgram)(void);
    GLuint (*CreateShader)(GLenum);
    void (*DeleteProgram)(GLuint);
    void (*DeleteShader)(GLuint);
    void (*Disable)(GLenum);
    void (*DisableVertexAttribArray)(GLuint);
    void (*DrawArrays)(GLenum, GLint, GLsizei);
    void (*Enable)(GLenum);
    void (*EnableVertexAttribArray)(GLuint);
    void (*GenTextures)(GLsizei, GLuint *);
    void (*GenerateMipmap)(GLenum);
    void (*GetBooleanv)(GLenum, GLboolean *);
    void (*GetIntegerv)(GLenum, GLint *);
    void (*GetProgramInfoLog)(GLuint, GLsizei, GLsizei *, char *);
    void (*GetProgramiv)(GLuint, GLenum, GLint *);
    void (*GetShaderInfoLog)(GLuint, GLsizei, GLsizei *, char *);
    void (*GetShaderiv)(GLuint, GLenum, GLint *);
    GLint (*GetUniformLocation)(GLuint, const char *);
    void (*GetVertexAttribPointerv)(GLuint, GLenum, void **);
    void (*GetVertexAttribiv)(GLuint, GLenum, GLint *);
    GLboolean (*IsEnabled)(GLenum);
    void (*LinkProgram)(GLuint);
    void (*ShaderSource)(GLuint, GLsizei, const char *const *, const GLint *);
    void (*TexImage2D)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum,
                       const void *);
    void (*TexParameteri)(GLenum, GLenum, GLint);
    void (*Uniform1i)(GLint, GLint);
    void (*Uniform3f)(GLint, GLfloat, GLfloat, GLfloat);
    void (*UseProgram)(GLuint);
    void (*VertexAttribPointer)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void *);
    void (*Viewport)(GLint, GLint, GLsizei, GLsizei);
};

static struct cursor_gl g_cur_gl;
static GLuint g_cur_prog = 0;
static GLuint g_cur_tex = 0;
static GLint g_cur_u_tint = -1;
static int g_cur_failed = 0;

/* Um ponteiro nulo aqui viraria SIGSEGV dentro do present do jogo. Resolver
   tudo de uma vez e desistir inteiro se faltar qualquer entrada. */
static int cursor_gl_resolve(void) {
    struct cursor_gl *g = &g_cur_gl;
    void **slot = (void **)g;
    static const char *const names[] = {
        "glActiveTexture", "glAttachShader", "glBindAttribLocation", "glBindBuffer",
        "glBindTexture", "glBlendEquation", "glBlendEquationSeparate", "glBlendFuncSeparate",
        "glColorMask", "glCompileShader", "glCreateProgram", "glCreateShader",
        "glDeleteProgram", "glDeleteShader", "glDisable", "glDisableVertexAttribArray",
        "glDrawArrays", "glEnable", "glEnableVertexAttribArray", "glGenTextures",
        "glGenerateMipmap", "glGetBooleanv", "glGetIntegerv", "glGetProgramInfoLog",
        "glGetProgramiv", "glGetShaderInfoLog", "glGetShaderiv", "glGetUniformLocation",
        "glGetVertexAttribPointerv", "glGetVertexAttribiv", "glIsEnabled", "glLinkProgram",
        "glShaderSource", "glTexImage2D", "glTexParameteri", "glUniform1i", "glUniform3f",
        "glUseProgram", "glVertexAttribPointer", "glViewport",
    };
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i) {
        void *fn = lookup_gl(names[i]);
        if (!fn) {
            fprintf(stderr, "[cursor] %s ausente — seta desligada\n", names[i]);
            return 0;
        }
        slot[i] = fn;
    }
    return 1;
}

static GLuint cursor_compile(GLenum type, const char *src) {
    struct cursor_gl *g = &g_cur_gl;
    GLuint sh = g->CreateShader(type);
    GLint ok = 0;
    char log[512];
    g->ShaderSource(sh, 1, &src, NULL);
    g->CompileShader(sh);
    g->GetShaderiv(sh, CUR_GL_COMPILE_STATUS, &ok);
    if (!ok) {
        g->GetShaderInfoLog(sh, sizeof(log), NULL, log);
        fprintf(stderr, "[cursor] shader falhou: %s\n", log);
        g->DeleteShader(sh);
        return 0;
    }
    return sh;
}

static void cursor_init(void) {
    struct cursor_gl *g = &g_cur_gl;
    /* Mali-450 (Utgard): todo uniform em mediump nos DOIS estagios — divergencia
       de precisao derruba o link com L0010 e ZERO erro de compilacao. */
    static const char *vs = "attribute vec2 a_pos;\n"
                            "attribute vec2 a_uv;\n"
                            "varying mediump vec2 v_uv;\n"
                            "void main(){ v_uv = a_uv; gl_Position = vec4(a_pos, 0.0, 1.0); }\n";
    static const char *fs = "precision mediump float;\n"
                            "uniform sampler2D u_tex;\n"
                            "uniform mediump vec3 u_tint;\n"
                            "varying mediump vec2 v_uv;\n"
                            "void main(){ mediump vec4 c = texture2D(u_tex, v_uv);\n"
                            "  gl_FragColor = vec4(c.rgb * u_tint, c.a); }\n";
    GLuint v = cursor_compile(CUR_GL_VERTEX_SHADER, vs);
    GLuint f = cursor_compile(CUR_GL_FRAGMENT_SHADER, fs);
    if (!v || !f) {
        if (v)
            g->DeleteShader(v);
        if (f)
            g->DeleteShader(f);
        g_cur_failed = 1;
        return;
    }
    g_cur_prog = g->CreateProgram();
    g->BindAttribLocation(g_cur_prog, 0, "a_pos");
    g->BindAttribLocation(g_cur_prog, 1, "a_uv");
    g->AttachShader(g_cur_prog, v);
    g->AttachShader(g_cur_prog, f);
    g->LinkProgram(g_cur_prog);
    g->DeleteShader(v);
    g->DeleteShader(f);
    GLint ok = 0;
    char log[512];
    g->GetProgramiv(g_cur_prog, CUR_GL_LINK_STATUS, &ok);
    if (!ok) {
        g->GetProgramInfoLog(g_cur_prog, sizeof(log), NULL, log);
        fprintf(stderr, "[cursor] link falhou: %s\n", log);
        g->DeleteProgram(g_cur_prog);
        g_cur_prog = 0;
        g_cur_failed = 1;
        return;
    }
    g_cur_u_tint = g->GetUniformLocation(g_cur_prog, "u_tint");
    GLint u_tex = g->GetUniformLocation(g_cur_prog, "u_tex");
    GLint prev_prog = 0;
    g->GetIntegerv(CUR_GL_CURRENT_PROGRAM, &prev_prog);
    g->UseProgram(g_cur_prog);
    g->Uniform1i(u_tex, 0);
    g->UseProgram((GLuint)prev_prog);
    g->GenTextures(1, &g_cur_tex);
    GLint prev_tex = 0;
    g->GetIntegerv(CUR_GL_TEXTURE_BINDING_2D, &prev_tex);
    g->BindTexture(CUR_GL_TEXTURE_2D, g_cur_tex);
    g->TexImage2D(CUR_GL_TEXTURE_2D, 0, CUR_GL_RGBA, SIMS3_ARROW_TEX_SIZE, SIMS3_ARROW_TEX_SIZE, 0,
                  CUR_GL_RGBA, CUR_GL_UNSIGNED_BYTE, sims3_arrow_tex);
    g->GenerateMipmap(CUR_GL_TEXTURE_2D); /* a seta encolhe com a tela: sem mipmap, serrilha */
    g->TexParameteri(CUR_GL_TEXTURE_2D, CUR_GL_TEXTURE_MIN_FILTER, CUR_GL_LINEAR_MIPMAP_LINEAR);
    g->TexParameteri(CUR_GL_TEXTURE_2D, CUR_GL_TEXTURE_MAG_FILTER, CUR_GL_LINEAR);
    g->TexParameteri(CUR_GL_TEXTURE_2D, CUR_GL_TEXTURE_WRAP_S, CUR_GL_CLAMP_TO_EDGE);
    g->TexParameteri(CUR_GL_TEXTURE_2D, CUR_GL_TEXTURE_WRAP_T, CUR_GL_CLAMP_TO_EDGE);
    g->BindTexture(CUR_GL_TEXTURE_2D, (GLuint)prev_tex);
    fprintf(stderr, "[cursor] seta texturizada pronta (prog=%u tex=%u)\n", g_cur_prog, g_cur_tex);
}

struct cursor_attrib {
    GLint enabled, size, type, normalized, stride, buffer;
    void *pointer;
};

static void cursor_save_attrib(GLuint i, struct cursor_attrib *s) {
    struct cursor_gl *g = &g_cur_gl;
    g->GetVertexAttribiv(i, CUR_GL_VAA_ENABLED, &s->enabled);
    g->GetVertexAttribiv(i, CUR_GL_VAA_SIZE, &s->size);
    g->GetVertexAttribiv(i, CUR_GL_VAA_TYPE, &s->type);
    g->GetVertexAttribiv(i, CUR_GL_VAA_NORMALIZED, &s->normalized);
    g->GetVertexAttribiv(i, CUR_GL_VAA_STRIDE, &s->stride);
    g->GetVertexAttribiv(i, CUR_GL_VAA_BUFFER_BINDING, &s->buffer);
    g->GetVertexAttribPointerv(i, CUR_GL_VAA_POINTER, &s->pointer);
}

static void cursor_restore_attrib(GLuint i, const struct cursor_attrib *s) {
    struct cursor_gl *g = &g_cur_gl;
    g->BindBuffer(CUR_GL_ARRAY_BUFFER, (GLuint)s->buffer);
    if (s->size)
        g->VertexAttribPointer(i, s->size, (GLenum)s->type, (GLboolean)s->normalized, s->stride,
                               s->pointer);
    if (s->enabled)
        g->EnableVertexAttribArray(i);
    else
        g->DisableVertexAttribArray(i);
}

static void frontend_cursor_gl_present(void) {
    if (!g_cursor_active || g_cur_failed) {
        return;
    }
    /* SIMS3_NO_CURSOR=1: apaga a mira para MEDIR. A seta e' desenhada por NOS
       dentro do framebuffer, entao qualquer comparacao de /dev/fb0 feita depois
       de mexer no ponteiro acusa "mudou" mesmo com o jogo parado — ja' produziu
       duas conclusoes erradas hoje. Com a mira fora, a captura e' so' o jogo. */
    {
        static int oculta = -1;
        if (oculta < 0) {
            const char *e = getenv("SIMS3_NO_CURSOR");
            oculta = e && e[0] == '1';
        }
        if (oculta) {
            return;
        }
    }
    struct cursor_gl *g = &g_cur_gl;
    if (!g->DrawArrays) {
        if (!cursor_gl_resolve()) {
            g_cur_failed = 1;
            return;
        }
    }
    if (!g_cur_prog) {
        cursor_init();
        if (!g_cur_prog) {
            return;
        }
    }
    const int W = (int)g_native_window.width;
    const int H = (int)g_native_window.height;
    if (W <= 0 || H <= 0) {
        return;
    }
    /* altura da seta proporcional a tela (~5,2%): jamais cravar resolucao. */
    float h_px = (float)H * 0.052f;
    if (h_px < 24.0f)
        h_px = 24.0f;
    if (h_px > 96.0f)
        h_px = 96.0f;
    {
        const char *e = getenv("SIMS3_CURSCALE");
        if (e) {
            float fscale = (float)atof(e);
            if (fscale > 0.1f && fscale < 10.0f)
                h_px *= fscale;
        }
    }
    const float scale = h_px / SIMS3_ARROW_DESIGN_H;
    const float quad = (float)SIMS3_ARROW_TEX_SIZE * scale;
    /* a PONTA da seta fica exatamente em (g_pointer_x,g_pointer_y) — e' ela que toca */
    const float qx = (float)g_pointer_x - SIMS3_ARROW_TIP_X * scale;
    const float qy = (float)g_pointer_y - SIMS3_ARROW_TIP_Y * scale;
    const float x0 = 2.0f * qx / (float)W - 1.0f;
    const float x1 = 2.0f * (qx + quad) / (float)W - 1.0f;
    const float y0 = 1.0f - 2.0f * qy / (float)H;
    const float y1 = 1.0f - 2.0f * (qy + quad) / (float)H;
    const float verts[16] = {x0, y0, 0.0f, 0.0f, x1, y0, 1.0f, 0.0f,
                             x0, y1, 0.0f, 1.0f, x1, y1, 1.0f, 1.0f};

    /* ---- salvar estado (o jogo deixa tudo do jeito dele) ---- */
    GLint prev_prog = 0, prev_active = 0, prev_tex = 0, prev_ab = 0, prev_vp[4] = {0, 0, 0, 0};
    GLint b_srgb = 0, b_drgb = 0, b_sa = 0, b_da = 0, b_eq_rgb = 0, b_eq_a = 0;
    GLboolean prev_cmask[4] = {CUR_GL_TRUE, CUR_GL_TRUE, CUR_GL_TRUE, CUR_GL_TRUE};
    GLboolean en_blend = g->IsEnabled(CUR_GL_BLEND);
    GLboolean en_depth = g->IsEnabled(CUR_GL_DEPTH_TEST);
    GLboolean en_cull = g->IsEnabled(CUR_GL_CULL_FACE);
    GLboolean en_scissor = g->IsEnabled(CUR_GL_SCISSOR_TEST);
    GLboolean en_stencil = g->IsEnabled(CUR_GL_STENCIL_TEST);
    g->GetIntegerv(CUR_GL_CURRENT_PROGRAM, &prev_prog);
    g->GetIntegerv(CUR_GL_ACTIVE_TEXTURE, &prev_active);
    g->GetIntegerv(CUR_GL_ARRAY_BUFFER_BINDING, &prev_ab);
    g->GetIntegerv(CUR_GL_VIEWPORT, prev_vp);
    g->GetIntegerv(CUR_GL_BLEND_SRC_RGB, &b_srgb);
    g->GetIntegerv(CUR_GL_BLEND_DST_RGB, &b_drgb);
    g->GetIntegerv(CUR_GL_BLEND_SRC_ALPHA, &b_sa);
    g->GetIntegerv(CUR_GL_BLEND_DST_ALPHA, &b_da);
    g->GetIntegerv(CUR_GL_BLEND_EQUATION_RGB, &b_eq_rgb);
    g->GetIntegerv(CUR_GL_BLEND_EQUATION_ALPHA, &b_eq_a);
    g->GetBooleanv(CUR_GL_COLOR_WRITEMASK, prev_cmask);
    g->ActiveTexture(CUR_GL_TEXTURE0);
    g->GetIntegerv(CUR_GL_TEXTURE_BINDING_2D, &prev_tex);
    struct cursor_attrib a0, a1;
    cursor_save_attrib(0, &a0);
    cursor_save_attrib(1, &a1);

    /* ---- desenhar ---- */
    g->UseProgram(g_cur_prog);
    g->BindTexture(CUR_GL_TEXTURE_2D, g_cur_tex);
    g->BindBuffer(CUR_GL_ARRAY_BUFFER, 0);
    g->Viewport(0, 0, W, H);
    g->Disable(CUR_GL_DEPTH_TEST);
    g->Disable(CUR_GL_CULL_FACE);
    g->Disable(CUR_GL_SCISSOR_TEST);
    g->Disable(CUR_GL_STENCIL_TEST);
    /* seguro medido no partyhard: a engine pode deixar um canal do colorMask
       desligado e o overlay some SEM ERRO NENHUM, com o log limpo. */
    g->ColorMask(CUR_GL_TRUE, CUR_GL_TRUE, CUR_GL_TRUE, CUR_GL_TRUE);
    g->Enable(CUR_GL_BLEND);
    g->BlendEquation(CUR_GL_FUNC_ADD);
    /* alfa PREMULTIPLICADO na arte */
    g->BlendFuncSeparate(CUR_GL_ONE, CUR_GL_ONE_MINUS_SRC_ALPHA, CUR_GL_ONE,
                         CUR_GL_ONE_MINUS_SRC_ALPHA);
    if (g_pointer_down)
        g->Uniform3f(g_cur_u_tint, 1.0f, 0.80f, 0.25f); /* dedo encostado */
    else
        g->Uniform3f(g_cur_u_tint, 1.0f, 1.0f, 1.0f);
    g->VertexAttribPointer(0, 2, CUR_GL_FLOAT, CUR_GL_FALSE, 16, verts);
    g->VertexAttribPointer(1, 2, CUR_GL_FLOAT, CUR_GL_FALSE, 16, verts + 2);
    g->EnableVertexAttribArray(0);
    g->EnableVertexAttribArray(1);
    g->DrawArrays(CUR_GL_TRIANGLE_STRIP, 0, 4);

    /* ---- restaurar estado ---- */
    cursor_restore_attrib(0, &a0);
    cursor_restore_attrib(1, &a1);
    g->BindBuffer(CUR_GL_ARRAY_BUFFER, (GLuint)prev_ab);
    g->BindTexture(CUR_GL_TEXTURE_2D, (GLuint)prev_tex);
    g->ActiveTexture((GLenum)prev_active);
    g->UseProgram((GLuint)prev_prog);
    g->Viewport(prev_vp[0], prev_vp[1], prev_vp[2], prev_vp[3]);
    g->BlendEquationSeparate((GLenum)b_eq_rgb, (GLenum)b_eq_a);
    g->BlendFuncSeparate((GLenum)b_srgb, (GLenum)b_drgb, (GLenum)b_sa, (GLenum)b_da);
    g->ColorMask(prev_cmask[0], prev_cmask[1], prev_cmask[2], prev_cmask[3]);
    if (!en_blend)
        g->Disable(CUR_GL_BLEND);
    if (en_depth)
        g->Enable(CUR_GL_DEPTH_TEST);
    if (en_cull)
        g->Enable(CUR_GL_CULL_FACE);
    if (en_scissor)
        g->Enable(CUR_GL_SCISSOR_TEST);
    if (en_stencil)
        g->Enable(CUR_GL_STENCIL_TEST);
}

/* The Sims 3 (NextOS): o jogo IMPORTA glGetError e o consulta ele mesmo. Duas
   consequencias:
   1) o `glErr=0x500` que aparece no resumo por quadro pode ser NOSSO glGetError
      roubando o erro do jogo (quem le, limpa) — por isso o resumo so le o erro
      quando SIMS3_GL_LOG esta ligado;
   2) embrulhando glGetError da' para saber ONDE o jogo notou o erro: o endereco
      de retorno aponta para o sitio de chamada dentro do modulo, o que localiza
      a operacao GL que falhou sem instrumentar as 227 funcoes. */
static GLenum host_glGetError(void) {
    GLenum (*real)(void) = lookup_gl("glGetError");
    GLenum err = real ? real() : 0;
    if (err && getenv("SIMS3_GLERR_LOG")) {
        static unsigned budget = 40;
        if (budget) {
            budget--;
            void *ra = __builtin_return_address(0);
            uintptr_t off = g_module_base && (uintptr_t)ra >= g_module_base
                                ? (uintptr_t)ra - g_module_base
                                : 0;
            fprintf(stderr, "[glerr] o jogo leu erro 0x%x em modulo+0x%lx\n", err,
                    (unsigned long)off);
            fflush(stderr);
        }
    }
    return err;
}

static EGLBoolean host_eglSwapBuffers(EGLDisplay display, EGLSurface surface) {
    EGLBoolean (*real)(EGLDisplay, EGLSurface) = lookup_egl("eglSwapBuffers");
    /* The Sims 3 (NextOS): o jogo chama s3ePointerUpdate TODO quadro (medido:
       2610 chamadas em 2940 quadros) e consome o toque SO por callback (zero
       polls de GetState/GetX/GetY). Despachar tambem daqui faria o callback do
       jogo rodar DENTRO do present — reentrante no proprio quadro. Regua da
       casa (codboz/audio): despachar no pump do frame, nunca reentrante. */
    if (!getenv("SIMS3_PUMP_ON_SWAP")) {
        input_pump_no_dispatch();
    } else {
        input_pump();
    }
    dispatch_due_timers();
    audio_pump(); /* NextOS: despacha END_SAMPLE p/ engine reciclar as vozes */
    /* NFS Shift (NextOS): resumo de render por frame (diag tela branca). */
    if (gl_log_on() && (g_gl_frame % 60u) == 0u) {
        GLenum (*gl_get_error)(void) = lookup_gl("glGetError");
        GLenum err = gl_get_error ? gl_get_error() : 0;
        fprintf(stderr,
                "[gl] frame=%u draws=%u semTex=%u texDistintas=%u tex=%u ctex=%u clears=%u "
                "texVazia=%u glErr=0x%x\n",
                g_gl_frame, g_gl_draws, g_draw_untex, g_frame_tex_n, g_gl_tex, g_gl_ctex,
                g_gl_clears, g_tex_null, (unsigned)err);
        s3e_pointer_poll_stats();
        {
            extern unsigned long s3e_timer_ms_calls(void);
            extern unsigned long s3e_timer_ust_calls(void);
            extern unsigned long s3e_yield_calls(void);
            extern unsigned long s3e_yield_evt_calls(void);
            static unsigned long p_ms = 0, p_ust = 0, p_y = 0, p_ye = 0;
            unsigned long ms = s3e_timer_ms_calls(), ust = s3e_timer_ust_calls();
            unsigned long y = s3e_yield_calls(), ye = s3e_yield_evt_calls();
            extern unsigned long s3e_timer_set_count(void);
            extern unsigned long s3e_timer_fired_count(void);
            extern unsigned long s3e_timer_cancel_count(void);
            static unsigned long p_set = 0, p_fire = 0, p_can = 0;
            unsigned long tset = s3e_timer_set_count(), tfire = s3e_timer_fired_count();
            unsigned long tcan = s3e_timer_cancel_count();
            fprintf(stderr,
                    "[laco] em 60 quadros: GetMs=+%lu UST=+%lu Yield=+%lu YieldEvt=+%lu "
                    "SetTimer=+%lu (total %lu) disparados=+%lu (total %lu) cancelados=+%lu\n",
                    ms - p_ms, ust - p_ust, y - p_y, ye - p_ye, tset - p_set, tset,
                    tfire - p_fire, tfire, tcan - p_can);
            p_ms = ms; p_ust = ust; p_y = y; p_ye = ye;
            p_set = tset; p_fire = tfire; p_can = tcan;
            {
                extern unsigned long g_thread_slot_calls[20];
                static unsigned long prev_slot[20];
                char linha[256];
                int n = 0;
                linha[0] = 0;
                for (int i = 0; i < 20; ++i) {
                    unsigned long d = g_thread_slot_calls[i] - prev_slot[i];
                    prev_slot[i] = g_thread_slot_calls[i];
                    if (d) {
                        n += snprintf(linha + n, sizeof(linha) - (size_t)n, " s%d=+%lu", i, d);
                        if (n >= (int)sizeof(linha) - 16) break;
                    }
                }
                fprintf(stderr, "[thread] slots em 60 quadros:%s\n", n ? linha : " (nenhum)");
            }
        }
        if (getenv("SIMS3_FILEOP_LOG")) {
            extern unsigned long g_fileop_read, g_fileop_getchar;
            fprintf(stderr, "[fileop] s3eFileRead=%lu s3eFileGetChar=%lu\n", g_fileop_read,
                    g_fileop_getchar);
        }
        s3e_module_peek();
        audio_peak_report();
        if (getenv("SIMS3_HOOK_VT") || getenv("SIMS3_HOOK_BL") || getenv("SIMS3_HOOK_FN")) {
            s3e_hook_report();
        }
    }
    g_gl_frame++;
    g_gl_draws = 0;
    g_draw_untex = 0;
    g_frame_tex_n = 0;
    g_gl_tex = 0;
    g_gl_ctex = 0;
    g_gl_clears = 0;
    frontend_cursor_gl_present();
    EGLBoolean result = real ? real(display, surface) : 0;
    pace_frame();
    return result;
}

struct host_symbol {
    const char *name;
    void *fn;
};

#define HOST(name) {#name, (void *)(uintptr_t)&name}

static const struct host_symbol HOST_SYMBOLS[] = {
    HOST(s3eMallocBase),
    HOST(s3eReallocBase),
    HOST(s3eFreeBase),
    HOST(s3eFileOpen),
    HOST(s3eFileClose),
    HOST(s3eFileRead),
    HOST(s3eFileWrite),
    HOST(s3eFileGetChar),
    HOST(s3eFilePutChar),
    HOST(s3eFileFlush),
    HOST(s3eFileSeek),
    HOST(s3eFileTell),
    HOST(s3eFileGetSize),
    HOST(s3eFileEOF),
    HOST(s3eFileCheckExists),
    HOST(s3eFileGetError),
    HOST(s3eFileGetErrorString),
    HOST(s3eFileOpenFromMemory),
    HOST(s3eFileGetFileInt),
    HOST(s3eFileMakeDirectory),
    HOST(s3eFileDelete),
    HOST(s3eFileRename),
    HOST(s3eFileAddUserFileSys),
    HOST(s3eFileListDirectory),
    HOST(s3eFileListClose),
    HOST(s3eCompressionDecomp),
    HOST(s3eCompressionDecompInit),
    HOST(s3eCompressionDecompRead),
    HOST(s3eCompressionDecompFinal),
    HOST(s3eTimerGetUST),
    HOST(s3eTimerGetMs),
    HOST(s3eTimerGetInt),
    HOST(s3eTimerSetTimer),
    HOST(s3eTimerCancelTimer),
    HOST(s3eTimerGetUTC),
    HOST(s3eTimerGetLocaltimeOffset),
    HOST(s3eDeviceRegister),
    HOST(s3eDeviceUnRegister),
    HOST(s3eDeviceYield),
    HOST(s3eDeviceYieldUntilEvent),
    HOST(s3eDeviceCheckQuitRequest),
    HOST(s3eDeviceCheckPauseRequest),
    HOST(s3eDeviceGetInt),
    HOST(s3eDeviceGetString),
    HOST(s3eDeviceSetInt),
    HOST(s3eDeviceBacklightOn),
    HOST(s3eDeviceRequestQuit),
    HOST(s3eDeviceAbort),
    HOST(s3eDeviceExit),
    HOST(s3eDebugOutputString),
    HOST(s3eDebugPrint),
    HOST(s3eDebugGetInt),
    HOST(s3eDebugIsDebuggerPresent),
    HOST(s3eDebugTraceLine),
    HOST(s3eDebugAssertShow),
    HOST(s3eDebugErrorShow),
    HOST(s3eKeyboardRegister),
    HOST(s3eKeyboardUnRegister),
    HOST(s3eKeyboardUpdate),
    HOST(s3eKeyboardGetState),
    HOST(s3eKeyboardAnyKey),
    HOST(s3eKeyboardGetInt),
    HOST(s3eKeyboardSetInt),
    HOST(s3eKeyboardGetDisplayName),
    HOST(s3eKeyboardClearState),
    HOST(s3ePointerRegister),
    HOST(s3ePointerUnRegister),
    HOST(s3ePointerUpdate),
    HOST(s3ePointerGetInt),
    HOST(s3ePointerSetInt),
    HOST(s3ePointerGetState),
    HOST(s3ePointerGetX),
    HOST(s3ePointerGetY),
    HOST(s3ePointerGetTouchState),
    HOST(s3ePointerGetTouchX),
    HOST(s3ePointerGetTouchY),
    HOST(s3ePointerGetPressure),
    HOST(s3ePointerGetTouchPressure),
    HOST(s3ePointerGetError),
    HOST(s3ePointerGetErrorString),
    HOST(s3eAccelerometerStart),
    HOST(s3eAccelerometerStop),
    HOST(s3eAccelerometerGetX),
    HOST(s3eAccelerometerGetY),
    HOST(s3eAccelerometerGetZ),
    HOST(s3eAccelerometerGetInt),
    HOST(s3eVideoGetInt),
    HOST(s3eVideoPlay),
    HOST(s3eVideoStop),
    HOST(s3eVideoResume),
    HOST(s3eAudioIsPlaying),
    HOST(s3eAudioSetInt),
    HOST(s3eAudioGetInt),
    HOST(s3eAudioPlay),
    HOST(s3eAudioPlayFromBuffer),
    HOST(s3eAudioStop),
    HOST(s3eAudioPause),
    HOST(s3eAudioResume),
    HOST(s3eAudioRegister),
    HOST(s3eAudioGetError),
    HOST(s3eAudioIsCodecSupported),
    HOST(s3eSoundGetFreeChannel),
    HOST(s3eSoundSetInt),
    HOST(s3eSoundGetInt),
    HOST(s3eSoundChannelRegister),
    HOST(s3eSoundChannelUnRegister),
    HOST(s3eSoundChannelPlay),
    HOST(s3eSoundChannelStop),
    HOST(s3eSoundChannelPause),
    HOST(s3eSoundChannelResume),
    HOST(s3eSoundChannelSetInt),
    HOST(s3eSoundChannelGetInt),
    HOST(s3eSoundStopAllChannels),
    HOST(s3eInetHtonl),
    HOST(s3eInetNtohl),
    HOST(s3eInetHtons),
    HOST(s3eInetNtohs),
    HOST(s3eInetAton),
    HOST(s3eInetNtoa),
    HOST(s3eInetToString),
    HOST(s3eInetLookup),
    HOST(s3eInetLookupCancel),
    HOST(s3eSocketCreate),
    HOST(s3eSocketClose),
    HOST(s3eSocketBind),
    HOST(s3eSocketListen),
    HOST(s3eSocketAccept),
    HOST(s3eSocketConnect),
    HOST(s3eSocketSend),
    HOST(s3eSocketSendTo),
    HOST(s3eSocketRecv),
    HOST(s3eSocketRecvFrom),
    HOST(s3eSocketReadable),
    HOST(s3eSocketWritable),
    HOST(s3eSocketGetInt),
    HOST(s3eSocketGetError),
    HOST(s3eSocketGetString),
    HOST(s3eSocketGetLocalName),
    HOST(s3eSocketGetPeerName),
    HOST(s3eMemoryGetInt),
    HOST(s3eMemorySetInt),
    HOST(s3eMemorySetUserMemMgr),
    HOST(s3eMemoryGetUserMemMgr),
    HOST(s3eMemoryHeapCreate),
    HOST(s3eMemoryHeapDestroy),
    HOST(s3eMemoryHeapAddress),
    HOST(s3eMemoryGetError),
    HOST(s3eMemoryGetErrorString),
    HOST(s3eSurfaceRegister),
    HOST(s3eSurfaceUnRegister),
    HOST(s3eSurfaceGetInt),
    HOST(s3eSurfacePtr),
    HOST(s3eSurfaceSetup),
    HOST(s3eSurfaceShow),
    HOST(s3eGLRegister),
    HOST(s3eGLUnRegister),
    HOST(s3eGLGetInt),
    HOST(s3eGLGetNativeWindow),
    HOST(s3eConfigGetInt),
    HOST(s3eConfigGetString),
    HOST(s3eExtGetHash),
};

static void *resolve_wrapped_host_proc(const char *symbol) {
    /* NFS Shift (NextOS): wrappers de contagem p/ diagnostico de render (so
       encaminham + contam; sem efeito colateral no jogo). */
    /* 🚨 ARMADILHA MEDIDA (02/09): estes wrappers estavam TODOS atras de
       gl_log_on(), inclusive os de shader. Sem SIMS3_GL_LOG o wrapper nem era
       instalado, entao "zero PROGRAM LINK FAIL" numa corrida normal NAO provava
       nada — era so' a ausencia do checador. Os que apenas CONTAM ou detectam
       falha MUDA passam a valer sempre; so' o que POLUI o log (erro por draw)
       continua atras da variavel. */
    if (strcmp(symbol, "glDrawArrays") == 0)
        return host_glDrawArrays;
    if (strcmp(symbol, "glDrawElements") == 0)
        return host_glDrawElements;
    if (strcmp(symbol, "glClear") == 0)
        return host_glClear;
    if (strcmp(symbol, "glTexImage2D") == 0)
        return host_glTexImage2D;
    if (strcmp(symbol, "glCompressedTexImage2D") == 0)
        return host_glCompressedTexImage2D;
    if (strcmp(symbol, "glBindTexture") == 0)
        return host_glBindTexture;
    if (strcmp(symbol, "glCompileShader") == 0)
        return host_glCompileShader;
    if (strcmp(symbol, "glLinkProgram") == 0)
        return host_glLinkProgram;
    if (gl_log_on()) {
        if (strcmp(symbol, "glBindFramebuffer") == 0)
            return host_glBindFramebuffer;
        if (strcmp(symbol, "glViewport") == 0)
            return host_glViewport;
    }
    if (strcmp(symbol, "glAlphaFunc") == 0)
        return host_glAlphaFunc;
    if (strcmp(symbol, "glBlendColor") == 0)
        return host_glBlendColor;
    if (strcmp(symbol, "glClearColor") == 0)
        return host_glClearColor;
    if (strcmp(symbol, "glClearDepthf") == 0)
        return host_glClearDepthf;
    if (strcmp(symbol, "glClearDepthfOES") == 0)
        return host_glClearDepthfOES;
    if (strcmp(symbol, "glColor4f") == 0)
        return host_glColor4f;
    if (strcmp(symbol, "glDepthRangef") == 0)
        return host_glDepthRangef;
    if (strcmp(symbol, "glDepthRangefOES") == 0)
        return host_glDepthRangefOES;
    if (strcmp(symbol, "glDrawTexfOES") == 0)
        return host_glDrawTexfOES;
    if (strcmp(symbol, "glFogf") == 0)
        return host_glFogf;
    if (strcmp(symbol, "glFrustumf") == 0)
        return host_glFrustumf;
    if (strcmp(symbol, "glFrustumfOES") == 0)
        return host_glFrustumfOES;
    if (strcmp(symbol, "glLightf") == 0)
        return host_glLightf;
    if (strcmp(symbol, "glLightModelf") == 0)
        return host_glLightModelf;
    if (strcmp(symbol, "glLineWidth") == 0)
        return host_glLineWidth;
    if (strcmp(symbol, "glMaterialf") == 0)
        return host_glMaterialf;
    if (strcmp(symbol, "glMultiTexCoord4f") == 0)
        return host_glMultiTexCoord4f;
    if (strcmp(symbol, "glNormal3f") == 0)
        return host_glNormal3f;
    if (strcmp(symbol, "glOrthof") == 0)
        return host_glOrthof;
    if (strcmp(symbol, "glOrthofOES") == 0)
        return host_glOrthofOES;
    if (strcmp(symbol, "glPointParameterf") == 0)
        return host_glPointParameterf;
    if (strcmp(symbol, "glPointSize") == 0)
        return host_glPointSize;
    if (strcmp(symbol, "glPolygonOffset") == 0)
        return host_glPolygonOffset;
    if (strcmp(symbol, "glRotatef") == 0)
        return host_glRotatef;
    if (strcmp(symbol, "glSampleCoverage") == 0)
        return host_glSampleCoverage;
    if (strcmp(symbol, "glScalef") == 0)
        return host_glScalef;
    if (strcmp(symbol, "glTexEnvf") == 0)
        return host_glTexEnvf;
    if (strcmp(symbol, "glTexGenfOES") == 0)
        return host_glTexGenfOES;
    if (strcmp(symbol, "glTexParameterf") == 0)
        return host_glTexParameterf;
    if (strcmp(symbol, "glTranslatef") == 0)
        return host_glTranslatef;
    if (strcmp(symbol, "glUniform1f") == 0)
        return host_glUniform1f;
    if (strcmp(symbol, "glUniform2f") == 0)
        return host_glUniform2f;
    if (strcmp(symbol, "glUniform3f") == 0)
        return host_glUniform3f;
    if (strcmp(symbol, "glUniform4f") == 0)
        return host_glUniform4f;
    if (strcmp(symbol, "glVertexAttrib4f") == 0)
        return host_glVertexAttrib4f;
    if (strcmp(symbol, "glGetError") == 0)
        return host_glGetError;
    if (strcmp(symbol, "eglGetProcAddress") == 0)
        return host_eglGetProcAddress;
    if (strcmp(symbol, "eglSwapBuffers") == 0)
        return host_eglSwapBuffers;
    return NULL;
}

void *s3e_host_resolve(const char *symbol) {
    void *wrapped = resolve_wrapped_host_proc(symbol);
    if (wrapped) {
        return wrapped;
    }
    if (strncmp(symbol, "egl", 3) == 0) {
        void *addr = lookup_egl(symbol);
        return addr ? addr : make_stub(symbol);
    }
    if (strncmp(symbol, "gl", 2) == 0) {
        void *addr = lookup_gl(symbol);
        return addr ? addr : make_stub(symbol);
    }
    for (size_t i = 0; i < sizeof(HOST_SYMBOLS) / sizeof(HOST_SYMBOLS[0]); ++i) {
        if (strcmp(symbol, HOST_SYMBOLS[i].name) == 0) {
            return HOST_SYMBOLS[i].fn;
        }
    }
    if (strncmp(symbol, "s3e", 3) == 0) {
        return make_stub(symbol);
    }
    return NULL;
}
