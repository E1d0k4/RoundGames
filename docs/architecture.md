# RoundGames Architecture

## Goal

RoundGames must behave like a small console:

- one firmware image
- one launcher
- multiple independent games
- shared display/touch/input services
- games isolated in their own directories
- new games added without rewriting the launcher

## Recommended stack

Use ESP-IDF with LVGL 9.

Reasoning:

- Waveshare currently validates ESP-IDF 5.5.5 and 6.0.2 for this board.
- The official board examples already cover the CO5300 AMOLED, CST9217 touch and LVGL.
- A component-based ESP-IDF layout maps well to a reusable game platform.

Arduino support can remain a secondary development option, but the main RoundGames firmware should have one canonical toolchain.

## Layers

### 1. Board layer

Responsible for:

- display initialization
- touch initialization
- power / brightness
- optional SD card
- optional audio
- board-specific GPIO/I2C/QSPI handling

No game should access raw board pins directly.

### 2. Platform layer

Shared services:

- screen manager
- touch/input manager
- navigation
- settings
- persistent storage
- game registry
- common UI widgets
- sound/haptics abstraction if added later

### 3. Launcher

The launcher displays installed/compiled games and starts a selected game.

Initial launcher:

- RoundGames title
- Tic-Tac-Toe tile
- future-game placeholders only in development, not on the production UI
- settings entry
- about/version entry

### 4. Game API

Every game follows the same lifecycle:

```text
init()
enter()
update()
draw()
handle_input()
pause()
exit()
deinit()
```

The exact C API will be finalized before implementing the first game so Tic-Tac-Toe becomes the reference implementation.

A game should not own the display driver or touch driver.

## Game directory rule

Every game gets its own directory:

```text
firmware/games/<game-id>/
```

Example:

```text
firmware/games/
├── tictactoe/
├── snake/
├── 2048/
└── breakout/
```

Each game contains its own:

- source
- game metadata
- assets
- tests
- README

Shared code belongs in platform/components, never duplicated into games.

## Tic-Tac-Toe modes

Initial design:

### Player count

- 1 Player vs AI
- 2 Players local

### Game modes

The first implementation should support:

- Classic 3×3
- Best-of-3 match
- Endless/local score mode

The mode system must be data-driven enough that additional modes can be added without rewriting touch handling.

### AI levels

For 1-player mode:

- Easy
- Normal
- Hard

Hard can use minimax because the 3×3 game state is tiny.

## Persistence

Persist only platform/game data that needs to survive reboot:

- settings
- selected preferences
- optional Tic-Tac-Toe statistics

Use a shared storage service so games do not manipulate NVS directly.

## Web installer

The installer consists of:

- `web-installer/index.html`
- `web-installer/manifest.json`
- GitHub Pages hosting
- GitHub Actions firmware build
- GitHub Release firmware assets

The browser installer must never require users to install Arduino IDE, ESP-IDF or drivers.

## Release flow

```text
git push
   ↓
GitHub Actions
   ↓
build + test
   ↓
firmware binaries
   ↓
GitHub Release
   ↓
web-installer manifest
   ↓
browser → USB → ESP32-S3
```

## Compatibility

The first target is:

- Waveshare ESP32-S3-Touch-AMOLED-1.75, standard version

Board variants such as 1.75-B and 1.75-G should not silently be treated as identical. Variant-specific support can be added later.


## System UI and interaction model

The system UI is intentionally hierarchical for the 466x466 display. The Quick Controls page contains categories and compact current values only; individual controls live on dedicated full-screen subpages.

### Reserved system gesture

A swipe from the top edge downward opens Quick Controls. This gesture is reserved by the platform and is dispatched before active-game input.

Input priority:

1. system/emergency controls where applicable
2. top-edge swipe-down
3. screensaver wake/exit
4. system UI controls
5. active game input

### Quick Controls

Quick Controls contains:

- Brightness
- Sound
- Dimming
- Language
- Clock & Date
- Screensaver
- Theme
- Information

Tapping a category opens its own page. For example, Brightness opens a page with a slider plus large +/- controls; Sound opens a volume page with a slider and mute/unmute action.

### Settings

Settings are owned by a central Settings Service and persisted through NVS. UI code and games never access NVS directly.

Initial settings include:

- normal brightness
- volume and mute
- dimming enabled, timeout and dimming brightness
- language
- 12/24-hour clock preference
- NTP synchronization
- screensaver enabled, timeout and theme
- system theme

### Localization

Initial platform languages:

- German
- English
- Dutch

All user-facing system strings go through localization. Missing translations fall back to English.

### Clock, dimming and screensaver

The PCF85063 is accessed through a shared RTC service. Optional NTP synchronization is handled by the platform.

Dimming has its own configurable brightness, independent from normal brightness. The screensaver starts after configurable inactivity and can use multiple clock/screensaver themes. Waking the screensaver restores the previous application without resetting game state.

### Information page

The Information page can show:

- RoundGames and firmware version
- Git commit and build date
- board target
- ESP-IDF and LVGL versions
- Flash and PSRAM information
- installed game IDs and versions

### Service boundaries

The intended platform components are:

- system_ui
- settings
- input
- localization
- rtc_service
- audio_service
- display_service
- screensaver
- theme
- version

Hardware access stays behind these services. This keeps individual games small and prevents settings, gestures or board-specific code from leaking into game implementations.
