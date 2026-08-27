#include "allstar_audio.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#if defined(_WIN32) && !defined(ALLSTAR_USE_SDL_AUDIO)
#include <windows.h>
#include <mmsystem.h>
#elif defined(ALLSTAR_USE_SDL_AUDIO)
#include <SDL3/SDL.h>
#endif

#define MIX_BUFFER_SAMPLES 2048
#define MIX_CHANNELS 2
#define MIX_SAMPLE_RATE 48000
#define MAX_SFX_VOICES 4
#define DMG_AUDIO_OVERSAMPLE 8

#if defined(_WIN32) || defined(ALLSTAR_USE_SDL_AUDIO)
#define ALLSTAR_AUDIO_OUTPUT 1
#else
#define ALLSTAR_AUDIO_OUTPUT 0
#endif

typedef struct {
    int16_t *samples;
    uint32_t sample_count; /* in 16-bit stereo frame units */
    uint32_t loop_start;
    bool loaded;
} PcmSound;

#if ALLSTAR_AUDIO_OUTPUT
static PcmSound g_bgm[ALLSTAR_BGM_COUNT];
static PcmSound g_sfx[ALLSTAR_SFX_COUNT];
static volatile AllStarBgmId g_requested_bgm = ALLSTAR_BGM_NONE;
static volatile AllStarBgmId g_current_bgm = ALLSTAR_BGM_NONE;
static uint32_t g_bgm_playhead = 0;

/* Active SFX voice slots */
typedef struct {
    AllStarSfxId sfx_id;
    uint32_t playhead;
    bool active;
} SfxVoice;

static SfxVoice g_sfx_voices[MAX_SFX_VOICES];

#if defined(_WIN32) && !defined(ALLSTAR_USE_SDL_AUDIO)
static HANDLE g_mixer_thread = NULL;
static volatile bool g_mixer_running = false;
static CRITICAL_SECTION g_audio_lock;
static bool g_lock_initialized = false;
#elif defined(ALLSTAR_USE_SDL_AUDIO)
static SDL_Mutex *g_audio_lock = NULL;
static SDL_AudioStream *g_audio_stream = NULL;
#endif

static void lock_audio(void) {
#if defined(_WIN32) && !defined(ALLSTAR_USE_SDL_AUDIO)
    if (g_lock_initialized) EnterCriticalSection(&g_audio_lock);
#elif defined(ALLSTAR_USE_SDL_AUDIO)
    if (g_audio_lock) SDL_LockMutex(g_audio_lock);
#endif
}

static void unlock_audio(void) {
#if defined(_WIN32) && !defined(ALLSTAR_USE_SDL_AUDIO)
    if (g_lock_initialized) LeaveCriticalSection(&g_audio_lock);
#elif defined(ALLSTAR_USE_SDL_AUDIO)
    if (g_audio_lock) SDL_UnlockMutex(g_audio_lock);
#endif
}
#endif

static FILE *open_binary_file(const char *path, const char *mode) {
#ifdef _MSC_VER
    FILE *file = NULL;
    fopen_s(&file, path, mode);
    return file;
#else
    return fopen(path, mode);
#endif
}

#if ALLSTAR_AUDIO_OUTPUT
static uint16_t read_le16(const uint8_t *bytes) {
    return (uint16_t)((uint16_t)bytes[0] |
                      ((uint16_t)bytes[1] << 8));
}

static uint32_t read_le32(const uint8_t *bytes) {
    return (uint32_t)bytes[0] |
           ((uint32_t)bytes[1] << 8) |
           ((uint32_t)bytes[2] << 16) |
           ((uint32_t)bytes[3] << 24);
}

static bool load_pcm_wav(const char *filename, PcmSound *out) {
    const char *base_path = NULL;
    FILE *f = NULL;
    char path[1024];
    if (!out) return false;
    memset(out, 0, sizeof(PcmSound));

    snprintf(path, sizeof(path), "assets/audio/%s", filename);
    f = open_binary_file(path, "rb");
    if (!f) {
        snprintf(path, sizeof(path), "build/assets/audio/%s", filename);
        f = open_binary_file(path, "rb");
    }
    if (!f) {
#if defined(_WIN32) && !defined(ALLSTAR_USE_SDL_AUDIO)
        char exe_dir[MAX_PATH];
        GetModuleFileNameA(NULL, exe_dir, MAX_PATH);
        char *last_slash = strrchr(exe_dir, '\\');
        if (last_slash) *last_slash = '\0';
        snprintf(path, sizeof(path), "%s\\assets\\audio\\%s", exe_dir, filename);
        f = open_binary_file(path, "rb");
#elif defined(ALLSTAR_USE_SDL_AUDIO)
        base_path = SDL_GetBasePath();
        if (base_path) {
            snprintf(path, sizeof(path), "%s../Resources/audio/%s",
                     base_path, filename);
            f = open_binary_file(path, "rb");
        }
#endif
    }
    if (!f) return false;

    /* Read RIFF header */
    uint8_t riff[12];
    if (fread(riff, 1, 12, f) != 12 || memcmp(riff, "RIFF", 4) != 0 || memcmp(riff + 8, "WAVE", 4) != 0) {
        fclose(f);
        return false;
    }

    uint16_t channels = 2;
    uint32_t sample_rate = 48000;
    uint16_t bits_per_sample = 16;
    uint8_t *raw_buf = NULL;
    uint32_t data_size = 0;

    /* Parse RIFF chunks */
    uint8_t chunk_hdr[8];
    while (fread(chunk_hdr, 1, 8, f) == 8) {
        uint32_t chunk_size = read_le32(chunk_hdr + 4);
        if (memcmp(chunk_hdr, "fmt ", 4) == 0) {
            uint8_t fmt_data[16];
            if (fread(fmt_data, 1, 16, f) == 16) {
                channels = read_le16(fmt_data + 2);
                sample_rate = read_le32(fmt_data + 4);
                bits_per_sample = read_le16(fmt_data + 14);
                if (chunk_size > 16) fseek(f, (long)(chunk_size - 16), SEEK_CUR);
            }
        } else if (memcmp(chunk_hdr, "data", 4) == 0) {
            data_size = chunk_size;
            raw_buf = (uint8_t*)malloc(data_size);
            if (raw_buf) {
                fread(raw_buf, 1, data_size, f);
            }
            break;
        } else {
            fseek(f, (long)chunk_size, SEEK_CUR);
        }
    }
    fclose(f);

    /* The native mixer preserves the Win32 host's fixed 48 kHz behavior. */
    (void)sample_rate;
    if (!raw_buf || data_size == 0 ||
        (channels != 1 && channels != 2) || bits_per_sample != 16) {
        if (raw_buf) free(raw_buf);
        return false;
    }

    uint32_t bytes_per_frame = channels * (bits_per_sample / 8);
    uint32_t total_frames = data_size / bytes_per_frame;
    out->samples = (int16_t*)malloc(total_frames * MIX_CHANNELS * sizeof(int16_t));
    if (!out->samples) {
        free(raw_buf);
        return false;
    }

    int16_t *src16 = (int16_t*)raw_buf;
    for (uint32_t i = 0; i < total_frames; i++) {
        if (channels == 1) {
            out->samples[i * 2 + 0] = src16[i];
            out->samples[i * 2 + 1] = src16[i];
        } else {
            out->samples[i * 2 + 0] = src16[i * 2 + 0];
            out->samples[i * 2 + 1] = src16[i * 2 + 1];
        }
    }
    free(raw_buf);

    out->sample_count = total_frames;
    out->loaded = true;
    return true;
}
#endif

/* The cartridge's made-basket path reaches sound command $05 at $1F23.
   Until the complete $3014 command-stream interpreter is shared by every
   mode, keep gameplay cues audible with deterministic DMG-like square-wave
   fallbacks. User-provided WAVs still take precedence when present. */
static bool generate_square_sequence(PcmSound *out,
                                     const float *frequencies,
                                     const float *durations,
                                     size_t segment_count,
                                     float amplitude) {
    uint32_t total_frames = 0;
    uint32_t cursor = 0;
    size_t segment;
    if (!out || !frequencies || !durations || segment_count == 0) return false;
    for (segment = 0; segment < segment_count; segment++) {
        total_frames += (uint32_t)(durations[segment] * MIX_SAMPLE_RATE);
    }
    if (total_frames == 0) return false;
    out->samples = (int16_t*)malloc(
        (size_t)total_frames * MIX_CHANNELS * sizeof(int16_t));
    if (!out->samples) return false;

    for (segment = 0; segment < segment_count; segment++) {
        uint32_t segment_frames =
            (uint32_t)(durations[segment] * MIX_SAMPLE_RATE);
        uint32_t frame;
        float phase = 0.0f;
        float phase_step = frequencies[segment] / (float)MIX_SAMPLE_RATE;
        for (frame = 0; frame < segment_frames; frame++) {
            float envelope = 1.0f - (float)frame / (float)segment_frames;
            int16_t sample = (int16_t)(
                (phase < 0.5f ? amplitude : -amplitude) * envelope);
            out->samples[(cursor + frame) * 2] = sample;
            out->samples[(cursor + frame) * 2 + 1] = sample;
            phase += phase_step;
            if (phase >= 1.0f) phase -= 1.0f;
        }
        cursor += segment_frames;
    }
    out->sample_count = total_frames;
    out->loaded = true;
    return true;
}

static bool generate_noise_burst(PcmSound *out, float duration,
                                 float amplitude) {
    uint32_t total_frames;
    uint32_t frame;
    uint16_t lfsr = 0x7fff;
    if (!out || duration <= 0.0f) return false;
    total_frames = (uint32_t)(duration * MIX_SAMPLE_RATE);
    out->samples = (int16_t*)malloc(
        (size_t)total_frames * MIX_CHANNELS * sizeof(int16_t));
    if (!out->samples) return false;
    for (frame = 0; frame < total_frames; frame++) {
        float envelope = 1.0f - (float)frame / (float)total_frames;
        uint16_t feedback = (uint16_t)((lfsr ^ (lfsr >> 1)) & 1);
        int16_t sample;
        lfsr = (uint16_t)((lfsr >> 1) | (feedback << 14));
        sample = (int16_t)((lfsr & 1 ? amplitude : -amplitude) * envelope);
        out->samples[frame * 2] = sample;
        out->samples[frame * 2 + 1] = sample;
    }
    out->sample_count = total_frames;
    out->loaded = true;
    return true;
}

static float dmg_square_duty(uint8_t duty_length) {
    static const float duty[4] = {0.125f, 0.25f, 0.5f, 0.75f};
    return duty[(duty_length >> 6) & 3u];
}

static float dmg_square_hz(uint16_t frequency) {
    if (frequency >= 2048) return 0.0f;
    return 131072.0f / (float)(2048u - frequency);
}

/* Render the exact per-frame register program extracted from $3014's ROM
   data. Timing uses one Game Boy frame (70224 clocks at 4194304 Hz), and
   square-1 sweep reproduces NR10 for command $0D between register writes. */
static void step_envelope(uint8_t envelope, uint8_t *volume,
                          double time, double *next_time) {
    uint8_t pace = envelope & 7u;
    if (!volume || !next_time || pace == 0) return;
    while (time >= *next_time) {
        if ((envelope & 0x08) != 0) {
            if (*volume < 15) (*volume)++;
        } else if (*volume > 0) {
            (*volume)--;
        }
        *next_time += (double)pace / 64.0;
    }
}

/*
 * The DMG envelope level `seconds` into a note, from the NR12/NR22/NR42 byte:
 * bits 7-4 the starting level, bit 3 the direction, bits 2-0 the pace in
 * 1/64s units.  A pace of zero means the level never moves.
 *
 * This is what the renderers step incrementally, exposed so a test can pin the
 * curve rather than eyeballing a waveform.
 */
uint8_t allstar_audio_rom_envelope_level(uint8_t envelope, double seconds) {
    uint8_t volume = (uint8_t)(envelope >> 4);
    uint8_t pace = envelope & 7u;
    double next = pace != 0 ? (double)pace / 64.0 : 1.0e30;
    if (seconds < 0.0) return volume;
    step_envelope(envelope, &volume, seconds, &next);
    return volume;
}

static bool generate_rom_program(PcmSound *out,
                                 const AllStarRomSfxProgram *program) {
    const double frame_seconds = 70224.0 / 4194304.0;
    uint32_t total_samples;
    uint32_t sample;
    uint32_t previous_frame = UINT32_MAX;
    uint16_t frequency1 = 0;
    uint16_t frequency2 = 0;
    uint16_t sweep_shadow = 0;
    double next_sweep_time = 0.0;
    float phase1 = 0.0f;
    float phase2 = 0.0f;
    float noise_phase = 0.0f;
    uint16_t noise_lfsr = 0x7fff;
    uint8_t noise_polynomial = 0;
    uint8_t noise_volume = 0;
    double next_noise_envelope_time = 0.0;
    float duty1;
    float duty2;
    uint8_t volume1;
    uint8_t volume2;
    double next_envelope1 = 1.0e30;
    double next_envelope2 = 1.0e30;
    if (!out || !program || program->frame_count == 0 ||
        program->frame_count > ALLSTAR_ROM_SFX_MAX_FRAMES) return false;
    total_samples = (uint32_t)ceil(
        program->frame_count * frame_seconds * MIX_SAMPLE_RATE);
    out->samples = (int16_t*)malloc(
        (size_t)total_samples * MIX_CHANNELS * sizeof(int16_t));
    if (!out->samples) return false;
    duty1 = dmg_square_duty(program->square1_duty_length);
    duty2 = dmg_square_duty(program->square2_duty_length);
    /* NR12/NR22 bits 7-4 are the starting level; the low three bits
       are the pace the envelope steps at, and bit 3 its direction.
       $0F's descriptor gives NR12 = $F1 -- level 15, decreasing, pace
       one -- so the cue decays to silence in fifteen 1/64s steps.
       Holding it flat instead makes a short blip a sustained tone. */
    volume1 = (uint8_t)(program->square1_envelope >> 4);
    volume2 = (uint8_t)(program->square2_envelope >> 4);

    for (sample = 0; sample < total_samples; sample++) {
        double time = (double)sample / MIX_SAMPLE_RATE;
        uint32_t frame_index = (uint32_t)(time / frame_seconds);
        const AllStarRomSfxFrame *frame;
        float mixed = 0.0f;
        if (frame_index >= program->frame_count)
            frame_index = program->frame_count - 1;
        frame = &program->frames[frame_index];
        if (frame_index != previous_frame) {
            if ((frame->flags & ALLSTAR_ROM_SFX_CHANNEL_1) != 0) {
                if ((frame->flags & ALLSTAR_ROM_SFX_TRIGGER_1) != 0) {
                    uint8_t pace1 = program->square1_envelope & 7u;
                    frequency1 = frame->square1_frequency;
                    sweep_shadow = frequency1;
                    phase1 = 0.0f;
                    volume1 = (uint8_t)(program->square1_envelope >> 4);
                    next_envelope1 = pace1 != 0
                        ? time + (double)pace1 / 64.0 : 1.0e30;
                    if ((program->square1_sweep & 0x70) != 0)
                        next_sweep_time = time +
                            ((program->square1_sweep >> 4) & 7u) / 128.0;
                } else {
                    frequency1 = (uint16_t)((frequency1 & 0x0700u) |
                        (frame->square1_frequency & 0x00ffu));
                }
            }
            if ((frame->flags & ALLSTAR_ROM_SFX_CHANNEL_2) != 0) {
                frequency2 = frame->square2_frequency;
                if ((frame->flags & ALLSTAR_ROM_SFX_TRIGGER_2) != 0) {
                    uint8_t pace2 = program->square2_envelope & 7u;
                    phase2 = 0.0f;
                    volume2 = (uint8_t)(program->square2_envelope >> 4);
                    next_envelope2 = pace2 != 0
                        ? time + (double)pace2 / 64.0 : 1.0e30;
                }
            }
            if ((frame->flags & ALLSTAR_ROM_SFX_CHANNEL_4) != 0) {
                noise_polynomial = frame->noise_polynomial;
                if ((frame->flags & ALLSTAR_ROM_SFX_TRIGGER_4) != 0) {
                    uint8_t pace = program->noise_envelope & 7u;
                    noise_lfsr = 0x7fff;
                    noise_phase = 0.0f;
                    noise_volume = program->noise_envelope >> 4;
                    next_noise_envelope_time = pace != 0
                        ? time + (double)pace / 64.0 : 1.0e30;
                }
            }
            previous_frame = frame_index;
        }

        if ((program->square1_sweep & 0x70) != 0) {
            uint8_t pace = (program->square1_sweep >> 4) & 7u;
            uint8_t shift = program->square1_sweep & 7u;
            while (pace != 0 && shift != 0 && time >= next_sweep_time) {
                uint16_t delta = (uint16_t)(sweep_shadow >> shift);
                if ((program->square1_sweep & 0x08) != 0)
                    sweep_shadow = (uint16_t)(sweep_shadow - delta);
                else
                    sweep_shadow = (uint16_t)(sweep_shadow + delta);
                if (sweep_shadow < 2048) frequency1 = sweep_shadow;
                next_sweep_time += pace / 128.0;
            }
        }

        step_envelope(program->square1_envelope, &volume1,
                      time, &next_envelope1);
        step_envelope(program->square2_envelope, &volume2,
                      time, &next_envelope2);

        if ((frame->flags & ALLSTAR_ROM_SFX_CHANNEL_1) != 0) {
            mixed += (phase1 < duty1 ? 1.0f : -1.0f) *
                ((float)volume1 / 15.0f);
            phase1 += dmg_square_hz(frequency1) / MIX_SAMPLE_RATE;
            if (phase1 >= 1.0f) phase1 -= floorf(phase1);
        }
        if ((frame->flags & ALLSTAR_ROM_SFX_CHANNEL_2) != 0) {
            mixed += (phase2 < duty2 ? 1.0f : -1.0f) *
                ((float)volume2 / 15.0f);
            phase2 += dmg_square_hz(frequency2) / MIX_SAMPLE_RATE;
            if (phase2 >= 1.0f) phase2 -= floorf(phase2);
        }
        if ((frame->flags & ALLSTAR_ROM_SFX_CHANNEL_4) != 0) {
            uint8_t divisor_code = noise_polynomial & 7u;
            uint8_t shift = noise_polynomial >> 4;
            float divisor = divisor_code == 0
                ? 0.5f : (float)divisor_code;
            float noise_hz = 262144.0f /
                (divisor * (float)(1u << shift));
            uint8_t pace = program->noise_envelope & 7u;
            while (pace != 0 && time >= next_noise_envelope_time) {
                if ((program->noise_envelope & 0x08) != 0) {
                    if (noise_volume < 15) noise_volume++;
                } else if (noise_volume > 0) {
                    noise_volume--;
                }
                next_noise_envelope_time += (double)pace / 64.0;
            }
            noise_phase += noise_hz / MIX_SAMPLE_RATE;
            while (noise_phase >= 1.0f) {
                uint16_t feedback = (uint16_t)(
                    (noise_lfsr ^ (noise_lfsr >> 1)) & 1u);
                noise_lfsr = (uint16_t)((noise_lfsr >> 1) |
                                        (feedback << 14));
                if ((noise_polynomial & 0x08) != 0) {
                    noise_lfsr = (uint16_t)(
                        (noise_lfsr & ~(1u << 6)) | (feedback << 6));
                }
                noise_phase -= 1.0f;
            }
            mixed += (noise_lfsr & 1u ? 1.0f : -1.0f) *
                ((float)noise_volume / 15.0f);
        }
        if (mixed > 1.8f) mixed = 1.8f;
        if (mixed < -1.8f) mixed = -1.8f;
        {
            int16_t value = (int16_t)(mixed * 6200.0f);
            out->samples[sample * 2] = value;
            out->samples[sample * 2 + 1] = value;
        }
    }
    out->sample_count = total_samples;
    out->loaded = true;
    return true;
}

static float dmg_wave_hz(uint16_t frequency) {
    if (frequency >= 2048) return 0.0f;
    return 65536.0f / (float)(2048u - frequency);
}

static bool generate_rom_music(PcmSound *out,
                               const AllStarRomMusicProgram *program) {
    const double frame_seconds = 70224.0 / 4194304.0;
    uint32_t total_samples;
    uint32_t sample;
    uint32_t previous_frame = UINT32_MAX;
    uint16_t frequency1 = 0;
    uint16_t frequency2 = 0;
    uint16_t wave_frequency = 0;
    uint16_t sweep_shadow = 0;
    uint8_t square1_sweep = 0;
    uint8_t square1_envelope = 0;
    uint8_t square2_envelope = 0;
    uint8_t noise_envelope = 0;
    uint8_t volume1 = 0;
    uint8_t volume2 = 0;
    uint8_t noise_volume = 0;
    uint8_t wave_level = 0;
    uint8_t wave_table = 0;
    uint8_t noise_polynomial = 0;
    float duty1 = 0.5f;
    float duty2 = 0.5f;
    float phase1 = 0.0f;
    float phase2 = 0.0f;
    float wave_phase = 0.0f;
    float noise_phase = 0.0f;
    uint16_t noise_lfsr = 0x7fff;
    double next_envelope1 = 1.0e30;
    double next_envelope2 = 1.0e30;
    double next_noise_envelope = 1.0e30;
    double next_sweep_time = 1.0e30;
    float high_pass_left = 0.0f;
    float high_pass_right = 0.0f;
    if (!out || !program || program->frame_count == 0 ||
        program->frame_count > ALLSTAR_ROM_MUSIC_MAX_FRAMES ||
        program->loop_frame >= program->frame_count) return false;
    total_samples = (uint32_t)ceil(
        program->frame_count * frame_seconds * MIX_SAMPLE_RATE);
    out->samples = (int16_t *)malloc(
        (size_t)total_samples * MIX_CHANNELS * sizeof(int16_t));
    if (!out->samples) return false;
    out->loop_start = (uint32_t)ceil(
        program->loop_frame * frame_seconds * MIX_SAMPLE_RATE);

    for (sample = 0; sample < total_samples; sample++) {
        double time = (double)sample / MIX_SAMPLE_RATE;
        uint32_t frame_index = (uint32_t)(time / frame_seconds);
        const AllStarRomMusicFrame *frame;
        float mixed_left = 0.0f;
        float mixed_right = 0.0f;
        if (frame_index >= program->frame_count)
            frame_index = program->frame_count - 1;
        frame = &program->frames[frame_index];
        if (frame_index != previous_frame) {
            if ((frame->flags & ALLSTAR_ROM_MUSIC_SQUARE1) != 0) {
                frequency1 = frame->square1_frequency;
                if ((frame->flags & ALLSTAR_ROM_MUSIC_TRIGGER1) != 0) {
                    uint8_t pace;
                    square1_sweep = frame->square1_sweep;
                    square1_envelope = frame->square1_envelope;
                    duty1 = dmg_square_duty(frame->square1_duty_length);
                    volume1 = square1_envelope >> 4;
                    pace = square1_envelope & 7u;
                    next_envelope1 = pace != 0
                        ? time + (double)pace / 64.0 : 1.0e30;
                    sweep_shadow = frequency1;
                    pace = (square1_sweep >> 4) & 7u;
                    next_sweep_time = pace != 0
                        ? time + (double)pace / 128.0 : 1.0e30;
                }
            }
            if ((frame->flags & ALLSTAR_ROM_MUSIC_SQUARE2) != 0) {
                frequency2 = frame->square2_frequency;
                if ((frame->flags & ALLSTAR_ROM_MUSIC_TRIGGER2) != 0) {
                    uint8_t pace;
                    square2_envelope = frame->square2_envelope;
                    duty2 = dmg_square_duty(frame->square2_duty_length);
                    volume2 = square2_envelope >> 4;
                    pace = square2_envelope & 7u;
                    next_envelope2 = pace != 0
                        ? time + (double)pace / 64.0 : 1.0e30;
                }
            }
            if ((frame->flags & ALLSTAR_ROM_MUSIC_WAVE) != 0) {
                wave_frequency = frame->wave_frequency;
                if ((frame->flags & ALLSTAR_ROM_MUSIC_TRIGGER_WAVE) != 0)
                    wave_phase = 0.0f;
                wave_level = frame->wave_output_level;
                wave_table = frame->wave_table & 0x0f;
            }
            if ((frame->flags & ALLSTAR_ROM_MUSIC_NOISE) != 0) {
                noise_polynomial = frame->noise_polynomial;
                if ((frame->flags & ALLSTAR_ROM_MUSIC_TRIGGER_NOISE) != 0) {
                    uint8_t pace;
                    noise_lfsr = 0x7fff;
                    noise_phase = 0.0f;
                    noise_envelope = frame->noise_envelope;
                    noise_volume = noise_envelope >> 4;
                    pace = noise_envelope & 7u;
                    next_noise_envelope = pace != 0
                        ? time + (double)pace / 64.0 : 1.0e30;
                }
            }
            previous_frame = frame_index;
        }

        step_envelope(square1_envelope, &volume1,
                      time, &next_envelope1);
        step_envelope(square2_envelope, &volume2,
                      time, &next_envelope2);
        step_envelope(noise_envelope, &noise_volume,
                      time, &next_noise_envelope);
        if ((square1_sweep & 0x70) != 0) {
            uint8_t pace = (square1_sweep >> 4) & 7u;
            uint8_t shift = square1_sweep & 7u;
            while (pace != 0 && shift != 0 && time >= next_sweep_time) {
                uint16_t delta = (uint16_t)(sweep_shadow >> shift);
                if ((square1_sweep & 0x08) != 0)
                    sweep_shadow = (uint16_t)(sweep_shadow - delta);
                else
                    sweep_shadow = (uint16_t)(sweep_shadow + delta);
                if (sweep_shadow < 2048) frequency1 = sweep_shadow;
                next_sweep_time += pace / 128.0;
            }
        }

        {
            unsigned sub_sample;
            const float oversampled_rate =
                (float)(MIX_SAMPLE_RATE * DMG_AUDIO_OVERSAMPLE);
            /* NR51: low nibble is the right side, high nibble the left. */
            const uint8_t panning = frame->panning;
            for (sub_sample = 0; sub_sample < DMG_AUDIO_OVERSAMPLE;
                 sub_sample++) {
                float sub_left = 0.0f;
                float sub_right = 0.0f;
                float voice;
                if ((frame->flags & ALLSTAR_ROM_MUSIC_SQUARE1) != 0) {
                    voice = (phase1 < duty1 ? 1.0f : -1.0f) *
                        ((float)volume1 / 15.0f);
                    if ((panning & 0x10u) != 0) sub_left += voice;
                    if ((panning & 0x01u) != 0) sub_right += voice;
                    phase1 += dmg_square_hz(frequency1) / oversampled_rate;
                    if (phase1 >= 1.0f) phase1 -= floorf(phase1);
                }
                if ((frame->flags & ALLSTAR_ROM_MUSIC_SQUARE2) != 0) {
                    voice = (phase2 < duty2 ? 1.0f : -1.0f) *
                        ((float)volume2 / 15.0f);
                    if ((panning & 0x20u) != 0) sub_left += voice;
                    if ((panning & 0x02u) != 0) sub_right += voice;
                    phase2 += dmg_square_hz(frequency2) / oversampled_rate;
                    if (phase2 >= 1.0f) phase2 -= floorf(phase2);
                }
                if ((frame->flags & ALLSTAR_ROM_MUSIC_WAVE) != 0) {
                    uint8_t level_code = (wave_level >> 5) & 3u;
                    float scale = level_code == 1 ? 1.0f :
                        (level_code == 2 ? 0.5f :
                         (level_code == 3 ? 0.25f : 0.0f));
                    unsigned wave_sample =
                        (unsigned)(wave_phase * 32.0f) & 31u;
                    uint8_t packed = program->wave_tables[wave_table]
                        [wave_sample >> 1];
                    uint8_t nibble = (wave_sample & 1u) != 0
                        ? packed & 0x0f : packed >> 4;
                    voice = (((float)nibble - 7.5f) / 7.5f) * scale;
                    if ((panning & 0x40u) != 0) sub_left += voice;
                    if ((panning & 0x04u) != 0) sub_right += voice;
                    wave_phase += dmg_wave_hz(wave_frequency) /
                        oversampled_rate;
                    if (wave_phase >= 1.0f)
                        wave_phase -= floorf(wave_phase);
                }
                if ((frame->flags & ALLSTAR_ROM_MUSIC_NOISE) != 0) {
                    uint8_t divisor_code = noise_polynomial & 7u;
                    uint8_t shift = noise_polynomial >> 4;
                    float divisor = divisor_code == 0
                        ? 0.5f : (float)divisor_code;
                    float noise_hz = 262144.0f /
                        (divisor * (float)(1u << shift));
                    noise_phase += noise_hz / oversampled_rate;
                    while (noise_phase >= 1.0f) {
                        uint16_t feedback = (uint16_t)(
                            (noise_lfsr ^ (noise_lfsr >> 1)) & 1u);
                        noise_lfsr = (uint16_t)((noise_lfsr >> 1) |
                                                (feedback << 14));
                        if ((noise_polynomial & 0x08) != 0) {
                            noise_lfsr = (uint16_t)(
                                (noise_lfsr & ~(1u << 6)) |
                                (feedback << 6));
                        }
                        noise_phase -= 1.0f;
                    }
                    voice = (noise_lfsr & 1u ? 1.0f : -1.0f) *
                        ((float)noise_volume / 15.0f);
                    if ((panning & 0x80u) != 0) sub_left += voice;
                    if ((panning & 0x08u) != 0) sub_right += voice;
                }
                mixed_left += sub_left;
                mixed_right += sub_right;
            }
            mixed_left /= DMG_AUDIO_OVERSAMPLE;
            mixed_right /= DMG_AUDIO_OVERSAMPLE;
        }
        {
            float passed = mixed_left - high_pass_left;
            high_pass_left = mixed_left - passed * 0.996337f;
            mixed_left = passed;
            passed = mixed_right - high_pass_right;
            high_pass_right = mixed_right - passed * 0.996337f;
            mixed_right = passed;
        }
        if (mixed_left > 3.4f) mixed_left = 3.4f;
        if (mixed_left < -3.4f) mixed_left = -3.4f;
        if (mixed_right > 3.4f) mixed_right = 3.4f;
        if (mixed_right < -3.4f) mixed_right = -3.4f;
        out->samples[sample * 2] = (int16_t)(mixed_left * 4800.0f);
        out->samples[sample * 2 + 1] = (int16_t)(mixed_right * 4800.0f);
    }
    out->sample_count = total_samples;
    out->loaded = true;
    return true;
}

static void free_pcm_sound(PcmSound *sound) {
    if (!sound) return;
    free(sound->samples);
    memset(sound, 0, sizeof(*sound));
}

#if ALLSTAR_AUDIO_OUTPUT
static void generate_gameplay_sfx_fallbacks(void) {
    static const float dribble_f[] = {110.0f};
    static const float dribble_d[] = {0.055f};
    static const float shoot_f[] = {880.0f, 660.0f};
    static const float shoot_d[] = {0.035f, 0.055f};
    static const float score_chime_f[] = {740.0f, 988.0f, 1318.0f};
    static const float score_chime_d[] = {0.055f, 0.055f, 0.12f};
    static const float shoe_f[] = {1760.0f, 1245.0f};
    static const float shoe_d[] = {0.025f, 0.045f};
    static const float rim_f[] = {190.0f, 145.0f};
    static const float rim_d[] = {0.045f, 0.09f};
    static const float buzzer_f[] = {120.0f, 100.0f};
    static const float buzzer_d[] = {0.18f, 0.22f};
    static const float whistle_f[] = {1760.0f, 2093.0f};
    static const float whistle_d[] = {0.09f, 0.14f};
    static const float cheer_f[] = {330.0f, 392.0f, 494.0f, 659.0f};
    static const float cheer_d[] = {0.08f, 0.08f, 0.08f, 0.18f};
    static const float horse_letter_f[] = {740.0f, 988.0f};
    static const float horse_letter_d[] = {0.10f, 0.22f};

    if (!g_sfx[ALLSTAR_SFX_DRIBBLE].loaded)
        generate_square_sequence(&g_sfx[ALLSTAR_SFX_DRIBBLE],
            dribble_f, dribble_d, 1, 6500.0f);
    if (!g_sfx[ALLSTAR_SFX_SHOOT].loaded)
        generate_square_sequence(&g_sfx[ALLSTAR_SFX_SHOOT],
            shoot_f, shoot_d, 2, 7500.0f);
    if (!g_sfx[ALLSTAR_SFX_SWISH].loaded)
        generate_noise_burst(&g_sfx[ALLSTAR_SFX_SWISH], 0.12f, 6500.0f);
    if (!g_sfx[ALLSTAR_SFX_RIM_CLANK].loaded)
        generate_square_sequence(&g_sfx[ALLSTAR_SFX_RIM_CLANK],
            rim_f, rim_d, 2, 9000.0f);
    if (!g_sfx[ALLSTAR_SFX_BUZZER].loaded)
        generate_square_sequence(&g_sfx[ALLSTAR_SFX_BUZZER],
            buzzer_f, buzzer_d, 2, 8000.0f);
    if (!g_sfx[ALLSTAR_SFX_WHISTLE].loaded)
        generate_square_sequence(&g_sfx[ALLSTAR_SFX_WHISTLE],
            whistle_f, whistle_d, 2, 7500.0f);
    if (!g_sfx[ALLSTAR_SFX_CHEER].loaded)
        generate_square_sequence(&g_sfx[ALLSTAR_SFX_CHEER],
            cheer_f, cheer_d, 4, 7500.0f);
    if (!g_sfx[ALLSTAR_SFX_SHOE_SQUEAK].loaded)
        generate_square_sequence(&g_sfx[ALLSTAR_SFX_SHOE_SQUEAK],
            shoe_f, shoe_d, 2, 5200.0f);
    if (!g_sfx[ALLSTAR_SFX_SCORE_CHIME].loaded)
        generate_square_sequence(&g_sfx[ALLSTAR_SFX_SCORE_CHIME],
            score_chime_f, score_chime_d, 3, 9000.0f);
    if (!g_sfx[ALLSTAR_SFX_FREE_THROW_NET].loaded)
        generate_noise_burst(&g_sfx[ALLSTAR_SFX_FREE_THROW_NET],
                             0.20f, 6500.0f);
    if (!g_sfx[ALLSTAR_SFX_FREE_THROW_CONTACT].loaded)
        generate_square_sequence(&g_sfx[ALLSTAR_SFX_FREE_THROW_CONTACT],
            dribble_f, dribble_d, 1, 6500.0f);
    if (!g_sfx[ALLSTAR_SFX_HORSE_LETTER].loaded)
        generate_square_sequence(&g_sfx[ALLSTAR_SFX_HORSE_LETTER],
            horse_letter_f, horse_letter_d, 2, 7500.0f);
}

static void mix_audio_frames(int16_t *buffer, uint32_t frame_count) {
    AllStarBgmId requested;
    uint32_t sample;
    int voice;
    if (!buffer || frame_count == 0) return;
    memset(buffer, 0,
           (size_t)frame_count * MIX_CHANNELS * sizeof(int16_t));

    requested = g_requested_bgm;
    if (requested != g_current_bgm) {
        g_current_bgm = requested;
        g_bgm_playhead = 0;
    }

    if (g_current_bgm != ALLSTAR_BGM_NONE &&
        g_current_bgm < ALLSTAR_BGM_COUNT &&
        g_bgm[g_current_bgm].loaded) {
        PcmSound *track = &g_bgm[g_current_bgm];
        for (sample = 0; sample < frame_count; sample++) {
            if (g_bgm_playhead >= track->sample_count)
                g_bgm_playhead = track->loop_start;
            buffer[sample * 2] = track->samples[g_bgm_playhead * 2];
            buffer[sample * 2 + 1] =
                track->samples[g_bgm_playhead * 2 + 1];
            g_bgm_playhead++;
        }
    }

    for (voice = 0; voice < MAX_SFX_VOICES; voice++) {
        if (g_sfx_voices[voice].active) {
            AllStarSfxId sfx_id = g_sfx_voices[voice].sfx_id;
            if (sfx_id < ALLSTAR_SFX_COUNT && g_sfx[sfx_id].loaded) {
                PcmSound *sfx = &g_sfx[sfx_id];
                for (sample = 0; sample < frame_count; sample++) {
                    if (g_sfx_voices[voice].playhead < sfx->sample_count) {
                        int32_t left = (int32_t)buffer[sample * 2] +
                            (int32_t)sfx->samples[
                                g_sfx_voices[voice].playhead * 2];
                        int32_t right = (int32_t)buffer[sample * 2 + 1] +
                            (int32_t)sfx->samples[
                                g_sfx_voices[voice].playhead * 2 + 1];
                        if (left > 32767) left = 32767;
                        else if (left < -32768) left = -32768;
                        if (right > 32767) right = 32767;
                        else if (right < -32768) right = -32768;
                        buffer[sample * 2] = (int16_t)left;
                        buffer[sample * 2 + 1] = (int16_t)right;
                        g_sfx_voices[voice].playhead++;
                    } else {
                        g_sfx_voices[voice].active = false;
                        break;
                    }
                }
            } else {
                g_sfx_voices[voice].active = false;
            }
        }
    }
}
#endif

#if defined(_WIN32) && !defined(ALLSTAR_USE_SDL_AUDIO)
static DWORD WINAPI audio_mixer_thread(LPVOID param) {
    (void)param;
    WAVEFORMATEX wfx;
    memset(&wfx, 0, sizeof(wfx));
    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nChannels = MIX_CHANNELS;
    wfx.nSamplesPerSec = MIX_SAMPLE_RATE;
    wfx.wBitsPerSample = 16;
    wfx.nBlockAlign = (MIX_CHANNELS * 16) / 8;
    wfx.nAvgBytesPerSec = MIX_SAMPLE_RATE * wfx.nBlockAlign;

    HWAVEOUT hWave = NULL;
    if (waveOutOpen(&hWave, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) {
        return 0;
    }

    #define NUM_BUFFERS 3
    WAVEHDR headers[NUM_BUFFERS];
    int16_t mix_buffers[NUM_BUFFERS][MIX_BUFFER_SAMPLES * MIX_CHANNELS];

    for (int i = 0; i < NUM_BUFFERS; i++) {
        memset(&headers[i], 0, sizeof(WAVEHDR));
        memset(mix_buffers[i], 0, sizeof(mix_buffers[i]));
        headers[i].lpData = (LPSTR)mix_buffers[i];
        headers[i].dwBufferLength = sizeof(mix_buffers[i]);
        waveOutPrepareHeader(hWave, &headers[i], sizeof(WAVEHDR));
        headers[i].dwFlags |= WHDR_DONE; /* Mark as ready for immediate refill */
    }

    int buf_idx = 0;
    while (g_mixer_running) {
        WAVEHDR *hdr = &headers[buf_idx];
        if (!(hdr->dwFlags & WHDR_DONE)) {
            Sleep(2);
            continue;
        }

        lock_audio();
        mix_audio_frames(mix_buffers[buf_idx], MIX_BUFFER_SAMPLES);
        unlock_audio();

        waveOutWrite(hWave, hdr, sizeof(WAVEHDR));
        buf_idx = (buf_idx + 1) % NUM_BUFFERS;
    }

    waveOutReset(hWave);
    for (int i = 0; i < NUM_BUFFERS; i++) {
        waveOutUnprepareHeader(hWave, &headers[i], sizeof(WAVEHDR));
    }
    waveOutClose(hWave);
    return 0;
}
#elif defined(ALLSTAR_USE_SDL_AUDIO)
static void SDLCALL sdl_audio_callback(void *userdata,
                                       SDL_AudioStream *stream,
                                       int additional_amount,
                                       int total_amount) {
    int16_t buffer[MIX_BUFFER_SAMPLES * MIX_CHANNELS];
    int bytes_remaining = additional_amount;
    (void)userdata;
    (void)total_amount;
    while (bytes_remaining >= (int)(MIX_CHANNELS * sizeof(int16_t))) {
        uint32_t frames = (uint32_t)bytes_remaining /
            (MIX_CHANNELS * sizeof(int16_t));
        int byte_count;
        if (frames > MIX_BUFFER_SAMPLES) frames = MIX_BUFFER_SAMPLES;
        byte_count = (int)(frames * MIX_CHANNELS * sizeof(int16_t));
        lock_audio();
        mix_audio_frames(buffer, frames);
        unlock_audio();
        if (!SDL_PutAudioStreamData(stream, buffer, byte_count)) break;
        bytes_remaining -= byte_count;
    }
}
#endif

void allstar_audio_init(AllStarAudioEngine *audio) {
    if (!audio) return;
    memset(audio, 0, sizeof(AllStarAudioEngine));
    audio->enabled = true;
    audio->volume = 1.0f;
    audio->current_bgm = ALLSTAR_BGM_NONE;

#if ALLSTAR_AUDIO_OUTPUT
#if defined(_WIN32) && !defined(ALLSTAR_USE_SDL_AUDIO)
    if (!g_lock_initialized) {
        InitializeCriticalSection(&g_audio_lock);
        g_lock_initialized = true;
    }
#elif defined(ALLSTAR_USE_SDL_AUDIO)
    if (!g_audio_lock) g_audio_lock = SDL_CreateMutex();
#endif

    /* Pre-cache audio tracks into RAM */
    load_pcm_wav("bgm_title.wav", &g_bgm[ALLSTAR_BGM_TITLE]);
    load_pcm_wav("bgm_menu.wav", &g_bgm[ALLSTAR_BGM_MENU]);
    load_pcm_wav("bgm_gameplay.wav", &g_bgm[ALLSTAR_BGM_GAMEPLAY]);
    load_pcm_wav("sfx_menu_move.wav", &g_sfx[ALLSTAR_SFX_MENU_MOVE]);
    load_pcm_wav("sfx_menu_select.wav", &g_sfx[ALLSTAR_SFX_MENU_SELECT]);
    load_pcm_wav("sfx_shoe_squeak.wav", &g_sfx[ALLSTAR_SFX_SHOE_SQUEAK]);
    load_pcm_wav("sfx_score_chime.wav", &g_sfx[ALLSTAR_SFX_SCORE_CHIME]);
    generate_gameplay_sfx_fallbacks();

    memset(g_sfx_voices, 0, sizeof(g_sfx_voices));
#if defined(_WIN32) && !defined(ALLSTAR_USE_SDL_AUDIO)
    g_mixer_running = true;
#endif
    g_requested_bgm = ALLSTAR_BGM_NONE;
    g_current_bgm = ALLSTAR_BGM_NONE;

#if defined(_WIN32) && !defined(ALLSTAR_USE_SDL_AUDIO)
    if (!g_mixer_thread) {
        g_mixer_thread = CreateThread(NULL, 0, audio_mixer_thread, NULL, 0, NULL);
    }
#elif defined(ALLSTAR_USE_SDL_AUDIO)
    if (g_audio_lock && !g_audio_stream) {
        SDL_AudioSpec spec;
        SDL_zero(spec);
        spec.format = SDL_AUDIO_S16LE;
        spec.channels = MIX_CHANNELS;
        spec.freq = MIX_SAMPLE_RATE;
        g_audio_stream = SDL_OpenAudioDeviceStream(
            SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec,
            sdl_audio_callback, NULL);
        if (g_audio_stream) {
            SDL_AudioDeviceID device =
                SDL_GetAudioStreamDevice(g_audio_stream);
            const char *device_name = SDL_GetAudioDeviceName(device);
            if (!SDL_ResumeAudioStreamDevice(g_audio_stream)) {
                SDL_Log("Could not start audio output: %s", SDL_GetError());
            } else {
                SDL_Log("Audio output started: %s, %d Hz, stereo S16",
                        device_name ? device_name : "default device",
                        MIX_SAMPLE_RATE);
            }
        } else {
            SDL_Log("Audio output disabled: %s", SDL_GetError());
        }
    }
#endif
#endif
}

void allstar_audio_shutdown(AllStarAudioEngine *audio) {
#if ALLSTAR_AUDIO_OUTPUT
    size_t index;
#if defined(_WIN32) && !defined(ALLSTAR_USE_SDL_AUDIO)
    g_mixer_running = false;
    if (g_mixer_thread) {
        WaitForSingleObject(g_mixer_thread, INFINITE);
        CloseHandle(g_mixer_thread);
        g_mixer_thread = NULL;
    }
#elif defined(ALLSTAR_USE_SDL_AUDIO)
    if (g_audio_stream) {
        SDL_PauseAudioStreamDevice(g_audio_stream);
        SDL_DestroyAudioStream(g_audio_stream);
        g_audio_stream = NULL;
    }
#endif
    lock_audio();
    for (index = 0; index < ALLSTAR_BGM_COUNT; index++)
        free_pcm_sound(&g_bgm[index]);
    for (index = 0; index < ALLSTAR_SFX_COUNT; index++)
        free_pcm_sound(&g_sfx[index]);
    memset(g_sfx_voices, 0, sizeof(g_sfx_voices));
    unlock_audio();
#if defined(_WIN32) && !defined(ALLSTAR_USE_SDL_AUDIO)
    if (g_lock_initialized) {
        DeleteCriticalSection(&g_audio_lock);
        g_lock_initialized = false;
    }
#elif defined(ALLSTAR_USE_SDL_AUDIO)
    if (g_audio_lock) {
        SDL_DestroyMutex(g_audio_lock);
        g_audio_lock = NULL;
    }
#endif
#endif
    if (audio) {
        audio->enabled = false;
        audio->current_bgm = ALLSTAR_BGM_NONE;
    }
}

bool allstar_audio_bind_rom_sfx(AllStarAudioEngine *audio,
                                const AllStarAssetPack *pack) {
    const AllStarRomSfxProgram *movement;
    const AllStarRomSfxProgram *score;
    const AllStarRomSfxProgram *dribble;
    const AllStarRomSfxProgram *navigation;
    const AllStarRomSfxProgram *confirm;
    const AllStarRomSfxProgram *rim;
    const AllStarRomSfxProgram *foul;
    const AllStarRomSfxProgram *free_throw_net;
    const AllStarRomSfxProgram *free_throw_contact;
    const AllStarRomSfxProgram *horse_letter;
    const AllStarRomSfxProgram *accuracy_result;
    const AllStarRomMusicProgram *title_music;
    if (!audio || !pack || !pack->is_loaded ||
        (pack->header.feature_flags &
            ALLSTAR_ASSET_FEATURE_GAMEPLAY_AUDIO) == 0 ||
        (pack->header.feature_flags &
            ALLSTAR_ASSET_FEATURE_ROM_MUSIC) == 0 ||
        pack->header.rom_sfx_program_count !=
            ALLSTAR_ROM_SFX_PROGRAM_COUNT ||
        pack->header.rom_music_program_count !=
            ALLSTAR_ROM_MUSIC_PROGRAM_COUNT) return false;
    movement = &pack->rom_sfx_programs[0];
    score = &pack->rom_sfx_programs[1];
    dribble = &pack->rom_sfx_programs[2];
    navigation = &pack->rom_sfx_programs[3];
    confirm = &pack->rom_sfx_programs[4];
    rim = &pack->rom_sfx_programs[5];
    foul = &pack->rom_sfx_programs[6];
    free_throw_net = &pack->rom_sfx_programs[7];
    free_throw_contact = &pack->rom_sfx_programs[8];
    horse_letter = &pack->rom_sfx_programs[9];
    accuracy_result = &pack->rom_sfx_programs[10];
    title_music = &pack->rom_music_programs[0];
    if (movement->command != 0x0d || movement->program_id != 0x11 ||
        movement->priority_frames != 0x14 || movement->frame_count != 3 ||
        movement->stream_pointer_1 != 0x3fa2 ||
        score->command != 0x05 || score->program_id != 0x0c ||
        score->priority_frames != 0x64 || score->frame_count != 72 ||
        score->stream_pointer_1 != 0x3ef6 ||
        score->stream_pointer_2 != 0x3f00 ||
        dribble->command != 0x0c || dribble->program_id != 0x02 ||
        dribble->priority_frames != 0x13 || dribble->frame_count != 6 ||
        dribble->stream_pointer_1 != 0x3d7f ||
        navigation->command != 0x0f || navigation->program_id != 0x07 ||
        navigation->priority_frames != 0x19 ||
        navigation->frame_count != 24 ||
        navigation->stream_pointer_1 != 0x3ebc ||
        confirm->command != 0x0e || confirm->program_id != 0x12 ||
        confirm->priority_frames != 0x32 || confirm->frame_count != 48 ||
        confirm->stream_pointer_1 != 0x3fa6 ||
        rim->command != 0x09 || rim->program_id != 0x0b ||
        rim->priority_frames != 0x23 || rim->frame_count != 24 ||
        rim->stream_pointer_1 != 0x3ef2 ||
        rim->noise_length != 0xeb || rim->noise_envelope != 0xf2 ||
        rim->noise_control != 0xbf ||
        rim->frames[0].noise_polynomial != 0x5a ||
        foul->command != 0x04 || foul->program_id != 0x0a ||
        foul->priority_frames != 0x1e || foul->frame_count != 30 ||
        foul->stream_pointer_1 != 0x3ed4 ||
        foul->stream_pointer_2 != 0x3ee0 ||
        foul->square1_sweep != 0x08 ||
        foul->square1_duty_length != 0x08 ||
        foul->square1_envelope != 0xa2 ||
        foul->square2_duty_length != 0x48 ||
        foul->square2_envelope != 0xa2 ||
        free_throw_net->command != 0x08 ||
        free_throw_net->program_id != 0x05 ||
        free_throw_net->priority_frames != 0x23 ||
        free_throw_net->frame_count != 57 ||
        free_throw_net->stream_pointer_1 != 0x3eac ||
        free_throw_contact->command != 0x0a ||
        free_throw_contact->program_id != 0x0d ||
        free_throw_contact->priority_frames != 0x1c ||
        free_throw_contact->frame_count != 12 ||
        free_throw_contact->stream_pointer_1 != 0x3f0a ||
        horse_letter->command != 0x07 ||
        horse_letter->program_id != 0x06 ||
        horse_letter->priority_frames != 0x2a ||
        horse_letter->frame_count != 42 ||
        horse_letter->stream_pointer_1 != 0x3eb6 ||
        accuracy_result->command != 0x02 ||
        accuracy_result->program_id != 0x08 ||
        accuracy_result->priority_frames != 0xaa ||
        accuracy_result->frame_count != 144 ||
        accuracy_result->stream_pointer_1 != 0x3ec0 ||
        accuracy_result->stream_pointer_2 != 0x3ec4 ||
        movement->source_checksum == 0 ||
        movement->source_checksum != score->source_checksum ||
        movement->source_checksum != dribble->source_checksum ||
        movement->source_checksum != navigation->source_checksum ||
        movement->source_checksum != confirm->source_checksum ||
        movement->source_checksum != rim->source_checksum ||
        movement->source_checksum != foul->source_checksum ||
        movement->source_checksum != free_throw_net->source_checksum ||
        movement->source_checksum != free_throw_contact->source_checksum ||
        movement->source_checksum != horse_letter->source_checksum ||
        movement->source_checksum != accuracy_result->source_checksum ||
        title_music->song_id != 1 || title_music->update_skip != 7 ||
        title_music->frame_count != 3360 ||
        title_music->loop_frame != 1568 ||
        title_music->program_pointer != 0x3b25 ||
        title_music->offset_pointer != 0x3aab ||
        title_music->source_checksum != 0x7ae8b9d0u ||
        title_music->frames[0].square1_frequency != 0x069e ||
        title_music->frames[0].square2_frequency != 0x0627 ||
        title_music->frames[0].wave_frequency != 0x053b ||
        title_music->frames[0].flags != 0xff)
        return false;
#if ALLSTAR_AUDIO_OUTPUT
    {
        PcmSound movement_pcm = {0};
        PcmSound score_pcm = {0};
        PcmSound dribble_pcm = {0};
        PcmSound navigation_pcm = {0};
        PcmSound confirm_pcm = {0};
        PcmSound rim_pcm = {0};
        PcmSound foul_pcm = {0};
        PcmSound free_throw_net_pcm = {0};
        PcmSound free_throw_contact_pcm = {0};
        PcmSound horse_letter_pcm = {0};
        PcmSound accuracy_result_pcm = {0};
        PcmSound title_music_pcm = {0};
        if (!generate_rom_program(&movement_pcm, movement) ||
            !generate_rom_program(&score_pcm, score) ||
            !generate_rom_program(&dribble_pcm, dribble) ||
            !generate_rom_program(&navigation_pcm, navigation) ||
            !generate_rom_program(&confirm_pcm, confirm) ||
            !generate_rom_program(&rim_pcm, rim) ||
            !generate_rom_program(&foul_pcm, foul) ||
            !generate_rom_program(&free_throw_net_pcm, free_throw_net) ||
            !generate_rom_program(&free_throw_contact_pcm,
                                  free_throw_contact) ||
            !generate_rom_program(&horse_letter_pcm, horse_letter) ||
            !generate_rom_program(&accuracy_result_pcm, accuracy_result) ||
            !generate_rom_music(&title_music_pcm, title_music)) {
            free_pcm_sound(&movement_pcm);
            free_pcm_sound(&score_pcm);
            free_pcm_sound(&dribble_pcm);
            free_pcm_sound(&navigation_pcm);
            free_pcm_sound(&confirm_pcm);
            free_pcm_sound(&rim_pcm);
            free_pcm_sound(&foul_pcm);
            free_pcm_sound(&free_throw_net_pcm);
            free_pcm_sound(&free_throw_contact_pcm);
            free_pcm_sound(&horse_letter_pcm);
            free_pcm_sound(&accuracy_result_pcm);
            free_pcm_sound(&title_music_pcm);
            return false;
        }
        lock_audio();
        free_pcm_sound(&g_bgm[ALLSTAR_BGM_TITLE]);
        free_pcm_sound(&g_sfx[ALLSTAR_SFX_SHOE_SQUEAK]);
        free_pcm_sound(&g_sfx[ALLSTAR_SFX_SCORE_CHIME]);
        free_pcm_sound(&g_sfx[ALLSTAR_SFX_DRIBBLE]);
        free_pcm_sound(&g_sfx[ALLSTAR_SFX_MENU_MOVE]);
        free_pcm_sound(&g_sfx[ALLSTAR_SFX_MENU_SELECT]);
        free_pcm_sound(&g_sfx[ALLSTAR_SFX_RIM_CLANK]);
        free_pcm_sound(&g_sfx[ALLSTAR_SFX_WHISTLE]);
        free_pcm_sound(&g_sfx[ALLSTAR_SFX_FREE_THROW_NET]);
        free_pcm_sound(&g_sfx[ALLSTAR_SFX_FREE_THROW_CONTACT]);
        free_pcm_sound(&g_sfx[ALLSTAR_SFX_HORSE_LETTER]);
        free_pcm_sound(&g_sfx[ALLSTAR_SFX_ACCURACY_RESULT]);
        g_bgm[ALLSTAR_BGM_TITLE] = title_music_pcm;
        g_sfx[ALLSTAR_SFX_SHOE_SQUEAK] = movement_pcm;
        g_sfx[ALLSTAR_SFX_SCORE_CHIME] = score_pcm;
        g_sfx[ALLSTAR_SFX_DRIBBLE] = dribble_pcm;
        g_sfx[ALLSTAR_SFX_MENU_MOVE] = navigation_pcm;
        g_sfx[ALLSTAR_SFX_MENU_SELECT] = confirm_pcm;
        g_sfx[ALLSTAR_SFX_RIM_CLANK] = rim_pcm;
        g_sfx[ALLSTAR_SFX_WHISTLE] = foul_pcm;
        g_sfx[ALLSTAR_SFX_FREE_THROW_NET] = free_throw_net_pcm;
        g_sfx[ALLSTAR_SFX_FREE_THROW_CONTACT] = free_throw_contact_pcm;
        g_sfx[ALLSTAR_SFX_HORSE_LETTER] = horse_letter_pcm;
        g_sfx[ALLSTAR_SFX_ACCURACY_RESULT] = accuracy_result_pcm;
        unlock_audio();
    }
#endif
    audio->rom_sfx_bound = true;
    audio->rom_sfx_source_checksum = movement->source_checksum;
    return true;
}

/*
 * The loudest sample the rendered cue reaches between two times.  This renders
 * through exactly the path the game plays, so a renderer that stops applying
 * the envelope shows up here rather than only in a helper's return value.
 */
bool allstar_audio_rom_sfx_peak(const AllStarAssetPack *pack, uint8_t command,
                                double start_seconds, double end_seconds,
                                int *peak) {
    const AllStarRomSfxProgram *program = NULL;
    PcmSound sound = {0};
    size_t i;
    uint32_t first;
    uint32_t last;
    int loudest = 0;
    if (!pack || !peak || end_seconds <= start_seconds) return false;
    if (pack->header.rom_sfx_program_count !=
            ALLSTAR_ROM_SFX_PROGRAM_COUNT) return false;
    for (i = 0; i < pack->header.rom_sfx_program_count; i++) {
        if (pack->rom_sfx_programs[i].command == command) {
            program = &pack->rom_sfx_programs[i];
            break;
        }
    }
    if (!program || !generate_rom_program(&sound, program)) return false;
    first = (uint32_t)(start_seconds * MIX_SAMPLE_RATE);
    last = (uint32_t)(end_seconds * MIX_SAMPLE_RATE);
    if (last > sound.sample_count) last = sound.sample_count;
    for (i = first; i < last; i++) {
        int value = sound.samples[i * MIX_CHANNELS];
        if (value < 0) value = -value;
        if (value > loudest) loudest = value;
    }
    *peak = loudest;
    free_pcm_sound(&sound);
    return true;
}

bool allstar_audio_export_rom_sfx_wav(const AllStarAssetPack *pack,
                                      uint8_t command,
                                      const char *filepath) {
    const AllStarRomSfxProgram *program = NULL;
    PcmSound sound = {0};
    FILE *file;
    uint32_t data_size;
    uint32_t riff_size;
    uint32_t byte_rate = MIX_SAMPLE_RATE * MIX_CHANNELS * sizeof(int16_t);
    uint16_t format = 1;
    uint16_t channels = MIX_CHANNELS;
    uint16_t block_align = MIX_CHANNELS * sizeof(int16_t);
    uint16_t bits = 16;
    size_t i;
    if (!pack || !filepath ||
        pack->header.rom_sfx_program_count !=
            ALLSTAR_ROM_SFX_PROGRAM_COUNT) return false;
    for (i = 0; i < pack->header.rom_sfx_program_count; i++) {
        if (pack->rom_sfx_programs[i].command == command) {
            program = &pack->rom_sfx_programs[i];
            break;
        }
    }
    if (!program || !generate_rom_program(&sound, program)) return false;
    file = open_binary_file(filepath, "wb");
    if (!file) {
        free_pcm_sound(&sound);
        return false;
    }
    data_size = (uint32_t)(sound.sample_count * MIX_CHANNELS *
                           sizeof(int16_t));
    riff_size = 36 + data_size;
    fwrite("RIFF", 1, 4, file);
    fwrite(&riff_size, sizeof(riff_size), 1, file);
    fwrite("WAVEfmt ", 1, 8, file);
    {
        uint32_t fmt_size = 16;
        fwrite(&fmt_size, sizeof(fmt_size), 1, file);
    }
    fwrite(&format, sizeof(format), 1, file);
    fwrite(&channels, sizeof(channels), 1, file);
    {
        uint32_t sample_rate = MIX_SAMPLE_RATE;
        fwrite(&sample_rate, sizeof(sample_rate), 1, file);
    }
    fwrite(&byte_rate, sizeof(byte_rate), 1, file);
    fwrite(&block_align, sizeof(block_align), 1, file);
    fwrite(&bits, sizeof(bits), 1, file);
    fwrite("data", 1, 4, file);
    fwrite(&data_size, sizeof(data_size), 1, file);
    fwrite(sound.samples, 1, data_size, file);
    fclose(file);
    free_pcm_sound(&sound);
    return true;
}

/*
 * The title song rendered straight out of the decoded program, so the
 * cartridge's stereo image can be listened to and measured.  $35B6 puts the
 * two square voices on opposite sides, which is why this is not mono.
 */
bool allstar_audio_export_rom_music_wav(const AllStarAssetPack *pack,
                                        const char *filepath) {
    PcmSound sound = {0};
    FILE *file;
    uint32_t data_size;
    uint32_t riff_size;
    uint32_t byte_rate = MIX_SAMPLE_RATE * MIX_CHANNELS * sizeof(int16_t);
    uint16_t format = 1;
    uint16_t channels = MIX_CHANNELS;
    uint16_t block_align = MIX_CHANNELS * sizeof(int16_t);
    uint16_t bits = 16;
    if (!pack || !filepath ||
        pack->header.rom_music_program_count !=
            ALLSTAR_ROM_MUSIC_PROGRAM_COUNT) return false;
    if (!generate_rom_music(&sound, &pack->rom_music_programs[0]))
        return false;
    file = open_binary_file(filepath, "wb");
    if (!file) {
        free_pcm_sound(&sound);
        return false;
    }
    data_size = (uint32_t)(sound.sample_count * MIX_CHANNELS *
                           sizeof(int16_t));
    riff_size = 36 + data_size;
    fwrite("RIFF", 1, 4, file);
    fwrite(&riff_size, sizeof(riff_size), 1, file);
    fwrite("WAVEfmt ", 1, 8, file);
    {
        uint32_t fmt_size = 16;
        fwrite(&fmt_size, sizeof(fmt_size), 1, file);
    }
    fwrite(&format, sizeof(format), 1, file);
    fwrite(&channels, sizeof(channels), 1, file);
    {
        uint32_t sample_rate = MIX_SAMPLE_RATE;
        fwrite(&sample_rate, sizeof(sample_rate), 1, file);
    }
    fwrite(&byte_rate, sizeof(byte_rate), 1, file);
    fwrite(&block_align, sizeof(block_align), 1, file);
    fwrite(&bits, sizeof(bits), 1, file);
    fwrite("data", 1, 4, file);
    fwrite(&data_size, sizeof(data_size), 1, file);
    fwrite(sound.samples, 1, data_size, file);
    fclose(file);
    free_pcm_sound(&sound);
    return true;
}

void allstar_audio_play_sfx(AllStarAudioEngine *audio, AllStarSfxId sfx) {
    if (!audio || !audio->enabled || sfx <= ALLSTAR_SFX_NONE ||
        sfx >= ALLSTAR_SFX_COUNT) return;
    audio->last_sfx = sfx;
    audio->sfx_play_count++;
#if ALLSTAR_AUDIO_OUTPUT
    lock_audio();
    /* The DMG command driver owns one voice per effect. Re-selecting command
       $0C on each record-6 update restarts that voice instead of stacking six
       simultaneous copies in the native PCM mixer. */
    int slot = -1;
    for (int i = 0; i < MAX_SFX_VOICES; i++) {
        if (g_sfx_voices[i].active && g_sfx_voices[i].sfx_id == sfx) {
            slot = i;
            break;
        }
    }
    for (int i = 0; slot == -1 && i < MAX_SFX_VOICES; i++) {
        if (!g_sfx_voices[i].active) {
            slot = i;
            break;
        }
    }
    if (slot == -1) slot = 0; /* Steal voice 0 */

    g_sfx_voices[slot].sfx_id = sfx;
    g_sfx_voices[slot].playhead = 0;
    g_sfx_voices[slot].active = true;
    unlock_audio();
#endif
}

/* Native fallback utility; no verified ROM-routine mapping. */
void allstar_audio_generate_tone(int channel, float frequency_hz, float duration_sec) {
    (void)channel;
    (void)frequency_hz;
    (void)duration_sec;
}

/* Native PCM BGM control; reset/vector address $0078 is not this routine. */
void allstar_audio_play_bgm(AllStarAudioEngine *audio, AllStarBgmId bgm) {
    if (!audio || !audio->enabled) return;
    if (audio->current_bgm == bgm) return;
    audio->current_bgm = bgm;

#if ALLSTAR_AUDIO_OUTPUT
    lock_audio();
    g_requested_bgm = bgm;
    unlock_audio();
#endif
}

/* Native PCM BGM control; serial address $007B is not this routine. */
void allstar_audio_stop_bgm(AllStarAudioEngine *audio) {
    if (!audio) return;
    audio->current_bgm = ALLSTAR_BGM_NONE;
#if ALLSTAR_AUDIO_OUTPUT
    lock_audio();
    g_requested_bgm = ALLSTAR_BGM_NONE;
    unlock_audio();
#endif
}

/* Native PCM update hook; $0002 is reset-padding, not an audio routine. */
void allstar_audio_update(AllStarAudioEngine *audio, float dt) {
    if (!audio || !audio->enabled) return;
    (void)dt;
}
