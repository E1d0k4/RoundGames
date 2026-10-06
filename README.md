# RoundGames

RoundGames is a modular game platform for the Waveshare ESP32-S3-Touch-AMOLED-1.75.

## Target hardware

- ESP32-S3R8
- 466 × 466 CO5300 QSPI AMOLED
- CST9217 capacitive touch
- 16 MB Flash
- 8 MB PSRAM
- Optional microSD
- Wi-Fi / Bluetooth

The project targets the official Waveshare board support and is designed as a multi-game launcher rather than a single-game firmware.

## First game

### Tic-Tac-Toe

The first game will provide:

- 1 player
- 2 players
- multiple game modes
- touch-first controls
- local score/state handling
- clean return to the RoundGames launcher

The game lives in its own directory so future games can be added without restructuring the project.

## Project structure

```text
RoundGames/
├── firmware/
│   ├── main/                  # RoundGames firmware / launcher
│   ├── components/            # Shared platform components
│   └── games/
│       └── tictactoe/         # Tic-Tac-Toe only
├── web-installer/             # Browser-based USB installer
├── docs/                      # Architecture and development docs
├── test/                      # Host/device tests
└── .github/
    └── workflows/             # Build, test and release automation
```

## Development direction

RoundGames is being built in layers:

1. Hardware abstraction / board bring-up
2. Touch + display UI framework
3. RoundGames launcher
4. Game interface/API
5. Tic-Tac-Toe
6. Web installer + release pipeline
7. Additional games

## Installer

The repository contains the web-installer from the beginning. The installer is intended to use Web Serial / ESP Web Tools and GitHub Releases as the firmware distribution point.

The production installer will be hosted through GitHub Pages over HTTPS.

## Status

Early architecture / foundation.
