# Notes for Claude

- INDIKAT products are **VST plugins** (JUCE 8, VST3 + AU + Standalone), not web pages.
- Use the same CI as `coop030/indikat-bass`: `.github/workflows/build.yml` builds Windows / macOS (universal,
  ad-hoc signed) / Linux and publishes a GitHub Release per push, with the macOS `INDIKAT installieren.command`.
- Look: shared `IndikatLookAndFeel` / `IndikatFonts` (Michroma, Questrial, Share Tech Mono), palette in `PluginEditor.cpp`,
  fixed base size scaled by the editor.
- Before pushing: build with `-DINDIKAT_TESTS=ON`, run `IndikatRenderTest` (all presets OK) and
  `IndikatRenderTest --snapshot ui.png` to check the layout.
- macOS install: `packaging/macos/install.sh` (curl one-liner, no quarantine). Point Mac users to it instead of browser downloads.
