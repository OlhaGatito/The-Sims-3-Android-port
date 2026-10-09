#ifndef NXMIX_H
#define NXMIX_H
#include <stdint.h>

struct sdl_mixer_api {
    int (*Init)(int flags);
    void (*Quit)(void);
    int (*OpenAudio)(int frequency, uint16_t format, int channels, int chunksize);
    void (*CloseAudio)(void);
    int (*AllocateChannels)(int num_channels);
    void (*ChannelFinished)(void (*callback)(int channel));
    int (*PlayChannelTimed)(int channel, void *chunk, int loops, int ticks);
    int (*Playing)(int channel);
    int (*HaltChannel)(int channel);
    void (*Pause)(int channel);
    void (*Resume)(int channel);
    int (*Volume)(int channel, int volume);
    void *(*LoadWAV_RW)(void *src, int freesrc);
    void (*FreeChunk)(void *chunk);
    void *(*LoadMUS)(const char *file);
    void *(*LoadMUS_RW)(void *rw, int freesrc);
    int (*PlayMusic)(void *music, int loops);
    int (*PlayingMusic)(void);
    int (*HaltMusic)(void);
    void (*PauseMusic)(void);
    void (*ResumeMusic)(void);
    int (*VolumeMusic)(int volume);
    void (*FreeMusic)(void *music);
    const char *(*GetError)(void);
};

int nxmix_bind(struct sdl_mixer_api *api, void *sdl2);
void *nxmix_rw_from_const_mem(const void *mem, int size);
int32_t nxmix_peak(void);
uint64_t nxmix_frames(void);
int nxmix_is_virtual(void);
int nxmix_selftest(void *sdl2, int seconds);
#endif
