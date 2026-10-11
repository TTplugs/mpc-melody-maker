# Integrated engine

The VST uses the improved Parts 1–8 engine through `src/engine/mm_candidate_engine.c`. The CLI remains a legacy bank utility.

## Runtime assets

Nine private files are required in `private/engine/` beside the VST:

- weighted.tables
- weighted.profiles
- theory5.mlib
- selection_weights.txt
- novelty.library
- sources.assets
- sources.catalog
- four_bar.mlib
- rhythm_bank.bin

`TTMM_CANDIDATE_DIR` can override the directory. These files must not be committed or included in a public release. They can contain recoverable musical material. The legacy encrypted-bank utility does not encrypt or protect this integrated asset bundle.

## Current behavior

Four-bar generation, 22 target scales, per-pad advancing seeds, three variation operators, candidate controls, session history and fixing. Native project format TTMMST04 retains seed counters and fixed states; older supported project versions remain readable. The complete session history is not serialized.

Phase 9 artwork is applied. Six tabs provide CREATE, VARIATIONS, paged MIDI FX, CREATIONS, EXPORT and SETTINGS. Each FX page proxies three positions in the twelve-slot chain; the underlying chain and creation format retain their original parameter positions.

Generation runs on a background thread explicitly assigned normal scheduling. Audio reads published phrases. Legacy bank loading is excluded when the integrated engine is enabled. UI component definitions and images not used by the new skin are pruned; scroll strips use compact frames.

The installed personal build is compiled for ARM. Manual routing, interaction, musical listening and resource-incident confirmation remain pending. Export writes SMF files, without automatic insertion into the host piano roll.
