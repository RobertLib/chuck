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

/* ---- Themed level scores -------------------------------------------- */

/*
 * Every level theme names its own score, and eighteen hand-sequenced synth
 * routines would be eighteen places to keep in tune with one another. A track
 * is therefore described rather than written out: a key, a tempo, a set of
 * 1/16 rhythms and a colour, which `synth_music_plan` turns into PCM. A new
 * sector's music is a table row beside its palette, not another function.
 *
 * Rhythms are sixteen 1/16 steps written as four beats, earliest step in the
 * low bit of each beat: MUSIC_BEATS(0x1, 0x1, 0x1, 0x1) is four on the floor,
 * 0x5 is straight eighths, 0x4 is the offbeat on its own, 0xF is sixteenths.
 */
#define MUSIC_BEATS(one, two, three, four)                                   \
    ((Uint16)((one) | ((two) << 4) | ((three) << 8) | ((four) << 12)))
#define MUSIC_BAR(index) (1u << (index))

/* The one detail of a track that is not a rhythm or a pitch: what the room
 * itself sounds like underneath the parts. */
typedef enum
{
    MUSIC_COLOUR_SWEEP = 1 << 0,   /* noise rise out of each section */
    MUSIC_COLOUR_CLANK = 1 << 1,   /* struck metal on an offbeat */
    MUSIC_COLOUR_SPARKLE = 1 << 2, /* high glints: glass, brass, stars */
    MUSIC_COLOUR_WIND = 1 << 3,    /* air moving past a wall */
    MUSIC_COLOUR_TICK = 1 << 4,    /* dry digital blip */
    MUSIC_COLOUR_DRIP = 1 << 5     /* a wet plink in a hard room */
} MusicColour;

typedef struct
{
    float bpm;
    int bars;               /* loop length; a multiple of four */
    float gain;             /* playback gain of the finished loop */
    int root;               /* MIDI note the progression is written against */
    const int *progression; /* one offset per bar, added to the root */
    bool minor;             /* chord quality of the pads, stabs and arps */
    float swing;            /* how late the offbeat eighth is, in 1/16 steps */

    Waveform bass_wave;
    float bass_gain;
    float bass_length;    /* in 1/16 steps */
    Uint16 bass_steps[2]; /* even bars, odd bars */

    int sub_note; /* absolute MIDI pedal; 0 follows the bar root, an octave down */
    float sub_gain;

    float pad_gain;
    int pad_offset;  /* semitones from the bar root */
    bool pad_halves; /* re-voice halfway through the bar */

    Uint16 kick_steps[2];
    Uint16 snare_steps[2];
    Uint16 hat_steps[2];
    float kick_gain;
    float snare_gain;
    float hat_gain;

    const int *lead_a; /* sixteen offsets from the bar root; -1 rests */
    const int *lead_b;
    Uint32 lead_bars; /* which bars of the loop carry the lead */
    float lead_gain;
    float lead_length;
    Waveform lead_wave;

    float arp_gain;  /* four-note figure on the beats of the full sections */
    float stab_gain; /* chord punches inside the full sections */
    Uint8 colours;
    Uint32 seed; /* keeps one track's noise from repeating another's */
} MusicPlan;

/* The drive: motorik saw bass and wet metal, unchanged in character from the
 * road music the pursuit shipped with. */
static const int PURSUIT_BARS[16] = {
    0, 1, -2, -5, 0, 3, 1, -2, 0, -4, -2, -5, 0, 1, 3, -2};
static const int PURSUIT_LEAD_A[16] = {
    24, -1, 27, -1, 25, -1, -1, 22, -1, 24, -1, 20, -1, 22, -1, -1};
static const int PURSUIT_LEAD_B[16] = {
    -1, 19, -1, 20, -1, 24, -1, -1, 25, -1, 24, -1, 22, -1, 20, -1};

/* The lobby: marble, brass and one lit street front. Nothing marches here —
 * no snare at all, a pad that holds the room and glints off the glazing. */
static const int LOBBY_BARS[12] = {0, 0, -4, 3, 0, 5, -2, -4, 0, 3, -2, -1};
static const int LOBBY_LEAD_A[16] = {
    -1, -1, 12, -1, -1, -1, 15, -1, -1, 19, -1, -1, 17, -1, -1, -1};
static const int LOBBY_LEAD_B[16] = {
    -1, 12, -1, -1, 16, -1, -1, -1, 19, -1, -1, 17, -1, -1, 15, -1};

/* The office floor: the workaday groove the campaign shipped with. */
static const int OFFICE_BARS[16] = {
    0, 0, 3, -2, 0, 5, 3, -2, 0, -4, -2, 3, 0, 5, -2, 0};
static const int OFFICE_LEAD_A[16] = {
    24, -1, -1, 27, -1, 31, -1, 29, -1, 27, -1, 24, -1, 22, -1, -1};
static const int OFFICE_LEAD_B[16] = {
    -1, 19, -1, 24, -1, -1, 27, -1, 31, -1, 29, -1, -1, 27, 24, -1};

/* The cold aisle: two-bar chords that barely move, a machine pulse instead of
 * a backbeat, and sixteenths where a melody would be. */
static const int SERVER_BARS[16] = {
    0, 0, 3, 3, -2, -2, 5, 5, 0, 0, 3, 3, -4, -4, -1, -1};
static const int SERVER_LEAD_A[16] = {
    12, -1, 19, -1, 15, -1, 22, -1, 12, -1, 19, -1, 24, -1, 19, -1};
static const int SERVER_LEAD_B[16] = {
    -1, 19, -1, 15, -1, 12, -1, 19, -1, 22, -1, 19, -1, 24, -1, 15};

/* The machine floor: half-time snare, a saw on the beats and something
 * heavy being struck off in the dark. */
static const int PLANT_BARS[16] = {
    0, 0, 3, 1, 0, -4, 3, 1, 0, 0, 5, 3, 0, 1, -2, 0};
static const int PLANT_LEAD_A[16] = {
    12, -1, -1, -1, 15, -1, -1, -1, -1, -1, 19, -1, -1, -1, 17, -1};
static const int PLANT_LEAD_B[16] = {
    -1, -1, 19, -1, -1, 17, -1, -1, 15, -1, -1, -1, 12, -1, -1, -1};

/* The galley: the warmest sector, so the only major key inside the building
 * and the only swung one. */
static const int CANTEEN_BARS[12] = {0, 5, 7, 0, -3, 5, 2, 7, 0, 5, -1, 0};
static const int CANTEEN_LEAD_A[16] = {
    16, -1, 19, -1, -1, 21, -1, 19, -1, 16, -1, -1, 12, -1, -1, -1};
static const int CANTEEN_LEAD_B[16] = {
    -1, 12, -1, 16, -1, -1, 19, -1, 21, -1, 19, -1, -1, 16, 12, -1};

/* The lab: a chromatic wobble that never settles, and a tritone in the lead
 * where the answering note should be. */
static const int LAB_BARS[12] = {0, 0, -1, 1, 0, 6, -1, 0, 0, 3, 1, -1};
static const int LAB_LEAD_A[16] = {
    -1, -1, 15, -1, -1, -1, 18, -1, -1, -1, -1, 14, -1, -1, -1, -1};
static const int LAB_LEAD_B[16] = {
    -1, 18, -1, -1, -1, 15, -1, -1, -1, -1, 20, -1, -1, -1, 15, -1};

/* The shelving canyons: slow, dusty and nearly empty. One soft pulse a bar is
 * the whole rhythm section. */
static const int ARCHIVE_BARS[8] = {0, 0, -2, 3, 0, -4, -2, 1};
static const int ARCHIVE_LEAD_A[16] = {
    -1, -1, -1, -1, 12, -1, -1, -1, -1, -1, 15, -1, -1, -1, -1, -1};
static const int ARCHIVE_LEAD_B[16] = {
    -1, -1, 10, -1, -1, -1, -1, -1, 12, -1, -1, -1, -1, -1, -1, -1};

/* The ring around the bunker: a march. Four on the floor, a snare that rolls
 * into the bar line and a static tonic pushed by one chromatic step. */
static const int SECURITY_BARS[16] = {
    0, 0, 0, -1, 0, 0, 3, 5, 0, 0, 0, -1, -4, -4, -2, -1};
static const int SECURITY_LEAD_A[16] = {
    12, -1, 12, -1, 15, -1, -1, -1, 17, -1, 15, -1, 12, -1, -1, -1};
static const int SECURITY_LEAD_B[16] = {
    -1, -1, 19, -1, 17, -1, 15, -1, -1, 12, -1, -1, 15, -1, -1, -1};

/* The crawl ducts: a fan pedal that never stops, air moving past the plating
 * and almost nothing on top of it. */
static const int DUCTS_BARS[12] = {0, 0, 1, 0, -2, -2, 1, 0, 0, 3, 1, -1};
static const int DUCTS_LEAD_A[16] = {
    -1, -1, -1, 12, -1, -1, -1, -1, 13, -1, -1, -1, -1, -1, 10, -1};
static const int DUCTS_LEAD_B[16] = {
    -1, 10, -1, -1, -1, -1, 13, -1, -1, -1, 12, -1, -1, -1, -1, -1};

/* The suite: the widest chords in the building, and the first track allowed
 * to punch them. */
static const int PENTHOUSE_BARS[16] = {
    0, 0, -4, 3, 0, 5, -2, -4, 0, -1, 3, 5, 0, -4, -2, -1};
static const int PENTHOUSE_LEAD_A[16] = {
    -1, -1, 12, -1, -1, 15, -1, -1, 19, -1, -1, 17, -1, -1, 15, -1};
static const int PENTHOUSE_LEAD_B[16] = {
    19, -1, -1, 17, -1, -1, 15, -1, -1, 12, -1, -1, 15, -1, -1, -1};

/* The skyline: the last sector, so the lead finally carries the loop instead
 * of answering it, over the wind of an open roof. */
static const int ROOF_BARS[16] = {
    0, 0, 3, 5, -2, -2, 3, 7, 0, 0, 5, 3, -4, -4, -2, 0};
static const int ROOF_LEAD_A[16] = {
    12, -1, -1, 15, -1, 19, -1, -1, 22, -1, 19, -1, -1, 17, -1, -1};
static const int ROOF_LEAD_B[16] = {
    -1, 19, -1, 22, -1, -1, 24, -1, 22, -1, 19, -1, 17, -1, 15, -1};

/* The restroom: a room to stop breathing hard in. Tiled, dripping, no kit. */
static const int RESTROOM_BARS[8] = {0, 0, -2, 1, 0, 3, -4, -1};
static const int RESTROOM_LEAD_A[16] = {
    -1, -1, -1, -1, -1, -1, 12, -1, -1, -1, -1, -1, -1, -1, -1, -1};
static const int RESTROOM_LEAD_B[16] = {
    -1, -1, 15, -1, -1, -1, -1, -1, -1, -1, 12, -1, -1, -1, -1, -1};

/* The night wall: wide, slow and exposed, with the city glittering under it. */
static const int FACADE_NIGHT_BARS[12] = {
    0, 0, 3, -2, 0, 5, 3, -2, 0, -4, -1, 0};
static const int FACADE_NIGHT_LEAD_A[16] = {
    -1, -1, 12, -1, -1, -1, -1, 19, -1, -1, 17, -1, -1, -1, 15, -1};
static const int FACADE_NIGHT_LEAD_B[16] = {
    -1, 15, -1, -1, 19, -1, -1, -1, -1, 17, -1, -1, 12, -1, -1, -1};

/* The storm wall: the only climb in a hurry. Rain on the cornices and a
 * bass line that never lets go. */
static const int FACADE_STORM_BARS[16] = {
    0, 0, 1, -2, 0, 5, 1, -4, 0, 3, -2, 1, 0, 5, 3, -1};
static const int FACADE_STORM_LEAD_A[16] = {
    12, -1, -1, 13, -1, -1, 15, -1, -1, 17, -1, 15, -1, -1, 13, -1};
static const int FACADE_STORM_LEAD_B[16] = {
    -1, 17, -1, 15, -1, 13, -1, 12, -1, -1, 15, -1, 13, -1, -1, -1};

/* The dawn wall: the one climb with the light coming, and the only major key
 * outside the building. */
static const int FACADE_DAWN_BARS[12] = {0, 0, 5, 7, 0, -3, 5, 2, 0, 7, 5, 0};
static const int FACADE_DAWN_LEAD_A[16] = {
    -1, -1, 12, -1, -1, 16, -1, -1, 19, -1, -1, -1, 16, -1, -1, -1};
static const int FACADE_DAWN_LEAD_B[16] = {
    -1, 19, -1, -1, 21, -1, -1, 19, -1, 16, -1, -1, 12, -1, -1, -1};

/* The high wall: thin air. Barely a bass note, barely a beat, and a long way
 * down between the phrases. */
static const int FACADE_HIGH_BARS[8] = {0, 0, -2, 3, 0, -4, -1, 0};
static const int FACADE_HIGH_LEAD_A[16] = {
    -1, -1, -1, -1, 12, -1, -1, -1, -1, -1, -1, -1, 19, -1, -1, -1};
static const int FACADE_HIGH_LEAD_B[16] = {
    -1, -1, 19, -1, -1, -1, -1, -1, 15, -1, -1, -1, -1, -1, -1, -1};

/*
 * One row per track. MUSIC_INTRO is deliberately absent: the title theme is
 * hand-sequenced below, and a plan with no progression means "not planned".
 */
static const MusicPlan MUSIC_PLANS[MUSIC_TRACK_COUNT] = {
    [MUSIC_PURSUIT] = {
        .bpm = 112.0f, .bars = 16, .gain = 0.225f,
        .root = 38, .progression = PURSUIT_BARS, .minor = true,
        .bass_wave = WAVE_SAW, .bass_gain = 0.18f, .bass_length = 1.4f,
        .bass_steps = {MUSIC_BEATS(0x5, 0x2, 0x9, 0x4),
                       MUSIC_BEATS(0x9, 0x4, 0x2, 0x5)},
        .sub_note = 26, .sub_gain = 0.052f,
        .pad_gain = 0.021f, .pad_offset = 12, .pad_halves = true,
        .kick_steps = {MUSIC_BEATS(0x1, 0x4, 0x8, 0x0),
                       MUSIC_BEATS(0x1, 0x4, 0x8, 0x4)},
        .snare_steps = {MUSIC_BEATS(0x0, 0x1, 0x0, 0x1),
                        MUSIC_BEATS(0x0, 0x1, 0x0, 0x1)},
        .hat_steps = {MUSIC_BEATS(0x2, 0x2, 0x2, 0x2),
                      MUSIC_BEATS(0x2, 0x2, 0x2, 0x2)},
        .kick_gain = 0.32f, .snare_gain = 0.16f, .hat_gain = 0.041f,
        .lead_a = PURSUIT_LEAD_A, .lead_b = PURSUIT_LEAD_B,
        .lead_bars = MUSIC_BAR(1) | MUSIC_BAR(3) | MUSIC_BAR(5) |
                     MUSIC_BAR(6) | MUSIC_BAR(12) | MUSIC_BAR(14) |
                     MUSIC_BAR(15),
        .lead_gain = 0.057f, .lead_length = 0.72f, .lead_wave = WAVE_SQUARE,
        .stab_gain = 0.040f,
        .colours = MUSIC_COLOUR_CLANK | MUSIC_COLOUR_SWEEP,
        .seed = 0x8200u},

    [MUSIC_LOBBY] = {
        .bpm = 88.0f, .bars = 12, .gain = 0.200f,
        .root = 45, .progression = LOBBY_BARS, .minor = true,
        .bass_wave = WAVE_TRIANGLE, .bass_gain = 0.14f, .bass_length = 2.8f,
        .bass_steps = {MUSIC_BEATS(0x1, 0x0, 0x1, 0x0),
                       MUSIC_BEATS(0x1, 0x0, 0x4, 0x0)},
        .sub_gain = 0.044f,
        .pad_gain = 0.032f, .pad_offset = 12,
        .kick_steps = {MUSIC_BEATS(0x1, 0x0, 0x0, 0x0),
                       MUSIC_BEATS(0x1, 0x0, 0x4, 0x0)},
        .hat_steps = {MUSIC_BEATS(0x0, 0x4, 0x0, 0x4),
                      MUSIC_BEATS(0x0, 0x4, 0x0, 0x4)},
        .kick_gain = 0.20f, .hat_gain = 0.030f,
        .lead_a = LOBBY_LEAD_A, .lead_b = LOBBY_LEAD_B,
        .lead_bars = MUSIC_BAR(2) | MUSIC_BAR(3) | MUSIC_BAR(6) |
                     MUSIC_BAR(7) | MUSIC_BAR(10) | MUSIC_BAR(11),
        .lead_gain = 0.048f, .lead_length = 2.6f, .lead_wave = WAVE_TRIANGLE,
        .colours = MUSIC_COLOUR_SPARKLE,
        .seed = 0x1300u},

    [MUSIC_OFFICE] = {
        .bpm = 96.0f, .bars = 16, .gain = 0.235f,
        .root = 40, .progression = OFFICE_BARS, .minor = true,
        .bass_wave = WAVE_SQUARE, .bass_gain = 0.19f, .bass_length = 1.7f,
        .bass_steps = {MUSIC_BEATS(0x9, 0x4, 0x4, 0x4),
                       MUSIC_BEATS(0x5, 0x2, 0x9, 0x4)},
        .sub_gain = 0.048f,
        .pad_gain = 0.027f, .pad_offset = 12,
        .kick_steps = {MUSIC_BEATS(0x1, 0x0, 0x4, 0x0),
                       MUSIC_BEATS(0x1, 0x4, 0x4, 0x0)},
        .snare_steps = {MUSIC_BEATS(0x0, 0x1, 0x0, 0x1),
                        MUSIC_BEATS(0x0, 0x1, 0x0, 0x1)},
        .hat_steps = {MUSIC_BEATS(0x4, 0x4, 0x4, 0x4),
                      MUSIC_BEATS(0x4, 0x4, 0x4, 0x4)},
        .kick_gain = 0.33f, .snare_gain = 0.14f, .hat_gain = 0.055f,
        .lead_a = OFFICE_LEAD_A, .lead_b = OFFICE_LEAD_B,
        .lead_bars = MUSIC_BAR(1) | MUSIC_BAR(3) | MUSIC_BAR(5) |
                     MUSIC_BAR(7) | MUSIC_BAR(9) | MUSIC_BAR(11) |
                     MUSIC_BAR(12) | MUSIC_BAR(13) | MUSIC_BAR(14) |
                     MUSIC_BAR(15),
        .lead_gain = 0.054f, .lead_length = 1.1f, .lead_wave = WAVE_TRIANGLE,
        .arp_gain = 0.038f, .stab_gain = 0.036f,
        .colours = MUSIC_COLOUR_SWEEP,
        .seed = 0x4100u},

    [MUSIC_SERVER] = {
        .bpm = 126.0f, .bars = 16, .gain = 0.215f,
        .root = 35, .progression = SERVER_BARS, .minor = true,
        .bass_wave = WAVE_SQUARE, .bass_gain = 0.17f, .bass_length = 1.0f,
        .bass_steps = {MUSIC_BEATS(0x5, 0x5, 0x5, 0x5),
                       MUSIC_BEATS(0x5, 0x5, 0x9, 0x5)},
        .sub_note = 35, .sub_gain = 0.050f,
        .pad_gain = 0.018f, .pad_offset = 24,
        .kick_steps = {MUSIC_BEATS(0x1, 0x0, 0x1, 0x0),
                       MUSIC_BEATS(0x1, 0x0, 0x1, 0x0)},
        .snare_steps = {MUSIC_BEATS(0x0, 0x0, 0x0, 0x1),
                        MUSIC_BEATS(0x0, 0x0, 0x0, 0x1)},
        .hat_steps = {MUSIC_BEATS(0x0, 0xF, 0x0, 0xF),
                      MUSIC_BEATS(0x0, 0xF, 0x0, 0xF)},
        .kick_gain = 0.26f, .snare_gain = 0.09f, .hat_gain = 0.030f,
        .lead_a = SERVER_LEAD_A, .lead_b = SERVER_LEAD_B,
        .lead_bars = MUSIC_BAR(1) | MUSIC_BAR(2) | MUSIC_BAR(3) |
                     MUSIC_BAR(5) | MUSIC_BAR(6) | MUSIC_BAR(7) |
                     MUSIC_BAR(9) | MUSIC_BAR(10) | MUSIC_BAR(11) |
                     MUSIC_BAR(13) | MUSIC_BAR(14) | MUSIC_BAR(15),
        .lead_gain = 0.045f, .lead_length = 0.60f, .lead_wave = WAVE_SQUARE,
        .colours = MUSIC_COLOUR_TICK,
        .seed = 0x2600u},

    [MUSIC_PLANT] = {
        .bpm = 104.0f, .bars = 16, .gain = 0.230f,
        .root = 33, .progression = PLANT_BARS, .minor = true,
        .bass_wave = WAVE_SAW, .bass_gain = 0.20f, .bass_length = 2.0f,
        .bass_steps = {MUSIC_BEATS(0x1, 0x1, 0x1, 0x9),
                       MUSIC_BEATS(0x1, 0x9, 0x1, 0x1)},
        .sub_gain = 0.058f,
        .pad_gain = 0.022f, .pad_offset = 12,
        .kick_steps = {MUSIC_BEATS(0x1, 0x0, 0x1, 0x0),
                       MUSIC_BEATS(0x1, 0x0, 0x1, 0x8)},
        .snare_steps = {MUSIC_BEATS(0x0, 0x0, 0x1, 0x0),
                        MUSIC_BEATS(0x0, 0x0, 0x1, 0x0)},
        .hat_steps = {MUSIC_BEATS(0x0, 0x4, 0x0, 0x4),
                      MUSIC_BEATS(0x0, 0x4, 0x0, 0x4)},
        .kick_gain = 0.34f, .snare_gain = 0.19f, .hat_gain = 0.048f,
        .lead_a = PLANT_LEAD_A, .lead_b = PLANT_LEAD_B,
        .lead_bars = MUSIC_BAR(2) | MUSIC_BAR(3) | MUSIC_BAR(6) |
                     MUSIC_BAR(7) | MUSIC_BAR(10) | MUSIC_BAR(11) |
                     MUSIC_BAR(14) | MUSIC_BAR(15),
        .lead_gain = 0.050f, .lead_length = 1.8f, .lead_wave = WAVE_TRIANGLE,
        .colours = MUSIC_COLOUR_CLANK | MUSIC_COLOUR_SWEEP,
        .seed = 0x3400u},

    [MUSIC_CANTEEN] = {
        .bpm = 108.0f, .bars = 12, .gain = 0.215f,
        .root = 43, .progression = CANTEEN_BARS, .minor = false,
        .swing = 0.28f,
        .bass_wave = WAVE_TRIANGLE, .bass_gain = 0.16f, .bass_length = 1.2f,
        .bass_steps = {MUSIC_BEATS(0x1, 0x4, 0x1, 0x4),
                       MUSIC_BEATS(0x1, 0x4, 0x5, 0x0)},
        .sub_gain = 0.038f,
        .pad_gain = 0.024f, .pad_offset = 12,
        .kick_steps = {MUSIC_BEATS(0x1, 0x0, 0x1, 0x0),
                       MUSIC_BEATS(0x1, 0x0, 0x1, 0x0)},
        .snare_steps = {MUSIC_BEATS(0x0, 0x1, 0x0, 0x1),
                        MUSIC_BEATS(0x0, 0x1, 0x0, 0x1)},
        .hat_steps = {MUSIC_BEATS(0x5, 0x5, 0x5, 0x5),
                      MUSIC_BEATS(0x5, 0x5, 0x5, 0x5)},
        .kick_gain = 0.26f, .snare_gain = 0.13f, .hat_gain = 0.036f,
        .lead_a = CANTEEN_LEAD_A, .lead_b = CANTEEN_LEAD_B,
        .lead_bars = MUSIC_BAR(1) | MUSIC_BAR(2) | MUSIC_BAR(4) |
                     MUSIC_BAR(5) | MUSIC_BAR(7) | MUSIC_BAR(8) |
                     MUSIC_BAR(10) | MUSIC_BAR(11),
        .lead_gain = 0.052f, .lead_length = 1.0f, .lead_wave = WAVE_TRIANGLE,
        .arp_gain = 0.032f,
        .colours = MUSIC_COLOUR_SPARKLE,
        .seed = 0x5500u},

    [MUSIC_LAB] = {
        .bpm = 92.0f, .bars = 12, .gain = 0.205f,
        .root = 36, .progression = LAB_BARS, .minor = true,
        .bass_wave = WAVE_TRIANGLE, .bass_gain = 0.15f, .bass_length = 2.4f,
        .bass_steps = {MUSIC_BEATS(0x1, 0x0, 0x2, 0x0),
                       MUSIC_BEATS(0x1, 0x0, 0x0, 0x2)},
        .sub_gain = 0.056f,
        .pad_gain = 0.026f, .pad_offset = 12, .pad_halves = true,
        .kick_steps = {MUSIC_BEATS(0x1, 0x0, 0x0, 0x0),
                       MUSIC_BEATS(0x1, 0x0, 0x0, 0x8)},
        .snare_steps = {MUSIC_BEATS(0x0, 0x0, 0x0, 0x1),
                        MUSIC_BEATS(0x0, 0x0, 0x0, 0x1)},
        .hat_steps = {MUSIC_BEATS(0x0, 0x2, 0x0, 0x2),
                      MUSIC_BEATS(0x0, 0x2, 0x0, 0x2)},
        .kick_gain = 0.22f, .snare_gain = 0.12f, .hat_gain = 0.034f,
        .lead_a = LAB_LEAD_A, .lead_b = LAB_LEAD_B,
        .lead_bars = MUSIC_BAR(1) | MUSIC_BAR(3) | MUSIC_BAR(4) |
                     MUSIC_BAR(7) | MUSIC_BAR(9) | MUSIC_BAR(10),
        .lead_gain = 0.046f, .lead_length = 1.6f, .lead_wave = WAVE_SINE,
        .colours = MUSIC_COLOUR_DRIP | MUSIC_COLOUR_TICK,
        .seed = 0x6700u},

    [MUSIC_ARCHIVE] = {
        .bpm = 76.0f, .bars = 8, .gain = 0.190f,
        .root = 38, .progression = ARCHIVE_BARS, .minor = true,
        .bass_wave = WAVE_TRIANGLE, .bass_gain = 0.13f, .bass_length = 3.0f,
        .bass_steps = {MUSIC_BEATS(0x1, 0x0, 0x0, 0x0),
                       MUSIC_BEATS(0x1, 0x0, 0x1, 0x0)},
        .sub_gain = 0.050f,
        .pad_gain = 0.030f, .pad_offset = 12,
        .kick_steps = {MUSIC_BEATS(0x1, 0x0, 0x0, 0x0),
                       MUSIC_BEATS(0x1, 0x0, 0x0, 0x0)},
        .hat_steps = {MUSIC_BEATS(0x0, 0x0, 0x0, 0x4),
                      MUSIC_BEATS(0x0, 0x0, 0x0, 0x4)},
        .kick_gain = 0.16f, .hat_gain = 0.024f,
        .lead_a = ARCHIVE_LEAD_A, .lead_b = ARCHIVE_LEAD_B,
        .lead_bars = MUSIC_BAR(2) | MUSIC_BAR(3) | MUSIC_BAR(6) |
                     MUSIC_BAR(7),
        .lead_gain = 0.040f, .lead_length = 3.0f, .lead_wave = WAVE_SINE,
        .colours = MUSIC_COLOUR_DRIP | MUSIC_COLOUR_SPARKLE,
        .seed = 0x7800u},

    [MUSIC_SECURITY] = {
        .bpm = 128.0f, .bars = 16, .gain = 0.235f,
        .root = 41, .progression = SECURITY_BARS, .minor = true,
        .bass_wave = WAVE_SQUARE, .bass_gain = 0.20f, .bass_length = 0.9f,
        .bass_steps = {MUSIC_BEATS(0xF, 0x5, 0xF, 0x5),
                       MUSIC_BEATS(0xF, 0x5, 0x5, 0xD)},
        .sub_gain = 0.050f,
        .pad_gain = 0.016f, .pad_offset = 12,
        .kick_steps = {MUSIC_BEATS(0x1, 0x1, 0x1, 0x1),
                       MUSIC_BEATS(0x1, 0x1, 0x1, 0x9)},
        .snare_steps = {MUSIC_BEATS(0x0, 0x1, 0x0, 0x1),
                        MUSIC_BEATS(0x0, 0x1, 0x0, 0x7)},
        .hat_steps = {MUSIC_BEATS(0x5, 0x5, 0x5, 0x5),
                      MUSIC_BEATS(0x5, 0x5, 0x5, 0x5)},
        .kick_gain = 0.32f, .snare_gain = 0.20f, .hat_gain = 0.038f,
        .lead_a = SECURITY_LEAD_A, .lead_b = SECURITY_LEAD_B,
        .lead_bars = MUSIC_BAR(1) | MUSIC_BAR(3) | MUSIC_BAR(5) |
                     MUSIC_BAR(7) | MUSIC_BAR(11) | MUSIC_BAR(12) |
                     MUSIC_BAR(13) | MUSIC_BAR(14) | MUSIC_BAR(15),
        .lead_gain = 0.058f, .lead_length = 1.0f, .lead_wave = WAVE_SQUARE,
        .arp_gain = 0.030f,
        .colours = MUSIC_COLOUR_SWEEP,
        .seed = 0x9900u},

    [MUSIC_DUCTS] = {
        .bpm = 84.0f, .bars = 12, .gain = 0.200f,
        .root = 34, .progression = DUCTS_BARS, .minor = true,
        .bass_wave = WAVE_TRIANGLE, .bass_gain = 0.14f, .bass_length = 2.6f,
        .bass_steps = {MUSIC_BEATS(0x1, 0x0, 0x0, 0x2),
                       MUSIC_BEATS(0x1, 0x0, 0x2, 0x0)},
        .sub_note = 29, .sub_gain = 0.060f,
        .pad_gain = 0.020f, .pad_offset = 12,
        .kick_steps = {MUSIC_BEATS(0x1, 0x0, 0x0, 0x0),
                       MUSIC_BEATS(0x1, 0x0, 0x1, 0x0)},
        .hat_steps = {MUSIC_BEATS(0x0, 0x0, 0x4, 0x0),
                      MUSIC_BEATS(0x0, 0x0, 0x4, 0x0)},
        .kick_gain = 0.20f, .hat_gain = 0.026f,
        .lead_a = DUCTS_LEAD_A, .lead_b = DUCTS_LEAD_B,
        .lead_bars = MUSIC_BAR(2) | MUSIC_BAR(3) | MUSIC_BAR(7) |
                     MUSIC_BAR(8) | MUSIC_BAR(11),
        .lead_gain = 0.042f, .lead_length = 2.0f, .lead_wave = WAVE_SINE,
        .colours = MUSIC_COLOUR_WIND | MUSIC_COLOUR_TICK,
        .seed = 0xab00u},

    [MUSIC_PENTHOUSE] = {
        .bpm = 100.0f, .bars = 16, .gain = 0.230f,
        .root = 46, .progression = PENTHOUSE_BARS, .minor = true,
        .bass_wave = WAVE_SQUARE, .bass_gain = 0.18f, .bass_length = 1.5f,
        .bass_steps = {MUSIC_BEATS(0x1, 0x1, 0x9, 0x4),
                       MUSIC_BEATS(0x9, 0x4, 0x1, 0x5)},
        .sub_gain = 0.052f,
        .pad_gain = 0.030f, .pad_offset = 0, .pad_halves = true,
        .kick_steps = {MUSIC_BEATS(0x1, 0x0, 0x4, 0x0),
                       MUSIC_BEATS(0x1, 0x4, 0x4, 0x0)},
        .snare_steps = {MUSIC_BEATS(0x0, 0x1, 0x0, 0x1),
                        MUSIC_BEATS(0x0, 0x1, 0x0, 0x1)},
        .hat_steps = {MUSIC_BEATS(0x4, 0x4, 0x4, 0x4),
                      MUSIC_BEATS(0x4, 0x4, 0x4, 0x4)},
        .kick_gain = 0.30f, .snare_gain = 0.17f, .hat_gain = 0.042f,
        .lead_a = PENTHOUSE_LEAD_A, .lead_b = PENTHOUSE_LEAD_B,
        .lead_bars = MUSIC_BAR(1) | MUSIC_BAR(3) | MUSIC_BAR(5) |
                     MUSIC_BAR(7) | MUSIC_BAR(9) | MUSIC_BAR(11) |
                     MUSIC_BAR(12) | MUSIC_BAR(13) | MUSIC_BAR(15),
        .lead_gain = 0.056f, .lead_length = 1.4f, .lead_wave = WAVE_TRIANGLE,
        .arp_gain = 0.030f, .stab_gain = 0.045f,
        .colours = MUSIC_COLOUR_SWEEP | MUSIC_COLOUR_SPARKLE,
        .seed = 0xbc00u},

    [MUSIC_ROOF] = {
        .bpm = 96.0f, .bars = 16, .gain = 0.240f,
        .root = 42, .progression = ROOF_BARS, .minor = true,
        .bass_wave = WAVE_SQUARE, .bass_gain = 0.19f, .bass_length = 1.6f,
        .bass_steps = {MUSIC_BEATS(0x1, 0x5, 0x1, 0x5),
                       MUSIC_BEATS(0x9, 0x1, 0x5, 0x9)},
        .sub_gain = 0.054f,
        .pad_gain = 0.034f, .pad_offset = 12,
        .kick_steps = {MUSIC_BEATS(0x1, 0x0, 0x1, 0x0),
                       MUSIC_BEATS(0x1, 0x0, 0x1, 0x8)},
        .snare_steps = {MUSIC_BEATS(0x0, 0x1, 0x0, 0x1),
                        MUSIC_BEATS(0x0, 0x1, 0x0, 0x5)},
        .hat_steps = {MUSIC_BEATS(0x5, 0x5, 0x5, 0x5),
                      MUSIC_BEATS(0x5, 0x5, 0x5, 0x5)},
        .kick_gain = 0.31f, .snare_gain = 0.18f, .hat_gain = 0.040f,
        .lead_a = ROOF_LEAD_A, .lead_b = ROOF_LEAD_B,
        .lead_bars = MUSIC_BAR(1) | MUSIC_BAR(2) | MUSIC_BAR(3) |
                     MUSIC_BAR(5) | MUSIC_BAR(6) | MUSIC_BAR(7) |
                     MUSIC_BAR(9) | MUSIC_BAR(10) | MUSIC_BAR(11) |
                     MUSIC_BAR(13) | MUSIC_BAR(14) | MUSIC_BAR(15),
        .lead_gain = 0.062f, .lead_length = 1.3f, .lead_wave = WAVE_SQUARE,
        .arp_gain = 0.036f, .stab_gain = 0.040f,
        .colours = MUSIC_COLOUR_SWEEP | MUSIC_COLOUR_SPARKLE |
                   MUSIC_COLOUR_WIND,
        .seed = 0xcd00u},

    [MUSIC_RESTROOM] = {
        .bpm = 72.0f, .bars = 8, .gain = 0.170f,
        .root = 39, .progression = RESTROOM_BARS, .minor = true,
        .bass_wave = WAVE_SINE, .bass_gain = 0.11f, .bass_length = 3.2f,
        .bass_steps = {MUSIC_BEATS(0x1, 0x0, 0x0, 0x0),
                       MUSIC_BEATS(0x1, 0x0, 0x1, 0x0)},
        .sub_gain = 0.046f,
        .pad_gain = 0.028f, .pad_offset = 12,
        .kick_steps = {MUSIC_BEATS(0x1, 0x0, 0x0, 0x0),
                       MUSIC_BEATS(0x1, 0x0, 0x0, 0x0)},
        .kick_gain = 0.12f,
        .lead_a = RESTROOM_LEAD_A, .lead_b = RESTROOM_LEAD_B,
        .lead_bars = MUSIC_BAR(2) | MUSIC_BAR(3) | MUSIC_BAR(6) |
                     MUSIC_BAR(7),
        .lead_gain = 0.038f, .lead_length = 3.5f, .lead_wave = WAVE_SINE,
        .colours = MUSIC_COLOUR_DRIP,
        .seed = 0xde00u},

    [MUSIC_FACADE_NIGHT] = {
        .bpm = 90.0f, .bars = 12, .gain = 0.210f,
        .root = 40, .progression = FACADE_NIGHT_BARS, .minor = true,
        .bass_wave = WAVE_TRIANGLE, .bass_gain = 0.15f, .bass_length = 2.2f,
        .bass_steps = {MUSIC_BEATS(0x1, 0x0, 0x1, 0x0),
                       MUSIC_BEATS(0x1, 0x0, 0x4, 0x0)},
        .sub_gain = 0.052f,
        .pad_gain = 0.032f, .pad_offset = 12, .pad_halves = true,
        .kick_steps = {MUSIC_BEATS(0x1, 0x0, 0x1, 0x0),
                       MUSIC_BEATS(0x1, 0x0, 0x1, 0x0)},
        .snare_steps = {MUSIC_BEATS(0x0, 0x0, 0x0, 0x1),
                        MUSIC_BEATS(0x0, 0x0, 0x0, 0x1)},
        .hat_steps = {MUSIC_BEATS(0x0, 0x4, 0x0, 0x4),
                      MUSIC_BEATS(0x0, 0x4, 0x0, 0x4)},
        .kick_gain = 0.24f, .snare_gain = 0.13f, .hat_gain = 0.032f,
        .lead_a = FACADE_NIGHT_LEAD_A, .lead_b = FACADE_NIGHT_LEAD_B,
        .lead_bars = MUSIC_BAR(2) | MUSIC_BAR(3) | MUSIC_BAR(6) |
                     MUSIC_BAR(7) | MUSIC_BAR(10) | MUSIC_BAR(11),
        .lead_gain = 0.050f, .lead_length = 2.0f, .lead_wave = WAVE_TRIANGLE,
        .colours = MUSIC_COLOUR_WIND | MUSIC_COLOUR_SPARKLE,
        .seed = 0xef00u},

    [MUSIC_FACADE_STORM] = {
        .bpm = 116.0f, .bars = 16, .gain = 0.235f,
        .root = 36, .progression = FACADE_STORM_BARS, .minor = true,
        .bass_wave = WAVE_SAW, .bass_gain = 0.19f, .bass_length = 1.1f,
        .bass_steps = {MUSIC_BEATS(0x5, 0x5, 0x5, 0xD),
                       MUSIC_BEATS(0x9, 0x5, 0x5, 0x5)},
        .sub_gain = 0.056f,
        .pad_gain = 0.020f, .pad_offset = 12,
        .kick_steps = {MUSIC_BEATS(0x1, 0x1, 0x1, 0x1),
                       MUSIC_BEATS(0x1, 0x1, 0x1, 0x9)},
        .snare_steps = {MUSIC_BEATS(0x0, 0x1, 0x0, 0x1),
                        MUSIC_BEATS(0x0, 0x1, 0x0, 0x1)},
        .hat_steps = {MUSIC_BEATS(0x5, 0x5, 0x5, 0x5),
                      MUSIC_BEATS(0x5, 0x5, 0x5, 0x5)},
        .kick_gain = 0.30f, .snare_gain = 0.18f, .hat_gain = 0.044f,
        .lead_a = FACADE_STORM_LEAD_A, .lead_b = FACADE_STORM_LEAD_B,
        .lead_bars = MUSIC_BAR(1) | MUSIC_BAR(2) | MUSIC_BAR(3) |
                     MUSIC_BAR(5) | MUSIC_BAR(6) | MUSIC_BAR(7) |
                     MUSIC_BAR(9) | MUSIC_BAR(11) | MUSIC_BAR(13) |
                     MUSIC_BAR(14) | MUSIC_BAR(15),
        .lead_gain = 0.056f, .lead_length = 0.90f, .lead_wave = WAVE_SQUARE,
        .colours = MUSIC_COLOUR_WIND | MUSIC_COLOUR_SWEEP |
                   MUSIC_COLOUR_CLANK,
        .seed = 0x10a00u},

    [MUSIC_FACADE_DAWN] = {
        .bpm = 82.0f, .bars = 12, .gain = 0.205f,
        .root = 45, .progression = FACADE_DAWN_BARS, .minor = false,
        .bass_wave = WAVE_TRIANGLE, .bass_gain = 0.14f, .bass_length = 2.4f,
        .bass_steps = {MUSIC_BEATS(0x1, 0x0, 0x1, 0x0),
                       MUSIC_BEATS(0x1, 0x0, 0x5, 0x0)},
        .sub_gain = 0.048f,
        .pad_gain = 0.034f, .pad_offset = 12,
        .kick_steps = {MUSIC_BEATS(0x1, 0x0, 0x1, 0x0),
                       MUSIC_BEATS(0x1, 0x0, 0x1, 0x0)},
        .snare_steps = {MUSIC_BEATS(0x0, 0x0, 0x0, 0x1),
                        MUSIC_BEATS(0x0, 0x0, 0x0, 0x1)},
        .hat_steps = {MUSIC_BEATS(0x0, 0x4, 0x0, 0x4),
                      MUSIC_BEATS(0x0, 0x4, 0x0, 0x4)},
        .kick_gain = 0.22f, .snare_gain = 0.12f, .hat_gain = 0.030f,
        .lead_a = FACADE_DAWN_LEAD_A, .lead_b = FACADE_DAWN_LEAD_B,
        .lead_bars = MUSIC_BAR(2) | MUSIC_BAR(3) | MUSIC_BAR(5) |
                     MUSIC_BAR(6) | MUSIC_BAR(9) | MUSIC_BAR(10) |
                     MUSIC_BAR(11),
        .lead_gain = 0.052f, .lead_length = 2.2f, .lead_wave = WAVE_TRIANGLE,
        .arp_gain = 0.030f,
        .colours = MUSIC_COLOUR_WIND | MUSIC_COLOUR_SPARKLE,
        .seed = 0x11b00u},

    [MUSIC_FACADE_HIGH] = {
        .bpm = 70.0f, .bars = 8, .gain = 0.180f,
        .root = 47, .progression = FACADE_HIGH_BARS, .minor = true,
        .bass_wave = WAVE_SINE, .bass_gain = 0.10f, .bass_length = 3.4f,
        .bass_steps = {MUSIC_BEATS(0x1, 0x0, 0x0, 0x0),
                       MUSIC_BEATS(0x1, 0x0, 0x0, 0x0)},
        .sub_gain = 0.040f,
        .pad_gain = 0.030f, .pad_offset = 12,
        .kick_steps = {MUSIC_BEATS(0x1, 0x0, 0x0, 0x0),
                       MUSIC_BEATS(0x1, 0x0, 0x0, 0x0)},
        .hat_steps = {MUSIC_BEATS(0x0, 0x0, 0x4, 0x0),
                      MUSIC_BEATS(0x0, 0x0, 0x4, 0x0)},
        .kick_gain = 0.14f, .hat_gain = 0.022f,
        .lead_a = FACADE_HIGH_LEAD_A, .lead_b = FACADE_HIGH_LEAD_B,
        .lead_bars = MUSIC_BAR(2) | MUSIC_BAR(3) | MUSIC_BAR(6) |
                     MUSIC_BAR(7),
        .lead_gain = 0.042f, .lead_length = 3.0f, .lead_wave = WAVE_SINE,
        .colours = MUSIC_COLOUR_WIND | MUSIC_COLOUR_SPARKLE,
        .seed = 0x12c00u},
};

/*
 * Turn one plan into PCM.
 *
 * The loop is read as four equal sections: a statement, a full one, a
 * breakdown that hands the bar to the pad and the drone, and a last one that
 * pushes hardest. Every track shares that arc so a level's music develops
 * rather than repeating a bar, and the plan decides what the arc is made of.
 */
static bool synth_music_plan(CachedSound *track, const MusicPlan *plan)
{
    static const int ARP_MINOR[4] = {12, 15, 19, 22};
    static const int ARP_MAJOR[4] = {12, 16, 19, 23};
    const float beat = 60.0f / plan->bpm;
    const float step = beat * 0.25f;
    const float duration = (float)plan->bars * 4.0f * beat;
    const int section_bars = plan->bars / 4;
    const int third = plan->minor ? 3 : 4;

    if (!begin_music(track, duration, plan->gain))
        return false;

    for (int bar = 0; bar < plan->bars; ++bar)
    {
        float bar_start = (float)bar * 16.0f * step;
        int root = plan->root + plan->progression[bar];
        int section = bar / section_bars;
        bool breakdown = section == 2;
        bool full = section == 1 || section == 3;
        int alternate = bar & 1;
        Uint32 seed = plan->seed + (Uint32)bar * 101u;

        /* The floor of the track: either its own pedal note or the bar root an
         * octave down. It is what the breakdown leans on. */
        if (plan->sub_gain > 0.0f)
        {
            int note = plan->sub_note != 0 ? plan->sub_note : root - 12;
            add_tone(track, bar_start, 15.6f * step,
                     midi_hz(note), midi_hz(note),
                     breakdown ? plan->sub_gain * 1.25f : plan->sub_gain,
                     WAVE_SINE, 0.08f, 0.25f);
        }

        if (plan->pad_gain > 0.0f)
        {
            float pad_gain =
                breakdown ? plan->pad_gain * 1.3f : plan->pad_gain;
            if (plan->pad_halves)
            {
                /* Re-voicing mid-bar leans on the leading tone in odd bars,
                 * which keeps a slow loop from sitting still. */
                add_music_pad(track, bar_start, 7.7f * step,
                              root + plan->pad_offset, plan->minor, pad_gain);
                add_music_pad(track, bar_start + 8.0f * step, 7.5f * step,
                              root + plan->pad_offset - alternate,
                              plan->minor, pad_gain);
            }
            else
            {
                add_music_pad(track, bar_start, 15.4f * step,
                              root + plan->pad_offset, plan->minor, pad_gain);
            }
        }

        Uint16 bass_mask = plan->bass_steps[alternate];
        Uint16 kick_mask = plan->kick_steps[alternate];
        Uint16 snare_mask = plan->snare_steps[alternate];
        Uint16 hat_mask = plan->hat_steps[alternate];
        if (breakdown)
        {
            /* Thin to the beats, and never lose the bar line. */
            bass_mask = (Uint16)((bass_mask & MUSIC_BEATS(0x1, 0x1, 0x1, 0x1)) |
                                 1u);
            kick_mask &= 1u;
            snare_mask = alternate
                             ? (Uint16)(snare_mask &
                                        MUSIC_BEATS(0x0, 0x0, 0x0, 0x1))
                             : 0u;
            hat_mask &= MUSIC_BEATS(0x0, 0x1, 0x0, 0x1);
        }
        else if (full)
        {
            hat_mask |= MUSIC_BEATS(0x4, 0x4, 0x4, 0x4);
        }

        for (int position = 0; position < 16; ++position)
        {
            /* Swing pushes the offbeat eighth late and drags the sixteenths
             * either side of it halfway along with it, so the whole bar leans
             * instead of only the notes that happen to fall on odd steps. */
            static const float SWING_PUSH[4] = {0.0f, 0.5f, 1.0f, 0.5f};
            float at = bar_start +
                       ((float)position +
                        plan->swing * SWING_PUSH[position & 3]) * step;
            Uint16 bit = (Uint16)(1u << position);

            if (bass_mask & bit)
            {
                int note = root;
                /* An answer inside the bar and a walk into the next one: the
                 * line moves without the plan having to spell out pitches. */
                if (position == 6 || position == 11)
                    note += alternate ? 7 : third;
                if (position == 14)
                    note -= 2;
                add_music_note(track, at,
                               plan->bass_length * (breakdown ? 1.5f : 1.0f) *
                                   step,
                               note,
                               breakdown ? plan->bass_gain * 0.8f
                                         : plan->bass_gain,
                               plan->bass_wave);
            }
            if (kick_mask & bit)
                add_music_kick(track, at,
                               position == 0 ? plan->kick_gain
                                             : plan->kick_gain * 0.72f,
                               seed + (Uint32)position * 37u);
            if (snare_mask & bit)
                add_music_snare(track, at,
                                full ? plan->snare_gain * 1.15f
                                     : plan->snare_gain,
                                seed + (Uint32)position * 41u);
            if (hat_mask & bit)
                add_music_hat(track, at,
                              (position & 3) == 0 ? plan->hat_gain * 1.2f
                                                  : plan->hat_gain,
                              seed + (Uint32)position * 43u);

            if (plan->lead_gain > 0.0f &&
                (plan->lead_bars & (1u << bar)) != 0)
            {
                const int *lead = (bar & 2) ? plan->lead_b : plan->lead_a;
                if (lead[position] >= 0)
                    add_music_note(track, at, plan->lead_length * step,
                                   root + lead[position],
                                   section == 3 ? plan->lead_gain * 1.2f
                                                : plan->lead_gain,
                                   plan->lead_wave);
            }
            if (full && plan->arp_gain > 0.0f && (position & 3) == 0)
            {
                const int *arp = plan->minor ? ARP_MINOR : ARP_MAJOR;
                add_music_note(track, at, 0.78f * step,
                               root + arp[position / 4], plan->arp_gain,
                               WAVE_TRIANGLE);
            }
            if (full && plan->stab_gain > 0.0f &&
                (position == 2 || position == 10))
            {
                add_music_chord_stab(track, at, 0.85f * step, root + 12,
                                     plan->minor, plan->stab_gain);
            }
        }

        if ((plan->colours & MUSIC_COLOUR_SWEEP) &&
            bar % section_bars == section_bars - 1)
        {
            /* A rise across the last bar of a section, so the loop reads as
             * four phrases instead of one bar sixteen times. */
            add_noise(track, bar_start + 12.0f * step, 3.7f * step,
                      0.052f, 0.32f, 0.42f, 0.08f, seed + 0x51u);
        }
        if (plan->colours & MUSIC_COLOUR_CLANK)
        {
            float at = bar_start + (alternate ? 7.0f : 9.0f) * step;
            add_noise(track, at, 0.085f, 0.052f, 0.28f, 0.002f, 0.072f,
                      seed + 0x71u);
            add_tone(track, at, 0.10f, 720.0f + (float)bar * 13.0f, 310.0f,
                     0.038f, WAVE_SAW, 0.002f, 0.082f);
        }
        if (plan->colours & MUSIC_COLOUR_SPARKLE)
        {
            add_music_note(track, bar_start + 3.0f * step, 2.2f * step,
                           root + 24, 0.030f, WAVE_SINE);
            add_music_note(track, bar_start + 11.0f * step, 2.0f * step,
                           root + 31, 0.024f, WAVE_SINE);
        }
        if (plan->colours & MUSIC_COLOUR_WIND)
        {
            /* Dark and wide: air moving past a wall, not a hiss over it. */
            add_noise(track, bar_start, 16.0f * step,
                      breakdown ? 0.030f : 0.022f, 0.004f,
                      6.0f * step, 6.0f * step, seed + 0x91u);
            add_noise(track, bar_start + 8.0f * step, 8.0f * step,
                      0.012f, 0.05f, 3.0f * step, 4.0f * step, seed + 0x93u);
        }
        if (plan->colours & MUSIC_COLOUR_TICK)
        {
            add_tone(track, bar_start + 3.0f * step, 0.030f, 2100.0f, 2100.0f,
                     0.020f, WAVE_SQUARE, 0.002f, 0.026f);
            add_tone(track, bar_start + 11.0f * step, 0.026f, 1580.0f, 1580.0f,
                     0.016f, WAVE_SQUARE, 0.002f, 0.022f);
        }
        if (plan->colours & MUSIC_COLOUR_DRIP)
        {
            float at = bar_start + ((bar % 3 == 0) ? 5.0f : 13.0f) * step;
            add_tone(track, at, 0.11f, 1450.0f, 900.0f, 0.030f,
                     WAVE_SINE, 0.002f, 0.10f);
        }
    }

    finish_music(track);
    return true;
}

static bool synth_music(AudioSystem *audio)
{
    /*
     * Only the title theme is built up front. It is heard before anything
     * else and comes back at every cutscene and game over, while a level's
     * score is built when its sector loads.
     */
    return synth_music_intro(&audio->music_tracks[MUSIC_INTRO]);
}

/* Build a track if it is not cached yet. False means the score stays silent —
 * as with a failed audio device, the game itself carries on. */
static bool ensure_music_track(AudioSystem *audio, int index)
{
    CachedSound *track = &audio->music_tracks[index];
    if (track->samples != NULL)
        return true;
    if (index == MUSIC_INTRO)
        return synth_music_intro(track);
    if (MUSIC_PLANS[index].progression == NULL)
        return false;
    return synth_music_plan(track, &MUSIC_PLANS[index]);
}

/*
 * Keep the title theme, the current track and the one before it; drop the
 * rest. The previous one is worth its memory because the restroom door
 * switches away and straight back, and rebuilding the sector's score on the
 * way out would cost a frame for nothing.
 */
static void release_stale_music(AudioSystem *audio)
{
    for (int i = 0; i < MUSIC_TRACK_COUNT; ++i)
    {
        if (i == MUSIC_INTRO || i == audio->current_music ||
            i == audio->previous_music)
            continue;
        SDL_free(audio->music_tracks[i].samples);
        audio->music_tracks[i].samples = NULL;
        audio->music_tracks[i].frame_count = 0;
    }
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
    case SFX_MENU_PAGE:
        /* A sheet turning: paper first, and one soft tick where the clip
         * takes it. Nothing tonal — a chime here would sound like an award. */
        if (!begin_sound(audio, effect, 0.16f, 0.30f, 40))
            return false;
        add_noise(s, 0.00f, 0.11f, 0.42f, 0.30f, 0.004f, 0.09f, 0x51b7u);
        add_tone(s, 0.07f, 0.06f, 720.0f, 470.0f, 0.16f, WAVE_TRIANGLE,
                 0.003f, 0.05f);
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
    case SFX_CHASE_ENGINE:
        /*
         * The pursuit engine is one cached rev that the chase retriggers faster
         * as the car speeds up, so a fixed-pitch sample still reads as revs.
         * It sits well below the effects it has to share the road with.
         */
        if (!begin_sound(audio, effect, 1.40f, 0.26f, 700))
            return false;
        add_tone(s, 0.00f, 1.40f, 58.0f, 74.0f, 0.54f,
                 WAVE_SAW, 0.10f, 0.30f);
        add_tone(s, 0.00f, 1.40f, 116.0f, 148.0f, 0.22f,
                 WAVE_TRIANGLE, 0.10f, 0.30f);
        add_noise(s, 0.00f, 1.40f, 0.20f, 0.030f,
                  0.10f, 0.32f, 0x3c17u);
        break;
    case SFX_CHASE_TIRES:
        if (!begin_sound(audio, effect, 0.42f, 0.34f, 200))
            return false;
        add_noise(s, 0.00f, 0.42f, 0.58f, 0.30f,
                  0.005f, 0.34f, 0x71a4u);
        add_tone(s, 0.00f, 0.36f, 880.0f, 430.0f, 0.20f,
                 WAVE_SAW, 0.006f, 0.30f);
        break;
    case SFX_CHASE_CRASH:
        if (!begin_sound(audio, effect, 0.62f, 0.52f, 140))
            return false;
        /* Sheet metal first, then the low thump of the two masses meeting. */
        add_noise(s, 0.00f, 0.10f, 0.95f, 0.62f,
                  0.002f, 0.09f, 0x9d31u);
        add_noise(s, 0.00f, 0.62f, 0.42f, 0.075f,
                  0.003f, 0.52f, 0x2f88u);
        add_tone(s, 0.00f, 0.44f, 132.0f, 46.0f, 0.72f,
                 WAVE_TRIANGLE, 0.003f, 0.38f);
        add_tone(s, 0.05f, 0.26f, 1560.0f, 620.0f, 0.20f,
                 WAVE_SQUARE, 0.003f, 0.22f);
        break;
    case SFX_CHASE_HORN:
        if (!begin_sound(audio, effect, 0.58f, 0.38f, 420))
            return false;
        add_tone(s, 0.00f, 0.58f, 392.0f, 388.0f, 0.34f,
                 WAVE_SQUARE, 0.012f, 0.20f);
        add_tone(s, 0.00f, 0.58f, 494.0f, 489.0f, 0.28f,
                 WAVE_SQUARE, 0.012f, 0.20f);
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
        if (!begin_sound(audio, effect, 1.05f, 0.48f, 850))
            return false;
        /* Two-tone building siren with a lower mechanical harmonic. The
         * sound nearly fills ALARM_SIREN_INTERVAL, leaving a short pulse gap. */
        add_tone(s, 0.00f, 0.52f, 510.0f, 790.0f, 0.55f,
                 WAVE_TRIANGLE, 0.012f, 0.10f);
        add_tone(s, 0.50f, 0.55f, 790.0f, 510.0f, 0.55f,
                 WAVE_TRIANGLE, 0.010f, 0.15f);
        add_tone(s, 0.00f, 1.05f, 255.0f, 260.0f, 0.22f,
                 WAVE_SAW, 0.015f, 0.15f);
        add_noise(s, 0.00f, 1.05f, 0.08f, 0.035f,
                  0.015f, 0.16f, 0xa1a2u);
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
    case SFX_ROCKET_LAUNCH:
        if (!begin_sound(audio, effect, 0.46f, 0.50f, 120))
            return false;
        add_noise(s, 0.00f, 0.42f, 0.88f, 0.10f,
                  0.004f, 0.34f, 0xb420u);
        add_tone(s, 0.00f, 0.38f, 132.0f, 48.0f, 0.66f,
                 WAVE_SAW, 0.003f, 0.31f);
        add_tone(s, 0.04f, 0.36f, 78.0f, 31.0f, 0.44f,
                 WAVE_SINE, 0.004f, 0.30f);
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
    case SFX_GUARD_RADIO:
        /* The same voice as SFX_GUARD_TALK put through a handset: opened and
         * closed by a squelch burst, and pitched a fifth higher into a much
         * narrower band, because a radio is thin where a man in the room is
         * chesty. The two have to be told apart across a corridor — one is a
         * guard who is distracted, the other is a guard who is reporting. */
        if (!begin_sound(audio, effect, 0.52f, 0.17f, 2100))
            return false;
        add_noise(s, 0.00f, 0.045f, 0.30f, 0.75f, 0.002f, 0.030f, 0x91c7u);
        add_tone(s, 0.05f, 0.11f, 285.0f, 246.0f, 0.34f, WAVE_SQUARE, 0.004f,
                 0.05f);
        add_tone(s, 0.18f, 0.09f, 340.0f, 302.0f, 0.30f, WAVE_SQUARE, 0.004f,
                 0.05f);
        add_tone(s, 0.29f, 0.13f, 262.0f, 322.0f, 0.32f, WAVE_SQUARE, 0.004f,
                 0.07f);
        /* The carrier hiss under the whole transmission, then the tail the
         * handset makes when the key is released. */
        add_noise(s, 0.04f, 0.40f, 0.11f, 0.50f, 0.008f, 0.08f, 0x3e42u);
        add_noise(s, 0.45f, 0.06f, 0.26f, 0.70f, 0.002f, 0.045f, 0x91c7u);
        break;
    case SFX_CIVILIAN_SCREAM:
        if (!begin_sound(audio, effect, 0.68f, 0.22f, 220))
            return false;
        /* A cry has to move the whole way through, or it turns into an alarm
         * tone: the pitch climbs into the shriek, holds, then sags away. The
         * octave partial supplies the rasp and the breath layer underneath is
         * what keeps it a person rather than a siren. */
        add_tone(s, 0.00f, 0.13f, 430.0f, 880.0f, 0.30f,
                 WAVE_SAW, 0.006f, 0.05f);
        add_tone(s, 0.11f, 0.33f, 900.0f, 790.0f, 0.38f,
                 WAVE_SAW, 0.010f, 0.15f);
        add_tone(s, 0.11f, 0.33f, 1790.0f, 1560.0f, 0.14f,
                 WAVE_TRIANGLE, 0.010f, 0.15f);
        add_tone(s, 0.41f, 0.24f, 760.0f, 505.0f, 0.24f,
                 WAVE_SAW, 0.008f, 0.19f);
        add_noise(s, 0.00f, 0.62f, 0.06f, 0.55f, 0.010f, 0.24f, 0x7c31u);
        break;
    case SFX_CIVILIAN_SHOUT:
        if (!begin_sound(audio, effect, 0.46f, 0.24f, 220))
            return false;
        /* The same voice an octave down and half as long: two barked
         * syllables instead of a held cry. */
        add_tone(s, 0.00f, 0.15f, 268.0f, 208.0f, 0.44f,
                 WAVE_SAW, 0.006f, 0.06f);
        add_tone(s, 0.00f, 0.15f, 536.0f, 424.0f, 0.17f,
                 WAVE_TRIANGLE, 0.006f, 0.06f);
        add_tone(s, 0.19f, 0.23f, 305.0f, 186.0f, 0.40f,
                 WAVE_SAW, 0.006f, 0.13f);
        add_noise(s, 0.00f, 0.42f, 0.07f, 0.42f, 0.008f, 0.15f, 0x2ba9u);
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
    case SFX_WALL_BREAK:
        /* Masonry, not timber: the crate is a crack and gone, a wall gives way
         * and then keeps coming down. The tail is the rubble, and it is most of
         * what says a hole has been opened rather than something hit. */
        if (!begin_sound(audio, effect, 0.72f, 0.46f, 140))
            return false;
        add_noise(s, 0.00f, 0.22f, 0.95f, 0.14f, 0.003f, 0.20f, 0x3d91u);
        add_tone(s, 0.00f, 0.36f, 92.0f, 28.0f, 0.60f, WAVE_TRIANGLE,
                 0.004f, 0.32f);
        add_noise(s, 0.15f, 0.28f, 0.46f, 0.29f, 0.005f, 0.26f, 0x7ac2u);
        add_noise(s, 0.36f, 0.30f, 0.27f, 0.40f, 0.006f, 0.28f, 0xb15fu);
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
    case SFX_WIND_GUST:
        /*
         * One long swell that covers the warning beat and the gust itself, so
         * the sound is the telegraph rather than an accent on top of it.
         */
        if (!begin_sound(audio, effect, 3.40f, 0.30f, 900))
            return false;
        add_noise(s, 0.00f, 3.40f, 0.62f, 0.055f, 0.85f, 1.30f, 0x2c7du);
        add_noise(s, 0.55f, 2.50f, 0.30f, 0.012f, 0.70f, 1.10f, 0x91a4u);
        add_tone(s, 0.30f, 2.70f, 168.0f, 96.0f, 0.10f,
                 WAVE_SINE, 0.80f, 1.10f);
        break;
    case SFX_BIRD_CALL:
        if (!begin_sound(audio, effect, 0.36f, 0.30f, 140))
            return false;
        add_tone(s, 0.00f, 0.09f, 1180.0f, 1520.0f, 0.30f,
                 WAVE_TRIANGLE, 0.004f, 0.06f);
        add_tone(s, 0.13f, 0.11f, 1420.0f, 980.0f, 0.27f,
                 WAVE_TRIANGLE, 0.004f, 0.08f);
        add_noise(s, 0.24f, 0.10f, 0.20f, 0.42f, 0.004f, 0.09f, 0x6b3au);
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
    case SFX_PICKUP_BAZOOKA:
        if (!begin_sound(audio, effect, 0.44f, 0.36f, 140))
            return false;
        add_tone(s, 0.00f, 0.14f, 185.0f, 245.0f, 0.48f,
                 WAVE_SQUARE, 0.004f, 0.10f);
        add_tone(s, 0.12f, 0.16f, 245.0f, 370.0f, 0.43f,
                 WAVE_TRIANGLE, 0.004f, 0.12f);
        add_tone(s, 0.27f, 0.17f, 370.0f, 555.0f, 0.39f,
                 WAVE_TRIANGLE, 0.004f, 0.13f);
        add_noise(s, 0.00f, 0.10f, 0.24f, 0.40f,
                  0.003f, 0.08f, 0xb42au);
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
    audio->previous_music = -1;
    /* Full until the shell says otherwise. Audio init runs before the saved
     * settings are applied, and a zeroed struct would mean the title theme
     * played silently on every launch that got the order slightly wrong. */
    audio->music_volume = 1.0f;
    audio->sfx_volume = 1.0f;

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
    /* A voice queued at zero gain still occupies one of sixteen and still
     * ducks the music, so silence is a return rather than a multiply. */
    if (audio->sfx_volume <= 0.0f)
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
                                      mix_headroom *
                                      audio->sfx_volume);
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
    /* A wall coming down changes the map, so it has to be audible from
     * wherever the player was standing when he threw the thing. */
    case SFX_WALL_BREAK:
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

/*
 * What the music stream is actually set to: the loop's own gain, the ducking
 * that makes room for a busy scene, and the player's level. It is one function
 * because three callers need the answer — starting a track, the per-frame
 * update, and a slider being moved — and a level that only took effect at the
 * next track change would read as a slider that does nothing.
 */
static void apply_music_gain(AudioSystem *audio, float duck)
{
    if (audio->music_stream == NULL || audio->current_music < 0 ||
        audio->current_music >= MUSIC_TRACK_COUNT)
        return;

    SDL_SetAudioStreamGain(audio->music_stream,
                           audio->music_tracks[audio->current_music].gain *
                               duck * audio->music_volume);
}

void audio_play_music(AudioSystem *audio, MusicTrack track)
{
    int track_index = (int)track;
    if (!audio->ready || audio->music_stream == NULL ||
        track_index < 0 || track_index >= MUSIC_TRACK_COUNT)
        return;
    if (audio->current_music == track_index)
        return;
    if (!ensure_music_track(audio, track_index))
        return;

    SDL_ClearAudioStream(audio->music_stream);
    audio->previous_music = audio->current_music;
    audio->current_music = track_index;
    release_stale_music(audio);
    apply_music_gain(audio, 1.0f);
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
    apply_music_gain(audio, duck);
}

void audio_set_volumes(AudioSystem *audio, float music, float sfx)
{
    audio->music_volume = clampf(music, 0.0f, 1.0f);
    audio->sfx_volume = clampf(sfx, 0.0f, 1.0f);
    /* Unducked: the next frame's audio_update_music puts the ducking back, and
     * a slider moved during a firefight must not sound quieter than the level
     * it was set to. */
    apply_music_gain(audio, 1.0f);
}

void audio_toggle_mute(AudioSystem *audio)
{
    audio->muted = !audio->muted;
    if (audio->device != 0)
        SDL_SetAudioDeviceGain(audio->device,
                               audio->muted ? 0.0f : AUDIO_MASTER_GAIN);
}
