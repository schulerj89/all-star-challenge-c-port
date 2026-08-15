#include "allstar_audio.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>

#define MIX_BUFFER_SAMPLES 2048
#define MIX_CHANNELS 2
#define MIX_SAMPLE_RATE 48000

typedef struct {
    int16_t *samples;
    uint32_t sample_count; /* in 16-bit stereo frame units */
    bool loaded;
} PcmSound;

static PcmSound g_bgm[ALLSTAR_BGM_COUNT];
static PcmSound g_sfx[ALLSTAR_SFX_COUNT];

static HANDLE g_mixer_thread = NULL;
static volatile bool g_mixer_running = false;
static volatile AllStarBgmId g_requested_bgm = ALLSTAR_BGM_NONE;
static volatile AllStarBgmId g_current_bgm = ALLSTAR_BGM_NONE;
static uint32_t g_bgm_playhead = 0;

/* Active SFX voice slots */
#define MAX_SFX_VOICES 4
typedef struct {
    AllStarSfxId sfx_id;
    uint32_t playhead;
    bool active;
} SfxVoice;

static SfxVoice g_sfx_voices[MAX_SFX_VOICES];
static CRITICAL_SECTION g_audio_lock;
static bool g_lock_initialized = false;

static bool load_pcm_wav(const char *filename, PcmSound *out) {
    if (!out) return false;
    memset(out, 0, sizeof(PcmSound));

    char path[MAX_PATH];
    snprintf(path, sizeof(path), "assets\\audio\\%s", filename);
    FILE *f = NULL;
    fopen_s(&f, path, "rb");
    if (!f) {
        snprintf(path, sizeof(path), "build\\assets\\audio\\%s", filename);
        fopen_s(&f, path, "rb");
    }
    if (!f) {
        char exe_dir[MAX_PATH];
        GetModuleFileNameA(NULL, exe_dir, MAX_PATH);
        char *last_slash = strrchr(exe_dir, '\\');
        if (last_slash) *last_slash = '\0';
        snprintf(path, sizeof(path), "%s\\assets\\audio\\%s", exe_dir, filename);
        fopen_s(&f, path, "rb");
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
        uint32_t chunk_size = *(uint32_t*)(chunk_hdr + 4);
        if (memcmp(chunk_hdr, "fmt ", 4) == 0) {
            uint8_t fmt_data[16];
            if (fread(fmt_data, 1, 16, f) == 16) {
                channels = *(uint16_t*)(fmt_data + 2);
                sample_rate = *(uint32_t*)(fmt_data + 4);
                bits_per_sample = *(uint16_t*)(fmt_data + 14);
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

    if (!raw_buf || data_size == 0) {
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
static bool generate_rom_square_program(PcmSound *out,
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
    float duty1;
    float duty2;
    float volume1;
    float volume2;
    if (!out || !program || program->frame_count == 0 ||
        program->frame_count > ALLSTAR_ROM_SFX_MAX_FRAMES) return false;
    total_samples = (uint32_t)ceil(
        program->frame_count * frame_seconds * MIX_SAMPLE_RATE);
    out->samples = (int16_t*)malloc(
        (size_t)total_samples * MIX_CHANNELS * sizeof(int16_t));
    if (!out->samples) return false;
    duty1 = dmg_square_duty(program->square1_duty_length);
    duty2 = dmg_square_duty(program->square2_duty_length);
    volume1 = (float)(program->square1_envelope >> 4) / 15.0f;
    volume2 = (float)(program->square2_envelope >> 4) / 15.0f;

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
                    frequency1 = frame->square1_frequency;
                    sweep_shadow = frequency1;
                    phase1 = 0.0f;
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
                if ((frame->flags & ALLSTAR_ROM_SFX_TRIGGER_2) != 0)
                    phase2 = 0.0f;
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

        if ((frame->flags & ALLSTAR_ROM_SFX_CHANNEL_1) != 0) {
            mixed += (phase1 < duty1 ? 1.0f : -1.0f) * volume1;
            phase1 += dmg_square_hz(frequency1) / MIX_SAMPLE_RATE;
            if (phase1 >= 1.0f) phase1 -= floorf(phase1);
        }
        if ((frame->flags & ALLSTAR_ROM_SFX_CHANNEL_2) != 0) {
            mixed += (phase2 < duty2 ? 1.0f : -1.0f) * volume2;
            phase2 += dmg_square_hz(frequency2) / MIX_SAMPLE_RATE;
            if (phase2 >= 1.0f) phase2 -= floorf(phase2);
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

static void free_pcm_sound(PcmSound *sound) {
    if (!sound) return;
    free(sound->samples);
    memset(sound, 0, sizeof(*sound));
}

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
}

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

        EnterCriticalSection(&g_audio_lock);

        /* Handle BGM track switch */
        AllStarBgmId req = g_requested_bgm;
        if (req != g_current_bgm) {
            g_current_bgm = req;
            g_bgm_playhead = 0;
        }

        int16_t *buf = mix_buffers[buf_idx];
        memset(buf, 0, sizeof(mix_buffers[buf_idx]));

        /* Mix BGM stream */
        if (g_current_bgm != ALLSTAR_BGM_NONE && g_bgm[g_current_bgm].loaded) {
            PcmSound *track = &g_bgm[g_current_bgm];
            for (uint32_t s = 0; s < MIX_BUFFER_SAMPLES; s++) {
                if (g_bgm_playhead >= track->sample_count) {
                    g_bgm_playhead = 0; /* Seamless infinite loop */
                }
                buf[s * 2 + 0] = track->samples[g_bgm_playhead * 2 + 0];
                buf[s * 2 + 1] = track->samples[g_bgm_playhead * 2 + 1];
                g_bgm_playhead++;
            }
        }

        /* Mix active SFX voices */
        for (int v = 0; v < MAX_SFX_VOICES; v++) {
            if (g_sfx_voices[v].active) {
                AllStarSfxId sfx_id = g_sfx_voices[v].sfx_id;
                if (sfx_id < ALLSTAR_SFX_COUNT && g_sfx[sfx_id].loaded) {
                    PcmSound *sfx = &g_sfx[sfx_id];
                    for (uint32_t s = 0; s < MIX_BUFFER_SAMPLES; s++) {
                        if (g_sfx_voices[v].playhead < sfx->sample_count) {
                            int32_t left = (int32_t)buf[s * 2 + 0] + (int32_t)sfx->samples[g_sfx_voices[v].playhead * 2 + 0];
                            int32_t right = (int32_t)buf[s * 2 + 1] + (int32_t)sfx->samples[g_sfx_voices[v].playhead * 2 + 1];

                            /* Saturation clip */
                            if (left > 32767) left = 32767;
                            else if (left < -32768) left = -32768;
                            if (right > 32767) right = 32767;
                            else if (right < -32768) right = -32768;

                            buf[s * 2 + 0] = (int16_t)left;
                            buf[s * 2 + 1] = (int16_t)right;
                            g_sfx_voices[v].playhead++;
                        } else {
                            g_sfx_voices[v].active = false;
                            break;
                        }
                    }
                } else {
                    g_sfx_voices[v].active = false;
                }
            }
        }

        LeaveCriticalSection(&g_audio_lock);

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
#endif

void allstar_audio_init(AllStarAudioEngine *audio) {
    if (!audio) return;
    memset(audio, 0, sizeof(AllStarAudioEngine));
    audio->enabled = true;
    audio->volume = 1.0f;
    audio->current_bgm = ALLSTAR_BGM_NONE;

#ifdef _WIN32
    if (!g_lock_initialized) {
        InitializeCriticalSection(&g_audio_lock);
        g_lock_initialized = true;
    }

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
    g_mixer_running = true;
    g_requested_bgm = ALLSTAR_BGM_NONE;
    g_current_bgm = ALLSTAR_BGM_NONE;

    if (!g_mixer_thread) {
        g_mixer_thread = CreateThread(NULL, 0, audio_mixer_thread, NULL, 0, NULL);
    }
#endif
}

bool allstar_audio_bind_rom_sfx(AllStarAudioEngine *audio,
                                const AllStarAssetPack *pack) {
    const AllStarRomSfxProgram *movement;
    const AllStarRomSfxProgram *score;
    if (!audio || !pack || !pack->is_loaded ||
        (pack->header.feature_flags &
            ALLSTAR_ASSET_FEATURE_ONE_ON_ONE_AUDIO) == 0 ||
        pack->header.rom_sfx_program_count !=
            ALLSTAR_ROM_SFX_PROGRAM_COUNT) return false;
    movement = &pack->rom_sfx_programs[0];
    score = &pack->rom_sfx_programs[1];
    if (movement->command != 0x0d || movement->program_id != 0x11 ||
        movement->priority_frames != 0x14 || movement->frame_count != 3 ||
        movement->stream_pointer_1 != 0x3fa2 ||
        score->command != 0x05 || score->program_id != 0x0c ||
        score->priority_frames != 0x64 || score->frame_count != 72 ||
        score->stream_pointer_1 != 0x3ef6 ||
        score->stream_pointer_2 != 0x3f00 ||
        movement->source_checksum == 0 ||
        movement->source_checksum != score->source_checksum) return false;
#ifdef _WIN32
    {
        PcmSound movement_pcm = {0};
        PcmSound score_pcm = {0};
        if (!generate_rom_square_program(&movement_pcm, movement) ||
            !generate_rom_square_program(&score_pcm, score)) {
            free_pcm_sound(&movement_pcm);
            free_pcm_sound(&score_pcm);
            return false;
        }
        EnterCriticalSection(&g_audio_lock);
        free_pcm_sound(&g_sfx[ALLSTAR_SFX_SHOE_SQUEAK]);
        free_pcm_sound(&g_sfx[ALLSTAR_SFX_SCORE_CHIME]);
        g_sfx[ALLSTAR_SFX_SHOE_SQUEAK] = movement_pcm;
        g_sfx[ALLSTAR_SFX_SCORE_CHIME] = score_pcm;
        LeaveCriticalSection(&g_audio_lock);
    }
#endif
    audio->rom_sfx_bound = true;
    audio->rom_sfx_source_checksum = movement->source_checksum;
    return true;
}

bool allstar_audio_export_rom_sfx_wav(const AllStarAssetPack *pack,
                                      uint8_t command,
                                      const char *filepath) {
#ifdef _WIN32
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
    if (!program || !generate_rom_square_program(&sound, program)) return false;
    file = NULL;
    fopen_s(&file, filepath, "wb");
    if (!file) {
        free_pcm_sound(&sound);
        return false;
    }
    data_size = sound.sample_count * MIX_CHANNELS * sizeof(int16_t);
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
#else
    (void)pack;
    (void)command;
    (void)filepath;
    return false;
#endif
}

void allstar_audio_play_sfx(AllStarAudioEngine *audio, AllStarSfxId sfx) {
    if (!audio || !audio->enabled || sfx <= ALLSTAR_SFX_NONE ||
        sfx >= ALLSTAR_SFX_COUNT) return;
    audio->last_sfx = sfx;
    audio->sfx_play_count++;
#ifdef _WIN32
    if (!g_lock_initialized) return;
    EnterCriticalSection(&g_audio_lock);
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
    LeaveCriticalSection(&g_audio_lock);
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

#ifdef _WIN32
    g_requested_bgm = bgm;
#endif
}

/* Native PCM BGM control; serial address $007B is not this routine. */
void allstar_audio_stop_bgm(AllStarAudioEngine *audio) {
    if (!audio) return;
    audio->current_bgm = ALLSTAR_BGM_NONE;
#ifdef _WIN32
    g_requested_bgm = ALLSTAR_BGM_NONE;
#endif
}

/* Native PCM update hook; $0002 is reset-padding, not an audio routine. */
void allstar_audio_update(AllStarAudioEngine *audio, float dt) {
    if (!audio || !audio->enabled) return;
    (void)dt;
}
