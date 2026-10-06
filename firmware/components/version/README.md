# Version Service

Provides version/build information for the Information page.

Expose:
- RoundGames release version;
- firmware version;
- Git commit;
- build date/time;
- board target;
- ESP-IDF version;
- LVGL version;
- Flash and PSRAM information;
- registered game IDs and versions.

Game metadata should be registered through the Game Registry rather than hard-coded in the Information screen.
