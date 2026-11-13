# Paffetti Dub Chords

Sampler di stab dub techno (VST3 / AU, macOS) – powered by Black Loops.

- `Source/` – codice del plugin (JUCE 8): processore, voce con DUB FILTER, effetti (`DubFX.h`), interfaccia retro.
- `Samples/` – libreria (FLAC + `library.json`); `tools/make_pack.py` la impacchetta in `Samples.pdcpak`.
- `Resources/presets.json` – 90 preset, generati da `tools/make_presets.py` (volumi da `tools/preset_levels.json`).
- `installer/` – installer .pkg (VST3, AU e libreria in /Library/Application Support/Black Loops).
- `.github/workflows/` – compilazione universale per Mac, test auval/pluginval, installer e prova d'installazione.
- `Tests/` – banco di prova (Linux) usato per le verifiche.

Compilare: `cmake -B build -DJUCE_DIR=/percorso/JUCE && cmake --build build --config Release`
