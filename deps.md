# deps.md — Dependency Guide for entservices-systemservices

## Overview

This document helps developers understand the dependency landscape and build requirements for the [entservices-systemservices](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/CMakeLists.txt) repository, which contains the SystemServices WPEFramework (Thunder) plugin for RDK devices.

The SystemServices plugin is a [WPEFramework plugin](https://app.dosu.dev/documents/0876c9fc-522c-46fd-a497-1f1e8392b93d) that provides comprehensive system management capabilities for RDK devices: power management, firmware updates, system configuration, device diagnostics, and log management. It exposes a JSON-RPC API (callsign `org.rdk.System`) and COM-RPC interface for C++ plugins [[1]](https://app.dosu.dev/documents/0876c9fc-522c-46fd-a497-1f1e8392b93d). The plugin requires [WPEFramework R4.4+](https://app.dosu.dev/documents/0876c9fc-522c-46fd-a497-1f1e8392b93d) and integrates with multiple RDK subsystems including IARM Bus for inter-process communication, Device Settings HAL for hardware abstraction, and RFC Service for dynamic feature flag configuration [[2]](https://app.dosu.dev/documents/0876c9fc-522c-46fd-a497-1f1e8392b93d).

This document serves as a practical guide for new developers setting up a build environment, focusing on the CMake-based build system [[3]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/CMakeLists.txt#L20-L20), required and optional dependencies, and configuration flags that control feature availability.

## Build System

The SystemServices plugin uses CMake as its build system. The build produces two shared libraries: a plugin registration library and an implementation library that contains the actual functionality.

### CMake Requirements

The root CMakeLists.txt requires CMake 3.3 or higher [[3]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/CMakeLists.txt#L20-L20), while the L1 test suite requires CMake 3.8 or higher [[4]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/Tests/L1Tests/CMakeLists.txt#L19-L19). In continuous integration, the build runs on Ubuntu 22.04 with CMake 3.16.x .

### C++ Standards

The plugin and implementation libraries use C++11 . L1 tests require C++14 [[11]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/Tests/L1Tests/CMakeLists.txt#L23-L24).

### Custom Find Modules

Because many RDK dependencies do not ship CMake configuration files, the repository maintains custom Find modules in the `cmake/` directory. The root CMakeLists.txt appends this directory to `CMAKE_MODULE_PATH` [[12]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/CMakeLists.txt#L24-L26). Custom Find modules exist for IARMBus, DS (Device Settings), RFC, Curl, Telemetry, DL, and WPEFrameworkHelpers.

### Build Options

Key CMake options control which components are built:

- **PLUGIN_SYSTEMSERVICES**: Gates the plugin build by conditionally adding the `plugin` subdirectory [[13]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/CMakeLists.txt#L54-L56).
- **COMCAST_CONFIG** (default ON): When enabled, includes `services.cmake`, which adds Comcast-specific compile definitions such as `USE_IARM_BUS`, `USE_IARMBUS`, `USE_TR_69`, `HAS_API_SYSTEM`, `HAS_API_POWERSTATE`, and `ENABLE_DEEP_SLEEP` .
- **TESTBINARIES**: When enabled, adds the `Tests` directory [[17]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/CMakeLists.txt#L29-L33).
- **RDK_SERVICES_L1_TEST**: Adds the L1 test subdirectory [[18]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/CMakeLists.txt#L50-L52).
- **RDK_SERVICE_L2_TEST**: Adds the L2 test subdirectory [[19]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/CMakeLists.txt#L46-L48).
- **WPEFRAMEWORK_CREATE_IPKG_TARGETS**: Enables DEB packaging via CPack [[20]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/CMakeLists.txt#L58-L71).

Additional build flags from `services.cmake` control optional features:

- **DISABLE_GEOGRAPHY_TIMEZONE**: Disables geography and timezone features [[21]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/services.cmake#L33-L35).
- **BUILD_ENABLE_SYSTIMEMGR_SUPPORT**: Enables system time manager support [[22]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/services.cmake#L37-L40).
- **BUILD_ENABLE_THERMAL_PROTECTION**: Enables thermal protection features [[23]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/services.cmake#L42-L45).
- **BUILD_ENABLE_DEVICE_MANUFACTURER_INFO**: Enables device manufacturer information [[24]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/services.cmake#L47-L50).
- **BUILD_ENABLE_LINK_LOCALTIME**: Enables linking with local time [[25]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/services.cmake#L52-L55).

### Build Helpers

The root CMakeLists.txt includes `CmakeHelperFunctions` to provide configuration and installation helpers [[26]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/CMakeLists.txt#L44-L44). The helper functions enable consistent configuration file generation across RDK plugins.

### Library Targets

The build produces two shared libraries in the `plugin/` subdirectory:

1. **${MODULE_NAME}** (typically `WPEFrameworkSystemServices`): The plugin registration library containing plugin lifecycle and JSON-RPC interface code .

2. **${PLUGIN_IMPLEMENTATION}** (typically `WPEFrameworkSystemServicesImplementation`): The implementation library containing the actual business logic .

Both libraries install to `${CMAKE_INSTALL_PREFIX}/lib/${STORAGE_DIRECTORY}/plugins` .

### Continuous Integration

CI workflows run on Ubuntu 22.04 with both GCC and Clang compilers . The workflows use CMake 3.16.x and Ninja as the build generator. Builds execute with parallel jobs using the `-j8` flag .

For local development and full native builds, use the `ghcr.io/rdkcentral/docker-rdk-ci:latest` Docker container, which includes the complete RDK build environment.

## Key Dependencies

SystemServices requires several RDK-specific and standard libraries to build and function. This section documents each major dependency, its role, version requirements, and how CMake discovers it. All custom Find modules live in the repository's `cmake/` directory [[12]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/CMakeLists.txt#L24-L26).

### WPEFramework (Thunder)

WPEFramework (also known as Thunder) is the plugin framework that provides the JSON-RPC protocol handler, service discovery and registration, plugin lifecycle management, and COM-RPC inter-plugin communication for RDK devices [[37]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/CMakeLists.txt#L22-L22). SystemServices is implemented as a WPEFramework plugin and requires several sub-packages from the framework.

The root CMakeLists.txt invokes `find_package(WPEFramework)` without a version argument [[37]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/CMakeLists.txt#L22-L22). The plugin build then requires three additional sub-packages [[38]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/plugin/CMakeLists.txt#L25-L28):

- `${NAMESPACE}Plugins` (REQUIRED) — plugin base classes and infrastructure
- `${NAMESPACE}Definitions` (REQUIRED) — shared definitions and interfaces
- `${NAMESPACE}Helpers` (REQUIRED) — utility library with logging, file access, and string helpers

The `NAMESPACE` variable determines the target prefix and is supplied externally, typically set to `WPEFramework` or `Thunder`.

Thunder R4 compatibility is controlled by the `USE_THUNDER_R4` flag. When enabled, the TestClient links against `${NAMESPACE}COM`; otherwise it links against `${NAMESPACE}Protocols` [[39]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/plugin/TestClient/CMakeLists.txt#L20-L24) [[40]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/plugin/TestClient/CMakeLists.txt#L40-L44). R4.4+ compatibility is recommended, but no explicit version constraint appears in the CMake files.

### IARM Bus

IARM Bus (Inter Application Resource Manager Bus) is the RDK inter-process communication framework used for coordinating system events across RDK components [[41]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/cmake/FindIARMBus.cmake). SystemServices uses IARM Bus for system-wide event coordination including power state changes, firmware updates, and system manager events.

CMake discovers IARM Bus via `cmake/FindIARMBus.cmake` [[41]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/cmake/FindIARMBus.cmake), which locates:

- Library: `IARMBus` [[42]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/cmake/FindIARMBus.cmake#L28-L28)
- Headers:
  - `libIARM.h` with path suffix `rdk/iarmbus` [[43]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/cmake/FindIARMBus.cmake#L29-L29)
  - `receiverMgr.h` with path suffix `rdk/iarmmgrs/receiver` [[44]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/cmake/FindIARMBus.cmake#L30-L30)
  - `sysMgr.h` with path suffix `rdk/iarmmgrs-hal` [[45]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/cmake/FindIARMBus.cmake#L31-L31)

All three header paths are combined into `IARMBUS_INCLUDE_DIRS` [[46]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/cmake/FindIARMBus.cmake#L34-L35). The Find module sets `IARMBUS_FOUND`, `IARMBUS_INCLUDE_DIRS`, and `IARMBUS_LIBRARIES` [[47]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/cmake/FindIARMBus.cmake#L20-L22). If IARM Bus is not found, the build emits "Module IARMBus required." but continues [[48]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/plugin/CMakeLists.txt#L84-L86).

When `COMCAST_CONFIG` is enabled (the default), IARM Bus usage is controlled by compile definitions `-DUSE_IARM_BUS` and `-DUSE_IARMBUS` [[49]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/services.cmake#L22-L25).

IARM Bus requires the IARM Bus daemon to be running at runtime.

### Device Settings (DS)

Device Settings provides the Hardware Abstraction Layer for display and audio settings on RDK devices. SystemServices uses DS to query and control display and audio hardware capabilities [[50]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/plugin/CMakeLists.txt#L93-L102).

CMake discovers DS via `cmake/FindDS.cmake` [[51]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/cmake/FindDS.cmake), which locates:

- Libraries: `ds` (display settings), `dshalcli` (DS HAL client) [[52]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/cmake/FindDS.cmake#L28-L29) [[53]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/cmake/FindDS.cmake#L36-L36)
- Headers:
  - `manager.hpp` with path suffix `rdk/ds` [[54]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/cmake/FindDS.cmake#L32-L32)
  - `dsTypes.h` with path suffix `rdk/halif/ds-hal` [[55]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/cmake/FindDS.cmake#L33-L33)
  - `dsMgr.h` with path suffix `rdk/ds-rpc` [[56]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/cmake/FindDS.cmake#L34-L34)

The Find module also searches for `ds-hal` and `IARMBus` libraries, but only `ds` and `dshalcli` are added to `DS_LIBRARIES` [[53]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/cmake/FindDS.cmake#L36-L36).

When DS is found, the build adds the `-DDS_FOUND` compile definition [[57]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/plugin/CMakeLists.txt#L96-L96). DS is optional; the build continues without it [[58]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/plugin/CMakeLists.txt#L100-L101).

### RFC (Remote Feature Control)

RFC is RDK's dynamic feature flag system that allows runtime query and modification of feature flags without firmware updates. SystemServices uses RFC for persisting device name, querying feature flags, and reading runtime configuration.

CMake discovers RFC via `cmake/FindRFC.cmake` [[59]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/cmake/FindRFC.cmake), which locates:

- Library: `rfcapi` [[60]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/cmake/FindRFC.cmake#L9-L9)
- Headers:
  - `rfcapi.h` (main API) [[61]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/cmake/FindRFC.cmake#L10-L10)
  - `wdmp-c.h` with path suffix `wdmp-c` (WARP Device Management Protocol C bindings) [[62]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/cmake/FindRFC.cmake#L11-L11)

The Find module combines both header paths into `RFC_INCLUDE_DIRS` [[63]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/cmake/FindRFC.cmake#L13-L13) and sets `RFC_FOUND` and `RFC_LIBRARIES` [[64]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/cmake/FindRFC.cmake#L16-L16).

If RFC is not found, the build emits "RFC lib required." [[65]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/plugin/CMakeLists.txt#L117-L117). No explicit version constraint is specified.

### libcurl

libcurl is a client-side URL transfer library for HTTP and HTTPS communication. SystemServices uses libcurl for log uploads to backend servers and tracking firmware download progress via HTTP.

CMake discovers libcurl via `cmake/FindCurl.cmake` [[66]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/cmake/FindCurl.cmake), a custom lightweight Find module that locates only the library via `find_library(CURL_LIBRARY NAMES curl)` [[67]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/cmake/FindCurl.cmake#L22-L22). The module does not search for headers.

The Find module sets `CURL_LIBRARY` [[68]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/cmake/FindCurl.cmake#L24-L24). If libcurl is not found, the build emits "Curl/libcurl required." [[69]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/plugin/CMakeLists.txt#L109-L109). No version constraint is specified.

### libprocps

libprocps provides functions for reading process and system information from the `/proc` filesystem. SystemServices uses it to retrieve process and system information for diagnostics and monitoring.

Discovery uses a direct `find_library(PROCPS_LIBRARIES NAMES procps)` call without a custom Find module [[70]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/plugin/CMakeLists.txt#L29-L29). The library is linked conditionally only if found [[71]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/plugin/CMakeLists.txt#L89-L91). No version constraint is specified.

### Telemetry (Optional)

The RDK telemetry message sender library emits telemetry events from SystemServices. This dependency is purely optional.

CMake discovers Telemetry via `cmake/FindTelemetry.cmake` [[72]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/cmake/FindTelemetry.cmake), which locates:

- Library: `telemetry_msgsender` [[73]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/cmake/FindTelemetry.cmake#L26-L26)
- Header: `telemetry_busmessage_sender.h` [[74]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/cmake/FindTelemetry.cmake#L27-L27)

The Find module sets `TELEMETRY_FOUND`, `TELEMETRY_INCLUDE_DIRS`, and `TELEMETRY_LIBRARIES` [[75]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/cmake/FindTelemetry.cmake#L20-L22). When found, Telemetry is linked to the `MODULE_NAME` target (the plugin interface library), not the implementation [[76]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/plugin/CMakeLists.txt#L131-L133). No warning is emitted if Telemetry is absent.

### libdl (Dynamic Linking)

libdl is the POSIX dynamic linking library. CMake discovers it via `cmake/FindDL.cmake` [[77]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/cmake/FindDL.cmake), which uses a simple `find_library(DL_LIBRARIES NAMES dl)` call [[78]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/cmake/FindDL.cmake#L22-L22).

## WPEFramework Helper Utilities

WPEFrameworkHelpers is a shared library that ships with WPEFramework, providing reusable utility functions used throughout SystemServices. These utilities are implemented externally in the WPEFramework ecosystem, not in the SystemServices repository [[79]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/plugin/SystemServicesHelper.cpp#L20-L35).

### Discovery and Linking

The CMake Find module `cmake/FindWPEFrameworkHelpers.cmake` discovers the library and headers:

- Library: `WPEFrameworkHelpers` in the `wpeframework/plugins` path suffix [[80]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/cmake/FindWPEFrameworkHelpers.cmake#L10-L12)
- Headers: located in the `wpeframework/helpers` path suffix, with `UtilsLogging.h` used as the discovery header [[81]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/cmake/FindWPEFrameworkHelpers.cmake#L14-L16)
- Creates an imported target `WPEFramework::WPEFrameworkHelpers` with the library location and include directories [[82]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/cmake/FindWPEFrameworkHelpers.cmake#L29-L33)

Both the plugin module and implementation library link against `${NAMESPACE}::${NAMESPACE}Helpers` [[83]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/plugin/CMakeLists.txt#L137-L138). The plugin build requires `find_package(${NAMESPACE}Helpers REQUIRED)` [[84]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/plugin/CMakeLists.txt#L28).

### Utility Headers

SystemServices uses the following WPEFrameworkHelpers utility headers:

**UtilsLogging.h** — Provides logging macros `LOGINFO`, `LOGWARN`, and `LOGERR` [[85]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/plugin/SystemServicesHelper.cpp#L30-L30). These macros are used extensively throughout `SystemServicesHelper.cpp` [[86]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/plugin/SystemServicesHelper.cpp#L127-L134) and `uploadlogs.cpp` [[87]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/plugin/uploadlogs.cpp#L33-L33) for diagnostic output.

**UtilsfileExists.h** — Provides `Utils::fileExists()` to check file existence before reading [[88]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/plugin/SystemServicesHelper.cpp#L31-L31). Used in `SystemServicesHelper.cpp` to verify files exist before attempting to read them, such as checking for configuration files and preference files .

**UtilsgetFileContent.h** — Provides `Utils::readFileContent()` to read file contents into a string [[91]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/plugin/SystemServicesHelper.cpp#L34-L34). Used in download progress tracking to read firmware download status from files [[92]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/plugin/SystemServicesHelper.cpp#L307-L308).

**UtilsString.h** — Provides string processing utilities including trimming, whitespace collapse, and string splitting [[93]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/plugin/SystemServicesHelper.cpp#L33-L33). Used when parsing download progress files to extract percentage values [[94]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/plugin/SystemServicesHelper.cpp#L317-L327).

**UtilsCStr.h** — Provides C-string manipulation helpers [[95]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/plugin/uploadlogs.cpp#L32-L32). Used in `uploadlogs.cpp` for log upload functionality [[96]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/plugin/uploadlogs.cpp#L27-L35).

### Test Framework Helpers

L1 tests add an additional include path for test-specific utilities: `${CMAKE_SOURCE_DIR}/../entservices-helpers/helpers` [[97]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/Tests/L1Tests/CMakeLists.txt#L105). This provides access to helpers from the `entservices-helpers` repository during test builds.

## Optional Feature Dependencies

SystemServices supports several optional compile-time features controlled by build flags. These features are conditionally compiled and enable specific hardware abstraction layers (HAL) or functional modules.

### Comcast Configuration

The `COMCAST_CONFIG` CMake option (default `ON`) includes [`services.cmake`](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/services.cmake) which defines Comcast-specific compile definitions [[14]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/CMakeLists.txt#L35-L38). Test builds in CI disable this option (`COMCAST_CONFIG=OFF`) to allow manual control of individual flags [[98]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/.github/workflows/L1-tests.yml).

### Core Feature Flags

The following flags are always enabled when `COMCAST_CONFIG=ON`:

- **`USE_IARM_BUS` / `USE_IARMBUS`**: Enable IARM Bus integration for RDK inter-process communication [[49]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/services.cmake#L22-L25).
- **`USE_TR_69`**: Enable TR-069 support [[99]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/services.cmake#L27).
- **`HAS_API_SYSTEM` / `HAS_API_POWERSTATE`**: Enable system and power state APIs [[100]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/services.cmake#L29-L30).
- **`ENABLE_DEEP_SLEEP`**: Always enabled; requires Deep Sleep Manager HAL at runtime [[101]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/services.cmake#L57).

### Optional HAL Features

#### Thermal Protection

- **Build flag**: `BUILD_ENABLE_THERMAL_PROTECTION`
- **Compile definitions**: `-DBUILD_ENABLE_THERMAL_PROTECTION -DENABLE_THERMAL_PROTECTION` [[102]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/services.cmake#L42-L44)
- **Purpose**: Enables `thermonitor.cpp` thermal monitoring code
- **Runtime**: Requires thermal monitor HAL

#### System Time Manager

- **Build flag**: `BUILD_ENABLE_SYSTIMEMGR_SUPPORT`
- **Compile definition**: `-DENABLE_SYSTIMEMGR_SUPPORT` [[103]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/services.cmake#L37-L39)
- **Purpose**: Enables system time manager integration
- **Test builds**: Always enabled for L1 and L2 test builds [[104]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/plugin/CMakeLists.txt#L141-L149)

#### Manufacturing (MFR) HAL

- **Build flag**: `BUILD_ENABLE_DEVICE_MANUFACTURER_INFO`
- **Compile definition**: `-DENABLE_DEVICE_MANUFACTURER_INFO` [[105]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/services.cmake#L47-L49)
- **Purpose**: Enables device manufacturer information APIs via MFR HAL

#### Link Localtime

- **Build flag**: `BUILD_ENABLE_LINK_LOCALTIME`
- **Compile definition**: `-DENABLE_LINK_LOCALTIME` [[106]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/services.cmake#L52-L54)
- **Purpose**: Enables symbolic linking for localtime configuration

#### Geography-Based Timezone

- **Build flag**: `DISABLE_GEOGRAPHY_TIMEZONE`
- **Compile definition**: `-DDISABLE_GEOGRAPHY_TIMEZONE` [[107]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/services.cmake#L33-L34)
- **Purpose**: Disables geography-based timezone features when set to `ON`

### Test Build Flags

The following flags control test binary generation:

- **`TESTBINARIES`**: Adds Tests directory to the build [[108]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/CMakeLists.txt#L29-L32).
- **`RDK_SERVICES_L1_TEST`**: Adds Tests/L1Tests directory to the build [[18]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/CMakeLists.txt#L50-L52).
- **`RDK_SERVICE_L2_TEST`**: Adds Tests/L2Tests directory to the build; requires TestMocklib library [[19]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/CMakeLists.txt#L46-L48) [[109]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/plugin/CMakeLists.txt#L120-L128).

## Version Constraints Summary

The table below summarizes the known version requirements and constraints for all dependencies. Note that most RDK-specific libraries (IARMBus, DS, RFC, etc.) do not carry explicit version constraints in the CMake build files; they are resolved through the platform's RDK sysroot.

| Dependency | Minimum Version | Notes |
|---|---|---|
| **CMake** | 3.3 (root), 3.8 (L1 tests) | CI standardizes on 3.16.x [[3]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/CMakeLists.txt#L20-L20) [[4]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/Tests/L1Tests/CMakeLists.txt#L19-L19) |
| **C++ Standard** | C++11 (plugin), C++14 (L1 tests) | Set via `CXX_STANDARD` in each target [[9]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/plugin/CMakeLists.txt#L42-L43) |
| **WPEFramework / Thunder** | R4.4+ recommended | `USE_THUNDER_R4=ON` selects R4 code paths; no version pinned in CMake [[39]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/plugin/TestClient/CMakeLists.txt#L20-L24) |
| **IARM Bus** | No explicit constraint | Resolved from RDK sysroot; `IARMBUS_FOUND` required for full functionality [[110]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/cmake/FindIARMBus.cmake#L38-L38) |
| **Device Settings (DS)** | No explicit constraint | Optional; build proceeds without it; `DS_FOUND` gates DS-dependent code [[111]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/cmake/FindDS.cmake#L44-L44) |
| **RFC (rfcapi)** | No explicit constraint | Emits warning if absent; required for feature-flag APIs [[64]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/cmake/FindRFC.cmake#L16-L16) |
| **libcurl** | No explicit constraint | Emits warning if absent; required for log upload and firmware progress [[67]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/cmake/FindCurl.cmake#L22-L22) |
| **libprocps** | No explicit constraint | Optional; linked only when found [[70]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/plugin/CMakeLists.txt#L29-L29) |
| **Telemetry** | No explicit constraint | Fully optional; no warning if absent [[112]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/cmake/FindTelemetry.cmake#L30-L30) |
| **OS (CI)** | Ubuntu 22.04 | For CI/test builds; production targets use the RDK docker image [[98]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/.github/workflows/L1-tests.yml) |
| **Compiler (CI)** | GCC or Clang | Both tested in CI matrix [[98]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/.github/workflows/L1-tests.yml) |

## Build Requirements

### Toolchain

The SystemServices plugin requires CMake 3.3 or higher for the root build [[3]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/CMakeLists.txt#L20-L20). If building L1 tests, CMake 3.8 or higher is required [[4]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/Tests/L1Tests/CMakeLists.txt#L19-L19). The CI environment uses CMake 3.16.x [[113]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/.github/workflows/L1-tests.yml#L82-L83).

The codebase requires a C++ compiler with C++11 support for the plugin and implementation code [[9]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/plugin/CMakeLists.txt#L42-L43). Test code requires C++14 [[11]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/Tests/L1Tests/CMakeLists.txt#L23-L24).

CI builds run on Ubuntu 22.04 with both GCC and Clang compilers [[33]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/.github/workflows/L1-tests.yml#L25-L28) [[34]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/.github/workflows/L2-tests.yml#L22-L25). Ninja is used as the build tool for parallel compilation with `-j8` flag [[114]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/.github/workflows/L1-tests.yml#L160-L162).

### Required Dependencies

A full plugin build requires the following packages:

**WPEFramework (Thunder)** — The plugin framework providing JSON-RPC and COM-RPC interfaces. Multiple sub-packages are required: `${NAMESPACE}Plugins`, `${NAMESPACE}Definitions`, and `${NAMESPACE}Helpers` [[38]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/plugin/CMakeLists.txt#L25-L28). The NAMESPACE variable must match your WPEFramework installation (typically `WPEFramework` or `Thunder`). Use `USE_THUNDER_R4=ON` for Thunder R4 compatibility, which is required in modern builds [[115]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/.github/workflows/L1-tests.yml#L481-L481).

**IARMBus** — Inter Application Resource Manager Bus for RDK inter-process communication [[116]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/plugin/CMakeLists.txt#L79-L87). Headers are discovered in `rdk/iarmbus`, `rdk/iarmmgrs/receiver`, and `rdk/iarmmgrs-hal`. The build emits "Module IARMBus required." if not found but continues [[117]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/plugin/CMakeLists.txt#L84-L84).

**libcurl** — Client-side URL transfer library for HTTP/HTTPS communication [[118]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/plugin/CMakeLists.txt#L104-L110). The build emits "Curl/libcurl required." if not found [[69]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/plugin/CMakeLists.txt#L109-L109).

**RFC** — Remote Feature Control library (`rfcapi`) for dynamic feature flag management [[119]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/plugin/CMakeLists.txt#L112-L118). The build emits "RFC lib required." if not found [[65]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/plugin/CMakeLists.txt#L117-L117).

**WPEFrameworkHelpers** — Utility library providing logging macros (`LOGINFO`, `LOGWARN`, `LOGERR`), file operations (`fileExists`, `readFileContent`), and string helpers. Linked via `${NAMESPACE}::${NAMESPACE}Helpers` [[83]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/plugin/CMakeLists.txt#L137-L138).

### Optional Dependencies

**Device Settings (DS)** — Hardware abstraction layer for display and audio device settings [[50]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/plugin/CMakeLists.txt#L93-L102). When found, adds `-DDS_FOUND` and links libraries `ds`, `dshalcli`, and requires headers in `rdk/ds`, `rdk/halif/ds-hal`, and `rdk/ds-rpc`.

**libprocps** — Library for reading process information from `/proc` [[70]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/plugin/CMakeLists.txt#L29-L29). Linked conditionally if found [[71]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/plugin/CMakeLists.txt#L89-L91).

**Telemetry** — RDK telemetry message sender library (`telemetry_msgsender`). Purely optional; linked to the plugin module when found [[120]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/plugin/CMakeLists.txt#L131-L134).

### Test Dependencies

For L1 and L2 test builds, additional requirements include:

- **entservices-testframework** repository providing mocks for platform interfaces [[121]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/Tests/README.md#L4-L5)
- **entservices-helpers** repository for helper includes [[122]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/Tests/L1Tests/CMakeLists.txt#L105-L105)
- **GTest/GMock** framework (Google Test v1.15.0 in CI)
- **TestMocklib** for L2 tests [[123]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/plugin/CMakeLists.txt#L120-L129)
- **act** tool for local GitHub Actions execution [[124]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/Tests/README.md#L27-L31)

### CMake Configuration Options

Key CMake options to set when building:

- `-DPLUGIN_SYSTEMSERVICES=ON` — Enables the plugin build [[13]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/CMakeLists.txt#L54-L56)
- `-DUSE_THUNDER_R4=ON` — Enables Thunder R4 compatibility (required for modern builds)
- `-DCOMCAST_CONFIG=ON` — Enables Comcast service definitions via `services.cmake` (default ON) [[14]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/CMakeLists.txt#L35-L38)
- `-DNAMESPACE=WPEFramework` or `-DNAMESPACE=Thunder` — Must match your WPEFramework installation
- `-DRDK_SERVICES_L1_TEST=ON` — Enables L1 test build [[18]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/CMakeLists.txt#L50-L52)
- `-DRDK_SERVICE_L2_TEST=ON` — Enables L2 test build [[19]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/CMakeLists.txt#L46-L48)

When `COMCAST_CONFIG=ON` (default), the build includes `services.cmake`, which sets compile definitions including `USE_IARM_BUS`, `USE_IARMBUS`, `HAS_API_SYSTEM`, `HAS_API_POWERSTATE`, and `ENABLE_DEEP_SLEEP` [[15]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/services.cmake#L22-L30) [[16]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/services.cmake#L57-L57).

### Production Build Environment

Full production builds should use the RDK CI Docker container image `ghcr.io/rdkcentral/docker-rdk-ci:latest`, which pre-installs all RDK platform dependencies [[98]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/.github/workflows/L1-tests.yml). This container provides WPEFramework, IARMBus, Device Settings, RFC, and other RDK-specific libraries in standard system paths, eliminating manual dependency installation.

## Installation and Setup

Dependencies for entservices-systemservices fall into three categories: WPEFramework/Thunder components provided by the RDK build infrastructure, RDK platform libraries installed to the sysroot, and standard system libraries. The installation method differs by category and development context.

### WPEFramework and Thunder

WPEFramework (Thunder) and its components are provided by the RDK build infrastructure. CI builds check out Thunder and ThunderTools at pinned commit SHAs [[98]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/.github/workflows/L1-tests.yml) and build them from source as part of the pipeline.

For development, use the `ghcr.io/rdkcentral/docker-rdk-ci:latest` container, which includes all required RDK build tooling.

### RDK Platform Libraries

IARMBus, Device Settings (DS), and RFC are installed to the sysroot by the RDK platform build, which uses Yocto/OpenEmbedded. Headers must be present at the expected path suffixes discovered by the custom CMake Find modules:

- **IARMBus**: `rdk/iarmbus`, `rdk/iarmmgrs/receiver`, `rdk/iarmmgrs-hal`
- **Device Settings**: `rdk/ds`, `rdk/halif/ds-hal`, `rdk/ds-rpc`
- **RFC**: `rfcapi.h`, `wdmp-c.h` with `wdmp-c` suffix

Libraries must be discoverable by CMake's `find_library` in standard system paths or via `CMAKE_PREFIX_PATH`.

### Standard System Libraries

libcurl and libprocps are available from standard Linux package managers:

```sh
apt install libcurl4-openssl-dev libprocps-dev
```

On RDK targets, these are pre-installed in the sysroot.

### WPEFrameworkHelpers

WPEFrameworkHelpers ships with the WPEFramework package. Headers are installed to `wpeframework/helpers` and the library to `wpeframework/plugins` [[125]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/cmake/FindWPEFrameworkHelpers.cmake#L11-L16). The custom Find module `cmake/FindWPEFrameworkHelpers.cmake` handles discovery [[126]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/cmake/FindWPEFrameworkHelpers.cmake).

### Test Builds

To build and run L1 or L2 tests locally:

1. Clone the testframework repository at version 2.0.0 or the develop branch alongside this repo:

   ```sh
   git clone -b 2.0.0 https://github.com/rdkcentral/entservices-testframework.git
   ```

   [[127]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/Tests/README.md)

2. Clone entservices-helpers at the develop branch alongside this repo:

   ```sh
   git clone -b develop https://github.com/rdkcentral/entservices-helpers.git
   ```

   [[97]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/Tests/L1Tests/CMakeLists.txt#L105)

3. Tests expect these repositories at `../entservices-testframework` and `../entservices-helpers` relative to this repository's root.

4. Install [nektos/act](https://github.com/nektos/act) for local GitHub Actions execution:

   ```sh
   curl -SL https://raw.githubusercontent.com/nektos/act/master/install.sh | bash
   ```

   [[128]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/Tests/README.md#L27-L28)

5. Run tests:

   ```sh
   ./bin/act -W .github/workflows/tests-trigger.yml -s GITHUB_TOKEN=<your_access_token>
   ```

   [[129]](https://github.com/rdkcentral/entservices-systemservices/blob/eb237b046390d9c3bb3acca095f8b0b657037c8c/Tests/README.md#L31)

### Runtime Dependencies

The following services must be running for SystemServices to function:

- IARM Bus daemon
- SysMgr (System Manager)
- RFC service
- Device Settings service

### Non-Standard Paths

The custom Find modules in the `cmake/` directory search standard system paths. If dependencies are installed to non-standard locations, set `CMAKE_PREFIX_PATH` or `PKG_CONFIG_PATH` accordingly.
