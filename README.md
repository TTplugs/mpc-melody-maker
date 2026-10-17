# TTplugs MPC Melody Maker

A generative melody, chord, and bassline engine designed specifically for **Akai MPC Standalone** hardware (MPC One, MPC Live, MPC Live II, MPC X, and MPC Key 61).

Developed by **TTplugs**, this plugin provides musical phrase generation, instant variations, and MIDI routing natively within the MPC standalone environment without relying on external computers or cloud services.

---

## Key Highlights

- **16-Pad Intuitive Matrix**: 4x4 matrix mapped directly to musical mood (Dark, Moody, Warm, Light) and complexity (Simple, Balanced, Detailed, Complex). Each pad touch generates a deterministic, musical 4-bar phrase.
- **341 Substyle Presets across 9 Genres**:
  - `General / Freestyle`
  - `Trap` (62 presets: *Abyss*, *Aftermath*, *Ambuscade*, *Armory*, ...)
  - `Hip-Hop` (38 presets: *Alleyway*, *Anvil*, *Asphalt*, *Backbeat*, ...)
  - `Pop` (44 presets: *Adoration*, *Affection*, *Airship*, *AllurePop*, ...)
  - `RnB` (38 presets: *Afterglow*, *Amaranth*, *Amour*, *Apricot*, ...)
  - `Latin` (34 presets: *Abanico*, *Acaricia*, *Alameda*, *Bolero*, ...)
  - `EDM` (51 presets: *Ablaze*, *Accelerator*, *Actuator*, *AuraBeam*, ...)
  - `Techno` (41 presets: *Abrasive*, *Acetylene*, *Adhesion*, *Aggregate*, ...)
  - `House` (32 presets: *Afterparty*, *AlleywayHouse*, *Aperitivo*, *Beachclub*, ...)
- **Dynamic Dual-Level Selector UI**:
  - **Style (Genre)**: Top-bar LCD dropdown selector providing instant switching between musical genres, touch-overlay selection, and hardware Q-Link assignment.
  - **Substyle (Preset)**: Continuous parameter knob and Q-Link control cycling through all 341 unique musical personality presets.
- **Three-Part Harmony**: Simultaneous generation of monophonic Lead Melody, 3-to-4-note Diatonic Chords, and Root/Motif Basslines, with independent MIDI channel routing (default channels 2, 3, 4) and ALSA sequencer destinations.
- **Advanced Harmonic Intelligence**:
  - 22 musical scales (Major, Minor, Pentatonics, Blues, Dorian, Phrygian, Lydian, Mixolydian, Melodic/Harmonic Minor, Hungarian, Egyptian, etc.).
  - Diatonic Voice Leading and stable pitch-class masks for both Light (Major) and Dark (Minor) contexts.
  - Contour preservation and harmonic anticipation rules ensuring smooth voice movement across chord changes.
- **Real-Time Variations & Non-Destructive MIDI FX**:
  - Light/Dark, Rhythm, and Masked variation operators with adjustable intensity and candidate selection (8–32 candidates).
  - 12 real-time reversible MIDI FX inserts (Swing, Gate, Humanize, Velocity, Repeat, Arp Up, Arp Down, Strum, Probability, Accent) with per-part targeting.
- **Bank & Session History**:
  - 128 creation slots with favorites tagging and scrollable recall bank.
  - 8-step session undo/redo history per pad with state preservation across MPC project saves.
- **Standard MIDI Export**: One-tap export to standard `.mid` files (individual parts or combined stems) ready to drag into the MPC sequencer tracks.

---

## System Architecture

```
                    ┌────────────────────────────────────────┐
                    │      Akai MPC Standalone Touch UI      │
                    │  (CREATE, VARIATIONS, MIDI FX, BANK)   │
                    └───────────────────┬────────────────────┘
                                        │
                                  [Q-Links / TUI]
                                        │
                                        ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                       TTplugs MPC Melody Maker (VST2)                       │
│                                                                             │
│  ┌────────────────────────┐  ┌─────────────────────┐  ┌──────────────────┐  │
│  │    Style & Substyle    │  │  Harmonic Context   │  │   Deterministic  │  │
│  │      Catalog DB        │  │     Engine (M5)     │  │   Input Queue    │  │
│  │ (9 Genres, 341 Presets)│  │ (Diatonic & Voice)  │  │ (Wait-free MPSC) │  │
│  └───────────┬────────────┘  └──────────┬──────────┘  └────────┬─────────┘  │
│              │                          │                      │            │
│              ▼                          ▼                      ▼            │
│  ┌───────────────────────────────────────────────────────────────────────┐  │
│  │            Generative Core & Contour Rules Engine (C11)               │  │
│  │  - Melodic Contour Preservation   - Voice Leading Diatonic Matrix     │  │
│  │  - Harmonic Anticipation          - Scale Snap (22 Scales)            │  │
│  └──────────────────────────────────────┬────────────────────────────────┘  │
│                                         │                                   │
│                                         ▼                                   │
│  ┌───────────────────────────────────────────────────────────────────────┐  │
│  │             Three-Part MIDI Output & ALSA Sequencer Engine            │  │
│  │        [Melody (Ch. 2)]    [Chords (Ch. 3)]    [Bass (Ch. 4)]         │  │
│  └───────────────────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## Directory Layout

```
.
├── Makefile                     # Build system for x86_64 host and ARM32 MPC target
├── include/
│   ├── mm_core.h                # Core request structures, scale definitions, generation API
│   ├── mm_creation.h            # Project serialization & creation file formats
│   ├── mm_crypto.h              # Deterministic binary decryption & validation
│   ├── mm_fx.h                  # Non-destructive MIDI FX processors (12 effects)
│   ├── mm_history.h             # 8-step per-pad non-volatile creation history
│   ├── mm_params.h              # VST parameter descriptors & enum definitions
│   ├── ttmm_phrase_rules.h      # Harmonic anticipation, voice leading & contour preservation
│   └── ttplugs_styles_db.h      # 9 genres and 341 substyle presets database
├── src/
│   ├── mm_core.c                # Generation algorithm & phrase manipulation
│   ├── mm_creation.c            # Creation file load/save & favorites indexing
│   ├── mm_crypto.c              # Cryptographic verification & library loading
│   ├── mm_fx.c                  # Real-time MIDI FX chain implementation
│   ├── mm_vst.c                 # VST2 plugin entrypoint, parameter binding & worker thread
│   └── engine/
│       ├── mm_candidate_engine.c# M5/M3 variation queue and candidate selection
│       ├── mm_harmonic_context.h# Active chord & harmonic context tracker
│       └── mm_input_queue.h     # Wait-free MPSC ring buffer for MPC touch events
├── skin/
│   └── params.json              # MPC GUI parameter bindings and display metadata
└── tools/
    ├── gen_parameters.py        # Generates mm_params.h and skin parameters
    ├── check_public.py          # Security audit rejecting private assets before commit
    └── mm_cli.c                 # Standalone command-line phrase generator
```

---

## Building from Source

### Prerequisites

- **Host Compiler**: GCC / Clang with C11 support.
- **Cross-Compiler for MPC Standalone**: `arm-linux-gnueabihf-gcc` (ARM32 hard-float).
- **Python**: 3.10+ with `Pillow` (for skin rendering tools).
- **Linux Environment / WSL2** (on Windows).

### Compilation

```bash
# 1. Generate parameter tables
python3 tools/gen_parameters.py

# 2. Build x86_64 test host VST
make build/mpc_melody_maker-host.so

# 3. Build native ARM32 plugin for Akai MPC One / Live / X
make build/mpc_melody_maker.so
```

The resulting ARM binary (`build/mpc_melody_maker.so`) is compiled with `-fvisibility=hidden -fdata-sections -ffunction-sections -Wl,--gc-sections -Wl,--no-undefined` to ensure optimal performance, minimum binary size (~430 KB), and complete zero-overhead runtime execution on the MPC ARM Cortex processor.

---

## Deployment to Akai MPC Hardware

1. Connect your Akai MPC to your local network via Ethernet or Wi-Fi.
2. Enable SSH access (or use standard MPC expansion tools).
3. Copy the compiled plugin and skin to the MPC Expansions directory:

```bash
MPC_IP="10.10.20.131"
TARGET_DIR="/media/acvs-content/Expansions/TTplugs - VST - MPC Melody Maker"

# Copy ARM plugin binary
scp build/mpc_melody_maker.so "root@${MPC_IP}:${TARGET_DIR}/mpc_melody_maker.so"

# Copy Skin definitions
scp skin/TUI.json skin/Q-Links.json "root@${MPC_IP}:${TARGET_DIR}/Plugin Skins/"
```

4. Launch the MPC software or restart `MPC` (`killall -9 MPC` or reboot).
5. Load **MPC Melody Maker** from your VST Plugin browser under the **TTplugs** vendor category.

---

## Parameter & Q-Link Layout

On the primary **CREATE** page, hardware controls map directly to:

| Q-Link | Parameter | Range | Description |
|:------:|:---------:|:-----:|:------------|
| **Q-Link 13** | `KEY` | C .. B | Root note of the generation key |
| **Q-Link 9** | `SCALE` | 0 .. 21 | Selected scale (Major, Minor, Pentatonics, Modes, etc.) |
| **Q-Link 5** | `SPEED` | Slow / Med / Fast | Note division and rhythmic density |
| **Q-Link 1** | `BARS` | 1 .. 8 | Pattern cycle length |
| **Q-Link 14** | `OCTAVE` | 2 .. 6 | Center pitch octave for the phrase |
| **Q-Link 10** | `SUBSTYLE` | 0 .. 341 | Specific substyle preset (or *All*) |
| **Q-Link 11** | `STYLE` | 0 .. 8 | Genre filter (*All*, *Trap*, *Hip-Hop*, *Pop*, *RnB*, *Latin*, *EDM*, *Techno*, *House*) |
| **Q-Link 2** | `POWER` | Off / On | Enable/mute output playback |

---

## Private Musical Libraries Notice

This repository contains the **open-source engine, DSP, VST interface, and UI specifications**. It contains no copyrighted source MIDI packages, private commercial phrase databases, or proprietary activation keys.

Users wishing to bundle their own custom phrase banks can compile `.ttbank` or `.mlib` files using the included command-line utilities.

---

## License

Copyright © 2024–2026 TTplugs. Distributed under the MIT License. See [LICENSE](LICENSE) for details.
