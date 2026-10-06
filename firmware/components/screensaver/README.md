# Screensaver Service

Owns inactivity tracking and screensaver lifecycle.

Requirements:
- configurable inactivity timeout;
- configurable dimmed brightness;
- multiple screensaver themes;
- clock-based screensavers as the initial family;
- wake on touch/button;
- restore the previous screen/application without resetting game state.

The screensaver must use the Display/Brightness service rather than manipulating the display hardware directly.
