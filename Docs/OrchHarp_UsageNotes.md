# OrchHarp — using it

You, at the plugin UI. For the design/architecture see `OrchHarp_Design.md`.

OrchHarp sits on a harp track **in place of OrchNoteFilter** (they both constrain
pitch class and would fight):

```
note source (MPL / clip / Dorico-exported MIDI) -> [Randomize] -> OrchHarp -> OrchNoteMapper -> OrchGate -> OrchCapture -> instrument
```

---

## The pedal diagram

The seven strokes are the harp's seven pedals, in the real board order
`D C B | E F G A` (left foot, divider, right foot). A stroke **above** the line
is that pedal flat, **through** the line natural, **below** sharp.

- **Solid blue** = what the harp is sounding right now.
- **Faint amber ghost** = where a pedal is heading while the governor moves it
  (you only see this with the governor on and a change in flight).

**Click a column** at flat / natural / sharp height to set that pedal — same as
using the combo box under it, just faster. The 7 combos and the diagram always
agree.

---

## The bank (12 slots)

Each slot holds a full pedal diagram (7 offsets) plus a name and a colour.

- **Click** a slot — recall it. Writes the 7 pedal params, so the diagram and
  combos jump to that slot. This is also what `bankSlot` automation and the
  CC / black-key triggers do.
- **Shift-click** a slot — save the **current** diagram into it.
- **Right-click** a slot — Rename… / Recolour….

The factory bank ships filled with the common glissando starting points (major,
the three minors, whole-tone, the pentatonics, both octatonics, quartal, two
atonal hexachords). Overwrite freely — a factory reset isn't wired yet, so keep
a slot or two spare if you're experimenting.

---

## Family Helper  ->  Write to slot

A calculator for building a slot from a scale instead of clicking seven pedals.
It does **not** change the live diagram — it only fills a bank slot.

1. **Family** — Major / Minor, 7-Chord, Whole Tone, or Pentatonic.
2. **Variant** — repopulates per family (Major / Natural Minor / Harmonic Minor /
   Melodic Minor; the five 7th-chord types; the two pentatonics).
3. **C** (base key) — the root, C…B.
4. The **number box** — which bank slot to write (1–12).
5. **Write to slot** — computes the best pedal diagram that sounds exactly that
   scale and drops it into that slot. For a 7-note scale that's the one correct
   spelling; for a smaller set (chords, pentatonics, whole-tone) the leftover
   pedals enharmonically double a scale note, which is how a harpist mutes a
   string.

Then **click that slot** to make it live.

Example: Family *Major / Minor*, Variant *Harmonic Minor*, key *A*, slot *4*,
Write to slot -> slot 4 now holds A harmonic minor; click it and white-key input
plays that scale.

---

## Playability Governor

Off = pedals follow your changes instantly (Synchron-Harp chaos). On = the
*sounding* diagram only moves as fast as a section harpist could pedal it:

- **Min Change Interval** — the fastest one pedal move is allowed (1/8 … 2 bars,
  4/4 assumed).
- **Changes at rests only** — hold every move until nothing is sounding on the
  track.
- **Avoid ringing pedal change** — skip, this move, any pedal whose string
  sounded in the last interval (buzz avoidance; irrelevant to the notated
  result).

Foot rules are automatic: at most one of `{B,C,D}` and one of `{E,F,G,A}` per
move. A flat↔sharp swing on one pedal takes two moves in this build.

While the transport is **stopped** the sounding diagram snaps straight to what
you've dialled in — you set the pedals during the rest, then play.

---

## Triggers (recall a bank slot hands-free)

- **CC# Bank** — default 49 (the reserved Harp slot). An incoming CC on that
  number picks a slot across its 0–127 range. `0` disables. This is how MC /
  OrchConductor re-pedals the harp along a piece.
- **CC Channel** — 0 = listen on any channel.
- **Ctrl direct-select note range** (default 0–11) — with **Black Keys = Control**,
  a black key in this range selects `slot = note − low`.
- **Ctrl step down / up note** (default 12 / 13) — one note steps `bankSlot` −1 /
  +1. Good for an LFO or ramp walking the bank, or a stepper pedal.

Feed a chromatic sonata with **Black Keys = Control** and its accidentals
re-pedal the harp on the fly — the emergent Synchron-style trick.

---

## Contour Glissando

A CC contour becomes a run: as the CC sweeps, OrchHarp fires a note every time
the mapped string changes.

- **Gliss CC#** — the CC to follow. `0` = engine off.
- **lo string / hi string** — the run's range, as absolute string indices
  (0 = the low C, each step = one string). Set **lo > hi** for a contour that
  descends as the CC rises.
- **base oct** — where string index 0 sits (2 = MIDI 24 / C1). Nudge to move the
  whole run up or down an octave.
- **Vel CC#** — a second CC for live velocity. `0` = use the fixed value.
- **fixed vel** — velocity when Vel CC# is 0.
- **Ring** — *Monophonic*: each new string cuts the previous note (clean run,
  the one to use for notation). *Ring*: notes pile up and ring like a real harp
  gliss; they release when the Gliss CC returns to 0, on transport stop, or on
  All-Notes-Off.

Draw a rising ramp -> ascending scale run. An LFO -> oscillating runs. A
hand-drawn curve -> that shape as harp notes. Change a pedal mid-sweep and the
run recolours from that point.

## Trigger Glissando

One note in a zone sprays a whole run — the classic notated gliss, no drawing.

- **Trigger note zone lo / hi** — the note range that fires a run. **0 / 0 =
  off.** Put this somewhere out of the way (e.g. MIDI 20–23).
- **Run direction** — Up / Down / Up-Down / Down-Up.
- **Run span (strings)** — how many strings the run covers. **Note velocity
  scales it**: a soft note ≈ a quarter of the span, full velocity = the whole
  span.
- **Run duration** — total length, tempo-synced (1/16 … 1 bar, 4/4 assumed).

The run starts from the string nearest the note you played and follows the
current diagram; a pedal change mid-run recolours the tail. Ring / Monophonic
is shared with the Contour engine.
