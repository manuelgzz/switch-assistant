# Changelog

This changelog documents changes introduced in the
manuelgzz/switch-assistant fork.

The original project and its previous history are maintained.

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

## Original Project

Upstream repository:
https://github.com/ErSeraph/switch-assistant

Fork repository:
https://github.com/manuelgzz/switch-assistant
