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
find_library(PROCPS_LIBRARIES NAMES procps)

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

include_directories(
        ../helpers)

target_link_libraries(${MODULE_NAME}
    PRIVATE
        CompileSettingsDebug::CompileSettingsDebug
        ${NAMESPACE}Plugins::${NAMESPACE}Plugins
        ${NAMESPACE}Definitions::${NAMESPACE}Definitions)

install(TARGETS ${MODULE_NAME}
    DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/${STORAGE_DIRECTORY}/plugins)

add_library(${PLUGIN_IMPLEMENTATION} SHARED
        SystemServicesImplementation.cpp
        Module.cpp
        cTimer.cpp
        thermonitor.cpp
        SystemServicesHelper.cpp
        uploadlogs.cpp
        platformcaps/platformcaps.cpp
        platformcaps/platformcapsdata.cpp
        platformcaps/platformcapsdatarpc.cpp
        )
set_source_files_properties(SystemServicesImplementation.cpp thermonitor.cpp platformcaps/platformcapsdata.cpp PROPERTIES COMPILE_FLAGS "-fexceptions")
set_target_properties(${PLUGIN_IMPLEMENTATION} PROPERTIES
        CXX_STANDARD 11
        CXX_STANDARD_REQUIRED YES)

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

target_include_directories(${PLUGIN_IMPLEMENTATION} PRIVATE ../helpers)
target_include_directories(${PLUGIN_IMPLEMENTATION} PRIVATE ./)

install(TARGETS ${PLUGIN_IMPLEMENTATION}
        DESTINATION lib/${STORAGE_DIRECTORY}/plugins)

write_config(${PLUGIN_NAME})

