# Settings Service

Central persistent settings model for RoundGames.

Responsibilities:
- hold validated runtime settings;
- load/save settings through ESP-IDF NVS;
- provide defaults for a fresh device;
- migrate settings when the settings schema changes.

Initial settings:
- brightness: 0..100
- volume: 0..100
- muted: boolean
- dimming_enabled: boolean
- dimming_timeout_minutes
- dimming_brightness: 0..100
- language: de/en/nl
- clock_24h: boolean
- ntp_sync_enabled: boolean
- screensaver_enabled: boolean
- screensaver_timeout_minutes
- screensaver_theme
- system_theme

UI and games must use the Settings Service rather than accessing NVS directly.
