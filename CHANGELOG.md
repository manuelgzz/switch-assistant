# Changelog

This changelog documents the original Switch Assistant releases
and the additional changes introduced in the manuelgzz fork.

## [v1.12.0] - Opt-in RTSP Control

### Changed
- RTSP screen streaming is disabled by default to avoid starting the RTSP
  capture subsystem on every console boot.
- Added **System > RTSP (risky)** in Switch Assistant to explicitly enable or
  disable RTSP.
- The setting is saved as `rtsp_enabled` in `config.ini` and requires a
  console reboot to apply.

### Fixed
- Prevented the RTSP capture subsystem from starting when it is not in use,
  avoiding the instability observed while launching games on affected consoles.

### Documentation
- Added `docs/rtsp-investigation.md` with the investigation evidence, test
  procedure, limitations, and follow-up plan.

## [v1.11.0] - SD Card Storage Sensors

### Added
- MQTT sensor for available SD card storage (`sd_free_space`).
- MQTT sensor for used SD card storage (`sd_used_space`).
- Home Assistant MQTT Discovery for both storage sensors.
- Storage values reported in GB with two decimal places.

## [v1.10.0] - Operation Mode Sensor

### Added
- MQTT sensor `operation_mode`.
- Nintendo Switch operating states:
  - Docked
  - Handheld
  - Sleep
- Home Assistant MQTT Discovery for the operation mode sensor.
- MQTT Last Will to publish `Sleep` on unexpected disconnection.

### Notes
- MQTT disconnections may also cause the operation mode to report
  `Sleep`, even when the console is not actually suspended.

## [v1.9.0] - Docked Mode Detection Fix

### Fixed
- Improved Nintendo Switch docked mode detection.
- Adjusted power-state monitoring and sleep handling.


## v1.8

- Add a compact binary title index to resolve game names without loading the full title database into sysmodule memory, fixing the game title detection (hopefully).

## v1.7

- Update the bundled title database.

## v1.6

- Improve game detection and sleep state publishing.

## v1.5

- Add a configurable Notifications option to disable the notifications if a custom Atmosphere isn't compatible with.

## v1.4

- Add live screen streaming to Home Assistant via RTSP.
- Publish a new MQTT entity with the console's RTSP stream URL for easy camera setup.

## v1.3

- Fix saved video recordings showing a green screen by excluding the notification overlay from the recording layer stack.
- Resolve the current game name from a bundled title database.
- Install the bundled title database alongside the Switch Assistant config files.

## v1.2

- Fix Switch Assistant title IDs to avoid collisions with existing homebrew sysmodules.
- Safely clean up old Switch Assistant installs only when identified by project markers.
- Improve notification text handling for Unicode payloads.
- Prevent old notifications from being shown again on boot.
- Add a configurable Boot Delay option for custom Atmosphere packs that crash when boot2 sysmodules start system services too early.

## v1.1

- Publish active game title ID and app running state sensors.
- Add Home Assistant popup notifications.

## v1.0

- Initial release.
