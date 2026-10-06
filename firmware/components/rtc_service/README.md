# RTC Service

Shared time/date abstraction around the PCF85063 RTC.

Responsibilities:
- read/set date and time;
- provide 12/24-hour formatting preference through settings;
- optionally synchronize from NTP;
- hide RTC hardware details from system UI and games.
