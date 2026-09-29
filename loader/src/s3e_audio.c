#include "s3e_host_internal.h"

enum {
    SDL_INIT_AUDIO = 0x00000010u,
    AUDIO_S16LSB = 0x8010u,
    MIX_INIT_MP3 = 0x00000008u,
    MIX_MAX_VOLUME = 128,
    S3E_MAX_VOLUME = 256,
    SOUND_CHANNELS = 24,
    AUDIO_STATUS_STOPPED = 0,
    AUDIO_STATUS_PLAYING = 1,
    AUDIO_STATUS_PAUSED = 2,
    IMA_ADPCM_BLOCK_BYTES = 512,
    SOUND_FREQUENCY = 22050,
    SOUND_RATE_DEFAULT = 22050,
    SOUND_RATE_SCALE_DEFAULT = 0x10000,
    SOUND_RATE_MAX = 0x40000,
    SOUND_OUTPUT_CHANNELS = 2,
    SOUND_BUFFER_SAMPLES = 1024,
    WAV_HEADER_BYTES = 44,
};

struct sdl_audio_api {
    int (*InitSubSystem)(uint32_t flags);
    void (*QuitSubSystem)(uint32_t flags);
    void *(*RWFromConstMem)(const void *mem, int size);
    const char *(*GetError)(void);
};

#include "nxmix.h"


struct sound_slot {
    void *chunk;
    int finished;
    int volume_s3e;
    int volume_mix;
    int rate;
    int rate_scale;
    int position;
};

/* NextOS: callbacks por canal (END_SAMPLE=0, STOP_AUDIO=1). A engine do jogo
   recicla as vozes dela quando recebe END_SAMPLE; sem despachar isto, ela acha
   que os canais nunca terminam e para de emitir sons novos (tiros mudos). */
enum {
    S3E_CHANNEL_END_SAMPLE = 0,
    S3E_CHANNEL_STOP_AUDIO = 1,
    S3E_CHANNEL_GEN_AUDIO = 2,
    S3E_CHANNEL_GEN_AUDIO_STEREO = 3,
    SOUND_CALLBACK_TYPES = 2,
};

struct s3e_sound_end_sample_info {
    int32_t channel;
    int32_t reps_remaining;
};

struct sound_channel_callback {
    void *fn;
    void *user_data;
};

static struct sound_channel_callback g_channel_callbacks[SOUND_CHANNELS][SOUND_CALLBACK_TYPES];
static int g_audio_pumping;

static void *g_sdl2_audio;
static void *g_sdl_mixer;
static struct sdl_audio_api g_sdl_audio;
static struct sdl_mixer_api g_mixer;
static struct sound_slot g_sound_slots[SOUND_CHANNELS];
static void *g_music;
static uint8_t *g_music_buffer;
static int g_audio_tried;
static int g_audio_ready;
static int g_audio_paused;
static int g_audio_volume_s3e = S3E_MAX_VOLUME;
static int g_audio_volume_mix = MIX_MAX_VOLUME;
static int g_sound_volume_s3e = S3E_MAX_VOLUME;
static int g_sound_volume_mix = MIX_MAX_VOLUME;
static int g_sound_rate = SOUND_RATE_DEFAULT;
static int g_mixer_mp3_ready;
static int g_sound_slots_initialized;

static const int8_t IMA_INDEX_TABLE[16] = {
    -1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8,
};

static const int16_t IMA_STEP_TABLE[89] = {
    7,     8,     9,     10,    11,    12,    13,    14,    16,    17,    19,    21,    23,
    25,    28,    31,    34,    37,    41,    45,    50,    55,    60,    66,    73,    80,
    88,    97,    107,   118,   130,   143,   157,   173,   190,   209,   230,   253,   279,
    307,   337,   371,   408,   449,   494,   544,   598,   658,   724,   796,   876,   963,
    1060,  1166,  1282,  1411,  1552,  1707,  1878,  2066,  2272,  2499,  2749,  3024,  3327,
    3660,  4026,  4428,  4871,  5358,  5894,  6484,  7132,  7845,  8630,  9493,  10442, 11487,
    12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767,
};

static int load_symbol(void *handle, void **slot, const char *name) {
    *slot = dlsym(handle, name);
    return *slot != NULL;
}

static void load_optional_symbol(void *handle, void **slot, const char *name) {
    *slot = dlsym(handle, name);
}

static const char *mixer_error(void) {
    if (g_mixer.GetError) {
        const char *error = g_mixer.GetError();
        if (error && error[0]) {
            return error;
        }
    }
    if (g_sdl_audio.GetError) {
        const char *error = g_sdl_audio.GetError();
        if (error && error[0]) {
            return error;
        }
    }
    return "unknown error";
}

static int clamp_volume_256(int value) {
    if (value < 0) {
        return 0;
    }
    if (value > S3E_MAX_VOLUME) {
        return S3E_MAX_VOLUME;
    }
    return value;
}

static int mix_volume_from_s3e(int value) {
    return clamp_volume_256(value) * MIX_MAX_VOLUME / S3E_MAX_VOLUME;
}

static int mixed_channel_volume(const struct sound_slot *slot) {
    return slot->volume_mix * g_sound_volume_mix / MIX_MAX_VOLUME;
}

static int channel_valid(int channel) {
    return channel >= 0 && channel < SOUND_CHANNELS;
}

static int16_t clamp_s16(int value) {
    if (value < INT16_MIN) {
        return INT16_MIN;
    }
    if (value > INT16_MAX) {
        return INT16_MAX;
    }
    return (int16_t)value;
}

static int clamp_rate(int value) {
    if (value < 0) {
        return 0;
    }
    if (value > SOUND_RATE_MAX) {
        return SOUND_RATE_MAX;
    }
    return value;
}

static void init_sound_slots(void) {
    if (g_sound_slots_initialized) {
        return;
    }
    for (int channel = 0; channel < SOUND_CHANNELS; ++channel) {
        g_sound_slots[channel].volume_s3e = S3E_MAX_VOLUME;
        g_sound_slots[channel].volume_mix = MIX_MAX_VOLUME;
        g_sound_slots[channel].rate = SOUND_RATE_DEFAULT;
        g_sound_slots[channel].rate_scale = SOUND_RATE_SCALE_DEFAULT;
    }
    g_sound_slots_initialized = 1;
}

static int rate_from_scale(int scale) {
    int scaled = scale >> 8;
    int64_t rate = (int64_t)g_sound_rate * scaled;
    if (rate < 0) {
        rate -= 0xff;
    } else {
        rate += 0xff;
    }
    return clamp_rate((int)(rate >> 8));
}

static int looks_like_ima_adpcm(const uint8_t *data, size_t byte_count) {
    if (!data || byte_count < 4) {
        return 0;
    }

    for (size_t offset = 0; offset < byte_count;) {
        size_t block_size = byte_count - offset;
        if (block_size > IMA_ADPCM_BLOCK_BYTES) {
            block_size = IMA_ADPCM_BLOCK_BYTES;
        }
        if (block_size < 4) {
            return 0;
        }

        const uint8_t *header = data + offset;
        if (header[2] > 88 || header[3] != 0) {
            return 0;
        }
        offset += block_size;
    }
    return 1;
}

static int decode_ima_nibble(int nibble, int *predictor, int *step_index) {
    int step = IMA_STEP_TABLE[*step_index];
    int diff = step >> 3;
    if (nibble & 1) {
        diff += step >> 2;
    }
    if (nibble & 2) {
        diff += step >> 1;
    }
    if (nibble & 4) {
        diff += step;
    }

    if (nibble & 8) {
        *predictor -= diff;
    } else {
        *predictor += diff;
    }
    *predictor = clamp_s16(*predictor);

    *step_index += IMA_INDEX_TABLE[nibble & 0x0f];
    if (*step_index < 0) {
        *step_index = 0;
    } else if (*step_index > 88) {
        *step_index = 88;
    }
    return *predictor;
}

static int16_t *decode_ima_adpcm_mono(const uint8_t *data, size_t byte_count,
                                      uint32_t *out_samples) {
    if (!looks_like_ima_adpcm(data, byte_count)) {
        return NULL;
    }

    size_t samples = 0;
    for (size_t offset = 0; offset < byte_count;) {
        size_t block_size = byte_count - offset;
        if (block_size > IMA_ADPCM_BLOCK_BYTES) {
            block_size = IMA_ADPCM_BLOCK_BYTES;
        }
        if (samples > UINT32_MAX - (1u + (block_size - 4u) * 2u)) {
            return NULL;
        }
        samples += 1u + (block_size - 4u) * 2u;
        offset += block_size;
    }

    int16_t *pcm = malloc(samples * sizeof(int16_t));
    if (!pcm) {
        return NULL;
    }

    int16_t *dst = pcm;
    for (size_t offset = 0; offset < byte_count;) {
        size_t block_size = byte_count - offset;
        if (block_size > IMA_ADPCM_BLOCK_BYTES) {
            block_size = IMA_ADPCM_BLOCK_BYTES;
        }

        const uint8_t *src = data + offset;
        int predictor = (int16_t)((uint16_t)src[0] | ((uint16_t)src[1] << 8));
        int step_index = src[2];
        *dst++ = (int16_t)predictor;

        for (size_t i = 4; i < block_size; ++i) {
            uint8_t byte = src[i];
            *dst++ = (int16_t)decode_ima_nibble(byte & 0x0f, &predictor, &step_index);
            *dst++ = (int16_t)decode_ima_nibble(byte >> 4, &predictor, &step_index);
        }
        offset += block_size;
    }

    *out_samples = (uint32_t)samples;
    return pcm;
}

static void write_le16(uint8_t *dst, uint16_t value) {
    dst[0] = (uint8_t)(value & 0xffu);
    dst[1] = (uint8_t)(value >> 8);
}

static void write_le32(uint8_t *dst, uint32_t value) {
    dst[0] = (uint8_t)(value & 0xffu);
    dst[1] = (uint8_t)((value >> 8) & 0xffu);
    dst[2] = (uint8_t)((value >> 16) & 0xffu);
    dst[3] = (uint8_t)(value >> 24);
}

static uint8_t *create_pcm16_mono_wav(const int16_t *data, uint32_t samples, int sample_rate,
                                      uint32_t *out_byte_count) {
    if (!data || samples == 0 || samples > (UINT32_MAX - WAV_HEADER_BYTES) / sizeof(int16_t)) {
        return NULL;
    }
    if (sample_rate <= 0 || sample_rate > SOUND_RATE_MAX) {
        sample_rate = SOUND_RATE_DEFAULT;
    }

    uint32_t data_size = samples * (uint32_t)sizeof(int16_t);
    uint32_t byte_count = WAV_HEADER_BYTES + data_size;
    uint8_t *buffer = malloc(byte_count);
    if (!buffer) {
        return NULL;
    }

    memcpy(buffer, "RIFF", 4);
    write_le32(buffer + 4, byte_count - 8u);
    memcpy(buffer + 8, "WAVEfmt ", 8);
    write_le32(buffer + 16, 16);
    write_le16(buffer + 20, 1);
    write_le16(buffer + 22, 1);
    write_le32(buffer + 24, (uint32_t)sample_rate);
    write_le32(buffer + 28, (uint32_t)sample_rate * sizeof(int16_t));
    write_le16(buffer + 32, sizeof(int16_t));
    write_le16(buffer + 34, 16);
    memcpy(buffer + 36, "data", 4);
    write_le32(buffer + 40, data_size);
    memcpy(buffer + WAV_HEADER_BYTES, data, data_size);

    *out_byte_count = byte_count;
    return buffer;
}

static void free_sound_slot(int channel) {
    if (!channel_valid(channel)) {
        return;
    }
    struct sound_slot *slot = &g_sound_slots[channel];
    if (slot->chunk && g_mixer.FreeChunk) {
        g_mixer.FreeChunk(slot->chunk);
    }
    slot->chunk = NULL;
    slot->finished = 0;
}

static void sound_channel_finished(int channel) {
    if (channel_valid(channel)) {
        g_sound_slots[channel].finished = 1;
    }
}

static void dispatch_channel_callback(int channel, int type) {
    struct sound_channel_callback *cb = &g_channel_callbacks[channel][type];
    if (!cb->fn) {
        return;
    }
    struct s3e_sound_end_sample_info info = {
        .channel = channel,
        .reps_remaining = 0,
    };
    ((s3e_callback_fn)(uintptr_t)cb->fn)(&info, cb->user_data);
}

static uint8_t g_channel_end_pending[SOUND_CHANNELS];

/* NextOS: libera canais terminados e MARCA o END_SAMPLE como pendente. NAO
   despacha aqui: isto roda dentro de s3eSoundChannelPlay/GetFreeChannel, e
   callback re-entrante no meio de um Play corrompe o estado de voz da engine
   (SIGSEGV 0x4a0d71f8). No s3e real, callbacks disparam no yield do frame. */
static void service_finished_channels(void) {
    if (!g_audio_ready) {
        return;
    }
    for (int channel = 0; channel < SOUND_CHANNELS; ++channel) {
        struct sound_slot *slot = &g_sound_slots[channel];
        if (!slot->chunk) {
            continue;
        }
        if (!slot->finished && g_mixer.Playing && g_mixer.Playing(channel) != 0) {
            continue;
        }
        free_sound_slot(channel);
        g_channel_end_pending[channel] = 1;
    }
}

/* Despacho dos END_SAMPLE pendentes — chamado SO' no pump do frame
   (eglSwapBuffers), nunca de dentro da API de som. */
void audio_pump(void) {
    if (!g_audio_ready || g_audio_pumping) {
        return;
    }
    g_audio_pumping = 1;
    service_finished_channels();
    for (int channel = 0; channel < SOUND_CHANNELS; ++channel) {
        if (!g_channel_end_pending[channel]) {
            continue;
        }
        g_channel_end_pending[channel] = 0;
        dispatch_channel_callback(channel, S3E_CHANNEL_END_SAMPLE);
    }
    g_audio_pumping = 0;
}

static int audio_open(void) {
    if (g_audio_tried) {
        return g_audio_ready;
    }
    g_audio_tried = 1;

    const char *sdl_names[] = {"libSDL2-2.0.so.0", "libSDL2.so", NULL};
    const char *mixer_names[] = {"libSDL2_mixer-2.0.so.0", "libSDL2_mixer.so", NULL};
    g_sdl2_audio = open_first(sdl_names);
    g_sdl_mixer = open_first(mixer_names);
    if (!g_sdl2_audio) {
        fprintf(stderr, "[audio] SDL2 indisponivel — sem audio\n");
        return 0;
    }
    if (!g_sdl_mixer) {
        /* The Sims 3 (NextOS): o device NAO tem libSDL2_mixer 32-bit (medido em
           /usr/lib32). Em vez de embarcar a biblioteca, usamos o mixer interno
           (nxmix) por cima do dispositivo de audio do SDL2, que o device tem.
           Sem isto o jogo fica MUDO e, pior, s3eSoundGetFreeChannel devolve -1
           e o jogo trata como falha. */
        fprintf(stderr, "[audio] libSDL2_mixer ausente — usando mixer interno (nxmix)\n");
        if (!load_symbol(g_sdl2_audio, (void **)&g_sdl_audio.InitSubSystem,
                         "SDL_InitSubSystem") ||
            !nxmix_bind(&g_mixer, g_sdl2_audio)) {
            fprintf(stderr, "[audio] nxmix nao pode ser ligado\n");
            return 0;
        }
        load_optional_symbol(g_sdl2_audio, (void **)&g_sdl_audio.QuitSubSystem,
                             "SDL_QuitSubSystem");
        load_optional_symbol(g_sdl2_audio, (void **)&g_sdl_audio.GetError, "SDL_GetError");
        g_sdl_audio.RWFromConstMem = nxmix_rw_from_const_mem;
        if (g_mixer.OpenAudio(SOUND_FREQUENCY, AUDIO_S16LSB, SOUND_OUTPUT_CHANNELS,
                              SOUND_BUFFER_SAMPLES) != 0) {
            fprintf(stderr, "[audio] nxmix OpenAudio falhou\n");
            return 0;
        }
        g_mixer.AllocateChannels(SOUND_CHANNELS);
        g_audio_ready = 1;
        return 1;
    }

    int ok = 1;
    ok &= load_symbol(g_sdl2_audio, (void **)&g_sdl_audio.InitSubSystem, "SDL_InitSubSystem");
    ok &= load_symbol(g_sdl2_audio, (void **)&g_sdl_audio.QuitSubSystem, "SDL_QuitSubSystem");
    ok &= load_symbol(g_sdl2_audio, (void **)&g_sdl_audio.RWFromConstMem, "SDL_RWFromConstMem");
    load_optional_symbol(g_sdl2_audio, (void **)&g_sdl_audio.GetError, "SDL_GetError");

    load_optional_symbol(g_sdl_mixer, (void **)&g_mixer.Init, "Mix_Init");
    load_optional_symbol(g_sdl_mixer, (void **)&g_mixer.Quit, "Mix_Quit");
    ok &= load_symbol(g_sdl_mixer, (void **)&g_mixer.OpenAudio, "Mix_OpenAudio");
    ok &= load_symbol(g_sdl_mixer, (void **)&g_mixer.CloseAudio, "Mix_CloseAudio");
    ok &= load_symbol(g_sdl_mixer, (void **)&g_mixer.AllocateChannels, "Mix_AllocateChannels");
    ok &= load_symbol(g_sdl_mixer, (void **)&g_mixer.ChannelFinished, "Mix_ChannelFinished");
    ok &= load_symbol(g_sdl_mixer, (void **)&g_mixer.PlayChannelTimed, "Mix_PlayChannelTimed");
    ok &= load_symbol(g_sdl_mixer, (void **)&g_mixer.Playing, "Mix_Playing");
    ok &= load_symbol(g_sdl_mixer, (void **)&g_mixer.HaltChannel, "Mix_HaltChannel");
    ok &= load_symbol(g_sdl_mixer, (void **)&g_mixer.Pause, "Mix_Pause");
    ok &= load_symbol(g_sdl_mixer, (void **)&g_mixer.Resume, "Mix_Resume");
    ok &= load_symbol(g_sdl_mixer, (void **)&g_mixer.Volume, "Mix_Volume");
    ok &= load_symbol(g_sdl_mixer, (void **)&g_mixer.LoadWAV_RW, "Mix_LoadWAV_RW");
    ok &= load_symbol(g_sdl_mixer, (void **)&g_mixer.FreeChunk, "Mix_FreeChunk");
    ok &= load_symbol(g_sdl_mixer, (void **)&g_mixer.LoadMUS, "Mix_LoadMUS");
    load_optional_symbol(g_sdl_mixer, (void **)&g_mixer.LoadMUS_RW, "Mix_LoadMUS_RW");
    ok &= load_symbol(g_sdl_mixer, (void **)&g_mixer.PlayMusic, "Mix_PlayMusic");
    ok &= load_symbol(g_sdl_mixer, (void **)&g_mixer.PlayingMusic, "Mix_PlayingMusic");
    ok &= load_symbol(g_sdl_mixer, (void **)&g_mixer.HaltMusic, "Mix_HaltMusic");
    ok &= load_symbol(g_sdl_mixer, (void **)&g_mixer.PauseMusic, "Mix_PauseMusic");
    ok &= load_symbol(g_sdl_mixer, (void **)&g_mixer.ResumeMusic, "Mix_ResumeMusic");
    ok &= load_symbol(g_sdl_mixer, (void **)&g_mixer.VolumeMusic, "Mix_VolumeMusic");
    ok &= load_symbol(g_sdl_mixer, (void **)&g_mixer.FreeMusic, "Mix_FreeMusic");
    load_optional_symbol(g_sdl_mixer, (void **)&g_mixer.GetError, "Mix_GetError");

    if (!ok) {
        fprintf(stderr, "[audio] SDL2_mixer symbols unavailable\n");
        return 0;
    }
    if (g_sdl_audio.InitSubSystem(SDL_INIT_AUDIO) != 0) {
        fprintf(stderr, "[audio] SDL audio init failed: %s\n", mixer_error());
        return 0;
    }
    if (g_mixer.Init) {
        g_mixer_mp3_ready = (g_mixer.Init(MIX_INIT_MP3) & MIX_INIT_MP3) != 0;
    }
    if (g_mixer.OpenAudio(SOUND_FREQUENCY, AUDIO_S16LSB, SOUND_OUTPUT_CHANNELS,
                          SOUND_BUFFER_SAMPLES) != 0) {
        fprintf(stderr, "[audio] SDL mixer open failed: %s\n", mixer_error());
        return 0;
    }

    g_mixer.AllocateChannels(SOUND_CHANNELS);
    g_mixer.ChannelFinished(sound_channel_finished);
    g_mixer.VolumeMusic(g_audio_volume_mix);
    init_sound_slots();
    for (int channel = 0; channel < SOUND_CHANNELS; ++channel) {
        g_mixer.Volume(channel, mixed_channel_volume(&g_sound_slots[channel]));
    }
    if (!g_mixer_mp3_ready) {
        fprintf(stderr,
                "[audio] MP3 decoder not reported by SDL2_mixer; music may be unavailable\n");
    }
    g_audio_ready = 1;
    return 1;
}

static void free_music(void) {
    if (g_music && g_mixer.FreeMusic) {
        g_mixer.FreeMusic(g_music);
    }
    g_music = NULL;
    free(g_music_buffer);
    g_music_buffer = NULL;
    g_audio_paused = 0;
}

static void stop_music(void) {
    if (g_audio_ready && g_mixer.HaltMusic) {
        g_mixer.HaltMusic();
    }
    free_music();
}

static int resolve_audio_path(const char *name, char *out, size_t out_size) {
    if (!name || !name[0]) {
        return 0;
    }
    if (name[0] == '/') {
        snprintf(out, out_size, "%s", name);
        return access(out, R_OK) == 0;
    }

    const char *patterns[] = {
        "%s/%s",
        "%s/assets/%s",
        /* NFS Shift (NextOS): trilhas licenciadas vivem em bgm/ na raiz do OBB
           (o jogo formata "%sbgm/%s"; se pedir so o nome do .mp3, resolve aqui). */
        "%s/bgm/%s",
        "%s/assets/bgm/%s",
    };
    for (size_t i = 0; i < sizeof(patterns) / sizeof(patterns[0]); ++i) {
        snprintf(out, out_size, patterns[i], g_root, name);
        if (access(out, R_OK) == 0) {
            return 1;
        }
    }
    return 0;
}

void audio_shutdown(void) {
    stop_music();
    for (int channel = 0; channel < SOUND_CHANNELS; ++channel) {
        if (g_audio_ready && g_mixer.HaltChannel) {
            g_mixer.HaltChannel(channel);
        }
        free_sound_slot(channel);
    }
    if (g_audio_ready && g_mixer.CloseAudio) {
        g_mixer.CloseAudio();
    }
    if (g_mixer.Quit) {
        g_mixer.Quit();
    }
    if (g_sdl_audio.QuitSubSystem) {
        g_sdl_audio.QuitSubSystem(SDL_INIT_AUDIO);
    }
    if (g_sdl_mixer) {
        dlclose(g_sdl_mixer);
    }
    if (g_sdl2_audio) {
        dlclose(g_sdl2_audio);
    }
    memset(&g_mixer, 0, sizeof(g_mixer));
    memset(&g_sdl_audio, 0, sizeof(g_sdl_audio));
    g_sdl_mixer = NULL;
    g_sdl2_audio = NULL;
    g_audio_ready = 0;
    g_audio_tried = 0;
    g_mixer_mp3_ready = 0;
    g_audio_paused = 0;
    g_sound_slots_initialized = 0;
}

int32_t s3eAudioIsPlaying(void) {
    return s3eAudioGetInt(1) == AUDIO_STATUS_PLAYING ? 1 : 0;
}

int32_t s3eAudioSetInt(uint32_t key, int32_t value) {
    if (key == 0) {
        g_audio_volume_s3e = clamp_volume_256(value);
        g_audio_volume_mix = mix_volume_from_s3e(g_audio_volume_s3e);
        if (audio_open()) {
            g_mixer.VolumeMusic(g_audio_volume_mix);
        }
        return 0;
    }
    return 0;
}

int32_t s3eAudioGetInt(uint32_t key) {
    switch (key) {
    case 0:
        return g_audio_volume_s3e;
    case 1:
        if (!audio_open() || !g_music || !g_mixer.PlayingMusic()) {
            return AUDIO_STATUS_STOPPED;
        }
        return g_audio_paused ? AUDIO_STATUS_PAUSED : AUDIO_STATUS_PLAYING;
    case 4:
    case 5:
        return 0;
    case 6:
    case 9:
        return audio_open() ? 1 : 0;
    default:
        return -1;
    }
}

/* PROVA DE AUDIO (regra da casa): nao basta "sem erro no log" — o que vale e' o
   PICO DE PCM medido na saida. O nxmix mede o pico no proprio callback do
   dispositivo, entao este numero e' o que foi realmente entregue a placa. */
void audio_peak_report(void) {
    static int32_t last_peak = -1;
    if (!g_audio_ready) {
        return;
    }
    int32_t peak = nxmix_peak();
    if (peak != last_peak) {
        last_peak = peak;
        fprintf(stderr, "[audio] PICO DE PCM = %d (%.1f%% da escala) apos %llu frames mixados%s\n",
                peak, (double)peak * 100.0 / 32768.0,
                (unsigned long long)nxmix_frames(),
                nxmix_is_virtual() ? " [VIRTUAL: sem dispositivo]" : "");
        fflush(stderr);
    }
}

int32_t s3eAudioPlay(const char *filename, uint32_t repeat) {
    /* NextOS: auditoria opt-in — CODBOZ_AUDIO_LOG=1 loga cada pedido de musica. */
    if (getenv("SIMS3_AUDIO_LOG")) {
        fprintf(stderr, "[audio] s3eAudioPlay(\"%s\", repeat=%u)\n",
                filename ? filename : "(null)", repeat);
    }
    if (!audio_open()) {
        return 1;
    }

    char path[1200];
    if (!resolve_audio_path(filename, path, sizeof(path))) {
        fprintf(stderr, "[audio] music file not found: %s\n", filename ? filename : "(null)");
        return 1;
    }

    stop_music();
    g_music = g_mixer.LoadMUS(path);
    if (!g_music) {
        fprintf(stderr, "[audio] music load failed: %s: %s\n", path, mixer_error());
        return 1;
    }

    g_audio_paused = 0;
    g_mixer.VolumeMusic(g_audio_volume_mix);
    /* NextOS FIX: na API s3eAudio, repeat=0 = tocar PARA SEMPRE (loop). O codigo
       original tocava 1 vez e parava -> "musica de fundo sumindo" (ex.: o
       underscore do menu e' pedido com repeat=0 e deve loopar). */
    if (g_mixer.PlayMusic(g_music, (repeat == 0) ? -1 : (int)repeat - 1) != 0) {
        fprintf(stderr, "[audio] music play failed: %s\n", mixer_error());
        stop_music();
        return 1;
    }
    return 0;
}

int32_t s3eAudioPlayFromBuffer(const void *buffer, uint32_t size, uint32_t repeat) {
    if (!audio_open() || !buffer || size == 0 || size > INT32_MAX || !g_mixer.LoadMUS_RW) {
        return 1;
    }

    uint8_t *copy = malloc(size);
    if (!copy) {
        return 1;
    }
    memcpy(copy, buffer, size);
    void *rw = g_sdl_audio.RWFromConstMem(copy, (int)size);
    if (!rw) {
        free(copy);
        return 1;
    }

    stop_music();
    g_music_buffer = copy;
    g_music = g_mixer.LoadMUS_RW(rw, 1);
    if (!g_music) {
        fprintf(stderr, "[audio] buffered music load failed: %s\n", mixer_error());
        stop_music();
        return 1;
    }
    g_audio_paused = 0;
    g_mixer.VolumeMusic(g_audio_volume_mix);
    /* NextOS FIX: idem s3eAudioPlay — repeat=0 = loop infinito na API s3e. */
    if (g_mixer.PlayMusic(g_music, (repeat == 0) ? -1 : (int)repeat - 1) != 0) {
        fprintf(stderr, "[audio] buffered music play failed: %s\n", mixer_error());
        stop_music();
        return 1;
    }
    return 0;
}

int32_t s3eAudioStop(void) {
    if (audio_open()) {
        stop_music();
    }
    return 0;
}

int32_t s3eAudioPause(void) {
    if (audio_open()) {
        g_mixer.PauseMusic();
        g_audio_paused = 1;
    }
    return 0;
}

int32_t s3eAudioResume(void) {
    if (audio_open()) {
        g_mixer.ResumeMusic();
        g_audio_paused = 0;
    }
    return 0;
}

int32_t s3eAudioRegister(uint32_t id, void *callback, void *user_data) {
    (void)id;
    (void)callback;
    (void)user_data;
    return 0;
}

int32_t s3eSoundGetFreeChannel(void) {
    init_sound_slots();
    if (!audio_open()) {
        return -1;
    }
    service_finished_channels();
    for (int channel = 0; channel < SOUND_CHANNELS; ++channel) {
        if (!g_sound_slots[channel].chunk) {
            return channel;
        }
    }
    /* NextOS: canais esgotados = sons novos silenciados; logar sempre (raro e grave). */
    fprintf(stderr, "[audio] NO FREE SOUND CHANNEL (all %d busy)\n", SOUND_CHANNELS);
    return -1;
}

int32_t s3eSoundSetInt(uint32_t key, int32_t value) {
    init_sound_slots();
    if (key == 0) {
        g_sound_volume_s3e = clamp_volume_256(value);
        g_sound_volume_mix = mix_volume_from_s3e(g_sound_volume_s3e);
        if (audio_open()) {
            for (int channel = 0; channel < SOUND_CHANNELS; ++channel) {
                g_mixer.Volume(channel, mixed_channel_volume(&g_sound_slots[channel]));
            }
        }
        return 0;
    }
    if (key == 2) {
        g_sound_rate = clamp_rate(value);
        for (int channel = 0; channel < SOUND_CHANNELS; ++channel) {
            if (!g_sound_slots[channel].chunk) {
                g_sound_slots[channel].rate = g_sound_rate;
            }
        }
        return 0;
    }
    return 1;
}

int32_t s3eSoundGetInt(uint32_t key) {
    switch (key) {
    case 0:
        return g_sound_volume_s3e;
    case 1:
        return SOUND_FREQUENCY;
    case 2:
        return g_sound_rate;
    case 3:
        return SOUND_CHANNELS;
    case 5:
        return audio_open() ? 1 : 0;
    case 7:
        return SOUND_OUTPUT_CHANNELS == 2 ? 1 : 0;
    default:
        return -1;
    }
}

int32_t s3eSoundChannelRegister(int32_t channel, uint32_t callback_id, void *callback,
                                void *user_data) {
    if (getenv("SIMS3_AUDIO_LOG")) {
        fprintf(stderr, "[audio] s3eSoundChannelRegister(ch=%d, cbid=%u, fn=%p)%s\n",
                (int)channel, callback_id, callback,
                (callback_id == S3E_CHANNEL_GEN_AUDIO ||
                 callback_id == S3E_CHANNEL_GEN_AUDIO_STEREO)
                    ? " <-- GEN_AUDIO (nao suportado)"
                    : "");
    }
    if (!channel_valid(channel)) {
        return 1;
    }
    if (callback_id < SOUND_CALLBACK_TYPES) {
        g_channel_callbacks[channel][callback_id].fn = callback;
        g_channel_callbacks[channel][callback_id].user_data = user_data;
        return 0;
    }
    /* GEN_AUDIO (2/3) nao suportado: retornar erro faz a engine usar o caminho
       simples (ChannelPlay) em vez de esperar streaming que nunca tocaria. */
    return 1;
}

int32_t s3eSoundChannelUnRegister(int32_t channel, uint32_t callback_id) {
    if (!channel_valid(channel) || callback_id >= SOUND_CALLBACK_TYPES) {
        return 1;
    }
    g_channel_callbacks[channel][callback_id].fn = NULL;
    g_channel_callbacks[channel][callback_id].user_data = NULL;
    return 0;
}

int32_t s3eSoundChannelPlay(int32_t channel, const void *data, uint32_t size, uint32_t repeat) {
    init_sound_slots();
    if (getenv("SIMS3_AUDIO_LOG")) {
        fprintf(stderr, "[audio] s3eSoundChannelPlay(ch=%d, samples=%u, repeat=%u)\n",
                (int)channel, size, repeat);
    }
    if (!audio_open() || !data || size == 0 || size > UINT32_MAX / 2u) {
        return 1;
    }
    service_finished_channels();

    if (channel < 0) {
        channel = s3eSoundGetFreeChannel();
    }
    if (!channel_valid(channel)) {
        return 1;
    }

    struct sound_slot *slot = &g_sound_slots[channel];
    uint32_t pcm_samples = size;
    int16_t *decoded = decode_ima_adpcm_mono(data, (size_t)size * sizeof(int16_t), &pcm_samples);
    const int16_t *pcm = decoded ? decoded : data;

    uint32_t wav_size = 0;
    uint8_t *wav = create_pcm16_mono_wav(pcm, pcm_samples, slot->rate, &wav_size);
    free(decoded);
    if (!wav) {
        return 1;
    }

    void *rw = g_sdl_audio.RWFromConstMem(wav, (int)wav_size);
    if (!rw) {
        free(wav);
        return 1;
    }

    void *chunk = g_mixer.LoadWAV_RW(rw, 1);
    free(wav);
    if (!chunk) {
        fprintf(stderr, "[audio] sound load failed: %s\n", mixer_error());
        return 1;
    }

    g_mixer.HaltChannel(channel);
    free_sound_slot(channel);
    slot->chunk = chunk;
    slot->finished = 0;
    g_mixer.Volume(channel, mixed_channel_volume(slot));

    /* NextOS: EMPIRICO (log de auditoria): este jogo passa repeat=0 em todos os
       one-shots = tocar UMA vez (a engine recicla via END_SAMPLE, nao via loop).
       repeat>1 = tocar N vezes. Nunca loop infinito vindo daqui. */
    int loops = (repeat <= 1) ? 0 : (int)repeat - 1;
    int played_channel = g_mixer.PlayChannelTimed(channel, chunk, loops, -1);
    if (played_channel < 0) {
        fprintf(stderr, "[audio] sound play failed: %s\n", mixer_error());
        free_sound_slot(channel);
        return 1;
    }
    return 0;
}

int32_t s3eSoundChannelStop(int32_t channel) {
    if (!audio_open() || !channel_valid(channel)) {
        return 1;
    }
    if (getenv("SIMS3_AUDIO_LOG")) {
        fprintf(stderr, "[audio] s3eSoundChannelStop(ch=%d)\n", (int)channel);
    }
    g_mixer.HaltChannel(channel);
    free_sound_slot(channel);
    /* NextOS: NAO despachar STOP_AUDIO aqui — a engine chamou Stop, ela JA sabe.
       E cancela END pendente: nao avisar "acabou" de som que ela mandou parar. */
    g_channel_end_pending[channel] = 0;
    return 0;
}

/* NFS Shift (NextOS): import do modulo que o loader base nao exportava. */
int32_t s3eSoundStopAllChannels(void) {
    if (!audio_open()) {
        return 1;
    }
    for (int channel = 0; channel < SOUND_CHANNELS; ++channel) {
        g_mixer.HaltChannel(channel);
        free_sound_slot(channel);
        g_channel_end_pending[channel] = 0;
    }
    return 0;
}

/* NFS Shift (NextOS): erros de musica ja sao logados nos pontos de falha;
   o jogo so consulta isto p/ decidir retry. */
int32_t s3eAudioGetError(void) {
    return 0;
}

/* NFS Shift (NextOS): codecs por SDL2_mixer — MP3 (trilhas bgm) e PCM.
   Enum s3eAudioCodec: MIDI=0 MP3=1 AAC=2 AACPLUS=3 QCP=4 PCM=5 SPF=6 AMR=7 MP4=8 */
int32_t s3eAudioIsCodecSupported(uint32_t codec) {
    return codec == 1 || codec == 5;
}

int32_t s3eSoundChannelPause(int32_t channel) {
    if (!audio_open() || !channel_valid(channel)) {
        return 1;
    }
    g_mixer.Pause(channel);
    return 0;
}

int32_t s3eSoundChannelResume(int32_t channel) {
    if (!audio_open() || !channel_valid(channel)) {
        return 1;
    }
    g_mixer.Resume(channel);
    return 0;
}

int32_t s3eSoundChannelSetInt(int32_t channel, uint32_t key, int32_t value) {
    init_sound_slots();
    if (!channel_valid(channel)) {
        return 1;
    }
    struct sound_slot *slot = &g_sound_slots[channel];

    switch (key) {
    case 0:
        slot->rate_scale = value;
        slot->rate = rate_from_scale(value);
        return 0;
    case 1:
        slot->rate = clamp_rate(value);
        return 0;
    case 2:
        slot->position = value;
        return 0;
    case 3:
        slot->volume_s3e = clamp_volume_256(value);
        slot->volume_mix = mix_volume_from_s3e(slot->volume_s3e);
        if (audio_open()) {
            g_mixer.Volume(channel, mixed_channel_volume(slot));
        }
        return 0;
    default:
        return 1;
    }
}

int32_t s3eSoundChannelGetInt(int32_t channel, uint32_t key) {
    init_sound_slots();
    if (!channel_valid(channel)) {
        return -1;
    }
    struct sound_slot *slot = &g_sound_slots[channel];

    switch (key) {
    case 0:
        return slot->rate_scale;
    case 1:
        return slot->rate;
    case 2:
        return slot->position;
    case 3:
        return slot->volume_s3e;
    case 4:
        if (!audio_open()) {
            return 0;
        }
        return g_mixer.Playing(channel) != 0 ? 1 : 0;
    case 5:
        if (!audio_open()) {
            return 0;
        }
        return g_mixer.Playing(channel) != 0 ? 1 : 0;
    default:
        return -1;
    }
}
