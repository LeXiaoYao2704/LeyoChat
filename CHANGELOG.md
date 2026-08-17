# Changelog

Notable user-visible changes are recorded here. The project has not yet made a
public tagged release.

## Unreleased

### Changed

- Renamed the product and deployment identity to LeyoChat.
- Removed built-in message-service endpoints and default credentials.
- Made `P2POnly` the default for a fresh client configuration.
- Added configurable service URL, credential, workspace, and transport mode.
- Added reproducible project-owned backgrounds, stickers, and placeholder
  images in place of media with unclear provenance.
- Added third-party provenance and license notices.

### Fixed

- Unified foreground visibility checks before sending read receipts.
- Preserved mention and reply metadata on the service delivery path.
- Improved durable delivery acknowledgements and local message state updates.
- Added service polling recovery and client notification handling.
- Removed service-token values from database warning logs.

## 0.4.0 - pre-public baseline

- Added the supervised desktop launcher and Windows service host.
- Added durable direct and group delivery through the optional message service.
- Added mixed-version routing policies and P2P fallback controls.
- Added installer-generated service credentials and workspace configuration.
- Added local recovery snapshots without automatic message resending.
