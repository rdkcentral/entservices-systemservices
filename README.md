# SystemServices Plugin

The SystemServices plugin is a WPEFramework (Thunder) plugin that provides comprehensive system-level management capabilities for RDK-based entertainment devices. It serves as the central point for controlling device power states, firmware updates, system configuration, and monitoring critical system parameters.

## Overview

SystemServices exposes a rich JSON-RPC API that enables applications to interact with core system capabilities through a standardized interface. It manages device operations including:

- **Power Management**: Multiple power states (ON, STANDBY, LIGHT_SLEEP, DEEP_SLEEP), network standby, wake source configuration, and thermal protection
- **Firmware Updates**: Over-the-air updates, scheduled reboots, update state tracking, and rollback support
- **System Configuration**: Device information, time zone management, territory settings, and operating modes
- **Diagnostics**: System uptime, boot type information, build information, and log upload capabilities

## Documentation

| Document | Purpose |
|---|---|
| [ARCHITECTURE.md](ARCHITECTURE.md) | Technical architecture, component diagrams, data flows, threading model, and dependencies |
| [PRODUCT.md](PRODUCT.md) | Product features, use cases, API capabilities, and deployment details |
| [CHANGELOG.md](CHANGELOG.md) | Version history and release notes |
| [CONTRIBUTING.md](CONTRIBUTING.md) | Contribution guidelines and CLA requirements |

> **New to this repository?** Start with [PRODUCT.md](PRODUCT.md) for a feature overview, then [ARCHITECTURE.md](ARCHITECTURE.md) for technical implementation details.

## Repository Structure

```
.
├── .github/
│   ├── workflows/          # GitHub Actions CI/CD workflows
│   ├── instructions/       # Development instructions and guidelines
│   └── skills/            # OpenSpec skills for change management
├── Tests/                  # L1 unit tests and L2 integration tests
├── cmake/                  # CMake module files and find-package helpers
├── openspec/              # OpenSpec specifications and configuration
├── plugin/                 # Thunder plugin source code (C++)
│   ├── platformcaps/      # Platform capabilities subsystem
│   └── TestClient/        # Test client for API validation
├── ARCHITECTURE.md         # Technical architecture documentation
├── CHANGELOG.md            # Version history
├── CMakeLists.txt          # Top-level CMake build definition
├── CONTRIBUTING.md         # Contribution guidelines
├── LICENSE                 # Apache License 2.0
├── NOTICE                  # Third-party notices
├── PRODUCT.md              # Product feature and API documentation
├── README.md               # This file
├── build_dependencies.sh   # Script to fetch and build external dependencies
└── cov_build.sh            # Coverage build script
```

## Building

### Prerequisites

| Tool | Minimum Version | Notes |
|---|---|---|
| CMake | 3.3+ | Build system |
| GCC or Clang | GCC 11+ / Clang 14+ | C++ compiler |
| Python 3 | 3.x | Required for JSON schema tools |
| Docker | Any | Required for native full-build workflow |

### Quick Build (using RDK CI Docker image)

```bash
docker pull ghcr.io/rdkcentral/docker-rdk-ci:latest
docker run --rm -v $(pwd):/workspace -w /workspace \
  ghcr.io/rdkcentral/docker-rdk-ci:latest \
  bash -c "sh -x build_dependencies.sh && sh -x cov_build.sh"
```

### Manual CMake Build

```bash
# 1. Fetch external dependencies
bash build_dependencies.sh

# 2. Configure
cmake -G Ninja \
  -S . \
  -B build \
  -DCMAKE_INSTALL_PREFIX="$PWD/install/usr" \
  -DPLUGIN_SYSTEMSERVICES=ON \
  -DUSE_THUNDER_R4=ON

# 3. Build and install
cmake --build build -j$(nproc)
cmake --install build
```

## Testing

The repository includes comprehensive test coverage:

- **L1 Tests**: Unit tests using Google Test framework without hardware dependencies
- **L2 Tests**: Integration tests against a live Thunder runtime

Run tests using the provided CMake configuration:

```bash
# Enable test binaries during configuration
cmake -DTESTBINARIES=ON -DPLUGIN_SYSTEMSERVICES=ON

# Run L1 tests
cd build
./Tests/L1Tests/SystemServices_L1Test

# Run L2 tests (requires Thunder runtime)
./Tests/L2Tests/SystemService_L2Test
```

## API Capabilities

The plugin provides 40+ JSON-RPC methods across these categories:

- **Power Control**: `getPowerState`, `setPowerState`, `setNetworkStandbyMode`, `getWakeupReason`, `setWakeupSrcConfiguration`
- **Firmware Management**: `updateFirmware`, `getFirmwareUpdateInfo`, `getFirmwareUpdateState`, `setFirmwareAutoReboot`
- **Device Information**: `getDeviceInfo`, `getSerialNumber`, `getSystemVersions`, `getMacAddresses`
- **Configuration**: `setTimeZoneDST`, `getTimeZoneDST`, `setTerritory`, `setFriendlyName`, `setMode`
- **System Control**: `reboot`, `requestSystemUptime`, `setDeepSleepTimer`, `getRFCConfig`
- **Diagnostics**: `uploadLogsAsync`, `abortLogUpload`, `getBootTypeInfo`, `getBuildType`
- **Privacy**: `setOptOutTelemetry`, `isOptOutTelemetry`, `setBlocklistFlag`

Additionally, it provides 15+ event notifications for real-time system state changes.

## CI/CD Workflows

The repository uses GitHub Actions for automated testing and quality checks:

- **L1-tests.yml**: Unit tests with coverage and Valgrind memory checking
- **L2-tests.yml**: Integration tests against live Thunder runtime
- **native_full_build.yml**: Full build validation in RDK environment
- **update-changelog-and-api-version.yml**: Enforces CHANGELOG.md updates
- **cla.yml**: Contributor License Agreement verification
- **fossid_integration_stateless_diffscan_target_repo.yml**: Open-source license scanning

## Contributing

Contributions are welcome! Please follow these guidelines:

1. Fork the repository and create a feature branch
2. Make your changes with focused, atomic commits
3. Update `CHANGELOG.md` for all changes targeting `main` or `release/**` branches
4. Ensure all tests pass (L1, L2, and native build)
5. Sign the Contributor License Agreement (CLA)
6. Open a pull request against `main` or `develop`

See [CONTRIBUTING.md](CONTRIBUTING.md) for detailed guidelines.

## License

This project is licensed under the Apache License 2.0. See [LICENSE](LICENSE) for the full license text.

## Support

For issues, questions, or contributions, please use the GitHub issue tracker and refer to the existing documentation for guidance.