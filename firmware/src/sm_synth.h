/* SPDX-License-Identifier: GPL-3.0-only */
/* SPLITMILK's two instruments: LEFT (keys F3..E4) and RIGHT (F4..G5), each its own sound, mode, octave and volume.
 * The engine is QUARTET's subtractive synth (gb_synth.h), cut down to two parts of 6 voices each:
 *   2 oscillators (SINE / TRIANGLE / SAW / SQUARE / NOISE / FM), a resonant low-pass filter opened by the envelope,
 *   one ADSR, one LFO per part (to PITCH, FILTER or AMP)
 *   24 instruments (QUARTET's leads, pads and basses, and SPLITMILK's BUBBLE POP, CLOUD, CURDLE), any on either side
 * Modes (per part): SINGLE KEY; CHORDS (major, minor, 7th, or "in key": the chord C major has on that note);
 * ARPEGGIO (up, down, up-down, random): the in-key triads of the keys held, over 2 octaves, a 16th note each,
 * on the drum machine's tempo (sm_drums.h's clock).
 * Included by splitmilk.c. The render runs in the audio interrupt; live notes come from the main loop through
 * live_q (main writes, the interrupt reads). Silent voices cost nothing. */

enum { W_SIN, W_TRI, W_SAW, W_SQR, W_NOISE, W_FM, NWAVE };   /* FM (osc 1): a sine whose pitch osc 2 bends */
enum { L_PITCH, L_FILT, L_AMP, NLTGT };
enum { LW_TRI, LW_SQR, LW_SAW, LW_RND, NLWAVE };

/* (snd_t, a sound's settings, is declared in splitmilk.c: each part keeps its own copy, set.snd[], to edit on GLO) */

/* the filter's frequency coefficient for cutoff 0..127 = 40 Hz .. 8 kHz (gb_synth.h: run once per sample it is
 * stable up to index 112, ~4.3 kHz, which is where CMAX stops it) */
static const uint16_t FTAB[128] = {
    93, 97, 101, 106, 110, 115, 120, 125, 130, 136, 142, 148, 154, 161, 167, 175, 182, 190, 198, 206, 215, 224,
    234, 244, 254, 265, 276, 288, 300, 313, 326, 340, 355, 370, 386, 402, 419, 437, 456, 475, 495, 516, 539, 561,
    585, 610, 636, 663, 692, 721, 752, 784, 817, 852, 888, 926, 966, 1007, 1050, 1094, 1141, 1190, 1240, 1293,
    1348, 1406, 1466, 1528, 1593, 1661, 1732, 1805, 1882, 1962, 2046, 2133, 2224, 2319, 2417, 2520, 2628, 2740,
    2856, 2978, 3105, 3237, 3374, 3518, 3668, 3824, 3987, 4156, 4333, 4517, 4709, 4910, 5118, 5336, 5563, 5799,
    6046, 6302, 6570, 6849, 7139, 7442, 7758, 8087, 8429, 8786, 9158, 9546, 9949, 10370, 10807, 11263, 11738,
    12232, 12747, 13283, 13840, 14420, 15024, 15652, 16305, 16984, 17689, 18423};
#define SVF_LIM 131071
#define CMAX 112
/* envelope speed 0..127: time constant 0.5 ms .. 4 s, per-16-sample step, Q15 */
static const uint16_t ENV_K[128] = {
    16908, 16101, 15314, 14546, 13801, 13079, 12382, 11710, 11064, 10444, 9851, 9285,
    8744, 8229, 7739, 7274, 6833, 6416, 6020, 5647, 5294, 4961, 4648, 4352,
    4074, 3813, 3567, 3336, 3120, 2916, 2726, 2547, 2379, 2222, 2075, 1938,
    1809, 1689, 1576, 1471, 1373, 1281, 1195, 1115, 1040, 970, 904, 843,
    787, 733, 684, 638, 594, 554, 517, 482, 449, 418, 390, 363,
    339, 316, 294, 274, 256, 238, 222, 207, 193, 180, 167, 156,
    145, 135, 126, 118, 110, 102, 95, 89, 83, 77, 72, 67,
    62, 58, 54, 50, 47, 44, 41, 38, 35, 33, 31, 29,
    27, 25, 23, 22, 20, 19, 17, 16, 15, 14, 13, 12,
    11, 11, 10, 9, 9, 8, 7, 7, 6, 6, 6, 5,
    5, 5, 4, 4, 4, 3, 3, 3};
/* LFO rate 0..127: 0.05 .. 20 Hz, phase step per sample */
static const uint32_t LFO_INC[128] = {
    4870, 5105, 5351, 5610, 5881, 6165, 6463, 6775,
    7102, 7445, 7805, 8182, 8577, 8992, 9426, 9881,
    10359, 10859, 11384, 11934, 12510, 13115, 13748, 14412,
    15108, 15838, 16603, 17405, 18246, 19128, 20052, 21020,
    22036, 23100, 24216, 25386, 26612, 27898, 29246, 30658,
    32139, 33692, 35320, 37026, 38814, 40689, 42655, 44716,
    46876, 49140, 51514, 54002, 56611, 59346, 62213, 65218,
    68369, 71671, 75134, 78763, 82568, 86556, 90738, 95121,
    99716, 104533, 109583, 114877, 120426, 126243, 132342, 138735,
    145437, 152463, 159828, 167549, 175642, 184127, 193022, 202346,
    212121, 222368, 233110, 244371, 256176, 268551, 281524, 295124,
    309381, 324326, 339994, 356418, 373635, 391685, 410606, 430441,
    451235, 473033, 495884, 519839, 544951, 571276, 598873, 627803,
    658131, 689923, 723252, 758190, 794816, 833212, 873462, 915657,
    959890, 1006260, 1054869, 1105827, 1159247, 1215247, 1273953, 1335494,
    1400008, 1467639, 1538537, 1612860, 1690773, 1772450, 1858072, 1947831};
/* osc 2 tuning, 2^(s / 12) for s = -24..+24 semitones (index s + 24), Q16 */
static const uint32_t SEMI[49] = {
    16384, 17358, 18390, 19484, 20643, 21870, 23170, 24548,
    26008, 27554, 29193, 30929, 32768, 34716, 36781, 38968,
    41285, 43740, 46341, 49097, 52016, 55109, 58386, 61858,
    65536, 69433, 73562, 77936, 82570, 87480, 92682, 98193,
    104032, 110218, 116772, 123715, 131072, 138866, 147123, 155872,
    165140, 174960, 185364, 196386, 208064, 220436, 233544, 247431,
    262144};

/* ---- the 24 instruments (from QUARTET's sounds): {cut, res, fenv, wave1, wave2, tune, mix, A, D, S, R, lfo rate,
 * depth, target, wave}. 10 leads, 7 pads / keys, 7 basses (INST_CATS) */
#define NINST 24u
enum { CAT_LEAD, CAT_PAD, CAT_BASS, NCAT };
static const snd_t SND[NINST] = {
    {95, 0, 45, W_TRI, W_SAW, 36, 20, 0, 96, 0, 80, 95, 0, L_PITCH, LW_TRI},   /* PIANO */
    {127, 0, 0, W_SIN, W_SIN, 43, 40, 0, 72, 0, 62, 95, 0, L_PITCH, LW_TRI},   /* XYLOPHONE */
    {45, 20, 85, W_SAW, W_SAW, 24, 50, 55, 85, 100, 60, 95, 8, L_PITCH, LW_TRI},   /* BRASS */
    {112, 0, 0, W_SIN, W_SIN, 48, 30, 0, 92, 0, 90, 95, 0, L_PITCH, LW_TRI},   /* MUSIC BOX */
    {70, 35, 55, W_SAW, W_SQR, 24, 45, 0, 92, 60, 62, 95, 0, L_PITCH, LW_TRI},   /* ELEC GUITAR */
    {80, 10, 25, W_SQR, W_SIN, 43, 45, 0, 60, 127, 30, 105, 10, L_PITCH, LW_TRI},   /* HAMMOND */
    {112, 0, 0, W_SIN, W_SIN, 24, 0, 70, 80, 127, 80, 90, 30, L_PITCH, LW_TRI},   /* THEREMIN */
    {85, 25, 30, W_SAW, W_SAW, 24, 50, 5, 70, 110, 55, 95, 0, L_PITCH, LW_TRI},   /* SYNTH */
    {112, 0, 0, W_SQR, W_SQR, 36, 15, 0, 60, 127, 20, 95, 0, L_PITCH, LW_TRI},   /* CHIPTUNE */
    {40, 100, 90, W_SIN, W_SIN, 36, 20, 0, 70, 0, 60, 110, 0, L_PITCH, LW_TRI},   /* BUBBLE POP: a sine through a resonant filter that snaps shut: a round plop */
    {80, 0, 0, W_SAW, W_SAW, 36, 40, 85, 100, 120, 90, 92, 12, L_PITCH, LW_TRI},   /* STRINGS */
    {72, 80, 0, W_SAW, W_NOISE, 24, 35, 88, 100, 120, 92, 92, 14, L_PITCH, LW_TRI},   /* CHOIR */
    {100, 0, 0, W_TRI, W_SIN, 36, 55, 2, 60, 127, 40, 96, 18, L_AMP, LW_TRI},   /* ORGAN */
    {100, 0, 50, W_FM, W_SIN, 36, 30, 0, 100, 40, 85, 85, 20, L_AMP, LW_TRI},   /* ELEC PIANO */
    {90, 0, 40, W_FM, W_SIN, 43, 15, 2, 100, 0, 100, 60, 0, L_PITCH, LW_TRI},   /* HANDPAN */
    {100, 10, 20, W_FM, W_SIN, 48, 20, 85, 100, 110, 95, 70, 10, L_AMP, LW_TRI},   /* GLASS PAD */
    {70, 5, 5, W_FM, W_SIN, 24, 10, 96, 100, 115, 104, 38, 10, L_AMP, LW_TRI},   /* CLOUD: a soft FM bell-pad, slow in and out: calming */
    {32, 20, 60, W_SAW, W_SQR, 24, 30, 50, 85, 100, 50, 85, 6, L_PITCH, LW_TRI},   /* TUBA */
    {55, 60, 100, W_SAW, W_SQR, 36, 35, 0, 70, 50, 45, 60, 0, L_FILT, LW_TRI},   /* SLAP */
    {25, 40, 110, W_SAW, W_SQR, 24, 30, 0, 60, 0, 40, 60, 0, L_FILT, LW_TRI},   /* PLUCKED */
    {50, 50, 70, W_SQR, W_SAW, 24, 50, 0, 75, 60, 45, 60, 0, L_FILT, LW_TRI},   /* SYNTH BASS */
    {30, 100, 95, W_SAW, W_SAW, 24, 0, 0, 66, 30, 40, 60, 0, L_FILT, LW_TRI},   /* ACID */
    {60, 30, 20, W_SAW, W_SAW, 24, 64, 0, 80, 127, 50, 30, 4, L_PITCH, LW_TRI},   /* REESE */
    {36, 108, 100, W_SQR, W_SAW, 36, 60, 0, 62, 40, 40, 60, 0, L_FILT, LW_TRI}};   /* CURDLE: square + saw an octave up, a wah that clips: dirty, funky */
static const char *const INST_NAME[NINST] = {
    "PIANO", "XYLOPHONE", "BRASS", "MUSIC BOX", "ELEC GUITAR", "HAMMOND", "THEREMIN", "SYNTH", "CHIPTUNE",
    "BUBBLE POP", "STRINGS", "CHOIR", "ORGAN", "ELEC PIANO", "HANDPAN", "GLASS PAD", "CLOUD", "TUBA", "SLAP",
    "PLUCKED", "SYNTH BASS", "ACID", "REESE", "CURDLE"};
static const uint8_t INST_CATS[NINST] = {
    CAT_LEAD, CAT_LEAD, CAT_LEAD, CAT_LEAD, CAT_LEAD, CAT_LEAD, CAT_LEAD, CAT_LEAD, CAT_LEAD, CAT_LEAD, CAT_PAD,
    CAT_PAD, CAT_PAD, CAT_PAD, CAT_PAD, CAT_PAD, CAT_PAD, CAT_BASS, CAT_BASS, CAT_BASS, CAT_BASS, CAT_BASS, CAT_BASS,
    CAT_BASS};
static const uint8_t INST_FROM_V2[30] = {0, 1, 2, 3, 4, 5, 6, 7, 7, 8, 9, 10, 4, 11, 12, 13, 7, 14, 9, 14, 17, 15, 16, 17, 18, 19, 20, 21, 22, 19};
/* a save from 0.8 (21 sounds): its sound numbers in today's 24 (BUBBLE POP, CLOUD and CURDLE came in) */
static const uint8_t INST_FROM_V6[21] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 10, 11, 12, 13, 14, 15, 17, 18, 19, 20, 21, 22};
/* a save from 0.5 .. 0.7 (23 sounds): its sound numbers in 0.8's 21 (FUZZ and FM BASS went: SYNTH BASS) */
static const uint8_t INST_FROM_V5[23] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 18, 18, 19, 20};
static const char *const CAT_NAME[NCAT] = {"LEAD", "PAD", "BASS"};
static const int32_t CAT_LEVEL[NCAT] = {6000, 4500, 11000};   /* one voice's loudness, Q15 */
static const int8_t CAT_TRANS[NCAT] = {0, 0, -12};           /* basses sound an octave below the keys */
#define INST_CAT(i) (INST_CATS[(i) % NINST])

/* ---- the modes */
enum { M_SINGLE, M_MAJ, M_MIN, M_SEV, M_OCT, M_ARP_UP, M_ARP_DN, M_ARP_UD, M_ARP_RND, NMODE };
#define CH_INKEY 99u                            /* (not a mode: the in-key triad, the arpeggiator's CHORDS) */
#define MODE_IS_ARP(m) ((m) >= M_ARP_UP)
static const char *const MODE_NAME[NMODE] = {"SINGLE KEY", "CHORD MAJOR", "CHORD MINOR", "CHORD 7TH", "OCTAVE",
                                             "ARP UP", "ARP DOWN", "ARP UP-DOWN", "ARP RANDOM"};
static const char *const MODE_GROUP[NMODE] = {"SINGLE", "CHORDS", "CHORDS", "CHORDS", "OCTAVE", "ARPEGGIO", "ARPEGGIO",
                                              "ARPEGGIO", "ARPEGGIO"};
/* the chord the major key has on each note, from its tonic (C, or where PRESETS moved it; black keys: major):
 * 0 major, 1 minor, 2 diminished */
static const uint8_t KEY_QUAL[12] = {0, 0, 1, 0, 1, 0, 0, 0, 0, 1, 0, 2};
static uint32_t chord_of(uint32_t mode, uint32_t root, uint8_t out[4])   /* the notes a key plays: how many */
{
    uint32_t n = 0, q, i;
    static const uint8_t IV[4][4] = {{0, 4, 7, 0}, {0, 3, 7, 0}, {0, 3, 6, 0}, {0, 4, 7, 10}};   /* maj min dim 7 */
    uint32_t cnt = 3;
    switch (mode) {
    case M_SINGLE: out[0] = (uint8_t)root; return 1;
    case M_MAJ: q = 0; break;
    case M_MIN: q = 1; break;
    case M_SEV: q = 3; cnt = 4; break;
    case M_OCT: out[0] = (uint8_t)root; if (root + 12u <= 127u) { out[1] = (uint8_t)(root + 12u); return 2; } return 1;
    default: q = KEY_QUAL[(uint32_t)((int32_t)root - song_key() + 120) % 12u]; break;   /* IN KEY, the arpeggios: in the key */
    }
    for (i = 0; i < cnt; i++) {
        uint32_t nt = root + IV[q][i];
        if (nt >= 1u && nt <= 127u) out[n++] = (uint8_t)nt;
    }
    return n;
}

/* ---- per part settings the interrupt reads (the main loop writes them: set.part[], splitmilk.c) */
#define NPART 2u
#define NVP 6u                                  /* voices a part */
#define NENG 3u                                 /* voice groups: CHOC, BERRY, and PADDOCK (the pad sequencer, SEQ) */
#define P_PAD 2u
#define PAD_VOICES 6u                           /* PADDOCK: a chord a bar (up to 4), and the last one's ring */
#define NVOX (NENG * NVP)
#define ARP_VOICES 3u                           /* voices a part uses on an ARP mode */
static struct {
    uint32_t ph, ph2, inc, inc2, age;
    int32_t amp, dstep, env;
    int32_t low, band, f, q;
    uint8_t gate, note, stage, blk, fmi, fast;   /* fast: let go quickly (the overload guard, synth_shed) */
} vx[NVOX];                                     /* interrupt only */
static uint32_t vx_ages;

/* main loop -> interrupt: live notes. on: 1 note on, 0 note off, 2 all of the part off (and the arpeggio forgotten) */
#define LIVE_N 64u
static struct { uint8_t part, note, on; } live_q[LIVE_N];
static volatile uint32_t live_w, live_r;
static void live_note(uint32_t part, uint32_t note, uint32_t on)   /* main loop */
{
    uint32_t w = live_w;
    if (w - live_r >= LIVE_N) return;
    live_q[w % LIVE_N].part = (uint8_t)part;
    live_q[w % LIVE_N].note = (uint8_t)note;
    live_q[w % LIVE_N].on = (uint8_t)on;
    live_w = w + 1u;                            /* (published last) */
}

static void voice_on(uint32_t p, uint32_t note)
{
    uint32_t v, pick = p * NVP, best = 0xFFFFFFFFu, tune = part_snd(p)->tune;
    if (!note || note > 127u) return;
    uint32_t nv = p == P_PAD ? PAD_VOICES : MODE_IS_ARP(part_mode(p)) ? ARP_VOICES : NVP;   /* (an arpeggio: one note at a time, 3 tails ring) */
    for (v = p * NVP; v < p * NVP + nv; v++) {  /* a free voice (the quietest), else the oldest held one */
        uint32_t score = vx[v].gate ? 0x80000000u + (vx_ages - vx[v].age > 0x7FFFFFFFu ? 0u : 0x7FFFFFFFu - (vx_ages - vx[v].age))
                       : (uint32_t)(vx[v].env >> 8) + 1u;
        if (score < best) { best = score; pick = v; }
    }
    vx[pick].inc = INC_C9[note % 12u] >> (10u - note / 12u);
    vx[pick].inc2 = (uint32_t)(((uint64_t)vx[pick].inc * SEMI[tune > 48u ? 48u : tune]) >> 16);
    vx[pick].inc2 += vx[pick].inc2 >> 8;        /* ~7 cents apart: wider */
    vx[pick].note = (uint8_t)note;
    vx[pick].age = ++vx_ages;
    vx[pick].stage = 0;
    vx[pick].fast = 0;
    vx[pick].gate = 1;
}
static void voice_off(uint32_t p, uint32_t note)   /* note 0: all of the part; else one voice (the oldest) of that note */
{
    uint32_t v, pick = NVOX, oldest = 0;
    for (v = p * NVP; v < p * NVP + NVP; v++) {
        if (!vx[v].gate) continue;
        if (!note) { vx[v].gate = 0; vx[v].stage = 2; continue; }
        if (vx[v].note == note && (pick == NVOX || vx_ages - vx[v].age > oldest)) { pick = v; oldest = vx_ages - vx[v].age; }
    }
    if (pick < NVOX) { vx[pick].gate = 0; vx[pick].stage = 2; }   /* (two keys sharing a note: each lets go of its own) */
}

/* ---- the arpeggiator, per part (its settings: set.arp[p], the ARP page): the keys held (their roots), as in-key
 * triads (NOTES: CHORDS) or as they are (KEYS), over 1..4 octaves, one note every 1/4 .. 1/32, each sounding GATE of
 * its length; the direction is the part's mode (UP, DOWN, UP-DOWN, RANDOM). It runs on the clock's 32nd notes. */
#define ARP_KEYS 8u
#define ARP_MAX 32u
static const uint8_t ARP_DIV[4] = {8, 4, 2, 1};   /* 32nds a note: 1/4, 1/8, 1/16, 1/32 */
static const char *const ARP_RATE_NAME[4] = {"1/4", "1/8", "1/16", "1/32"};
static const uint8_t ARP_GATE_PCT[4] = {25, 50, 75, 100};
static const char *const ARP_NOTES_NAME[2] = {"CHORDS", "KEYS"};
static uint8_t arp_key[NPART][ARP_KEYS], arp_nk[NPART];   /* interrupt only */
static uint8_t arp_cur[NPART];
static uint32_t arp_idx[NPART], arp_gate[NPART];          /* the gate: samples until the note ends */
static uint32_t arp_half, arp_half_t32, arp_len32;        /* the 32nd in the middle of a 16th: samples to go, its number */

static void arp_add(uint32_t p, uint32_t root)
{
    uint32_t i;
    for (i = 0; i < arp_nk[p]; i++) if (arp_key[p][i] == root) return;
    if (arp_nk[p] >= ARP_KEYS) return;
    if (!arp_nk[p]) arp_idx[p] = 0;
    arp_key[p][arp_nk[p]++] = (uint8_t)root;
}
static void arp_del(uint32_t p, uint32_t root)
{
    uint32_t i;
    for (i = 0; i < arp_nk[p] && arp_key[p][i] != root; i++) {}
    if (i == arp_nk[p]) return;
    for (; i + 1u < arp_nk[p]; i++) arp_key[p][i] = arp_key[p][i + 1u];
    arp_nk[p]--;
}
static uint32_t arp_notes(uint32_t p, uint8_t out[ARP_MAX])   /* the notes to walk through, low to high */
{
    uint32_t n = 0, i, j, o, c, octs = (set.arp[p].oct & 3u) + 1u, keys_only = set.arp[p].notes & 1u;
    uint8_t ch[4];
    for (o = 0; o < octs; o++)
        for (i = 0; i < arp_nk[p]; i++) {
            if (keys_only) { ch[0] = (uint8_t)(arp_key[p][i] + 12u * o); c = ch[0] <= 127u ? 1u : 0u; }
            else c = chord_of(CH_INKEY, arp_key[p][i] + 12u * o, ch);
            for (j = 0; j < c; j++) {
                uint32_t k, nt = ch[j], dup = 0;
                for (k = 0; k < n; k++) if (out[k] == nt) dup = 1;
                if (dup || n >= ARP_MAX || nt > 127u) continue;
                for (k = n; k > 0u && out[k - 1u] > nt; k--) out[k] = out[k - 1u];   /* (sorted as it goes in) */
                out[k] = (uint8_t)nt;
                n++;
            }
        }
    return n;
}
static void arp_tick32(uint32_t t32)            /* interrupt: a 32nd note (t32: its number in the bar, 0..31) */
{
    static uint32_t rnd = 77777u;
    uint32_t p;
    for (p = 0; p < NPART; p++) {
        uint32_t m = part_mode(p), n, i, div = ARP_DIV[set.arp[p].rate & 3u];
        uint8_t nt[ARP_MAX];
        if (!MODE_IS_ARP(m) || !arp_nk[p] || t32 % div) continue;
        n = arp_notes(p, nt);
        if (!n) continue;
        if (m == M_ARP_RND) { rnd ^= rnd << 13; rnd ^= rnd >> 17; rnd ^= rnd << 5; i = rnd % n; }
        else if (m == M_ARP_UD && n > 1u) {     /* up then down, the ends not twice: 0 1 2 3 2 1 0 1 ... */
            uint32_t span = 2u * n - 2u, k = arp_idx[p]++ % span;
            i = k < n ? k : span - k;
        } else {
            i = arp_idx[p]++ % n;
            if (m == M_ARP_DN) i = n - 1u - i;
        }
        if (arp_cur[p]) voice_off(p, arp_cur[p]);
        voice_on(p, nt[i]);
        arp_cur[p] = nt[i];
        arp_gate[p] = arp_len32 * div * ARP_GATE_PCT[set.arp[p].gate & 3u] / 100u;   /* (100 %: until the next) */
        if (arp_gate[p] >= arp_len32 * div) arp_gate[p] = arp_len32 * div + 64u;
    }
}
static void arp_step(uint32_t step, uint32_t step_samples)   /* interrupt: a 16th note starts (the clock, sm_drums.h) */
{
    arp_len32 = step_samples / 2u;
    arp_half = arp_len32 ? arp_len32 : 1u;      /* (its second 32nd, half way: arp_gates fires it) */
    arp_half_t32 = (step % 16u) * 2u + 1u;
    arp_tick32((step % 16u) * 2u);
}
static void arp_gates(uint32_t n)               /* interrupt: n samples have passed */
{
    uint32_t p;
    if (arp_half) {
        if (arp_half > n) arp_half -= n;
        else { arp_half = 0; arp_tick32(arp_half_t32); }
    }
    for (p = 0; p < NPART; p++)
        if (arp_gate[p]) {
            if (arp_gate[p] > n) arp_gate[p] -= n;
            else { arp_gate[p] = 0; if (arp_cur[p]) { voice_off(p, arp_cur[p]); arp_cur[p] = 0; } }
        }
}

static void synth_events(void)                  /* interrupt: the live notes from the main loop */
{
    while (live_r != live_w) {
        uint32_t r = live_r, p = live_q[r % LIVE_N].part % NENG, nt = live_q[r % LIVE_N].note, on = live_q[r % LIVE_N].on;
        if (on == 2u) {                         /* all off: the mode, sound or octave changed */
            voice_off(p, 0);
            if (p < NPART) { arp_nk[p] = 0; arp_cur[p] = 0; arp_gate[p] = 0; }
        } else if (MODE_IS_ARP(part_mode(p))) {
            if (on) arp_add(p, nt); else arp_del(p, nt);
            if (!arp_nk[p] && arp_cur[p]) { voice_off(p, arp_cur[p]); arp_cur[p] = 0; arp_gate[p] = 0; }
        } else if (on) voice_on(p, nt);
        else voice_off(p, nt);
        live_r = r + 1u;
    }
}

/* the overload guard: the sound took nearly all its time. The quietest note that is already fading (let go) fades
 * out in a few ms instead: the sound stays whole (no drop-outs) and what goes is what you would hear least */
static void synth_shed(void)
{
    uint32_t v, pick = NVOX;
    int32_t least = 0x7FFFFFFF;
    for (v = 0; v < NVOX; v++)
        if (!vx[v].gate && !vx[v].fast && vx[v].env && vx[v].env < least) { least = vx[v].env; pick = v; }
    if (pick < NVOX) vx[pick].fast = 1;
}
static int synth_quiet(void)
{
    uint32_t v;
    for (v = 0; v < NVOX; v++)
        if (vx[v].gate || vx[v].amp || vx[v].env) return 0;
    return 1;
}

static inline int32_t osc_wave(uint32_t w, uint32_t ph)   /* one cycle over 2^32 -> -32767..32767 */
{
    static uint32_t nz = 24680u;
    int32_t s = (int32_t)ph;
    switch (w) {
    case W_TRI: return ((s ^ (s >> 31)) >> 15) - 32768;
    case W_SAW: return s >> 16;
    case W_SQR: return s < 0 ? -32767 : 32767;
    case W_NOISE: nz ^= nz << 13; nz ^= nz >> 17; nz ^= nz << 5; return (int32_t)nz >> 16;
    default: return sine(ph);
    }
}

static void synth_render(uint32_t p, int32_t *o0, uint32_t n)   /* part p: n samples added into o0 (mono) */
{
    static uint32_t lfo_ph[NENG], rnd = 12345u;
    static int32_t lfo_rnd[NENG];
    uint32_t inst = part_inst(p), cat = INST_CAT(inst), v, i;
    const snd_t *sp = part_snd(p);              /* the part's own copy of its sound (GLO edits it); PADDOCK: its preset */
    int32_t level = CAT_LEVEL[cat], full = level << 8, lv, lmod;
    uint32_t old = lfo_ph[p], w1 = sp->w1 % NWAVE, w2 = sp->w2 % NWAVE, mix = sp->mix & 127u;
    int32_t sus = (full >> 7) * (int32_t)(sp->s + (sp->s >> 6));
    lfo_ph[p] += LFO_INC[sp->lrate & 127u] * n;
    if (sus > full) sus = full;
    switch (sp->lwave) {
    case LW_TRI: { int32_t s = (int32_t)lfo_ph[p]; lv = ((s ^ (s >> 31)) >> 15) - 32768; break; }
    case LW_SQR: lv = (int32_t)lfo_ph[p] < 0 ? -32767 : 32767; break;
    case LW_SAW: lv = 32767 - (int32_t)(lfo_ph[p] >> 16); break;
    default:
        if (lfo_ph[p] < old) { rnd ^= rnd << 13; rnd ^= rnd >> 17; rnd ^= rnd << 5; lfo_rnd[p] = (int32_t)rnd >> 16; }
        lv = lfo_rnd[p];
    }
    lmod = (lv * (int32_t)(sp->ldepth & 127u)) >> 7;
    for (v = p * NVP; v < p * NVP + NVP; v++) {
        int32_t amp = vx[v].amp, dstep = vx[v].dstep, low = vx[v].low, band = vx[v].band, f = vx[v].f, q = vx[v].q;
        uint32_t ph = vx[v].ph, ph2 = vx[v].ph2, i1 = vx[v].inc, i2 = vx[v].inc2, blk = vx[v].blk, fmi = vx[v].fmi;
        int32_t *o = o0;
        if (!vx[v].gate && !amp && !vx[v].env) continue;   /* silent: costs nothing */
        if (sp->ltgt == L_PITCH && lmod) {      /* vibrato: up to +-1 semitone */
            int32_t pm = (lmod * 3874) >> 15;
            i1 += (uint32_t)(int32_t)(((int64_t)i1 * pm) >> 16);
            i2 += (uint32_t)(int32_t)(((int64_t)i2 * pm) >> 16);
        }
        for (i = 0; i < n; ) {
            uint32_t m, j;
            if (blk == 0u) {                    /* every 16 samples: envelope, LFO, filter coefficients */
                int32_t env = vx[v].env, eff, en, c;
                if (vx[v].stage == 0u) {
                    env += (int32_t)(((int64_t)(full + (full >> 2) + (full >> 4) - env) * ENV_K[sp->a & 127u]) >> 15);
                    if (env >= full) { env = full; vx[v].stage = 1; }
                } else if (vx[v].stage == 1u)
                    env += (int32_t)(((int64_t)(sus - env) * ENV_K[sp->d & 127u]) >> 15);
                else {
                    env -= (int32_t)(((int64_t)env * ENV_K[vx[v].fast ? 16u : sp->r & 127u]) >> 15);   /* (fast: ~1.5 ms) */
                    if (env < 4096) env = 0;
                }
                vx[v].env = env;
                eff = env;
                if (sp->ltgt == L_AMP)
                    eff = (int32_t)(((int64_t)eff * (32768 - ((((lv + 32768) >> 1) * (int32_t)(sp->ldepth & 127u)) >> 7))) >> 15);
                if (!env && vx[v].stage == 2u) { amp = 0; dstep = 0; }
                else dstep = (eff - amp) >> 4;
                en = ((env >> 8) * 32767) / level;
                c = (int32_t)sp->cut + ((en * (int32_t)sp->fenv) >> 15);
                if (w1 == W_FM) {
                    int32_t fx = (int32_t)mix + ((en * (int32_t)sp->fenv) >> 15);
                    fmi = (uint32_t)(fx > 127 ? 127 : fx);
                }
                if (sp->ltgt == L_FILT) c += (lmod * 64) >> 15;
                c = c < 0 ? 0 : c > CMAX ? CMAX : c;
                f = FTAB[c];
                q = 16384 - (int32_t)(sp->res & 127u) * 119;
                if (low > SVF_LIM) low = SVF_LIM; else if (low < -SVF_LIM) low = -SVF_LIM;
                if (band > SVF_LIM) band = SVF_LIM; else if (band < -SVF_LIM) band = -SVF_LIM;
                blk = 16;
            }
            m = n - i < blk ? n - i : blk;
            blk -= m;
            i += m;
            for (j = 0; j < m; j++) {
                int32_t x, high, s;
                amp += dstep;
                ph += i1;
                if (w1 == W_FM) {
                    ph2 += i2;
                    x = sine(ph + ((uint32_t)(sine(ph2) * (int32_t)fmi) << 9)) >> 1;
                } else if (mix) {
                    ph2 += i2;
                    x = (osc_wave(w1, ph) * (int32_t)(128u - mix) + osc_wave(w2, ph2) * (int32_t)mix) >> 8;
                } else
                    x = osc_wave(w1, ph) >> 1;
                low += (f * band) >> 14;        /* Chamberlin state-variable filter */
                high = x - low - ((q * band) >> 13);
                band += (f * high) >> 14;
                s = low > 32767 ? 32767 : low < -32767 ? -32767 : low;
                s = (s * (amp >> 8)) >> 15;
                *o++ += s;
            }
        }
        vx[v].amp = amp; vx[v].dstep = dstep; vx[v].ph = ph; vx[v].ph2 = ph2; vx[v].fmi = (uint8_t)fmi;
        vx[v].low = low; vx[v].band = band; vx[v].f = f; vx[v].q = q; vx[v].blk = (uint8_t)blk;
    }
}
