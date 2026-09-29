/* nxmix — mixer interno do port (The Sims 3 / NextOS).
 *
 * POR QUE EXISTE: o `/usr/lib32` do device NAO tem `libSDL2_mixer` (medido:
 * 291 bibliotecas, nenhuma `libSDL2_mixer*`). O host s3e herdado fala SO com
 * SDL2_mixer, entao `audio_open()` falhava, `s3eSoundGetFreeChannel` devolvia
 * -1 e `s3eSoundChannelPlay` devolvia erro — o jogo ficava MUDO.
 *
 * Em vez de embarcar SDL2_mixer (dependencia nova e proibida no ZIP), este
 * arquivo implementa a MESMA superficie `Mix_*` que o host ja usa, por cima do
 * dispositivo de audio do SDL2 que o device TEM. Toda a semantica s3e que a
 * casa ja pagou (canais, END_SAMPLE no pump do frame, repeat=0 = loop infinito
 * na musica e uma vez no SFX) fica intacta: so o fundo do poco muda.
 *
 * O jogo entrega SFX como PCM 16-bit mono cru (o host embrulha num WAV minimo);
 * a musica sao os 13 MP3 do OBB, decodificados aqui por minimp3.
 */
#include "s3e_host_internal.h"
#include "nxmix.h"
#include <math.h>

#define MINIMP3_ONLY_MP3
#define MINIMP3_IMPLEMENTATION
#include "minimp3.h"

#define NXMIX_MAX_CHANNELS 32
#define NXMIX_MAX_VOLUME 128
#define NXMIX_DEVICE_CHANNELS 2

struct nxmix_chunk {
    int16_t *pcm;      /* mono 16-bit */
    uint32_t frames;
    int rate;
};

struct nxmix_rw {
    uint8_t *data;
    size_t size;
    int owns;
};

struct nxmix_voice {
    struct nxmix_chunk *chunk;
    double pos;        /* posicao em frames da FONTE (fracionaria: reamostragem) */
    double step;
    int loops_left;    /* -1 = infinito */
    int volume;
    int active;
    int paused;
};

struct nxmix_music {
    int16_t *pcm;      /* estereo intercalado, ja na taxa do dispositivo */
    uint32_t frames;
    uint32_t pos;
    int loops_left;
    int active;
    int paused;
};

static struct {
    int (*InitSubSystem)(uint32_t flags);
    uint32_t (*OpenAudioDevice)(const char *device, int iscapture, const void *desired,
                                void *obtained, int allowed_changes);
    void (*CloseAudioDevice)(uint32_t dev);
    void (*PauseAudioDevice)(uint32_t dev, int pause_on);
    void (*LockAudioDevice)(uint32_t dev);
    void (*UnlockAudioDevice)(uint32_t dev);
    const char *(*GetError)(void);
} g_sdl;

/* SDL_AudioSpec: layout estavel do SDL2. */
struct sdl_audio_spec {
    int freq;
    uint16_t format;
    uint8_t channels;
    uint8_t silence;
    uint16_t samples;
    uint16_t padding;
    uint32_t size;
    void (*callback)(void *userdata, uint8_t *stream, int len);
    void *userdata;
};

static uint32_t g_device;
static int g_device_rate = 22050;
static int g_ready;
static int g_virtual;               /* sem dispositivo: relogio virtual */
static uint64_t g_virtual_base_ms;
static struct nxmix_voice g_voices[NXMIX_MAX_CHANNELS];
static int g_num_channels;
static struct nxmix_music g_music_state;
static int g_music_volume = NXMIX_MAX_VOLUME;
static char g_error[128] = "";
static int32_t g_peak;              /* pico de PCM medido na saida (prova de audio) */
static uint64_t g_frames_mixed;

static void nxmix_lock(void) {
    if (!g_virtual && g_sdl.LockAudioDevice) {
        g_sdl.LockAudioDevice(g_device);
    }
}

static void nxmix_unlock(void) {
    if (!g_virtual && g_sdl.UnlockAudioDevice) {
        g_sdl.UnlockAudioDevice(g_device);
    }
}

int32_t nxmix_peak(void) {
    return g_peak;
}

uint64_t nxmix_frames(void) {
    return g_frames_mixed;
}

int nxmix_is_virtual(void) {
    return g_virtual;
}

static void nxmix_callback(void *userdata, uint8_t *stream, int len) {
    (void)userdata;
    int16_t *out = (int16_t *)(void *)stream;
    int frames = len / (int)(sizeof(int16_t) * NXMIX_DEVICE_CHANNELS);
    memset(stream, 0, (size_t)len);

    for (int f = 0; f < frames; ++f) {
        int32_t left = 0, right = 0;

        for (int c = 0; c < g_num_channels; ++c) {
            struct nxmix_voice *v = &g_voices[c];
            if (!v->active || v->paused || !v->chunk) {
                continue;
            }
            uint32_t idx = (uint32_t)v->pos;
            if (idx >= v->chunk->frames) {
                if (v->loops_left < 0) {
                    v->pos = 0;
                    idx = 0;
                } else if (v->loops_left > 0) {
                    v->loops_left--;
                    v->pos = 0;
                    idx = 0;
                } else {
                    v->active = 0;
                    continue;
                }
            }
            int32_t s = v->chunk->pcm[idx];
            s = s * v->volume / NXMIX_MAX_VOLUME;
            left += s;
            right += s;
            v->pos += v->step;
        }

        struct nxmix_music *m = &g_music_state;
        if (m->active && !m->paused && m->pcm) {
            if (m->pos >= m->frames) {
                if (m->loops_left < 0) {
                    m->pos = 0;
                } else if (m->loops_left > 0) {
                    m->loops_left--;
                    m->pos = 0;
                } else {
                    m->active = 0;
                }
            }
            if (m->active) {
                left += m->pcm[m->pos * 2] * g_music_volume / NXMIX_MAX_VOLUME;
                right += m->pcm[m->pos * 2 + 1] * g_music_volume / NXMIX_MAX_VOLUME;
                m->pos++;
            }
        }

        if (left > 32767) left = 32767;
        if (left < -32768) left = -32768;
        if (right > 32767) right = 32767;
        if (right < -32768) right = -32768;
        out[f * 2] = (int16_t)left;
        out[f * 2 + 1] = (int16_t)right;

        int32_t mag = left < 0 ? -left : left;
        if (mag > g_peak) {
            g_peak = mag;
        }
    }
    g_frames_mixed += (uint64_t)frames;
}

/* ---------------- superficie Mix_* ---------------- */

static int nxmix_Init(int flags) {
    return flags;
}

static void nxmix_Quit(void) {
}

static int nxmix_OpenAudio(int frequency, uint16_t format, int channels, int chunksize) {
    (void)format;
    (void)channels;
    g_device_rate = frequency > 0 ? frequency : 22050;
    g_virtual_base_ms = monotonic_ms();

    if (!g_sdl.OpenAudioDevice || !g_sdl.InitSubSystem) {
        g_virtual = 1;
        g_ready = 1;
        fprintf(stderr, "[nxmix] sem SDL_OpenAudioDevice — audio em modo VIRTUAL (silencioso)\n");
        return 0;
    }
    if (g_sdl.InitSubSystem(0x00000010u /* SDL_INIT_AUDIO */) != 0) {
        g_virtual = 1;
        g_ready = 1;
        fprintf(stderr, "[nxmix] SDL_InitSubSystem(AUDIO) falhou (%s) — modo VIRTUAL\n",
                g_sdl.GetError ? g_sdl.GetError() : "?");
        return 0;
    }

    struct sdl_audio_spec want, have;
    memset(&want, 0, sizeof(want));
    memset(&have, 0, sizeof(have));
    want.freq = g_device_rate;
    want.format = 0x8010; /* AUDIO_S16LSB */
    want.channels = NXMIX_DEVICE_CHANNELS;
    want.samples = (uint16_t)(chunksize > 0 ? chunksize : 1024);
    want.callback = nxmix_callback;

    /* JAMAIS cravar driver de audio (regra #6): quem escolhe e' o SDL. */
    g_device = g_sdl.OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (!g_device) {
        g_virtual = 1;
        g_ready = 1;
        fprintf(stderr, "[nxmix] SDL_OpenAudioDevice falhou (%s) — modo VIRTUAL\n",
                g_sdl.GetError ? g_sdl.GetError() : "?");
        return 0;
    }
    g_device_rate = have.freq > 0 ? have.freq : g_device_rate;
    g_sdl.PauseAudioDevice(g_device, 0);
    g_ready = 1;
    fprintf(stderr, "[nxmix] dispositivo aberto: %d Hz, %d canais, buffer %u\n", g_device_rate,
            have.channels, have.samples);
    return 0;
}

static void nxmix_CloseAudio(void) {
    if (!g_virtual && g_device && g_sdl.CloseAudioDevice) {
        g_sdl.PauseAudioDevice(g_device, 1);
        g_sdl.CloseAudioDevice(g_device);
    }
    g_device = 0;
    g_ready = 0;
}

static int nxmix_AllocateChannels(int num_channels) {
    if (num_channels < 0) {
        return g_num_channels;
    }
    if (num_channels > NXMIX_MAX_CHANNELS) {
        num_channels = NXMIX_MAX_CHANNELS;
    }
    nxmix_lock();
    g_num_channels = num_channels;
    nxmix_unlock();
    return g_num_channels;
}

static void nxmix_ChannelFinished(void (*callback)(int channel)) {
    /* Deliberadamente NAO registrado: o host ja recolhe o fim de voz pelo
       Playing() dentro do pump do frame. Chamar callback do jogo da thread de
       audio seria reentrante — a casa ja pagou esse bug no codboz. */
    (void)callback;
}

static int nxmix_PlayChannelTimed(int channel, void *chunk, int loops, int ticks) {
    (void)ticks;
    struct nxmix_chunk *c = chunk;
    if (!c || channel < 0 || channel >= g_num_channels) {
        return -1;
    }
    nxmix_lock();
    struct nxmix_voice *v = &g_voices[channel];
    v->chunk = c;
    v->pos = 0.0;
    v->step = (double)c->rate / (double)g_device_rate;
    v->loops_left = loops;
    v->active = 1;
    v->paused = 0;
    if (g_virtual) {
        /* Sem dispositivo o tempo de vida da voz e' contado pelo relogio. */
        v->step = 0.0;
        v->pos = (double)(monotonic_ms() +
                          (uint64_t)c->frames * 1000u / (uint32_t)(c->rate > 0 ? c->rate : 22050));
    }
    nxmix_unlock();
    return channel;
}

static int nxmix_Playing(int channel) {
    if (channel < 0 || channel >= g_num_channels) {
        int n = 0;
        for (int i = 0; i < g_num_channels; ++i) {
            n += g_voices[i].active ? 1 : 0;
        }
        return n;
    }
    struct nxmix_voice *v = &g_voices[channel];
    if (!v->active) {
        return 0;
    }
    if (g_virtual && v->pos > 0.0 && (double)monotonic_ms() >= v->pos) {
        v->active = 0;
        return 0;
    }
    return 1;
}

static int nxmix_HaltChannel(int channel) {
    nxmix_lock();
    if (channel < 0) {
        for (int i = 0; i < NXMIX_MAX_CHANNELS; ++i) {
            g_voices[i].active = 0;
            g_voices[i].chunk = NULL;
        }
    } else if (channel < NXMIX_MAX_CHANNELS) {
        g_voices[channel].active = 0;
        g_voices[channel].chunk = NULL;
    }
    nxmix_unlock();
    return 0;
}

static void nxmix_Pause(int channel) {
    if (channel >= 0 && channel < NXMIX_MAX_CHANNELS) {
        g_voices[channel].paused = 1;
    }
}

static void nxmix_Resume(int channel) {
    if (channel >= 0 && channel < NXMIX_MAX_CHANNELS) {
        g_voices[channel].paused = 0;
    }
}

static int nxmix_Volume(int channel, int volume) {
    if (channel < 0 || channel >= NXMIX_MAX_CHANNELS) {
        return NXMIX_MAX_VOLUME;
    }
    int old = g_voices[channel].volume;
    if (volume >= 0) {
        g_voices[channel].volume = volume > NXMIX_MAX_VOLUME ? NXMIX_MAX_VOLUME : volume;
    }
    return old;
}

/* O host embrulha o PCM do jogo num WAV minimo e passa por SDL_RWFromConstMem.
   Como as duas pontas sao nossas, trocamos tambem o RWFromConstMem. */
void *nxmix_rw_from_const_mem(const void *mem, int size) {
    if (!mem || size <= 0) {
        return NULL;
    }
    struct nxmix_rw *rw = calloc(1, sizeof(*rw));
    if (!rw) {
        return NULL;
    }
    rw->data = malloc((size_t)size);
    if (!rw->data) {
        free(rw);
        return NULL;
    }
    memcpy(rw->data, mem, (size_t)size);
    rw->size = (size_t)size;
    rw->owns = 1;
    return rw;
}

static uint32_t rd32le(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void *nxmix_LoadWAV_RW(void *src, int freesrc) {
    struct nxmix_rw *rw = src;
    if (!rw || rw->size < 44 || memcmp(rw->data, "RIFF", 4) != 0) {
        snprintf(g_error, sizeof(g_error), "WAV invalido");
        return NULL;
    }
    int rate = (int)rd32le(rw->data + 24);
    uint32_t data_size = rd32le(rw->data + 40);
    if (data_size > rw->size - 44) {
        data_size = (uint32_t)(rw->size - 44);
    }
    struct nxmix_chunk *c = calloc(1, sizeof(*c));
    if (!c) {
        return NULL;
    }
    c->frames = data_size / sizeof(int16_t);
    c->rate = rate > 0 ? rate : 22050;
    c->pcm = malloc(data_size ? data_size : 2);
    if (!c->pcm) {
        free(c);
        return NULL;
    }
    memcpy(c->pcm, rw->data + 44, data_size);
    if (freesrc && rw->owns) {
        free(rw->data);
        free(rw);
    }
    return c;
}

static void nxmix_FreeChunk(void *chunk) {
    struct nxmix_chunk *c = chunk;
    if (!c) {
        return;
    }
    nxmix_lock();
    for (int i = 0; i < NXMIX_MAX_CHANNELS; ++i) {
        if (g_voices[i].chunk == c) {
            g_voices[i].active = 0;
            g_voices[i].chunk = NULL;
        }
    }
    nxmix_unlock();
    free(c->pcm);
    free(c);
}

/* --------- musica: MP3 do OBB decodificado com minimp3 --------- */

static struct nxmix_music *decode_mp3_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        snprintf(g_error, sizeof(g_error), "nao abriu %s", path);
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (fsize <= 0) {
        fclose(f);
        return NULL;
    }
    uint8_t *raw = malloc((size_t)fsize);
    if (!raw || fread(raw, 1, (size_t)fsize, f) != (size_t)fsize) {
        free(raw);
        fclose(f);
        return NULL;
    }
    fclose(f);

    mp3dec_t dec;
    mp3dec_init(&dec);
    size_t cap = 1u << 20;
    int16_t *pcm = malloc(cap * sizeof(int16_t));
    size_t used = 0;
    int src_rate = g_device_rate;
    int src_ch = 2;
    const uint8_t *p = raw;
    int left = (int)fsize;
    int16_t frame_pcm[MINIMP3_MAX_SAMPLES_PER_FRAME];

    while (left > 0 && pcm) {
        mp3dec_frame_info_t info;
        int samples = mp3dec_decode_frame(&dec, p, left, frame_pcm, &info);
        if (info.frame_bytes <= 0) {
            break;
        }
        p += info.frame_bytes;
        left -= info.frame_bytes;
        if (samples <= 0) {
            continue;
        }
        src_rate = info.hz;
        src_ch = info.channels;
        size_t need = used + (size_t)samples * (size_t)src_ch;
        if (need > cap) {
            while (need > cap) {
                cap *= 2;
            }
            int16_t *n = realloc(pcm, cap * sizeof(int16_t));
            if (!n) {
                free(pcm);
                pcm = NULL;
                break;
            }
            pcm = n;
        }
        memcpy(pcm + used, frame_pcm, (size_t)samples * (size_t)src_ch * sizeof(int16_t));
        used = need;
    }
    free(raw);
    if (!pcm || used == 0) {
        free(pcm);
        return NULL;
    }

    uint32_t src_frames = (uint32_t)(used / (size_t)src_ch);
    /* reamostra para a taxa do dispositivo e forca estereo intercalado */
    double ratio = (double)src_rate / (double)g_device_rate;
    uint32_t out_frames = (uint32_t)((double)src_frames / ratio);
    struct nxmix_music *m = calloc(1, sizeof(*m));
    if (!m) {
        free(pcm);
        return NULL;
    }
    m->pcm = malloc((size_t)out_frames * 2 * sizeof(int16_t));
    if (!m->pcm) {
        free(pcm);
        free(m);
        return NULL;
    }
    for (uint32_t i = 0; i < out_frames; ++i) {
        uint32_t si = (uint32_t)((double)i * ratio);
        if (si >= src_frames) {
            si = src_frames - 1;
        }
        int16_t l = pcm[si * (uint32_t)src_ch];
        int16_t r = (src_ch > 1) ? pcm[si * (uint32_t)src_ch + 1] : l;
        m->pcm[i * 2] = l;
        m->pcm[i * 2 + 1] = r;
    }
    m->frames = out_frames;
    free(pcm);
    return m;
}

static void *nxmix_LoadMUS(const char *file) {
    if (!file) {
        return NULL;
    }
    if (g_virtual) {
        struct nxmix_music *m = calloc(1, sizeof(*m));
        return m; /* sem dispositivo: objeto vazio, so para o jogo seguir */
    }
    return decode_mp3_file(file);
}

static void *nxmix_LoadMUS_RW(void *rw, int freesrc) {
    (void)rw;
    (void)freesrc;
    return NULL;
}

static int nxmix_PlayMusic(void *music, int loops) {
    struct nxmix_music *m = music;
    if (!m) {
        return -1;
    }
    nxmix_lock();
    g_music_state.pcm = m->pcm;
    g_music_state.frames = m->frames;
    g_music_state.pos = 0;
    g_music_state.loops_left = loops;
    g_music_state.active = m->pcm != NULL;
    g_music_state.paused = 0;
    nxmix_unlock();
    return 0;
}

static int nxmix_PlayingMusic(void) {
    return g_music_state.active ? 1 : 0;
}

static int nxmix_HaltMusic(void) {
    nxmix_lock();
    g_music_state.active = 0;
    g_music_state.pcm = NULL;
    nxmix_unlock();
    return 0;
}

static void nxmix_PauseMusic(void) {
    g_music_state.paused = 1;
}

static void nxmix_ResumeMusic(void) {
    g_music_state.paused = 0;
}

static int nxmix_VolumeMusic(int volume) {
    int old = g_music_volume;
    if (volume >= 0) {
        g_music_volume = volume > NXMIX_MAX_VOLUME ? NXMIX_MAX_VOLUME : volume;
    }
    return old;
}

static void nxmix_FreeMusic(void *music) {
    struct nxmix_music *m = music;
    if (!m) {
        return;
    }
    nxmix_lock();
    if (g_music_state.pcm == m->pcm) {
        g_music_state.active = 0;
        g_music_state.pcm = NULL;
    }
    nxmix_unlock();
    free(m->pcm);
    free(m);
}

static const char *nxmix_GetError(void) {
    return g_error;
}

/* Auto-teste do caminho de audio: abre o dispositivo de verdade, toca um tom e
   devolve o PICO DE PCM medido DENTRO do callback do dispositivo. E' a prova
   honesta de que a placa recebeu amostras — "sem erro no log" nao prova nada
   (mudo sem erro costuma ser o PulseAudio detendo a placa). */
int nxmix_selftest(void *sdl2, int seconds) {
    struct sdl_mixer_api api;
    memset(&api, 0, sizeof(api));
    if (!nxmix_bind(&api, sdl2)) {
        fprintf(stderr, "[nxmix] selftest: bind falhou\n");
        return 1;
    }
    if (api.OpenAudio(22050, 0x8010, 2, 1024) != 0) {
        fprintf(stderr, "[nxmix] selftest: OpenAudio falhou\n");
        return 1;
    }
    api.AllocateChannels(8);

    /* tom de 440 Hz, 1 s, PCM 16-bit mono — mesmo formato que o jogo entrega */
    uint32_t rate = 22050, frames = rate;
    uint32_t data_size = frames * (uint32_t)sizeof(int16_t);
    uint8_t *wav = malloc(44 + data_size);
    if (!wav) {
        return 1;
    }
    memcpy(wav, "RIFF", 4);
    uint32_t total = 44 + data_size - 8;
    wav[4] = (uint8_t)total; wav[5] = (uint8_t)(total >> 8);
    wav[6] = (uint8_t)(total >> 16); wav[7] = (uint8_t)(total >> 24);
    memcpy(wav + 8, "WAVEfmt ", 8);
    wav[16] = 16; wav[17] = wav[18] = wav[19] = 0;
    wav[20] = 1; wav[21] = 0; wav[22] = 1; wav[23] = 0;
    wav[24] = (uint8_t)rate; wav[25] = (uint8_t)(rate >> 8);
    wav[26] = (uint8_t)(rate >> 16); wav[27] = (uint8_t)(rate >> 24);
    uint32_t br = rate * 2;
    wav[28] = (uint8_t)br; wav[29] = (uint8_t)(br >> 8);
    wav[30] = (uint8_t)(br >> 16); wav[31] = (uint8_t)(br >> 24);
    wav[32] = 2; wav[33] = 0; wav[34] = 16; wav[35] = 0;
    memcpy(wav + 36, "data", 4);
    wav[40] = (uint8_t)data_size; wav[41] = (uint8_t)(data_size >> 8);
    wav[42] = (uint8_t)(data_size >> 16); wav[43] = (uint8_t)(data_size >> 24);
    int16_t *pcm = (int16_t *)(void *)(wav + 44);
    for (uint32_t i = 0; i < frames; ++i) {
        double t = (double)i / (double)rate;
        pcm[i] = (int16_t)(20000.0 * sin(2.0 * 3.14159265358979 * 440.0 * t));
    }

    void *rw = nxmix_rw_from_const_mem(wav, (int)(44 + data_size));
    void *chunk = api.LoadWAV_RW(rw, 1);
    free(wav);
    if (!chunk) {
        fprintf(stderr, "[nxmix] selftest: LoadWAV falhou\n");
        return 1;
    }
    api.Volume(0, NXMIX_MAX_VOLUME);
    api.PlayChannelTimed(0, chunk, 0, -1);
    for (int i = 0; i < seconds * 10; ++i) {
        sleep_ms(100);
    }
    int32_t peak = nxmix_peak();
    fprintf(stderr,
            "[nxmix] SELFTEST: pico=%d (%.1f%% da escala), %llu frames mixados, modo=%s\n", peak,
            (double)peak * 100.0 / 32768.0, (unsigned long long)nxmix_frames(),
            g_virtual ? "VIRTUAL" : "DISPOSITIVO REAL");
    api.CloseAudio();
    return (peak > 1000 && !g_virtual) ? 0 : 1;
}

int nxmix_bind(struct sdl_mixer_api *api, void *sdl2) {
    if (!api || !sdl2) {
        return 0;
    }
    g_sdl.InitSubSystem = dlsym(sdl2, "SDL_InitSubSystem");
    g_sdl.OpenAudioDevice = dlsym(sdl2, "SDL_OpenAudioDevice");
    g_sdl.CloseAudioDevice = dlsym(sdl2, "SDL_CloseAudioDevice");
    g_sdl.PauseAudioDevice = dlsym(sdl2, "SDL_PauseAudioDevice");
    g_sdl.LockAudioDevice = dlsym(sdl2, "SDL_LockAudioDevice");
    g_sdl.UnlockAudioDevice = dlsym(sdl2, "SDL_UnlockAudioDevice");
    g_sdl.GetError = dlsym(sdl2, "SDL_GetError");

    api->Init = nxmix_Init;
    api->Quit = nxmix_Quit;
    api->OpenAudio = nxmix_OpenAudio;
    api->CloseAudio = nxmix_CloseAudio;
    api->AllocateChannels = nxmix_AllocateChannels;
    api->ChannelFinished = nxmix_ChannelFinished;
    api->PlayChannelTimed = nxmix_PlayChannelTimed;
    api->Playing = nxmix_Playing;
    api->HaltChannel = nxmix_HaltChannel;
    api->Pause = nxmix_Pause;
    api->Resume = nxmix_Resume;
    api->Volume = nxmix_Volume;
    api->LoadWAV_RW = nxmix_LoadWAV_RW;
    api->FreeChunk = nxmix_FreeChunk;
    api->LoadMUS = nxmix_LoadMUS;
    api->LoadMUS_RW = nxmix_LoadMUS_RW;
    api->PlayMusic = nxmix_PlayMusic;
    api->PlayingMusic = nxmix_PlayingMusic;
    api->HaltMusic = nxmix_HaltMusic;
    api->PauseMusic = nxmix_PauseMusic;
    api->ResumeMusic = nxmix_ResumeMusic;
    api->VolumeMusic = nxmix_VolumeMusic;
    api->FreeMusic = nxmix_FreeMusic;
    api->GetError = nxmix_GetError;
    for (int i = 0; i < NXMIX_MAX_CHANNELS; ++i) {
        g_voices[i].volume = NXMIX_MAX_VOLUME;
    }
    return 1;
}
