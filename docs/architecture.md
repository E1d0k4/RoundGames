# RoundGames Architecture

This document distinguishes the **current implementation** from the intended direction. It must not describe planned behavior as if it were already implemented.

## Project target

RoundGames is a multi-game firmware for the Waveshare ESP32-S3-Touch-AMOLED-1.75, using ESP-IDF, C/C++, and LVGL. The repository's CI currently builds with ESP-IDF 5.5.5 and targets ESP32-S3.

Board variants must not be assumed interchangeable without checking display, touch, power-management, and peripheral compatibility.

## Current implementation

The current firmware code is primarily located in `firmware/main/`, not in separate game directories.

### Shared firmware modules

- `main.c`: application startup, launcher/settings UI and system-level coordination
- `display.c/.h`: display integration
- `input.c/.h`: touch, gestures and game/system input routing
- `launcher.c/.h`: paged game launcher
- `game_manager.c/.h`: game start/stop lifecycle dispatch
- `settings.c/.h`: persisted settings
- `language.c/.h`: language selection and translation helpers
- `theme.c/.h`: theme selection
- `screensaver.c/.h`: inactivity/screensaver behavior
- `clock.c/.h`: clock support
- `audio.c/.h`: audio controls/integration
- `power_button.c/.h`: power-button behavior
- `settings_ui.c/.h`: reusable settings UI helpers

### Current game implementations

- Tic-Tac-Toe: `tic_tac_toe.c/.h`
- Snake: `snake.c/.h`
- Vegg: `vegg_game.cpp/.h`, with `vegg_sprites.h`
- Orbit Breaker: `orbit_breaker.cpp/.h`

The game manager currently dispatches these four games. The launcher also contains placeholder labels for additional slots; a visible placeholder is not an implemented game.

The folders under `firmware/components/` currently contain component-level README files, not all of the corresponding service implementations as independent ESP-IDF components. Likewise, `firmware/games/tictactoe/` is currently only a placeholder directory. The repository should be reorganized incrementally rather than pretending the proposed structure already exists.

## Current game lifecycle

Games are selected and started by `game_manager.c`. The manager calls the relevant game's open/stop functions and coordinates the active-game state with input handling and the screensaver. New games should use this shared lifecycle rather than independently initializing the board or taking over the display driver.

Before adding a game, inspect the existing game headers and manager. Add the source to `firmware/main/CMakeLists.txt`, add an explicit game ID and start/stop handling, and update launcher labels/count consistently. Do not add a launcher label alone and describe it as a working game.

## Intended architecture

The target architecture is one firmware image containing:

1. Board-specific display, touch, power and peripheral integration.
2. Shared services for input, settings, localization, clock, audio, theme and screensaver.
3. A launcher and game manager.
4. Independent games that use shared lifecycle and input conventions.

A later refactor may move services and game sources into separate ESP-IDF components/directories. Such a move should be incremental and build-tested; it is not a prerequisite for every new game.

## Input and system UI

The intended interaction model reserves the top-edge swipe-down gesture for system controls. System gestures should be handled centrally and should not be consumed accidentally by games. The power button has separate short-press and long-press behavior defined in the firmware.

The detailed intended interaction specification is in [System UI](system-ui.md). That document is a design reference; verify the current implementation before assuming every listed control or option is complete.

## Settings and persistence

Platform preferences should be read and written through the shared settings layer, not by each game independently. When adding settings, keep default values, bounds checking, persistence, and UI updates consistent. Any changes to persisted settings should consider existing devices that already have saved values.

## Build and web installer

- Firmware project: `firmware/`
- ESP-IDF component registration: `firmware/main/CMakeLists.txt`
- Default target/configuration: `firmware/sdkconfig.defaults`
- Browser installer: `web-installer/index.html`
- Installer manifest: `web-installer/manifest.json`
- Build/deployment automation: `.github/workflows/`

The main build workflow compiles the ESP32-S3 firmware, creates a merged binary, prepares versioned installer metadata, deploys the Pages artifact, and checks that the published manifest and binary match the commit being built. Always inspect the actual workflow run and deployed installer after changes; a source commit alone does not establish that a flashable build is available.

## Development and release rules

- Keep the existing RoundGames firmware and system behavior as the foundation.
- Preserve original game rules, artwork, animation timing and intended controls where possible.
- Keep hardware-specific work in shared platform code.
- Prefer original code and assets. Review and preserve notices for any deliberately added third-party material.
- Verify compile/build success before calling a change firmware-ready.
- For installer releases, verify the published manifest and binary correspond to the intended commit.
- Test on the physical device for rendering, touch, timing, power behavior and other hardware-dependent behavior; compile success cannot prove these.

## Future work (not a claim of current implementation)

- Move game sources into independent directories or components without breaking builds.
- Add automated host-side tests for game logic where practical.
- Keep launcher metadata synchronized with registered games instead of maintaining duplicate lists.
- Improve version/build information and release verification.
- Consider a PC simulator only as a separate, explicitly scoped project; it is not currently part of this repository's firmware.
