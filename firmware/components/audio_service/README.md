# Audio Service

Shared audio abstraction for RoundGames.

The service owns codec/audio hardware access and exposes:
- set volume;
- get volume;
- mute/unmute;
- get mute state.

Games should never access the audio codec directly.

Volume and mute state are persisted through the Settings Service.
