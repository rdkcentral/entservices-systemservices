# SystemServices Plugin

## Overview

The SystemServices plugin is a WPEFramework (Thunder) plugin that provides comprehensive system management capabilities for RDK devices including power management, firmware updates, system configuration, device diagnostics, and log management through a standardized JSON-RPC API (callsign: `org.rdk.SystemServices`, version: 3.4.1).

## Description

The SystemServices plugin serves as the central control point for system-level operations on RDK (Reference Design Kit) devices. It abstracts hardware-specific functionality through HAL (Hardware Abstraction Layer) interfaces and exposes a rich set of JSON-RPC APIs for managing device lifecycle, power states, firmware updates, system configuration, and diagnostic operations.

**Key Characteristics:**
- **Plugin Type**: WPEFramework R4.4+ plugin
- **Callsign**: `org.rdk.SystemServices`
- **Version**: 3.4.1 (Major: 3, Minor: 4, Patch: 1)
- **API Protocol**: JSON-RPC over WPEFramework
- **Platform**: Linux-based RDK platforms
- **License**: Apache 2.0

**Core Problem Solved:**
RDK devices require a unified interface for system management operations that abstracts platform-specific hardware differences while providing consistent APIs for power control, firmware management, configuration, and diagnostics. SystemServices provides this abstraction layer, enabling applications to control device behavior without direct hardware interaction.

**System Integration:**
The plugin integrates with multiple RDK subsystems:
- **IARM Bus**: Inter-process communication for system-wide coordination
- **Device Settings HAL**: Hardware abstraction for display and audio
- **PowerManager Plugin**: Coordinated power state management
- **FirmwareUpdate Plugin**: Firmware update orchestration
- **RFC Service**: Dynamic feature flag configuration
- **MFR Manager**: Manufacturing data access
- **SysMgr**: System manager for device state

**Key Terminology:**
- **IARM Bus**: Inter-Application Communication Bus for RDK components
- **HAL**: Hardware Abstraction Layer
- **RFC**: Remote Feature Control - dynamic feature flag system
- **DST**: Daylight Saving Time
- **STB**: Set-Top Box
- **MoCA**: Multimedia over Coax Alliance
- **CEC**: Consumer Electronics Control (HDMI control protocol)
- **EAS**: Emergency Alert System
- **FSR**: Factory Shipped Reset

## Requirements

### Functional Requirements

#### Power Management
- **REQ-PWR-001**: The plugin SHALL support power state transitions between ON, STANDBY, LIGHT_SLEEP, and DEEP_SLEEP states
- **REQ-PWR-002**: The plugin SHALL provide APIs to get and set the current power state with reason tracking
- **REQ-PWR-003**: The plugin SHALL support network standby mode to keep network interfaces active during standby
- **REQ-PWR-004**: The plugin SHALL allow configuration of wakeup sources per power state (IR, CEC, Voice, Bluetooth, Timer, Power Key, LAN, WiFi, RF4CE, Presence Detection)
- **REQ-PWR-005**: The plugin SHALL track and report the reason for the last device wakeup
- **REQ-PWR-006**: The plugin SHALL support thermal protection with configurable temperature thresholds
- **REQ-PWR-007**: The plugin SHALL support deep sleep timer for scheduled wakeup
- **REQ-PWR-008**: The plugin SHALL emit events on power state transitions and thermal threshold changes

#### Firmware Management
- **REQ-FW-001**: The plugin SHALL initiate firmware update processes via updateFirmware() API
- **REQ-FW-002**: The plugin SHALL track firmware update state through defined states (Uninitialized, Requesting, Downloading, Failed, DownLoad Complete, Validation Complete, Preparing to Reboot)
- **REQ-FW-003**: The plugin SHALL support configurable auto-reboot after firmware updates with delays up to 24 hours
- **REQ-FW-004**: The plugin SHALL provide APIs to query current, available, and downloaded firmware versions
- **REQ-FW-005**: The plugin SHALL report firmware update failure reasons for diagnostics
- **REQ-FW-006**: The plugin SHALL emit events on firmware update state changes and pending reboots

#### System Configuration
- **REQ-CFG-001**: The plugin SHALL support timezone configuration with DST awareness using standard timezone database
- **REQ-CFG-002**: The plugin SHALL support territory and region configuration with ISO 3166-1 and ISO 3166-2 validation
- **REQ-CFG-003**: The plugin SHALL allow setting and getting user-friendly device names with RFC persistence
- **REQ-CFG-004**: The plugin SHALL support operating modes: NORMAL, WAREHOUSE (retail demo), and EAS (Emergency Alert System)
- **REQ-CFG-005**: The plugin SHALL provide RFC integration for querying dynamic feature flag values
- **REQ-CFG-006**: The plugin SHALL emit events when timezone, territory, or friendly name changes

#### Device Information & Diagnostics
- **REQ-INFO-001**: The plugin SHALL provide APIs to retrieve device information including serial numbers, model name, hardware ID, and manufacturer data
- **REQ-INFO-002**: The plugin SHALL report system uptime since last boot
- **REQ-INFO-003**: The plugin SHALL identify boot type and reason (cold boot, warm boot, firmware update, power loss)
- **REQ-INFO-004**: The plugin SHALL retrieve MAC addresses for all network interfaces (ECM, eSTB, MoCA, Ethernet, WiFi, Bluetooth, RF4CE)
- **REQ-INFO-005**: The plugin SHALL provide software version information for STB components
- **REQ-INFO-006**: The plugin SHALL support platform capability queries
- **REQ-INFO-007**: The plugin SHALL track and report device migration status

#### Privacy & Compliance
- **REQ-PRIV-001**: The plugin SHALL provide telemetry opt-out capability with persistent storage
- **REQ-PRIV-002**: The plugin SHALL support application/domain blocklist management for parental controls
- **REQ-PRIV-003**: The plugin SHALL manage Factory Shipped Reset (FSR) flag for device provisioning

#### System Control
- **REQ-CTL-001**: The plugin SHALL provide reboot functionality with reason tracking
- **REQ-CTL-002**: The plugin SHALL coordinate reboots with PowerManager plugin
- **REQ-CTL-003**: The plugin SHALL track and report previous reboot information and reasons

#### Log Management
- **REQ-LOG-001**: The plugin SHALL support asynchronous log file upload to backend servers
- **REQ-LOG-002**: The plugin SHALL allow aborting in-progress log uploads
- **REQ-LOG-003**: The plugin SHALL emit events tracking log upload status (SUCCESS, FAILURE, ABORTED)

### Non-Functional Requirements

#### Performance
- **NFR-PERF-001**: Asynchronous operations (firmware download, log upload, MAC retrieval) SHALL NOT block the main plugin thread
- **NFR-PERF-002**: The plugin SHALL have minimal memory footprint suitable for embedded devices
- **NFR-PERF-003**: API response time SHALL be under 100ms for synchronous operations (excluding HAL latency)
- **NFR-PERF-004**: Event notifications SHALL be delivered to subscribers within 50ms of state change

#### Reliability
- **NFR-REL-001**: Critical configuration SHALL persist across device reboots
- **NFR-REL-002**: HAL failures SHALL be gracefully handled with appropriate error responses
- **NFR-REL-003**: Plugin SHALL recover from IARM Bus connection failures
- **NFR-REL-004**: Thread-safe access SHALL be enforced for all shared state

#### Security
- **NFR-SEC-001**: All external inputs SHALL be validated using regex patterns to prevent injection attacks
- **NFR-SEC-002**: Sensitive data SHALL be stored in `/opt/secure/persistent` with appropriate permissions
- **NFR-SEC-003**: WPEFramework security token validation SHALL be supported (configurable)
- **NFR-SEC-004**: Plugin SHALL run with minimal required system privileges

#### Compatibility
- **NFR-COMPAT-001**: Plugin SHALL be compatible with WPEFramework R4.4+
- **NFR-COMPAT-002**: Plugin SHALL follow RDK plugin architecture patterns
- **NFR-COMPAT-003**: API SHALL maintain backward compatibility within major version
- **NFR-COMPAT-004**: Conditional compilation flags SHALL enable platform-specific features without breaking other platforms

#### Maintainability
- **NFR-MAINT-001**: Code SHALL follow C++11/14 standards and WPEFramework conventions
- **NFR-MAINT-002**: All public APIs SHALL be documented with parameter descriptions and error codes
- **NFR-MAINT-003**: Code SHALL use enterprise error code framework for consistent error reporting
- **NFR-MAINT-004**: Changes SHALL be tracked in CHANGELOG.md with version updates

## Architecture / Design

### Component Structure

```
SystemServices Plugin
├── Core Plugin (SystemServices.cpp/h)
│   ├── JSON-RPC Interface Layer
│   ├── Event Notification System
│   └── IPlugin Implementation
│
├── Implementation Layer (SystemServicesImplementation.cpp/h)
│   ├── Power Management
│   ├── Firmware Management
│   ├── System Configuration
│   ├── Diagnostics & Monitoring
│   └── Log Management
│
├── Helper Modules
│   ├── SystemServicesHelper (common utilities)
│   ├── Platform Capabilities (platformcaps)
│   ├── Thermal Monitor (thermonitor)
│   ├── Upload Logs (uploadlogs)
│   └── Timer Utilities (cTimer)
│
└── Hardware Abstraction Layer (HAL)
    ├── IARM Bus (inter-process communication)
    ├── Device Settings (DS HAL)
    ├── MFR Manager (manufacturing data)
    ├── RFC Service (feature flags)
    └── Deep Sleep Manager (optional)
```

### Integration Points

**Required Services:**
- **IARM Bus**: Inter-process communication backbone
- **Device Settings HAL**: Hardware control abstraction
- **RFC Service**: Remote Feature Control for dynamic configuration
- **SysMgr**: System manager for device state coordination

**Optional Services:**
- **PowerManager Plugin**: Power state coordination and notifications
- **FirmwareUpdate Plugin**: Firmware update orchestration
- **Deep Sleep Manager HAL**: Deep sleep hardware control
- **Thermal Monitor**: Temperature threshold monitoring

## External Interfaces

The SystemServices plugin exposes its functionality through JSON-RPC APIs organized into seven major capability groups. All APIs follow the WPEFramework JSON-RPC protocol and are accessible via the plugin callsign `org.rdk.SystemServices`.

### 1. Power Management APIs

**Objective:** Control device power states and manage wake/sleep behavior.

#### Power States
- **ON**: Device fully operational
- **STANDBY**: Low-power state, quick resume
- **LIGHT_SLEEP**: Reduced power, partial hardware shutdown
- **DEEP_SLEEP**: Minimal power, extensive hardware shutdown

#### Features
- **Power State Control**: Transition between power states with reason tracking
- **Network Standby Mode**: Keep network interfaces active during standby for remote wake
- **Wakeup Source Configuration**: Configure multiple wake triggers per power state
  - IR Remote
  - CEC (HDMI Consumer Electronics Control)
  - Voice Assistant
  - Bluetooth
  - Timer
  - Power Key
  - LAN (Wake-on-LAN)
  - WiFi
  - RF4CE Remote
  - Presence Detection
- **Wakeup Reason Tracking**: Identify what triggered the last device wakeup
- **Thermal Protection**: Monitor temperature thresholds and receive alerts
- **Deep Sleep Timer**: Schedule automatic wake from deep sleep

#### API Methods
- `getPowerState()`: Get current and previous power states
- `setPowerState(powerState, standbyReason)`: Set device power state
- `getNetworkStandbyMode()`: Get network standby mode status
- `setNetworkStandbyMode(nwStandby)`: Enable/disable network during standby
- `getWakeupReason()`: Get reason for last wakeup (requires ENABLE_DEEP_SLEEP)
- `getLastWakeupKeyCode()`: Get key code that triggered wakeup
- `setWakeupSrcConfiguration(powerState, wakeupSources)`: Configure wake sources per state
- `getAvailableStandbyModes()`: Get supported standby modes
- `getPreferredStandbyMode()`: Get platform's preferred standby mode
- `setPreferredStandbyMode(standbyMode)`: Set preferred standby mode
- `setDeepSleepTimer(seconds)`: Schedule wake from deep sleep
- `getCoreTemperature()`: Get device core temperature (requires ENABLE_THERMAL_PROTECTION)
- `getTemperatureThresholds()`: Get thermal threshold configuration
- `setTemperatureThresholds(thresholds)`: Set thermal warning/critical thresholds
- `getPowerStateBeforeReboot()`: Get power state before last reboot
- `getPowerStateIsManagedByDevice()`: Check if power is device-managed

#### Events
- `onSystemPowerStateChanged`: Power state transition occurred
- `onTemperatureThresholdChanged`: Thermal threshold exceeded or returned to normal

### 2. Firmware Management APIs

**Objective:** Manage firmware updates, track update state, and control reboot behavior.

#### Features
- **Firmware Update Initiation**: Trigger firmware download and installation
- **Update State Monitoring**: Real-time tracking of update progress
  - `Uninitialized`: No update activity
  - `Requesting`: Checking for updates
  - `Downloading`: Downloading firmware
  - `Failed`: Update failed
  - `DownLoad Complete`: Download finished successfully
  - `Validation Complete`: Downloaded firmware validated
  - `Preparing to Reboot`: Ready to install
- **Auto-Reboot Management**: Configure automatic or delayed reboots
  - Reboot delays up to 24 hours (86400 seconds)
  - RFC-controlled auto-reboot feature flag
- **Version Tracking**: Query current, available, and downloaded firmware versions
- **Failure Diagnostics**: Retrieve failure reasons for failed updates
- **Rollback Support**: Track and report installation failures

#### API Methods
- `updateFirmware()`: Initiate firmware update process
- `getFirmwareUpdateInfo(GUID)`: Get firmware update availability (async)
- `getFirmwareUpdateState()`: Get current firmware update state
- `getDownloadedFirmwareInfo()`: Get info about downloaded firmware
- `getFirmwareDownloadPercent()`: Get download progress percentage
- `getLastFirmwareFailureReason()`: Get reason for last update failure
- `setFirmwareAutoReboot(enable)`: Enable/disable automatic reboot after update
- `setFirmwareRebootDelay(delaySeconds)`: Set delay before auto-reboot (RFC-based)

#### Events
- `onFirmwareUpdateInfoReceived`: Firmware update check completed
- `onFirmwareUpdateStateChanged`: Update state transition
- `onFirmwarePendingReboot`: Device ready to reboot for firmware installation

### 3. System Configuration APIs

**Objective:** Configure system-level settings including time, location, and device identity.

#### Features
- **Time Zone Management**: Set system timezone with DST awareness
  - Comprehensive timezone database from `/usr/share/zoneinfo`
  - Automatic DST handling
  - Timezone accuracy tracking
- **Territory Configuration**: Set device regional settings
  - ISO 3166-1 (country codes) and ISO 3166-2 (subdivision codes) support
  - Validation against standard territory/region lists
  - Region-specific regulatory compliance
- **Device Identity**: Configure user-facing device names
  - Friendly names for easy identification
  - RFC-based friendly name persistence
  - Default: "Living Room"
- **Operating Modes**: Support specialized device modes
  - **Normal**: Standard operation
  - **Warehouse**: Retail display mode with demo content
  - **EAS**: Emergency Alert System mode
  - Mode timers for automatic mode expiration
- **RFC Integration**: Dynamic feature flag management
  - Query RFC configuration values
  - Real-time feature enablement
- **Store Demo Management**: Configure retail demonstration content
  - Demo video link management
  - Persistent store mode configuration

#### API Methods
- `setTimeZoneDST(timeZone, accuracy)`: Set system timezone
- `getTimeZoneDST()`: Get current timezone configuration
- `getTimeZones()`: Get list of available timezones
- `setTerritory(territory, region)`: Set device territory/region
- `getTerritory()`: Get current territory configuration
- `setFriendlyName(friendlyName)`: Set user-friendly device name
- `getFriendlyName()`: Get current friendly name
- `setMode(modeInfo)`: Set operating mode with optional duration
- `getRFCConfig(rfcList)`: Get RFC feature flag values
- `setGzEnabled(enabled)`: Enable/disable gzip compression (deprecated)
- `isGzEnabled()`: Check gzip compression status (deprecated)
- `getStoreDemoLink()`: Get retail demo video URL
- `setStoreDemoLink(url)`: Set retail demo video URL
- `getXconfParams()`: Get Xconf configuration parameters (requires ENABLE_XCONF_PARAMS)

#### Events
- `onTerritoryChanged`: Territory or region configuration changed
- `onTimeZoneDSTChanged`: Timezone or accuracy changed
- `onSystemModeChanged`: Operating mode changed
- `onFriendlyNameChanged`: Device friendly name changed
- `onSystemClockSet`: System clock synchronized (requires ENABLE_SYSTIMEMGR_SUPPORT)

### 4. Device Information & Diagnostics APIs

**Objective:** Retrieve device hardware details, system state, and diagnostic information.

#### Features
- **Device Information Retrieval**: Query hardware and software details
  - Serial numbers (standard and manufacturing)
  - Model name and hardware ID
  - Manufacturer information
  - Device ID and friendly ID
  - Software build information
- **System Uptime**: Track operational duration since last boot
- **Boot Type Detection**: Identify boot reason and type
  - Cold boot
  - Warm boot
  - Firmware update reboot
  - Power loss recovery
- **Network Interface Information**: MAC address retrieval
  - ECM (Embedded Cable Modem)
  - eSTB (Set-Top Box)
  - MoCA (Multimedia over Coax Alliance)
  - Ethernet
  - WiFi
  - Bluetooth
  - RF4CE
- **System Versions**: Software component version tracking
  - STB version
  - Receiver version
  - STB timestamp
- **Migration Status**: Track device platform migration state
- **Platform Capabilities**: Query platform-specific feature support

#### API Methods
- `getDeviceInfo(params)`: Get comprehensive device information
- `getSerialNumber()`: Get device serial number
- `getMfgSerialNumber()`: Get manufacturing serial number (requires ENABLE_DEVICE_MANUFACTURER_INFO)
- `getSystemVersions()`: Get software version information
- `getMacAddresses(GUID)`: Get network interface MAC addresses (async)
- `requestSystemUptime()`: Get system uptime in seconds
- `getBootTypeInfo()`: Get boot type and reason
- `getBuildType()`: Get software build type (debug/release)
- `getStateInfo(param)`: Get various state information
- `queryMocaStatus()`: Get MoCA interface status
- `getMilestones()`: Get system initialization milestones
- `getMigrationStatus()`: Get device migration status
- `setMigrationStatus(status)`: Set migration status
- `getPlatformConfiguration(query)`: Query platform capabilities

#### Events
- `onMacAddressesRetrieved`: MAC address retrieval completed (async)
- `onDeviceMgtUpdateReceived`: Device management update notification

### 5. Privacy & Compliance APIs

**Objective:** Manage user privacy preferences and compliance settings.

#### Features
- **Telemetry Opt-Out**: Allow users to disable telemetry data collection
  - Persistent opt-out status in `/opt/tmtryoptout`
- **Application Blocklist**: Manage application or domain blocklists
  - Parental control support
  - Content filtering
- **FSR Flag Management**: Factory Shipped Reset flag for device provisioning

#### API Methods
- `isOptOutTelemetry()`: Check telemetry opt-out status
- `setOptOutTelemetry(optOut)`: Enable/disable telemetry collection
- `getBlocklistFlag()`: Get current blocklist status
- `setBlocklistFlag(blocklist)`: Set blocklist flag
- `getFSRFlag()`: Get Factory Shipped Reset flag
- `setFSRFlag(fsrFlag)`: Set FSR flag

### 6. System Control APIs

**Objective:** Control system-level operations including reboot and device lifecycle.

#### Features
- **System Reboot**: Initiate device reboot with reason tracking
- **Reboot Reason**: Track and report reboot causes
- **Cached Reboot Request**: Handle deferred reboot requests
- **Power Manager Coordination**: Integrate with PowerManager for coordinated operations

#### API Methods
- `reboot(rebootReason)`: Reboot device with optional reason
- `cacheContains(key)`: Check if cached reboot request exists
- `clearLastDeepSleepReason()`: Clear stored deep sleep reason
- `getLastDeepSleepReason()`: Get last deep sleep entry reason
- `getPreviousRebootInfo()`: Get information about last reboot
- `getPreviousRebootInfo2()`: Get detailed reboot information
- `getPreviousRebootReason()`: Get reason for last reboot

#### Events
- `onRebootRequest`: Reboot initiated by application or system

### 7. Log Management APIs

**Objective:** Enable remote diagnostics through asynchronous log uploads.

#### Features
- **Asynchronous Upload**: Non-blocking log file upload to backend servers
- **Progress Tracking**: Monitor upload state
  - `UPLOAD_SUCCESS`: Upload completed successfully
  - `UPLOAD_FAILURE`: Upload failed
  - `UPLOAD_ABORTED`: Upload cancelled by user
- **Abort Capability**: Cancel in-progress uploads
- **Process Management**: Thread-safe upload process tracking
- **RFC-Controlled**: Optional log upload before deep sleep

#### API Methods
- `uploadLogs()`: Synchronous log upload (deprecated)
- `uploadLogsAsync()`: Asynchronous log upload with status events
- `abortLogUpload()`: Cancel in-progress log upload

#### Events
- `onLogUpload`: Log upload state changed

## Data Models

### Common Types

```cpp
// Power States
enum PowerState {
    POWER_STATE_ON,
    POWER_STATE_STANDBY,
    POWER_STATE_LIGHT_SLEEP,
    POWER_STATE_DEEP_SLEEP,
    POWER_STATE_UNKNOWN
};

// Wakeup Sources
enum WakeupSrcType {
    WAKEUP_SRC_VOICE,
    WAKEUP_SRC_PRESENCEDETECTED,
    WAKEUP_SRC_BLUETOOTH,
    WAKEUP_SRC_WIFI,
    WAKEUP_SRC_IR,
    WAKEUP_SRC_POWERKEY,
    WAKEUP_SRC_TIMER,
    WAKEUP_SRC_CEC,
    WAKEUP_SRC_LAN,
    WAKEUP_SRC_RF4CE,
    WAKEUP_SRC_UNKNOWN
};

// Firmware Update States
enum FirmwareUpdateState {
    FirmwareUpdateStateUninitialized,
    FirmwareUpdateStateRequesting,
    FirmwareUpdateStateDownloading,
    FirmwareUpdateStateFailed,
    FirmwareUpdateStateDownLoadComplete,
    FirmwareUpdateStateValidationComplete,
    FirmwareUpdateStatePreparingToReboot
};

// Operating Modes
#define MODE_NORMAL     "NORMAL"
#define MODE_WAREHOUSE  "WAREHOUSE"
#define MODE_EAS        "EAS"
```

### Configuration Structures

```cpp
struct WakeupSourceConfig {
    WakeupSrcType type;
    bool enabled;
};

struct DeviceInfo {
    string serialNumber;
    string modelName;
    string hardwareID;
    string manufacturer;
    string friendly_id;
    // Additional fields based on params
};

struct ModeInfo {
    string mode;          // Operating mode
    int duration;         // Duration in seconds (optional)
};

struct SystemVersionsInfo {
    string stbVersion;
    string receiverVersion;
    string stbTimestamp;
};
```

## Error Handling

### Error Codes

The plugin uses the enterprise error code framework (`entservices_errorcodes.h`):

- **SysSrv_OK**: Operation successful
- **SysSrv_MethodNotFound**: JSON-RPC method not found
- **SysSrv_InvalidParameters**: Invalid or missing parameters
- **SysSrv_MissingKeyValues**: Required parameters missing
- **SysSrv_DeviceSettingsError**: Device Settings HAL error
- **SysSrv_UnknownError**: Unspecified error
- **SysSrv_FileAccessFailed**: File I/O error
- **SysSrv_Unexpected**: Unexpected internal error

### Input Validation

All external inputs are validated using regex patterns:

```cpp
#define REGEX_UNALLOWABLE_INPUT "[^[:alnum:]_-]{1}"
```

This prevents injection attacks by restricting inputs to alphanumeric characters, underscores, and hyphens.

### Exception Handling

Device Settings exceptions are caught and logged with context. HAL failures are gracefully handled with appropriate error responses to clients.

## State Management

### Persistence

Critical states are persisted to survive reboots:

**Storage Locations:**
- `/opt/persistent/` - General persistent storage
- `/opt/secure/persistent/` - Secure storage for sensitive data
- `/opt/persistent/localtime` - Timezone symlink
- `/opt/tmtryoptout` - Telemetry opt-out status
- `/opt/secure/persistent/opflashstore/devicestate.txt` - Device state
- `/opt/secure/persistent/MigrationStatus` - Migration status

**Settings Framework:**
- Uses `cSettings` helper for key-value persistence
- Temporary settings stored in SYSTEM_SERVICE_TEMP_FILE
- Thread-safe access with mutex protection

### State Synchronization

**IARM Bus Events:**
- Broadcast state changes system-wide
- Subscribe to SysMgr, MfrMgr, DS Manager events
- Coordinate with other RDK components

**PowerManager Integration:**
- Bidirectional notifications for power state changes
- Thermal mode change notifications
- Reboot coordination

## Threading Model

### Asynchronous Operations

**ThreadRAII Wrapper:**
- Safe thread lifecycle management
- RAII-based cleanup
- Used for:
  - Firmware downloads
  - Log uploads
  - MAC address retrieval

### Timer Operations

**cTimer Utility:**
- Mode expiration timers
- Deep sleep scheduling
- Reboot delay timers
- Background timer thread management

### Synchronization

**Mutexes:**
- `m_uploadLogsMutex`: Protect log upload state
- `m_territoryMutex`: Protect territory configuration
- `_adminLock`: Critical section for plugin operations

## Security

### Input Sanitization

- Regex-based validation prevents injection attacks
- Path traversal prevention
- Command injection protection

### Secure Storage

- Sensitive data in `/opt/secure/persistent`
- Secure wrapper functions for system calls
- Token-based authentication support (optional)

### Access Control

- WPEFramework security token validation
- Can be disabled for development builds
- Appropriate system privileges for hardware access

## Dependencies

### Build Dependencies

**Required:**
- WPEFramework (Thunder) R4.4+
- IARM Bus library
- Device Settings HAL
- RFC library
- libcurl (HTTP/HTTPS)
- libprocps (process info)

**Optional:**
- Deep Sleep Manager HAL (`ENABLE_DEEP_SLEEP`)
- Thermal Monitor (`ENABLE_THERMAL_PROTECTION`)
- System Time Manager (`ENABLE_SYSTIMEMGR_SUPPORT`)
- MFR HAL (`ENABLE_DEVICE_MANUFACTURER_INFO`)

### Runtime Dependencies

**Required Services:**
- IARM Bus daemon
- SysMgr (System Manager)
- RFC service
- Device Settings service

**Optional Services:**
- PowerManager plugin
- FirmwareUpdate plugin
- Deep Sleep HAL daemon

## Configuration

### Build Flags

Conditional compilation flags control optional features:

- `ENABLE_THERMAL_PROTECTION`: Enable thermal monitoring
- `ENABLE_DEEP_SLEEP`: Enable deep sleep functionality
- `ENABLE_DEVICE_MANUFACTURER_INFO`: Enable MFR data APIs
- `ENABLE_SYSTIMEMGR_SUPPORT`: Enable system time manager integration
- `ENABLE_XCONF_PARAMS`: Enable Xconf parameter retrieval
- `USE_IARMBUS` / `USE_IARM_BUS`: Enable IARM Bus integration
- `HAS_API_SYSTEM`: Enable system APIs
- `HAS_API_POWERSTATE`: Enable power state APIs

### Configuration Files

**Plugin Configuration:**
- `SystemServices.config`: JSON-RPC configuration
- `SystemServices.conf.in`: Template configuration

**System Configuration:**
- `/etc/device.properties`: Device property definitions
- `/usr/share/zoneinfo/*`: Timezone database
- `/usr/share/iso-codes/json/`: ISO territory codes

## Conformance Testing & Validation

### Test Strategy

The SystemServices plugin employs a multi-layered testing approach to ensure conformance with specifications:

### Test Framework

**L1 Tests (Unit):**
- Location: `Tests/L1Tests/tests/test_SystemServices.cpp`
- Unit-level API testing
- Mock HAL interfaces

**L2 Tests (Integration):**
- Location: `Tests/L2Tests/tests/SystemService_L2Test.cpp`
- End-to-end integration testing
- Real HAL interaction

**Test Client:**
- Location: `plugin/TestClient/systemServiceTestClient.cpp`
- Interactive JSON-RPC testing
- Manual API validation

## Performance

### Resource Efficiency

- **Memory Footprint**: Minimal runtime overhead
- **CPU Usage**: Asynchronous operations prevent blocking
- **Network**: Efficient curl-based transfers
- **Storage**: Minimal persistent state

### Scalability

- **Multi-Instance**: Can coexist with other plugins
- **Event Broadcasting**: Efficient subscriber notification
- **Modular Design**: Easy feature addition

## Versioning & Compatibility

### Versioning Scheme
- **Plugin Version**: 3.4.1 follows semantic versioning (MAJOR.MINOR.PATCH)
- **Major Version**: Breaking API changes
- **Minor Version**: New features, backward compatible
- **Patch Version**: Bug fixes, backward compatible

### Compatibility Guarantees
- **Backward Compatibility**: APIs maintain compatibility within major version
- **Platform Compatibility**: Requires WPEFramework R4.4+
- **HAL Compatibility**: Platform-specific HAL versions may vary
- **Conditional Features**: Build flags enable/disable features without breaking other platforms

### Migration Path
- Deprecated APIs marked in documentation with replacement recommendations
- Minimum one minor version cycle before API removal
- `setGzEnabled()`/`isGzEnabled()` deprecated - alternative compression methods recommended
- `uploadLogs()` deprecated in favor of `uploadLogsAsync()`

## Compliance & Standards

### RDK Standards

- Follows RDK plugin architecture patterns
- Uses standard RDK HAL interfaces
- IARM Bus message format compliance

### Coding Standards

- C++11/14 standards
- WPEFramework coding conventions
- Enterprise error code framework

### Documentation

- Apache 2.0 License headers
- CHANGELOG.md for version tracking
- Comprehensive API documentation

## Extension Points

### Adding New Features

1. **New JSON-RPC Methods**: Register in Initialize()
2. **New Events**: Extend INotification interface
3. **New Platform Capabilities**: Extend platformcaps subsystem
4. **New Helper Functions**: Add to SystemServicesHelper

### Plugin Integration

**Consuming SystemServices:**
```cpp
auto systemServices = service->QueryInterfaceByCallsign<Exchange::ISystemServices>(
    "org.rdk.SystemServices"
);
```

**Event Subscription:**
```cpp
systemServices->Register(notificationHandler);
```

## Known Constraints

### Platform Dependencies

- Requires Linux-based RDK platform
- IARM Bus must be available and initialized
- Device Settings HAL must be present
- RFC service must be running

### Performance Limits

- Maximum reboot delay: 24 hours (86400 seconds)
- Mode timer maximum depends on platform
- Upload log size limited by available memory

### Compatibility

- WPEFramework R4.4+ required
- Specific HAL versions may have compatibility requirements
- Some features require platform-specific HAL support

## Covered Code

The following code files and methods/classes are covered by this specification:

### Core Plugin Files
- plugin/SystemServices.cpp:
    - SystemServices::Initialize
    - SystemServices::Deinitialize
    - SystemServices::Notification::OnFirmwareUpdateInfoReceived
    - SystemServices::Notification::OnRebootRequest
    - SystemServices::Notification::OnSystemPowerStateChanged
    - SystemServices::Notification::OnTerritoryChanged
    - SystemServices::Notification::OnTimeZoneDSTChanged
    - SystemServices::Notification::OnMacAddressesRetreived
    - SystemServices::Notification::OnSystemModeChanged
    - SystemServices::Notification::OnLogUpload
    - SystemServices::Notification::OnFirmwareUpdateStateChanged
    - SystemServices::Notification::OnTemperatureThresholdChanged
    - SystemServices::Notification::OnSystemClockSet
    - SystemServices::Notification::OnFirmwarePendingReboot
    - SystemServices::Notification::OnFriendlyNameChanged
    - SystemServices::Notification::OnDeviceMgtUpdateReceived
    - SystemServices::Notification::OnBlocklistChanged
    - SystemServices::Notification::OnTimeStatusChanged
    - SystemServices::Notification::OnNetworkStandbyModeChanged

- plugin/SystemServices.h:
    - SystemServices class definition
    - SystemServices::Notification class definition

### Implementation Layer
- plugin/SystemServicesImplementation.cpp:
    - SystemServicesImplementation::Configure
    - SystemServicesImplementation::InitializeIARM
    - SystemServicesImplementation::DeinitializeIARM
    - SystemServicesImplementation::InitializePowerManager
    - SystemServicesImplementation::registerEventHandlers
    - SystemServicesImplementation::Dispatch
    - SystemServicesImplementation::dispatchEvent

- plugin/SystemServicesImplementation.h:
    - SystemServicesImplementation class definition
    - SystemServicesImplementation::PowerManagerNotification class definition

### Power Management APIs
- plugin/SystemServicesImplementation.cpp:
    - SystemServicesImplementation::GetPowerState
    - SystemServicesImplementation::SetPowerState
    - SystemServicesImplementation::GetNetworkStandbyMode
    - SystemServicesImplementation::SetNetworkStandbyMode
    - SystemServicesImplementation::GetWakeupReason
    - SystemServicesImplementation::GetLastWakeupKeyCode
    - SystemServicesImplementation::SetWakeupSrcConfiguration
    - SystemServicesImplementation::SetDeepSleepTimer
    - SystemServicesImplementation::GetPowerStateBeforeReboot
    - SystemServicesImplementation::OnPowerModeChanged
    - SystemServicesImplementation::OnNetworkStandbyModeChanged
    - SystemServicesImplementation::OnThermalModeChanged
    - SystemServicesImplementation::OnTemperatureThresholdChanged

### Firmware Management APIs
- plugin/SystemServicesImplementation.cpp:
    - SystemServicesImplementation::UpdateFirmware
    - SystemServicesImplementation::GetFirmwareUpdateInfo
    - SystemServicesImplementation::GetFirmwareUpdateState
    - SystemServicesImplementation::GetDownloadedFirmwareInfo
    - SystemServicesImplementation::GetFirmwareDownloadPercent
    - SystemServicesImplementation::GetLastFirmwareFailureReason
    - SystemServicesImplementation::SetFirmwareAutoReboot
    - SystemServicesImplementation::OnFirmwareUpdateInfoRecieved
    - SystemServicesImplementation::OnFirmwareUpdateStateChange
    - SystemServicesImplementation::OnFirmwarePendingReboot
    - SystemServicesImplementation::firmwareUpdateInfoReceived
    - SystemServicesImplementation::reportFirmwareUpdateInfoReceived

### System Configuration APIs
- plugin/SystemServicesImplementation.cpp:
    - SystemServicesImplementation::SetTimeZoneDST
    - SystemServicesImplementation::GetTimeZoneDST
    - SystemServicesImplementation::GetTimeZones
    - SystemServicesImplementation::processTimeZones
    - SystemServicesImplementation::SetTerritory
    - SystemServicesImplementation::GetTerritory
    - SystemServicesImplementation::writeTerritory
    - SystemServicesImplementation::readTerritoryFromFile
    - SystemServicesImplementation::getAlpha2ForTerritory
    - SystemServicesImplementation::isSubdivisionExists
    - SystemServicesImplementation::isRegionValidForTerritory
    - SystemServicesImplementation::SetFriendlyName
    - SystemServicesImplementation::GetFriendlyName
    - SystemServicesImplementation::SetMode
    - SystemServicesImplementation::GetRFCConfig
    - SystemServicesImplementation::OnTerritoryChanged
    - SystemServicesImplementation::OnTimeZoneDSTChanged
    - SystemServicesImplementation::OnSystemModeChanged
    - SystemServicesImplementation::OnFriendlyNameChanged
    - SystemServicesImplementation::startModeTimer
    - SystemServicesImplementation::stopModeTimer
    - SystemServicesImplementation::updateDuration

### Device Information & Diagnostics APIs
- plugin/SystemServicesImplementation.cpp:
    - SystemServicesImplementation::GetDeviceInfo
    - SystemServicesImplementation::GetSerialNumber
    - SystemServicesImplementation::GetMfgSerialNumber
    - SystemServicesImplementation::GetSystemVersions
    - SystemServicesImplementation::getMacAddresses
    - SystemServicesImplementation::getMacAddressesAsync
    - SystemServicesImplementation::RequestSystemUptime
    - SystemServicesImplementation::GetBootTypeInfo
    - SystemServicesImplementation::GetBuildType
    - SystemServicesImplementation::GetMigrationStatus
    - SystemServicesImplementation::SetMigrationStatus
    - SystemServicesImplementation::GetPlatformConfiguration
    - SystemServicesImplementation::OnDeviceMgtUpdateReceived

### Privacy & Compliance APIs
- plugin/SystemServicesImplementation.cpp:
    - SystemServicesImplementation::IsOptOutTelemetry
    - SystemServicesImplementation::SetOptOutTelemetry
    - SystemServicesImplementation::GetBlocklistFlag
    - SystemServicesImplementation::SetBlocklistFlag
    - SystemServicesImplementation::GetFSRFlag
    - SystemServicesImplementation::SetFSRFlag
    - SystemServicesImplementation::OnBlocklistChanged

### System Control APIs
- plugin/SystemServicesImplementation.cpp:
    - SystemServicesImplementation::Reboot
    - SystemServicesImplementation::OnRebootBegin
    - SystemServicesImplementation::OnPwrMgrReboot
    - SystemServicesImplementation::OnRebootRequest

### Log Management APIs
- plugin/SystemServicesImplementation.cpp:
    - SystemServicesImplementation::UploadLogsAsync
    - SystemServicesImplementation::AbortLogUpload
    - SystemServicesImplementation::OnLogUpload

- plugin/uploadlogs.cpp:
    - uploadLogsMain

- plugin/uploadlogs.h:
    - uploadLogsMain function declaration

### Helper Modules
- plugin/SystemServicesHelper.cpp:
    - Helper utility functions

- plugin/SystemServicesHelper.h:
    - Helper utility declarations

- plugin/cTimer.cpp:
    - cTimer class implementation

- plugin/cTimer.h:
    - cTimer class definition

- plugin/thermonitor.cpp:
    - Thermal monitoring implementation

- plugin/thermonitor.h:
    - Thermal monitoring interface

- plugin/platformcaps/platformcaps.cpp:
    - Platform capabilities implementation

- plugin/platformcaps/platformcaps.h:
    - Platform capabilities interface

- plugin/platformcaps/platformcapsdata.cpp:
    - Platform capabilities data management

- plugin/platformcaps/platformcapsdata.h:
    - Platform capabilities data structures

- plugin/platformcaps/platformcapsdatarpc.cpp:
    - Platform capabilities RPC interface

### Module Files
- plugin/Module.cpp:
    - Module initialization

- plugin/Module.h:
    - Module declarations

### Test Files
- Tests/L1Tests/tests/test_SystemServices.cpp:
    - Unit tests for SystemServices

- Tests/L2Tests/tests/SystemService_L2Test.cpp:
    - Integration tests for SystemServices

- plugin/TestClient/systemServiceTestClient.cpp:
    - Manual test client for JSON-RPC APIs

## Open Queries

_No open queries at this time._

## References

### Related RDK Documentation
- WPEFramework (Thunder) Plugin Development Guide
- RDK Device Settings HAL Specification
- IARM Bus Architecture Documentation
- RFC (Remote Feature Control) Service Specification

### Standards
- ISO 3166-1: Country codes (territory configuration)
- ISO 3166-2: Subdivision codes (region configuration)
- JSON-RPC 2.0 Specification
- Semantic Versioning 2.0.0 (semver.org)

### External Resources
- IANA Timezone Database: /usr/share/zoneinfo
- Apache License 2.0: https://www.apache.org/licenses/LICENSE-2.0

## Change History

- [2026-08-04] - openspec-templater - Restructured to match OpenSpec spec template format. Added Requirements section with functional and non-functional requirements. Added Covered Code section with complete file and method mappings. Reorganized content into standard template sections (Overview, Description, Requirements, Architecture/Design, External Interfaces, Performance, Security, Versioning & Compatibility, Conformance Testing & Validation).

