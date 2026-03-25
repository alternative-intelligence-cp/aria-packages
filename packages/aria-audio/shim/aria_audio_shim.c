/*
 * aria_audio_shim.c — Software audio synthesis for Aria virtual console
 *
 * All functions return int32_t (0=success, -1=error, or value as documented).
 *
 * Design: "dry mode" (no hardware output) is the default. Call
 * aria_audio_init(1) for dry mode (tests), aria_audio_init(0) to attempt
 * actual output via /dev/dsp (OSS) or stdout pipe in the future.
 *
 * For the virtual console prototype, all playback is synchronous and
 * in-memory. A real implementation would spin a mixing thread; that is
 * intentionally left for a future LIB-19+ milestone.
 *
 * Waveform codes:
 *   0 = square      1 = triangle    2 = sawtooth    3 = noise (LFSR)
 *
 * Channel layout: 4 independent channels (0-3), like classic 8-bit hardware.
 *
 * MIDI note numbers: middle C = 60, A4 = 69 (440 Hz).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#ifdef __linux__
#include <alsa/asoundlib.h>
#endif

#define AUDIO_CHANNELS   4
#define AUDIO_SAMPLE_RATE 44100
#define AUDIO_FRAME_SAMPLES 735  /* 44100 / 60 */

/* ---- ALSA state ---- */
#ifdef __linux__
static snd_pcm_t *g_pcm = NULL;
#endif
static int32_t g_phase[AUDIO_CHANNELS]; /* per-channel phase accumulator */

/* ---- per-channel state ---- */

typedef struct {
    int      active;       /* 1 if "playing" */
    int32_t  freq_hz;
    int32_t  waveform;     /* 0-3 */
    int32_t  duration_ms;
    int32_t  volume;       /* 0-15 */
    uint32_t lfsr;         /* noise state */
    /* ADSR envelope */
    int32_t  env_attack;   /* attack time in ms */
    int32_t  env_decay;    /* decay time in ms */
    int32_t  env_sustain;  /* sustain level 0-15 */
    int32_t  env_release;  /* release time in ms */
    int32_t  env_enabled;  /* 1 if ADSR active */
} Channel;

static int     g_initialized = 0;
static int     g_dry_mode    = 1;  /* default: dry */
static Channel g_channels[AUDIO_CHANNELS];

/* ---- note frequency table (MIDI 0-127 -> Hz, rounded) ----
 * Generated from: round(440.0 * 2^((n-69)/12.0))
 * Only the commonly used range 21-108 is accurate; outside that
 * the table uses clamped values.
 */
static const int32_t NOTE_FREQ[128] = {
     8,    9,    9,   10,   10,   11,   12,   12,   13,   14,   15,   15,  /* 0-11 */
    16,   17,   18,   19,   21,   22,   23,   25,   26,   28,   29,   31,  /* 12-23 */
    33,   35,   37,   39,   41,   44,   46,   49,   52,   55,   58,   62,  /* 24-35 */
    65,   69,   73,   78,   82,   87,   92,   98,  104,  110,  117,  123,  /* 36-47 */
   131,  139,  147,  156,  165,  175,  185,  196,  208,  220,  233,  247,  /* 48-59 */
   262,  277,  294,  311,  330,  349,  370,  392,  415,  440,  466,  494,  /* 60-71 */
   523,  554,  587,  622,  659,  698,  740,  784,  831,  880,  932,  988,  /* 72-83 */
  1047, 1109, 1175, 1245, 1319, 1397, 1480, 1568, 1661, 1760, 1865, 1976, /* 84-95 */
  2093, 2217, 2349, 2489, 2637, 2794, 2960, 3136, 3322, 3520, 3729, 3951, /* 96-107 */
  4186, 4435, 4699, 4978, 5274, 5588, 5920, 6272, 6645, 7040, 7459, 7902, /* 108-119 */
  8372, 8870, 9397, 9956,10548,11175,11840,12544                           /* 120-127 */
};

/* ---- public API ---- */

/*
 * Initialize audio subsystem.
 *   dry_mode=1: silent mode for tests (always succeeds)
 *   dry_mode=0: attempt hardware output (falls back to dry if unavailable)
 * Returns 0 on success, -1 on error.
 */
int32_t aria_audio_init(int32_t dry_mode) {
    if (g_initialized) return 0;
    g_dry_mode = (dry_mode != 0) ? 1 : 0;
    memset(g_channels, 0, sizeof(g_channels));
    memset(g_phase, 0, sizeof(g_phase));
    for (int i = 0; i < AUDIO_CHANNELS; i++) {
        g_channels[i].volume = 15;
        g_channels[i].lfsr   = (uint32_t)(0xACE1u + i);
    }
#ifdef __linux__
    if (!g_dry_mode) {
        int err = snd_pcm_open(&g_pcm, "default", SND_PCM_STREAM_PLAYBACK, 0);
        if (err < 0) {
            g_pcm = NULL;
            g_dry_mode = 1; /* fall back to dry */
        } else {
            snd_pcm_set_params(g_pcm, SND_PCM_FORMAT_S16_LE,
                               SND_PCM_ACCESS_RW_INTERLEAVED,
                               1, AUDIO_SAMPLE_RATE, 1, 50000);
        }
    }
#endif
    g_initialized = 1;
    return 0;
}

/* Shutdown and reset state. Returns 0. */
int32_t aria_audio_shutdown(void) {
#ifdef __linux__
    if (g_pcm) {
        snd_pcm_drain(g_pcm);
        snd_pcm_close(g_pcm);
        g_pcm = NULL;
    }
#endif
    memset(g_channels, 0, sizeof(g_channels));
    memset(g_phase, 0, sizeof(g_phase));
    g_initialized = 0;
    return 0;
}

/* Returns 1 if initialized, 0 otherwise. */
int32_t aria_audio_is_init(void) {
    return g_initialized ? 1 : 0;
}

/* Returns 1 if in dry mode, 0 if in hardware mode. */
int32_t aria_audio_is_dry(void) {
    return g_dry_mode ? 1 : 0;
}

/* Returns the fixed sample rate (44100). */
int32_t aria_audio_sample_rate(void) {
    return AUDIO_SAMPLE_RATE;
}

/*
 * Calculate sample count for a given duration in milliseconds.
 * samples = ms * 44100 / 1000
 */
int32_t aria_audio_samples_for_ms(int32_t ms) {
    if (ms < 0) return 0;
    /* Avoid overflow: ms * 44100 can overflow int32 around 48000ms */
    int64_t s = (int64_t)ms * (int64_t)AUDIO_SAMPLE_RATE / 1000LL;
    if (s > 0x7FFFFFFF) s = 0x7FFFFFFF;
    return (int32_t)s;
}

/*
 * Return frequency in Hz for a MIDI note number (0-127).
 * Returns -1 if out of range.
 */
int32_t aria_audio_note_freq(int32_t midi_note) {
    if (midi_note < 0 || midi_note > 127) return -1;
    return NOTE_FREQ[midi_note];
}

/*
 * Play a tone on a channel.
 *   channel   : 0-3
 *   freq_hz   : frequency in Hz (1-20000)
 *   waveform  : 0=square 1=triangle 2=sawtooth 3=noise
 *   duration_ms: 0 = play until stopped; >0 = play for N ms then stop
 * In dry mode, just marks the channel as active.
 * Returns 0 on success, -1 on bad args.
 */
int32_t aria_audio_play_tone(int32_t channel, int32_t freq_hz,
                              int32_t waveform, int32_t duration_ms) {
    if (!g_initialized) return -1;
    if (channel < 0 || channel >= AUDIO_CHANNELS) return -1;
    if (freq_hz < 1 || freq_hz > 20000) return -1;
    if (waveform < 0 || waveform > 3) return -1;
    if (duration_ms < 0) return -1;
    Channel *ch = &g_channels[channel];
    ch->active      = 1;
    ch->freq_hz     = freq_hz;
    ch->waveform    = waveform;
    ch->duration_ms = duration_ms;
    return 0;
}

/* Stop a channel. Returns 0 on success, -1 if bad channel. */
int32_t aria_audio_stop_channel(int32_t channel) {
    if (channel < 0 || channel >= AUDIO_CHANNELS) return -1;
    g_channels[channel].active = 0;
    return 0;
}

/* Stop all channels. Returns 0. */
int32_t aria_audio_stop_all(void) {
    for (int i = 0; i < AUDIO_CHANNELS; i++)
        g_channels[i].active = 0;
    return 0;
}

/*
 * Set channel volume (0=silent, 15=max).
 * Returns 0 on success, -1 on bad args.
 */
int32_t aria_audio_set_volume(int32_t channel, int32_t vol) {
    if (channel < 0 || channel >= AUDIO_CHANNELS) return -1;
    if (vol < 0 || vol > 15) return -1;
    g_channels[channel].volume = vol;
    return 0;
}

/* Returns 1 if channel is active, 0 if not, -1 on bad channel. */
int32_t aria_audio_is_playing(int32_t channel) {
    if (channel < 0 || channel >= AUDIO_CHANNELS) return -1;
    return g_channels[channel].active ? 1 : 0;
}

/* Returns current volume for channel (0-15), or -1 on bad channel. */
int32_t aria_audio_get_volume(int32_t channel) {
    if (channel < 0 || channel >= AUDIO_CHANNELS) return -1;
    return g_channels[channel].volume;
}

/*
 * Generate one square wave sample for the given frequency.
 * Returns +1 or -1 (as int32, representing the sign of the square wave
 * at a given phase offset).
 *   phase_sample: absolute sample index
 *   freq_hz     : frequency
 * Returns 1 for first half of period, -1 for second half.
 */
int32_t aria_audio_square_sample(int32_t phase_sample, int32_t freq_hz) {
    if (freq_hz <= 0) return 1;
    int32_t period = AUDIO_SAMPLE_RATE / freq_hz;
    if (period < 1) period = 1;
    int32_t pos = phase_sample % period;
    return (pos < period / 2) ? 1 : (0 - 1);
}

/*
 * Advance a 16-bit Galois LFSR one step and return the next state.
 * Polynomial: x^16 + x^14 + x^13 + x^11 + 1 (taps: bits 0,2,3,5)
 * Returns 0 or 1.
 */
int32_t aria_audio_lfsr_step(int32_t state) {
    uint32_t s = (uint32_t)state;
    if (s == 0) s = 0xACE1u;
    /* galois LFSR step */
    uint32_t bit = s & 1u;
    s >>= 1;
    if (bit) s ^= 0xB400u; /* taps for maximal 16-bit sequence */
    return (int32_t)s;
}

/* ---- ADSR envelope ---- */

/*
 * Set ADSR envelope parameters for a channel.
 *   attack_ms  : ramp from 0 to max volume
 *   decay_ms   : ramp from max to sustain level
 *   sustain    : held level (0-15)
 *   release_ms : ramp from sustain to 0 after note off
 * Returns 0 on success, -1 on bad args.
 */
int32_t aria_audio_set_envelope(int32_t channel, int32_t attack_ms,
                                 int32_t decay_ms, int32_t sustain,
                                 int32_t release_ms) {
    if (channel < 0 || channel >= AUDIO_CHANNELS) return -1;
    if (attack_ms < 0 || decay_ms < 0 || release_ms < 0) return -1;
    if (sustain < 0 || sustain > 15) return -1;
    Channel *ch = &g_channels[channel];
    ch->env_attack  = attack_ms;
    ch->env_decay   = decay_ms;
    ch->env_sustain = sustain;
    ch->env_release = release_ms;
    ch->env_enabled = 1;
    return 0;
}

/*
 * Clear envelope for a channel (reverts to flat volume).
 * Returns 0 on success, -1 on bad channel.
 */
int32_t aria_audio_clear_envelope(int32_t channel) {
    if (channel < 0 || channel >= AUDIO_CHANNELS) return -1;
    Channel *ch = &g_channels[channel];
    ch->env_attack  = 0;
    ch->env_decay   = 0;
    ch->env_sustain = 15;
    ch->env_release = 0;
    ch->env_enabled = 0;
    return 0;
}

/*
 * Returns 1 if the channel has an ADSR envelope set, 0 otherwise.
 */
int32_t aria_audio_has_envelope(int32_t channel) {
    if (channel < 0 || channel >= AUDIO_CHANNELS) return -1;
    return g_channels[channel].env_enabled ? 1 : 0;
}

/*
 * Compute the envelope amplitude level (0-15) at a given elapsed time.
 * Phases: attack → decay → sustain (held indefinitely).
 * Release phase is handled separately via aria_audio_envelope_release.
 *
 *   elapsed_ms: time since note-on in milliseconds
 *
 * Returns volume level 0-15.
 */
int32_t aria_audio_envelope_level(int32_t channel, int32_t elapsed_ms) {
    if (channel < 0 || channel >= AUDIO_CHANNELS) return 0;
    Channel *ch = &g_channels[channel];
    if (!ch->env_enabled) return ch->volume;
    if (elapsed_ms < 0) elapsed_ms = 0;

    int32_t max_vol = ch->volume;  /* peak volume */
    int32_t sus_vol = ch->env_sustain;

    /* Attack phase: ramp 0 → max_vol */
    if (elapsed_ms < ch->env_attack) {
        if (ch->env_attack == 0) return max_vol;
        return (int32_t)((int64_t)max_vol * elapsed_ms / ch->env_attack);
    }

    /* Decay phase: ramp max_vol → sus_vol */
    int32_t decay_start = ch->env_attack;
    if (elapsed_ms < decay_start + ch->env_decay) {
        if (ch->env_decay == 0) return sus_vol;
        int32_t t = elapsed_ms - decay_start;
        int32_t drop = max_vol - sus_vol;
        return max_vol - (int32_t)((int64_t)drop * t / ch->env_decay);
    }

    /* Sustain phase: hold at sus_vol */
    return sus_vol;
}

/*
 * Compute the release envelope level (sus → 0) given time since note-off.
 * Returns volume level 0-15.
 */
int32_t aria_audio_envelope_release(int32_t channel, int32_t release_elapsed_ms) {
    if (channel < 0 || channel >= AUDIO_CHANNELS) return 0;
    Channel *ch = &g_channels[channel];
    if (!ch->env_enabled || ch->env_release == 0) return 0;
    if (release_elapsed_ms < 0) release_elapsed_ms = 0;
    if (release_elapsed_ms >= ch->env_release) return 0;

    int32_t sus_vol = ch->env_sustain;
    return sus_vol - (int32_t)((int64_t)sus_vol * release_elapsed_ms / ch->env_release);
}

/* ---- sample generation helpers ---- */

/*
 * Generate one sample for a channel at its current phase.
 * Returns a signed 16-bit sample value (-32767 to 32767).
 * Advances the channel's phase accumulator.
 */
static int16_t _generate_sample(int ch_idx) {
    Channel *ch = &g_channels[ch_idx];
    if (!ch->active || ch->freq_hz <= 0) return 0;

    int32_t period = AUDIO_SAMPLE_RATE / ch->freq_hz;
    if (period < 1) period = 1;
    int32_t pos = g_phase[ch_idx] % period;
    int16_t sample = 0;

    switch (ch->waveform) {
        case 0: /* square */
            sample = (pos < period / 2) ? 8191 : -8191;
            break;
        case 1: /* triangle */
            if (pos < period / 2) {
                sample = (int16_t)(-8191 + (int32_t)(16382LL * pos / (period / 2)));
            } else {
                sample = (int16_t)(8191 - (int32_t)(16382LL * (pos - period / 2) / (period / 2)));
            }
            break;
        case 2: /* sawtooth */
            sample = (int16_t)(-8191 + (int32_t)(16382LL * pos / period));
            break;
        case 3: /* noise (LFSR) */
            ch->lfsr = (uint32_t)aria_audio_lfsr_step((int32_t)ch->lfsr);
            sample = (ch->lfsr & 1) ? 8191 : -8191;
            break;
    }

    /* Apply volume (0-15 → scale) */
    sample = (int16_t)((int32_t)sample * ch->volume / 15);
    g_phase[ch_idx]++;
    return sample;
}

/*
 * Render one frame of audio (735 samples at 44100Hz/60fps) and write
 * to the ALSA PCM device. In dry mode, just advances phase accumulators.
 * Call once per game frame.
 * Returns 0 on success, -1 on error.
 */
int32_t aria_audio_tick(void) {
    if (!g_initialized) return -1;

    int16_t buf[AUDIO_FRAME_SAMPLES];
    memset(buf, 0, sizeof(buf));

    for (int s = 0; s < AUDIO_FRAME_SAMPLES; s++) {
        int32_t mix = 0;
        for (int c = 0; c < AUDIO_CHANNELS; c++) {
            mix += (int32_t)_generate_sample(c);
        }
        /* Clamp to int16 range */
        if (mix > 32767) mix = 32767;
        if (mix < -32767) mix = -32767;
        buf[s] = (int16_t)mix;
    }

#ifdef __linux__
    if (!g_dry_mode && g_pcm) {
        snd_pcm_sframes_t frames = snd_pcm_writei(g_pcm, buf, AUDIO_FRAME_SAMPLES);
        if (frames < 0) {
            snd_pcm_recover(g_pcm, (int)frames, 1);
        }
    }
#endif
    return 0;
}
