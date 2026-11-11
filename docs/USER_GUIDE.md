# SPLITMILK user guide

SPLITMILK turns the M-VAVE FM-1 into two instruments on one keyboard, with effects, an arpeggiator, a drum machine
and a pad sequencer. This guide covers everything it does. For installing it, see the [README](../README.md).

**Contents**

1. [The basics](#1-the-basics)
2. [Playing](#2-playing)
3. [Effects](#3-effects-fx-sel-env-lfo-edit)
4. [The arpeggiator](#4-the-arpeggiator-arp)
5. [The wave editor](#5-the-wave-editor-glo)
6. [The sequencer: drums and Paddock](#6-the-sequencer-seq)
7. [Saving, loading and starting over](#7-saving-loading-and-starting-over)
8. [MIDI](#8-midi)
9. [Reference: sounds, modes and kits](#9-reference)
10. [Troubleshooting](#10-troubleshooting)

---

## 1. The basics

### Two instruments

The keyboard is split in two, and each half is its own instrument:

| | Keys | Key lights | On screen |
| --- | --- | --- | --- |
| **CHOC** (left) | the lower keys, F3 to E4 at first | off, lit while pressed | the left half, chocolate brown |
| **BERRY** (right) | the upper keys, F4 to G5 at first | always lit, dark while pressed | the right half, strawberry pink |

Both play at the same time. At first CHOC is a piano and BERRY a music box; you can give each any of the 24 sounds.

### Switching on

The loading screen fills a glass with chocolate and strawberry milk, then the main screen appears. Press any key or
button to skip it. Your last setup comes back as you left it.

### The main screen

Each half of the screen shows one instrument, with its four settings numbered after the knob that changes them:

1. **OCTAVE**: from -3 to +3, with the notes its keys play right now (for example `F3-E4`)
2. **SOUND**: its sound, and the sound's group (LEAD, PAD or BASS)
3. **MODE**: how a key plays (single note, a chord, an octave, or an arpeggio)
4. **VOLUME**: from 0 to 100 %, shown as a glass of milk filling up

A small keyboard at the bottom of each half shows the keys you are holding. The bar along the bottom of the screen
shows the drum kit, the tempo and the 16 steps of the drum pattern; it also shows short messages when you change
something.

### Choosing an instrument to edit

The knobs, and the effect, arpeggiator and wave editor pages, always change one instrument:

- Press **OCT-** to choose **CHOC**. OCT- lights up and CHOC's half of the screen gets a brown outline.
- Press **OCT+** to choose **BERRY**. OCT+ lights up and BERRY's half gets a pink outline.

The choice stays until you press the other button. The `-` or `+` in each instrument's name tag on screen reminds
you which button chooses it.

### Getting around

- **HOME** always goes back to the main screen.
- Each page's button (ARP, GLO, SEQ, SAVE) opens it, and pressing it again goes back.
- Pages that work on one instrument show its name tag in the top right corner. Press OCT- or OCT+ to switch the page
  to the other instrument.

---

## 2. Playing

### Sound, octave, mode and volume

On the main screen, with an instrument chosen (OCT- or OCT+):

| Knob | Changes |
| --- | --- |
| **KNOB 1** | octave (-3 to +3) |
| **KNOB 2** | sound |
| **KNOB 3** | mode |
| **KNOB 4** | volume (in steps of 5 %) |

Changing the octave, sound or mode stops that instrument's notes so nothing is left hanging; press the keys again.

### Modes

| Mode | Each key plays |
| --- | --- |
| **SINGLE KEY** | its own note |
| **CHORD MAJOR** | a major chord built on that note |
| **CHORD MINOR** | a minor chord |
| **CHORD 7TH** | a seventh chord (four notes) |
| **OCTAVE** | its note and the note an octave above |
| **ARP UP / DOWN / UP-DOWN / RANDOM** | the arpeggiator: the keys you hold play one note at a time, in that order |

The arpeggiator follows the tempo, whether or not the drums are playing. Its speed, range and note length are on the
[ARP page](#4-the-arpeggiator-arp).

### Moving the split

Turn **SELECT** on the main screen to move the split one key at a time: clockwise gives CHOC more keys, anticlockwise
gives BERRY more. Each side always keeps at least one key. While you turn, the bar at the bottom shows where the split
is (for example `SPLIT E4 | F4`), and the key lights follow. Notes that are sounding carry on.

### Changing key

Turn **PRESETS** to move everything up or down a semitone at a time, up to an octave either way: both instruments,
the chords, the arpeggiator, Paddock and incoming MIDI notes. The bar shows the key while you turn (for example
`KEY D +2`) and keeps showing it next to the tempo when it isn't C. Changing key stops the notes that are sounding.

### Tempo

Turn **ALGORITHM** to set the tempo, from 40 to 240 BPM. It drives the drums, Paddock, the arpeggiator and the delay
effect. (On the sequencer page, ALGORITHM does something else; use KNOB 1 there.)

### Latch

**Latch** keeps notes playing after you let go of the keys.

1. On the main screen, hold a note, a chord or an arpeggio.
2. Press **REC**. The bar shows **LATCH** and REC stays lit.
3. Let go of the keys: what you were holding keeps playing (an arpeggio keeps running).
4. Press **REC** again to stop it.

While latched, the keys are free: you can play anything over the top, including the same keys. A latch also survives
moving the split.

---

## 3. Effects: FX, SEL, ENV, LFO, EDIT

Each instrument has five effects:

| Button | Effect | Its settings |
| --- | --- | --- |
| **FX** | Reverb | Level, Tone, Time |
| **SEL** | Delay | Repeat (1/16 to 1/2 note, in time with the tempo), Echo, Intensity |
| **ENV** | Distortion | Level, Tone, Dist |
| **LFO** | Chorus | Rate, Depth, Level |
| **EDIT** | Phaser | Rate, Depth, Feedback |

- **Tap** a button to turn that effect on or off for the chosen instrument. The button lights while the effect is on,
  and the bar shows a message (for example `REVERB ON  CHOC`).
- **Hold** a button for half a second to open its page. There, **KNOB 1** turns the effect on (three clicks
  clockwise) or off (three clicks back), and **KNOB 2-4** set its three settings, shown from 0 to 20.
- To put an effect back to its default settings, **hold REC** for a second on its page.

There is one reverb, one delay and one chorus, shared by both instruments. Each instrument turns them on and off and
sets its own amount (Reverb Level, Delay Echo, Chorus Level), but the other settings of those three are the same for
both; on the page they are marked **BOTH**. Distortion and phaser belong to each instrument separately. The drums and
Paddock have no effects.

---

## 4. The arpeggiator: ARP

The arpeggiator plays when an instrument's mode is one of the **ARP** modes; the mode sets the direction (up, down,
up-down or random). The ARP button glows when the chosen instrument is in an ARP mode. Press **ARP** to set it up:

| Knob | Sets |
| --- | --- |
| **KNOB 1** | Rate: a note every 1/4, 1/8, 1/16 or 1/32 |
| **KNOB 2** | Octaves: how many octaves it climbs, 1 to 4 |
| **KNOB 3** | Gate: how long each note sounds, 25 % to 100 % |
| **KNOB 4** | Notes: **CHORDS** plays a chord (in the current key) on each key you hold; **KEYS** plays just the keys |

To put it back to its defaults (1/16, 2 octaves, 50 %, chords), **hold REC** for a second on this page.

---

## 5. The wave editor: GLO

**GLO** opens the chosen instrument's sound for detailed editing. Turn **SELECT** to move between its four pages.
Each page draws a picture of what it shapes, and has four settings on KNOB 1-4:

| Page | Picture | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
| --- | --- | --- | --- | --- | --- |
| **OSC** | the waveform | Wave 1 | Wave 2 | Tune 2 (-24 to +24 semitones) | Mix |
| **FILTER** | the filter curve | Cutoff | Resonance | Env amount | |
| **ENV** | the envelope | Attack | Decay | Sustain | Release |
| **LFO** | the LFO's wave | Rate | Depth | Target (pitch, filter or volume) | Shape |

The waves are Sine, Triangle, Saw, Square, Noise and FM (Wave 1 only: a sine bent by Wave 2).

- To put the page you are on back to how the sound started, **hold REC** for a second.
- Choosing a different sound with KNOB 2 on the main screen starts again from that sound's own settings, replacing
  your edits.

---

## 6. The sequencer: SEQ

Press **SEQ** to open the sequencer, and **SEQ** again (or **HOME**) to go back; everything keeps playing. Turn
**SELECT** to switch between its two tabs, **Dairy Drums** and **Paddock**. **PLAY** starts and stops the drums and
Paddock together, from any screen.

### Dairy Drums: the drum machine

Six rows of drums (kick, snare, closed hat, open hat, clap and percussion) and 16 steps.

- The **16 white keys** are the 16 steps. Press one to add or remove the chosen drum on that step. The key lights
  show the chosen drum's steps.
- The **first six black keys** (F#3, G#3, A#3, C#4, D#4, F#4) choose a drum and play it. **ALGORITHM** also steps
  through the drums.
- **KNOB 1** tempo, **KNOB 2** kit, **KNOB 3** swing (0 to 100), **KNOB 4** drum volume.
- **REC**: tap it to clear the chosen drum's row; hold it for a second to clear the whole pattern.

The **CPU** number at the top shows how hard the sound engine is working. If it reaches 80 % it turns pink, and
SPLITMILK quietly cuts the faintest fading notes short so the sound stays clean.

### Paddock: the pad sequencer

Paddock is a third instrument that plays a slow progression along with the drums. Each bar holds one note or one
chord (up to four notes) for its whole length, and there are up to four bars, which loop.

- **To set a bar**, press a key, or hold the keys of a chord together. You hear it, and when you let go, the next bar
  is selected, so four chords in a row fill four bars.
- **ALGORITHM** moves the selected bar's note or chord up or down a semitone at a time. On an empty bar it starts
  at C3.
- **OCT-** and **OCT+** select the bar before or after.
- **PRESETS** moves Paddock up or down an octave (-3 to +3).
- **REC**: tap it to make the selected bar silent (and move on to the next); hold it for a second to clear every bar.
- **KNOB 1** tempo, **KNOB 2** sound (one of the 7 pad sounds), **KNOB 3** number of bars (1 to 4), **KNOB 4** volume.

Each bar's card shows its lowest note and the chord's name (MAJ, MIN, 7, SUS4 and so on). The bar being set is
outlined, and the playing bar fills up pink as it plays.

---

## 7. Saving, loading and starting over

### Automatic saving

Your current setup is saved by itself about two seconds after a change, once the drums are stopped and nothing is
sounding, so switching off loses nothing. (It waits for silence because saving would interrupt the sound.)

### The save page

Press **SAVE** to open the save page (and **SAVE** again to go back). There are four slots, and each holds your whole
setup: both instruments with their sounds, effects and arpeggiators, the split, the key, the drums and Paddock.

- **KNOB 1** chooses a slot.
- **REC** saves to it; **PLAY** loads it. Each one asks first (`SAVE 2?`): press the same button again within three
  seconds to confirm. Any other button, or waiting, cancels.

The drums stop before saving or loading. A slot is checked before it is loaded, so a damaged slot shows
`LOAD FAILED` and changes nothing.

### Starting over

Hold **REC** for a second on the main screen. Everything goes back to how it was first installed, and the bar shows
`EVERYTHING RESET`. Your four save slots are kept.

---

## 8. MIDI

- **MIDI in** (USB or the TRS jack, any channel): notes are split at the same point as the keys and play CHOC or
  BERRY in their current mode and sound, moved into the current key. The OCTAVE setting doesn't apply to MIDI notes.
- **MIDI out** (USB): the keys you press, CHOC on channel 1 and BERRY on channel 2. Each key sends its own note,
  not the chord it plays.

---

## 9. Reference

### The 24 sounds

| Group | Sounds |
| --- | --- |
| **Lead** | Piano, Xylophone, Brass, Music Box, Elec Guitar, Hammond, Theremin, Synth, Chiptune, Bubble Pop |
| **Pad** | Strings, Choir, Organ, Elec Piano, Handpan, Glass Pad, Cloud |
| **Bass** | Tuba, Slap, Plucked, Synth Bass, Acid, Reese, Curdle |

Basses sound an octave lower than the keys. Each instrument can play up to six notes at once (three in an ARP mode).

### The drum kits

| Kit | Character | Percussion row |
| --- | --- | --- |
| **ROCK** | punchy kick with a beater click, fat snare | floor tom |
| **MINI POPS** | real samples of a Korg Mini Pops 120 | wood block (the clap row plays a bright snare) |
| **CR-78** | short, light and clicky | conga |
| **606** | small and snappy | shaker |
| **808** | long, low boom (the default kit) | high tom |
| **LO-FI** | low and crushed, like an old sampler | low tom |
| **INDUSTRIAL** | loud and long, crushed hard | heavy knock |
| **LM-1** | real samples of a Linn LM-1 | high tom (the clap row plays a low tom) |

On the sampled kits, the drum rows on screen are named after what they play (for example SNAP and WOOD on the Mini
Pops, TOM L and TOM H on the LM-1).

---

## 10. Troubleshooting

- **The sound crackles or breaks up.** Too many notes are sounding at once. Look at the CPU number on the sequencer
  page; use fewer effects, a shorter sound, or fewer notes in the arpeggiator.
- **Notes keep playing.** Latch may be on (REC is lit): press **REC**. Changing the sound or mode also stops all of
  that instrument's notes.
- **A knob changes the wrong instrument.** Press **OCT-** (CHOC) or **OCT+** (BERRY) to choose.
- **On the sequencer, OCT- / OCT+ don't choose an instrument.** On the Paddock tab they select bars instead; go back
  to the main screen or the Dairy Drums tab.
- **The FM-1 won't start, or an install was interrupted.** Hold **OCT-** while switching on to enter USB rescue
  mode, then install again (see the [README](../README.md)).
