# Localization Service

All user-visible system strings must go through localization.

Initial languages:
- de — Deutsch
- en — English
- nl — Nederlands

Rules:
- no hard-coded user-facing strings in reusable system components;
- game strings use the same localization mechanism;
- missing translations fall back to English;
- language selection is persisted by the Settings Service.
