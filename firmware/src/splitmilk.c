/* SPDX-License-Identifier: GPL-3.0-only */
/* SPLITMILK: a split keyboard for the M-VAVE FM-1, built on Dinghy's platform (through QUARTET).
 *
 * The keyboard is two instruments at once:
 *   LEFT   "CHOC" (chocolate milk) the lower keys F3..E4 (12 keys), dark until you press them
 *   RIGHT  "BERRY" (strawberry milk) the upper keys F4..G5 (15 keys), always lit (a pressed key goes dark)
 * Each has its own sound (24: leads, pads / keys and basses), mode, octave and volume:
 *   SINGLE KEY    a note a key
 *   CHORDS        a chord a key: MAJOR, MINOR, 7TH      OCTAVE   the note and the one an octave up
 *   ARPEGGIO      the in-key triads of the keys held, over 2 octaves, a 16th note each: UP, DOWN, UP-DOWN, RANDOM
 *                 (on the drum machine's tempo, whether it plays or not)
 * The screen is split the same way: LEFT's settings on the left half, RIGHT's on the right, the drums below.
 * The look is milk: cream and white, carton-blue ink, cow-print headers, the volume as a glass of each flavour, the
 * drum steps as milk drops, and on the loading page the two milks fill a glass half way.
 *
 *   OCT-        choose LEFT: from now on KNOB1 octave, KNOB2 sound, KNOB3 mode, KNOB4 volume set LEFT
 *   OCT+        choose RIGHT: the knobs set RIGHT         (the chosen side's OCT button is lit, its half outlined)
 *   SELECT      move the split, a key at a time (clockwise: LEFT gets more keys; 1..26 keys each side)
 *   PRESETS     the key: everything a semitone up / down (-12..+12), the arpeggiator's in-key chords with it
 *   REC         tapped on the split screen with keys held: LATCH, they go on sounding; REC again lets them go
 *   ALGORITHM   the tempo (the drums and the arpeggios); on the SEQ page: the drum, or PADDOCK's note
 *   PLAY        start / stop the sequencer (the drums and PADDOCK)
 *   SEQ         the sequencer's page, two tabs (SELECT): DAIRY DRUMS and PADDOCK (SEQ again or HOME: back)
 *   FX SEL ENV LFO EDIT   REVERB, DELAY, DISTORTION, CHORUS, PHASER (sm_fx.h) for the chosen instrument: a tap turns
 *               it on / off, a hold (0.5 s) opens its page (KNOB1 on / off, KNOB2-4 its knobs)
 *   ARP         the arpeggiator's page: rate, octaves, gate, notes (the direction is the mode)
 *   GLO         the wave editor: OSC, FILTER, ENV, LFO (SELECT turns the pages), a picture of each
 *   REC         held 1 s on an option page: that page's settings as new (the page's note says so); held 1 s on the
 *               split screen: everything as new (the save slots stay). Everything starts empty: drums, PADDOCK
 *   SAVE        the save page: 4 slots, KNOB1 the slot, REC saves, PLAY loads (each asks first), SAVE: back
 *   MASTER      the volume (platform.c)
 *   MIDI in     notes split where the keys are split (each in its half's mode, in the key; not the octave)
 *
 * The SEQ page (like the wave editor: SELECT turns the tabs):
 * DAIRY DRUMS: 6 drums (KICK SNARE HAT OPEN-HAT CLAP PERC) x 16 steps, 8 kits (MINI POPS and LM-1 play samples; their
 *   CLAP and PERC rows: a snap and a wood block / the LM-1's toms).
 *   the 16 white keys  the 16 steps: turn the chosen drum on / off on that step
 *   the first 6 black keys (F#3 G#3 A#3 C#4 D#4 F#4)  choose the drum (and play it); ALGORITHM does too
 *   KNOB1 tempo, KNOB2 kit, KNOB3 swing, KNOB4 drum volume (OCT- / OCT+ still choose the side for the split screen)
 *   REC  tap: clear the chosen drum's row; hold 1 s: clear the whole pattern
 * PADDOCK: a pad sequencer, a third instrument: a note or a chord (up to 4) a bar, held all 16 steps, 1..4 bars.
 *   a key (or ALGORITHM, a semitone a click)  the note of the bar being set; a key also moves on to the next bar
 *   OCT- / OCT+  the bar before / after       PRESETS  PADDOCK's octave       REC  tap: a rest; hold 1 s: all rests
 *   KNOB1 tempo, KNOB2 its sound (one of the 7 pads), KNOB3 bars 1..4, KNOB4 its volume (an empty bar starts at C3)
 * Everything is kept in flash by itself (saved 2 s after a change, when nothing sounds).
 *
 * Rules of the road (no FPU, no libgcc): integers only, no 64-bit division. tests/host.c runs this file on a
 * computer: the screen as PNG, the sound as WAV. */
static void app_idle(void);
#define LCD_IDLE app_idle
#define APP_NAME "SPLITMILK"
#define STORE_MAGIC 0x4B4C4D53u                 /* "SMLK": SPLITMILK's own flash data */
#define CV_MAX (240u * 128u)
#include "platform.c"

/* ---- shared sound helpers */
static int32_t sine_calc(uint32_t ph)           /* a full cycle over 2^32 -> -32767..32767 (a refined parabola) */
{
    int32_t x = (int32_t)ph >> 16, ax = x < 0 ? -x : x, y, ay;
    y = (x * (32768 - ax)) >> 13;
    ay = y < 0 ? -y : y;
    y += ((((y * ay) >> 15) - y) * 7373) >> 15;
    return y > 32767 ? 32767 : y < -32767 ? -32767 : y;
}
/* the sine everything uses (oscillators, drums, pedals): a table of 2048 steps in RAM, filled at boot from
 * sine_calc, read with a straight line between steps: a few operations instead of the parabola's multiplies */
#define SINE_BITS 11u
static int16_t sine_t[(1u << SINE_BITS) + 1u];
static void sine_init(void)
{
    uint32_t i;
    for (i = 0; i <= 1u << SINE_BITS; i++) sine_t[i] = (int16_t)sine_calc(i << (32u - SINE_BITS));
}
static inline int32_t sine(uint32_t ph)
{
    uint32_t i = ph >> (32u - SINE_BITS);
    int32_t a = sine_t[i], f = (int32_t)((ph >> (16u - SINE_BITS)) & 0xFFFFu);
    return a + (((sine_t[i + 1u] - a) * f) >> 16);
}
static const uint32_t INC_C9[12] = {            /* phase steps of MIDI 120..131; lower octaves shift right */
    815363807u, 863847862u, 915214929u, 969636441u, 1027294024u, 1088380105u,
    1153098554u, 1221665363u, 1294309365u, 1371273005u, 1452813141u, 1539201906u};

/* ---- the settings (all of SPLITMILK's state, in one settings sector; the save page keeps copies in 4 slots) */
#define SET_VER 9u                              /* 9: HIP-HOP and 909 gone (0.11); 8: the LM-1 kit gone, new defaults (0.10); 7: 24 sounds, OCTAVE mode, PADDOCK chords (0.9); 6: 21 sounds, PADDOCK on
                                                   pads (0.8); 5: + PADDOCK (0.7); 4: + each instrument's sound, pedals, arpeggiator
                                                   (0.6); 3: 23 sounds (0.5); 2: + split and key (0.3); 1: 0.1 */
typedef struct { uint8_t inst, mode, vol; int8_t oct; } part_t;
typedef struct {                                /* a sound (sm_synth.h); 0..127 unless said. GLO edits it */
    uint8_t cut, res, fenv;                     /* filter: cutoff, resonance, how far the envelope opens it */
    uint8_t w1, w2, tune, mix;                  /* oscillators: waves 0..5, osc 2 tuning 0..48 (24 = same), mix */
    uint8_t a, d, s, r;                         /* the envelope */
    uint8_t lrate, ldepth, ltgt, lwave;         /* the LFO: rate, depth, target 0..2, wave 0..3 */
} snd_t;
typedef struct {                                /* an instrument's pedals (sm_fx.h); 0..127 unless said */
    uint8_t on;                                 /* bit e: pedal e on (E_REV, E_DLY, E_DST, E_CHO, E_PHS) */
    uint8_t rev_send, dly_send, cho_send;       /* REVERB LEVEL, DELAY ECHO, CHORUS LEVEL: what goes to the shared ones */
    uint8_t drive, tone, dlevel;                /* DISTORTION: dist, tone, level */
    uint8_t prate, pdepth, pfb;                 /* PHASER: rate, depth, feedback */
} tfx_t;
typedef struct {                                /* the shared pedals' knobs (one reverb, one delay, one chorus) */
    uint8_t rtone, rtime;                       /* REVERB: tone, time */
    uint8_t dtime, dfb;                         /* DELAY: repeat rate 0..6 (1/16 .. 1/2), intensity */
    uint8_t crate, cdepth;                      /* CHORUS: rate, depth */
} gfx_t;
typedef struct { uint8_t rate, oct, gate, notes; } arp_t;   /* the ARP page: 1/4..1/32, 1..4 octaves, 25..100 %, CHORDS / KEYS */
typedef struct {                                /* PADDOCK, the pad sequencer (SEQ): a chord a bar, held all 16 steps */
    uint8_t inst, bars, vol;                    /* its sound (one of the pads, as its preset), bars 1..4, volume 0..100 */
    int8_t oct;                                 /* the octave, -3..+3 (PRESETS on its page) */
    uint8_t note[4][4];                         /* each bar's chord (1..4 notes, low to high), as the keys played (0: none) */
} pad_t;
#define FXN 256u                                /* frames the effects take at a time (the audio block) */
static struct {
    uint8_t ver;
    uint8_t bpm, kit, swing, dvol;              /* the drum machine: tempo, kit, swing 0..100, volume 0..100 */
    part_t part[2];                             /* LEFT, RIGHT */
    uint16_t dpat[6];                           /* the drum pattern: bit s = step s */
    uint8_t split;                              /* (v2) keys in LEFT, 1..26: LEFT is keys 0..split-1 (SELECT) */
    int8_t key;                                 /* (v2) the key: semitones from C, -12..+12 (PRESETS) */
    snd_t snd[2];                               /* (v4) each instrument's own sound (from its preset; GLO edits it) */
    tfx_t fx[2];                                /* (v4) each instrument's pedals (FX SEL ENV LFO EDIT) */
    gfx_t gfx;                                  /* (v4) the shared pedals' knobs */
    arp_t arp[2];                               /* (v4) each instrument's arpeggiator (ARP) */
    pad_t pad;                                  /* (v5) PADDOCK */
} set;
#define SET_V1_SIZE ((uint32_t)((uint8_t *)&set.split - (uint8_t *)&set))   /* a 0.1 / 0.2 save: no split, no key */
#define SET_V3_SIZE ((uint32_t)((uint8_t *)&set.snd - (uint8_t *)&set))     /* a 0.3 .. 0.5 save: no sounds, pedals, arp */
#define SET_V4_SIZE ((uint32_t)((uint8_t *)&set.pad - (uint8_t *)&set))     /* a 0.6 save: no PADDOCK */
#define SET_V6_SIZE ((uint32_t)((uint8_t *)&set.pad.note[1] - (uint8_t *)&set))   /* a 0.7 / 0.8 save: a note a bar */
static int32_t song_key(void) { return set.key; }
/* the engine's voice groups: 0 CHOC, 1 BERRY (set.part[]), 2 PADDOCK (set.pad: single notes, its sound's preset) */
static uint32_t part_inst(uint32_t p) { return (p >= 2u ? set.pad.inst : set.part[p & 1u].inst) % 24u; }   /* (NINST) */
static uint32_t part_mode(uint32_t p) { return p >= 2u ? 0u : set.part[p & 1u].mode % 9u; }
static const snd_t *part_snd(uint32_t p);

#include "sm_synth.h"
static const snd_t *part_snd(uint32_t p) { return p >= NPART ? &SND[set.pad.inst % NINST] : &set.snd[p & 1u]; }
static uint32_t pad_note_of(uint32_t raw)       /* what a PADDOCK note sounds: in the key, its octave, its sound's register */
{
    int32_t n = (int32_t)raw + set.key + 12 * set.pad.oct + CAT_TRANS[INST_CAT(set.pad.inst % NINST)];
    return (uint32_t)(n < 12 ? 12 : n > 120 ? 120 : n);
}
#include "sm_drums.h"
#include "sm_fx.h"
typedef char inst_count_ok[NINST == 24u && NMODE == 9u && NDR == 6u ? 1 : -1];

static const tfx_t FX0 = {0, 60, 60, 70, 60, 80, 32, 40, 90, 60};   /* every pedal off; reverb, delay, chorus levels; dist; phaser */
static const gfx_t GFX0 = {70, 70, 3, 50, 40, 60};   /* reverb tone, time; delay 1/8 dotted, intensity; chorus rate, depth */
static const arp_t ARP0 = {2, 1, 1, 0};         /* 1/16, 2 octaves, 50 %, CHORDS (the arpeggio of 0.1 .. 0.5) */
static const pad_t PAD0 = {10, 4, 70, 0, {{0}}};   /* STRINGS, 4 bars, all rests: an empty PADDOCK */
#define PAD_START 48u                           /* ALGORITHM on a rest: C3 */
static void pad_defaults(void) { set.pad = PAD0; }
static void extras_defaults(void)               /* the v4 settings, as new: each sound its preset, pedals off */
{
    uint32_t p;
    for (p = 0; p < NPART; p++) { set.snd[p] = SND[part_inst(p)]; set.fx[p] = FX0; set.arp[p] = ARP0; }
    set.gfx = GFX0;
}
static void set_defaults(void)
{
    set.ver = SET_VER;
    set.bpm = 110; set.kit = KIT_808; set.swing = 0; set.dvol = 70;   /* 808 */
    set.part[0].inst = 0; set.part[0].mode = M_SINGLE; set.part[0].vol = 70; set.part[0].oct = 0;   /* LEFT: PIANO */
    set.part[1].inst = 3; set.part[1].mode = M_SINGLE; set.part[1].vol = 70; set.part[1].oct = 0;   /* RIGHT: MUSIC BOX */
    {                                           /* the drums: an empty pattern */
        uint32_t d;
        for (d = 0; d < NDR; d++) set.dpat[d] = 0;
    }
    set.split = 12;                             /* F3..E4 | F4..G5 */
    set.key = 0;                                /* C */
    extras_defaults();
    pad_defaults();
}
static int bytes_le(const void *p, uint32_t n, uint32_t mx)   /* every byte <= mx */
{
    const uint8_t *b = (const uint8_t *)p;
    while (n--) if (*b++ > mx) return 0;
    return 1;
}
static int settings_ok(void)
{
    uint32_t p;
    if (set.ver != SET_VER || set.bpm < BPM_MIN || set.bpm > BPM_MAX || set.kit >= NKIT || set.swing > 100u || set.dvol > 100u) return 0;
    if (set.split < 1u || set.split > 26u || set.key < -12 || set.key > 12) return 0;
    if (!bytes_le(&set.gfx, sizeof set.gfx, 127u) || set.gfx.dtime >= NDLY) return 0;
    for (p = 0; p < NPART; p++) {
        const snd_t *s = &set.snd[p];
        const arp_t *a = &set.arp[p];
        if (set.part[p].inst >= NINST || set.part[p].mode >= NMODE || set.part[p].vol > 100u || set.part[p].oct < -3 || set.part[p].oct > 3) return 0;
        if (!bytes_le(s, sizeof *s, 127u) || s->w1 >= NWAVE || s->w2 >= NWAVE || s->tune > 48u || s->ltgt >= NLTGT || s->lwave >= NLWAVE) return 0;
        if (!bytes_le(&set.fx[p], sizeof set.fx[p], 127u) || set.fx[p].on >= 1u << NFX) return 0;
        if (a->rate > 3u || a->oct > 3u || a->gate > 3u || a->notes > 1u) return 0;
    }
    if (set.pad.inst >= NINST || INST_CAT(set.pad.inst) != CAT_PAD || set.pad.bars < 1u || set.pad.bars > 4u || set.pad.vol > 100u || set.pad.oct < -3 || set.pad.oct > 3) return 0;
    for (p = 0; p < 16u; p++) {
        uint32_t n = set.pad.note[p / 4u][p % 4u];
        if (n && (n < 24u || n > 108u)) return 0;
    }
    return 1;
}
/* the settings sector or a save slot, into set: this version's, or an older one's brought up to date (what it lacks
 * starts as new). 0 = loaded and sound; -1 = nothing there, or not valid (set is then as new) */
typedef int (*loader_t)(uint32_t i, void *p, uint32_t n);
static int ld_settings(uint32_t i, void *p, uint32_t n) { (void)i; return settings_load(p, n); }
static int ld_slot(uint32_t i, void *p, uint32_t n) { return slot_load(i, p, n); }
static int set_load(loader_t ld, uint32_t i)
{
    uint32_t p;
    set_defaults();
    if (ld(i, &set, sizeof set) == 0) {}
    else if (ld(i, &set, SET_V6_SIZE) == 0 && (set.ver == 5u || set.ver == 6u)) {}
    else if (ld(i, &set, SET_V4_SIZE) == 0 && set.ver == 4u) {}
    else if (ld(i, &set, SET_V3_SIZE) == 0 && (set.ver == 2u || set.ver == 3u)) {}
    else if (ld(i, &set, SET_V1_SIZE) == 0 && set.ver == 1u) { set.ver = 2u; set.split = 12; set.key = 0; }
    else { set_defaults(); return -1; }
    if (set.ver == 2u) {                        /* from before 0.5: the sounds renumbered (7 of the 30 went) */
        for (p = 0; p < NPART; p++) set.part[p].inst = INST_FROM_V2[set.part[p].inst % 30u];
        set.ver = 3u;
    }
    if (set.ver >= 3u && set.ver <= 5u) {       /* from before 0.8: renumbered again (FUZZ, FM BASS went), first */
        for (p = 0; p < NPART; p++) set.part[p].inst = INST_FROM_V5[set.part[p].inst % 23u];
        if (set.ver == 5u) set.pad.inst = INST_FROM_V5[set.pad.inst % 23u];
    }
    if (set.ver >= 3u && set.ver <= 6u) {       /* from before 0.9: renumbered (3 sounds came in); CHORD IN KEY went */
        for (p = 0; p < NPART; p++) {
            set.part[p].inst = INST_FROM_V6[set.part[p].inst % 21u];
            if (set.part[p].mode == M_OCT) set.part[p].mode = M_MAJ;   /* (OCTAVE took CHORD IN KEY's place) */
        }
        if (set.ver >= 5u) {                    /* PADDOCK: its sound renumbered; a note a bar becomes a chord of one */
            uint8_t one[4];
            uint32_t b2;
            set.pad.inst = INST_FROM_V6[set.pad.inst % 21u];
            for (b2 = 0; b2 < 4u; b2++) one[b2] = set.pad.note[0][b2];
            for (b2 = 0; b2 < 16u; b2++) set.pad.note[b2 / 4u][b2 % 4u] = 0;
            for (b2 = 0; b2 < 4u; b2++) set.pad.note[b2][0] = one[b2];
        }
    }
    if (set.ver == 3u) { extras_defaults(); set.ver = 4u; }   /* from before 0.6: sounds as their presets, pedals off */
    if (set.ver == 4u) { pad_defaults(); set.ver = 5u; }   /* from before 0.7: PADDOCK as new */
    if (set.ver == 5u) {                        /* from before 0.8: PADDOCK plays pads only */
        if (INST_CAT(set.pad.inst % NINST) != CAT_PAD) set.pad.inst = PAD0.inst;
        set.ver = 6u;
    }
    if (set.ver == 6u) set.ver = 7u;
    if (set.ver == 7u) {                        /* from before 0.10: the LM-1 kit went (it becomes the 808), the kits after it move up */
        if (set.kit == 4u) set.kit = 5u;        /* (0.10's 808) */
        else if (set.kit > 4u && set.kit <= 9u) set.kit--;
        set.ver = 8u;
    }
    if (set.ver == 8u) {                        /* from before 0.11: HIP-HOP (it becomes LO-FI) and 909 (ROCK) went */
        static const uint8_t FROM_V8[9] = {0, 5, 1, 2, 3, 4, 0, 5, 6};   /* 0.10's ROCK HIP-HOP MINI-POPS CR-78 606 808 909 LO-FI INDUSTRIAL */
        set.kit = FROM_V8[set.kit % 9u];
        set.ver = SET_VER;
    }
    if (!settings_ok()) { set_defaults(); return -1; }
    return 0;
}
static void settings_from_flash(void) { set_load(ld_settings, 0); }

/* ---- the sound: the two instruments and the drums, each in its buffer, then the pedals and the mix (sm_fx.h) */
#define CHUNK 64u
#ifndef DINGHY_HOST
#define AUDIO_BUDGET 139319u                    /* 24 MHz ticks in a 256-frame block (5.8 ms) */
static volatile uint32_t cpu_peak, cpu_reset;   /* the longest app_audio since the main loop last asked (ticks) */
#endif
static void app_audio(int32_t *out, uint32_t n)
{
    int32_t gain[NTB];
    uint32_t i;
#ifndef DINGHY_HOST
    static uint32_t reset_done;
    uint32_t t0 = fm1_ticks(), dt;
#endif
    gain[0] = set.part[0].vol * 8192 / 100;     /* (the instruments x 2: one voice alone is quiet) */
    gain[1] = set.part[1].vol * 8192 / 100;
    gain[2] = set.dvol * 4096 / 100;
    gain[3] = set.pad.vol * 8192 / 100;
    synth_events();
    while (n) {
        uint32_t m = n > FXN ? FXN : n, j = 0, t;
        for (t = 0; t < NTB; t++) for (i = 0; i < m; i++) tb[t][i] = 0;
        while (j < m) {                         /* up to the next 16th at a time: its drums and arpeggio notes start first */
            uint32_t seg = clock_advance(m - j > CHUNK ? CHUNK : m - j);
            synth_render(0, &tb[0][j], seg);
            synth_render(1, &tb[1][j], seg);
            synth_render(P_PAD, &tb[3][j], seg);
            drums_render(&tb[2][j], seg);
            arp_gates(seg);
            j += seg;
        }
        fx_process(out, m, gain);
        for (i = 0; i < 2u * m; i++) {          /* a soft knee, then the wall */
            int32_t s = out[i];
            if (s > 16384) s = 16384 + ((s - 16384) >> 1);
            else if (s < -16384) s = -16384 + ((s + 16384) >> 1);
            out[i] = s > 32767 ? 32767 : s < -32767 ? -32767 : s;
        }
        out += 2u * m;
        n -= m;
    }
#ifndef DINGHY_HOST
    dt = fm1_ticks() - t0;
    if (dt > AUDIO_BUDGET * 4u / 5u) synth_shed();   /* over 80 %: the quietest fading note goes, before it drops out */
    if (reset_done != cpu_reset) { reset_done = cpu_reset; cpu_peak = 0; }
    if (dt > cpu_peak) cpu_peak = dt;
#endif
}
static int quiet(void) { return !clk_run && drums_quiet() && synth_quiet(); }

/* ---- the keys */
#define KEY0 53u                                /* key 0 = F3 */
#define NKEY 27u
#define SPLIT ((uint32_t)set.split)             /* keys 0..SPLIT-1 LEFT, the rest RIGHT (SELECT; 12 = F3..E4 | F4..G5) */
#define KEY_PART(k) ((k) >= SPLIT ? 1u : 0u)
static int black(uint32_t note) { return (0x54Au >> (note % 12u)) & 1u; }
static uint8_t key_root[NKEY];                  /* the root each held key plays (0: none, or forgotten by a reset) */
static uint8_t key_part[NKEY];                  /* the side it plays it on (kept: the split may move while it sounds) */
static uint8_t midi_held[128];                  /* MIDI notes held: their part + 1 */
/* LATCH (REC on the split screen): the notes the keys held were playing go on, kept here (side, root), not on the keys:
 * the keys are free at once, to play anything on whichever side they are now (the split may have moved) */
static struct { uint8_t part, root; } latched[NKEY];
static uint32_t latch_n;                        /* how many (0: LATCH off) */

static void part_play(uint32_t p, uint32_t root, uint32_t on)   /* a key (or a MIDI note) in the part's mode */
{
    uint32_t m = part_mode(p), c, i;
    uint8_t nt[4];
    if (MODE_IS_ARP(m)) { live_note(p, root, on); return; }
    c = chord_of(m, root, nt);
    for (i = 0; i < c; i++) live_note(p, nt[i], on);
}
static void part_reset(uint32_t p)              /* its sound, mode or octave changes: every note of it stops */
{
    uint32_t k;
    live_note(p, 0, 2);
    for (k = 0; k < NKEY; k++) if (key_part[k] == p) key_root[k] = 0;
    for (k = 0; k < latch_n; )                  /* (its latched notes have stopped too) */
        if (latched[k].part == p) latched[k] = latched[--latch_n]; else k++;
    for (k = 0; k < 128u; k++) if (midi_held[k] == p + 1u) midi_held[k] = 0;
}
static void all_reset(void)                     /* the split or the key moves: every note stops */
{
    uint32_t k;
    part_reset(0); part_reset(1);
    for (k = 0; k < NKEY; k++) key_root[k] = 0;
    latch_n = 0;
}
static void latch_release(void)                 /* REC again: the latched notes end, each on its own side */
{
    uint32_t i;
    for (i = 0; i < latch_n; i++) {
        part_play(latched[i].part, latched[i].root, 0);
        midi_send(0x80u | latched[i].part, latched[i].root, 0);
    }
    latch_n = 0;
}
static const char *const NOTE_NAME[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
static uint32_t key_note(uint32_t k)            /* the root key k plays now */
{
    uint32_t p = KEY_PART(k);
    int32_t n = (int32_t)(KEY0 + k) + set.key + 12 * set.part[p].oct + CAT_TRANS[INST_CAT(part_inst(p))];
    return (uint32_t)(n < 12 ? 12 : n > 120 ? 120 : n);
}

/* ---- MIDI in */
static uint32_t idle_ok;
static void midi_in(void)
{
    uint8_t st, d1, d2;
    while (midi_get(&st, &d1, &d2) != 0) {
        uint32_t ty = st & 0xF0u, p = d1 < KEY0 + SPLIT ? 0u : 1u;   /* (split where the keys are split) */
        int32_t nn = (int32_t)d1 + set.key;     /* (in the key, like the keys) */
        if (d1 > 127u) continue;
        if (ty == 0x90u && d2) {
            if (midi_held[d1]) continue;
            if (nn < 1 || nn > 127) continue;
            midi_held[d1] = (uint8_t)(p + 1u);
            part_play(p, (uint32_t)nn, 1);
        } else if (ty == 0x80u || ty == 0x90u) {
            if (!midi_held[d1]) continue;
            if (nn >= 1 && nn <= 127) part_play(midi_held[d1] - 1u, (uint32_t)nn, 0);
            midi_held[d1] = 0;
        }
    }
}
static void app_idle(void) { if (idle_ok) midi_in(); }

/* ---- the screen, milk themed: a cream background, white cards, carton-blue ink. LEFT is CHOC (chocolate milk),
 * RIGHT is BERRY (strawberry milk); the halves' headers are cow print. */
static const uint8_t PART_RGB[2][3] = {{139, 90, 60}, {255, 122, 162}};   /* chocolate, strawberry */
static const char *const PART_NAME[2] = {"CHOC", "BERRY"};
#define C_BG RGB(255, 246, 229)                 /* cream */
#define C_CARD RGB(255, 255, 255)               /* milk */
#define C_INK RGB(30, 58, 110)                  /* the carton's blue */
#define C_SUB RGB(122, 139, 168)
#define C_DIMC RGB(232, 226, 212)               /* empty: a step */
#define C_DIMC4 RGB(214, 206, 190)              /* an empty step on a beat */
#define C_SPOT RGB(26, 26, 30)                  /* cow patches */
#define C_SKY RGB(126, 156, 206)                /* a drum's steps (not the chosen drum) */
static uint16_t prgb(uint32_t p, uint32_t k)    /* the flavour's colour, k % of the way to white */
{
    const uint8_t *c = PART_RGB[p & 1u];
    return RGB(c[0] + (255u - c[0]) * k / 100u, c[1] + (255u - c[1]) * k / 100u, c[2] + (255u - c[2]) * k / 100u);
}
static uint16_t pink(void) { return prgb(1, 0); }
static uint16_t flavour_ink(uint32_t p) { return p ? C_INK : C_BG; }   /* text on the flavour's colour */
static uint32_t mix(uint32_t h, uint32_t v) { return (h ^ v) * 16777619u; }
static char *put(char *p, const char *a) { while (*a) *p++ = *a++; *p = 0; return p; }
static char *put_num(char *p, int32_t v)
{
    char t[12];
    uint32_t n = 0, u;
    if (v < 0) { *p++ = '-'; u = (uint32_t)-v; } else u = (uint32_t)v;
    do { t[n++] = (char)('0' + u % 10u); u /= 10u; } while (u);
    while (n) *p++ = t[--n];
    *p = 0;
    return p;
}
static char *put_note(char *p, uint32_t note)
{
    static const char *const NN[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    p = put(p, NN[note % 12u]);
    return put_num(p, (int32_t)(note / 12u) - 1);
}
static void text_right(int32_t xr, int32_t y, const char *s, uint16_t c, uint16_t bg) { cv_text_bg(xr - text_w(&FONT_S, s), y, &FONT_S, s, c, bg); }
static void text_mid(int32_t xc, int32_t y, const felucca_font_t *f, const char *s, uint16_t c, uint16_t bg) { cv_text_bg(xc - text_w(f, s) / 2, y, f, s, c, bg); }

/* ---- shapes: ellipses, milk drops, cow print, a glass */
static int32_t clip_x0 = -999, clip_y0 = -999, clip_x1 = 999, clip_y1 = 999;   /* ellipses stay inside this box */
static void ellipse(int32_t cx, int32_t cy, int32_t rx, int32_t ry, uint16_t c)
{
    int32_t dy;
    if (ry < 1) ry = 1;
    for (dy = -ry; dy <= ry; dy++) {
        int32_t h = rx * isqrt_small(ry * ry - dy * dy) / ry, x0 = cx - h, x1 = cx + h + 1, y = cy + dy;
        if (y < clip_y0 || y >= clip_y1) continue;
        if (x0 < clip_x0) x0 = clip_x0;
        if (x1 > clip_x1) x1 = clip_x1;
        if (x1 > x0) cv_rect(x0, y, x1 - x0, 1, c);
    }
}
static void disc(int32_t cx, int32_t cy, int32_t r, uint16_t c) { ellipse(cx, cy, r, r, c); }
static void drop(int32_t cx, int32_t cy, int32_t r, uint16_t c)   /* a drop: round below, a point above */
{
    int32_t i;
    disc(cx, cy, r, c);
    for (i = 0; i < 2 * r; i++) {
        int32_t w = r * i / (2 * r);
        cv_rect(cx - w, cy - 2 * r + i, 2 * w + 1, 1, c);
    }
}
/* cow hide: a few big patches with soft, lumpy edges, placed by hand so they leave plenty of white and run off
 * the strip's edges (as if the strip were cut from a cow). Each patch is 4 ellipses: {dx, dy, rx, ry} */
static const int8_t PATCH[3][4][4] = {
    {{0, 0, 10, 7}, {8, -3, 6, 5}, {-7, 4, 6, 4}, {3, 6, 5, 3}},
    {{0, 0, 7, 8}, {-6, -5, 5, 4}, {6, 3, 5, 6}, {-2, 8, 4, 3}},
    {{0, 0, 12, 6}, {9, 4, 6, 4}, {-10, -2, 5, 4}, {1, -5, 6, 3}}};
static void cow_patch(int32_t cx, int32_t cy, uint32_t shape, int32_t flip)
{
    uint32_t i;
    for (i = 0; i < 4u; i++) {
        const int8_t *e = PATCH[shape % 3u][i];
        ellipse(cx + (flip ? -e[0] : e[0]), cy + e[1], e[2], e[3], C_SPOT);
    }
}
/* where the patches lie: {x, y, shape} from the strip's corner. A half's header (RIGHT's is the mirror image), the
 * drum page's header */
static const int16_t COW_HALF[2][3][3] = {     /* (right of each half's tag: CHOC's is shorter, more hide shows) */
    {{78, 6, 2}, {100, 22, 1}, {66, 27, 0}},
    {{86, 4, 0}, {104, 24, 2}, {78, 28, 1}}};
static const int16_t COW_WIDE[6][3] = {{10, 28, 1}, {64, 2, 2}, {116, 27, 0}, {150, 1, 1}, {192, 29, 2}, {234, 4, 0}};
static void cow_strip(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t layout, uint16_t bg)   /* layout 0 / 1 half, 2 wide */
{
    int32_t r = 6, i, n = layout == 2u ? 6 : 3;
    cv_rect(x, y, w, h, C_CARD);
    clip_x0 = x; clip_y0 = y; clip_x1 = x + w; clip_y1 = y + h;
    for (i = 0; i < n; i++) {
        const int16_t *c = layout == 2u ? COW_WIDE[i] : COW_HALF[layout & 1u][i];
        cow_patch(x + c[0], y + c[1], (uint32_t)c[2], (int32_t)(layout & 1u));
    }
    clip_x0 = clip_y0 = -999; clip_x1 = clip_y1 = 999;
    for (i = 0; i < r; i++) {                   /* round corners */
        int32_t d = r - 1 - i, ins = r - isqrt_small(r * r - d * d);
        cv_rect(x, y + i, ins, 1, bg); cv_rect(x + w - ins, y + i, ins, 1, bg);
        cv_rect(x, y + h - 1 - i, ins, 1, bg); cv_rect(x + w - ins, y + h - 1 - i, ins, 1, bg);
    }
}
static void pill(int32_t x, int32_t y, const char *s, uint16_t fill, uint16_t ink)   /* a tag, with a milk-white rim */
{
    int32_t w = text_w(&FONT_S, s) + 12;
    cv_rrect(x - 2, y - 2, w + 4, 24, 8, C_CARD);
    cv_rrect(x, y, w, 20, 6, fill);
    cv_text_bg(x + 6, y + 2, &FONT_S, s, ink, fill);
}
static void glass(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t pct, uint16_t milk)   /* a glass, pct full */
{
    int32_t i, lvl = (h - 4) * (int32_t)pct / 100;
    for (i = 0; i < h; i++) {                   /* a tumbler, narrower at the foot */
        int32_t ins = 3 * i / h, fb = h - 2 - i;
        cv_rect(x + ins, y + i, w - 2 * ins, 1, C_INK);
        if (i > 0 && i < h - 2)
            cv_rect(x + ins + 2, y + i, w - 2 * ins - 4, 1, fb < lvl ? (fb == lvl - 1 ? C_CARD : milk) : C_CARD);
    }
}

/* a little keyboard of the part's keys, the held ones in its flavour */
static void mini_keys(uint32_t p, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t held)
{
    uint32_t k0 = p ? SPLIT : 0u, k1 = p ? NKEY : SPLIT, k, nw = 0, i;
    int32_t kw;
    for (k = k0; k < k1; k++) if (!black(KEY0 + k)) nw++;
    if (!nw) nw = 1;                            /* (a half that is one black key) */
    kw = w / (int32_t)nw;
    if (kw > 14) kw = 14;                       /* (a few keys: piano-sized, not stretched across the half) */
    cv_rrect(x - 2, y - 2, kw * (int32_t)nw + 3, h + 4, 3, C_INK);
    for (k = k0, i = 0; k < k1; k++)
        if (!black(KEY0 + k)) {
            cv_rect(x + (int32_t)i * kw, y, kw - 1, h, held >> k & 1u ? prgb(p, 0) : C_BG);
            i++;
        }
    for (k = k0, i = 0; k < k1; k++) {
        if (!black(KEY0 + k)) { i++; continue; }
        cv_rect(x + (int32_t)i * kw - kw / 3, y, kw * 2 / 3, h * 3 / 5, held >> k & 1u ? prgb(p, 0) : C_INK);
    }
}

#define PW 118
#define PH 206
#define BAR_Y 208
static void draw_panel(uint32_t p, uint32_t active, uint32_t held)
{
    const part_t *pt = &set.part[p];
    uint32_t inst = part_inst(p);
    uint16_t bg = C_CARD, lab = active ? C_INK : C_SUB, lo, hi;
    int32_t y;
    char b[24], *q;
    cv_begin(PW, PH, C_BG);
    if (active) { cv_rrect(0, 0, PW, PH, 9, prgb(p, 0)); cv_rrect(3, 3, PW - 6, PH - 6, 7, bg); }
    else { cv_rrect(0, 0, PW, PH, 9, C_DIMC); cv_rrect(1, 1, PW - 2, PH - 2, 8, bg); }
    cow_strip(6, 6, PW - 12, 26, p, bg);   /* the header: cow print, the flavour's tag, its button */
    {                                           /* the tag: its button's sign (OCT- / OCT+) in a dot, then the name */
        int32_t w = text_w(&FONT_S, PART_NAME[p]) + 32;
        uint16_t f = prgb(p, 0), dot = active ? C_INK : C_CARD, sign = active ? C_CARD : C_INK;
        cv_rrect(8, 7, w + 4, 24, 8, C_CARD);
        cv_rrect(10, 9, w, 20, 6, f);
        disc(20, 19, 7, dot);
        cv_rect(16, 18, 9, 3, sign);
        if (p) cv_rect(19, 15, 3, 9, sign);
        cv_text_bg(32, 11, &FONT_S, PART_NAME[p], flavour_ink(p), f);
    }
    y = 38;
    cv_text_bg(10, y, &FONT_S, "1 OCTAVE", lab, bg);
    q = b; if (pt->oct > 0) *q++ = '+'; put_num(q, pt->oct);
    cv_text_bg(10, y + 16, &FONT_S, b, C_INK, bg);
    lo = (uint16_t)key_note(p ? SPLIT : 0u); hi = (uint16_t)key_note(p ? NKEY - 1u : SPLIT - 1u);
    q = put_note(b, lo); q = put(q, "-"); put_note(q, hi);
    text_right(PW - 10, y + 16, b, C_SUB, bg);
    y += 34;
    cv_text_bg(10, y, &FONT_S, "2 SOUND", lab, bg);
    text_right(PW - 10, y, CAT_NAME[INST_CAT(inst)], C_SUB, bg);
    cv_text_bg(10, y + 16, &FONT_S, INST_NAME[inst], C_INK, bg);
    y += 34;
    cv_text_bg(10, y, &FONT_S, "3 MODE", lab, bg);
    cv_text_bg(10, y + 16, &FONT_S, MODE_NAME[part_mode(p)], C_INK, bg);
    y += 34;
    cv_text_bg(10, y, &FONT_S, "4 VOLUME", lab, bg);
    q = put_num(b, pt->vol); put(q, "%");
    cv_text_bg(10, y + 16, &FONT_S, b, C_INK, bg);
    glass(PW - 34, y + 2, 22, 30, pt->vol, prgb(p, 0));   /* the volume as a glass of the flavour's milk */
    mini_keys(p, 11, 178, PW - 22, 20, held);
    cv_blit(p ? 240 - PW : 0, 0);
}

/* ---- the option pages: the pedals (FX SEL ENV LFO EDIT), the arpeggiator (ARP), the wave editor (GLO), the save
 * page (SAVE). Each shows the instrument OCT- / OCT+ chose. Top: a cow-hide strip with the page's name and the
 * instrument's tag; middle: its controls, numbered after the knobs; bottom: a note (how to reset it, a message) */
enum { PG_HOME, PG_DRUMS, PG_FX, PG_ARP = PG_FX + NFX, PG_EDIT, PG_SAVE };
static const char *const FX_NAME[NFX] = {"REVERB", "DELAY", "DISTORTION", "CHORUS", "PHASER"};
static const char *const FX_BTN_NAME[NFX] = {"FX", "SEL", "ENV", "LFO", "EDIT"};
static const uint8_t FX_BTN[NFX] = {BTN_FX, BTN_SEL, BTN_ENV, BTN_LFO, BTN_EDIT};
/* each pedal is a milkshake flavour: blueberry, vanilla, caramel, mint, grape */
static const uint8_t FX_RGB[NFX][3] = {{150, 182, 238}, {246, 222, 140}, {232, 156, 80}, {142, 216, 182}, {188, 152, 224}};
static const char *const FX_FLAVOUR[NFX] = {"BLUEBERRY", "VANILLA", "CARAMEL", "MINT", "GRAPE"};
#define MID_Y 36                                /* the middle: rows 36 .. 205 */
#define MID_H 170
#define FOOT_Y 208
static uint16_t rgbv(const uint8_t *c, uint32_t k) { return RGB(c[0] + (255u - c[0]) * k / 100u, c[1] + (255u - c[1]) * k / 100u, c[2] + (255u - c[2]) * k / 100u); }

static void side_tag(int32_t xr, int32_t y, uint32_t p)   /* the instrument's tag, its right edge at xr: "- CHOC" */
{
    int32_t w = text_w(&FONT_S, PART_NAME[p]) + 32, x = xr - w;
    uint16_t f = prgb(p, 0);
    cv_rrect(x - 2, y - 2, w + 4, 24, 8, C_CARD);
    cv_rrect(x, y, w, 20, 6, f);
    disc(x + 10, y + 10, 7, C_INK);
    cv_rect(x + 6, y + 9, 9, 3, C_CARD);
    if (p) cv_rect(x + 9, y + 6, 3, 9, C_CARD);
    cv_text_bg(x + 22, y + 2, &FONT_S, PART_NAME[p], flavour_ink(p), f);
}
static void draw_page_head(const char *title, uint32_t p)
{
    cv_begin(240, 32, C_BG);
    cow_strip(0, 0, 240, 32, 2u, C_BG);
    pill(8, 6, title, C_INK, C_CARD);
    side_tag(232, 6, p);
    cv_blit(0, 0);
}
static void draw_foot(const char *l1, const char *l2, uint32_t l1_hot)   /* two lines of notes; l1_hot: in pink */
{
    cv_begin(240, 240 - FOOT_Y, C_BG);
    cv_rrect(0, 0, 240, 240 - FOOT_Y, 8, C_CARD);
    if (l1) text_mid(120, 0, &FONT_S, l1, l1_hot ? pink() : C_INK, C_CARD);
    if (l2) text_mid(120, 16, &FONT_S, l2, C_SUB, C_CARD);
    cv_blit(0, FOOT_Y);
}

/* a knob: a ring of dots lit up to the value, a milk-white face with an ink rim, an ink pointer */
static void knob_xy(int32_t cx, int32_t cy, int32_t deg, int32_t r, int32_t *x, int32_t *y)
{
    uint32_t ph = (uint32_t)(deg * 11930465);   /* degrees -> a phase (2^32 a turn) */
    *x = cx + (r * sine(ph)) / 32767;
    *y = cy - (r * sine(ph + 0x40000000u)) / 32767;
}
static void draw_knob(int32_t cx, int32_t cy, uint32_t v, uint32_t mx, uint16_t ring)
{
    int32_t i, x0, y0, x1, y1, deg;
    if (!mx) mx = 1;
    for (i = 0; i <= 18; i++) {                 /* the ring: dots from -135 to +135 degrees */
        knob_xy(cx, cy, -135 + i * 15, 21, &x0, &y0);
        cv_rect(x0 - 1, y0 - 1, 3, 3, (uint32_t)i * mx <= v * 18u && v ? ring : C_DIMC4);
    }
    disc(cx, cy, 16, C_INK);
    disc(cx, cy, 13, C_CARD);
    deg = -135 + (int32_t)(270u * v / mx);
    knob_xy(cx, cy, deg, 3, &x0, &y0);
    knob_xy(cx, cy, deg, 12, &x1, &y1);
    cv_line(x0, y0, x1, y1, C_INK);
    cv_line(x0 + 1, y0, x1 + 1, y1, C_INK);
    cv_line(x0, y0 + 1, x1, y1 + 1, C_INK);
}
static void badge(int32_t x, int32_t y, uint32_t n)   /* the knob's number in an ink dot */
{
    char s[2];
    s[0] = (char)('0' + n); s[1] = 0;
    disc(x + 8, y + 8, 8, C_INK);
    text_mid(x + 8, y, &FONT_S, s, C_CARD, C_INK);
}
typedef struct { const char *name; uint32_t v, mx; const char *val; uint8_t shared, sw; } kview_t;
/* a control card (2 x 2 of them in the middle), x, y in the middle's coordinates: the number, the name, a knob (or
 * a switch), the value; "BOTH" when the setting is the same for both instruments */
static void control_card(int32_t x, int32_t y, uint32_t n, const kview_t *k, uint16_t accent)
{
    cv_rrect(x, y, 112, 79, 8, C_CARD);
    badge(x + 6, y + 6, n);
    cv_text_bg(x + 28, y + 6, &FONT_S, k->name, C_INK, C_CARD);
    if (k->sw) {                                /* KNOB1 on a pedal: ON / OFF, a switch */
        int32_t on = k->v != 0u;
        cv_rrect(x + 10, y + 38, 44, 24, 12, on ? accent : C_DIMC4);
        disc(on ? x + 42 : x + 22, y + 50, 9, C_CARD);
        disc(on ? x + 42 : x + 22, y + 50, 7, on ? C_INK : C_SUB);
    } else draw_knob(x + 32, y + 50, k->v, k->mx, accent);
    text_right(x + 106, y + 42, k->val, C_INK, C_CARD);
    if (k->shared) text_right(x + 106, y + 60, "BOTH", C_SUB, C_CARD);
}
static void draw_controls(const kview_t k[4], const uint8_t *rgb)   /* the middle: a flavoured board, four cards */
{
    uint32_t half, i;
    uint16_t body = rgbv(rgb, 0);
    for (half = 0; half < 2u; half++) {
        cv_begin(240, MID_H / 2, C_BG);
        cv_oy = -(int32_t)half * (MID_H / 2);
        cv_rrect(0, 0, 240, MID_H, 10, body);
        for (i = 0; i < 4u; i++) control_card(4 + 120 * (int32_t)(i & 1u), 4 + 83 * (int32_t)(i >> 1), i + 1u, &k[i], C_INK);
        cv_oy = 0;
        cv_blit(0, MID_Y + half * (MID_H / 2));
    }
}

/* ---- a pedal page: KNOB1 on / off, KNOB2-4 its three knobs (0..127, shown 0..20; DELAY's REPEAT a note value) */
typedef struct { const char *name; uint8_t *v; uint8_t max; uint8_t shared; } pknob_t;
static void pedal_knobs(uint32_t p, uint32_t e, pknob_t k[3])
{
    tfx_t *f = &set.fx[p];
    gfx_t *g = &set.gfx;
    switch (e) {
    case E_REV: k[0] = (pknob_t){"LEVEL", &f->rev_send, 127, 0}; k[1] = (pknob_t){"TONE", &g->rtone, 127, 1};
                k[2] = (pknob_t){"TIME", &g->rtime, 127, 1}; break;
    case E_DLY: k[0] = (pknob_t){"REPEAT", &g->dtime, NDLY - 1, 1}; k[1] = (pknob_t){"ECHO", &f->dly_send, 127, 0};
                k[2] = (pknob_t){"INTENSITY", &g->dfb, 127, 1}; break;
    case E_DST: k[0] = (pknob_t){"LEVEL", &f->dlevel, 127, 0}; k[1] = (pknob_t){"TONE", &f->tone, 127, 0};
                k[2] = (pknob_t){"DIST", &f->drive, 127, 0}; break;
    case E_CHO: k[0] = (pknob_t){"RATE", &g->crate, 127, 1}; k[1] = (pknob_t){"DEPTH", &g->cdepth, 127, 1};
                k[2] = (pknob_t){"LEVEL", &f->cho_send, 127, 0}; break;
    default:    k[0] = (pknob_t){"RATE", &f->prate, 127, 0}; k[1] = (pknob_t){"DEPTH", &f->pdepth, 127, 0};
                k[2] = (pknob_t){"FEEDBACK", &f->pfb, 127, 0};
    }
}
#define PEDAL_STEPS 20
static uint32_t pedal_level(uint32_t v) { return (v * PEDAL_STEPS + 63u) / 127u; }   /* 0..127 -> 0..20 */
static void draw_pedal(uint32_t p, uint32_t e)
{
    char val[4][12];
    kview_t kv[4];
    pknob_t pk[3];
    uint32_t i, on = set.fx[p].on >> e & 1u;
    pedal_knobs(p, e, pk);
    put(val[0], on ? "ON" : "OFF");
    kv[0] = (kview_t){"ON/OFF", on, 1, val[0], 0, 1};
    for (i = 0; i < 3u; i++) {
        uint32_t v = *pk[i].v;
        if (pk[i].max == 127u) put_num(val[i + 1u], (int32_t)pedal_level(v));
        else put(val[i + 1u], DLY_NAME[v % NDLY]);
        kv[i + 1u] = (kview_t){pk[i].name, v, pk[i].max, val[i + 1u], pk[i].shared, 0};
    }
    draw_controls(kv, FX_RGB[e]);
}
static void pedal_reset(uint32_t p, uint32_t e)   /* hold REC on a pedal page: that pedal as new, and off */
{
    tfx_t *f = &set.fx[p];
    gfx_t *g = &set.gfx;
    f->on &= (uint8_t)~(1u << e);
    switch (e) {
    case E_REV: f->rev_send = FX0.rev_send; g->rtone = GFX0.rtone; g->rtime = GFX0.rtime; break;
    case E_DLY: f->dly_send = FX0.dly_send; g->dtime = GFX0.dtime; g->dfb = GFX0.dfb; break;
    case E_DST: f->drive = FX0.drive; f->tone = FX0.tone; f->dlevel = FX0.dlevel; break;
    case E_CHO: f->cho_send = FX0.cho_send; g->crate = GFX0.crate; g->cdepth = GFX0.cdepth; break;
    default:    f->prate = FX0.prate; f->pdepth = FX0.pdepth; f->pfb = FX0.pfb;
    }
}

/* ---- the ARP page: KNOB1 rate, KNOB2 octaves, KNOB3 gate, KNOB4 notes (the direction is the mode, KNOB3 at home) */
static const uint8_t ARP_RGB[3] = {255, 214, 120};   /* butterscotch */
static void draw_arp(uint32_t p)
{
    static const char *const OCT_NAME[4] = {"1", "2", "3", "4"};
    char g[8];
    const arp_t *a = &set.arp[p];
    kview_t kv[4];
    put(put_num(g, ARP_GATE_PCT[a->gate & 3u]), "%");
    kv[0] = (kview_t){"RATE", a->rate & 3u, 3, ARP_RATE_NAME[a->rate & 3u], 0, 0};
    kv[1] = (kview_t){"OCTAVES", a->oct & 3u, 3, OCT_NAME[a->oct & 3u], 0, 0};
    kv[2] = (kview_t){"GATE", a->gate & 3u, 3, g, 0, 0};
    kv[3] = (kview_t){"NOTES", a->notes & 1u, 1, ARP_NOTES_NAME[a->notes & 1u], 0, 0};
    draw_controls(kv, ARP_RGB);
}

/* ---- the wave editor (GLO): the instrument's sound, four pages (SELECT): OSC, FILTER, ENV, LFO. A picture of what
 * the page shapes, then its four settings, each on a knob */
enum { ED_OSC, ED_FILT, ED_ENV, ED_LFO, NED };
static const char *const ED_NAME[NED] = {"OSC", "FILTER", "ENV", "LFO"};
static const char *const WAVE_NAME[NWAVE] = {"SINE", "TRI", "SAW", "SQUARE", "NOISE", "FM"};
static const char *const LTGT_NAME[NLTGT] = {"PITCH", "FILTER", "VOLUME"};
static const char *const LWAVE_NAME[NLWAVE] = {"TRI", "SQUARE", "SAW", "RANDOM"};
typedef struct { const char *name; uint8_t off, max, step; } eparam_t;   /* off: its place in snd_t */
#define SO(f) ((uint8_t)__builtin_offsetof(snd_t, f))
static const eparam_t EPARAM[NED][4] = {
    {{"WAVE 1", SO(w1), NWAVE - 1, 1}, {"WAVE 2", SO(w2), NWAVE - 1, 1}, {"TUNE 2", SO(tune), 48, 1}, {"MIX", SO(mix), 127, 4}},
    {{"CUTOFF", SO(cut), 127, 4}, {"RESONANCE", SO(res), 127, 4}, {"ENV AMT", SO(fenv), 127, 4}, {"", 0, 0, 0}},
    {{"ATTACK", SO(a), 127, 4}, {"DECAY", SO(d), 127, 4}, {"SUSTAIN", SO(s), 127, 4}, {"RELEASE", SO(r), 127, 4}},
    {{"RATE", SO(lrate), 127, 4}, {"DEPTH", SO(ldepth), 127, 4}, {"TARGET", SO(ltgt), NLTGT - 1, 1}, {"SHAPE", SO(lwave), NLWAVE - 1, 1}}};
static void eparam_text(char *b, uint32_t tab, uint32_t i, uint32_t v)
{
    if (tab == ED_OSC && i < 2u) put(b, WAVE_NAME[v % NWAVE]);
    else if (tab == ED_OSC && i == 2u) { if (v > 24u) *b++ = '+'; put_num(b, (int32_t)v - 24); }
    else if (tab == ED_LFO && i == 2u) put(b, LTGT_NAME[v % NLTGT]);
    else if (tab == ED_LFO && i == 3u) put(b, LWAVE_NAME[v % NLWAVE]);
    else put(put_num(b, (int32_t)(v * 100u / 127u)), "%");
}
static void plot_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t c)   /* a 2 px line */
{
    cv_line(x0, y0, x1, y1, c);
    cv_line(x0, y0 + 1, x1, y1 + 1, c);
}
#define GX 8                                    /* the picture's box, in the middle's coordinates */
#define GY 26
#define GW 224
#define GH 70
static void draw_edit_top(uint32_t p, uint32_t tab)   /* the tabs and the picture: the middle's rows 0..99 */
{
    const snd_t *s = &set.snd[p];
    uint16_t fl = prgb(p, 0), pale = prgb(p, 65);
    int32_t i, mid = GY + GH / 2, px = 0, py = 0;
    cv_begin(240, 100, C_BG);
    for (i = 0; i < (int32_t)NED; i++) {        /* the tabs: the page shown in ink */
        int32_t x = 4 + i * 59;
        uint32_t me = (uint32_t)i == tab;
        cv_rrect(x, 0, 55, 22, 7, me ? C_INK : C_CARD);
        text_mid(x + 27, 3, &FONT_S, ED_NAME[i], me ? C_CARD : C_SUB, me ? C_INK : C_CARD);
    }
    cv_rrect(GX - 4, GY - 4, GW + 8, GH + 8, 8, C_CARD);
    if (tab == ED_OSC) {                        /* two cycles of the oscillators, mixed as the engine mixes them */
        uint32_t tune = s->tune > 48u ? 48u : s->tune, nz = 777u;
        cv_rect(GX, mid, GW, 1, C_DIMC);
        for (i = 0; i < GW; i++) {
            uint32_t ph = (uint32_t)i * (0xFFFFFFFFu / (GW / 2)), ph2 = (uint32_t)(((uint64_t)ph * SEMI[tune]) >> 16);
            int32_t v, y;
            if (s->w1 == W_FM) v = sine(ph + ((uint32_t)(sine(ph2) * (int32_t)s->mix) << 9));
            else {
                int32_t a = s->w1 == W_NOISE ? (nz ^= nz << 13, nz ^= nz >> 17, nz ^= nz << 5, (int32_t)nz >> 16) : osc_wave(s->w1 % NWAVE, ph);
                int32_t b = s->w2 == W_NOISE ? (nz ^= nz << 13, nz ^= nz >> 17, nz ^= nz << 5, (int32_t)nz >> 16) : osc_wave(s->w2 % NWAVE, ph2);
                v = s->mix ? (a * (int32_t)(128u - s->mix) + b * (int32_t)s->mix) >> 7 : a;
            }
            y = mid - (v * (GH / 2 - 3)) / 32767;
            if (i) plot_line(GX + px, py, GX + i, y, C_INK);
            px = i; py = y;
        }
    } else if (tab == ED_FILT) {                /* the filter's response: flat, a bump at the cutoff, then falling; the
                                                   envelope opens it to the pale curve */
        int32_t pass;
        for (pass = 1; pass >= 0; pass--) {
            int32_t c = (int32_t)s->cut + (pass ? (int32_t)s->fenv : 0);
            if (c > 127) c = 127;
            for (i = 0; i < GW; i++) {
                int32_t f = i * 127 / GW, d = f - c, h, y;
                h = d <= 0 ? 0 : -d * 3;        /* below the cutoff: 0; above: falling */
                h += (int32_t)s->res * 18 / (8 + d * d);   /* the resonance: a peak at the cutoff */
                y = GY + GH - 12 - (GH - 24) * 2 / 3 - h * (GH - 24) / 127;
                if (y < GY + 2) y = GY + 2;
                if (y > GY + GH - 2) y = GY + GH - 2;
                if (!pass) cv_rect(GX + i, y, 1, GY + GH - y, pale);
                if (i) plot_line(GX + px, py, GX + i, y, pass ? C_DIMC4 : C_INK);
                px = i; py = y;
            }
        }
        i = (int32_t)s->cut * GW / 127;
        cv_rect(GX + i, GY, 1, GH, fl);
    } else if (tab == ED_ENV) {                 /* the envelope: attack, decay to the sustain level, held, release */
        int32_t wa = 4 + (int32_t)s->a * (int32_t)s->a / 290, wd = 4 + (int32_t)s->d * (int32_t)s->d / 290;
        int32_t wr = 4 + (int32_t)s->r * (int32_t)s->r / 290, top = GY + 4, bot = GY + GH - 2;
        int32_t sy = bot - ((int32_t)s->s * (bot - top)) / 127, x0 = GX + 2, x1 = x0 + wa, x2 = x1 + wd, x3 = GX + GW - 2 - wr, x4 = GX + GW - 2;
        if (x3 < x2 + 8) x3 = x2 + 8;
        for (i = x0; i <= x4; i++) {            /* the shape, filled pale */
            int32_t y = i < x1 ? bot - (bot - top) * (i - x0) / (wa ? wa : 1) : i < x2 ? top + (sy - top) * (i - x1) / (wd ? wd : 1)
                      : i < x3 ? sy : sy + (bot - sy) * (i - x3) / (x4 - x3 ? x4 - x3 : 1);
            cv_rect(i, y, 1, bot - y, pale);
        }
        plot_line(x0, bot, x1, top, C_INK);
        plot_line(x1, top, x2, sy, C_INK);
        plot_line(x2, sy, x3, sy, C_INK);
        plot_line(x3, sy, x4, bot, C_INK);
    } else {                                    /* the LFO: its shape; more waves the faster, taller the deeper */
        uint32_t cyc = 1u + s->lrate / 26u, rnd = 4242u, seg = ~0u;
        int32_t amp = 2 + (int32_t)s->ldepth * (GH / 2 - 4) / 127, last = 0;
        cv_rect(GX, mid, GW, 1, C_DIMC);
        for (i = 0; i < GW; i++) {
            uint32_t ph = (uint32_t)i * (0xFFFFFFFFu / (uint32_t)GW) * cyc;
            int32_t v, y, sv = (int32_t)ph;
            switch (s->lwave) {
            case LW_TRI: v = ((sv ^ (sv >> 31)) >> 15) - 32768; break;
            case LW_SQR: v = sv < 0 ? -32767 : 32767; break;
            case LW_SAW: v = 32767 - (int32_t)(ph >> 16); break;
            default:
                if ((uint32_t)i * cyc * 4u / GW != seg) {   /* RANDOM: a new level 4 times a cycle */
                    seg = (uint32_t)i * cyc * 4u / GW;
                    rnd ^= rnd << 13; rnd ^= rnd >> 17; rnd ^= rnd << 5; last = (int32_t)rnd >> 16;
                }
                v = last;
            }
            y = mid - v * amp / 32767;
            if (i) plot_line(GX + px, py, GX + i, y, C_INK);
            px = i; py = y;
        }
    }
    cv_blit(0, MID_Y);
}
static void draw_edit_knobs(uint32_t p, uint32_t tab)   /* the four settings: the middle's rows 100..169 */
{
    const uint8_t *sb = (const uint8_t *)&set.snd[p];
    uint32_t i;
    char b[12];
    cv_begin(240, 70, C_BG);
    for (i = 0; i < 4u; i++) {
        const eparam_t *e = &EPARAM[tab][i];
        int32_t x = 4 + 120 * (int32_t)(i & 1u), y = 2 + 34 * (int32_t)(i >> 1);
        cv_rrect(x, y, 112, 31, 7, C_CARD);
        if (!e->max) continue;
        badge(x + 4, y + 7, i + 1u);
        cv_text_bg(x + 24, y + 1, &FONT_S, e->name, C_SUB, C_CARD);
        eparam_text(b, tab, i, sb[e->off]);
        text_right(x + 106, y + 14, b, C_INK, C_CARD);
        cv_rect(x + 24, y + 24, 30, 3, C_DIMC);   /* (short: a long value, SQUARE, sits to its right) */
        cv_rect(x + 24, y + 24, 30 * (int32_t)sb[e->off] / (int32_t)e->max, 3, prgb(p, 0));
    }
    cv_blit(0, MID_Y + 100);
}
static void edit_reset(uint32_t p, uint32_t tab)   /* hold REC on the editor: the page's settings as the preset has them */
{
    uint8_t *sb = (uint8_t *)&set.snd[p];
    const uint8_t *pb = (const uint8_t *)&SND[part_inst(p)];
    uint32_t i;
    for (i = 0; i < 4u; i++) if (EPARAM[tab][i].max) sb[EPARAM[tab][i].off] = pb[EPARAM[tab][i].off];
}

/* ---- the save page (SAVE): 4 slots, each the whole setup. KNOB1 picks one; REC saves, PLAY loads, each asking
 * first (the same button again within 3 s). A load is checked whole before it replaces your work */
#define NPROJ 4u
static struct { uint8_t full, bpm, inst[2]; } proj_info[NPROJ];
static void draw_save(uint32_t slot, uint32_t confirm)
{
    uint32_t half, i;
    char s[32], *q;
    for (half = 0; half < 2u; half++) {
        cv_begin(240, MID_H / 2, C_BG);
        cv_oy = -(int32_t)half * (MID_H / 2);
        for (i = 0; i < NPROJ; i++) {
            int32_t y = (int32_t)i * 42;
            uint32_t me = i == slot, full = proj_info[i].full;
            if (me) { cv_rrect(0, y, 240, 39, 8, C_INK); cv_rrect(2, y + 2, 236, 35, 7, C_CARD); }
            else cv_rrect(0, y, 240, 39, 8, C_CARD);
            disc(22, y + 19, 13, me ? C_INK : C_DIMC);
            s[0] = (char)('1' + i); s[1] = 0;
            text_mid(22, y + 11, &FONT_S, s, me ? C_CARD : C_INK, me ? C_INK : C_DIMC);
            q = put(s, "SAVE "); *q++ = (char)('1' + i); *q = 0;
            cv_text_bg(44, y + 3, &FONT_S, s, C_INK, C_CARD);
            if (full) {
                q = put_num(s, proj_info[i].bpm); put(q, " BPM");
                text_right(230, y + 3, s, C_SUB, C_CARD);
                cv_text_bg(44, y + 19, &FONT_S, INST_NAME[proj_info[i].inst[0] % NINST], prgb(0, 0), C_CARD);
                text_right(230, y + 19, INST_NAME[proj_info[i].inst[1] % NINST], prgb(1, 0), C_CARD);
            } else cv_text_bg(44, y + 19, &FONT_S, "EMPTY", C_SUB, C_CARD);
        }
        if (confirm) {                          /* "sure?" */
            const char *l2 = confirm == 1u ? "PRESS REC AGAIN" : "PRESS PLAY AGAIN";
            const char *l3 = confirm == 1u ? (proj_info[slot].full ? "REPLACES WHAT IS THERE" : "YOUR WHOLE SETUP") : "REPLACES YOUR SETUP";
            q = put(s, confirm == 1u ? "SAVE " : "LOAD "); *q++ = (char)('1' + slot); *q++ = '?'; *q = 0;
            cv_rrect(14, 34, 212, 100, 12, pink());
            cv_rrect(18, 38, 204, 92, 10, C_CARD);
            text_mid(120, 44, &FONT_L, s, C_INK, C_CARD);
            text_mid(120, 84, &FONT_S, l2, C_INK, C_CARD);
            text_mid(120, 104, &FONT_S, l3, C_SUB, C_CARD);
        }
        cv_oy = 0;
        cv_blit(0, MID_Y + half * (MID_H / 2));
    }
}

static char *put_key(char *p)                   /* "KEY D +2", "KEY C" */
{
    p = put(p, "KEY ");
    p = put(p, NOTE_NAME[(uint32_t)(set.key + 24) % 12u]);
    if (set.key) { p = put(p, set.key > 0 ? " +" : " "); p = put_num(p, set.key); }
    return p;
}
static void draw_bar(uint32_t run, uint32_t pos, uint32_t say, uint32_t side)   /* the drums along the bottom; say:
                                                   SPLIT (1), KEY (2), pedal e switched (3 + e) */
{
    char b[32], *q;
    uint32_t s;
    cv_begin(240, 240 - BAR_Y, C_BG);
    cv_rrect(0, 0, 240, 240 - BAR_Y, 8, C_CARD);
    if (say == 1u) {                            /* SELECT turned: LEFT's top key | RIGHT's bottom key */
        q = put(b, "SPLIT  "); q = put_note(q, KEY0 + SPLIT - 1u); q = put(q, " | "); put_note(q, KEY0 + SPLIT);
        text_mid(120, 1, &FONT_S, b, pink(), C_CARD);
    } else if (say == 2u) {                     /* PRESETS turned: the key */
        put_key(b);
        text_mid(120, 1, &FONT_S, b, pink(), C_CARD);
    } else if (say == 20u) {                    /* REC held on the split screen */
        text_mid(120, 1, &FONT_S, "EVERYTHING RESET", pink(), C_CARD);
    } else if (say == 21u || say == 22u) {      /* REC tapped: LATCH */
        text_mid(120, 1, &FONT_S, say == 21u ? "LATCH ON" : "LATCH OFF", pink(), C_CARD);
    } else if (say >= 3u && say < 3u + NFX) {   /* a pedal button tapped: "REVERB ON  CHOC" */
        q = put(b, FX_NAME[say - 3u]); q = put(q, set.fx[side].on >> (say - 3u) & 1u ? " ON  " : " OFF  "); put(q, PART_NAME[side]);
        text_mid(120, 1, &FONT_S, b, pink(), C_CARD);
    } else {
        q = put(b, run ? "> " : ""); put(q, KIT_NAME[set.kit % NKIT]);
        if (latch_n) { cv_rrect(6, 1, 56, 17, 6, pink()); cv_text_bg(11, 1, &FONT_S, "LATCH", C_CARD, pink()); }   /* (latched) */
        else cv_text_bg(10, 1, &FONT_S, b, run ? pink() : C_INK, C_CARD);
        q = put_num(b, set.bpm); put(q, " BPM");
        text_right(230, 1, b, C_INK, C_CARD);
        if (set.key) { put_key(b); text_right(230 - 72, 1, b + 4, C_SUB, C_CARD); }   /* the key, when it is not C */
    }
    for (s = 0; s < NSTEP; s++) {               /* the 16 steps as milk drops: ink where a drum plays, pink playing */
        uint32_t any = 0, d;
        for (d = 0; d < NDR; d++) any |= set.dpat[d] >> s & 1u;
        drop(15 + (int32_t)s * 14, 25, 4, run && s == pos ? pink() : any ? C_INK : (s % 4u == 0u ? C_DIMC4 : C_DIMC));
    }
    cv_blit(0, BAR_Y);
}

/* ---- the SEQ page: DAIRY DRUMS and PADDOCK */
static const char *const DR_NAME[NDR] = {"KICK", "SNARE", "HAT", "OPEN", "CLAP", "PERC"};
#define TABS_Y 36                               /* the SEQ page: the tabs, then the page (like the wave editor) */
#define GRID_Y 62
#define ROW_H 22
enum { SQ_DRUMS, SQ_PAD, NSQ };
static const char *const SQ_NAME[NSQ] = {"DAIRY DRUMS", "PADDOCK"};
static void draw_seq_head(uint32_t run, uint32_t cpu)   /* cpu: the sound's load, % (999: not known, on a computer) */
{
    const char *st = run ? "PLAYING" : "STOPPED";
    char c[8];
    cv_begin(240, 32, C_BG);
    cow_strip(0, 0, 240, 32, 2u, C_BG);
    pill(8, 6, "SEQUENCER", C_INK, C_CARD);
    pill(232 - text_w(&FONT_S, st) - 12, 6, st, run ? pink() : C_CARD, C_INK);
    if (cpu == 999u) put(c, "--"); else put(put_num(c, (int32_t)cpu), "%");   /* the CPU, between the tags */
    cv_rrect(112, 0, 46, 32, 6, C_CARD);
    text_mid(135, 0, &FONT_S, "CPU", C_SUB, C_CARD);
    text_mid(135, 16, &FONT_S, c, cpu != 999u && cpu >= 80u ? pink() : C_INK, C_CARD);
    cv_blit(0, 0);
}
static void draw_seq_tabs(uint32_t tab)         /* DAIRY DRUMS | PADDOCK (SELECT): the page shown in ink */
{
    uint32_t i;
    cv_begin(240, 24, C_BG);
    for (i = 0; i < NSQ; i++) {
        int32_t x = 4 + (int32_t)i * 118;
        uint32_t me = i == tab;
        cv_rrect(x, 0, 114, 22, 7, me ? C_INK : C_CARD);
        text_mid(x + 57, 3, &FONT_S, SQ_NAME[i], me ? C_CARD : C_SUB, me ? C_INK : C_CARD);
    }
    cv_blit(0, TABS_Y);
}
static void draw_seq_rows(uint32_t d0, uint32_t d1, uint32_t sel, uint32_t run, uint32_t pos)
{
    uint32_t d, s;
    cv_begin(240, (d1 - d0) * ROW_H, C_BG);
    for (d = d0; d < d1; d++) {
        int32_t y = (int32_t)(d - d0) * ROW_H;
        if (d == sel) { cv_rrect(0, y, 240, ROW_H - 2, 6, C_INK); cv_rrect(2, y + 2, 236, ROW_H - 6, 5, C_CARD); }
        else cv_rrect(0, y, 240, ROW_H - 2, 6, C_CARD);
        cv_text_bg(7, y + 2, &FONT_S, kit_samples(set.kit) ? kit_samples(set.kit)->row[d] : DR_NAME[d], d == sel ? C_INK : C_SUB, C_CARD);   /* (a sampled kit: its rows' own names) */
        for (s = 0; s < NSTEP; s++) {
            uint32_t on = set.dpat[d] >> s & 1u, head = run && s == pos;
            int32_t x = 50 + (int32_t)s * 12 - 1 + (int32_t)(s / 4u);
            uint16_t c = on ? (d == sel ? C_INK : C_SKY) : (s % 4u == 0u ? C_DIMC4 : C_DIMC);
            if (head) c = on ? pink() : prgb(1, 70);
            cv_rrect(x, y + 4, 10, ROW_H - 10, 3, c);
        }
    }
    cv_blit(0, GRID_Y + d0 * ROW_H);
}
/* PADDOCK: four bar cards (the note each holds, as it sounds), the bar being set outlined, the bar playing in pink
 * with its 16 steps going by; then how to work it */
static const char *chord_name(const uint8_t *nt)   /* a bar's chord by its shape (low to high): MAJ, MIN, 7, OCT ... */
{
    static const struct { uint16_t iv; const char *name; } SHAPE[] = {   /* iv: bit i = a note i semitones up (mod 12) */
        {1u << 4 | 1u << 7, "MAJ"}, {1u << 3 | 1u << 7, "MIN"}, {1u << 3 | 1u << 6, "DIM"}, {1u << 4 | 1u << 8, "AUG"},
        {1u << 5 | 1u << 7, "SUS4"}, {1u << 2 | 1u << 7, "SUS2"}, {1u << 4 | 1u << 7 | 1u << 10, "7"},
        {1u << 4 | 1u << 7 | 1u << 11, "MAJ7"}, {1u << 3 | 1u << 7 | 1u << 10, "MIN7"}, {1u << 7, "5TH"}};
    uint32_t i, iv = 0, n = 0;
    for (i = 0; i < 4u; i++) if (nt[i]) { n++; if (i) iv |= 1u << ((nt[i] - nt[0]) % 12u); }
    if (n <= 1u) return "";
    iv &= ~1u;                                  /* (octaves of the root) */
    if (!iv) return "OCT";
    for (i = 0; i < sizeof SHAPE / sizeof SHAPE[0]; i++) if (SHAPE[i].iv == iv) return SHAPE[i].name;
    return n == 2u ? "2 NOTES" : n == 3u ? "3 NOTES" : "4 NOTES";
}
static void draw_pad(uint32_t cur, uint32_t run, uint32_t bar, uint32_t pos)
{
    uint32_t i, s;
    char b[32], *q;
    cv_begin(240, 128, C_BG);
    for (i = 0; i < 4u; i++) {
        int32_t x = 4 + (int32_t)i * 59;
        uint32_t used = i < set.pad.bars, playing = run && used && i == bar, raw = set.pad.note[i][0];
        uint16_t card = used ? C_CARD : C_DIMC;
        if (i == cur) { cv_rrect(x, 0, 55, 72, 8, C_INK); cv_rrect(x + 2, 2, 51, 68, 7, card); }
        else cv_rrect(x, 0, 55, 72, 8, card);
        cv_rrect(x + 4, 4, 47, 18, 5, used ? C_DIMC : C_DIMC4);
        if (playing) cv_rrect(x + 4, 4, 47 * (int32_t)(pos + 1u) / (int32_t)NSTEP, 18, 5, pink());   /* (its 16 steps going by) */
        q = put(b, "BAR "); *q++ = (char)('1' + i); *q = 0;
        text_mid(x + 27, 5, &FONT_S, b, used ? C_INK : C_SUB, used ? C_DIMC : C_DIMC4);
        if (raw) put_note(b, pad_note_of(raw)); else put(b, "--");   /* the chord's lowest note, as it sounds; its shape */
        text_mid(x + 27, 22, &FONT_L, b, used ? C_INK : C_SUB, card);
        if (raw) text_mid(x + 27, 52, &FONT_S, chord_name(set.pad.note[i]), used ? pink() : C_SUB, card);
        (void)s;
    }
    text_mid(120, 78, &FONT_S, "KEYS: A NOTE OR A CHORD", C_INK, C_BG);
    q = put(b, "ALGO: SHIFT  PRESETS: OCT "); if (set.pad.oct > 0) *q++ = '+'; put_num(q, set.pad.oct);
    text_mid(120, 94, &FONT_S, b, C_SUB, C_BG);
    text_mid(120, 110, &FONT_S, "OCT-/OCT+: BAR  REC: REST", C_SUB, C_BG);
    cv_blit(0, GRID_Y);
}
static void draw_pad_foot(void)
{
    char b[32], *q;
    cv_begin(240, 240 - 196, C_BG);
    cv_rrect(0, 0, 240, 44, 8, C_CARD);
    q = put(b, "1 TEMPO "); put_num(q, set.bpm);
    cv_text_bg(8, 4, &FONT_S, b, C_INK, C_CARD);
    q = put(b, "2 "); put(q, INST_NAME[set.pad.inst % NINST]);
    cv_text_bg(128, 4, &FONT_S, b, C_INK, C_CARD);
    q = put(b, "3 BARS "); put_num(q, set.pad.bars);
    cv_text_bg(8, 22, &FONT_S, b, C_INK, C_CARD);
    q = put(b, "4 VOLUME "); put_num(q, set.pad.vol);
    cv_text_bg(128, 22, &FONT_S, b, C_INK, C_CARD);
    cv_blit(0, 196);
}
static void draw_seq_foot(uint32_t sel)
{
    char b[32], *q;
    cv_begin(240, 240 - 196, C_BG);
    cv_rrect(0, 0, 240, 44, 8, C_CARD);
    q = put(b, "1 TEMPO "); put_num(q, set.bpm);
    cv_text_bg(8, 4, &FONT_S, b, C_INK, C_CARD);
    q = put(b, "2 "); put(q, KIT_NAME[set.kit % NKIT]);
    cv_text_bg(128, 4, &FONT_S, b, C_INK, C_CARD);
    q = put(b, "3 SWING "); put_num(q, set.swing);
    cv_text_bg(8, 22, &FONT_S, b, C_INK, C_CARD);
    q = put(b, "4 VOLUME "); put_num(q, set.dvol);
    cv_text_bg(128, 22, &FONT_S, b, C_INK, C_CARD);
    (void)sel;
    cv_blit(0, 196);
}

/* ---- the loading page: under the name, a glass. Chocolate milk and strawberry milk pour in side by side, drops
 * falling, and rise, waving, until the glass is half full; then they settle. ms: time since switching on (a frame
 * every 60 ms or so) */
#define GL_CX 120                               /* the glass: a tumbler, wider at the top */
#define GL_Y0 98
#define GL_Y1 228
#define GL_TOP 62                               /* half widths, outside the wall */
#define GL_BOT 46
#define GL_WALL 4
static void text_big(int32_t x, int32_t y, const char *s, uint16_t c, uint16_t bg)   /* FONT_L at 1.5 x, soft edges */
{
    uint16_t ramp[16];
    int32_t r = c >> 11, g = (c >> 5) & 63, b = c & 31, r0 = bg >> 11, g0 = (bg >> 5) & 63, b0 = bg & 31;
    uint32_t a;
    const felucca_font_t *f = &FONT_L;
    for (a = 0; a < 16u; a++)
        ramp[a] = (uint16_t)(((uint32_t)(r0 + (r - r0) * (int32_t)a / 15) << 11) | ((uint32_t)(g0 + (g - g0) * (int32_t)a / 15) << 5)
                             | (uint32_t)(b0 + (b - b0) * (int32_t)a / 15));
    for (; *s; s++) {
        uint32_t gi = glyph(f, (uint8_t)*s), w = f->bw[gi], bpr = (w + 1u) / 2u, ox, oy;
        const uint8_t *gd = f->data + f->off[gi];
        for (oy = 0; oy < f->h * 3u / 2u; oy++)
            for (ox = 0; ox < w * 3u / 2u; ox++) {
                uint32_t gx = ox * 2u / 3u, gy = oy * 2u / 3u, v = gd[gy * bpr + gx / 2u];
                v = (gx & 1u) ? (v & 15u) : (v >> 4);
                if (v) cv_pset(x - (int32_t)f->pad * 3 / 2 + (int32_t)ox, y + (int32_t)oy, ramp[v]);
            }
        x += f->adv[gi] * 3 / 2;
    }
}
static int32_t glass_hw(int32_t y) { return GL_TOP - (GL_TOP - GL_BOT) * (y - GL_Y0) / (GL_Y1 - GL_Y0); }
static void draw_splash_part(int32_t oy, uint32_t ms)   /* the screen rows oy .. oy + 119 */
{
    uint32_t t = ms > 1300u ? 1300u : ms, ease = t * (2600u - t) / 1690u;   /* 0..1000, fast then slowing (ease out) */
    int32_t inner_bot = GL_Y1 - 7, half = GL_Y0 + (inner_bot - GL_Y0) / 2;   /* the milk: from the bottom to half way */
    int32_t lv = inner_bot - (int32_t)((uint32_t)(inner_bot - half) * ease / 1000u), x, y, ws, wm, x0;
    cv_begin(240, 120, C_BG);
    cv_oy = -oy;
    ws = text_w(&FONT_L, "SPLIT") * 3 / 2; wm = text_w(&FONT_L, "MILK") * 3 / 2; x0 = 120 - (ws + wm + 2) / 2;
    for (x = 0; x < 3; x++) {                   /* the name, half as big again, in its two flavours (bold: 3 px wide) */
        text_big(x0 + x, 30, "SPLIT", prgb(0, 0), C_BG);
        text_big(x0 + ws + x, 30, "MILK", pink(), C_BG);
    }
    for (y = GL_Y0; y < GL_Y1; y++) {           /* the glass: its inside, a pale blue-white */
        int32_t hw = glass_hw(y) - GL_WALL;
        cv_rect(GL_CX - hw, y, 2 * hw, 1, RGB(238, 245, 252));
    }
    for (x = GL_CX - GL_TOP + GL_WALL; x < GL_CX + GL_TOP - GL_WALL; x++) {   /* the two milks, each its own wave */
        uint32_t p = x >= GL_CX, ph = (uint32_t)x * 0x04000000u + (p ? ms : 0u - ms) * 0x00C00000u;
        int32_t s = lv + ((sine(ph) * (ms < 1300u ? 3 : 1)) >> 15);
        for (y = s; y < inner_bot + 1; y++) {
            int32_t hw = glass_hw(y) - GL_WALL;
            if (x < GL_CX - hw || x >= GL_CX + hw) continue;   /* (inside the glass only) */
            cv_rect(x, y, 1, 1, y < s + 3 ? prgb(p, 55) : prgb(p, 0));   /* the froth on top, then the milk */
        }
    }
    if (ms < 1300u) {                           /* drops falling in while it fills, one of each */
        int32_t k;
        for (k = 0; k < 2; k++) {
            int32_t fall = (int32_t)((ms + (uint32_t)k * 170u) % 340u), dy = GL_Y0 - 14 + fall * (lv - GL_Y0 + 10) / 340;
            drop(k ? GL_CX + 18 : GL_CX - 18, dy, 4, prgb((uint32_t)k, 0));
        }
    }
    for (y = GL_Y0; y < GL_Y1; y++) {           /* the walls, in ink, and the shine down the left */
        int32_t hw = glass_hw(y);
        cv_rect(GL_CX - hw, y, GL_WALL, 1, C_INK);
        cv_rect(GL_CX + hw - GL_WALL, y, GL_WALL, 1, C_INK);
        if (y > GL_Y0 + 8 && y < GL_Y1 - 22) cv_rect(GL_CX - hw + GL_WALL + 5, y, 3, 1, y < lv ? C_CARD : prgb(0, 40));
    }
    for (y = GL_Y1 - 7; y < GL_Y1; y++) {       /* the thick foot */
        int32_t hw = glass_hw(y);
        cv_rect(GL_CX - hw, y, 2 * hw, 1, C_INK);
    }
    cv_rect(GL_CX - GL_TOP - 2, GL_Y0, 2 * GL_TOP + 4, 3, C_INK);   /* the rim */
    if (lv < inner_bot - 44) {                  /* the names, in the milk (up where the glass is wider) */
        text_mid(GL_CX - 24, inner_bot - 36, &FONT_S, "CHOC", C_BG, prgb(0, 0));
        text_mid(GL_CX + 24, inner_bot - 36, &FONT_S, "BERRY", C_INK, pink());
    }
    cv_oy = 0;
    cv_blit(0, (uint32_t)oy);
}
static void draw_splash(uint32_t ms) { draw_splash_part(0, ms); draw_splash_part(120, ms); }

/* ---- knobs */
static int clampi(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : v > hi ? hi : v; }
static uint32_t wrap(int32_t v, uint32_t n) { int32_t m = v % (int32_t)n; return (uint32_t)(m < 0 ? m + (int32_t)n : m); }
static int knob_steps(int32_t e, int32_t *acc, int32_t clicks)   /* whole steps from clicks (a few clicks a step) */
{
    int32_t s;
    *acc += e;
    s = *acc / clicks;
    *acc -= s * clicks;
    return s;
}
#define SWITCH_CLICKS 3                         /* KNOB1 on a pedal: clicks to switch it on (clockwise) / off */

/* ---- the save slots: a slot is read into a spare copy and checked whole before it replaces the current work */
typedef char setup_fits_in_a_slot[sizeof set <= SLOT_MAX && NPROJ <= NSLOT ? 1 : -1];
static uint8_t proj_buf[sizeof set], proj_keep[sizeof set];
static void bytes_copy(void *to, const void *from, uint32_t n)
{
    uint8_t *d = (uint8_t *)to;
    const uint8_t *s = (const uint8_t *)from;
    while (n--) *d++ = *s++;
}
static void proj_scan(void)                     /* what each slot holds (at boot, a few ms) */
{
    uint32_t i;
    for (i = 0; i < NPROJ; i++) {
        proj_info[i].full = (slot_load(i, proj_buf, sizeof set) == 0 && proj_buf[0] == SET_VER)
                            || (slot_load(i, proj_buf, SET_V6_SIZE) == 0 && (proj_buf[0] == 5u || proj_buf[0] == 6u))
                            || (slot_load(i, proj_buf, SET_V4_SIZE) == 0 && proj_buf[0] == 4u);   /* (older slots) */
        proj_info[i].bpm = proj_buf[1];
        proj_info[i].inst[0] = proj_buf[5];     /* (set.part[0].inst, set.part[1].inst) */
        proj_info[i].inst[1] = proj_buf[9];
    }
}
static int proj_save(uint32_t i)                /* 0 = saved */
{
    if (slot_save(i, &set, sizeof set)) return -1;
    proj_info[i].full = 1; proj_info[i].bpm = set.bpm;
    proj_info[i].inst[0] = set.part[0].inst; proj_info[i].inst[1] = set.part[1].inst;
    return 0;
}
static int proj_load(uint32_t i)                /* 0 = loaded; -1 = empty or broken (the current work is untouched) */
{
    bytes_copy(proj_keep, &set, sizeof set);
    if (set_load(ld_slot, i)) { bytes_copy(&set, proj_keep, sizeof set); return -1; }
    all_reset();
    clock_set_bpm(set.bpm);
    return 0;
}
typedef char set_layout_ok[__builtin_offsetof(__typeof__(set), part[1].inst) == 9 && __builtin_offsetof(__typeof__(set), part[0].inst) == 5 ? 1 : -1];   /* (proj_scan reads them there) */

static void app_main(void)
{
    uint32_t prev = 0, k, p, dirty = 0, t_change = 0, page = PG_HOME, sel_drum = DR_KICK, rec_t0 = 0, rec_down = 0;
    uint32_t drawn[6] = {~0u, ~0u, ~0u, ~0u, ~0u, ~0u}, page_drawn = ~0u;   /* what each screen part shows (a hash) */
    uint32_t note_t = 0, note_what = 0, side = 0;   /* the bar says SPLIT (1) or KEY (2) for 1.5 s; side: OCT- 0, OCT+ 1 */
    uint32_t fx_t0[NFX] = {0}, fx_armed = 0;    /* when each pedal button went down; bit e: still a tap (not held 0.5 s) */
    uint32_t cpu_t = 0, cpu_pct = 999;           /* the CPU readout (SEQ page) */
    uint32_t seq_tab = SQ_DRUMS, pad_cursor = 0, pv_t = 0, pad_chord_n = 0;   /* the SEQ page; PADDOCK: the bar being set,
                                                   the notes gathered for its chord so far */
    uint8_t pv[4] = {0, 0, 0, 0};               /* PADDOCK: a chord heard (ALGORITHM, stopped) */
    uint8_t pad_held[NKEY] = {0};               /* PADDOCK: the note each held key sounds (0: none) */
    uint32_t tab = ED_OSC, proj_slot = 0, confirm = 0, confirm_t = 0, proj_go = 0, proj_go_t = 0;
    int32_t acc[4] = {0, 0, 0, 0}, sw_acc = 0;  /* the knobs' clicks gathered towards the next step */
    const char *msg = 0;                        /* a message in the footer for 1.5 s from msg_t (RESET, SAVED 2 ...) */
    uint32_t msg_t = 0, msg_hot = 0;
    static char msg_buf[24];
    uint8_t slot[NKEY];                         /* a white key: its step 0..15; a black key: 0..10 */
    sine_init();
    settings_from_flash();
    clock_set_bpm(set.bpm);
    proj_scan();
    {
        uint32_t w = 0, bl = 0;
        for (k = 0; k < NKEY; k++) slot[k] = (uint8_t)(black(KEY0 + k) ? bl++ : w++);
    }
    {                                           /* the loading page: ~2.2 s, or until a key or a button */
        uint32_t t0 = fm1_ms, last = ~0u;
        while (fm1_ms - t0 < 2200u && !fm1_in.buttons && !fm1_in.notes) {
            uint32_t ms = fm1_ms - t0;
            if (last == ~0u || ms - last >= 60u) { last = ms; draw_splash(ms); }   /* a frame every 60 ms (or slower) */
            platform_poll();
        }
    }
    lcd_fill(0, 0, 240, 240, C_BG);
#define CHANGED() (dirty = 1, t_change = fm1_ms)
#define SAY(s, hot) (msg = (s), msg_hot = (hot), msg_t = fm1_ms ? fm1_ms : 1u)
    for (;;) {
        uint32_t keys, down, up, b, rel, held, run, pos, st;
        int32_t e[4];
        platform_poll();

        /* -- the keys */
        keys = fm1_in.notes & ((1u << NKEY) - 1u);
        down = keys & ~prev;
        up = prev & ~keys;
        prev = keys;
        for (k = 0; k < NKEY; k++) {
            uint32_t kp = key_part[k];          /* (the side its note went to, wherever the split is now) */
            if (up >> k & 1u && pad_held[k]) { live_note(P_PAD, pad_held[k], 0); pad_held[k] = 0; }
            if (up >> k & 1u && key_root[k]) {
                part_play(kp, key_root[k], 0);
                midi_send(0x80u | kp, key_root[k], 0);
                key_root[k] = 0;
            }
            if (!(down >> k & 1u)) continue;
            if (page == PG_DRUMS && seq_tab == SQ_PAD) {   /* PADDOCK: the keys held together are the bar's chord (heard) */
                uint8_t *nt = set.pad.note[pad_cursor];
                uint32_t j, n = KEY0 + k;
                if (!pad_chord_n) for (j = 0; j < 4u; j++) nt[j] = 0;   /* (a new chord replaces what was there) */
                if (pad_chord_n < 4u) {         /* in, low to high */
                    for (j = pad_chord_n; j > 0u && nt[j - 1u] > n; j--) nt[j] = nt[j - 1u];
                    nt[j] = (uint8_t)n;
                    pad_chord_n++;
                }
                pad_held[k] = (uint8_t)pad_note_of(n);
                live_note(P_PAD, pad_held[k], 1);
                CHANGED();
                continue;
            }
            if (page == PG_DRUMS) {             /* the drum machine: white keys = steps, black keys = drums */
                if (black(KEY0 + k)) {
                    if (slot[k] < NDR) { sel_drum = slot[k]; hit_req[sel_drum]++; }
                } else {
                    set.dpat[sel_drum] ^= (uint16_t)(1u << slot[k]);
                    if (!clk_run && set.dpat[sel_drum] >> slot[k] & 1u) hit_req[sel_drum]++;
                    CHANGED();
                }
                continue;
            }
            kp = KEY_PART(k);
            key_part[k] = (uint8_t)kp;
            key_root[k] = (uint8_t)key_note(k);
            part_play(kp, key_root[k], 1);
            midi_send(0x90u | kp, key_root[k], 100);   /* (out: LEFT on channel 1, RIGHT on 2) */
        }

        if (pad_chord_n) {                      /* PADDOCK: every key of the chord let go: on to the next bar */
            uint32_t any = 0;
            for (k = 0; k < NKEY; k++) any |= pad_held[k];
            if (!any) { pad_chord_n = 0; pad_cursor = (pad_cursor + 1u) % set.pad.bars; }
        }

        /* -- the buttons */
        b = fm1_input_edges(&rel);
        held = fm1_in.buttons;
        if (page == PG_DRUMS && seq_tab == SQ_PAD) {   /* PADDOCK: OCT- / OCT+ move to the bar before / after */
            if (b & (1u << BTN_OCT_DOWN) && pad_cursor > 0u) pad_cursor--;
            if (b & (1u << BTN_OCT_UP) && pad_cursor + 1u < set.pad.bars) pad_cursor++;
        } else {
            if (b & (1u << BTN_OCT_DOWN)) side = 0;  /* OCT- / OCT+: the instrument the knobs and the pages set */
            if (b & (1u << BTN_OCT_UP)) side = 1;
        }
        if (b & (1u << BTN_SAVE)) { page = page == PG_SAVE ? PG_HOME : PG_SAVE; confirm = 0; }
        if (page == PG_SAVE) {                  /* the save page: REC saves, PLAY loads, each asks first */
            uint32_t mask = 1u << BTN_PLAY | 1u << BTN_REC, op = b >> BTN_REC & 1u ? 1u : b >> BTN_PLAY & 1u ? 2u : 0u;
            if (b & ~mask & ~(1u << BTN_SAVE)) confirm = 0;   /* another button: cancelled */
            if (op && !proj_go) {
                if (op == 2u && !proj_info[proj_slot].full) { SAY("EMPTY", 1); confirm = 0; }
                else if (confirm != op || fm1_ms - confirm_t > 3000u) { confirm = op; confirm_t = fm1_ms; }
                else {                          /* sure: stop, let it ring out, then do it (below) */
                    confirm = 0; proj_go = op; proj_go_t = fm1_ms; clk_run = 0;
                    SAY(op == 1u ? "SAVING" : "LOADING", 0);
                }
            }
            b &= ~mask; rel &= ~mask; rec_down = 0;   /* (PLAY and REC do only this here) */
        }
        if (proj_go && (quiet() || fm1_ms - proj_go_t > 1500u)) {   /* (writing the flash stalls the sound: in silence) */
            int rc = proj_go == 2u ? proj_load(proj_slot) : proj_save(proj_slot);
            char *q = put(msg_buf, rc ? (proj_go == 2u ? "LOAD FAILED" : "SAVE FAILED") : (proj_go == 2u ? "LOADED " : "SAVED "));
            if (!rc) { *q++ = (char)('1' + proj_slot); *q = 0; }
            SAY(msg_buf, 1);
            if (!rc && proj_go == 2u) CHANGED();
            proj_go = 0;
        }
        if (b & (1u << BTN_SEQ)) page = page == PG_DRUMS ? PG_HOME : PG_DRUMS;
        if (b & (1u << BTN_HOME)) page = PG_HOME;
        if (b & (1u << BTN_ARP)) page = page == PG_ARP ? PG_HOME : PG_ARP;
        if (b & (1u << BTN_GLO)) page = page == PG_EDIT ? PG_HOME : PG_EDIT;
        for (k = 0; k < NFX; k++) {             /* the pedals: a tap switches it on / off, a hold (0.5 s) opens its page */
            uint32_t bit = 1u << FX_BTN[k];
            if (b & bit) { fx_t0[k] = fm1_ms; fx_armed |= 1u << k; }
            if (fx_armed >> k & 1u && held & bit && fm1_ms - fx_t0[k] >= 500u) {
                fx_armed &= ~(1u << k);
                page = PG_FX + k;
            }
            if (rel & bit) {
                if (fx_armed >> k & 1u) {
                    set.fx[side].on ^= (uint8_t)(1u << k);
                    {
                        char *q = put(msg_buf, FX_NAME[k]);
                        put(q, set.fx[side].on >> k & 1u ? " ON" : " OFF");
                        SAY(msg_buf, 0);
                        note_t = fm1_ms; note_what = 3u + k;   /* (the home bar says it too) */
                    }
                    CHANGED();
                }
                fx_armed &= ~(1u << k);
            }
        }
        if (b & (1u << BTN_PLAY)) {
            if (clk_run) clk_run = 0;
            else { clk_run = 1; clk_go++; }     /* (running first: step 1's drums and PADDOCK's first bar sound) */
        }
        if (b & (1u << BTN_REC)) {
            rec_t0 = fm1_ms; rec_down = 1;
            if (page == PG_HOME) {              /* LATCH: the notes held go on; REC again lets them go */
                if (latch_n) { latch_release(); note_t = fm1_ms; note_what = 22; }
                else {                          /* the held keys' notes move to the latch; the keys are free */
                    for (k = 0; k < NKEY; k++)
                        if (keys >> k & 1u && key_root[k] && latch_n < NKEY) {
                            latched[latch_n].part = key_part[k]; latched[latch_n].root = key_root[k]; latch_n++;
                            key_root[k] = 0;    /* (letting go of the key now ends nothing) */
                        }
                    if (latch_n) { note_t = fm1_ms; note_what = 21; }
                }
            }
        }
        if (rec_down && fm1_ms - rec_t0 >= 1000u) {   /* REC held 1 s: the drum pattern cleared, or the page's option reset */
            const char *what = 0;
            if (page == PG_HOME) {              /* the split screen: everything as new (the save slots stay) */
                clk_run = 0;
                all_reset();
                live_note(P_PAD, 0, 2);
                set_defaults();
                clock_set_bpm(set.bpm);
                side = 0; pad_cursor = 0; sel_drum = DR_KICK;
                note_t = fm1_ms; note_what = 20;
                CHANGED();
            } else if (page == PG_DRUMS && seq_tab == SQ_PAD) { for (k = 0; k < 16u; k++) set.pad.note[k / 4u][k % 4u] = 0; what = "PADDOCK"; }
            else if (page == PG_DRUMS) { for (k = 0; k < NDR; k++) set.dpat[k] = 0; what = "PATTERN"; }
            else if (page >= PG_FX && page < PG_FX + NFX) { pedal_reset(side, page - PG_FX); what = FX_NAME[page - PG_FX]; }
            else if (page == PG_ARP) { set.arp[side] = ARP0; what = "ARP"; }
            else if (page == PG_EDIT) { edit_reset(side, tab); what = ED_NAME[tab]; }
            if (what) {
                char *q = put(msg_buf, what);
                put(q, page == PG_DRUMS ? " CLEARED" : " RESET");
                SAY(msg_buf, 1);
                CHANGED();
            }
            rec_down = 0;
        }
        if (rel & (1u << BTN_REC)) {
            if (rec_down && page == PG_DRUMS && seq_tab == SQ_PAD) {   /* a tap: a rest in the bar, the next bar */
                for (k = 0; k < 4u; k++) set.pad.note[pad_cursor][k] = 0;
                pad_cursor = (pad_cursor + 1u) % set.pad.bars;
                CHANGED();
            } else if (rec_down && page == PG_DRUMS) { set.dpat[sel_drum] = 0; CHANGED(); }   /* a tap: the chosen drum's row */
            rec_down = 0;
        }
        if (confirm && (fm1_ms - confirm_t > 3000u || page != PG_SAVE)) confirm = 0;   /* "sure?" lapses */

        /* -- the knobs, by page */
        e[0] = fm1_enc_take(ENC_KNOB1); e[1] = fm1_enc_take(ENC_KNOB2);
        e[2] = fm1_enc_take(ENC_KNOB3); e[3] = fm1_enc_take(ENC_KNOB4);
        if (page == PG_HOME) {                  /* the chosen instrument: octave, sound, mode, volume */
            part_t *pt = &set.part[side];
            int32_t v;
            if (e[0]) { v = clampi(pt->oct + e[0], -3, 3); if (v != pt->oct) { part_reset(side); pt->oct = (int8_t)v; CHANGED(); } }
            if (e[1]) {                         /* a new sound: its preset (what the wave editor changed goes) */
                part_reset(side);
                pt->inst = (uint8_t)wrap((int32_t)pt->inst + e[1], NINST);
                set.snd[side] = SND[pt->inst];
                CHANGED();
            }
            if (e[2]) { part_reset(side); pt->mode = (uint8_t)wrap((int32_t)pt->mode + e[2], NMODE); CHANGED(); }
            if (e[3]) { v = clampi(pt->vol + 5 * e[3], 0, 100); if (v != pt->vol) { pt->vol = (uint8_t)v; CHANGED(); } }
        } else if (page == PG_DRUMS && seq_tab == SQ_PAD) {   /* PADDOCK: tempo, sound, bars, volume */
            if (e[0]) { set.bpm = (uint8_t)clampi(set.bpm + e[0], BPM_MIN, BPM_MAX); clock_set_bpm(set.bpm); CHANGED(); }
            if (e[1]) {                         /* its sound: the pads only, round and round */
                int32_t s = e[1] > 0 ? 1 : -1, n = e[1] > 0 ? e[1] : -e[1];
                uint32_t i = set.pad.inst % NINST;
                live_note(P_PAD, 0, 2);
                while (n--) do i = wrap((int32_t)i + s, NINST); while (INST_CAT(i) != CAT_PAD);
                set.pad.inst = (uint8_t)i;
                CHANGED();
            }
            if (e[2]) {
                int32_t s = clampi((int32_t)set.pad.bars + knob_steps(e[2], &acc[2], 2), 1, 4);
                if (s != set.pad.bars) { set.pad.bars = (uint8_t)s; if (pad_cursor >= set.pad.bars) pad_cursor = set.pad.bars - 1u; CHANGED(); }
            }
            if (e[3]) { set.pad.vol = (uint8_t)clampi(set.pad.vol + 5 * e[3], 0, 100); CHANGED(); }
        } else if (page == PG_DRUMS) {
            if (e[0]) { set.bpm = (uint8_t)clampi(set.bpm + e[0], BPM_MIN, BPM_MAX); clock_set_bpm(set.bpm); CHANGED(); }
            if (e[1]) { set.kit = (uint8_t)wrap((int32_t)set.kit + e[1], NKIT); CHANGED(); }
            if (e[2]) { set.swing = (uint8_t)clampi(set.swing + 5 * e[2], 0, 100); CHANGED(); }
            if (e[3]) { set.dvol = (uint8_t)clampi(set.dvol + 5 * e[3], 0, 100); CHANGED(); }
        } else if (page >= PG_FX && page < PG_FX + NFX) {   /* a pedal: KNOB1 on / off (3 clicks), KNOB2-4 its knobs */
            uint32_t fe = page - PG_FX;
            pknob_t pk[3];
            pedal_knobs(side, fe, pk);
            if (e[0]) {
                if ((e[0] > 0) != (sw_acc > 0)) sw_acc = 0;
                sw_acc += e[0];
                if (sw_acc >= SWITCH_CLICKS || sw_acc <= -SWITCH_CLICKS) {
                    uint8_t was = set.fx[side].on;
                    if (sw_acc > 0) set.fx[side].on |= (uint8_t)(1u << fe); else set.fx[side].on &= (uint8_t)~(1u << fe);
                    if (was != set.fx[side].on) CHANGED();
                    sw_acc = 0;
                }
            }
            for (k = 0; k < 3u; k++) {
                int32_t s, l;
                if (!e[k + 1u]) continue;
                if (pk[k].max == 127u) {        /* 0..127 in 20 steps, a click each */
                    l = clampi((int32_t)pedal_level(*pk[k].v) + e[k + 1u], 0, PEDAL_STEPS);
                    s = l * 127 / PEDAL_STEPS;
                } else s = clampi((int32_t)*pk[k].v + knob_steps(e[k + 1u], &acc[k + 1u], 2), 0, pk[k].max);
                if (s != *pk[k].v) { *pk[k].v = (uint8_t)s; CHANGED(); }
            }
        } else if (page == PG_ARP) {            /* the arpeggiator: rate, octaves, gate, notes (2 clicks a step) */
            arp_t *a = &set.arp[side];
            uint8_t *v[4] = {&a->rate, &a->oct, &a->gate, &a->notes};
            static const uint8_t MX[4] = {3, 3, 3, 1};
            for (k = 0; k < 4u; k++) {
                int32_t s;
                if (!e[k]) continue;
                s = clampi((int32_t)*v[k] + knob_steps(e[k], &acc[k], 2), 0, MX[k]);
                if (s != *v[k]) { *v[k] = (uint8_t)s; CHANGED(); }
            }
        } else if (page == PG_EDIT) {           /* the wave editor: the page's four settings */
            uint8_t *sb = (uint8_t *)&set.snd[side];
            for (k = 0; k < 4u; k++) {
                const eparam_t *ep = &EPARAM[tab][k];
                int32_t s;
                if (!e[k] || !ep->max) continue;
                s = ep->step > 1u ? (int32_t)sb[ep->off] + e[k] * ep->step
                                  : (int32_t)sb[ep->off] + knob_steps(e[k], &acc[k], ep->max > 10u ? 1 : 2);
                s = clampi(s, 0, ep->max);
                if (s != sb[ep->off]) { sb[ep->off] = (uint8_t)s; CHANGED(); }
            }
        } else if (page == PG_SAVE) {
            if (e[0]) {
                uint32_t sl = (uint32_t)clampi((int32_t)proj_slot + e[0], 0, NPROJ - 1);
                if (sl != proj_slot) { proj_slot = sl; confirm = 0; }
            }
        }
        {
            int32_t es = fm1_enc_take(ENC_SELECT), ep = fm1_enc_take(ENC_PRESETS), ea = fm1_enc_take(ENC_ALGORITHM), v;
            if (es && page == PG_DRUMS) seq_tab = wrap((int32_t)seq_tab + es, NSQ);   /* the SEQ page: DAIRY DRUMS / PADDOCK */
            else if (es && page == PG_EDIT) tab = wrap((int32_t)tab + es, NED);         /* the editor: its page */
            else if (es) {                      /* SELECT: the split, a key at a time (clockwise: LEFT gets more keys) */
                v = clampi((int32_t)set.split + es, 1, 26);
                if (v != (int32_t)set.split) { set.split = (uint8_t)v; CHANGED(); }   /* (what sounds goes on: each key
                                                   lets go on the side it started on; LATCH stays) */
                note_t = fm1_ms; note_what = 1;
            }
            if (ep && page == PG_DRUMS && seq_tab == SQ_PAD) {   /* PADDOCK: its octave */
                v = clampi(set.pad.oct + ep, -3, 3);
                if (v != set.pad.oct) { live_note(P_PAD, 0, 2); set.pad.oct = (int8_t)v; CHANGED(); }
                ep = 0;
            }
            if (ea && page == PG_DRUMS) {       /* the SEQ page: ALGORITHM picks the drum, or sets the bar's note */
                if (seq_tab == SQ_DRUMS) sel_drum = wrap((int32_t)sel_drum + ea, NDR);
                else {                          /* the bar's chord, all of it, a semitone a click (a rest starts at C3) */
                    uint8_t *nt = set.pad.note[pad_cursor];
                    uint32_t j;
                    if (!nt[0]) nt[0] = (uint8_t)PAD_START;
                    else for (j = 0; j < 4u; j++) if (nt[j]) nt[j] = (uint8_t)clampi((int32_t)nt[j] + ea, 24, 108);
                    CHANGED();
                    if (!clk_run) {             /* stopped: hear it */
                        for (j = 0; j < 4u; j++) if (pv[j]) live_note(P_PAD, pv[j], 0);
                        for (j = 0; j < 4u; j++) { pv[j] = nt[j] ? (uint8_t)pad_note_of(nt[j]) : 0u; if (pv[j]) live_note(P_PAD, pv[j], 1); }
                        pv_t = fm1_ms;
                    }
                }
                ea = 0;
            }
            if (ep) {                           /* PRESETS: the key, a semitone at a time, everything with it */
                v = clampi(set.key + ep, -12, 12);
                if (v != set.key) { all_reset(); set.key = (int8_t)v; CHANGED(); }
                note_t = fm1_ms; note_what = 2;
            }
            if (ea) {                           /* ALGORITHM: the tempo */
                set.bpm = (uint8_t)clampi(set.bpm + ea, BPM_MIN, BPM_MAX);
                clock_set_bpm(set.bpm);
                CHANGED();
            }
        }
        midi_in();

        if (fm1_ms - cpu_t >= 500u) {           /* the sound's load, twice a second: the longest block of the last 0.5 s */
            cpu_t = fm1_ms;
#ifndef DINGHY_HOST
            cpu_pct = (cpu_peak * 100u + AUDIO_BUDGET / 2u) / AUDIO_BUDGET;
            if (cpu_pct > 100u) cpu_pct = 100u;
            cpu_reset++;
#endif
        }

        if (pv[0] && fm1_ms - pv_t > 400u) {     /* the chord heard ends */
            for (k = 0; k < 4u; k++) { if (pv[k]) live_note(P_PAD, pv[k], 0); pv[k] = 0; }
        }

        /* -- the LEDs */
        run = clk_run;
        pos = clk_pos;
        for (k = 0; k < NKEY; k++) {
            uint32_t on;
            if (page == PG_DRUMS && seq_tab == SQ_PAD)   /* PADDOCK: the key of the bar being set, and the keys held */
                on = set.pad.note[pad_cursor][0] == KEY0 + k || set.pad.note[pad_cursor][1] == KEY0 + k
                  || set.pad.note[pad_cursor][2] == KEY0 + k || set.pad.note[pad_cursor][3] == KEY0 + k || keys >> k & 1u;
            else if (page == PG_DRUMS) {
                if (black(KEY0 + k)) on = slot[k] == sel_drum;
                else on = (set.dpat[sel_drum] >> slot[k] & 1u) ^ (run && slot[k] == pos);
            } else if (KEY_PART(k)) on = !(keys >> k & 1u);   /* RIGHT: always lit, dark while pressed */
            else on = keys >> k & 1u;                        /* LEFT: lit while pressed */
            fm1_led_key(KEY_LED(k), on);
            fm1_led_key_glow(KEY_LED(k), 0);
        }
        if (page == PG_DRUMS && seq_tab == SQ_PAD) {   /* PADDOCK: lit where the bar can move */
            fm1_led_key(BTN_OCT_DOWN, pad_cursor > 0u);
            fm1_led_key(BTN_OCT_UP, pad_cursor + 1u < set.pad.bars);
        } else {
            fm1_led_key(BTN_OCT_DOWN, side == 0u);   /* the instrument being set: lit */
            fm1_led_key(BTN_OCT_UP, side == 1u);
        }
        for (k = 0; k < NFX; k++) {             /* the pedal buttons: lit = on for this instrument; its page open: a glow */
            uint32_t on = set.fx[side].on >> k & 1u;
            fm1_led_key(FX_BTN[k], on);
            fm1_led_key_glow(FX_BTN[k], !on && page == PG_FX + k);
        }
        fm1_led_key(BTN_ARP, page == PG_ARP);
        fm1_led_key_glow(BTN_ARP, page != PG_ARP && MODE_IS_ARP(part_mode(side)));
        fm1_led_key(BTN_GLO, page == PG_EDIT);
        fm1_led_key(BTN_SAVE, page == PG_SAVE);
        fm1_led_key(BTN_SEQ, page == PG_DRUMS);
        if (page == PG_SAVE && confirm) fm1_led_key(BTN_PLAY, confirm == 2u && (fm1_ms / 150u) % 2u);
        else fm1_led_key(BTN_PLAY, run);
        fm1_led_key(BTN_REC, (rec_down && (fm1_ms / 150u) % 2u) || (page == PG_SAVE && confirm == 1u && (fm1_ms / 150u) % 2u)
                             || (page == PG_HOME && latch_n && !rec_down));   /* (lit: LATCH on) */

        /* -- the screen: each part redrawn when what it shows changed */
        idle_ok = 1;
        if (page != page_drawn) {
            if (page_drawn != ~0u && !proj_go) msg = 0;   /* (a message belongs to the page it was said on) */
            page_drawn = page;
            lcd_fill(0, 0, 240, 240, C_BG);
            for (k = 0; k < 6u; k++) drawn[k] = ~0u;
        }
        {
            uint32_t show = msg && fm1_ms - msg_t < 1500u;
            const char *m1 = show ? msg : 0;
            if (page == PG_HOME) {
                for (p = 0; p < NPART; p++) {
                    const part_t *pt = &set.part[p];
                    uint32_t mask = p ? keys & ~((1u << SPLIT) - 1u) : keys & ((1u << SPLIT) - 1u);
                    st = mix(mix(mix(mix(mix(mix(mix(mix(p, pt->inst), pt->mode), pt->vol), (uint32_t)(pt->oct + 8)), side == p), mask), set.split), (uint32_t)(set.key + 20));
                    if (st != drawn[p]) { drawn[p] = st; draw_panel(p, side == p, mask); }
                }
                {
                    uint32_t say = note_what && fm1_ms - note_t < 1500u ? note_what : 0u;
                    st = mix(mix(mix(mix(mix(mix(mix(mix(run, run ? pos : 99u), set.bpm), set.kit), 7u), say), set.split), (uint32_t)(set.key + 20)), set.fx[side].on);
                    st = mix(st, latch_n != 0u);
                    for (k = 0; k < NDR; k++) st = mix(st, set.dpat[k]);
                    if (st != drawn[2]) { drawn[2] = st; draw_bar(run, pos, say, side); }
                }
            } else if (page == PG_DRUMS) {
                if ((st = mix(mix(run, 3u), cpu_pct)) != drawn[0]) { drawn[0] = st; draw_seq_head(run, cpu_pct); }
                if ((st = mix(seq_tab, 21u)) != drawn[4]) {   /* another tab: its page drawn afresh */
                    drawn[4] = st;
                    drawn[1] = drawn[2] = drawn[3] = ~0u;
                    lcd_fill(0, GRID_Y, 240, 196 - GRID_Y, C_BG);
                    draw_seq_tabs(seq_tab);
                }
                if (seq_tab == SQ_DRUMS) {
                    for (k = 0; k < 2u; k++) {
                        uint32_t d;
                        st = mix(mix(mix(mix(k, sel_drum), run ? pos : 99u), run), set.kit);
                        for (d = k * 3u; d < k * 3u + 3u; d++) st = mix(st, set.dpat[d]);
                        if (st != drawn[1u + k]) { drawn[1u + k] = st; draw_seq_rows(k * 3u, k * 3u + 3u, sel_drum, run, pos); }
                    }
                    st = mix(mix(mix(mix(set.bpm, set.kit), set.swing), set.dvol), 11u);
                    if (st != drawn[3]) { drawn[3] = st; draw_seq_foot(sel_drum); }
                } else {
                    uint32_t bar = clk_bar;
                    st = mix(mix(mix(mix(mix(mix(pad_cursor, run), run ? bar << 4 | pos : 99u), set.pad.bars), (uint32_t)(set.pad.oct + 8)), set.pad.inst), (uint32_t)(set.key + 20));
                    for (k = 0; k < 16u; k++) st = mix(st, set.pad.note[k / 4u][k % 4u]);
                    if (st != drawn[1]) { drawn[1] = st; draw_pad(pad_cursor, run, bar, pos); }
                    st = mix(mix(mix(mix(set.bpm, set.pad.inst), set.pad.bars), set.pad.vol), 13u);
                    if (st != drawn[3]) { drawn[3] = st; draw_pad_foot(); }
                }
            } else {
                const char *title = page == PG_ARP ? "ARPEGGIATOR" : page == PG_EDIT ? "WAVE EDITOR" : page == PG_SAVE ? "SAVE" : FX_NAME[page - PG_FX];
                const char *l1 = 0, *l2 = "HOLD REC: RESET";
                static char l1b[32];
                if ((st = mix(mix(page, side), 5u)) != drawn[0]) { drawn[0] = st; draw_page_head(title, side); }
                if (page >= PG_FX && page < PG_FX + NFX) {
                    const uint8_t *fb = (const uint8_t *)&set.fx[side], *gb = (const uint8_t *)&set.gfx;
                    st = mix(page, side);
                    for (k = 0; k < sizeof(tfx_t); k++) st = mix(st, fb[k]);
                    for (k = 0; k < sizeof(gfx_t); k++) st = mix(st, gb[k]);
                    if (st != drawn[1]) { drawn[1] = st; draw_pedal(side, page - PG_FX); }
                    put(put(put(put(l1b, FX_FLAVOUR[page - PG_FX]), "  TAP "), FX_BTN_NAME[page - PG_FX]), ": ON/OFF");
                    l1 = l1b;
                } else if (page == PG_ARP) {
                    const uint8_t *ab = (const uint8_t *)&set.arp[side];
                    st = mix(page, side);
                    for (k = 0; k < sizeof(arp_t); k++) st = mix(st, ab[k]);
                    if (st != drawn[1]) { drawn[1] = st; draw_arp(side); }
                    if (MODE_IS_ARP(part_mode(side))) put(put(l1b, MODE_NAME[part_mode(side)]), " (THE MODE)");
                    else put(l1b, "MODE IS NOT AN ARP");
                    l1 = l1b;
                } else if (page == PG_EDIT) {
                    const uint8_t *sb = (const uint8_t *)&set.snd[side];
                    st = mix(mix(page, side), tab);
                    for (k = 0; k < sizeof(snd_t); k++) st = mix(st, sb[k]);
                    if (st != drawn[1]) { drawn[1] = st; draw_edit_top(side, tab); draw_edit_knobs(side, tab); }
                    put(put(l1b, INST_NAME[part_inst(side)]), "  SELECT: PAGE");
                    l1 = l1b;
                } else {                        /* the save page */
                    st = mix(mix(proj_slot, confirm), 9u);
                    for (k = 0; k < NPROJ; k++) st = mix(mix(mix(st, proj_info[k].full), proj_info[k].bpm), proj_info[k].inst[0] | proj_info[k].inst[1] << 8);
                    if (st != drawn[1]) { drawn[1] = st; draw_save(proj_slot, confirm); }
                    l1 = "REC: SAVE   PLAY: LOAD";
                    l2 = "KNOB 1: SLOT   SAVE: BACK";
                }
                st = mix(mix(mix(page, side), show ? msg_t : 0u), 17u);
                if (l1) for (k = 0; l1[k]; k++) st = mix(st, (uint8_t)l1[k]);
                if (st != drawn[2]) { drawn[2] = st; draw_foot(m1 ? m1 : l1, l2, m1 != 0 && msg_hot); }
            }
        }
        idle_ok = 0;

        if (dirty && quiet() && !proj_go && fm1_ms - t_change > 2000u) {   /* 2 s after a change, when silent */
            settings_save(&set, sizeof set);
            dirty = 0;
        }
    }
}
