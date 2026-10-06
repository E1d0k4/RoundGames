# RoundGames System UI

The system UI is intentionally hierarchical. The 466x466 display must never be overloaded with every setting at once.

## Navigation

- Swipe from the top edge downward: open the system Quick Controls.
- Quick Controls shows categories only, with a short current value.
- Tapping a category opens a dedicated full-screen settings page.
- Every settings page has a large Back action.
- System gestures have priority over games; a game does not receive the swipe-down gesture.

## Quick Controls

The first-level control page contains:

1. Brightness
2. Sound
3. Dimming
4. Language
5. Clock & Date
6. Screensaver
7. Theme
8. Information

Each entry is a large touch target and shows only a compact status such as "70%" or "Deutsch".

## Dedicated settings pages

### Brightness
- Slider and/or large +/- controls.
- Persist normal brightness.
- Apply changes immediately.

### Sound
- Volume slider.
- Large mute/unmute action.
- Persist volume and mute state.
- Keep audio implementation behind a shared service so games do not access codec hardware directly.

### Dimming
- Enable/disable automatic dimming.
- Select inactivity timeout.
- Select the brightness level used while dimmed.
- Dimming brightness is independent of normal brightness.

### Language
- Language is a system setting, not a game setting.
- UI strings must come from the localization service.
- Initial language set: German, English, Dutch.
- Adding a language must not require changing game logic.

### Clock & Date
- Set time and date.
- 12/24 hour preference.
- Optional network time synchronization.
- RTC access is owned by the platform service.

### Screensaver
- Enable/disable.
- Select inactivity timeout.
- Select a clock/screensaver theme.
- Screensaver starts after inactivity and can restore the previous application without resetting game state.

### Theme
- Shared visual theme selection.
- Games consume theme tokens instead of hard-coding system colors and dimensions where practical.

### Information
Display useful diagnostic/version information without requiring a connection:
- RoundGames version
- Firmware/build version
- Git commit
- Board target
- ESP-IDF version
- LVGL version
- Flash size
- PSRAM size
- Installed game versions

## Input priority

Input is processed in this order:

1. Emergency/system controls where applicable.
2. Swipe-down system gesture.
3. Screensaver wake/exit.
4. System UI controls.
5. Active game input.

This prevents a game from accidentally consuming the reserved system gesture.

## Persistence

User preferences are stored through a central Settings Service backed by NVS. UI code must not write NVS directly.

Settings keys should be versioned/migratable so future firmware can add or rename preferences safely.

## Suggested directory structure

```
firmware/
  components/
    system_ui/
    settings/
    input/
    localization/
    rtc_service/
    audio_service/
    display_service/
    screensaver/
    theme/
    version/
  games/
    tictactoe/
  main/
```

Each service should expose a small interface and hide hardware-specific details.
