# MPC Melody Maker

Hip-hop MIDI generator for Akai MPC standalone, by **TTplugs**. The integrated engine improves the original Parts 1–8 family-based generator. It does not replace it with Composition Next or a neural generator.

## Current implementation

- New four-bar bases on every pad touch, with automatically advancing per-pad seeds.
- Melody, chords and bass, separate enables, channels and named ALSA destinations.
- 22 target scales; slow, medium and fast corpus selection.
- Light/Dark, Rhythm and Masked variation operators; intensity, character and 8–32 candidates.
- Eight-entry session history per pad and pad fixing. Projects retain current phrases, seeds, counters and fixed states.
- 128 private creation slots, favorites and a scrollable bank.
- Twelve reversible MIDI effect inserts, presented three at a time with page navigation and per-part targeting.
- CREATE, VARIATIONS, MIDI FX, CREATIONS, EXPORT and SETTINGS, using the supplied physical skin artwork.
- MPC transport synchronization. POWER enables or silences playback.
- MIDI file export for melody, chords, bass or all parts. All writes a combined file and separate part files. **Automatic piano-roll insertion is not implemented.**

The implementation was compiled for x86 and ARM and installed for user evaluation. Compilation does not establish touchscreen compatibility, musical quality or resolution of the reported MPC resource incident. Manual QA remains pending.

## Private musical data

This repository contains code and UI resources only. It contains no source MIDI, processed phrase library, learned musical tables, activation key, creation bank or personal installation archive. Personal installation packages and MPC project chunks contain musical data and must not be published.

The integrated engine requires nine privately prepared files under `private/engine/` beside the VST; see [ENGINE-INTEGRATION.md](ENGINE-INTEGRATION.md). Those files and the private preparation pipeline are not distributed here. The historical importer and encryption tools are retained for the **legacy CLI** and do not prepare the integrated engine's assets. The new engine assets are not claimed to be protected for public distribution.

## Build

Requirements: C11 compiler, ARM hard-float cross compiler, Python 3.11+, Pillow and the [sd88me MPC VST framework](https://github.com/sd88me/mpc-vst-plugins).

```sh
python3 tools/gen_parameters.py
make build/mpc_melody_maker-host.so build/mpc_melody_maker.so
MPC_VST=/path/to/mpc-vst-plugins python3 tools/build_skin.py
```

Prepare a **personal** installation from your separately provided assets:

```sh
python3 tools/package.py --engine-dir /private/path/to/engine-assets
```

`TTMM_CANDIDATE_DIR` overrides the engine asset directory. `TTMM_EXPORT_DIR` and `TTMM_CREATIONS_DIR` override personal export and creation directories.

The CLI target `build/mm-generate` uses the historical bank format and is independent of the integrated VST engine.

## Usage and publication

See [the usage guide](docs/GUIA-DE-USO.md). MIDI channels default to 2, 3 and 4 to preserve the existing routing configuration. The current integrated generator fixes phrase length at four bars.

Before publishing, stage the intended source and run `python3 tools/check_public.py`. It examines Git index blobs, rejects known private assets and keys, and does not inspect or publish the personal runtime folders. `tools/publish-github.sh --public` creates the independent `TTplugs/mpc-melody-maker` repository from a clean committed source checkout authenticated as TTplugs.

## License

See [LICENSE](LICENSE). Musical libraries remain private and are not covered by this source distribution.
