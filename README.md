# RoundGames

RoundGames is a multi-game firmware project for the **Waveshare ESP32-S3-Touch-AMOLED-1.75**. It is built with **ESP-IDF, C/C++, and LVGL 9** and is intended to provide one launcher, shared device functions, and multiple games on the same device.

## Target hardware

- ESP32-S3R8
- 466 × 466 CO5300 QSPI AMOLED display
- CST9217 capacitive touch
- 16 MB flash and 8 MB PSRAM on the target board
- Board power management and available audio features through the supported board interfaces

The exact board variant and its peripherals matter; do not assume that another 1.75-inch variant is interchangeable without checking its hardware support.

## Current games

The firmware currently contains these game implementations:

- **Tic-Tac-Toe**
- **Snake**
- **Vegg**
- **Orbit Breaker**

The launcher and game manager are currently implemented in `firmware/main/`. Some older planning documents describe a future per-game directory layout; the current source tree has not yet been fully reorganized to match that proposal.

## Device features

The firmware source includes shared code for:

- Launcher with game pages
- Touch and system/game input handling
- Settings persistence
- Language selection
- Themes
- Clock/date support
- Screensaver and dimming behavior
- Display brightness and power-button behavior
- Audio controls and device information

Feature availability and behavior should be confirmed against the current firmware build and on the physical device. Documentation of intended behavior is not a guarantee that every feature is fully implemented or hardware-verified.

## Repository layout

```text
RoundGames/
├── firmware/
│   ├── main/                  # Firmware, launcher, game manager and current games
│   ├── components/            # Component documentation / shared service areas
│   ├── games/tictactoe/       # Reserved game directory (currently placeholder)
│   ├── CMakeLists.txt         # ESP-IDF project configuration
│   ├── sdkconfig.defaults
│   └── partitions.csv
├── web-installer/
│   ├── index.html             # Browser-based installer page
│   └── manifest.json          # Installer manifest; CI updates release build metadata
├── docs/
│   ├── architecture.md
│   └── system-ui.md
└── .github/workflows/
    ├── build-firmware.yml
    └── deploy-installer.yml
```

## Build and toolchain

The canonical firmware toolchain is **ESP-IDF** (the CI workflow currently uses ESP-IDF 5.5.5) with the ESP32-S3 target. LVGL is used for the display UI. Arduino is not a separate language and is not the project's primary framework.

The automated workflow builds the firmware and prepares a combined binary for the browser installer. A successful source change or commit alone does **not** prove that the resulting firmware has built, deployed, or is being served by the installer; check the workflow and published manifest/binary before flashing.

For board setup and current ESP-IDF instructions, see the [ESP-IDF documentation for ESP32-S3](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/).

## Web installer

The installer page is in `web-installer/index.html`. The intended workflow uses a supported Chromium-based browser and Web Serial/ESP Web Tools to flash the firmware over USB. The installer uses GitHub Pages for hosting and the build workflow to publish firmware metadata and binaries.

Do not flash a build marked as ready until the matching CI build and deployed installer manifest/binary have been verified.

## Development principles

- Keep the existing RoundGames firmware and its device behavior as the foundation.
- Integrate new games through the shared game manager and input/lifecycle conventions.
- Preserve each game's graphics, animation timing, and intended controls as closely as practical.
- Keep hardware access in shared platform code rather than duplicating it in individual games.
- Prefer original assets and code. When third-party code or assets are intentionally introduced, review their licenses and retain the required notices.
- Test game behavior on the physical device; a successful compile alone cannot verify frame pacing, touch behavior, or visual fidelity.

## Documentation

- [Architecture](docs/architecture.md)
- [System UI and interaction model](docs/system-ui.md)

## Project status

RoundGames is under active development. The repository contains working firmware and several game implementations, but the architecture, documentation, installer/release process, and game organization continue to evolve. Check the current source and CI status for the actual state rather than relying on older planning descriptions.
