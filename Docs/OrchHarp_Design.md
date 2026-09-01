# OrchHarp — Design

Date: 2026-08-31 (Phase 1 + 2a built 2026-09-01)
Status: **Phase 1 live** (transformer — pure logic + check tool, processor,
visual editor with the real harp pedal-diagram readout; live-tested in Bitwig,
"stunning results"). **Phase 2a built, not yet live-tested** — the two glissando
engines (contour-follower + trigger-gesture), 11 new params, 40 check assertions
green, VST3 clean (VS 18 2026 / JUCE `C:/JUCE/JUCE`). Contour engine + pedal-run
recolouring confirmed live 2026-09-01; click-to-edit on the pedal diagram added.
**Phase 2b** (pedal-change markers → OrchCapture) deprioritised — see §9.
Usage walkthrough: `OrchHarp_UsageNotes.md`.
Repo: `C:\AudioDev\Repos\OrchHarp` (git initialised, no remote yet). GitHub
`johnpascu77-dotcom/OrchHarp` (public, like the rest of the Orch family).
Plugin code `Ohrp`, VST3, MIDI effect. Consumes the CC49/Harp slot reserved in
OrchConductor since v1 (previously behaviour-undefined).

---

## 1. Purpose

A MIDI-effect emulation of a **concert (pedal) harp's pitch behaviour**, inspired
by the Vienna Synchron Harp's "Pedal mode" — but with **MIDI at the end**, so the
transformed stream can be captured (OrchCapture) and notated in Dorico.

The user's existing workflow: feed classical-sonata MIDI through the Synchron
Harp, choose/switch pedal diagrams, and get a "completely different playback" —
pitches filtered and transformed onto the harp's 7 available strings, with the
music's own low notes flipping pedal diagrams on the fly. The Synchron Harp has
**no MIDI out** (the transform happens inside a sampler), so that result can
never be recovered as notes. OrchHarp re-implements the transform as a MIDI
effect the user controls and adapts, oriented toward **atonal / exotic-subset**
use rather than the Synchron product's tonal focus.

It also becomes the **notation twin** of the Synchron Harp: run the same MIDI
through both with matching diagrams and the score matches what you hear.

### Relationship to OrchNoteFilter

OrchHarp shares DNA with ONF (pitch-class constraint, nearest-snap, CC control,
a keyswitch zone) but is a **separate plugin**: its vocabulary is *pedal
diagrams* (7 strings, one per letter name), it *consumes* keyswitches to switch
diagrams rather than passing them through, it is the CC49 instrument, and it
carries a **playability governor** that has no ONF equivalent. Reuse ONF's
pure-logic style (`OrchNoteFilterFieldLogic` is the model), not its code.

## 2. Chain position

Replaces ONF on a harp track (they would fight — both constrain pitch class):

```
Note source (MPL / clip / Dorico-exported MIDI) → [Randomize] → OrchHarp → OrchNoteMapper → OrchGate → OrchCapture → instrument
```

---

## 3. The string model

7 strings per octave, indexed by letter: `C=0 D=1 E=2 F=3 G=4 A=5 B=6`.

```
stringSemitone[letter] = {0, 2, 4, 5, 7, 9, 11}[letter] + pedalOffset[letter]
pedalOffset ∈ { flat = -1, natural = 0, sharp = +1 }
```

A **pedal diagram** = the 7 `pedalOffset` values, one per letter. The 7 resulting
string pitches **may collide** (`E♯` and `F♮` both = F): that is the harpist's
"double" pedalling — the way you mute a pitch class in real playing and writing.
**Strict 7** — there is no "off". Muting = enharmonic doubling.

`OrchHarpPedalLogic` (pure, checked):
- `stringPitchClass(letter, diagram)` → 0..11
- `diagramPitchClasses(diagram)` → the (up-to-7) distinct pcs
- `familyVariantKeyToDiagram(family, variant, baseKey)` → diagram (bank-fill helper)
- `nearestStringIndex(inputPc, diagram)` → letter (for the black-key "Nearest" mode)
- `playablePedalStep(current, target, lastMoveInfo)` → the next intermediate
  diagram one move closer to `target` under the foot rules (§6)

---

## 4. Input handling

### Master mode — `mode` param: **Pedal** / **Chromatic**

- **Chromatic** = total bypass. Every message passes through untouched.
- **Pedal** = the transform is active (below).

### In Pedal mode

- **White-key input** (letter ∈ C D E F G A B, any octave) → that letter's
  string, same octave, sounding its pedal-tuned pitch. A white-key run in → the
  current diagram's scale out. This is the primary "play the strings" path,
  matching the Synchron Harp — the gliss engine (Phase 2) and a human at a
  keyboard both drive it the same way.

- **Black-key input** (C♯ D♯ F♯ G♯ A♯) → per `blackKeyMode` param:
  - **Control** (default) — the note is consumed and routed to the control zone
    (§7): recall / step a bank slot. Feed a chromatic sonata and its accidentals
    re-pedal the harp on the fly — the emergent Synchron-style behaviour.
  - **Nearest string** — the note sounds, snapped to the nearest string pitch in
    the current diagram (clean transform, no note loss). Uses
    `nearestStringIndex`.
  - **Drop** — filtered out (creates rhythmic gaps where accidentals were).
  - **Nudge** (experimental, off-by-default feel) — a black key far outside the
    current diagram bends the *nearest pedal* one step toward it, so the harp
    gradually re-pedals to follow the music. Subject to the governor.

- **Note-off pairing** — like ONF: a note-on entered `activeNotes` with its
  emitted pitch; the matching note-off releases that pitch. FIFO by
  (channel, input note). Consumed control-zone black keys emit no note and their
  offs are swallowed.

- **Same-string collision** — two live notes mapping to the same string pitch
  (e.g. white E and white F when `E♯` doubles F): **drop the second** (the one
  that arrived later). Playable-part priority; a section harpist has one string.

- **Held notes across a pedal change** — a note already sounding keeps its
  pitch; the new diagram applies from the next note-on. (A real string doesn't
  re-pitch mid-ring without a buzz.)

---

## 5. The pedal diagram: parameters + bank

### The live diagram = 7 automatable params

`pedalC pedalD pedalE pedalF pedalG pedalA pedalB` — each an
`AudioParameterChoice { "Flat", "Natural", "Sharp" }`. **These are the
target/requested diagram.** Automate them individually from Bitwig — draw a
pedal-automation lane per string. The governor (§6) paces how fast the *sounding*
diagram follows; it never writes back to the params.

### The bank

`bankSlot` — `AudioParameterInt 0..N-1` (N = 12 to start). Automatable: automate
diagram changes by automating the slot. Changing `bankSlot` (by param, CC49, or
black-key control) **writes the 7 pedal params** for that slot
(`setValueNotifyingHost`, guarded — only on real change), same pattern as ONF's
preset → 12-toggle write.

Each slot stores `{ 7 pedal offsets, name, colour }`. Slots persist in
`getStateInformation` (a ValueTree child, not automatable — it's a preset store).
Editor: **Shift-click a slot to save** the current diagram into it; right-click
to rename / recolour. Empty slot = C major.

### Family / variant / base-key — a bank-fill helper, not a live panel

A small editor sub-panel: pick **Family** (Major/Minor · 7-Chord · Whole Tone ·
Pentatonic), **Variant**, **Base Key** → `familyVariantKeyToDiagram` → "Write to
slot ___". Major/Minor is clean 7-note math; 7-Chord / Whole Tone / Pentatonic
are < 7 letters and get best-fit doubling. Factory bank ships pre-filled:
major, natural/melodic/harmonic minor, whole tone, major/minor pentatonic,
octatonic (both), quartal, and a couple of atonal hexachord sets — the "universal
glissando" starting points; the user builds exotic subsets in the custom editor.

---

## 6. Playability governor

`playability` param — **On** (default) / **Off**.

- **Off** = the sounding diagram tracks the requested diagram instantly (Synchron
  Harp chaos — kept deliberately; part of what the user likes for fast, wild
  re-pedalling).
- **On** = the sounding diagram is only allowed to change as fast as a section
  harpist could pedal it.

### The foot rules

- **Left foot**: B, C, D pedals. **Right foot**: E, F, G, A pedals.
- One foot moves one pedal per "move". So a single move = **≤1 change among
  {B,C,D}** and **≤1 change among {E,F,G,A}** — never two from the same foot.
- A ♭→♯ change on one pedal is **one move** (the foot passes the middle notch in
  one motion).

### Realising an unreachable target

`playablePedalStep(current, target)` returns the next intermediate diagram one
move closer to `target`: pick the most "useful" left-foot change and the most
useful right-foot change (heuristic — favour pedals whose string is about to be
played; fall back to any differing pedal). Apply, record the move's ppq, repeat
on later ticks until `sounding == target`. A **new target arriving mid-transition
just re-aims** from the current sounding diagram.

### Timing

- `minChangeInterval` param — `AudioParameterChoice { "1/8", "1/4", "1/2", "1 bar", "2 bars" }`
  (musical, off the playhead ppq). The governor won't emit the next move until
  that much has elapsed since the last.
- `changesAtRestsOnly` param — bool. When on, a move is held until no note is
  sounding on the track, then applied. Overrides the interval for slow / exposed
  passages.
- `avoidRingingPedalChange` param — bool, off by default. When on, a pedal whose
  string sounded within the last `minChangeInterval` is skipped for this move
  (buzz avoidance for realistic rendering; irrelevant for the score).

### The traps this encodes (orchestral-harp forum wisdom)

3+ pedals at once → impossible (spread); two same-foot at once → impossible
(sequence); re-pedalling faster than the passage allows → unplayable (interval /
rests-only); double changes are harder than single (the heuristic prefers one
change per move when it can). This is an **orchestral section harp**, not a
virtuoso-competition part.

---

## 7. Triggers

CC-first (this is a CC-driven rig), with a keyboard path for the emergent trick
and manual play.

### CC49 — the reserved Harp slot

`ccBankSelect` param — `AudioParameterInt 0..127`, default **49**. An incoming CC
on that number selects a bank slot: `slot = round(value / 127 * (N-1))`. So
OrchConductor / MC's narrative arc re-pedals the harp along the piece
(`Combi preset` / Narrative Scan → CC49 → diagram). `0` on the param disables the
CC path.

### Black-key control zone — **both** direct and step

Different automation styles want different addressing, so both, on separate
ranges:

- **Direct select** — `ctrlDirectLo` / `ctrlDirectHi` params (Int note numbers,
  default a 12-note range e.g. C-1..B-1). A black key in range →
  `slot = note - ctrlDirectLo`. Good for "the sonata's accidentals jump between
  named diagrams."
- **Step** — `ctrlStepDownNote` / `ctrlStepUpNote` params (Int, default e.g.
  A♯-2 / C♯-1). One note → `bankSlot -= 1` / `+= 1` (clamped). Good for LFO /
  ramp automation walking through the bank, and for a stepper pedal.

Only active when `blackKeyMode == Control`. Notes consumed here emit nothing.

### UI

The 7 pedal controls, the bank grid, the family/variant helper, a live
harp-pedal-diagram readout showing **sounding vs requested** (pedals still in
transit highlighted), and the governor controls.

### MC broadcast of an exact diagram — noted, not Phase 1

ONF broadcasts its 12-pc mask over 2 CCs. OrchHarp could take a diagram (7 trits
≈ 12 bits) the same way over 2 CCs from `ccMaskBase`. Deferred; CC49→slot covers
the arc-driven case.

---

## 8. Automatable parameters (full list — Bitwig)

Everything the user touches while composing is an `AudioParameter`, in a
sensible remote-control order (8 per Bitwig page):

| # | ID | Type | Default | Notes |
|---|---|---|---|---|
| 1 | `mode` | Choice: Pedal / Chromatic | Pedal | master transform on/off |
| 2 | `pedalC` | Choice: Flat/Natural/Sharp | Natural | requested diagram |
| 3 | `pedalD` | Choice | Natural | |
| 4 | `pedalE` | Choice | Natural | |
| 5 | `pedalF` | Choice | Natural | |
| 6 | `pedalG` | Choice | Natural | |
| 7 | `pedalA` | Choice | Natural | |
| 8 | `pedalB` | Choice | Natural | |
| 9 | `bankSlot` | Int 0..11 | 0 | recall writes the 7 pedals |
| 10 | `blackKeyMode` | Choice: Control / Nearest / Drop / Nudge | Control | |
| 11 | `playability` | Bool | On | the governor |
| 12 | `minChangeInterval` | Choice: 1/8…2 bars | 1/4 | |
| 13 | `changesAtRestsOnly` | Bool | Off | |
| 14 | `avoidRingingPedalChange` | Bool | Off | |
| 15 | `ccBankSelect` | Int 0..127 | 49 | 0 = off |
| 16 | `ccChannel` | Int 0..16 | 0 (any) | |
| 17 | `ctrlDirectLo` | Int 0..127 | (C-1) | black-key direct-select range low |
| 18 | `ctrlDirectHi` | Int 0..127 | (B-1) | |
| 19 | `ctrlStepDownNote` | Int 0..127 | (A#-2) | |
| 20 | `ctrlStepUpNote` | Int 0..127 | (C#-1) | |

The bank contents (7 offsets + name + colour per slot) and the family/variant
helper selections are **state, not parameters** (preset store).

UI status readouts (atomics, message thread): last input → output note+string,
sounding diagram, requested diagram, moves-in-transit count, last CC.

---

## 9. Phase 2 — the generator + notation

### Contour-follower glissando — BUILT (Phase 2a, 2026-09-01)

`glissCc` param (Int, 0 = off). The CC value maps via `ohrp::mapContour` to an
**absolute string index** between `glissLoString` and `glissHiString`
(`glissLoString` may exceed `glissHiString` for a descending map);
`ohrp::stringIndexToNote` resolves it against the **sounding** diagram at the
`glissBaseOctave` (string index 0 = MIDI 24 by default). A note fires each time
that string index changes — a rising ramp → ascending run, an LFO → oscillating
runs, a hand-drawn curve → that shape, an MC ModulationRoute → arc-driven gliss.
Because the pitch is resolved live, re-pedalling mid-sweep recolours the run.
Velocity: `glissVelCc` (0 = use fixed `glissVelocity`). `glissRing`:
**Monophonic** (each new string note-offs the previous) / **Ring** (notes ring
and pile up; damped when `glissCc` returns to 0, on transport stop, or
All-Notes-Off). The `glissCc` / `glissVelCc` messages are consumed, not passed
downstream. Gliss notes are emitted on channel 1.

### Trigger-gesture glissando — BUILT (Phase 2a, 2026-09-01)

`glissTrigLo` / `glissTrigHi` note zone (0 / 0 = off). A note-on in the zone is
consumed and schedules a run. Start = `glissRunAnchor`: *Trigger Note*
(`noteToNearestStringIndex` of the trigger pitch, clamped to the window) /
*Low String* / *High String*. `span = glissRunSpan * (0.25 + 0.75·vel/127)`
strings, `glissRunDirection` (Up / Down / Up-Down / Down-Up), spread evenly over
`glissRunDuration` (1/16…1 bar, 4/4 assumed). Every index is **clamped to
`[min(glissLoString,glissHiString), max(...)]`** with consecutive duplicates
dropped, so a run stops at the configured string instead of flooring at MIDI 0.
Scheduled events carry a **string index**, resolved to a pitch at emission time,
so a pedal change mid-run recolours the tail. `glissBaseOctave` is 0..7.
Monophonic / Ring per `glissRing`. Scheduler = a ppq-sorted `pendingGliss`
vector drained each block.

### Pedal-change markers — Phase 2b, DEPRIORITISED (2026-09-01)

Original idea: on each *sounding*-diagram change emit a marker (text meta, or a
tagged note in a reserved range) that OrchCapture records onto a `<name> Pedals`
track → Dorico renders the pedal diagrams. Same pattern as OrchCapture's
`<name> KS` track; needs an OrchCapture-side addition (it captures note-on/off
only — no CC, no SysEx).

**User call (2026-09-01):** probably not needed for Dorico. If pedal diagrams are
wanted in the score they can be derived downstream in music21 from the note
stream (or a lightweight side file OrchHarp could dump), not carried through
OrchCapture. Revisit only if that proves painful. Not building it for now.

---

## 10. Phasing

| Phase | What |
|---|---|
| **1 — transformer** ✅ | String model, white-key → string, black-key modes, Chromatic bypass, 7 pedal params + bank + family/variant helper, the playability governor, CC49 + black-key (direct + step) triggers, live diagram readout. Pure `OrchHarpPedalLogic` + `OrchHarpPedalLogicCheck`. Factory bank. Built + live-tested 2026-09-01. |
| **2a — glissando engines** ✅ | Contour-follower gliss + trigger-gesture gliss. Built 2026-09-01, not yet live-tested. |
| ~~**2b — notation**~~ | Pedal-change markers → OrchCapture. **Deprioritised** — user will derive pedal diagrams downstream in music21 if needed, not through OrchCapture. |
| **later** | MC 2-CC diagram broadcast; harmonics / près-de-la-table as an articulation hint (probably OrchNoteMapper's job, not here). |

## 11. Build

CMake mirrors OrchNoteFilter / OrchCapture: `add_subdirectory("C:/JUCE/JUCE" JUCE)`,
`juce_add_plugin` VST3 + `IS_MIDI_EFFECT`, `COPY_PLUGIN_AFTER_BUILD FALSE` (copy
by hand or flip once elevated), a `juce_add_console_app` check target for
`OrchHarpPedalLogic`. Files:
`Source/OrchHarpProcessor.{h,cpp}`, `Source/OrchHarpEditor.{h,cpp}`,
`Source/OrchHarpPedalLogic.{h,cpp}`, `Tools/OrchHarpPedalLogicCheck.cpp`.

## 12. Open items for the build thread

Phase 1 resolutions (2026-09-01):

- **Black-key control note defaults** — direct-select `ctrlDirectLo/Hi` = MIDI
  0..11 (C-1..B-1), `ctrlStepDownNote/UpNote` = MIDI 12 / 13. All below any
  folded harp playing range.
- **Family/variant → diagram** — no per-family tables. `bestFitDiagram(pcSet)`
  brute-forces all 3^7 pedal assignments and scores: uncovered set member
  (worst) → string sounding outside the set → least pedal effort → best-fit
  distance. Exact for 7-note scales, enharmonic doubling for smaller sets.
  `familyVariantKeyToDiagram` and the factory bank both go through it.
- **Governor heuristic** — simple: per foot, step the first disagreeing pedal
  that isn't the one that foot moved last (spreads the work), one notch toward
  target. "Pedal whose string is imminent" weighting deferred until tuned
  against a real sonata.
- **♭→♯ is two moves** in this build (one notch per move), not one — slightly
  conservative vs §6. TODO: allow the full swing as one move once tuned live.
- **4/4 assumed** for `minChangeInterval` beat math (1/8=0.5 … 2 bars=8 beats).
  Time-signature-aware version is a later refinement.

Still open:

- `minChangeInterval` plain-ms option for free-tempo / no-transport use — while
  the transport is stopped the governor snaps sounding = requested each block
  (the "set pedals during the rest" model), so this only matters for a running
  free-tempo host.
- Governor imminent-string weighting (above).
