# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]
- Protect start function from being executed when libpax is already started

## [1.1.0] - 2022-10-29
- Adapt Wifi country settings to ESP IDF v4.4 Wi-Fi API
- Keep backward compatibility to 1.0.1

## [1.0.1] - 2022-04-14
- Count randomized MACs only (Wifi and BLE)
- fix use LSB format to interprete BLE MACS
- fix potential stack crash in fill_counter()

## [1.0.0 RC2] - 2021-11-12
- Bug fixes
- Get rid of BLE stack and use low level VHCI to enhance memory usage and drop unused code
- Cumulated mode available (be careful with your config!)

## [0.1.3] - 2021-04-05
### Removed
- Removed LIBPAX_MAX_SIZE from API, since value is now fixed to 0xFFFF
- Redesgined hash table (now using a bitmap) to improve memory efficiency and lookup speed

## [0.1.2] - 2021-03-23
### Removed
- Removed COUNT_ENS code which is not working at this point

## [0.1.1] - 2021-03-22
### Updated
- Updated espressif32 platform to be at same version as in paxcounter project
- Updated hash table size default to be smaller to use less RAM
### Added
- Added unittest with toggle test for wifi ble
### Fixed
- Fixed setting timer handle to NULL for being able to reinit the lib

## [0.0.1] - 2020-12-07
### Added
- Initial version of LibPAX
