# If not stated otherwise in this file or this component's license file the
# following copyright and licenses apply:
#
# Copyright 2020 RDK Management
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

set(PLUGIN_NAME SystemServices)
set(MODULE_NAME ${NAMESPACE}${PLUGIN_NAME})
set(PLUGIN_IMPLEMENTATION ${MODULE_NAME}Implementation)

set(PLUGIN_SYSTEM_AUTOSTART false CACHE STRING "To automatically start System plugin.")
set(PLUGIN_SYSTEMSERVICE_STARTUPORDER "" CACHE STRING "To configure startup order of SystemServices plugin")

find_package(${NAMESPACE}Plugins REQUIRED)
find_package(${NAMESPACE}Definitions REQUIRED)
find_package(CompileSettingsDebug CONFIG REQUIRED)
find_package(${NAMESPACE}Helpers REQUIRED)
find_library(PROCPS_LIBRARIES NAMES procps)
find_library(SECURE_WRAPPER_LIBRARIES NAMES secure_wrapper)

find_package(Telemetry)

if(BUILD_TESTS)
    add_subdirectory(TestClient)
endif()

add_library(${MODULE_NAME} SHARED
        SystemServices.cpp
        Module.cpp)

set_target_properties(${MODULE_NAME} PROPERTIES
        CXX_STANDARD 11
        CXX_STANDARD_REQUIRED YES)

target_link_libraries(${MODULE_NAME}
    PRIVATE
        CompileSettingsDebug::CompileSettingsDebug
        ${NAMESPACE}Plugins::${NAMESPACE}Plugins
        ${NAMESPACE}Definitions::${NAMESPACE}Definitions)

install(TARGETS ${MODULE_NAME}
    DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/${STORAGE_DIRECTORY}/plugins)

option(USE_DEVICESETTING_PLUGIN
    "Enable COM-RPC integration with entservices-devicesettings for SystemServices (replaces libds/IARM device:: calls)"
    ON)

if (USE_DEVICESETTING_PLUGIN)
    # ── DS_COMRPC/ path ───────────────────────────────────────────────────────
    # COM-RPC to entservices-devicesettings (IDeviceSettingsHost / IDeviceSettingsAudio).
    # Does NOT link libds.so or libdshalcli.so.
    set(SS_DS_IMPL_SRCS
        DS_COMRPC/SystemServicesImplementation.cpp
        DS_COMRPC/platformcapsdata.cpp)
    set(SS_DS_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/DS_COMRPC)
    message(STATUS "SystemServices: USE_DEVICESETTING_PLUGIN=ON — DS_COMRPC/ path (no libds)")
else()
    # ── DS_IARM/ path ─────────────────────────────────────────────────────────
    # IARM / libds path using device:: host APIs directly.
    set(SS_DS_IMPL_SRCS
        DS_IARM/SystemServicesImplementation.cpp
        DS_IARM/platformcapsdata.cpp)
    set(SS_DS_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/DS_IARM)
    message(STATUS "SystemServices: USE_DEVICESETTING_PLUGIN=OFF — DS_IARM/ path (libds/IARM)")
endif() # USE_DEVICESETTING_PLUGIN

add_library(${PLUGIN_IMPLEMENTATION} SHARED
        ${SS_DS_IMPL_SRCS}
        Module.cpp
        cTimer.cpp
        thermonitor.cpp
        SystemServicesHelper.cpp
        uploadlogs.cpp
        platformcaps/platformcaps.cpp
        platformcaps/platformcapsdatarpc.cpp
        )
set_source_files_properties(${SS_DS_IMPL_SRCS} thermonitor.cpp PROPERTIES COMPILE_FLAGS "-fexceptions")
set_target_properties(${PLUGIN_IMPLEMENTATION} PROPERTIES
        CXX_STANDARD 11
        CXX_STANDARD_REQUIRED YES)

# DS-path-specific compile definitions and link libraries (target now exists)
if (USE_DEVICESETTING_PLUGIN)
    find_package(${NAMESPACE}Definitions REQUIRED)
    find_package(${NAMESPACE}Helpers REQUIRED)
    target_compile_definitions(${PLUGIN_IMPLEMENTATION} PRIVATE USE_DEVICESETTING_PLUGIN)
    target_link_libraries(${PLUGIN_IMPLEMENTATION} PRIVATE
        ${NAMESPACE}Definitions::${NAMESPACE}Definitions
        ${NAMESPACE}::${NAMESPACE}Helpers)
endif()

target_compile_definitions(${PLUGIN_IMPLEMENTATION} PRIVATE MODULE_NAME=Plugin_${PLUGIN_NAME})

if (NOT RDK_SERVICES_L1_TEST)
    target_compile_options(${PLUGIN_IMPLEMENTATION} PRIVATE -Wno-error)
endif ()

list(APPEND CMAKE_MODULE_PATH
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/")

find_package(IARMBus)
if (IARMBus_FOUND)
    target_include_directories(${PLUGIN_IMPLEMENTATION} PRIVATE ${IARMBUS_INCLUDE_DIRS})
    target_link_libraries(${PLUGIN_IMPLEMENTATION} PRIVATE ${NAMESPACE}Plugins::${NAMESPACE}Plugins ${IARMBUS_LIBRARIES})
else (IARMBus_FOUND)
    message ("Module IARMBus required.")
    target_include_directories(${PLUGIN_IMPLEMENTATION} PRIVATE ${IARMBUS_INCLUDE_DIRS})
    target_link_libraries(${PLUGIN_IMPLEMENTATION} PRIVATE ${NAMESPACE}Plugins::${NAMESPACE}Plugins ${IARMBUS_LIBRARIES})
endif(IARMBus_FOUND)

if(PROCPS_LIBRARIES)
    target_link_libraries(${PLUGIN_IMPLEMENTATION} PRIVATE ${PROCPS_LIBRARIES})
endif(PROCPS_LIBRARIES)

if(SECURE_WRAPPER_LIBRARIES)
    target_link_libraries(${PLUGIN_IMPLEMENTATION} PRIVATE ${SECURE_WRAPPER_LIBRARIES})
endif(SECURE_WRAPPER_LIBRARIES)

find_package(DS)
if (DS_FOUND)
	find_package(IARMBus)
    add_definitions(-DDS_FOUND)
    target_include_directories(${PLUGIN_IMPLEMENTATION} PRIVATE ${IARMBUS_INCLUDE_DIRS})
    target_include_directories(${PLUGIN_IMPLEMENTATION} PRIVATE ${DS_INCLUDE_DIRS})
    target_link_libraries(${PLUGIN_IMPLEMENTATION} PRIVATE ${NAMESPACE}Plugins::${NAMESPACE}Plugins ${IARMBUS_LIBRARIES} ${DS_LIBRARIES})
else (DS_FOUND)
    target_link_libraries(${PLUGIN_IMPLEMENTATION} PRIVATE ${NAMESPACE}Plugins::${NAMESPACE}Plugins)
endif(DS_FOUND)

find_package(CURL)
if (CURL_FOUND)
	include_directories(${CURL_INCLUDE_DIRS})
	target_link_libraries(${PLUGIN_IMPLEMENTATION} PRIVATE ${CURL_LIBRARIES})
else (CURL_FOUND)
	message ("Curl/libcurl required.")
endif (CURL_FOUND)

find_package(RFC)
if (RFC_FOUND)
	target_include_directories(${PLUGIN_IMPLEMENTATION} PRIVATE ${RFC_INCLUDE_DIRS})
	target_link_libraries(${PLUGIN_IMPLEMENTATION} PRIVATE ${RFC_LIBRARIES})
else (RFC_FOUND)
	message ("RFC lib required.")
endif (RFC_FOUND)

find_library(UPLOADSTBLOGS_LIBRARIES NAMES uploadstblogs)
if (UPLOADSTBLOGS_LIBRARIES)
    find_path(UPLOADSTBLOGS_INCLUDE_DIRS NAMES uploadstblogs.h PATH_SUFFIXES uploadstblogs)
    if (UPLOADSTBLOGS_INCLUDE_DIRS)
        target_include_directories(${PLUGIN_IMPLEMENTATION} PRIVATE ${UPLOADSTBLOGS_INCLUDE_DIRS})
    endif()
    target_link_libraries(${PLUGIN_IMPLEMENTATION} PRIVATE ${UPLOADSTBLOGS_LIBRARIES})
else (UPLOADSTBLOGS_LIBRARIES)
    message ("uploadstblogs library required.")
endif (UPLOADSTBLOGS_LIBRARIES)

if (RDK_SERVICE_L2_TEST)
    message ("L2 test Enabled")
    find_library(TESTMOCKLIB_LIBRARIES NAMES TestMocklib)
    if (TESTMOCKLIB_LIBRARIES)
        message ("linking mock libraries ${TESTMOCKLIB_LIBRARIES} library")
        target_link_libraries(${PLUGIN_IMPLEMENTATION} PRIVATE ${TESTMOCKLIB_LIBRARIES})
    else (TESTMOCKLIB_LIBRARIES)
        message ("Require ${TESTMOCKLIB_LIBRARIES} library")
    endif (TESTMOCKLIB_LIBRARIES)
endif()

if(TELEMETRY_FOUND)
    target_link_libraries(${MODULE_NAME} PRIVATE ${TELEMETRY_LIBRARIES})
    target_include_directories(${MODULE_NAME} PRIVATE ${TELEMETRY_INCLUDE_DIRS})
endif()

target_include_directories(${PLUGIN_IMPLEMENTATION} PRIVATE ./
    ${SS_DS_INCLUDE_DIR})
target_link_libraries(${MODULE_NAME} PRIVATE ${NAMESPACE}::${NAMESPACE}Helpers)
target_link_libraries(${PLUGIN_IMPLEMENTATION} PRIVATE ${NAMESPACE}::${NAMESPACE}Helpers)

#Add mock headers and SYSTIMEMGR support for L1 and L2 tests only
if (RDK_SERVICES_L1_TEST OR RDK_SERVICE_L2_TEST)
    message ("Test build enabled - adding mock include directories and SYSTIMEMGR support")
    if (EXISTS ${CMAKE_SOURCE_DIR}/../entservices-testframework/Tests/mocks)
        target_include_directories(${PLUGIN_IMPLEMENTATION} PRIVATE 
            ${CMAKE_SOURCE_DIR}/../entservices-testframework/Tests/mocks)
        message ("Mock path: ${CMAKE_SOURCE_DIR}/../entservices-testframework/Tests/mocks")
    endif()
    # Enable SYSTIMEMGR support for tests
    target_compile_definitions(${PLUGIN_IMPLEMENTATION} PRIVATE ENABLE_SYSTIMEMGR_SUPPORT)
endif()

install(TARGETS ${PLUGIN_IMPLEMENTATION}
        DESTINATION lib/${STORAGE_DIRECTORY}/plugins)

write_config(${PLUGIN_NAME})

