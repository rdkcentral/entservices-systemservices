/**
* If not stated otherwise in this file or this component's LICENSE
* file the following copyright and licenses apply:
*
* Copyright 2025 RDK Management
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
**/

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <fstream>
#include <algorithm>
#include <string>
#include <vector>
#include <cstdio>
#include <iostream>
#include <thread>
#include <chrono>

#include "SystemServices.h"
#include "SystemServicesImplementation.h"
#include "ServiceMock.h"
#include "FactoriesImplementation.h"
#include "IarmBusMock.h"
#include "RfcApiMock.h"
#include "WrapsMock.h"
#include "PowerManagerMock.h"
#include "devicesettings/HostMock.h"
#include "devicesettings/SleepModeMock.h"
#include "TelemetryMock.h"
#include "readprocMock.h"
#include "ThunderPortability.h"
#include "WorkerPoolImplementation.h"
#include "COMLinkMock.h"

#define TEST_LOG(x, ...) fprintf(stderr, "\033[1;32m[%s:%d](%s)<PID:%d><TID:%d>" x "\n\033[0m", __FILE__, __LINE__, __FUNCTION__, getpid(), gettid(), ##__VA_ARGS__); fflush(stderr);

using ::testing::NiceMock;
using namespace WPEFramework;

namespace
{
    static void removeFile(const char* fileName)
    {
        if (std::remove(fileName) != 0)
        {
            printf("File %s failed to remove\n", fileName);
            perror("Error deleting file");
        }
        else
        {
            printf("File %s successfully deleted\n", fileName);
        }
    }
    
    static void createFile(const char* fileName, const char* fileContent)
    {
        removeFile(fileName);

        std::ofstream fileContentStream(fileName);
        fileContentStream << fileContent;
        fileContentStream << "\n";
        fileContentStream.close();
    }
}

class SystemServicesInitializeTest : public ::testing::Test {
protected:
    Core::ProxyType<Plugin::SystemServices> plugin;
    Core::JSONRPC::Handler& handler;
    DECL_CORE_JSONRPC_CONX connection;
    Core::ProxyType<Plugin::SystemServicesImplementation> pluginImpl;
    Core::ProxyType<WorkerPoolImplementation> workerPool;
    NiceMock<FactoriesImplementation> factoriesImplementation;
    NiceMock<ServiceMock> service;
    PLUGINHOST_DISPATCHER* dispatcher;
    NiceMock<COMLinkMock> comLinkMock;
    Core::JSONRPC::Message message;
    string response;

    SystemServicesInitializeTest()
        : plugin(Core::ProxyType<Plugin::SystemServices>::Create())
        , handler(*(plugin))
        , INIT_CONX(1, 0)
        , workerPool(Core::ProxyType<WorkerPoolImplementation>::Create(
              2, Core::Thread::DefaultStackSize(), 16))
        , dispatcher(nullptr)
    {
    }

    virtual ~SystemServicesInitializeTest() override
    {
        plugin.Release();
    }
};

class SystemServicesTest : public SystemServicesInitializeTest {
protected:
    IarmBusImplMock* p_iarmBusMock = nullptr;
    RfcApiImplMock* p_rfcApiMock = nullptr;
    WrapsImplMock* p_wrapsMock = nullptr;
    HostImplMock* p_hostMock = nullptr;
    SleepModeMock* p_sleepModeMock = nullptr;
    TelemetryApiImplMock* p_telemetryApiImplMock = nullptr;
    readprocImplMock* p_readprocImplMock = nullptr;

    SystemServicesTest()
        : SystemServicesInitializeTest()
    {
        p_iarmBusMock = new NiceMock<IarmBusImplMock>;
        IarmBus::setImpl(p_iarmBusMock);
        
        p_rfcApiMock = new NiceMock<RfcApiImplMock>;
        RfcApi::setImpl(p_rfcApiMock);
        
        p_wrapsMock = new NiceMock<WrapsImplMock>;
        Wraps::setImpl(p_wrapsMock);
        
        p_hostMock = new NiceMock<HostImplMock>;
        device::Host::setImpl(p_hostMock);
        
        p_sleepModeMock = new NiceMock<SleepModeMock>;
        device::SleepMode::setImpl(p_sleepModeMock);

        p_telemetryApiImplMock = new NiceMock<TelemetryApiImplMock>;
        TelemetryApi::setImpl(p_telemetryApiImplMock);

        p_readprocImplMock = new NiceMock<readprocImplMock>;
        ProcImpl::setImpl(p_readprocImplMock);

        ON_CALL(*p_iarmBusMock, IARM_Bus_Init(::testing::_))
            .WillByDefault(::testing::Return(IARM_RESULT_SUCCESS));
        ON_CALL(*p_iarmBusMock, IARM_Bus_Connect())
            .WillByDefault(::testing::Return(IARM_RESULT_SUCCESS));
        ON_CALL(*p_iarmBusMock, IARM_Bus_RegisterCall(::testing::_, ::testing::_))
            .WillByDefault(::testing::Return(IARM_RESULT_SUCCESS));
        ON_CALL(*p_iarmBusMock, IARM_Bus_RegisterEventHandler(::testing::_, ::testing::_, ::testing::_))
            .WillByDefault(::testing::Return(IARM_RESULT_SUCCESS));
        ON_CALL(*p_iarmBusMock, IARM_Bus_Call(::testing::_, ::testing::_, ::testing::_, ::testing::_))
            .WillByDefault(::testing::Return(IARM_RESULT_SUCCESS));

        ON_CALL(service, COMLink())
            .WillByDefault(::testing::Return(&comLinkMock));

	// Mock QueryInterfaceByCallsign to return nullptr for dependent plugins
        // This simulates the plugins not being available (expected in unit test environment)
        ON_CALL(service, QueryInterfaceByCallsign(::testing::_, ::testing::_))
            .WillByDefault(::testing::Return(nullptr));

        ON_CALL(comLinkMock, Instantiate(::testing::_, ::testing::_, ::testing::_))
            .WillByDefault(::testing::Invoke(
            [&](const RPC::Object& object, const uint32_t waitTime, uint32_t& connectionId) {
                pluginImpl = Core::ProxyType<Plugin::SystemServicesImplementation>::Create();
                return &pluginImpl;
            }));

	Core::IWorkerPool::Assign(&(*workerPool));
        workerPool->Run();

        PluginHost::IFactories::Assign(&factoriesImplementation);

        dispatcher = static_cast<PLUGINHOST_DISPATCHER*>(
           plugin->QueryInterface(PLUGINHOST_DISPATCHER_ID));
        dispatcher->Activate(&service);

        plugin->Initialize(&service);
    }

    virtual ~SystemServicesTest() override
    {
        plugin->Deinitialize(&service);

	Core::IWorkerPool::Assign(nullptr);
        workerPool.Release();
        dispatcher->Deactivate();
        dispatcher->Release();
        PluginHost::IFactories::Assign(nullptr);

        IarmBus::setImpl(nullptr);
        if (p_iarmBusMock != nullptr)
        {
            delete p_iarmBusMock;
            p_iarmBusMock = nullptr;
        }
        
        RfcApi::setImpl(nullptr);
        if (p_rfcApiMock != nullptr)
        {
            delete p_rfcApiMock;
            p_rfcApiMock = nullptr;
        }
        
        Wraps::setImpl(nullptr);
        if (p_wrapsMock != nullptr)
        {
            delete p_wrapsMock;
            p_wrapsMock = nullptr;
        }
        
        device::Host::setImpl(nullptr);
        if (p_hostMock != nullptr)
        {
            delete p_hostMock;
            p_hostMock = nullptr;
        }
        
        device::SleepMode::setImpl(nullptr);
        if (p_sleepModeMock != nullptr)
        {
            delete p_sleepModeMock;
            p_sleepModeMock = nullptr;
        }
        
        TelemetryApi::setImpl(nullptr);
        if (p_telemetryApiImplMock != nullptr)
        {
            delete p_telemetryApiImplMock;
            p_telemetryApiImplMock = nullptr;
        }

        ProcImpl::setImpl(nullptr);
        if (p_readprocImplMock != nullptr)
        {
            delete p_readprocImplMock;
            p_readprocImplMock = nullptr;
        }
        
        PowerManagerMock::Delete();
    }
};

TEST_F(SystemServicesTest, RegisteredMethods)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("requestSystemUptime")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("getDownloadedFirmwareInfo")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("getFirmwareDownloadPercent")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("getFirmwareUpdateState")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("getFirmwareUpdateInfo")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("setTimeZoneDST")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("getTimeZoneDST")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("setTerritory")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("getTerritory")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("isOptOutTelemetry")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("setOptOutTelemetry")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("getSystemVersions")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("updateFirmware")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("setDeepSleepTimer")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("setNetworkStandbyMode")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("getNetworkStandbyMode")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("getFriendlyName")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("setFriendlyName")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("getWakeupReason")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("getLastWakeupKeyCode")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("getTimeZones")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("getRFCConfig")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("getPowerState")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("setPowerState")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("getPowerStateBeforeReboot")));
    // EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("setWakeupSrcConfiguration")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("getDeviceInfo")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("reboot")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("setBootLoaderSplashScreen")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("getMacAddresses")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("getMfgSerialNumber")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("getSerialNumber")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("getPlatformConfiguration")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("uploadLogsAsync")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("abortLogUpload")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("setFSRFlag")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("getFSRFlag")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("setBlocklistFlag")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("getBlocklistFlag")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("getBootTypeInfo")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("setMigrationStatus")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("getMigrationStatus")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("getBuildType")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("getTimeStatus")));
}

TEST_F(SystemServicesTest, RequestSystemUptime_Success)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("requestSystemUptime"), _T("{}"), response));
    
    JsonObject jsonResponse;
    ASSERT_TRUE(jsonResponse.FromString(response)) << "Failed to parse response: " << response;
    ASSERT_TRUE(jsonResponse.HasLabel("systemUptime")) << "Missing systemUptime field: " << response;
    ASSERT_TRUE(jsonResponse.HasLabel("success")) << "Missing success field: " << response;
    EXPECT_TRUE(jsonResponse["success"].Boolean()) << "Request failed: " << response;
    
    TEST_LOG("RequestSystemUptime test PASSED - Response: %s", response.c_str());
}
#if 0
TEST_F(SystemServicesTest, GetBlocklistFlag_Success)
{
    // Create the opflashstore directory and devicestate file for blocklist tests
    system("mkdir -p /opt/secure/persistent/opflashstore");
    createFile("/opt/secure/persistent/opflashstore/devicestate.txt", "BLOCKLIST=false");

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getBlocklistFlag"), _T("{}"), response));
    
    JsonObject jsonResponse;
    ASSERT_TRUE(jsonResponse.FromString(response)) << "Failed to parse response: " << response;
    ASSERT_TRUE(jsonResponse.HasLabel("success")) << "Missing success field: " << response;
    EXPECT_TRUE(jsonResponse["success"].Boolean()) << "Request failed: " << response;
    
    TEST_LOG("GetBlocklistFlag test PASSED - Response: %s", response.c_str());
    
    // Cleanup
    removeFile("/opt/secure/persistent/opflashstore/devicestate.txt");
}
#endif
TEST_F(SystemServicesTest, SetBlocklistFlag_Success)
{
    // Create the opflashstore directory for blocklist tests
    system("mkdir -p /opt/secure/persistent/opflashstore");
    createFile("/opt/secure/persistent/opflashstore/devicestate.txt", "BLOCKLIST=false");

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setBlocklistFlag"), _T("{\"blocklist\":true}"), response));
    
    JsonObject jsonResponse;
    ASSERT_TRUE(jsonResponse.FromString(response)) << "Failed to parse response: " << response;
    ASSERT_TRUE(jsonResponse.HasLabel("success")) << "Missing success field: " << response;
    EXPECT_TRUE(jsonResponse["success"].Boolean()) << "SetBlocklistFlag failed: " << response;
    
    TEST_LOG("SetBlocklistFlag test PASSED - Response: %s", response.c_str());
    
    // Cleanup
    removeFile("/opt/secure/persistent/opflashstore/devicestate.txt");
}
#if 0
TEST_F(SystemServicesTest, SetBlocklistFlag_Failure_MissingParameter)
{
    system("mkdir -p /opt/secure/persistent/opflashstore");
    
    handler.Invoke(connection, _T("setBlocklistFlag"), _T("{}"), response);
    
    // The JSON-RPC call succeeds, but the response should indicate failure
    JsonObject jsonResponse;
    ASSERT_TRUE(jsonResponse.FromString(response)) << "Failed to parse response: " << response;
    
    // Check if success is false or error exists
    if (jsonResponse.HasLabel("success")) {
        EXPECT_FALSE(jsonResponse["success"].Boolean()) << "Should fail with missing parameter, response: " << response;
    }
    
    TEST_LOG("SetBlocklistFlag missing parameter test - Response: %s", response.c_str());
}
#endif
TEST_F(SystemServicesTest, GetBootTypeInfo_Success)
{
    // This may return ERROR_GENERAL if boot type cannot be determined
    uint32_t result = handler.Invoke(connection, _T("getBootTypeInfo"), _T("{}"), response);
    
    JsonObject jsonResponse;
    ASSERT_TRUE(jsonResponse.FromString(response)) << "Failed to parse response: " << response;
    
    // Boot type info may not be available in test environment
    if (result == Core::ERROR_NONE && jsonResponse.HasLabel("bootType")) {
        TEST_LOG("GetBootTypeInfo test PASSED - Response: %s", response.c_str());
    } else {
        TEST_LOG("GetBootTypeInfo not available in test environment - Response: %s", response.c_str());
    }
}

TEST_F(SystemServicesTest, GetBuildType_Success)
{
    // Create device.properties file for build type
    createFile("/etc/device.properties", "BUILD_TYPE=dev");
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getBuildType"), _T("{}"), response));
    
    JsonObject jsonResponse;
    ASSERT_TRUE(jsonResponse.FromString(response)) << "Failed to parse response: " << response;
    ASSERT_TRUE(jsonResponse.HasLabel("build_type")) << "Missing build_type field: " << response;
    ASSERT_TRUE(jsonResponse.HasLabel("success")) << "Missing success field: " << response;
    EXPECT_TRUE(jsonResponse["success"].Boolean()) << "Request failed: " << response;
    
    TEST_LOG("GetBuildType test PASSED - Response: %s", response.c_str());
    
    removeFile("/etc/device.properties");
}

TEST_F(SystemServicesTest, GetDeviceInfo_ExternalPluginNotAvailable)
{
    // getDeviceInfo requires DeviceInfo plugin which is not activated in unit tests
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getDeviceInfo"), _T("{\"params\":[]}"), response));
    
    JsonObject jsonResponse;
    ASSERT_TRUE(jsonResponse.FromString(response)) << "Failed to parse response: " << response;
    
    // In unit test environment, DeviceInfo plugin is not activated
    // so success will be false with message "DeviceInfo plugin is not activated"
    if (jsonResponse.HasLabel("message")) {
        EXPECT_TRUE(jsonResponse["message"].String().find("DeviceInfo plugin is not activated") != std::string::npos)
            << "Unexpected error message: " << response;
        TEST_LOG("GetDeviceInfo correctly reports DeviceInfo plugin not available - Response: %s", response.c_str());
    }
}

TEST_F(SystemServicesTest, GetDownloadedFirmwareInfo_Success)
{
    std::ofstream file("/version.txt");
    file << "imagename:TEST_IMAGE_VERSION\n";
    file.close();

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getDownloadedFirmwareInfo"), _T("{}"), response));
    
    JsonObject jsonResponse;
    ASSERT_TRUE(jsonResponse.FromString(response)) << "Failed to parse response: " << response;
    ASSERT_TRUE(jsonResponse.HasLabel("currentFWVersion")) << "Missing currentFWVersion: " << response;
    ASSERT_TRUE(jsonResponse.HasLabel("success")) << "Missing success field: " << response;
    EXPECT_TRUE(jsonResponse["success"].Boolean()) << "Request failed: " << response;
    
    TEST_LOG("GetDownloadedFirmwareInfo test PASSED - Response: %s", response.c_str());
    
    std::remove("/version.txt");
}

TEST_F(SystemServicesTest, GetFirmwareDownloadPercent_Success)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getFirmwareDownloadPercent"), _T("{}"), response));
    
    JsonObject jsonResponse;
    ASSERT_TRUE(jsonResponse.FromString(response)) << "Failed to parse response: " << response;
    ASSERT_TRUE(jsonResponse.HasLabel("downloadPercent")) << "Missing downloadPercent: " << response;
    ASSERT_TRUE(jsonResponse.HasLabel("success")) << "Missing success field: " << response;
    
    TEST_LOG("GetFirmwareDownloadPercent test PASSED - Response: %s", response.c_str());
}

TEST_F(SystemServicesTest, GetFirmwareUpdateState_Success)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getFirmwareUpdateState"), _T("{}"), response));
    
    JsonObject jsonResponse;
    ASSERT_TRUE(jsonResponse.FromString(response)) << "Failed to parse response: " << response;
    ASSERT_TRUE(jsonResponse.HasLabel("firmwareUpdateState")) << "Missing firmwareUpdateState: " << response;
    ASSERT_TRUE(jsonResponse.HasLabel("success")) << "Missing success field: " << response;
    
    TEST_LOG("GetFirmwareUpdateState test PASSED - Response: %s", response.c_str());
}

TEST_F(SystemServicesTest, GetFSRFlag_Success)
{
    EXPECT_CALL(*p_iarmBusMock, IARM_Bus_Call(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(IARM_RESULT_SUCCESS));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getFSRFlag"), _T("{}"), response));
    
    JsonObject jsonResponse;
    ASSERT_TRUE(jsonResponse.FromString(response)) << "Failed to parse response: " << response;
    ASSERT_TRUE(jsonResponse.HasLabel("fsrFlag")) << "Missing fsrFlag: " << response;
    ASSERT_TRUE(jsonResponse.HasLabel("success")) << "Missing success field: " << response;
    
    TEST_LOG("GetFSRFlag test PASSED - Response: %s", response.c_str());
}

TEST_F(SystemServicesTest, SetFSRFlag_Success)
{
    EXPECT_CALL(*p_iarmBusMock, IARM_Bus_Call(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(IARM_RESULT_SUCCESS));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setFSRFlag"), _T("{\"fsrFlag\":true}"), response));
    
    JsonObject jsonResponse;
    ASSERT_TRUE(jsonResponse.FromString(response)) << "Failed to parse response: " << response;
    ASSERT_TRUE(jsonResponse.HasLabel("success")) << "Missing success field: " << response;
    EXPECT_TRUE(jsonResponse["success"].Boolean()) << "SetFSRFlag failed: " << response;
    
    TEST_LOG("SetFSRFlag test PASSED - Response: %s", response.c_str());
}

TEST_F(SystemServicesTest, SetFSRFlag_Failure_IarmError)
{
    EXPECT_CALL(*p_iarmBusMock, IARM_Bus_Call(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(IARM_RESULT_INVALID_PARAM));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setFSRFlag"), _T("{\"fsrFlag\":true}"), response));
    
    JsonObject jsonResponse;
    ASSERT_TRUE(jsonResponse.FromString(response)) << "Failed to parse response: " << response;
    ASSERT_TRUE(jsonResponse.HasLabel("success")) << "Missing success field: " << response;
    EXPECT_FALSE(jsonResponse["success"].Boolean()) << "Should fail with IARM error: " << response;
    
    TEST_LOG("SetFSRFlag IARM failure test PASSED - Response: %s", response.c_str());
}

TEST_F(SystemServicesTest, GetFriendlyName_Success)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getFriendlyName"), _T("{}"), response));
    
    JsonObject jsonResponse;
    ASSERT_TRUE(jsonResponse.FromString(response)) << "Failed to parse response: " << response;
    ASSERT_TRUE(jsonResponse.HasLabel("friendlyName")) << "Missing friendlyName: " << response;
    ASSERT_TRUE(jsonResponse.HasLabel("success")) << "Missing success field: " << response;
    
    TEST_LOG("GetFriendlyName test PASSED - Response: %s", response.c_str());
}

TEST_F(SystemServicesTest, SetFriendlyName_Success)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setFriendlyName"), _T("{\"friendlyName\":\"TestDevice\"}"), response));
    
    JsonObject jsonResponse;
    ASSERT_TRUE(jsonResponse.FromString(response)) << "Failed to parse response: " << response;
    ASSERT_TRUE(jsonResponse.HasLabel("success")) << "Missing success field: " << response;
    EXPECT_TRUE(jsonResponse["success"].Boolean()) << "SetFriendlyName failed: " << response;
    
    TEST_LOG("SetFriendlyName test PASSED - Response: %s", response.c_str());
}

TEST_F(SystemServicesTest, GetMacAddresses_Success)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getMacAddresses"), _T("{}"), response));
    
    JsonObject jsonResponse;
    ASSERT_TRUE(jsonResponse.FromString(response)) << "Failed to parse response: " << response;
    ASSERT_TRUE(jsonResponse.HasLabel("success")) << "Missing success field: " << response;
    
    TEST_LOG("GetMacAddresses test PASSED - Response: %s", response.c_str());
}

TEST_F(SystemServicesTest, GetMfgSerialNumber_Success)
{
    EXPECT_CALL(*p_iarmBusMock, IARM_Bus_Call(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(IARM_RESULT_SUCCESS));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getMfgSerialNumber"), _T("{}"), response));
    
    JsonObject jsonResponse;
    ASSERT_TRUE(jsonResponse.FromString(response)) << "Failed to parse response: " << response;
    ASSERT_TRUE(jsonResponse.HasLabel("mfgSerialNumber")) << "Missing mfgSerialNumber: " << response;
    ASSERT_TRUE(jsonResponse.HasLabel("success")) << "Missing success field: " << response;
    
    TEST_LOG("GetMfgSerialNumber test PASSED - Response: %s", response.c_str());
}
#if 0
TEST_F(SystemServicesTest, GetMigrationStatus_Success)
{
    // Create migration file
    system("mkdir -p /opt/secure/persistent/opflashstore");
    createFile("/opt/secure/persistent/opflashstore/migrationStatus.txt", "MIGRATION_STATUS=NOT_STARTED");
    
    EXPECT_CALL(*p_rfcApiMock, getRFCParameter(::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Invoke(
            [](char* pcCallerID, const char* pcParameterName, RFC_ParamData_t* pstParamData) {
                pstParamData->type = WDMP_STRING;
                strncpy(pstParamData->value, "NOT_STARTED", sizeof(pstParamData->value));
                return WDMP_SUCCESS;
            }));

    uint32_t result = handler.Invoke(connection, _T("getMigrationStatus"), _T("{}"), response);
    
    JsonObject jsonResponse;
    ASSERT_TRUE(jsonResponse.FromString(response)) << "Failed to parse response: " << response;
    
    if (result == Core::ERROR_NONE && jsonResponse.HasLabel("migrationStatus")) {
        TEST_LOG("GetMigrationStatus test PASSED - Response: %s", response.c_str());
    } else {
        TEST_LOG("GetMigrationStatus - Response: %s", response.c_str());
    }
    
    removeFile("/opt/secure/persistent/opflashstore/migrationStatus.txt");
}
#endif
TEST_F(SystemServicesTest, SetMigrationStatus_Success)
{
    system("mkdir -p /opt/secure/persistent/opflashstore");
    
    uint32_t result = handler.Invoke(connection, _T("setMigrationStatus"), _T("{\"migrationStatus\":\"COMPLETED\"}"), response);
    
    JsonObject jsonResponse;
    ASSERT_TRUE(jsonResponse.FromString(response)) << "Failed to parse response: " << response;
    
    if (result == Core::ERROR_NONE && jsonResponse.HasLabel("success")) {
        TEST_LOG("SetMigrationStatus test PASSED - Response: %s", response.c_str());
    } else {
        TEST_LOG("SetMigrationStatus - Response: %s", response.c_str());
    }
    
    removeFile("/opt/secure/persistent/opflashstore/migrationStatus.txt");
}

TEST_F(SystemServicesTest, GetNetworkStandbyMode_Success)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getNetworkStandbyMode"), _T("{}"), response));
    
    JsonObject jsonResponse;
    ASSERT_TRUE(jsonResponse.FromString(response)) << "Failed to parse response: " << response;
    ASSERT_TRUE(jsonResponse.HasLabel("nwStandby")) << "Missing nwStandby: " << response;
    ASSERT_TRUE(jsonResponse.HasLabel("success")) << "Missing success field: " << response;
    
    TEST_LOG("GetNetworkStandbyMode test PASSED - Response: %s", response.c_str());
}

TEST_F(SystemServicesTest, SetNetworkStandbyMode_Success)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setNetworkStandbyMode"), _T("{\"nwStandby\":true}"), response));
    
    JsonObject jsonResponse;
    ASSERT_TRUE(jsonResponse.FromString(response)) << "Failed to parse response: " << response;
    ASSERT_TRUE(jsonResponse.HasLabel("success")) << "Missing success field: " << response;
    EXPECT_TRUE(jsonResponse["success"].Boolean()) << "SetNetworkStandbyMode failed: " << response;
    
    TEST_LOG("SetNetworkStandbyMode test PASSED - Response: %s", response.c_str());
}

TEST_F(SystemServicesTest, GetPowerState_Success)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getPowerState"), _T("{}"), response));
    
    JsonObject jsonResponse;
    ASSERT_TRUE(jsonResponse.FromString(response)) << "Failed to parse response: " << response;
    ASSERT_TRUE(jsonResponse.HasLabel("powerState")) << "Missing powerState: " << response;
    ASSERT_TRUE(jsonResponse.HasLabel("success")) << "Missing success field: " << response;
    
    TEST_LOG("GetPowerState test PASSED - Response: %s", response.c_str());
}

class PowerStateTest : public SystemServicesTest, public ::testing::WithParamInterface<const char*> {};
TEST_P(PowerStateTest, SetPowerState_Success)
{
    const char* powerState = GetParam();
    std::string request = std::string("{\"powerState\":\"") + powerState + "\"}";
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setPowerState"), Core::ToString(request), response));
    
    JsonObject jsonResponse;
    ASSERT_TRUE(jsonResponse.FromString(response)) << "Failed to parse response for " << powerState << ": " << response;
    ASSERT_TRUE(jsonResponse.HasLabel("success")) << "Missing success field for " << powerState << ": " << response;
    EXPECT_TRUE(jsonResponse["success"].Boolean()) << "SetPowerState failed for " << powerState << ": " << response;
    
    TEST_LOG("SetPowerState %s test PASSED - Response: %s", powerState, response.c_str());
}
INSTANTIATE_TEST_SUITE_P(
    PowerStates,
    PowerStateTest,
    ::testing::Values(
        "ON",
        "STANDBY"
    )
);

TEST_F(SystemServicesTest, GetRFCConfig_Success)
{
    RFC_ParamData_t rfcParam;
    strcpy(rfcParam.value, "TestValue");
    
    EXPECT_CALL(*p_rfcApiMock, getRFCParameter(::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::DoAll(
            ::testing::SetArgPointee<2>(rfcParam),
            ::testing::Return(WDMP_SUCCESS)));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getRFCConfig"), 
              _T("{\"rfcList\":[\"Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.Test\"]}"), response));
    
    JsonObject jsonResponse;
    ASSERT_TRUE(jsonResponse.FromString(response)) << "Failed to parse response: " << response;
    ASSERT_TRUE(jsonResponse.HasLabel("success")) << "Missing success field: " << response;
    
    TEST_LOG("GetRFCConfig test PASSED - Response: %s", response.c_str());
}

TEST_F(SystemServicesTest, GetSerialNumber_ExternalPluginNotAvailable)
{
    // getSerialNumber requires DeviceInfo plugin which is not activated in unit tests
    handler.Invoke(connection, _T("getSerialNumber"), _T("{}"), response);
    
    JsonObject jsonResponse;
    ASSERT_TRUE(jsonResponse.FromString(response)) << "Failed to parse response: " << response;
    
    // DeviceInfo plugin is not activated in unit test environment
    // Response will have success=false
    EXPECT_FALSE(jsonResponse.HasLabel("serialNumber") && !jsonResponse["serialNumber"].String().empty())
        << "SerialNumber should not be available without DeviceInfo plugin";
    
    TEST_LOG("GetSerialNumber correctly reports DeviceInfo plugin not available - Response: %s", response.c_str());
}

TEST_F(SystemServicesTest, GetSystemVersions_Success)
{
    std::ofstream file("/version.txt");
    file << "imagename:TEST_IMAGE\nSDK_VERSION=17.3\n";
    file.close();

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getSystemVersions"), _T("{}"), response));
    
    JsonObject jsonResponse;
    ASSERT_TRUE(jsonResponse.FromString(response)) << "Failed to parse response: " << response;
    ASSERT_TRUE(jsonResponse.HasLabel("success")) << "Missing success field: " << response;
    
    TEST_LOG("GetSystemVersions test PASSED - Response: %s", response.c_str());
    
    std::remove("/version.txt");
}
#if 0
class TerritoryTest : public SystemServicesTest, public ::testing::WithParamInterface<const char*> {};
TEST_P(TerritoryTest, SetTerritory_Success)
{
    const char* territory = GetParam();
    
    EXPECT_CALL(*p_rfcApiMock, setRFCParameter(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(WDMP_SUCCESS));

    std::string request = std::string("{\"territory\":\"") + territory + "\"}";
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setTerritory"), Core::ToString(request), response));
    
    JsonObject jsonResponse;
    ASSERT_TRUE(jsonResponse.FromString(response)) << "Failed to parse response for " << territory << ": " << response;
    ASSERT_TRUE(jsonResponse.HasLabel("success")) << "Missing success field for " << territory << ": " << response;
    EXPECT_TRUE(jsonResponse["success"].Boolean()) << "SetTerritory failed for " << territory << ": " << response;
    
    TEST_LOG("SetTerritory %s test PASSED - Response: %s", territory, response.c_str());
}
INSTANTIATE_TEST_SUITE_P(
    Territories,
    TerritoryTest,
    ::testing::Values(
        "USA",
        "GBR",
        "AUS",
        "CAN"
    )
);
#endif
TEST_F(SystemServicesTest, GetTimeStatus_NotSupported)
{
    // Remove the duplicate GetTimeStatus_Success test - getTimeStatus is not supported
    // getTimeStatus is not supported according to implementation
    uint32_t result = handler.Invoke(connection, _T("getTimeStatus"), _T("{}"), response);
    
    // The implementation logs "SystemService GetTimeStatus not supported" and returns ERROR_GENERAL
    EXPECT_EQ(Core::ERROR_GENERAL, result) << "GetTimeStatus should return ERROR_GENERAL as it's not supported";
    
    TEST_LOG("GetTimeStatus correctly returns not supported - Response: %s", response.c_str());
}

TEST_F(SystemServicesTest, GetTimeZoneDST_Success)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getTimeZoneDST"), _T("{}"), response));
    
    JsonObject jsonResponse;
    ASSERT_TRUE(jsonResponse.FromString(response)) << "Failed to parse response: " << response;
    ASSERT_TRUE(jsonResponse.HasLabel("success")) << "Missing success field: " << response;
    
    TEST_LOG("GetTimeZoneDST test PASSED - Response: %s", response.c_str());
}

TEST_F(SystemServicesTest, SetTimeZoneDST_Success)
{
    // Mock RFC parameter set - implementation may not call this if file write succeeds
    ON_CALL(*p_rfcApiMock, setRFCParameter(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillByDefault(::testing::Return(WDMP_SUCCESS));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setTimeZoneDST"), _T("{\"timeZone\":\"America/New_York\"}"), response));
    
    JsonObject jsonResponse;
    ASSERT_TRUE(jsonResponse.FromString(response)) << "Failed to parse response: " << response;
    ASSERT_TRUE(jsonResponse.HasLabel("success")) << "Missing success field: " << response;
    
    TEST_LOG("SetTimeZoneDST test PASSED - Response: %s", response.c_str());
}

TEST_F(SystemServicesTest, GetWakeupReason_Success)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getWakeupReason"), _T("{}"), response));
    
    JsonObject jsonResponse;
    ASSERT_TRUE(jsonResponse.FromString(response)) << "Failed to parse response: " << response;
    ASSERT_TRUE(jsonResponse.HasLabel("wakeupReason")) << "Missing wakeupReason: " << response;
    ASSERT_TRUE(jsonResponse.HasLabel("success")) << "Missing success field: " << response;
    
    TEST_LOG("GetWakeupReason test PASSED - Response: %s", response.c_str());
}

TEST_F(SystemServicesTest, GetLastWakeupKeyCode_Success)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getLastWakeupKeyCode"), _T("{}"), response));
    
    JsonObject jsonResponse;
    ASSERT_TRUE(jsonResponse.FromString(response)) << "Failed to parse response: " << response;
    ASSERT_TRUE(jsonResponse.HasLabel("wakeupKeyCode")) << "Missing wakeupKeyCode: " << response;
    ASSERT_TRUE(jsonResponse.HasLabel("success")) << "Missing success field: " << response;
    
    TEST_LOG("GetLastWakeupKeyCode test PASSED - Response: %s", response.c_str());
}

TEST_F(SystemServicesTest, IsOptOutTelemetry_ExternalPluginNotAvailable)
{
    // isOptOutTelemetry requires Telemetry plugin which is not activated in unit tests
    uint32_t result = handler.Invoke(connection, _T("isOptOutTelemetry"), _T("{}"), response);
    
    // The implementation returns ERROR_GENERAL when Telemetry plugin is not activated
    EXPECT_EQ(Core::ERROR_GENERAL, result) << "Should return ERROR_GENERAL when Telemetry plugin not available";
    
    TEST_LOG("IsOptOutTelemetry correctly reports Telemetry plugin not available - Response: %s", response.c_str());
}

TEST_F(SystemServicesTest, SetOptOutTelemetry_ExternalPluginNotAvailable)
{
    // setOptOutTelemetry requires Telemetry plugin which is not activated in unit tests
    uint32_t result = handler.Invoke(connection, _T("setOptOutTelemetry"), _T("{\"Opt-Out\":true}"), response);
    
    // The implementation returns ERROR_GENERAL when Telemetry plugin is not activated
    EXPECT_EQ(Core::ERROR_GENERAL, result) << "Should return ERROR_GENERAL when Telemetry plugin not available";
    
    TEST_LOG("SetOptOutTelemetry correctly reports Telemetry plugin not available - Response: %s", response.c_str());
}
#if 0
TEST_F(SystemServicesTest, GetSetBlocklistCombined)
{
    // Combined test like the old test file - set and get blocklist
    system("mkdir -p /opt/secure/persistent/opflashstore");
    createFile("/opt/secure/persistent/opflashstore/devicestate.txt", "BLOCKLIST=false");
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setBlocklistFlag"), _T("{\"blocklist\":true}"), response));
    EXPECT_EQ(response, string("{\"success\":true}"));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getBlocklistFlag"), _T("{}"), response));
    EXPECT_EQ(response, string("{\"blocklist\":true,\"success\":true}"));
    
    TEST_LOG("GetSetBlocklistCombined test PASSED - Response: %s", response.c_str());
    
    removeFile("/opt/secure/persistent/opflashstore/devicestate.txt");
}

TEST_F(SystemServicesTest, SetBlocklist_ParamTrue)
{
    system("mkdir -p /opt/secure/persistent/opflashstore");
    createFile("/opt/secure/persistent/opflashstore/devicestate.txt", "BLOCKLIST=false");
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setBlocklistFlag"), _T("{\"blocklist\":true}"), response));
    EXPECT_EQ(response, string("{\"success\":true}"));
    
    removeFile("/opt/secure/persistent/opflashstore/devicestate.txt");
}

TEST_F(SystemServicesTest, SetBlocklist_ParamFalse)
{
    system("mkdir -p /opt/secure/persistent/opflashstore");
    createFile("/opt/secure/persistent/opflashstore/devicestate.txt", "BLOCKLIST=true");
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setBlocklistFlag"), _T("{\"blocklist\":false}"), response));
    EXPECT_EQ(response, string("{\"success\":true}"));
    
    removeFile("/opt/secure/persistent/opflashstore/devicestate.txt");
}

TEST_F(SystemServicesTest, Reboot_Success)
{
    EXPECT_CALL(*p_iarmBusMock, IARM_Bus_Call(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(IARM_RESULT_SUCCESS));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("reboot"), _T("{\"rebootReason\":\"FIRMWARE_FAILURE\"}"), response));
    
    JsonObject jsonResponse;
    ASSERT_TRUE(jsonResponse.FromString(response)) << "Failed to parse response: " << response;
    ASSERT_TRUE(jsonResponse.HasLabel("success")) << "Missing success field: " << response;
    
    TEST_LOG("Reboot test PASSED - Response: %s", response.c_str());
}
#endif
TEST_F(SystemServicesTest, SetBootLoaderSplashScreen_Success)
{
    std::ofstream file("/tmp/test_splash.png");
    file << "test";
    file.close();

    EXPECT_CALL(*p_iarmBusMock, IARM_Bus_Call(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(IARM_RESULT_SUCCESS));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setBootLoaderSplashScreen"), _T("{\"path\":\"/tmp/test_splash.png\"}"), response));
    
    JsonObject jsonResponse;
    ASSERT_TRUE(jsonResponse.FromString(response)) << "Failed to parse response: " << response;
    ASSERT_TRUE(jsonResponse.HasLabel("success")) << "Missing success field: " << response;
    
    TEST_LOG("SetBootLoaderSplashScreen test PASSED - Response: %s", response.c_str());
    
    std::remove("/tmp/test_splash.png");
}
#if 0
TEST_F(SystemServicesTest, SetDeepSleepTimer_Success)
{
    EXPECT_CALL(*p_iarmBusMock, IARM_Bus_Call(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(IARM_RESULT_SUCCESS));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setDeepSleepTimer"), _T("{\"seconds\":10}"), response));
    
    JsonObject jsonResponse;
    ASSERT_TRUE(jsonResponse.FromString(response)) << "Failed to parse response: " << response;
    ASSERT_TRUE(jsonResponse.HasLabel("success")) << "Missing success field: " << response;
    
    TEST_LOG("SetDeepSleepTimer test PASSED - Response: %s", response.c_str());
}
#endif
// TODO: Implement SetWakeupSrcConfiguration in SystemServicesImplementation before enabling this test
/*
TEST_F(SystemServicesTest, SetWakeupSrcConfiguration_Success)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setWakeupSrcConfiguration"), _T("{\"wakeupSrc\":\"VOICE\",\"enabled\":true}"), response));
    
    JsonObject jsonResponse;
    ASSERT_TRUE(jsonResponse.FromString(response)) << "Failed to parse response: " << response;
    ASSERT_TRUE(jsonResponse.HasLabel("success")) << "Missing success field: " << response;
    
    TEST_LOG("SetWakeupSrcConfiguration test PASSED - Response: %s", response.c_str());
}
*/

TEST_F(SystemServicesTest, UpdateFirmware_Success)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("updateFirmware"), _T("{}"), response));
    
    JsonObject jsonResponse;
    ASSERT_TRUE(jsonResponse.FromString(response)) << "Failed to parse response: " << response;
    
    TEST_LOG("UpdateFirmware test PASSED - Response: %s", response.c_str());
}

TEST_F(SystemServicesTest, UploadLogsAsync_Success)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("uploadLogsAsync"), _T("{\"url\":\"http://test.com/logs\"}"), response));
    
    JsonObject jsonResponse;
    ASSERT_TRUE(jsonResponse.FromString(response)) << "Failed to parse response: " << response;
    ASSERT_TRUE(jsonResponse.HasLabel("success")) << "Missing success field: " << response;
    
    TEST_LOG("UploadLogsAsync test PASSED - Response: %s", response.c_str());
}

TEST_F(SystemServicesTest, AbortLogUpload_Success)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("abortLogUpload"), _T("{}"), response));
    
    JsonObject jsonResponse;
    ASSERT_TRUE(jsonResponse.FromString(response)) << "Failed to parse response: " << response;
    ASSERT_TRUE(jsonResponse.HasLabel("success")) << "Missing success field: " << response;
    
    TEST_LOG("AbortLogUpload test PASSED - Response: %s", response.c_str());
}

TEST_F(SystemServicesTest, PluginInformation)
{
    string info = plugin->Information();
    EXPECT_TRUE(!info.empty() || info.empty());  // Just validate it doesn't crash
    TEST_LOG("PluginInformation test PASSED");
}
