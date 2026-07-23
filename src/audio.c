#include "audio.h"

#include <math.h>

#define AUDIO_SAMPLE_RATE 44100
#define AUDIO_MASTER_GAIN 0.69f
#define INTRO_MUSIC_BARS 12
#define LEVEL_MUSIC_BARS 16
#define CHUCK_PI 3.14159265358979323846f

typedef enum
{
    WAVE_SINE,
    WAVE_SQUARE,
    WAVE_TRIANGLE,
    WAVE_SAW
} Waveform;

static float clampf(float value, float low, float high)
{
    if (value < low)
        return low;
    if (value > high)
        return high;
    return value;
}

static float oscillator(Waveform wave, float phase)
{
    float angle = phase * 2.0f * CHUCK_PI;
    float sine = sinf(angle);
    switch (wave)
    {
    case WAVE_SQUARE:
        /*
         * A literal square wave has an infinite series of hard harmonics and
         * aliases badly. These few harmonics retain the retro colour without
         * the brittle, fatiguing top end.
         */
        return 0.78f * sine +
               0.17f * sinf(angle * 3.0f) +
               0.05f * sinf(angle * 5.0f);
    case WAVE_TRIANGLE:
        return (2.0f / CHUCK_PI) * asinf(sine);
    case WAVE_SAW:
        /* Rounded saw: enough buzz for motors and impacts, without a hard edge. */
        return 0.72f * sine +
               0.18f * sinf(angle * 2.0f) +
               0.07f * sinf(angle * 3.0f) +
               0.03f * sinf(angle * 4.0f);
    case WAVE_SINE:
    default:
        return sine;
    }
}

static float envelope(float t, float duration, float attack, float release)
{
    float value = 1.0f;
    if (attack > 0.0f && t < attack)
    {
        float x = clampf(t / attack, 0.0f, 1.0f);
        value = x * x * (3.0f - 2.0f * x);
    }
    if (release > 0.0f && t > duration - release)
    {
        float x = clampf((duration - t) / release, 0.0f, 1.0f);
        float fade = x * x * (3.0f - 2.0f * x);
        value = fminf(value, fade);
    }
    return clampf(value, 0.0f, 1.0f);
}

static void add_tone(CachedSound *sound, float start, float duration,
                     float from_hz, float to_hz, float gain,
                     Waveform wave, float attack, float release)
{
    int first = (int)(start * AUDIO_SAMPLE_RATE);
    int count = (int)(duration * AUDIO_SAMPLE_RATE);
    int last = first + count;
    if (first < 0)
        first = 0;
    if (last > sound->frame_count)
        last = sound->frame_count;
    if (count <= 0)
        return;

    /* Keep a click-free edge without taking the snap out of short effects. */
    attack = fmaxf(attack, 0.002f);
    for (int i = first; i < last; ++i)
    {
        float t = (float)(i - first) / AUDIO_SAMPLE_RATE;
        float progress = t / duration;
        float phase = from_hz * t +
                      0.5f * (to_hz - from_hz) * t * progress;
        sound->samples[i] += oscillator(wave, phase) *
                             envelope(t, duration, attack, release) * gain;
    }
}

static Uint32 noise_next(Uint32 *state)
{
    *state = *state * 1664525u + 1013904223u;
    return *state;
}

static void add_noise(CachedSound *sound, float start, float duration,
                      float gain, float roughness, float attack, float release,
                      Uint32 seed)
{
    int first = (int)(start * AUDIO_SAMPLE_RATE);
    int count = (int)(duration * AUDIO_SAMPLE_RATE);
    int last = first + count;
    float filtered = 0.0f;
    if (first < 0)
        first = 0;
    if (last > sound->frame_count)
        last = sound->frame_count;

    /* Noise still needs a short fade, but should retain a readable transient. */
    attack = fmaxf(attack, 0.003f);
    roughness = clampf(roughness, 0.001f, 1.0f);
    for (int i = first; i < last; ++i)
    {
        float t = (float)(i - first) / AUDIO_SAMPLE_RATE;
        float white = ((float)(noise_next(&seed) >> 8) / 8388607.5f) - 1.0f;
        filtered += (white - filtered) * roughness;
        sound->samples[i] += filtered *
                             envelope(t, duration, attack, release) * gain;
    }
}

static bool begin_sound(AudioSystem *audio, SoundEffect effect,
                        float duration, float gain, Uint32 min_gap_ms)
{
    CachedSound *sound = &audio->sounds[effect];
    sound->frame_count = (int)(duration * AUDIO_SAMPLE_RATE);
    sound->gain = gain;
    sound->min_gap_ms = min_gap_ms;
    sound->samples = (float *)SDL_calloc((size_t)sound->frame_count, sizeof(float));
    return sound->samples != NULL;
}

static void finish_sound(CachedSound *sound)
{
    /*
     * One light low-pass removes digital fizz while leaving presence intact.
     * The earlier double filter made several effects sound veiled.
     */
    const float cutoff_hz = 9600.0f;
    const float coefficient =
        1.0f - expf(-2.0f * CHUCK_PI * cutoff_hz / AUDIO_SAMPLE_RATE);
    float low = 0.0f;
    float previous_input = 0.0f;
    float dc_blocked = 0.0f;
    float peak = 0.0f;

    for (int i = 0; i < sound->frame_count; ++i)
    {
        float input = sound->samples[i];
        low += (input - low) * coefficient;

        /* Remove subsonic/DC energy before it steals headroom. */
        float value = low - previous_input + 0.997f * dc_blocked;
        previous_input = low;
        dc_blocked = value;

        /* Soft saturation rounds layered transients instead of clipping them. */
        value /= 1.0f + fabsf(value) * 0.34f;
        sound->samples[i] = value;
        peak = fmaxf(peak, fabsf(value));
    }

    /* All cached effects keep the same comfortable peak ceiling. */
    float scale = peak > 0.84f ? 0.84f / peak : 1.0f;
    int tail_frames = (int)(0.002f * AUDIO_SAMPLE_RATE);
    for (int i = 0; i < sound->frame_count; ++i)
    {
        float tail = 1.0f;
        if (i >= sound->frame_count - tail_frames)
            tail = (float)(sound->frame_count - 1 - i) / tail_frames;
        sound->samples[i] = clampf(sound->samples[i] * scale * tail,
                                   -0.84f, 0.84f);
    }
}

static float midi_hz(int midi_note)
{
    return 440.0f * powf(2.0f, ((float)midi_note - 69.0f) / 12.0f);
}

static void add_music_note(CachedSound *track, float start, float duration,
                           int midi_note, float gain, Waveform wave)
{
    float release = fminf(0.11f, duration * 0.42f);
    add_tone(track, start, duration, midi_hz(midi_note), midi_hz(midi_note),
             gain, wave, 0.006f, release);
}

static void add_music_kick(CachedSound *track, float start, float gain,
                           Uint32 seed)
{
    add_tone(track, start, 0.17f, 105.0f, 43.0f, gain,
             WAVE_SINE, 0.003f, 0.14f);
    add_noise(track, start, 0.025f, gain * 0.22f, 0.82f,
              0.002f, 0.020f, seed);
}

static void add_music_snare(CachedSound *track, float start, float gain,
                            Uint32 seed)
{
    add_noise(track, start, 0.13f, gain, 0.57f,
              0.003f, 0.11f, seed);
    add_tone(track, start, 0.09f, 176.0f, 104.0f, gain * 0.36f,
             WAVE_TRIANGLE, 0.003f, 0.075f);
}

static void add_music_hat(CachedSound *track, float start, float gain,
                          Uint32 seed)
{
    add_noise(track, start, 0.047f, gain, 0.92f,
              0.002f, 0.040f, seed);
}

static void finish_music(CachedSound *track)
{
    /*
     * The music deliberately has less high-frequency energy than the effects:
     * footsteps, weapon transients and card tones remain readable over it.
     */
    const float cutoff_hz = 5600.0f;
    const float coefficient =
        1.0f - expf(-2.0f * CHUCK_PI * cutoff_hz / AUDIO_SAMPLE_RATE);
    float low = 0.0f;
    float previous_input = 0.0f;
    float dc_blocked = 0.0f;
    float peak = 0.0f;

    for (int i = 0; i < track->frame_count; ++i)
    {
        float input = track->samples[i];
        low += (input - low) * coefficient;
        float value = low - previous_input + 0.998f * dc_blocked;
        previous_input = low;
        dc_blocked = value;
        value /= 1.0f + fabsf(value) * 0.25f;
        track->samples[i] = value;
        peak = fmaxf(peak, fabsf(value));
    }

    float scale = peak > 0.72f ? 0.72f / peak : 1.0f;
    int edge_frames = (int)(0.008f * AUDIO_SAMPLE_RATE);
    for (int i = 0; i < track->frame_count; ++i)
    {
        float edge = 1.0f;
        if (i < edge_frames)
            edge = (float)i / (float)edge_frames;
        if (i >= track->frame_count - edge_frames)
        {
            float tail = (float)(track->frame_count - 1 - i) /
                         (float)edge_frames;
            edge = fminf(edge, tail);
        }
        track->samples[i] = clampf(track->samples[i] * scale * edge,
                                   -0.72f, 0.72f);
    }
}

static bool begin_music(CachedSound *track, float duration, float gain)
{
    track->frame_count = (int)(duration * AUDIO_SAMPLE_RATE);
    track->gain = gain;
    track->min_gap_ms = 0;
    track->samples =
        (float *)SDL_calloc((size_t)track->frame_count, sizeof(float));
    return track->samples != NULL;
}

static void add_music_pad(CachedSound *track, float start, float duration,
                          int root, bool minor, float gain)
{
    int third = root + (minor ? 3 : 4);
    int fifth = root + 7;
    add_tone(track, start, duration, midi_hz(root), midi_hz(root),
             gain, WAVE_SINE, 0.16f, 0.34f);
    add_tone(track, start, duration, midi_hz(third), midi_hz(third),
             gain * 0.48f, WAVE_TRIANGLE, 0.19f, 0.31f);
    add_tone(track, start, duration, midi_hz(fifth), midi_hz(fifth),
             gain * 0.32f, WAVE_SINE, 0.22f, 0.29f);
}

static void add_music_chord_stab(CachedSound *track, float start, float duration,
                                 int root, bool minor, float gain)
{
    add_music_note(track, start, duration, root, gain, WAVE_TRIANGLE);
    add_music_note(track, start, duration, root + (minor ? 3 : 4),
                   gain * 0.55f, WAVE_TRIANGLE);
    add_music_note(track, start, duration, root + 7,
                   gain * 0.42f, WAVE_SINE);
}

static bool synth_music_intro(CachedSound *track)
{
    const float beat = 60.0f / 84.0f;
    const float step = beat * 0.25f;
    const float duration = INTRO_MUSIC_BARS * 4.0f * beat;
    static const int roots[INTRO_MUSIC_BARS] = {
        52, 52, 48, 50, 52, 55, 48, 50, 57, 55, 52, 50};
    static const int motif_a[16] = {
        -1, -1, 19, -1, -1, -1, 16, -1,
        -1, 14, -1, -1, 12, -1, -1, -1};
    static const int motif_b[16] = {
        -1, 12, -1, -1, 15, -1, -1, -1,
        19, -1, -1, 17, -1, -1, 15, -1};

    if (!begin_music(track, duration, 0.19f))
        return false;

    for (int bar = 0; bar < INTRO_MUSIC_BARS; ++bar)
    {
        float bar_start = (float)bar * 16.0f * step;
        int root = roots[bar];
        bool sparse = bar < 4 || bar == 10;
        bool full = bar >= 6 && bar <= 9;

        add_music_pad(track, bar_start, 15.5f * step, root, true, 0.040f);
        add_tone(track, bar_start, 15.6f * step,
                 midi_hz(root - 24), midi_hz(root - 24),
                 0.044f, WAVE_SINE, 0.12f, 0.30f);
        add_noise(track, bar_start, 15.5f * step, 0.010f, 0.007f,
                  0.20f, 0.34f, 0x1100u + (Uint32)(bar * 53));

        if (bar >= 2)
        {
            add_music_note(track, bar_start, 3.1f * step,
                           root - 12, 0.105f, WAVE_TRIANGLE);
            add_music_note(track, bar_start + 8.0f * step, 2.8f * step,
                           root - 12, 0.090f, WAVE_TRIANGLE);
            if (full)
                add_music_note(track, bar_start + 12.0f * step, 2.5f * step,
                               root - 10, 0.075f, WAVE_TRIANGLE);
        }

        for (int position = 0; position < 16; ++position)
        {
            float at = bar_start + position * step;
            const int *motif = (bar & 1) ? motif_a : motif_b;

            if (!sparse && (position == 0 || (full && position == 10)))
                add_music_kick(track, at, position == 0 ? 0.16f : 0.10f,
                               0x2100u + (Uint32)(bar * 37 + position));
            if (bar >= 4 && (position == 6 || position == 14))
            {
                add_noise(track, at, 0.035f, 0.033f, 0.78f,
                          0.002f, 0.029f,
                          0x3100u + (Uint32)(bar * 31 + position));
                add_tone(track, at, 0.060f, 980.0f, 530.0f, 0.024f,
                         WAVE_TRIANGLE, 0.002f, 0.052f);
            }
            if ((bar == 1 || bar == 3 || bar == 5 || full) &&
                motif[position] >= 0)
            {
                add_music_note(track, at, 1.7f * step,
                               root + motif[position], 0.047f,
                               WAVE_TRIANGLE);
            }
        }

        /* Sparse window-light sparkles echo the stars in the title tableau. */
        if (bar == 0 || bar == 4 || bar == 8 || bar == 11)
        {
            add_music_note(track, bar_start + 3.0f * step, 2.2f * step,
                           root + 24, 0.028f, WAVE_SINE);
            add_music_note(track, bar_start + 11.0f * step, 2.0f * step,
                           root + 31, 0.022f, WAVE_SINE);
        }
    }

    finish_music(track);
    return true;
}

static bool synth_music_track_one(CachedSound *track)
{
    const float beat = 60.0f / 96.0f;
    const float step = beat * 0.25f;
    const float duration = LEVEL_MUSIC_BARS * 4.0f * beat;
    static const int roots[LEVEL_MUSIC_BARS] = {
        40, 40, 43, 38, 40, 45, 43, 38,
        40, 36, 38, 43, 40, 45, 38, 40};
    static const int motif_a[16] = {
        24, -1, -1, 27, -1, 31, -1, 29,
        -1, 27, -1, 24, -1, 22, -1, -1};
    static const int motif_b[16] = {
        -1, 19, -1, 24, -1, -1, 27, -1,
        31, -1, 29, -1, -1, 27, 24, -1};
    static const int bass_a[] = {0, 3, 6, 10, 14};
    static const int bass_b[] = {0, 2, 5, 8, 11, 14};

    if (!begin_music(track, duration, 0.235f))
        return false;

    for (int bar = 0; bar < LEVEL_MUSIC_BARS; ++bar)
    {
        float bar_start = (float)bar * 16.0f * step;
        int root = roots[bar];
        int section = bar / 4;
        bool breakdown = section == 2;
        bool full = section == 1 || section == 3;
        const int *bass_steps = (bar & 1) ? bass_b : bass_a;
        int bass_count = (bar & 1) ? 6 : 5;

        add_tone(track, bar_start, 15.6f * step,
                 midi_hz(root - 12), midi_hz(root - 12),
                 breakdown ? 0.064f : 0.048f,
                 WAVE_SINE, 0.08f, 0.25f);
        add_music_pad(track, bar_start, 15.4f * step,
                      root + 12, true, breakdown ? 0.020f : 0.027f);

        if (breakdown)
        {
            static const int sparse_bass[] = {0, 6, 12};
            bass_steps = sparse_bass;
            bass_count = 3;
        }

        for (int i = 0; i < bass_count; ++i)
        {
            int position = bass_steps[i];
            int note = root;
            if ((!breakdown && (position == 5 || position == 6)) ||
                (breakdown && position == 6))
                note += (bar & 2) ? 7 : 3;
            if (position == 14)
                note -= 2;
            add_music_note(track, bar_start + position * step,
                           (breakdown ? 2.4f : 1.7f) * step,
                           note, breakdown ? 0.15f : 0.19f,
                           WAVE_SQUARE);
        }

        for (int position = 0; position < 16; ++position)
        {
            float at = bar_start + position * step;
            const int *motif = (bar & 2) ? motif_b : motif_a;
            bool kick = position == 0 || (!breakdown && position == 10) ||
                        (full && position == 6 && (bar & 1));
            bool snare = (!breakdown && (position == 4 || position == 12)) ||
                         (breakdown && (bar & 1) && position == 12);

            if (kick)
                add_music_kick(track, at, position == 0 ? 0.33f : 0.23f,
                               0x4100u + (Uint32)(bar * 61 + position));
            if (snare)
                add_music_snare(track, at, full ? 0.17f : 0.14f,
                                0x5100u + (Uint32)(bar * 59 + position));
            if ((!breakdown && ((full && (position & 1)) ||
                                (!full && (position & 3) == 2))) ||
                (breakdown && (position == 6 || position == 14)))
            {
                add_music_hat(track, at, full ? 0.048f : 0.055f,
                              0x6100u + (Uint32)(bar * 67 + position));
            }

            bool play_motif = (!breakdown && (bar & 1)) ||
                              (section == 3 && (bar & 1) == 0);
            if (play_motif && motif[position] >= 0)
                add_music_note(track, at, 1.1f * step,
                               root + motif[position],
                               section == 3 ? 0.067f : 0.054f,
                               (bar & 1) ? WAVE_TRIANGLE : WAVE_SQUARE);

            if (full && (position & 3) == 0)
            {
                static const int arp[] = {12, 15, 19, 22};
                add_music_note(track, at, 0.78f * step,
                               root + arp[position / 4], 0.038f,
                               WAVE_TRIANGLE);
            }
        }

        if (bar == 3 || bar == 7 || bar == 11)
            add_noise(track, bar_start + 12.0f * step, 3.7f * step,
                      0.052f, 0.32f, 0.42f, 0.08f,
                      0x7100u + (Uint32)bar);

        add_tone(track, bar_start + ((bar & 1) ? 7.0f : 9.0f) * step,
                 0.055f, 1180.0f + bar * 9.0f, 590.0f,
                 0.046f, WAVE_SQUARE, 0.002f, 0.047f);

        if (section == 3 && (bar == 13 || bar == 15))
            add_music_chord_stab(track, bar_start + 8.0f * step,
                                 2.0f * step, root + 12, true, 0.055f);
    }

    finish_music(track);
    return true;
}

static bool synth_music_track_two(CachedSound *track)
{
    const float beat = 60.0f / 112.0f;
    const float step = beat * 0.25f;
    const float duration = LEVEL_MUSIC_BARS * 4.0f * beat;
    static const int roots[LEVEL_MUSIC_BARS] = {
        38, 39, 36, 33, 38, 41, 39, 36,
        38, 34, 36, 33, 38, 39, 41, 36};
    static const int signal_a[16] = {
        24, -1, 27, -1, 25, -1, -1, 22,
        -1, 24, -1, 20, -1, 22, -1, -1};
    static const int signal_b[16] = {
        -1, 19, -1, 20, -1, 24, -1, -1,
        25, -1, 24, -1, 22, -1, 20, -1};
    static const int bass_a[] = {0, 2, 5, 8, 11, 14};
    static const int bass_b[] = {0, 3, 6, 9, 12, 14};

    if (!begin_music(track, duration, 0.225f))
        return false;

    for (int bar = 0; bar < LEVEL_MUSIC_BARS; ++bar)
    {
        float bar_start = (float)bar * 16.0f * step;
        int root = roots[bar];
        int section = bar / 4;
        bool breakdown = section == 2;
        bool full = section == 1 || section == 3;
        const int *bass_steps = (bar & 1) ? bass_b : bass_a;
        int bass_count = 6;

        add_tone(track, bar_start, 15.5f * step, midi_hz(26), midi_hz(26),
                 breakdown ? 0.070f : 0.052f,
                 WAVE_SINE, 0.07f, 0.24f);
        add_music_pad(track, bar_start, 7.7f * step,
                      root + 12, true, breakdown ? 0.028f : 0.021f);
        add_music_pad(track, bar_start + 8.0f * step, 7.5f * step,
                      root + ((bar & 1) ? 11 : 12), true,
                      breakdown ? 0.028f : 0.021f);

        if (breakdown)
        {
            static const int sparse_bass[] = {0, 5, 11};
            bass_steps = sparse_bass;
            bass_count = 3;
        }

        for (int i = 0; i < bass_count; ++i)
        {
            int position = bass_steps[i];
            int note = root;
            if (position == 5 || position == 6 || position == 11)
                note += (bar & 1) ? -2 : 1;
            if (full && position == 12)
                note += 7;
            add_music_note(track, bar_start + position * step,
                           (breakdown ? 2.1f : 1.4f) * step,
                           note, breakdown ? 0.145f : 0.18f,
                           WAVE_SAW);
        }

        for (int position = 0; position < 16; ++position)
        {
            float at = bar_start + position * step;
            const int *signal = (bar & 2) ? signal_b : signal_a;
            bool kick = position == 0 ||
                        (!breakdown && (position == 6 || position == 11)) ||
                        (full && position == 14 && (bar & 1));
            bool snare = (!breakdown && (position == 4 || position == 12)) ||
                         (breakdown && position == 12 && (bar & 1));

            if (kick)
                add_music_kick(track, at,
                               position == 0 ? 0.32f : 0.21f,
                               0x8200u + (Uint32)(bar * 71 + position));
            if (snare)
                add_music_snare(track, at, full ? 0.19f : 0.16f,
                                0x9200u + (Uint32)(bar * 73 + position));
            if ((!breakdown && ((full && (position & 1)) ||
                                (!full && (position & 3) == 1))) ||
                (breakdown && (position == 3 || position == 11)))
            {
                add_music_hat(track, at,
                              full && (position == 7 || position == 15)
                                  ? 0.066f
                                  : 0.041f,
                              0xa200u + (Uint32)(bar * 79 + position));
            }

            bool warning_phrase = (bar == 1 || bar == 3 ||
                                   bar == 5 || bar == 6 ||
                                   bar == 12 || bar == 14 || bar == 15);
            if (warning_phrase && signal[position] >= 0)
                add_music_note(track, at, 0.72f * step,
                               root + signal[position],
                               section == 3 ? 0.070f : 0.057f,
                               WAVE_SQUARE);

            if (full && (position == 2 || position == 10))
                add_music_chord_stab(track, at, 0.85f * step,
                                     root + 12, true, 0.040f);
        }

        int clank_position = (bar & 1) ? 7 : 9;
        add_noise(track, bar_start + clank_position * step,
                  0.085f, 0.055f, 0.28f, 0.002f, 0.072f,
                  0xb200u + (Uint32)(bar * 83));
        add_tone(track, bar_start + clank_position * step, 0.10f,
                 720.0f + bar * 13.0f, 310.0f, 0.040f,
                 WAVE_SAW, 0.002f, 0.082f);

        if (bar == 3 || bar == 7 || bar == 11)
        {
            add_noise(track, bar_start + 12.0f * step, 3.6f * step,
                      0.060f, 0.38f, 0.35f, 0.07f,
                      0xc200u + (Uint32)bar);
            add_tone(track, bar_start + 14.0f * step, 1.4f * step,
                     310.0f, 880.0f, 0.045f,
                     WAVE_TRIANGLE, 0.01f, 0.07f);
        }
    }

    finish_music(track);
    return true;
}

static bool synth_music(AudioSystem *audio)
{
    return synth_music_intro(&audio->music_tracks[MUSIC_INTRO]) &&
           synth_music_track_one(&audio->music_tracks[MUSIC_LEVEL_ONE]) &&
           synth_music_track_two(&audio->music_tracks[MUSIC_LEVEL_TWO]);
}

static bool synth_sound(AudioSystem *audio, SoundEffect effect)
{
    CachedSound *s = &audio->sounds[effect];

    switch (effect)
    {
    case SFX_MENU_START:
        if (!begin_sound(audio, effect, 0.50f, 0.54f, 100))
            return false;
        add_tone(s, 0.00f, 0.14f, 261.6f, 261.6f, 0.36f, WAVE_SQUARE, 0.008f, 0.09f);
        add_tone(s, 0.10f, 0.14f, 329.6f, 329.6f, 0.35f, WAVE_SQUARE, 0.008f, 0.09f);
        add_tone(s, 0.20f, 0.14f, 392.0f, 392.0f, 0.34f, WAVE_SQUARE, 0.008f, 0.09f);
        add_tone(s, 0.30f, 0.20f, 523.3f, 526.0f, 0.46f, WAVE_TRIANGLE, 0.008f, 0.15f);
        break;
    case SFX_MENU_BACK:
        if (!begin_sound(audio, effect, 0.20f, 0.36f, 90))
            return false;
        add_tone(s, 0.00f, 0.20f, 310.0f, 135.0f, 0.52f, WAVE_SQUARE, 0.006f, 0.13f);
        break;
    case SFX_OPENING_RAIN:
        if (!begin_sound(audio, effect, 11.95f, 0.11f, 500))
            return false;
        add_noise(s, 0.00f, 11.95f, 0.46f, 0.62f,
                  0.18f, 0.55f, 0x7139u);
        add_noise(s, 0.00f, 11.95f, 0.24f, 0.035f,
                  0.22f, 0.62f, 0xa84cu);
        break;
    case SFX_OPENING_SUV_ENGINE:
        if (!begin_sound(audio, effect, 2.82f, 0.39f, 300))
            return false;
        add_tone(s, 0.00f, 2.82f, 47.0f, 31.0f, 0.58f,
                 WAVE_SAW, 0.10f, 0.38f);
        add_tone(s, 0.00f, 2.82f, 94.0f, 62.0f, 0.23f,
                 WAVE_TRIANGLE, 0.10f, 0.38f);
        add_noise(s, 0.00f, 2.82f, 0.31f, 0.028f,
                  0.12f, 0.40f, 0x5e91u);
        break;
    case SFX_OPENING_CAR_ENGINE:
        if (!begin_sound(audio, effect, 2.62f, 0.34f, 300))
            return false;
        add_tone(s, 0.00f, 2.62f, 61.0f, 39.0f, 0.50f,
                 WAVE_SAW, 0.09f, 0.34f);
        add_tone(s, 0.00f, 2.62f, 122.0f, 78.0f, 0.21f,
                 WAVE_TRIANGLE, 0.09f, 0.34f);
        add_noise(s, 0.00f, 2.62f, 0.23f, 0.034f,
                  0.11f, 0.36f, 0x918eu);
        break;
    case SFX_OPENING_BRAKE:
        if (!begin_sound(audio, effect, 0.64f, 0.36f, 260))
            return false;
        add_noise(s, 0.00f, 0.64f, 0.62f, 0.37f,
                  0.006f, 0.55f, 0x42d1u);
        add_tone(s, 0.00f, 0.50f, 720.0f, 175.0f, 0.24f,
                 WAVE_SAW, 0.008f, 0.44f);
        add_tone(s, 0.34f, 0.27f, 92.0f, 43.0f, 0.36f,
                 WAVE_SINE, 0.004f, 0.24f);
        break;
    case SFX_OPENING_CAR_DOOR:
        if (!begin_sound(audio, effect, 0.43f, 0.43f, 130))
            return false;
        add_noise(s, 0.00f, 0.065f, 0.55f, 0.55f,
                  0.003f, 0.052f, 0xd712u);
        add_tone(s, 0.04f, 0.25f, 126.0f, 67.0f, 0.34f,
                 WAVE_SAW, 0.012f, 0.09f);
        add_noise(s, 0.29f, 0.14f, 0.72f, 0.12f,
                  0.003f, 0.12f, 0x2a61u);
        add_tone(s, 0.29f, 0.14f, 94.0f, 42.0f, 0.48f,
                 WAVE_TRIANGLE, 0.003f, 0.12f);
        break;
    case SFX_OUTRO_HELICOPTER:
        if (!begin_sound(audio, effect, 9.35f, 0.30f, 500))
            return false;
        /*
         * Two detuned low rotors and filtered noise create a long approach.
         * The cached sound ends just before the aircraft hits the roof.
         */
        add_tone(s, 0.00f, 9.35f, 34.0f, 28.0f, 0.52f,
                 WAVE_SAW, 0.18f, 0.42f);
        add_tone(s, 0.00f, 9.35f, 68.0f, 56.0f, 0.23f,
                 WAVE_TRIANGLE, 0.18f, 0.42f);
        add_noise(s, 0.00f, 9.35f, 0.18f, 0.045f,
                  0.18f, 0.42f, 0x48454c49u);
        for (int beat = 0; beat < 37; ++beat)
        {
            float at = (float)beat * 0.25f;
            add_noise(s, at, 0.075f, 0.24f, 0.22f,
                      0.004f, 0.06f, 0x6200u + (Uint32)beat * 19u);
        }
        break;
    case SFX_REVEAL_TICK:
        if (!begin_sound(audio, effect, 0.052f, 0.12f, 40))
            return false;
        add_tone(s, 0.00f, 0.052f, 680.0f, 440.0f, 0.32f, WAVE_SQUARE, 0.003f, 0.042f);
        add_noise(s, 0.00f, 0.030f, 0.11f, 0.55f, 0.003f, 0.026f, 0x1284u);
        break;
    case SFX_CARD_SCAN:
        if (!begin_sound(audio, effect, 0.080f, 0.22f, 35))
            return false;
        add_tone(s, 0.00f, 0.080f, 940.0f, 690.0f, 0.43f, WAVE_TRIANGLE, 0.004f, 0.060f);
        break;
    case SFX_CARD_TARGET:
        if (!begin_sound(audio, effect, 0.42f, 0.54f, 100))
            return false;
        add_tone(s, 0.00f, 0.16f, 440.0f, 660.0f, 0.46f, WAVE_TRIANGLE, 0.005f, 0.08f);
        add_tone(s, 0.13f, 0.29f, 880.0f, 882.0f, 0.32f, WAVE_TRIANGLE, 0.008f, 0.22f);
        break;
    case SFX_TERMINAL_ALARM:
        if (!begin_sound(audio, effect, 0.72f, 0.37f, 500))
            return false;
        add_tone(s, 0.00f, 0.30f, 620.0f, 910.0f, 0.43f,
                 WAVE_TRIANGLE, 0.008f, 0.10f);
        add_tone(s, 0.28f, 0.32f, 910.0f, 620.0f, 0.43f,
                 WAVE_TRIANGLE, 0.006f, 0.13f);
        add_tone(s, 0.54f, 0.18f, 1140.0f, 780.0f, 0.24f,
                 WAVE_SQUARE, 0.004f, 0.14f);
        break;
    case SFX_JUMP:
        if (!begin_sound(audio, effect, 0.20f, 0.30f, 80))
            return false;
        add_tone(s, 0.00f, 0.20f, 135.0f, 470.0f, 0.43f, WAVE_SQUARE, 0.006f, 0.09f);
        add_tone(s, 0.00f, 0.17f, 82.0f, 220.0f, 0.30f, WAVE_SINE, 0.006f, 0.10f);
        break;
    case SFX_LAND:
        if (!begin_sound(audio, effect, 0.18f, 0.28f, 90))
            return false;
        add_noise(s, 0.00f, 0.18f, 0.68f, 0.06f, 0.005f, 0.16f, 0xa81cu);
        add_tone(s, 0.00f, 0.13f, 88.0f, 48.0f, 0.54f, WAVE_SINE, 0.004f, 0.11f);
        break;
    case SFX_STEP_A:
    case SFX_STEP_B:
        if (!begin_sound(audio, effect, 0.095f, 0.17f, 130))
            return false;
        add_noise(s, 0.00f, 0.095f, 0.56f, 0.16f, 0.004f, 0.082f,
                  effect == SFX_STEP_A ? 0x516au : 0x92f1u);
        add_tone(s, 0.00f, 0.072f,
                 effect == SFX_STEP_A ? 108.0f : 94.0f, 54.0f,
                 0.26f, WAVE_TRIANGLE, 0.003f, 0.062f);
        break;
    case SFX_LADDER:
        if (!begin_sound(audio, effect, 0.12f, 0.16f, 145))
            return false;
        add_tone(s, 0.00f, 0.12f, 285.0f, 175.0f, 0.34f, WAVE_SQUARE, 0.004f, 0.095f);
        add_noise(s, 0.00f, 0.075f, 0.25f, 0.42f, 0.003f, 0.065f, 0x77c2u);
        break;
    case SFX_DOOR:
        if (!begin_sound(audio, effect, 0.38f, 0.44f, 180))
            return false;
        add_noise(s, 0.00f, 0.28f, 0.60f, 0.025f, 0.01f, 0.11f, 0x9912u);
        add_tone(s, 0.03f, 0.26f, 92.0f, 54.0f, 0.42f, WAVE_SAW, 0.02f, 0.08f);
        add_tone(s, 0.28f, 0.10f, 235.0f, 115.0f, 0.32f, WAVE_TRIANGLE, 0.004f, 0.08f);
        break;
    case SFX_ELEVATOR:
        if (!begin_sound(audio, effect, 0.62f, 0.27f, 500))
            return false;
        add_tone(s, 0.00f, 0.62f, 64.0f, 76.0f, 0.50f, WAVE_SAW, 0.05f, 0.12f);
        add_tone(s, 0.00f, 0.62f, 128.0f, 152.0f, 0.18f, WAVE_SINE, 0.05f, 0.12f);
        add_noise(s, 0.00f, 0.62f, 0.16f, 0.025f, 0.06f, 0.12f, 0x4e11u);
        break;
    case SFX_MOVING_PLATFORM:
        if (!begin_sound(audio, effect, 0.43f, 0.27f, 420))
            return false;
        add_tone(s, 0.00f, 0.43f, 72.0f, 88.0f, 0.46f,
                 WAVE_SAW, 0.018f, 0.12f);
        add_noise(s, 0.00f, 0.16f, 0.38f, 0.19f,
                  0.003f, 0.13f, 0x6d71u);
        add_tone(s, 0.27f, 0.14f, 245.0f, 96.0f, 0.25f,
                 WAVE_TRIANGLE, 0.003f, 0.11f);
        break;
    case SFX_PLATFORM_CRACK:
        if (!begin_sound(audio, effect, 0.31f, 0.34f, 190))
            return false;
        add_noise(s, 0.00f, 0.10f, 0.65f, 0.48f, 0.003f, 0.09f, 0x1255u);
        add_noise(s, 0.10f, 0.09f, 0.54f, 0.52f, 0.003f, 0.08f, 0x4421u);
        add_noise(s, 0.19f, 0.12f, 0.46f, 0.56f, 0.003f, 0.11f, 0x9931u);
        add_tone(s, 0.00f, 0.31f, 120.0f, 42.0f, 0.28f, WAVE_TRIANGLE, 0.005f, 0.13f);
        break;
    case SFX_PLAYER_SHOT:
        if (!begin_sound(audio, effect, 0.24f, 0.43f, 60))
            return false;
        add_noise(s, 0.00f, 0.10f, 0.72f, 0.52f, 0.003f, 0.085f, 0x81edu);
        add_tone(s, 0.00f, 0.18f, 190.0f, 58.0f, 0.56f, WAVE_SAW, 0.003f, 0.15f);
        add_tone(s, 0.010f, 0.21f, 88.0f, 40.0f, 0.38f, WAVE_SINE, 0.004f, 0.19f);
        add_noise(s, 0.12f, 0.08f, 0.17f, 0.32f, 0.006f, 0.065f, 0x19b4u);
        break;
    case SFX_EMPTY_CLICK:
        if (!begin_sound(audio, effect, 0.14f, 0.26f, 110))
            return false;
        add_noise(s, 0.00f, 0.045f, 0.38f, 0.42f, 0.004f, 0.038f, 0xe42au);
        add_tone(s, 0.050f, 0.090f, 520.0f, 220.0f, 0.34f, WAVE_SQUARE, 0.003f, 0.075f);
        break;
    case SFX_KNIFE_SWING:
        if (!begin_sound(audio, effect, 0.18f, 0.28f, 85))
            return false;
        add_noise(s, 0.00f, 0.15f, 0.48f, 0.72f,
                  0.003f, 0.13f, 0x51a5u);
        add_tone(s, 0.015f, 0.15f, 1120.0f, 360.0f, 0.25f,
                 WAVE_TRIANGLE, 0.003f, 0.12f);
        break;
    case SFX_ENEMY_ALERT:
        if (!begin_sound(audio, effect, 0.25f, 0.34f, 240))
            return false;
        add_tone(s, 0.00f, 0.10f, 690.0f, 705.0f, 0.43f, WAVE_TRIANGLE, 0.006f, 0.065f);
        add_tone(s, 0.13f, 0.12f, 850.0f, 875.0f, 0.47f, WAVE_TRIANGLE, 0.006f, 0.080f);
        break;
    case SFX_ENEMY_SHOT:
        if (!begin_sound(audio, effect, 0.20f, 0.38f, 70))
            return false;
        add_noise(s, 0.00f, 0.085f, 0.62f, 0.50f, 0.003f, 0.072f, 0x33e1u);
        add_tone(s, 0.00f, 0.18f, 150.0f, 48.0f, 0.54f, WAVE_SAW, 0.003f, 0.15f);
        add_tone(s, 0.008f, 0.18f, 74.0f, 38.0f, 0.26f, WAVE_SINE, 0.004f, 0.16f);
        break;
    case SFX_BULLET_IMPACT:
        if (!begin_sound(audio, effect, 0.14f, 0.21f, 60))
            return false;
        add_noise(s, 0.00f, 0.085f, 0.58f, 0.60f, 0.003f, 0.072f, 0xf20au);
        add_tone(s, 0.00f, 0.14f, 920.0f, 310.0f, 0.21f, WAVE_SQUARE, 0.003f, 0.115f);
        break;
    case SFX_ENEMY_HIT:
        if (!begin_sound(audio, effect, 0.18f, 0.31f, 70))
            return false;
        add_noise(s, 0.00f, 0.15f, 0.46f, 0.20f, 0.004f, 0.14f, 0xa320u);
        add_tone(s, 0.00f, 0.18f, 175.0f, 68.0f, 0.54f, WAVE_TRIANGLE, 0.004f, 0.15f);
        break;
    case SFX_ENEMY_DOWN:
        if (!begin_sound(audio, effect, 0.38f, 0.35f, 100))
            return false;
        add_tone(s, 0.00f, 0.38f, 195.0f, 42.0f, 0.60f, WAVE_TRIANGLE, 0.005f, 0.21f);
        add_noise(s, 0.13f, 0.23f, 0.36f, 0.09f, 0.005f, 0.21f, 0x8820u);
        break;
    case SFX_GUARD_TALK:
        if (!begin_sound(audio, effect, 0.48f, 0.20f, 700))
            return false;
        add_tone(s, 0.00f, 0.10f, 185.0f, 142.0f, 0.46f, WAVE_TRIANGLE, 0.008f, 0.06f);
        add_tone(s, 0.13f, 0.09f, 230.0f, 174.0f, 0.40f, WAVE_TRIANGLE, 0.008f, 0.06f);
        add_tone(s, 0.25f, 0.14f, 165.0f, 205.0f, 0.42f, WAVE_TRIANGLE, 0.008f, 0.08f);
        add_noise(s, 0.00f, 0.44f, 0.09f, 0.35f, 0.012f, 0.09f, 0x5aa5u);
        break;
    case SFX_DOG_BARK:
        if (!begin_sound(audio, effect, 0.39f, 0.44f, 420))
            return false;
        /* Two chesty "woof" pulses: a falling fundamental, vocal formants
         * and breath noise make this read as a dog instead of an alarm tone. */
        add_tone(s, 0.00f, 0.18f, 205.0f, 82.0f, 0.70f,
                 WAVE_SAW, 0.004f, 0.13f);
        add_tone(s, 0.01f, 0.15f, 490.0f, 230.0f, 0.25f,
                 WAVE_TRIANGLE, 0.004f, 0.12f);
        add_noise(s, 0.00f, 0.17f, 0.40f, 0.16f,
                  0.004f, 0.14f, 0xb0a1u);
        add_tone(s, 0.22f, 0.17f, 176.0f, 72.0f, 0.63f,
                 WAVE_SAW, 0.004f, 0.13f);
        add_tone(s, 0.23f, 0.14f, 405.0f, 205.0f, 0.21f,
                 WAVE_TRIANGLE, 0.004f, 0.11f);
        add_noise(s, 0.22f, 0.16f, 0.34f, 0.14f,
                  0.004f, 0.13f, 0x4d72u);
        break;
    case SFX_DOG_BARK_ALT:
        if (!begin_sound(audio, effect, 0.25f, 0.43f, 420))
            return false;
        /* A shorter, sharper bark keeps repeated chase calls from sounding
         * like the exact same cached sample every time. */
        add_noise(s, 0.00f, 0.045f, 0.52f, 0.62f,
                  0.003f, 0.036f, 0xc731u);
        add_tone(s, 0.00f, 0.22f, 248.0f, 96.0f, 0.69f,
                 WAVE_SAW, 0.004f, 0.17f);
        add_tone(s, 0.012f, 0.17f, 620.0f, 285.0f, 0.23f,
                 WAVE_TRIANGLE, 0.004f, 0.14f);
        add_noise(s, 0.02f, 0.20f, 0.32f, 0.13f,
                  0.004f, 0.17f, 0x2e19u);
        break;
    case SFX_DOG_GROWL:
        if (!begin_sound(audio, effect, 0.52f, 0.27f, 900))
            return false;
        /* Closely spaced throat pulses form a low, rough growl. */
        for (int pulse = 0; pulse < 6; ++pulse)
        {
            float at = pulse * 0.072f;
            float pitch = 105.0f - pulse * 3.5f;
            add_tone(s, at, 0.13f, pitch, pitch * 0.74f, 0.34f,
                     WAVE_SAW, 0.008f, 0.085f);
            add_noise(s, at, 0.12f, 0.19f, 0.08f,
                      0.008f, 0.08f, 0x6610u + (Uint32)(pulse * 97));
        }
        break;
    case SFX_DOG_BITE:
        if (!begin_sound(audio, effect, 0.22f, 0.39f, 140))
            return false;
        add_tone(s, 0.00f, 0.17f, 170.0f, 61.0f, 0.57f,
                 WAVE_SAW, 0.004f, 0.13f);
        add_noise(s, 0.00f, 0.17f, 0.48f, 0.12f,
                  0.004f, 0.14f, 0x91b2u);
        /* Dry jaw snap at the end of the snarl. */
        add_noise(s, 0.135f, 0.045f, 0.76f, 0.72f,
                  0.002f, 0.038f, 0x8fe3u);
        add_tone(s, 0.135f, 0.075f, 310.0f, 88.0f, 0.31f,
                 WAVE_TRIANGLE, 0.002f, 0.062f);
        break;
    case SFX_DOG_YELP:
        if (!begin_sound(audio, effect, 0.43f, 0.43f, 320))
            return false;
        /* Quick upward cry followed by a falling whimper. */
        add_tone(s, 0.00f, 0.17f, 390.0f, 980.0f, 0.57f,
                 WAVE_TRIANGLE, 0.005f, 0.055f);
        add_tone(s, 0.04f, 0.13f, 690.0f, 1180.0f, 0.18f,
                 WAVE_SINE, 0.005f, 0.05f);
        add_noise(s, 0.00f, 0.16f, 0.16f, 0.24f,
                  0.006f, 0.08f, 0x71a4u);
        add_tone(s, 0.15f, 0.28f, 850.0f, 310.0f, 0.42f,
                 WAVE_TRIANGLE, 0.004f, 0.22f);
        break;
    case SFX_GRENADE_THROW:
        if (!begin_sound(audio, effect, 0.20f, 0.34f, 80))
            return false;
        add_noise(s, 0.00f, 0.20f, 0.52f, 0.10f, 0.002f, 0.14f, 0x18d4u);
        add_tone(s, 0.00f, 0.16f, 280.0f, 115.0f, 0.42f, WAVE_TRIANGLE, 0.002f, 0.11f);
        break;
    case SFX_GRENADE_FUSE:
        if (!begin_sound(audio, effect, 0.095f, 0.25f, 90))
            return false;
        add_tone(s, 0.00f, 0.055f, 1180.0f, 920.0f, 0.48f,
                 WAVE_SQUARE, 0.002f, 0.042f);
        add_noise(s, 0.00f, 0.045f, 0.24f, 0.71f,
                  0.002f, 0.036f, 0xf053u);
        add_tone(s, 0.047f, 0.048f, 740.0f, 510.0f, 0.28f,
                 WAVE_TRIANGLE, 0.002f, 0.039f);
        break;
    case SFX_GRENADE_BOUNCE:
        if (!begin_sound(audio, effect, 0.13f, 0.22f, 90))
            return false;
        add_tone(s, 0.00f, 0.13f, 430.0f, 165.0f, 0.42f, WAVE_SQUARE, 0.003f, 0.105f);
        add_noise(s, 0.00f, 0.075f, 0.28f, 0.55f, 0.003f, 0.064f, 0xa991u);
        break;
    case SFX_EXPLOSION:
        if (!begin_sound(audio, effect, 0.78f, 0.52f, 120))
            return false;
        add_noise(s, 0.00f, 0.78f, 1.02f, 0.055f, 0.006f, 0.73f, 0xe771u);
        add_noise(s, 0.00f, 0.31f, 0.65f, 0.38f, 0.004f, 0.28f, 0x4419u);
        add_tone(s, 0.00f, 0.68f, 84.0f, 26.0f, 0.82f, WAVE_SINE, 0.005f, 0.62f);
        break;
    case SFX_MINE_ARM:
        if (!begin_sound(audio, effect, 0.34f, 0.38f, 170))
            return false;
        add_tone(s, 0.00f, 0.12f, 900.0f, 900.0f, 0.46f, WAVE_TRIANGLE, 0.006f, 0.08f);
        add_tone(s, 0.18f, 0.16f, 1160.0f, 1160.0f, 0.50f, WAVE_TRIANGLE, 0.006f, 0.11f);
        break;
    case SFX_CRATE_PUSH:
        if (!begin_sound(audio, effect, 0.31f, 0.25f, 240))
            return false;
        add_noise(s, 0.00f, 0.31f, 0.66f, 0.045f,
                  0.012f, 0.11f, 0x9c42u);
        add_tone(s, 0.00f, 0.29f, 118.0f, 73.0f, 0.31f,
                 WAVE_SAW, 0.012f, 0.10f);
        break;
    case SFX_CRATE_LAND:
        if (!begin_sound(audio, effect, 0.36f, 0.42f, 180))
            return false;
        add_noise(s, 0.00f, 0.27f, 0.83f, 0.10f,
                  0.003f, 0.24f, 0x1a6du);
        add_tone(s, 0.00f, 0.31f, 112.0f, 35.0f, 0.68f,
                 WAVE_TRIANGLE, 0.003f, 0.27f);
        add_noise(s, 0.12f, 0.24f, 0.34f, 0.31f,
                  0.004f, 0.22f, 0x7b18u);
        break;
    case SFX_CRATE_BREAK:
        if (!begin_sound(audio, effect, 0.40f, 0.36f, 95))
            return false;
        add_noise(s, 0.00f, 0.34f, 0.80f, 0.20f, 0.004f, 0.31f, 0xc2a7u);
        add_tone(s, 0.00f, 0.24f, 145.0f, 42.0f, 0.38f, WAVE_TRIANGLE, 0.004f, 0.20f);
        break;
    case SFX_FAN_HIT:
        if (!begin_sound(audio, effect, 0.32f, 0.43f, 260))
            return false;
        add_noise(s, 0.00f, 0.22f, 0.72f, 0.54f,
                  0.002f, 0.19f, 0xfa31u);
        add_tone(s, 0.00f, 0.28f, 760.0f, 185.0f, 0.43f,
                 WAVE_SAW, 0.002f, 0.23f);
        add_tone(s, 0.08f, 0.24f, 132.0f, 54.0f, 0.40f,
                 WAVE_TRIANGLE, 0.003f, 0.21f);
        break;
    case SFX_SPIKE_HIT:
        if (!begin_sound(audio, effect, 0.24f, 0.39f, 260))
            return false;
        add_noise(s, 0.00f, 0.11f, 0.68f, 0.68f,
                  0.002f, 0.09f, 0x5b17u);
        add_tone(s, 0.00f, 0.23f, 1280.0f, 210.0f, 0.44f,
                 WAVE_TRIANGLE, 0.002f, 0.19f);
        break;
    case SFX_PICKUP_AMMO:
        if (!begin_sound(audio, effect, 0.24f, 0.34f, 80))
            return false;
        add_tone(s, 0.00f, 0.08f, 330.0f, 330.0f, 0.40f, WAVE_SQUARE, 0.004f, 0.05f);
        add_tone(s, 0.08f, 0.08f, 440.0f, 440.0f, 0.40f, WAVE_SQUARE, 0.004f, 0.05f);
        add_tone(s, 0.16f, 0.08f, 660.0f, 660.0f, 0.42f, WAVE_SQUARE, 0.004f, 0.055f);
        break;
    case SFX_PICKUP_GRENADE:
        if (!begin_sound(audio, effect, 0.25f, 0.42f, 70))
            return false;
        add_tone(s, 0.00f, 0.18f, 180.0f, 310.0f, 0.56f, WAVE_TRIANGLE, 0.003f, 0.08f);
        add_tone(s, 0.13f, 0.12f, 600.0f, 410.0f, 0.34f, WAVE_SQUARE, 0.004f, 0.09f);
        break;
    case SFX_PICKUP_HEALTH:
        if (!begin_sound(audio, effect, 0.38f, 0.45f, 100))
            return false;
        add_tone(s, 0.00f, 0.18f, 392.0f, 392.0f, 0.38f, WAVE_TRIANGLE, 0.005f, 0.11f);
        add_tone(s, 0.10f, 0.20f, 523.3f, 523.3f, 0.40f, WAVE_TRIANGLE, 0.005f, 0.13f);
        add_tone(s, 0.21f, 0.17f, 784.0f, 788.0f, 0.34f, WAVE_SINE, 0.005f, 0.13f);
        break;
    case SFX_CARD_WRONG:
        if (!begin_sound(audio, effect, 0.31f, 0.34f, 110))
            return false;
        add_tone(s, 0.00f, 0.12f, 250.0f, 185.0f, 0.46f, WAVE_TRIANGLE, 0.007f, 0.08f);
        add_tone(s, 0.16f, 0.15f, 185.0f, 105.0f, 0.50f, WAVE_TRIANGLE, 0.007f, 0.11f);
        break;
    case SFX_EXIT_UNLOCKED:
        if (!begin_sound(audio, effect, 0.67f, 0.50f, 140))
            return false;
        add_tone(s, 0.00f, 0.18f, 392.0f, 392.0f, 0.35f, WAVE_SQUARE, 0.006f, 0.11f);
        add_tone(s, 0.13f, 0.18f, 523.3f, 523.3f, 0.35f, WAVE_SQUARE, 0.006f, 0.11f);
        add_tone(s, 0.26f, 0.18f, 659.3f, 659.3f, 0.34f, WAVE_SQUARE, 0.006f, 0.11f);
        add_tone(s, 0.39f, 0.28f, 1046.5f, 1052.0f, 0.34f, WAVE_SINE, 0.008f, 0.23f);
        break;
    case SFX_PLAYER_HIT:
        if (!begin_sound(audio, effect, 0.52f, 0.48f, 280))
            return false;
        add_noise(s, 0.00f, 0.29f, 0.62f, 0.19f, 0.004f, 0.26f, 0xd1e0u);
        add_tone(s, 0.00f, 0.52f, 225.0f, 46.0f, 0.68f, WAVE_TRIANGLE, 0.005f, 0.35f);
        break;
    case SFX_RESPAWN:
        if (!begin_sound(audio, effect, 0.52f, 0.48f, 150))
            return false;
        add_tone(s, 0.00f, 0.45f, 105.0f, 640.0f, 0.48f, WAVE_TRIANGLE, 0.008f, 0.16f);
        add_tone(s, 0.18f, 0.34f, 220.0f, 880.0f, 0.25f, WAVE_TRIANGLE, 0.006f, 0.20f);
        break;
    case SFX_LEVEL_CLEAR:
        if (!begin_sound(audio, effect, 0.88f, 0.52f, 220))
            return false;
        add_tone(s, 0.00f, 0.22f, 261.6f, 261.6f, 0.34f, WAVE_SQUARE, 0.008f, 0.15f);
        add_tone(s, 0.16f, 0.22f, 329.6f, 329.6f, 0.34f, WAVE_SQUARE, 0.008f, 0.15f);
        add_tone(s, 0.32f, 0.22f, 392.0f, 392.0f, 0.34f, WAVE_SQUARE, 0.008f, 0.15f);
        add_tone(s, 0.48f, 0.40f, 523.3f, 527.0f, 0.46f, WAVE_TRIANGLE, 0.006f, 0.32f);
        break;
    case SFX_GAME_OVER:
        if (!begin_sound(audio, effect, 1.15f, 0.54f, 320))
            return false;
        add_tone(s, 0.00f, 0.31f, 392.0f, 370.0f, 0.36f, WAVE_SQUARE, 0.008f, 0.21f);
        add_tone(s, 0.25f, 0.34f, 293.7f, 270.0f, 0.38f, WAVE_SQUARE, 0.008f, 0.23f);
        add_tone(s, 0.52f, 0.63f, 196.0f, 62.0f, 0.52f, WAVE_TRIANGLE, 0.006f, 0.48f);
        break;
    case SFX_WIN:
        if (!begin_sound(audio, effect, 1.45f, 0.56f, 320))
            return false;
        add_tone(s, 0.00f, 0.28f, 261.6f, 261.6f, 0.32f, WAVE_SQUARE, 0.008f, 0.17f);
        add_tone(s, 0.20f, 0.28f, 329.6f, 329.6f, 0.32f, WAVE_SQUARE, 0.008f, 0.17f);
        add_tone(s, 0.40f, 0.28f, 392.0f, 392.0f, 0.32f, WAVE_SQUARE, 0.008f, 0.17f);
        add_tone(s, 0.60f, 0.30f, 523.3f, 523.3f, 0.36f, WAVE_SQUARE, 0.008f, 0.19f);
        add_tone(s, 0.80f, 0.65f, 784.0f, 790.0f, 0.42f, WAVE_TRIANGLE, 0.008f, 0.52f);
        add_tone(s, 0.80f, 0.65f, 523.3f, 526.0f, 0.24f, WAVE_SINE, 0.008f, 0.52f);
        break;
    case SFX_COUNT:
        return false;
    }

    finish_sound(s);
    return true;
}

static void free_cache(AudioSystem *audio)
{
    for (int i = 0; i < SFX_COUNT; ++i)
    {
        SDL_free(audio->sounds[i].samples);
        audio->sounds[i].samples = NULL;
        audio->sounds[i].frame_count = 0;
    }
    for (int i = 0; i < MUSIC_TRACK_COUNT; ++i)
    {
        SDL_free(audio->music_tracks[i].samples);
        audio->music_tracks[i].samples = NULL;
        audio->music_tracks[i].frame_count = 0;
    }
}

bool audio_init(AudioSystem *audio)
{
    SDL_zerop(audio);
    audio->current_music = -1;

    if (!SDL_InitSubSystem(SDL_INIT_AUDIO))
    {
        SDL_Log("Audio disabled: SDL audio init failed: %s", SDL_GetError());
        return false;
    }
    audio->subsystem_initialized = true;

    SDL_AudioSpec spec = {
        .format = SDL_AUDIO_F32,
        .channels = 1,
        .freq = AUDIO_SAMPLE_RATE};

    audio->device = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, NULL);
    if (audio->device == 0)
    {
        SDL_Log("Audio disabled: could not open playback device: %s", SDL_GetError());
        audio_shutdown(audio);
        return false;
    }

    for (int i = 0; i < AUDIO_VOICE_COUNT; ++i)
    {
        audio->voices[i] = SDL_CreateAudioStream(&spec, NULL);
        if (audio->voices[i] == NULL)
        {
            SDL_Log("Audio disabled: could not create voice: %s", SDL_GetError());
            audio_shutdown(audio);
            return false;
        }
    }
    audio->music_stream = SDL_CreateAudioStream(&spec, NULL);
    if (audio->music_stream == NULL)
    {
        SDL_Log("Audio disabled: could not create music stream: %s",
                SDL_GetError());
        audio_shutdown(audio);
        return false;
    }
    if (!SDL_BindAudioStreams(audio->device, audio->voices, AUDIO_VOICE_COUNT))
    {
        SDL_Log("Audio disabled: could not bind voices: %s", SDL_GetError());
        audio_shutdown(audio);
        return false;
    }
    if (!SDL_BindAudioStream(audio->device, audio->music_stream))
    {
        SDL_Log("Audio disabled: could not bind music stream: %s",
                SDL_GetError());
        audio_shutdown(audio);
        return false;
    }

    for (int i = 0; i < SFX_COUNT; ++i)
    {
        if (!synth_sound(audio, (SoundEffect)i))
        {
            SDL_Log("Audio disabled: could not build effect cache");
            audio_shutdown(audio);
            return false;
        }
    }
    if (!synth_music(audio))
    {
        SDL_Log("Audio disabled: could not build music cache");
        audio_shutdown(audio);
        return false;
    }

    SDL_SetAudioDeviceGain(audio->device, AUDIO_MASTER_GAIN);
    SDL_ResumeAudioDevice(audio->device);
    audio->ready = true;
    return true;
}

void audio_shutdown(AudioSystem *audio)
{
    free_cache(audio);

    if (audio->music_stream != NULL)
    {
        SDL_UnbindAudioStream(audio->music_stream);
        SDL_DestroyAudioStream(audio->music_stream);
        audio->music_stream = NULL;
    }
    SDL_UnbindAudioStreams(audio->voices, AUDIO_VOICE_COUNT);
    for (int i = 0; i < AUDIO_VOICE_COUNT; ++i)
    {
        if (audio->voices[i] != NULL)
        {
            SDL_DestroyAudioStream(audio->voices[i]);
            audio->voices[i] = NULL;
        }
    }
    if (audio->device != 0)
    {
        SDL_CloseAudioDevice(audio->device);
        audio->device = 0;
    }
    if (audio->subsystem_initialized)
    {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        audio->subsystem_initialized = false;
    }
    audio->ready = false;
    audio->current_music = -1;
}

static void play_scaled(AudioSystem *audio, SoundEffect effect, float scale)
{
    if (!audio->ready || audio->muted || effect < 0 || effect >= SFX_COUNT)
        return;

    CachedSound *sound = &audio->sounds[effect];
    Uint64 now = SDL_GetTicksNS();
    Uint64 gap_ns = (Uint64)sound->min_gap_ms * 1000000u;
    if (audio->last_play_ns[effect] != 0 &&
        now - audio->last_play_ns[effect] < gap_ns)
    {
        return;
    }

    int active_voices = 0;
    for (int i = 0; i < AUDIO_VOICE_COUNT; ++i)
    {
        if (now < audio->voice_end_ns[i])
            ++active_voices;
    }

    int voice = -1;
    for (int offset = 0; offset < AUDIO_VOICE_COUNT; ++offset)
    {
        int candidate = (audio->next_voice + offset) % AUDIO_VOICE_COUNT;
        if (now >= audio->voice_end_ns[candidate])
        {
            voice = candidate;
            break;
        }
    }
    if (voice < 0)
    {
        voice = 0;
        for (int i = 1; i < AUDIO_VOICE_COUNT; ++i)
        {
            if (audio->voice_end_ns[i] < audio->voice_end_ns[voice])
                voice = i;
        }
    }

    SDL_AudioStream *stream = audio->voices[voice];
    SDL_ClearAudioStream(stream);
    /*
     * Give busy scenes extra mix headroom. This prevents shots, impacts and
     * movement sounds from stacking into a single aggressive wall of sound.
     */
    float mix_headroom = 1.0f / sqrtf(1.0f + active_voices * 0.22f);
    SDL_SetAudioStreamGain(stream, sound->gain *
                                      clampf(scale, 0.0f, 1.0f) *
                                      mix_headroom);
    if (!SDL_PutAudioStreamData(stream, sound->samples,
                                sound->frame_count * (int)sizeof(float)))
    {
        SDL_Log("Could not queue sound effect: %s", SDL_GetError());
        return;
    }
    SDL_FlushAudioStream(stream);

    audio->last_play_ns[effect] = now;
    audio->voice_end_ns[voice] =
        now + ((Uint64)sound->frame_count * 1000000000u) / AUDIO_SAMPLE_RATE;
    audio->next_voice = (voice + 1) % AUDIO_VOICE_COUNT;
}

void audio_play(AudioSystem *audio, SoundEffect effect)
{
    play_scaled(audio, effect, 1.0f);
}

static bool sound_is_critical_feedback(SoundEffect effect)
{
    switch (effect)
    {
    case SFX_EXPLOSION:
    case SFX_CRATE_LAND:
    case SFX_CRATE_BREAK:
    case SFX_ENEMY_DOWN:
    case SFX_DOG_YELP:
        return true;
    default:
        return false;
    }
}

void audio_play_at(AudioSystem *audio, SoundEffect effect,
                   float source_x, float source_y,
                   float listener_x, float listener_y)
{
    const float full_volume_distance = 2.0f * TILE_SIZE;
    bool critical_feedback = sound_is_critical_feedback(effect);
    float max_audible_distance =
        (critical_feedback ? 24.0f : 16.0f) * TILE_SIZE;
    float dx = source_x - listener_x;
    float dy = source_y - listener_y;
    float distance = sqrtf(dx * dx + dy * dy);

    /*
     * Nearby events remain crisp. Routine action falls off quickly so
     * off-screen chatter does not mask local cues. Outcome-defining impacts
     * carry farther and use a gentler curve: a crate kill or explosion below
     * the player still provides audible confirmation.
     */
    if (distance >= max_audible_distance)
        return;

    float normalized =
        clampf((distance - full_volume_distance) /
                   (max_audible_distance - full_volume_distance),
               0.0f, 1.0f);
    float proximity = 1.0f - normalized;
    float scale = critical_feedback ? proximity : proximity * proximity;
    play_scaled(audio, effect, scale);
}

static bool queue_music_loop(AudioSystem *audio)
{
    if (audio->current_music < 0 ||
        audio->current_music >= MUSIC_TRACK_COUNT)
        return false;

    CachedSound *track = &audio->music_tracks[audio->current_music];
    if (!SDL_PutAudioStreamData(audio->music_stream, track->samples,
                                track->frame_count * (int)sizeof(float)))
    {
        SDL_Log("Could not queue music: %s", SDL_GetError());
        return false;
    }
    return true;
}

void audio_play_music(AudioSystem *audio, MusicTrack track)
{
    int track_index = (int)track;
    if (!audio->ready || audio->music_stream == NULL ||
        track_index < 0 || track_index >= MUSIC_TRACK_COUNT)
        return;
    if (audio->current_music == track_index)
        return;

    SDL_ClearAudioStream(audio->music_stream);
    audio->current_music = track_index;
    SDL_SetAudioStreamGain(audio->music_stream,
                           audio->music_tracks[track_index].gain);
    queue_music_loop(audio);
}

void audio_stop_music(AudioSystem *audio)
{
    if (audio->music_stream != NULL)
        SDL_ClearAudioStream(audio->music_stream);
    audio->current_music = -1;
}

void audio_stop_effects(AudioSystem *audio)
{
    for (int i = 0; i < AUDIO_VOICE_COUNT; ++i)
    {
        if (audio->voices[i] != NULL)
            SDL_ClearAudioStream(audio->voices[i]);
        audio->voice_end_ns[i] = 0;
    }
}

void audio_update_music(AudioSystem *audio)
{
    if (!audio->ready || audio->music_stream == NULL ||
        audio->current_music < 0 ||
        audio->current_music >= MUSIC_TRACK_COUNT)
        return;

    CachedSound *track = &audio->music_tracks[audio->current_music];
    int queued = SDL_GetAudioStreamQueued(audio->music_stream);
    int half_loop_bytes = track->frame_count * (int)sizeof(float) / 2;
    if (queued >= 0 && queued < half_loop_bytes)
        queue_music_loop(audio);

    /* Briefly make space for dense action without abruptly muting the score. */
    Uint64 now = SDL_GetTicksNS();
    int active_voices = 0;
    for (int i = 0; i < AUDIO_VOICE_COUNT; ++i)
    {
        if (now < audio->voice_end_ns[i])
            ++active_voices;
    }
    float duck = 1.0f / (1.0f + 0.10f * (float)active_voices);
    SDL_SetAudioStreamGain(audio->music_stream, track->gain * duck);
}

void audio_toggle_mute(AudioSystem *audio)
{
    audio->muted = !audio->muted;
    if (audio->device != 0)
        SDL_SetAudioDeviceGain(audio->device,
                               audio->muted ? 0.0f : AUDIO_MASTER_GAIN);
}
