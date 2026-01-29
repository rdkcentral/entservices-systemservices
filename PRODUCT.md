# SystemServices Plugin - Product Documentation

## Product Overview

The SystemServices plugin is a comprehensive system management solution for RDK (Reference Design Kit) devices, providing essential functionality for controlling device operations, managing firmware updates, and monitoring system health. As a WPEFramework (Thunder) plugin, it exposes a rich JSON-RPC API that enables applications to interact with core system capabilities through a standardized interface.

## Key Features

### Power Management
- **Flexible Power States**: Support for multiple power states including ON, STANDBY, LIGHT_SLEEP, and DEEP_SLEEP modes
- **Network Standby Mode**: Keep network interfaces active during standby for quick wake and remote management
- **Wake Source Configuration**: Configure multiple wake triggers including IR remote, CEC, voice commands, Bluetooth, timers, and network packets
- **Wakeup Reason Tracking**: Identify what triggered the last device wakeup for analytics and user experience optimization
- **Thermal Protection**: Monitor device temperature and receive notifications when thermal thresholds are exceeded

### Firmware Management
- **Over-the-Air Updates**: Download and install firmware updates remotely with progress tracking
- **Scheduled Reboots**: Configure automatic or delayed reboots with delays up to 24 hours for convenient update timing
- **Update State Monitoring**: Real-time visibility into firmware update progress (Downloading, Downloaded, Installing, Failed, etc.)
- **Rollback Support**: Track firmware installation failures and retrieve failure reasons for diagnostics
- **Version Management**: Query current firmware version, available updates, and downloaded firmware information

### System Configuration
- **Device Information Retrieval**: Access hardware details including serial numbers, model information, manufacturer data, and device IDs
- **Time Zone Management**: Configure system timezone and daylight saving time settings with comprehensive timezone database
- **Territory Configuration**: Set device regional settings to comply with local regulations and preferences
- **Friendly Device Naming**: Allow users to set custom device names for easy identification in multi-device environments
- **RFC Integration**: Dynamic feature flag management through Remote Feature Control
- **Operating Modes**: Support specialized operating modes including Normal, Warehouse (for retail display), and Emergency Alert System (EAS)

### Diagnostic and Monitoring
- **System Uptime**: Track device operational duration since last boot
- **Boot Type Information**: Identify device boot reasons (cold boot, warm boot, firmware update, etc.)
- **Build Information**: Retrieve software build type and version details
- **MAC Address Retrieval**: Query network interface MAC addresses for device identification
- **Log Upload**: Asynchronous log file upload to backend servers for remote diagnostics with abort capability

### Privacy and Compliance
- **Telemetry Opt-Out**: Allow users to disable telemetry data collection
- **Blocklist Management**: Manage application or domain blocklists for parental controls
- **Migration Status**: Track device migration state during platform transitions

## Use Cases

### Consumer Electronics Devices
1. **Set-Top Boxes (STBs)**: Complete system management for cable and streaming devices
2. **Smart TVs**: Power management and firmware updates for smart television platforms
3. **Streaming Devices**: System control for OTT streaming dongles and boxes

### Service Provider Operations
1. **Remote Device Management**: Centralized control of deployed devices for firmware updates and configuration
2. **Fleet Monitoring**: Track device health, uptime, and thermal status across large deployments
3. **Staged Rollouts**: Controlled firmware deployments with scheduled reboot windows
4. **Diagnostic Data Collection**: Remote log retrieval for customer support and troubleshooting

### Smart Home Integration
1. **Voice Assistant Integration**: Wake device from standby using voice commands
2. **Home Automation**: Integration with smart home systems through power state and network standby controls
3. **Energy Management**: Optimize power consumption through intelligent standby modes

### Retail and Demo Scenarios
1. **Warehouse Mode**: Special display mode for retail environments with looping demo content
2. **Demo Link Management**: Configure and retrieve store demonstration content
3. **Reset Capabilities**: Factory reset support for returned or refurbished devices

## API Capabilities

### Core APIs (40+ Methods)
- **Power Control**: `getPowerState`, `setPowerState`, `setNetworkStandbyMode`, `getWakeupReason`, `setWakeupSrcConfiguration`
- **Firmware Management**: `updateFirmware`, `getFirmwareUpdateInfo`, `getFirmwareUpdateState`, `getDownloadedFirmwareInfo`, `setFirmwareAutoReboot`
- **Device Information**: `getDeviceInfo`, `getSerialNumber`, `getMfgSerialNumber`, `getSystemVersions`, `getMacAddresses`
- **Configuration**: `setTimeZoneDST`, `getTimeZoneDST`, `setTerritory`, `getTerritory`, `setFriendlyName`, `getFriendlyName`
- **System Control**: `reboot`, `requestSystemUptime`, `setMode`, `setDeepSleepTimer`, `getRFCConfig`
- **Diagnostics**: `uploadLogsAsync`, `abortLogUpload`, `getBootTypeInfo`, `getBuildType`, `getLastFirmwareFailureReason`
- **Privacy**: `setOptOutTelemetry`, `isOptOutTelemetry`, `setBlocklistFlag`, `getBlocklistFlag`

### Event Notifications (15+ Events)
Real-time notifications for system state changes including power state transitions, firmware update progress, thermal threshold changes, territory updates, timezone modifications, and more.

## Integration Benefits

### For Application Developers
- **Unified Interface**: Single JSON-RPC API for all system-level operations
- **Event-Driven Architecture**: Subscribe to relevant system events for responsive applications
- **Comprehensive Documentation**: Well-documented APIs with clear parameter specifications
- **Error Handling**: Structured error codes and messages for robust error handling

### For Device Manufacturers
- **Platform Abstraction**: Hardware-agnostic interface through HAL integration
- **Customizable Features**: Conditional compilation flags for platform-specific capabilities
- **Standards Compliance**: Built on WPEFramework industry-standard plugin architecture
- **Test Framework**: Integrated L1 (unit) and L2 (integration) test support

### For Service Providers
- **Remote Management**: Full device lifecycle management capabilities
- **Operational Efficiency**: Automated firmware updates with flexible scheduling
- **Customer Support**: Remote diagnostics and log retrieval
- **Fleet Intelligence**: System-wide monitoring and analytics capabilities

## Performance Characteristics

### Resource Efficiency
- **Low Memory Footprint**: Efficient memory management with minimal runtime overhead
- **Asynchronous Operations**: Non-blocking design for firmware downloads and log uploads
- **Thread Pool Management**: Controlled threading with RAII-based lifecycle management

### Reliability
- **State Persistence**: Critical configuration persisted across reboots
- **Error Recovery**: Graceful handling of HAL failures and network interruptions
- **Input Validation**: Robust input sanitization to prevent injection attacks
- **Secure Storage**: Sensitive data stored in protected filesystem locations

### Scalability
- **Multi-Instance Support**: Can coexist with other WPEFramework plugins
- **Event Broadcasting**: Efficient notification mechanism for multiple subscribers
- **Modular Design**: Easy integration of additional features through helper framework

## Deployment Considerations

### System Requirements
- **WPEFramework**: Thunder R4.4+ compatible
- **Operating System**: Linux-based RDK platform
- **Required Services**: IARM Bus, Device Settings HAL, RFC service
- **Optional Dependencies**: Deep Sleep HAL, Thermal Monitor (for advanced features)

### Configuration
- **Plugin Configuration**: JSON-based configuration file (`SystemServices.config`)
- **Feature Flags**: Build-time flags for optional capabilities (ENABLE_THERMAL_PROTECTION, ENABLE_DEEP_SLEEP, etc.)
- **Persistent Storage**: Requires `/opt/persistent` and `/opt/secure/persistent` directories

### Security
- **Access Control**: Optional token-based authentication support
- **Secure Communication**: HTTPS support for firmware and log uploads
- **Input Sanitization**: Regex-based validation of all external inputs
- **Privilege Management**: Runs with appropriate system privileges for hardware access

## Future Roadmap Compatibility

The plugin architecture supports future enhancements including:
- Enhanced telemetry and analytics capabilities
- Additional wake source types as hardware evolves
- Extended platform capability discovery
- Integration with cloud-based device management platforms
- Advanced power optimization algorithms
- Support for A/B firmware update mechanisms

## Summary

The SystemServices plugin delivers a robust, feature-rich system management solution for RDK-based devices. Its comprehensive API coverage, reliable performance, and flexible integration options make it an essential component for consumer electronics devices, enabling manufacturers and service providers to deliver sophisticated device management capabilities while maintaining a consistent developer experience across the RDK ecosystem.
