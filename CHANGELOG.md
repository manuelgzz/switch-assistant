# Changelog

This changelog documents the original Switch Assistant releases
and the additional changes introduced in the manuelgzz fork.

## [v1.12.1] - Home Assistant Controls and Retained Telemetry

### Added
- Per-console MQTT command topics, brightness/backlight controls, volume controls,
  native mute with fallback, diagnostics and additional system/controller sensors.
- Direct running-application detection with Title ID and diagnostic reporting.
- Four configurable overlay quick actions published as Home Assistant events.
- Branch builds and downloadable GitHub Actions artifacts, using a pinned devkitPro image.
- Host regression tests for MQTT framing/discovery/commands, audio, game lookup,
  runtime firmware initialization, and title-index byte offsets.

### Fixed
- Initialize the Horizon firmware version before version-dependent service calls.
- Regenerate the title index from the exact catalog bytes, preserving line endings
  and recovering names from a stale installed index.
- Retain the last sensor readings across suspension and MQTT disconnections.
- Keep audio readback and controls on the same supported service route and use
  hardware-sized volume steps.

### Current Limitations
- Sleep is inferred from MQTT disconnection, which can take about 45 seconds;
  a network outage can also display Sleep. `Power state source` exposes this origin.
- Remote Sleep, forced Close Game and the Home Assistant RTSP toggle are removed.
- PSC power-management registration is removed following the reported suspend failure.
- Background game launching, Daybreak and automatic firmware/Atmosphere updates
  are not included in this release.

### Installation
- Replace `sdmc:/switch/switch-ha/switch-ha.nro`, open Switch Assistant to install
  the embedded components and title catalog, then fully reboot the console.
- Manual MQTT automations must use `switch_ha/<client_id>/command` instead of
  the former shared discovery-prefix command topic.

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
