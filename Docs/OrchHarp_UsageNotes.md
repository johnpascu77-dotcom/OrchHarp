# OrchHarp — using it

You, at the plugin UI. For the design/architecture see `OrchHarp_Design.md`.

OrchHarp sits on a harp track **in place of OrchNoteFilter** (they both constrain
pitch class and would fight):

```
note source (MPL / clip / Dorico-exported MIDI) -> [Randomize] -> OrchHarp -> OrchNoteMapper -> OrchGate -> OrchCapture -> instrument
```

---

The editor has three tabs. **Harp** — the composing surface (mode, contour,
pedal diagram, the 7 pedals, the bank, the Family Helper). **Motion** — what
drives change over time (the governor, the CC / note triggers, both glissando
engines). **Voicing** — the playability reduction (hands, polyphony, span).
The status line at the bottom is always visible.

## Contour mode  (Harp tab)

`Pitch Mode` = **Contour** stops mapping exact pitches and instead keeps the
input melody's **shape and rhythm**, re-drawing it on the current pedal
diagram's degrees. A chromatic line through a whole-tone or hexachord diagram
comes out as that scale with the same gesture — an instant, controllable
"variation" of a line for a development section.

- **step** — how far the output moves per input interval: *Tight* (scale-step
  feel), *Literal* (1 degree per semitone — wide), *Compress*, *Expand*.
- **chords** — *Monophonic* (a simultaneity keeps only its first note) or
  *Stack* (the rest take the next lower diagram degrees).
- **low / high note** — the register the contour is kept inside.

The contour is a **cumulative walk** — each note steps from where the last one
landed — kept inside **low / high note**. It re-seeds near the input's real
pitch after a rest (≥ 2 beats) or on transport stop, so each phrase starts in
the right register instead of drifting. Re-pedal mid-phrase and the line
recolours. Black-key modes don't apply in Contour. Absolute mode is the normal
transform.

## Voicing  (Voicing tab)  —  off by default

Turn **Voicing on** to reduce the input to what a harpist could physically
grab. The **Left hand** / **Right hand** buttons set that instance's hand,
range, protect and **Split = Block** in one click (concert-harp values: LH
C1–C5 / Keep Lowest, RH G2–G7 / Keep Highest). Switch the split to **Channel**
afterward only if your source is already hand-separated onto MIDI ch 1 / 2.
Notes are grouped by onset (**onset window ms** ≈ one hand placement) and each
group is filtered:

- **Hand** — Both / Left / Right. **Split** — how a chord is divided between the
  hands: *Block* (Left takes notes below the **Split note**, Right takes the
  rest), *Interlock* (alternating notes), *Channel* (the source already put the
  hands on separate MIDI channels — set **Split channels L / R**), *Off*.
- **Split note** — the register line for *Block* (default C4). It also decides
  which hand a **lone** note goes to under *Block* or *Interlock*, so a
  monophonic line lands on one instance instead of being dropped by both.
- **Max voices** — notes kept per hand per placement (4 = a hand).
- **Range mode** — *Min/Max* (the hand window is **Hand low / high note**) or
  *Center/Span* (the window is **Center note** ± **Span**/2). In Center/Span
  every note is transposed by (Center − 60) semitones and snapped to the nearest
  string — so **automating Center note** during playback walks the material
  **stepwise up and down the pedal scale**, through the adjacent notes (not by
  octave jumps). Center 60 = no shift. Works from a single repeated note: a
  rhythm on one pitch walks the scale with the knob. This is one harpist's hand;
  give the two instances their own Center automation (a Random modulator at
  different rates) for two independent travelling hands. **Span** keeps the
  result near Center — anything past the window octave-folds or clamps back.
  **Hand low / high note stays the hard reach limit in both modes** — a
  travelling Center is clipped into it, so the hand can't run off the
  instrument; drive Center past the limit and the hand pins at its edge.
- **Hand low / high note** / **Center note / span** + **out-of-range** — Drop /
  Fold an octave / Clamp, applied at the window edge.
- **Max span** — the widest reach (16 semitones = a 10th). **Over-span**:
  *Drop Widest*, *Fold* an outlier in, or *Roll* (fast strum). **Roll rate**
  sets the strum spacing.
- **Protect** — which notes survive when something has to go: *Keep Both Ends*
  (bass + top, drop from the inside), *Keep Lowest*, *Keep Highest*, *None*.
  Protect always wins over span.
- **Output channel** — 0 keeps the source channel; set a value to tag all this
  instance's output — **voiced notes and the glissando engines** — to one
  channel. In a two-instance rig this is what puts each hand (and its gliss) on
  its own Dorico voice. Stays editable with Voicing off, for a gliss-only rig.
- **Damp on next attack** (on by default) — notes struck **together** (a chord)
  keep their full length; a **new** attack cuts off whatever was still ringing
  from the previous one. A legato or overlapping melodic line then notates as
  clean successive notes instead of a stack of overlapping ties, and a repeated
  pitch comes through instead of being eaten as a same-string collision. Turn
  it off to keep every note ringing for its full input length.

### Two-instance left/right rig

Load **two** OrchHarp instances on the harp track (or parallel tracks). Feed
both the same MIDI **and** the same pedal automation / CC49 (both must sound the
same diagram or they drift apart harmonically). Hit **Left hand** on one,
**Right hand** on the other. Set both to the same **Split** / **Max voices** /
**Onset window**. The split is deterministic, so the two instances carve up
every chord identically with no link between them.

- **Split = Channel** needs the *source* to already put left-hand notes on MIDI
  ch 1 and right-hand on ch 2 (a Dorico two-stave export, or a Bitwig note-FX
  splitting by pitch). Then leave **Output channel = 0** — ch 1 / 2 pass
  straight through to Dorico as two voices.
- **Split = Block** derives the hands from a single stream by register: Left
  keeps notes below the **Split note**, Right keeps the rest. Both instances
  must use the **same Split note**. Set **Output channel** 1 and 2 to tag them
  for Dorico's two voices (up / down stems).

Then send both into OrchCapture → the merge is one harp part, two voices.

### Contour range with Voicing on

The **contour window** (Harp tab) bounds the generated line; the **hand range**
(Voicing tab) then clamps each hand's slice. For a two-instance contour texture,
set *both* instances' contour window to the whole intended register and let the
hand ranges + split carve it.

A **Monophonic** contour is one note per onset. Block / Interlock split then
route each note to a *single* hand by the **Split note**: a line that stays on
one side of it plays on only one instance. To get **two variants of one
melodic line**, don't split — set **Split = Off** (or **Hand = Both**) on both
instances and differ them by **Contour step**, diagram, or contour window.
Split is for dividing a genuinely two-handed texture (use **Stack** chords, or
a polyphonic input).

Voicing off = the plugin behaves exactly as before.

## Pedal-change markers in the score

OrchHarp can hand its pedal changes to OrchCapture so they land as text in the
Dorico score (instead of leaning on Dorico's semi-automatic *Calculate Harp
Pedals*). Nothing to switch on:

- While the transport runs, OrchHarp logs every pedal-diagram change of the take.
- On **stop**, it writes them to a temp file (`orchharp-pedals-<id>.txt`).
- When **OrchCapture** next exports (its auto-save-on-stop, or a manual drag-out),
  it folds any fresh OrchHarp file into its section markers — so the score shows
  `Harp pedals: Db C B | E F# G Ab` at each change, in the same board notation
  Dorico's harp-pedal popover uses (copy it straight across).

It logs the **requested** diagram (the pedal move you automated), not the
governor's intermediate steps, and assumes 4/4 for the bar position — same as
the rest of OrchHarp and OrchCapture. Two instances of a L/R rig both write
their file; OrchCapture de-duplicates identical changes.

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
- **Shift-click** (or Ctrl / Alt-click) a slot — save the **current** diagram
  into it. The slot re-labels itself with the spelling so you can see it took.
- **Right-click** a slot — *Save current diagram here* / Rename… / Recolour…
  (the menu Save is a reliable alternative if your host eats modifier-clicks).

The factory bank ships filled with the common glissando starting points (major,
the three minors, whole-tone, the pentatonics, both octatonics, quartal, two
atonal hexachords). Overwrite freely — a factory reset isn't wired yet, so keep
a slot or two spare if you're experimenting.

---

## Family Helper  ->  Write to slot

A calculator for filling a bank slot from a scale instead of clicking seven
pedals. It does **not** change the live diagram — it only writes a slot (and
**renames the slot** to match what it wrote).

1. **Family** — Major / Minor, 7-Chord, Whole Tone, or Pentatonic.
2. **Variant** — repopulates per family (Major / Natural Minor / Harmonic Minor /
   Melodic Minor; the five 7th-chord types; the two pentatonics).
3. **C** (base key) — the root, C…B.
4. The **number box** — which bank slot to write (1–12).
5. **Write to slot** — computes the best pedal diagram that sounds exactly that
   scale, drops it into that slot and names the slot (e.g. "A Harmonic Minor").
   For a 7-note scale that's the one correct spelling; for a smaller set the
   leftover pedals enharmonically double a scale note.

Then **click that slot** to make it live.

### Custom scales / hexachords  ->  "or PC set"

The family list doesn't cover every set. Type a **pitch-class set** into the
`or PC set` field — space, comma or semicolon separated, e.g. `0 1 2 6 7 8` or
`0,1,4,5,8,9` — pick a slot in the number box, and hit **Write PC set**. It
best-fits the seven pedals to that set (extra letters double the nearest member)
and names the slot `Set {0,1,2,6,7,8}`. This is the path for atonal subsets.

### Any other diagram

Set the pedals by hand — the 7 combos or clicking the diagram — then
**Shift-click a slot** to save the current diagram there. Right-click the slot
to rename it.

---

## Playability Governor

Off = pedals follow your changes instantly (Synchron-Harp chaos). On = the
*sounding* diagram only moves as fast as a section harpist could pedal it:

- **Min Change Interval** — the fastest one pedal move is allowed (1/8 … 2 bars,
  4/4 assumed).
- **Changes at rests only** — hold every move until nothing is sounding on the
  track.
- **Avoid ringing pedal change** — delay a move on a pedal whose string sounded
  in the last interval (buzz avoidance; irrelevant to the notated result). It's
  a *soft* delay — once a move is well overdue it goes through anyway, so a
  dense passage can't freeze the governor.

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

**Black keys outside the ctrl zone**, with **Black Keys = Control**, re-pedal the
harp: the accidental re-tunes its nearest pedal so that pitch class plays, and
sounds on that string. Subsequent naturals on that letter keep the re-tuned
pitch until another change (a bank recall, CC49, or a different accidental). Feed
a chromatic sonata this way and it re-pedals itself — the emergent Synchron-style
trick. (This is a deliberate pedal move, so it ignores the governor's pacing.)

---

## Contour Glissando

A CC contour becomes a run: as the CC sweeps between two notes, OrchHarp fires a
note every time the diagram-snapped string changes.

- **Gliss CC#** — the CC to follow. `0` = engine off.
- **Gliss low note / high note** — the two ends of the sweep, as real notes
  (shown as note names). CC 0 → low note, CC 127 → high note. Set **low above
  high** for a contour that descends as the CC rises. Only the harp strings that
  fall between them play — the CC steps through the current diagram's degrees,
  not chromatically.
- **Vel CC#** — a second CC for live velocity. `0` = use the fixed value.
- **fixed vel** — velocity when Vel CC# is 0.
- **Ring** — *Monophonic*: each new string cuts the previous note (clean run —
  the one for notation). *Ring*: notes pile up and ring like a real harp gliss.
- **Release** — how long the CC must sit still before the **last held note is
  let go** (Hold / 1/8 / 1/4 / 1/2 / 1 bar). *Hold* keeps the old behaviour
  (the note rings until the CC hits 0 or the transport stops). Use 1/4 or 1/8
  so a drawn ramp ends with a note of sane length in the score.

Draw a rising ramp → ascending run. An LFO → oscillating runs. A hand-drawn
curve → that shape as harp notes. Change a pedal mid-sweep and the run recolours
from that point.

## Trigger Glissando

One note in a zone fires a whole run — the classic notated gliss, no drawing.
The trigger note's pitch is irrelevant; it just fires.

- **Trigger note zone lo / hi** — the note range that fires a run. **0 / 0 =
  off.** Put it somewhere out of the way (e.g. MIDI 20–23).
- **Run low note / high note** — the two ends of the run, as real notes. This is
  a **separate** window from the Contour engine's.
- **Run direction** — Up (from the low note), Down (from the high note),
  Up-Down, Down-Up.
- **Run duration** — total length, tempo-synced (1/16 … 1 bar; "1 bar" follows
  the host time signature).
- **Note velocity scales the reach** — a soft trigger note runs a short way in
  from the start end; full velocity runs the whole window.

The run follows the current diagram; a pedal change mid-run recolours the tail.
Ring / Monophonic and Release are shared with the Contour engine.

So for a big descending gliss: **Run low note** = the bottom, **Run high note**
= the top, **Run direction = Down**, and hit the trigger note hard for the full
span. No base-octave guessing — the two notes say exactly where it runs.

## Bisbigliando

The harp "whisper" — a fast measured tremolo on one pitch. Hold a note in the
bisb zone and it rustles until you release it; a **Bisbigliando** marker goes to
the score.

- **Bisb zone lo / hi** — the trigger range. **0 / 0 = off.** Keep it clear of
  the harp's playing range (e.g. MIDI 16–19).
- **Bisb rate** — the tremolo subdivision (1/16 … 1/64, straight or triplet),
  tempo-synced.
- **Enharmonic rock** — off: repeats the one note (the standard tremolo Dorico
  engraves). On: rocks between that string and its nearest neighbour within a
  few semitones, for the real doubled-pedal shimmer when the diagram has one.

Notes ride the diagram and the **Output channel** tag like the gliss engines.
