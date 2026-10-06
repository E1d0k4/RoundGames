# Display Service

Shared display abstraction.

Responsibilities:
- initialize and control the CO5300/LVGL display stack;
- set/get normal brightness;
- set dimmed brightness;
- restore normal brightness;
- expose display capabilities to the system UI.

Games and screensavers should not manipulate board-specific display hardware directly.
