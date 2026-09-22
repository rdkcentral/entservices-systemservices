# SystemServices Plugin Architecture

## Overview

The SystemServices plugin is a Thunder plugin that provides comprehensive system-level management capabilities for RDK-based devices. It serves as the central point for controlling device power states, firmware updates, system configuration, and monitoring critical system parameters.

## System Architecture

### Component Hierarchy

```
┌─────────────────────────────────────────────────────────────┐
│                   Thunder Core                         │
│              (Thunder Plugin Framework)                      │
└──────────────────────┬──────────────────────────────────────┘
                       │
       ┌───────────────┴───────────────┐
       │   SystemServices Plugin       │
       │  (JSON-RPC Interface)         │
       └───────────┬───────────────────┘
                   │
    ┌──────────────┼──────────────┐
    │              │              │
┌───▼────┐  ┌─────▼──────┐  ┌───▼──────┐
│ Power  │  │  Firmware  │  │  System  │
│ Mgmt   │  │   Update   │  │  Config  │
└────┬───┘  └─────┬──────┘  └────┬─────┘
     │            │              │
┌────▼────────────▼──────────────▼─────┐
│      Hardware Abstraction Layer      │
│  (IARM, Device Settings, RFC, MFR)   │
└──────────────────────────────────────┘
```

### Core Components

#### 1. Plugin Framework Integration
- **Base Classes**: Inherits from `PluginHost::IPlugin` and implements JSON-RPC support
- **Service Registration**: Registered as a Thunder service with versioning (Major: 3, Minor: 4, Patch: 1)
- **Multi-Handler Support**: Supports multiple JSON-RPC handlers for flexible API versioning

#### 2. Power Management Subsystem
- **Power State Control**: Manages transitions between ON, STANDBY, LIGHT_SLEEP, and DEEP_SLEEP states
- **Network Standby Mode**: Controls device wake-on-LAN and network accessibility during standby
- **Wakeup Source Configuration**: Configures and tracks device wakeup triggers (IR, CEC, Voice, Bluetooth, Timer, etc.)
- **Thermal Protection**: Monitors temperature thresholds and triggers thermal events
- **Power Manager Integration**: Interfaces with PowerManager plugin through `IPowerManager` interface

#### 3. Firmware Management Subsystem
- **Firmware Update**: Orchestrates firmware download and installation processes
- **Auto-Reboot Management**: Handles scheduled reboots with configurable delays (up to 24 hours)
- **Update State Tracking**: Monitors firmware update progress through multiple states (Downloading, Downloaded, Failed, etc.)
- **Rollback Support**: Tracks firmware failure reasons for diagnostics
- **Upload Logs**: Asynchronous log upload functionality with progress tracking

#### 4. System Configuration Management
- **Device Information**: Retrieves hardware details (serial numbers, model, manufacturer info)
- **Time Zone Management**: Configures system timezone and DST settings
- **Territory Settings**: Manages device regional configuration
- **RFC Configuration**: Interfaces with Remote Feature Control for dynamic feature flags
- **Operating Modes**: Supports Normal, Warehouse, and EAS (Emergency Alert System) modes
- **Friendly Name**: User-configurable device naming

#### 5. Platform Capabilities (platformcaps)
- **Capability Discovery**: Provides platform-specific feature detection
- **Data Management**: Handles platform capability data storage and retrieval
- **RPC Interface**: Exposes platform capabilities through JSON-RPC methods

## Data Flow Architecture

### Power State Transition Flow
```
User Request → JSON-RPC Handler → Power State Validator
    → IARM Bus Command → Device Settings HAL → Hardware
    → Event Notification → PowerManager Plugin → Client Notification
```

### Firmware Update Flow
```
updateFirmware() → Download Manager → Progress Monitoring
    → Installation Trigger → Reboot Scheduler
    → Event Notifications (State Changes) → Client Updates
```

### Configuration Persistence Flow
```
Configuration Change → Validation → cSettings Writer
    → Persistent Storage (/opt/persistent) → IARM Event
    → System-wide Notification
```

## Plugin Integration Points

### Thunder Integration
- **JSON-RPC Protocol**: All APIs exposed through Thunder's JSON-RPC mechanism
- **Service Discovery**: Discoverable through Thunder controller
- **Event Notifications**: Publishes events to subscribed clients
- **Configuration Management**: Uses Thunder's configuration framework

### PowerManager Interface
- **Bidirectional Communication**: Both consumes and provides power-related services
- **Notification Callbacks**: Receives power state change notifications
- **Mode Management**: Coordinates power modes with system-wide power policies

### IARM Bus Communication
- **Broadcast Events**: Publishes system state changes to IARM bus
- **Service Coordination**: Coordinates with other IARM-based services (SysMgr, MfrMgr, DS Manager)
- **Thread Safety**: Implements mutex-based synchronization for IARM operations

## Technical Implementation Details

### Threading Model
- **Asynchronous Operations**: Uses `Utils::ThreadRAII` for safe thread management
- **Background Tasks**: Firmware downloads, log uploads, and MAC address retrieval run asynchronously
- **Timer-based Operations**: Mode timers and deep sleep timers use `cTimer` wrapper

### State Management
- **State Persistence**: Critical states stored in `/opt/persistent` and `/opt/secure/persistent`
- **Settings Framework**: Uses `cSettings` helper for key-value storage
- **State Synchronization**: IARM bus ensures system-wide state consistency

### Error Handling
- **JSON-RPC Error Codes**: Uses enterprise error code framework (`entservices_errorcodes.h`)
- **Exception Handling**: Device Settings exceptions caught and logged with context
- **Validation**: Input validation using regex patterns to prevent injection attacks

### Security Considerations
- **Input Sanitization**: Regex-based validation of user inputs (`REGEX_UNALLOWABLE_INPUT`)
- **Secure Storage**: Sensitive data stored in `/opt/secure/persistent`
- **Token Validation**: Supports optional security token validation (can be disabled)
- **Secure Wrappers**: Uses `secure_wrapper.h` for system calls

## Dependencies

### Required Libraries
- **Thunder**: Core plugin framework and JSON-RPC support
- **IARM Bus**: Inter-process communication for RDK components
- **Device Settings (DS)**: Hardware abstraction for display and audio
- **RFC**: Remote Feature Control for dynamic configuration
- **libcurl**: HTTP/HTTPS communication for firmware and log uploads
- **libprocps**: System process information

### Helper Utilities
- **UtilsCStr**: C-string manipulation helpers
- **UtilsIarm**: IARM bus convenience wrappers
- **UtilsJsonRpc**: JSON-RPC utility functions
- **UtilsString**: String processing utilities
- **UtilsfileExists**: File system utilities
- **UtilsgetFileContent**: File reading utilities
- **UtilsProcess**: Process management utilities
- **UtilsLogging**: Centralized logging macros
- **cSettings**: Settings persistence framework

### Hardware Abstraction Layer
- **MFR Manager**: Manufacturing data access
- **Deep Sleep Manager**: Deep sleep hardware control
- **Thermal Monitor**: Temperature monitoring (optional)

## Extension Points

The plugin architecture supports extensibility through:
1. **Additional JSON-RPC Methods**: New APIs can be registered using `registerMethod`
2. **Event Notifications**: New events can be added by extending notification system
3. **Platform Capabilities**: Platform-specific features through platformcaps subsystem
4. **Helper Framework**: Reusable utility functions in helpers directory

## Build System

- **CMake-based**: Uses modern CMake (3.3+) with modular configuration
- **Conditional Compilation**: Feature flags control optional functionality (ENABLE_THERMAL_PROTECTION, ENABLE_DEEP_SLEEP, etc.)
- **Test Integration**: Supports L1 (unit) and L2 (integration) test frameworks
- **Dependency Management**: Uses CMake Find modules for external dependencies
