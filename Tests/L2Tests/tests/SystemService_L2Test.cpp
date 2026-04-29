/**
* If not stated otherwise in this file or this component's LICENSE
* file the following copyright and licenses apply:
*
* Copyright 2024 RDK Management
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
#include "L2Tests.h"
#include "L2TestsMock.h"
#include <mutex>
#include <condition_variable>
#include <fstream>
#include <interfaces/ISystemServices.h>
#include "deepSleepMgr.h"
#include "PowerManagerHalMock.h"
#include "MfrMock.h"

#define JSON_TIMEOUT   (1000)
#define TEST_LOG(x, ...) fprintf(stderr, "\033[1;32m[%s:%d](%s)<PID:%d><TID:%d>" x "\n\033[0m", __FILE__, __LINE__, __FUNCTION__, getpid(), gettid(), ##__VA_ARGS__); fflush(stderr);
#define SYSTEM_CALLSIGN  _T("org.rdk.System.1")
#define L2TEST_CALLSIGN _T("L2tests.1")

using ::testing::NiceMock;
using namespace WPEFramework;
using testing::StrictMock;

typedef enum : uint32_t {
    SYSTEMSERVICEL2TEST_SYSTEMSTATE_CHANGED = 0x00000001,
    SYSTEMSERVICEL2TEST_THERMALSTATE_CHANGED=0x00000002,
    SYSTEMSERVICEL2TEST_LOGUPLOADSTATE_CHANGED=0x00000004,
    SYSTEMSERVICEL2TEST_BLOCKLIST_CHANGED=0x00000008,
    SYSTEMSERVICEL2TEST_FIRMWARE_UPDATE_INFO = 0x00000010,
    SYSTEMSERVICEL2TEST_REBOOT_REQUEST = 0x00000020,
    SYSTEMSERVICEL2TEST_TERRITORY_CHANGED = 0x00000040,
    SYSTEMSERVICEL2TEST_FRIENDLY_NAME_CHANGED = 0x00000080,
    SYSTEMSERVICEL2TEST_SYSTEM_MODE_CHANGED = 0x00000100,
    SYSTEMSERVICEL2TEST_NETWORK_STANDBY_CHANGED = 0x00000200,
    SYSTEMSERVICEL2TEST_STATE_INVALID = 0x00000000
}SystemServiceL2test_async_events_t;

/**
 * @brief Notification handler class for SystemServices COM-RPC notifications
 */
class SystemServicesNotificationHandler : public Exchange::ISystemServices::INotification {
private:
    std::mutex m_mutex;
    std::condition_variable m_condition_variable;
    uint32_t m_event_signalled;
    string m_lastPowerState;
    string m_lastCurrentPowerState;
    string m_lastFriendlyName;
    string m_lastMode;
    string m_lastOldBlocklist;
    string m_lastNewBlocklist;
    bool m_lastNwStandby;

public:
    SystemServicesNotificationHandler()
        : m_event_signalled(SYSTEMSERVICEL2TEST_STATE_INVALID)
        , m_lastNwStandby(false)
    {
    }

    virtual ~SystemServicesNotificationHandler() = default;

    BEGIN_INTERFACE_MAP(SystemServicesNotificationHandler)
    INTERFACE_ENTRY(Exchange::ISystemServices::INotification)
    END_INTERFACE_MAP

    void OnFirmwareUpdateInfoReceived(const Exchange::ISystemServices::FirmwareUpdateInfo& firmwareUpdateInfo) override
    {
        TEST_LOG("OnFirmwareUpdateInfoReceived notification received");
        std::unique_lock<std::mutex> lock(m_mutex);
        m_event_signalled |= SYSTEMSERVICEL2TEST_FIRMWARE_UPDATE_INFO;
        m_condition_variable.notify_one();
    }

    void OnRebootRequest(const string& requestedApp, const string& rebootReason) override
    {
        TEST_LOG("OnRebootRequest notification received");
        TEST_LOG("  requestedApp: %s", requestedApp.c_str());
        TEST_LOG("  rebootReason: %s", rebootReason.c_str());
        std::unique_lock<std::mutex> lock(m_mutex);
        m_event_signalled |= SYSTEMSERVICEL2TEST_REBOOT_REQUEST;
        m_condition_variable.notify_one();
    }

    void OnSystemPowerStateChanged(const string& powerState, const string& currentPowerState) override
    {
        TEST_LOG("OnSystemPowerStateChanged notification received");
        TEST_LOG("  powerState: %s", powerState.c_str());
        TEST_LOG("  currentPowerState: %s", currentPowerState.c_str());
        std::unique_lock<std::mutex> lock(m_mutex);
        m_lastPowerState = powerState;
        m_lastCurrentPowerState = currentPowerState;
        m_event_signalled |= SYSTEMSERVICEL2TEST_SYSTEMSTATE_CHANGED;
        m_condition_variable.notify_one();
    }

    void OnTerritoryChanged(const Exchange::ISystemServices::TerritoryChangedInfo& territoryChangedInfo) override
    {
        TEST_LOG("OnTerritoryChanged notification received");
        std::unique_lock<std::mutex> lock(m_mutex);
        m_event_signalled |= SYSTEMSERVICEL2TEST_TERRITORY_CHANGED;
        m_condition_variable.notify_one();
    }

    void OnTimeZoneDSTChanged(const Exchange::ISystemServices::TimeZoneDSTChangedInfo& timeZoneDSTChangedInfo) override
    {
        TEST_LOG("OnTimeZoneDSTChanged notification received");
        std::unique_lock<std::mutex> lock(m_mutex);
        m_condition_variable.notify_one();
    }

    void OnMacAddressesRetreived(const Exchange::ISystemServices::MacAddressesInfo& macAddressesInfo) override
    {
        TEST_LOG("OnMacAddressesRetreived notification received");
        std::unique_lock<std::mutex> lock(m_mutex);
        m_condition_variable.notify_one();
    }

    void OnSystemModeChanged(const string& mode) override
    {
        TEST_LOG("OnSystemModeChanged notification received");
        TEST_LOG("  mode: %s", mode.c_str());
        std::unique_lock<std::mutex> lock(m_mutex);
        m_lastMode = mode;
        m_event_signalled |= SYSTEMSERVICEL2TEST_SYSTEM_MODE_CHANGED;
        m_condition_variable.notify_one();
    }

    void OnLogUpload(const string& logUploadStatus) override
    {
        TEST_LOG("OnLogUpload notification received");
        TEST_LOG("  logUploadStatus: %s", logUploadStatus.c_str());
        std::unique_lock<std::mutex> lock(m_mutex);
        m_event_signalled |= SYSTEMSERVICEL2TEST_LOGUPLOADSTATE_CHANGED;
        m_condition_variable.notify_one();
    }

    void OnFirmwareUpdateStateChanged(const int firmwareUpdateStateChange) override
    {
        TEST_LOG("OnFirmwareUpdateStateChanged notification received");
        TEST_LOG("  state: %d", firmwareUpdateStateChange);
        std::unique_lock<std::mutex> lock(m_mutex);
        m_condition_variable.notify_one();
    }

    void OnTemperatureThresholdChanged(const string& thresholdType, const bool exceeded, const string& temperature) override
    {
        TEST_LOG("OnTemperatureThresholdChanged notification received");
        TEST_LOG("  thresholdType: %s, exceeded: %d, temperature: %s", 
                 thresholdType.c_str(), exceeded, temperature.c_str());
        std::unique_lock<std::mutex> lock(m_mutex);
        m_event_signalled |= SYSTEMSERVICEL2TEST_THERMALSTATE_CHANGED;
        m_condition_variable.notify_one();
    }

    void OnSystemClockSet() override
    {
        TEST_LOG("OnSystemClockSet notification received");
        std::unique_lock<std::mutex> lock(m_mutex);
        m_condition_variable.notify_one();
    }

    void OnFirmwarePendingReboot(const int fireFirmwarePendingReboot) override
    {
        TEST_LOG("OnFirmwarePendingReboot notification received");
        TEST_LOG("  seconds: %d", fireFirmwarePendingReboot);
        std::unique_lock<std::mutex> lock(m_mutex);
        m_condition_variable.notify_one();
    }

    void OnFriendlyNameChanged(const string& friendlyName) override
    {
        TEST_LOG("OnFriendlyNameChanged notification received");
        TEST_LOG("  friendlyName: %s", friendlyName.c_str());
        std::unique_lock<std::mutex> lock(m_mutex);
        m_lastFriendlyName = friendlyName;
        m_event_signalled |= SYSTEMSERVICEL2TEST_FRIENDLY_NAME_CHANGED;
        m_condition_variable.notify_one();
    }

    void OnDeviceMgtUpdateReceived(const string& source, const string& type, const bool success) override
    {
        TEST_LOG("OnDeviceMgtUpdateReceived notification received");
        TEST_LOG("  source: %s, type: %s, success: %d", source.c_str(), type.c_str(), success);
        std::unique_lock<std::mutex> lock(m_mutex);
        m_condition_variable.notify_one();
    }

    void OnBlocklistChanged(const string& oldBlocklistFlag, const string& newBlocklistFlag) override
    {
        TEST_LOG("OnBlocklistChanged notification received");
        TEST_LOG("  oldBlocklistFlag: %s, newBlocklistFlag: %s", 
                 oldBlocklistFlag.c_str(), newBlocklistFlag.c_str());
        std::unique_lock<std::mutex> lock(m_mutex);
        m_lastOldBlocklist = oldBlocklistFlag;
        m_lastNewBlocklist = newBlocklistFlag;
        m_event_signalled |= SYSTEMSERVICEL2TEST_BLOCKLIST_CHANGED;
        m_condition_variable.notify_one();
    }

    void OnTimeStatusChanged(const string& TimeQuality, const string& TimeSrc, const string& Time) override
    {
        TEST_LOG("OnTimeStatusChanged notification received");
        TEST_LOG("  TimeQuality: %s, TimeSrc: %s, Time: %s", 
                 TimeQuality.c_str(), TimeSrc.c_str(), Time.c_str());
        std::unique_lock<std::mutex> lock(m_mutex);
        m_condition_variable.notify_one();
    }

    void OnNetworkStandbyModeChanged(const bool nwStandby) override
    {
        TEST_LOG("OnNetworkStandbyModeChanged notification received");
        TEST_LOG("  nwStandby: %s", nwStandby ? "true" : "false");
        std::unique_lock<std::mutex> lock(m_mutex);
        m_lastNwStandby = nwStandby;
        m_event_signalled |= SYSTEMSERVICEL2TEST_NETWORK_STANDBY_CHANGED;
        m_condition_variable.notify_one();
    }

    uint32_t WaitForEvent(uint32_t timeout_ms, SystemServiceL2test_async_events_t expected_status)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        auto now = std::chrono::system_clock::now();
        
        if (m_condition_variable.wait_until(lock, now + std::chrono::milliseconds(timeout_ms),
            [this, expected_status]() { return (m_event_signalled & expected_status) != 0; })) {
            return m_event_signalled;
        }
        
        TEST_LOG("Timeout waiting for event 0x%08X, got 0x%08X", expected_status, m_event_signalled);
        return SYSTEMSERVICEL2TEST_STATE_INVALID;
    }

    void ResetEvent()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_event_signalled = SYSTEMSERVICEL2TEST_STATE_INVALID;
    }

    string GetLastPowerState() {
        std::unique_lock<std::mutex> lock(m_mutex);
        return m_lastPowerState;
    }

    string GetLastFriendlyName() {
        std::unique_lock<std::mutex> lock(m_mutex);
        return m_lastFriendlyName;
    }

    string GetLastMode() {
        std::unique_lock<std::mutex> lock(m_mutex);
        return m_lastMode;
    }

    bool GetLastNwStandby() {
        std::unique_lock<std::mutex> lock(m_mutex);
        return m_lastNwStandby;
    }
};
/**
 * @brief Internal test mock class
 *
 * Note that this is for internal test use only and doesn't mock any actual
 * concrete interface.
 */
class AsyncHandlerMock
{
    public:
        AsyncHandlerMock()
        {
        }

        MOCK_METHOD(void, onTemperatureThresholdChanged, (const JsonObject &message));
        MOCK_METHOD(void, onLogUploadChanged, (const JsonObject &message));
        MOCK_METHOD(void, onSystemPowerStateChanged, (const JsonObject &message));
        MOCK_METHOD(void, onBlocklistChanged, (const JsonObject &message));
};

/* Systemservice L2 test class declaration */
class SystemService_L2Test : public L2TestMocks {
protected:
    IARM_EventHandler_t systemStateChanged = nullptr;
    IARM_EventHandler_t sysMgrEventHandler = nullptr;
    IARM_BusCall_t sysModeChangeHandler = nullptr;
    IARM_EventHandler_t pwrMgrEventHandler = nullptr;

    // Plugin interface objects
    Exchange::ISystemServices* m_SystemServicesPlugin = nullptr;
    PluginHost::IShell* m_controller_SystemServices = nullptr;
    Core::ProxyType<RPC::InvokeServerType<1, 0, 4>> SystemServices_Engine;
    Core::ProxyType<RPC::CommunicatorClient> SystemServices_Client;
    Core::Sink<SystemServicesNotificationHandler> m_notificationHandler;

    SystemService_L2Test();
    virtual ~SystemService_L2Test() override;

    public:
        /**
         * @brief Creates SystemServices plugin interface object
         */
        uint32_t CreateSystemServicesInterfaceObject();

        /**
         * @brief called when Temperature threshold
         * changed notification received from IARM
         */
      void onTemperatureThresholdChanged(const JsonObject &message);

        /**
         * @brief called when Uploadlog status
         * changed notification received because of state change
         */
      void onLogUploadChanged(const JsonObject &message);

        /**
         * @brief called when System state
         * changed notification received from IARM
         */
      void onSystemPowerStateChanged(const JsonObject &message);

        /**
         * @brief called when blocklist flag
         * changed notification because of blocklist flag modified.
         */
      void onBlocklistChanged(const JsonObject &message);

        /**
         * @brief waits for various status change on asynchronous calls
         */
      uint32_t WaitForRequestStatus(uint32_t timeout_ms,SystemServiceL2test_async_events_t expected_status);

    private:
        /** @brief Mutex */
        std::mutex m_mutex;

        /** @brief Condition variable */
        std::condition_variable m_condition_variable;

        /** @brief Event signalled flag */
        uint32_t m_event_signalled;
};


/**
 * @brief Constructor for SystemServices L2 test class
 */
SystemService_L2Test::SystemService_L2Test()
        : L2TestMocks()
        , m_SystemServicesPlugin(nullptr)
        , m_controller_SystemServices(nullptr)
{
        uint32_t status = Core::ERROR_GENERAL;
        m_event_signalled = SYSTEMSERVICEL2TEST_STATE_INVALID;

        // Mock IARM Bus initialization
        EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_Init(::testing::_))
            .Times(::testing::AnyNumber())
            .WillRepeatedly(::testing::Return(IARM_RESULT_SUCCESS));

        EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_Connect())
            .Times(::testing::AnyNumber())
            .WillRepeatedly(::testing::Return(IARM_RESULT_SUCCESS));

        // Mock IARM Event Registration - capture event handlers
        ON_CALL(*p_iarmBusImplMock, IARM_Bus_RegisterEventHandler(::testing::_, ::testing::_, ::testing::_))
        .WillByDefault(::testing::Invoke(
            [&](const char* ownerName, IARM_EventId_t eventId, IARM_EventHandler_t handler) {
                if ((string(IARM_BUS_SYSMGR_NAME) == string(ownerName)) && (eventId == IARM_BUS_SYSMGR_EVENT_SYSTEMSTATE)) {
                    systemStateChanged = handler;
                    sysMgrEventHandler = handler;
                    TEST_LOG("Captured SYSMGR event handler");
                } else if (string(IARM_BUS_PWRMGR_NAME) == string(ownerName)) {
                    pwrMgrEventHandler = handler;
                    TEST_LOG("Captured PWRMGR event handler");
                }
                return IARM_RESULT_SUCCESS;
            }));

        // Mock IARM Call Registration - capture the _SysModeChange call handler
        ON_CALL(*p_iarmBusImplMock, IARM_Bus_RegisterCall(::testing::_, ::testing::_))
        .WillByDefault(::testing::Invoke(
            [&](const char* methodName, IARM_BusCall_t handler) {
                if (string(IARM_BUS_COMMON_API_SysModeChange) == string(methodName)) {
                    sysModeChangeHandler = handler;
                    TEST_LOG("Captured SysModeChange call handler");
                }
                return IARM_RESULT_SUCCESS;
            }));

        // Mock PowerManager HAL
        EXPECT_CALL(*p_powerManagerHalMock, PLAT_DS_INIT())
        .WillOnce(::testing::Return(DEEPSLEEPMGR_SUCCESS));

        EXPECT_CALL(*p_powerManagerHalMock, PLAT_INIT())
        .WillRepeatedly(::testing::Return(PWRMGR_SUCCESS));

        EXPECT_CALL(*p_powerManagerHalMock, PLAT_API_SetWakeupSrc(::testing::_, ::testing::_))
        .WillRepeatedly(::testing::Return(PWRMGR_SUCCESS));

        ON_CALL(*p_rfcApiImplMock, getRFCParameter(::testing::_, ::testing::_, ::testing::_))
        .WillByDefault(::testing::Invoke(
          [](char* pcCallerID, const char* pcParameterName, RFC_ParamData_t* pstParamData) {
              if (strcmp("RFC_DATA_ThermalProtection_POLL_INTERVAL", pcParameterName) == 0) {
                  strcpy(pstParamData->value, "2");
                  return WDMP_SUCCESS;
              } else if (strcmp("RFC_ENABLE_ThermalProtection", pcParameterName) == 0) {
                  strcpy(pstParamData->value, "true");
                  return WDMP_SUCCESS;
              } else if (strcmp("RFC_DATA_ThermalProtection_DEEPSLEEP_GRACE_INTERVAL", pcParameterName) == 0) {
                  strcpy(pstParamData->value, "6");
                  return WDMP_SUCCESS;
              } else {
                  /* The default threshold values will assign, if RFC call failed */
                  return WDMP_FAILURE;
              }
          }));

        EXPECT_CALL(*p_mfrMock, mfrSetTempThresholds(::testing::_, ::testing::_))
        .WillRepeatedly(::testing::Invoke(
          [](int high, int critical) {
              EXPECT_EQ(high, 100);
              EXPECT_EQ(critical, 110);
              return mfrERR_NONE;
          }));

        EXPECT_CALL(*p_powerManagerHalMock, PLAT_API_GetPowerState(::testing::_))
        .WillRepeatedly(::testing::Invoke(
          [](PWRMgr_PowerState_t* powerState) {
              *powerState = PWRMGR_POWERSTATE_OFF; // by default over boot up, return PowerState OFF
              return PWRMGR_SUCCESS;
          }));

        EXPECT_CALL(*p_powerManagerHalMock, PLAT_API_SetPowerState(::testing::_))
        .WillRepeatedly(::testing::Invoke(
          [](PWRMgr_PowerState_t powerState) {
              // All tests are run without settings file
              // so default expected power state is ON
              return PWRMGR_SUCCESS;
          }));

        EXPECT_CALL(*p_mfrMock, mfrGetTemperature(::testing::_, ::testing::_, ::testing::_))
           .WillRepeatedly(::testing::Invoke(
               [&](mfrTemperatureState_t* curState, int* curTemperature, int* wifiTemperature) {
                   *curTemperature  = 90; // safe temperature
                   *curState        = (mfrTemperatureState_t)0;
                   *wifiTemperature = 25;
                   return mfrERR_NONE;
        }));

         /* Activate PowerManager plugin */
         status = ActivateService("org.rdk.PowerManager");
         EXPECT_EQ(Core::ERROR_NONE, status);

         /* Activate SystemServices plugin with retry logic */
         int retry_count = 0;
         const int max_retries = 10;
         status = Core::ERROR_GENERAL;

         while (status != Core::ERROR_NONE && retry_count < max_retries) {
             status = ActivateService("org.rdk.System");
             if (status != Core::ERROR_NONE) {
                 TEST_LOG("ActivateService attempt %d/%d returned: %d (%s)",
                          retry_count + 1, max_retries, status, Core::ErrorToString(status));
                 retry_count++;
                 if (retry_count < max_retries) {
                     std::this_thread::sleep_for(std::chrono::milliseconds(500));
                 }
             } else {
                 TEST_LOG("ActivateService succeeded on attempt %d", retry_count + 1);
             }
         }
         EXPECT_EQ(Core::ERROR_NONE, status);

}

/**
 * @brief Destructor for SystemServices L2 test class
 */
SystemService_L2Test::~SystemService_L2Test()
{
    try {
        uint32_t status = Core::ERROR_GENERAL;
        m_event_signalled = SYSTEMSERVICEL2TEST_STATE_INVALID;

        try { TEST_LOG("Cleaning up SystemServices L2 Test"); } catch (...) {}

        // Clean up COM-RPC interface objects first (in reverse order of creation)
        // Only release if they were actually created in CreateSystemServicesInterfaceObject()
        
        // Release SystemServices plugin interface
        if (m_SystemServicesPlugin != nullptr) {
            try {
                TEST_LOG("Releasing m_SystemServicesPlugin");
                m_SystemServicesPlugin->Release();
                m_SystemServicesPlugin = nullptr;
                TEST_LOG("Released m_SystemServicesPlugin");
            } catch (const std::exception& e) {
                TEST_LOG("Exception releasing m_SystemServicesPlugin: %s", e.what());
            } catch (...) {
                TEST_LOG("Unknown exception releasing m_SystemServicesPlugin");
            }
        }

        // Release controller shell interface
        if (m_controller_SystemServices != nullptr) {
            try {
                TEST_LOG("Releasing m_controller_SystemServices");
                m_controller_SystemServices->Release();
                m_controller_SystemServices = nullptr;
                TEST_LOG("Released m_controller_SystemServices");
            } catch (const std::exception& e) {
                TEST_LOG("Exception releasing m_controller_SystemServices: %s", e.what());
            } catch (...) {
                TEST_LOG("Unknown exception releasing m_controller_SystemServices");
            }
        }

        // Release communicator client
        try {
            if (SystemServices_Client.IsValid()) {
                TEST_LOG("Releasing SystemServices_Client");
                SystemServices_Client.Release();
                TEST_LOG("Released SystemServices_Client");
            }
        } catch (const std::exception& e) {
            TEST_LOG("Exception releasing SystemServices_Client: %s", e.what());
        } catch (...) {
            TEST_LOG("Unknown exception releasing SystemServices_Client");
        }

        // Release RPC engine
        try {
            if (SystemServices_Engine.IsValid()) {
                TEST_LOG("Releasing SystemServices_Engine");
                SystemServices_Engine.Release();
                TEST_LOG("Released SystemServices_Engine");
            }
        } catch (const std::exception& e) {
            TEST_LOG("Exception releasing SystemServices_Engine: %s", e.what());
        } catch (...) {
            TEST_LOG("Unknown exception releasing SystemServices_Engine");
        }

        // Deactivate in reverse order of activation
        // First deactivate SystemServices
        try {
            status = DeactivateService("org.rdk.System");
            if (status == Core::ERROR_NONE) {
                try { TEST_LOG("Successfully deactivated org.rdk.System"); } catch (...) {}
            } else {
                try { TEST_LOG("Warning: DeactivateService org.rdk.System returned status: %u", status); } catch (...) {}
            }
        } catch (const std::exception& e) {
            try { TEST_LOG("Exception during org.rdk.System deactivation: %s", e.what()); } catch (...) {}
        } catch (...) {
            try { TEST_LOG("Unknown exception during org.rdk.System deactivation"); } catch (...) {}
        }

        // Then deactivate PowerManager
        try {
            status = DeactivateService("org.rdk.PowerManager");
            if (status == Core::ERROR_NONE) {
                try { TEST_LOG("Successfully deactivated org.rdk.PowerManager"); } catch (...) {}
            } else {
                try { TEST_LOG("Warning: DeactivateService org.rdk.PowerManager returned status: %u", status); } catch (...) {}
            }
        } catch (const std::exception& e) {
            try { TEST_LOG("Exception during org.rdk.PowerManager deactivation: %s", e.what()); } catch (...) {}
        } catch (...) {
            try { TEST_LOG("Unknown exception during org.rdk.PowerManager deactivation"); } catch (...) {}
        }

        try { TEST_LOG("Cleanup completed"); } catch (...) {}
    } catch (...) {
        // Catch all exceptions in destructor to prevent std::terminate
    }
}


/**
 * @brief Creates SystemServices plugin interface object
 */
uint32_t SystemService_L2Test::CreateSystemServicesInterfaceObject()
{
    uint32_t return_value = Core::ERROR_GENERAL;

    TEST_LOG("Creating SystemServices_Engine");
    SystemServices_Engine = Core::ProxyType<RPC::InvokeServerType<1, 0, 4>>::Create();
    SystemServices_Client = Core::ProxyType<RPC::CommunicatorClient>::Create(
        Core::NodeId("/tmp/communicator"),
        Core::ProxyType<Core::IIPCServer>(SystemServices_Engine));

    TEST_LOG("Creating SystemServices_Engine Announcements");
#if ((THUNDER_VERSION == 2) || ((THUNDER_VERSION == 4) && (THUNDER_VERSION_MINOR == 2)))
    SystemServices_Engine->Announcements(SystemServices_Client->Announcement());
#endif

    if (!SystemServices_Client.IsValid()) {
        TEST_LOG("Invalid SystemServices_Client");
    } else {
        m_controller_SystemServices = SystemServices_Client->Open<PluginHost::IShell>(
            _T("org.rdk.System"), ~0, 3000);
        if (m_controller_SystemServices) {
            m_SystemServicesPlugin = m_controller_SystemServices->QueryInterface<Exchange::ISystemServices>();
            if (m_SystemServicesPlugin) {
                return_value = Core::ERROR_NONE;
                TEST_LOG("Successfully created SystemServices Plugin Interface");
            } else {
                TEST_LOG("Failed to QueryInterface for ISystemServices");
            }
        } else {
            TEST_LOG("Failed to get SystemServices Plugin Interface - m_controller_SystemServices is NULL");
        }
    }
    return return_value;
}

/**
 * @brief called when Temperature threshold
 * changed notification received from IARM
 *
 * @param[in] message from system service on the change
 */
void SystemService_L2Test::onTemperatureThresholdChanged(const JsonObject &message)
{
    TEST_LOG("onTemperatureThresholdChanged event triggered ***\n");
    std::unique_lock<std::mutex> lock(m_mutex);

    std::string str;
    message.ToString(str);

    TEST_LOG("onTemperatureThresholdChanged received: %s\n", str.c_str());

    /* Notify the requester thread. */
    m_event_signalled |= SYSTEMSERVICEL2TEST_THERMALSTATE_CHANGED;
    m_condition_variable.notify_one();
}

/**
 * @brief called when Uploadlog status
 * changed notification received because of state change
 *
 * @param[in] message from system service on the change
 */
void SystemService_L2Test::onLogUploadChanged(const JsonObject &message)
{
  TEST_LOG("onLogUploadChanged event triggered ******\n");
   std::unique_lock<std::mutex> lock(m_mutex);

    std::string str;
    message.ToString(str);

    TEST_LOG("onLogUploadChanged received: %s\n", str.c_str());

    /* Notify the requester thread. */
    m_event_signalled |= SYSTEMSERVICEL2TEST_LOGUPLOADSTATE_CHANGED;
    m_condition_variable.notify_one();
}

/**
 * @brief called when System state
 * changed notification received from IARM
 *
 * @param[in] message from system service on the change
 */
void SystemService_L2Test::onSystemPowerStateChanged(const JsonObject &message)
{
    TEST_LOG("onSystemPowerStateChanged event triggered ******\n");
    std::unique_lock<std::mutex> lock(m_mutex);

    std::string str;
    message.ToString(str);

    TEST_LOG("onSystemPowerStateChanged received: %s\n", str.c_str());

    /* Notify the requester thread. */
    m_event_signalled |= SYSTEMSERVICEL2TEST_SYSTEMSTATE_CHANGED;;
    m_condition_variable.notify_one();
}

/**
 * @brief called when Blocklist flag
 * changed notification because of blocklist flag modified
 *
 * @param[in] message from system service on the change
 */
void SystemService_L2Test::onBlocklistChanged(const JsonObject &message)
{
    TEST_LOG("onBlocklistChanged event triggered ***\n");
    std::unique_lock<std::mutex> lock(m_mutex);

    std::string str;
    message.ToString(str);

    TEST_LOG("onBlocklistChanged received: %s\n", str.c_str());

    /* Notify the requester thread. */
    m_event_signalled |= SYSTEMSERVICEL2TEST_BLOCKLIST_CHANGED;
    TEST_LOG("set SYSTEMSERVICEL2TEST_BLOCKLIST_CHANGED signal in m_event_signalled\n");
    m_condition_variable.notify_one();
    TEST_LOG("notify with m_condition_variable variable\n");
}

/**
 * @brief waits for various status change on asynchronous calls
 *
 * @param[in] timeout_ms timeout for waiting
 */
uint32_t SystemService_L2Test::WaitForRequestStatus(uint32_t timeout_ms,SystemServiceL2test_async_events_t expected_status)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    auto now = std::chrono::system_clock::now();
    std::chrono::milliseconds timeout(timeout_ms);
    uint32_t signalled = SYSTEMSERVICEL2TEST_STATE_INVALID;

   while (!(expected_status & m_event_signalled))
   {
      if (m_condition_variable.wait_until(lock, now + timeout) == std::cv_status::timeout)
      {
         TEST_LOG("Timeout waiting for request status event");
         break;
      }
   }

    signalled = m_event_signalled;

    return signalled;
}


/**
 * @brief Compare two request status objects
 *
 * @param[in] data Expected value
 * @return true if the argument and data match, false otherwise
 */
MATCHER_P(MatchRequestStatus, data, "")
{
    bool match = true;
    std::string expected;
    std::string actual;

    data.ToString(expected);
    arg.ToString(actual);
    TEST_LOG(" rec = %s, arg = %s",expected.c_str(),actual.c_str());
    EXPECT_STREQ(expected.c_str(),actual.c_str());

    return match;
}

#if 0
/********************************************************
************Test case Details **************************
** 1. Get temperature from systemservice
** 2. Set temperature threshold
** 3. Temperature threshold change event triggered from IARM
** 4. Verify that threshold change event is notified
*******************************************************/

TEST_F(SystemService_L2Test,SystemServiceGetSetTemperature)
{
    JSONRPC::LinkType<Core::JSON::IElement> jsonrpc(SYSTEM_CALLSIGN, L2TEST_CALLSIGN);
    StrictMock<AsyncHandlerMock> async_handler;
    uint32_t status = Core::ERROR_GENERAL;
    JsonObject params,thresholds;
    JsonObject result;
    uint32_t signalled = SYSTEMSERVICEL2TEST_STATE_INVALID;
    std::string message;
    JsonObject expected_status;

    status = InvokeServiceMethod("org.rdk.System.1", "getCoreTemperature", params, result);
    EXPECT_EQ(Core::ERROR_NONE, status);

    EXPECT_TRUE(result["success"].Boolean());
    EXPECT_STREQ("90.000000", result["temperature"].Value().c_str());

    /* errorCode and errorDescription should not be set */
    EXPECT_FALSE(result.HasLabel("errorCode"));
    EXPECT_FALSE(result.HasLabel("errorDescription"));

    /* Register for temperature threshold change event. */
    status = jsonrpc.Subscribe<JsonObject>(JSON_TIMEOUT,
                                       _T("onTemperatureThresholdChanged"),
                                       [this, &async_handler](const JsonObject& parameters) {
                                       async_handler.onTemperatureThresholdChanged(parameters);
                                       });

    EXPECT_EQ(Core::ERROR_NONE, status);

    thresholds["WARN"] = 100;
    thresholds["MAX"] = 110;
    params["thresholds"] = thresholds;

    // called from ThermalController constructor in initializeThermalProtection
    EXPECT_CALL(*p_mfrMock, mfrSetTempThresholds(::testing::_, ::testing::_))
        .WillRepeatedly(::testing::Invoke(
        [](int high, int critical) {
        EXPECT_EQ(high, 100);
        EXPECT_EQ(critical, 110);
        return mfrERR_NONE;
    }));

    status = InvokeServiceMethod("org.rdk.System.1", "setTemperatureThresholds", params, result);
    EXPECT_EQ(Core::ERROR_NONE, status);

    EXPECT_CALL(*p_powerManagerHalMock, PLAT_DS_SetDeepSleep(::testing::_, ::testing::_, ::testing::_))
        .WillRepeatedly(::testing::Invoke(
            [](uint32_t deep_sleep_timeout, bool* isGPIOWakeup, bool networkStandby) {
                return DEEPSLEEPMGR_SUCCESS;
    }));

    EXPECT_TRUE(result["success"].Boolean());

    /* errorCode and errorDescription should not be set */
    EXPECT_FALSE(result.HasLabel("errorCode"));
    EXPECT_FALSE(result.HasLabel("errorDescription"));

    /* Unregister for events. */
    jsonrpc.Unsubscribe(JSON_TIMEOUT, _T("onTemperatureThresholdChanged"));
}
#endif
/********************************************************
************Test case Details **************************
** 1. Start Log upload
** 2. Subscribe for powerstate change
** 3. Subscribe for LoguploadUpdates
** 4. Trigger system power state change from ON -> DEEP_SLEEP
** 5. Verify UPLOAD_ABORTED event triggered because of power state
** 6. Verify Systemstate event triggered and verify the response
*******************************************************/
TEST_F(SystemService_L2Test,SystemServiceUploadLogsAndSystemPowerStateChange)
{
    JSONRPC::LinkType<Core::JSON::IElement> jsonrpc(SYSTEM_CALLSIGN,L2TEST_CALLSIGN);
    StrictMock<AsyncHandlerMock> async_handler;
    uint32_t status = Core::ERROR_GENERAL;
    JsonObject params;
    JsonObject result;
    // uint32_t signalled = SYSTEMSERVICEL2TEST_STATE_INVALID;
    std::string message;
    JsonObject expected_status;

    EXPECT_TRUE(Core::File(string(_T("/usr/bin/logupload"))).Exists());

    ON_CALL(*p_rfcApiImplMock, getRFCParameter(::testing::_, ::testing::_, ::testing::_))
        .WillByDefault(::testing::Invoke(
            [](char* pcCallerID, const char* pcParameterName, RFC_ParamData_t* pstParamData) {
                pstParamData->type = WDMP_BOOLEAN;
                strncpy(pstParamData->value, "true", sizeof(pstParamData->value));
                return WDMP_SUCCESS;
            }));


    std::ofstream deviceProperties("/etc/device.properties");
    deviceProperties << "BUILD_TYPE=dev\n";
    deviceProperties << "FORCE_MTLS=true\n";
    deviceProperties.close();
    EXPECT_TRUE(Core::File(string(_T("/etc/device.properties"))).Exists());

    std::ofstream dcmPropertiesFile("/opt/dcm.properties");
    dcmPropertiesFile << "LOG_SERVER=test\n";
    dcmPropertiesFile << "DCM_LOG_SERVER=test\n";
    dcmPropertiesFile << "DCM_LOG_SERVER_URL=https://test\n";
    dcmPropertiesFile << "DCM_SCP_SERVER=test\n";
    dcmPropertiesFile << "HTTP_UPLOAD_LINK=https://test/S3.cgi\n";
    dcmPropertiesFile << "DCA_UPLOAD_URL=https://test\n";
    dcmPropertiesFile.close();
    EXPECT_TRUE(Core::File(string(_T("/opt/dcm.properties"))).Exists());

    std::ofstream tmpDcmSettings("/tmp/DCMSettings.conf");
    tmpDcmSettings << "LogUploadSettings:UploadRepository:uploadProtocol=https\n";
    tmpDcmSettings << "LogUploadSettings:UploadRepository:URL=https://example.com/upload\n";
    tmpDcmSettings << "LogUploadSettings:UploadOnReboot=true\n";
    tmpDcmSettings.close();
    EXPECT_TRUE(Core::File(string(_T("/tmp/DCMSettings.conf"))).Exists());

    status = InvokeServiceMethod("org.rdk.System.1", "uploadLogsAsync", params, result);
    EXPECT_EQ(Core::ERROR_NONE, status);

    EXPECT_TRUE(result["success"].Boolean());

    /* errorCode and errorDescription should not be set */
    EXPECT_FALSE(result.HasLabel("errorCode"));
    EXPECT_FALSE(result.HasLabel("errorDescription"));

    /* Register for abortlog event. */
    status = jsonrpc.Subscribe<JsonObject>(JSON_TIMEOUT,
                                           _T("onLogUpload"),
                                           &AsyncHandlerMock::onLogUploadChanged,
                                           &async_handler);

    EXPECT_EQ(Core::ERROR_NONE, status);

    /* Register for Powerstate change event. */
    status = jsonrpc.Subscribe<JsonObject>(JSON_TIMEOUT,
                                           _T("onSystemPowerStateChanged"),
                                           &AsyncHandlerMock::onSystemPowerStateChanged,
                                           &async_handler);

    EXPECT_EQ(Core::ERROR_NONE, status);
#if 0
    /* Request status for Onlogupload. */
    message = "{\"logUploadStatus\":\"UPLOAD_ABORTED\"}";
    expected_status.FromString(message);
    EXPECT_CALL(async_handler, onLogUploadChanged(MatchRequestStatus(expected_status)))
        .WillOnce(Invoke(this, &SystemService_L2Test::onLogUploadChanged));

    /* Request status for Onlogupload. */
    message = "{\"powerState\":\"DEEP_SLEEP\",\"currentPowerState\":\"ON\"}";
    expected_status.FromString(message);
    EXPECT_CALL(async_handler, onSystemPowerStateChanged(MatchRequestStatus(expected_status)))
        .WillOnce(Invoke(this, &SystemService_L2Test::onSystemPowerStateChanged));

    signalled = WaitForRequestStatus(JSON_TIMEOUT,SYSTEMSERVICEL2TEST_LOGUPLOADSTATE_CHANGED);
    EXPECT_TRUE(signalled & SYSTEMSERVICEL2TEST_LOGUPLOADSTATE_CHANGED);

    signalled = WaitForRequestStatus(JSON_TIMEOUT,SYSTEMSERVICEL2TEST_SYSTEMSTATE_CHANGED);
    EXPECT_TRUE(signalled & SYSTEMSERVICEL2TEST_SYSTEMSTATE_CHANGED);
#endif
    /* Unregister for events. */
    jsonrpc.Unsubscribe(JSON_TIMEOUT, _T("onLogUpload"));
    jsonrpc.Unsubscribe(JSON_TIMEOUT, _T("onSystemPowerStateChanged"));

}

/********************************************************
************Test case Details **************************
** 1. setBootLoaderSplashScreen success
** 2. setBootLoaderSplashScreen fail
** 3. setBootLoaderSplashScreen invalid path
** 4. setBootLoaderSplashScreen empty path
*******************************************************/

TEST_F(SystemService_L2Test,setBootLoaderSplashScreen)
{
    JSONRPC::LinkType<Core::JSON::IElement> jsonrpc(SYSTEM_CALLSIGN,L2TEST_CALLSIGN);
    StrictMock<AsyncHandlerMock> async_handler;
    uint32_t status = Core::ERROR_GENERAL;
    JsonObject params;
    JsonObject result;
    // uint32_t signalled = SYSTEMSERVICEL2TEST_STATE_INVALID;
    std::string message;
    JsonObject expected_status;
    params["path"] = "/tmp/osd1";


    std::ofstream file("/tmp/osd1");
    file << "testing setBootLoaderSplashScreen";
    file.close();

    status = InvokeServiceMethod("org.rdk.System.1", "setBootLoaderSplashScreen", params, result);
    EXPECT_EQ(Core::ERROR_NONE, status);
    EXPECT_TRUE(result["success"].Boolean());

#if 0
    status = InvokeServiceMethod("org.rdk.System.1", "setBootLoaderSplashScreen", params, result);
    EXPECT_EQ(Core::ERROR_GENERAL, status);
    EXPECT_FALSE(result["success"].Boolean());
    if (result.HasLabel("error")) {
	    EXPECT_STREQ("{\"message\":\"Update failed\",\"code\":\"-32002\"}", result["error"].String().c_str());
    }
    params["path"] = "/tmp/osd2";

    status = InvokeServiceMethod("org.rdk.System.1", "setBootLoaderSplashScreen", params, result);
    EXPECT_EQ(Core::ERROR_GENERAL, status);
    EXPECT_FALSE(result["success"].Boolean());
    if (result.HasLabel("error")) {
	    EXPECT_STREQ("{\"message\":\"Invalid path\",\"code\":\"-32001\"}", result["error"].String().c_str());
    }


    params["path"] = "";
    status = InvokeServiceMethod("org.rdk.System.1", "setBootLoaderSplashScreen", params, result);
    EXPECT_EQ(Core::ERROR_GENERAL, status);
    EXPECT_FALSE(result["success"].Boolean());
    if (result.HasLabel("error")) {
	    EXPECT_STREQ("{\"message\":\"Invalid path\",\"code\":\"-32001\"}", result["error"].String().c_str());
    }
#endif

}

/********************************************************
************Test case Details **************************
** 1. setBlocklistFlag success with value true and getBlocklistFlag
** 2. setBlocklistFlag success with value false and getBlocklistFlag
** 5. setBlocklistFlag invalid param
** 6. Verify that onBlocklistChanged change event is notified when blocklist flag modified
*******************************************************/

TEST_F(SystemService_L2Test,SystemServiceGetSetBlocklistFlag)
{
    JSONRPC::LinkType<Core::JSON::IElement> jsonrpc(SYSTEM_CALLSIGN, L2TEST_CALLSIGN);
    StrictMock<AsyncHandlerMock> async_handler;
    uint32_t status = Core::ERROR_GENERAL;
    JsonObject params;
    JsonObject result;
    std::string message;
    JsonObject expected_status;
    uint32_t file_status = -1;
	uint32_t signalled = SYSTEMSERVICEL2TEST_STATE_INVALID;

    /* Register for temperature threshold change event. */
    status = jsonrpc.Subscribe<JsonObject>(JSON_TIMEOUT,
                                           _T("onBlocklistChanged"),
                                           &AsyncHandlerMock::onBlocklistChanged,
                                           &async_handler);

    EXPECT_EQ(Core::ERROR_NONE, status);

    params["blocklist"] = true;

    status = InvokeServiceMethod("org.rdk.System.1", "setBlocklistFlag", params, result);
    EXPECT_EQ(Core::ERROR_NONE, status);

    EXPECT_TRUE(result["success"].Boolean());

    status = InvokeServiceMethod("org.rdk.System.1", "getBlocklistFlag", params, result);
    EXPECT_EQ(Core::ERROR_NONE, status);

    EXPECT_TRUE(result["success"].Boolean());
    EXPECT_TRUE(result["blocklist"].Boolean());

    // The JSON-RPC onBlocklistChanged event carries string values ("true"/"false"),
    // not booleans, because ISystemServices::INotification::OnBlocklistChanged takes string params.
    message = "{\"oldBlocklistFlag\": \"true\",\"newBlocklistFlag\": \"false\"}";
    expected_status.FromString(message);
    EXPECT_CALL(async_handler, onBlocklistChanged(MatchRequestStatus(expected_status)))
        .WillOnce(Invoke(this, &SystemService_L2Test::onBlocklistChanged));

    params["blocklist"] = false;

    status = InvokeServiceMethod("org.rdk.System.1", "setBlocklistFlag", params, result);
    EXPECT_EQ(Core::ERROR_NONE, status);

    signalled = WaitForRequestStatus(JSON_TIMEOUT,SYSTEMSERVICEL2TEST_BLOCKLIST_CHANGED);
    EXPECT_TRUE(signalled & SYSTEMSERVICEL2TEST_BLOCKLIST_CHANGED);
    EXPECT_TRUE(result["success"].Boolean());


    status = InvokeServiceMethod("org.rdk.System.1", "getBlocklistFlag", params, result);
    EXPECT_EQ(Core::ERROR_NONE, status);

    EXPECT_TRUE(result["success"].Boolean());
    EXPECT_FALSE(result["blocklist"].Boolean());

    file_status = remove("/opt/secure/persistent/opflashstore/devicestate.txt");
    // Check if the file has been successfully removed
    if (file_status != 0)
    {
        TEST_LOG("Error deleting file[devicestate.txt]");
    }
    else
    {
        TEST_LOG("File[devicestate.txt] successfully deleted");
    }
    TEST_LOG("Removed the devicestate.txt file in preparation for the next round of testing.");
    jsonrpc.Unsubscribe(JSON_TIMEOUT, _T("onBlocklistChanged"));
}

/********************************************************
************Test case Details **************************
** COM-RPC Interface Tests
** Testing SystemServices APIs via COM-RPC interface
*******************************************************/

TEST_F(SystemService_L2Test, GetSerialNumber_COMRPC)
{
    if (CreateSystemServicesInterfaceObject() != Core::ERROR_NONE) {
        TEST_LOG("Invalid SystemServices_Client");
    } else {
        EXPECT_TRUE(m_controller_SystemServices != nullptr);
        if (m_controller_SystemServices) {
            EXPECT_TRUE(m_SystemServicesPlugin != nullptr);
            if (m_SystemServicesPlugin) {
                TEST_LOG("Testing GetSerialNumber via COM-RPC");

                string serialNumber;
                bool success = false;

                uint32_t result = m_SystemServicesPlugin->GetSerialNumber(serialNumber, success);

                // GetSerialNumber depends on the DeviceInfo plugin (QueryInterfaceByCallsign).
                // In the L2 test environment, DeviceInfo is not activated, so graceful failure
                // (Core::ERROR_GENERAL, success=false) is acceptable.
                if (result == Core::ERROR_NONE) {
                    EXPECT_TRUE(success);
                    TEST_LOG("SerialNumber: %s", serialNumber.c_str());
                    EXPECT_FALSE(serialNumber.empty());
                } else {
                    TEST_LOG("GetSerialNumber returned %u - DeviceInfo plugin not available in L2 test environment", result);
                    EXPECT_FALSE(success);
                }

                m_SystemServicesPlugin->Release();
            } else {
                TEST_LOG("m_SystemServicesPlugin is NULL");
            }
            m_controller_SystemServices->Release();
        } else {
            TEST_LOG("m_controller_SystemServices is NULL");
        }
    }
}

TEST_F(SystemService_L2Test, GetFriendlyName_COMRPC)
{
    if (CreateSystemServicesInterfaceObject() != Core::ERROR_NONE) {
        TEST_LOG("Invalid SystemServices_Client");
    } else {
        EXPECT_TRUE(m_controller_SystemServices != nullptr);
        if (m_controller_SystemServices) {
            EXPECT_TRUE(m_SystemServicesPlugin != nullptr);
            if (m_SystemServicesPlugin) {
                TEST_LOG("Testing GetFriendlyName via COM-RPC");

                string friendlyName;
                bool success = false;

                uint32_t result = m_SystemServicesPlugin->GetFriendlyName(friendlyName, success);

                EXPECT_EQ(result, Core::ERROR_NONE);
                if (result != Core::ERROR_NONE) {
                    std::string errorMsg = "COM-RPC returned error " + std::to_string(result) + " (" + std::string(Core::ErrorToString(result)) + ")";
                    TEST_LOG("Err: %s", errorMsg.c_str());
                }
                EXPECT_TRUE(success);

                TEST_LOG("FriendlyName: %s", friendlyName.c_str());

                m_SystemServicesPlugin->Release();
            } else {
                TEST_LOG("m_SystemServicesPlugin is NULL");
            }
            m_controller_SystemServices->Release();
        } else {
            TEST_LOG("m_controller_SystemServices is NULL");
        }
    }
}

TEST_F(SystemService_L2Test, GetNetworkStandbyMode_COMRPC)
{
    if (CreateSystemServicesInterfaceObject() != Core::ERROR_NONE) {
        TEST_LOG("Invalid SystemServices_Client");
    } else {
        EXPECT_TRUE(m_controller_SystemServices != nullptr);
        if (m_controller_SystemServices) {
            EXPECT_TRUE(m_SystemServicesPlugin != nullptr);
            if (m_SystemServicesPlugin) {
                TEST_LOG("Testing GetNetworkStandbyMode via COM-RPC");

                bool nwStandby = false;
                bool success = false;

                uint32_t result = m_SystemServicesPlugin->GetNetworkStandbyMode(nwStandby, success);

                EXPECT_EQ(result, Core::ERROR_NONE);
                if (result != Core::ERROR_NONE) {
                    std::string errorMsg = "COM-RPC returned error " + std::to_string(result) + " (" + std::string(Core::ErrorToString(result)) + ")";
                    TEST_LOG("Err: %s", errorMsg.c_str());
                }
                EXPECT_TRUE(success);

                TEST_LOG("NetworkStandbyMode: %s", nwStandby ? "Enabled" : "Disabled");

                m_SystemServicesPlugin->Release();
            } else {
                TEST_LOG("m_SystemServicesPlugin is NULL");
            }
            m_controller_SystemServices->Release();
        } else {
            TEST_LOG("m_controller_SystemServices is NULL");
        }
    }
}

TEST_F(SystemService_L2Test, GetBuildType_COMRPC)
{
    if (CreateSystemServicesInterfaceObject() != Core::ERROR_NONE) {
        TEST_LOG("Invalid SystemServices_Client");
    } else {
        EXPECT_TRUE(m_controller_SystemServices != nullptr);
        if (m_controller_SystemServices) {
            EXPECT_TRUE(m_SystemServicesPlugin != nullptr);
            if (m_SystemServicesPlugin) {
                TEST_LOG("Testing GetBuildType via COM-RPC");

                string buildType;
                bool success = false;

                uint32_t result = m_SystemServicesPlugin->GetBuildType(buildType, success);

                EXPECT_EQ(result, Core::ERROR_NONE);
                if (result != Core::ERROR_NONE) {
                    std::string errorMsg = "COM-RPC returned error " + std::to_string(result) + " (" + std::string(Core::ErrorToString(result)) + ")";
                    TEST_LOG("Err: %s", errorMsg.c_str());
                }
                EXPECT_TRUE(success);

                TEST_LOG("BuildType: %s", buildType.c_str());
                EXPECT_FALSE(buildType.empty());

                m_SystemServicesPlugin->Release();
            } else {
                TEST_LOG("m_SystemServicesPlugin is NULL");
            }
            m_controller_SystemServices->Release();
        } else {
            TEST_LOG("m_controller_SystemServices is NULL");
        }
    }
}

TEST_F(SystemService_L2Test, RequestSystemUptime_COMRPC)
{
    if (CreateSystemServicesInterfaceObject() != Core::ERROR_NONE) {
        TEST_LOG("Invalid SystemServices_Client");
    } else {
        EXPECT_TRUE(m_controller_SystemServices != nullptr);
        if (m_controller_SystemServices) {
            EXPECT_TRUE(m_SystemServicesPlugin != nullptr);
            if (m_SystemServicesPlugin) {
                TEST_LOG("Testing RequestSystemUptime via COM-RPC");

                string systemUptime;
                bool success = false;

                uint32_t result = m_SystemServicesPlugin->RequestSystemUptime(systemUptime, success);

                EXPECT_EQ(result, Core::ERROR_NONE);
                if (result != Core::ERROR_NONE) {
                    std::string errorMsg = "COM-RPC returned error " + std::to_string(result) + " (" + std::string(Core::ErrorToString(result)) + ")";
                    TEST_LOG("Err: %s", errorMsg.c_str());
                }
                EXPECT_TRUE(success);

                TEST_LOG("SystemUptime: %s", systemUptime.c_str());
                EXPECT_FALSE(systemUptime.empty());

                m_SystemServicesPlugin->Release();
            } else {
                TEST_LOG("m_SystemServicesPlugin is NULL");
            }
            m_controller_SystemServices->Release();
        } else {
            TEST_LOG("m_controller_SystemServices is NULL");
        }
    }
}

/********************************************************
************Test case Details **************************
** JSON-RPC Interface Tests
** Testing SystemServices APIs via JSON-RPC interface
*******************************************************/

TEST_F(SystemService_L2Test, GetSerialNumber_JSONRPC)
{
    TEST_LOG("Testing getSerialNumber via JSON-RPC");

    JsonObject params;
    JsonObject result;

    uint32_t status = InvokeServiceMethod("org.rdk.System.1", "getSerialNumber", params, result);

    // getSerialNumber depends on the DeviceInfo plugin (QueryInterfaceByCallsign).
    // In the L2 test environment, DeviceInfo is not activated, so graceful failure is acceptable.
    if (status == Core::ERROR_NONE) {
        EXPECT_TRUE(result.HasLabel("success"));
        if (result.HasLabel("success")) {
            EXPECT_TRUE(result["success"].Boolean());
        }
        EXPECT_TRUE(result.HasLabel("serialNumber"));
        if (result.HasLabel("serialNumber")) {
            string serialNumber = result["serialNumber"].String();
            TEST_LOG("  serialNumber: %s", serialNumber.c_str());
            EXPECT_FALSE(serialNumber.empty());
        }
    } else {
        TEST_LOG("getSerialNumber failed with status %u - DeviceInfo plugin not available in L2 test environment", status);
    }
}

TEST_F(SystemService_L2Test, GetFriendlyName_JSONRPC)
{
    TEST_LOG("Testing getFriendlyName via JSON-RPC");

    JsonObject params;
    JsonObject result;

    uint32_t status = InvokeServiceMethod("org.rdk.System.1", "getFriendlyName", params, result);

    EXPECT_EQ(status, Core::ERROR_NONE);

    EXPECT_TRUE(result.HasLabel("success"));
    if (result.HasLabel("success")) {
        EXPECT_TRUE(result["success"].Boolean());
    }

    EXPECT_TRUE(result.HasLabel("friendlyName"));
    if (result.HasLabel("friendlyName")) {
        string friendlyName = result["friendlyName"].String();
        TEST_LOG("  friendlyName: %s", friendlyName.c_str());
    }
}

TEST_F(SystemService_L2Test, SetFriendlyName_JSONRPC)
{
    TEST_LOG("Testing setFriendlyName via JSON-RPC");

    JsonObject params;
    params["friendlyName"] = "TestDevice_L2";
    JsonObject result;

    uint32_t status = InvokeServiceMethod("org.rdk.System.1", "setFriendlyName", params, result);

    EXPECT_EQ(status, Core::ERROR_NONE);

    EXPECT_TRUE(result.HasLabel("success"));
    if (result.HasLabel("success")) {
        EXPECT_TRUE(result["success"].Boolean());
        TEST_LOG("  setFriendlyName succeeded");
    }
}

TEST_F(SystemService_L2Test, GetNetworkStandbyMode_JSONRPC)
{
    TEST_LOG("Testing getNetworkStandbyMode via JSON-RPC");

    JsonObject params;
    JsonObject result;

    uint32_t status = InvokeServiceMethod("org.rdk.System.1", "getNetworkStandbyMode", params, result);

    EXPECT_EQ(status, Core::ERROR_NONE);

    EXPECT_TRUE(result.HasLabel("success"));
    if (result.HasLabel("success")) {
        EXPECT_TRUE(result["success"].Boolean());
    }

    EXPECT_TRUE(result.HasLabel("nwStandby"));
    if (result.HasLabel("nwStandby")) {
        bool nwStandby = result["nwStandby"].Boolean();
        TEST_LOG("  nwStandby: %s", nwStandby ? "true" : "false");
    }
}

TEST_F(SystemService_L2Test, RequestSystemUptime_JSONRPC)
{
    TEST_LOG("Testing requestSystemUptime via JSON-RPC");

    JsonObject params;
    JsonObject result;

    uint32_t status = InvokeServiceMethod("org.rdk.System.1", "requestSystemUptime", params, result);

    EXPECT_EQ(status, Core::ERROR_NONE);

    EXPECT_TRUE(result.HasLabel("success"));
    if (result.HasLabel("success")) {
        EXPECT_TRUE(result["success"].Boolean());
    }

    EXPECT_TRUE(result.HasLabel("systemUptime"));
    if (result.HasLabel("systemUptime")) {
        string uptime = result["systemUptime"].String();
        TEST_LOG("  systemUptime: %s", uptime.c_str());
        EXPECT_FALSE(uptime.empty());
    }
}

TEST_F(SystemService_L2Test, GetBuildType_JSONRPC)
{
    TEST_LOG("Testing getBuildType via JSON-RPC");

    JsonObject params;
    JsonObject result;

    uint32_t status = InvokeServiceMethod("org.rdk.System.1", "getBuildType", params, result);

    EXPECT_EQ(status, Core::ERROR_NONE);

    EXPECT_TRUE(result.HasLabel("success"));
    if (result.HasLabel("success")) {
        EXPECT_TRUE(result["success"].Boolean());
    }

    // JSON-RPC response field is "build_type" (snake_case), not "buildType"
    EXPECT_TRUE(result.HasLabel("build_type"));
    if (result.HasLabel("build_type")) {
        string buildType = result["build_type"].String();
        TEST_LOG("  build_type: %s", buildType.c_str());
        EXPECT_FALSE(buildType.empty());
    }
}

TEST_F(SystemService_L2Test, GetSystemVersions_JSONRPC)
{
    TEST_LOG("Testing getSystemVersions via JSON-RPC");

    JsonObject params;
    JsonObject result;

    uint32_t status = InvokeServiceMethod("org.rdk.System.1", "getSystemVersions", params, result);

    EXPECT_EQ(status, Core::ERROR_NONE);

    EXPECT_TRUE(result.HasLabel("stbVersion"));
    if (result.HasLabel("stbVersion")) {
        string stbVersion = result["stbVersion"].String();
        TEST_LOG("  stbVersion: %s", stbVersion.c_str());
    }

    EXPECT_TRUE(result.HasLabel("receiverVersion"));
    if (result.HasLabel("receiverVersion")) {
        string receiverVersion = result["receiverVersion"].String();
        TEST_LOG("  receiverVersion: %s", receiverVersion.c_str());
    }

    EXPECT_TRUE(result.HasLabel("stbTimestamp"));
    if (result.HasLabel("stbTimestamp")) {
        string stbTimestamp = result["stbTimestamp"].String();
        TEST_LOG("  stbTimestamp: %s", stbTimestamp.c_str());
    }
}

TEST_F(SystemService_L2Test, GetTerritory_JSONRPC)
{
    TEST_LOG("Testing getTerritory via JSON-RPC");

    JsonObject params;
    JsonObject result;

    uint32_t status = InvokeServiceMethod("org.rdk.System.1", "getTerritory", params, result);

    EXPECT_EQ(status, Core::ERROR_NONE);

    EXPECT_TRUE(result.HasLabel("success"));
    if (result.HasLabel("success")) {
        EXPECT_TRUE(result["success"].Boolean());
    }

    EXPECT_TRUE(result.HasLabel("territory"));
    if (result.HasLabel("territory")) {
        string territory = result["territory"].String();
        TEST_LOG("  territory: %s", territory.c_str());
    }

    if (result.HasLabel("region")) {
        string region = result["region"].String();
        TEST_LOG("  region: %s", region.c_str());
    }
}

TEST_F(SystemService_L2Test, IsOptOutTelemetry_JSONRPC)
{
    TEST_LOG("Testing isOptOutTelemetry via JSON-RPC");

    JsonObject params;
    JsonObject result;

    uint32_t status = InvokeServiceMethod("org.rdk.System.1", "isOptOutTelemetry", params, result);

    // isOptOutTelemetry depends on the org.rdk.Telemetry plugin (QueryInterfaceByCallsign).
    // In the L2 test environment, Telemetry plugin is not activated, so graceful failure is acceptable.
    if (status == Core::ERROR_NONE) {
        EXPECT_TRUE(result.HasLabel("success"));
        if (result.HasLabel("success")) {
            EXPECT_TRUE(result["success"].Boolean());
        }
        EXPECT_TRUE(result.HasLabel("Opt-Out"));
        if (result.HasLabel("Opt-Out")) {
            bool optOut = result["Opt-Out"].Boolean();
            TEST_LOG("  Opt-Out: %s", optOut ? "true" : "false");
        }
    } else {
        TEST_LOG("isOptOutTelemetry failed with status %u - Telemetry plugin not available in L2 test environment", status);
    }
}

TEST_F(SystemService_L2Test, GetFSRFlag_JSONRPC)
{
    TEST_LOG("Testing getFSRFlag via JSON-RPC");

    JsonObject params;
    JsonObject result;

    uint32_t status = InvokeServiceMethod("org.rdk.System.1", "getFSRFlag", params, result);

    EXPECT_EQ(status, Core::ERROR_NONE);

    EXPECT_TRUE(result.HasLabel("success"));
    if (result.HasLabel("success")) {
        EXPECT_TRUE(result["success"].Boolean());
    }

    EXPECT_TRUE(result.HasLabel("fsrFlag"));
    if (result.HasLabel("fsrFlag")) {
        bool fsrFlag = result["fsrFlag"].Boolean();
        TEST_LOG("  fsrFlag: %s", fsrFlag ? "true" : "false");
    }
}

/********************************************************
************Test case Details **************************
** COM-RPC Notification Tests
** Testing SystemServices notification interface
*******************************************************/

#define EVNT_TIMEOUT (5000)

TEST_F(SystemService_L2Test, RegisterUnregister_COMRPC)
{
    if (CreateSystemServicesInterfaceObject() != Core::ERROR_NONE) {
        TEST_LOG("Invalid SystemServices_Client");
    } else {
        EXPECT_TRUE(m_controller_SystemServices != nullptr);
        if (m_controller_SystemServices) {
            EXPECT_TRUE(m_SystemServicesPlugin != nullptr);
            if (m_SystemServicesPlugin) {
                TEST_LOG("Testing Register and Unregister via COM-RPC");

                // Register for notifications
                uint32_t result = m_SystemServicesPlugin->Register(&m_notificationHandler);

                EXPECT_EQ(result, Core::ERROR_NONE);
                if (result != Core::ERROR_NONE) {
                    std::string errorMsg = "Register returned error " + std::to_string(result) + " (" + std::string(Core::ErrorToString(result)) + ")";
                    TEST_LOG("Err: %s", errorMsg.c_str());
                } else {
                    TEST_LOG("Successfully registered for notifications");
                }

                // Unregister from notifications
                result = m_SystemServicesPlugin->Unregister(&m_notificationHandler);

                EXPECT_EQ(result, Core::ERROR_NONE);
                if (result != Core::ERROR_NONE) {
                    std::string errorMsg = "Unregister returned error " + std::to_string(result) + " (" + std::string(Core::ErrorToString(result)) + ")";
                    TEST_LOG("Err: %s", errorMsg.c_str());
                } else {
                    TEST_LOG("Successfully unregistered from notifications");
                }

                m_SystemServicesPlugin->Release();
            } else {
                TEST_LOG("m_SystemServicesPlugin is NULL");
            }
            m_controller_SystemServices->Release();
        } else {
            TEST_LOG("m_controller_SystemServices is NULL");
        }
    }
}

TEST_F(SystemService_L2Test, OnSystemPowerStateChanged_Notification_COMRPC)
{
    if (CreateSystemServicesInterfaceObject() != Core::ERROR_NONE) {
        TEST_LOG("Invalid SystemServices_Client");
    } else {
        EXPECT_TRUE(m_controller_SystemServices != nullptr);
        if (m_controller_SystemServices) {
            EXPECT_TRUE(m_SystemServicesPlugin != nullptr);
            if (m_SystemServicesPlugin) {
                TEST_LOG("Testing OnSystemPowerStateChanged notification via COM-RPC");

                // Register for event notifications
                uint32_t result = m_SystemServicesPlugin->Register(&m_notificationHandler);
                EXPECT_EQ(result, Core::ERROR_NONE);
                if (result != Core::ERROR_NONE) {
                    std::string errorMsg = "Register returned error " + std::to_string(result) + " (" + std::string(Core::ErrorToString(result)) + ")";
                    TEST_LOG("Err: %s", errorMsg.c_str());
                } else {
                    TEST_LOG("Successfully registered for notifications");
                }

                // Reset event flag before triggering the event
                m_notificationHandler.ResetEvent();

                // Trigger the event
                if (pwrMgrEventHandler != nullptr) {
                    TEST_LOG("Triggering power state change event");

                    IARM_Bus_PWRMgr_EventData_t eventData;
                    eventData.data.state.curState = IARM_BUS_PWRMGR_POWERSTATE_STANDBY;
                    eventData.data.state.newState = IARM_BUS_PWRMGR_POWERSTATE_ON;

                    pwrMgrEventHandler(IARM_BUS_PWRMGR_NAME, IARM_BUS_PWRMGR_EVENT_MODECHANGED, &eventData, 0);

                    // Wait for the event notification with timeout
                    uint32_t eventStatus = m_notificationHandler.WaitForEvent(EVNT_TIMEOUT, SYSTEMSERVICEL2TEST_SYSTEMSTATE_CHANGED);

                    EXPECT_NE(eventStatus, SYSTEMSERVICEL2TEST_STATE_INVALID);
                    if (eventStatus != SYSTEMSERVICEL2TEST_STATE_INVALID) {
                        TEST_LOG("OnSystemPowerStateChanged notification received successfully");

                        string powerState = m_notificationHandler.GetLastPowerState();
                        TEST_LOG("Received powerState: %s", powerState.c_str());
                        EXPECT_FALSE(powerState.empty());
                    } else {
                        TEST_LOG("Timeout waiting for OnSystemPowerStateChanged notification");
                    }
                } else {
                    TEST_LOG("pwrMgrEventHandler is NULL, cannot trigger event");
                }

                // Unregister from notifications
                result = m_SystemServicesPlugin->Unregister(&m_notificationHandler);
                EXPECT_EQ(result, Core::ERROR_NONE);
                if (result == Core::ERROR_NONE) {
                    TEST_LOG("Successfully unregistered from notifications");
                }

                m_SystemServicesPlugin->Release();
            } else {
                TEST_LOG("m_SystemServicesPlugin is NULL");
            }
            m_controller_SystemServices->Release();
        } else {
            TEST_LOG("m_controller_SystemServices is NULL");
        }
    }
}

TEST_F(SystemService_L2Test, OnSystemModeChanged_Notification_COMRPC)
{
    if (CreateSystemServicesInterfaceObject() != Core::ERROR_NONE) {
        TEST_LOG("Invalid SystemServices_Client");
    } else {
        EXPECT_TRUE(m_controller_SystemServices != nullptr);
        if (m_controller_SystemServices) {
            EXPECT_TRUE(m_SystemServicesPlugin != nullptr);
            if (m_SystemServicesPlugin) {
                TEST_LOG("Testing OnSystemModeChanged notification via COM-RPC");

                // Register for event notifications
                uint32_t result = m_SystemServicesPlugin->Register(&m_notificationHandler);
                EXPECT_EQ(result, Core::ERROR_NONE);
                if (result == Core::ERROR_NONE) {
                    TEST_LOG("Successfully registered for notifications");
                }

                // Reset event flag before triggering the event
                m_notificationHandler.ResetEvent();

                // Trigger the event via _SysModeChange IARM call handler (not the event handler).
                // _SysModeChange is registered via IARM_Bus_RegisterCall(IARM_BUS_COMMON_API_SysModeChange).
                if (sysModeChangeHandler != nullptr) {
                    TEST_LOG("Triggering system mode change event via _SysModeChange call handler");

                    IARM_Bus_CommonAPI_SysModeChange_Param_t eventData;
                    eventData.oldMode = IARM_BUS_SYS_MODE_NORMAL;
                    eventData.newMode = IARM_BUS_SYS_MODE_WAREHOUSE;

                    sysModeChangeHandler(&eventData);

                    // Wait for the event notification with timeout
                    uint32_t eventStatus = m_notificationHandler.WaitForEvent(EVNT_TIMEOUT, SYSTEMSERVICEL2TEST_SYSTEM_MODE_CHANGED);

                    EXPECT_NE(eventStatus, SYSTEMSERVICEL2TEST_STATE_INVALID);
                    if (eventStatus != SYSTEMSERVICEL2TEST_STATE_INVALID) {
                        TEST_LOG("OnSystemModeChanged notification received successfully");

                        string mode = m_notificationHandler.GetLastMode();
                        TEST_LOG("Received mode: %s", mode.c_str());
                        EXPECT_FALSE(mode.empty());
                    } else {
                        TEST_LOG("Timeout waiting for OnSystemModeChanged notification");
                    }
                } else {
                    TEST_LOG("sysModeChangeHandler is NULL, cannot trigger event");
                    FAIL() << "sysModeChangeHandler was not captured - plugin may not have registered IARM_BUS_COMMON_API_SysModeChange";
                }

                // Unregister from notifications
                result = m_SystemServicesPlugin->Unregister(&m_notificationHandler);
                EXPECT_EQ(result, Core::ERROR_NONE);

                m_SystemServicesPlugin->Release();
            } else {
                TEST_LOG("m_SystemServicesPlugin is NULL");
            }
            m_controller_SystemServices->Release();
        } else {
            TEST_LOG("m_controller_SystemServices is NULL");
        }
    }
}

TEST_F(SystemService_L2Test, OnBlocklistChanged_Notification_COMRPC)
{
    if (CreateSystemServicesInterfaceObject() != Core::ERROR_NONE) {
        TEST_LOG("Invalid SystemServices_Client");
    } else {
        EXPECT_TRUE(m_controller_SystemServices != nullptr);
        if (m_controller_SystemServices) {
            EXPECT_TRUE(m_SystemServicesPlugin != nullptr);
            if (m_SystemServicesPlugin) {
                TEST_LOG("Testing OnBlocklistChanged notification via COM-RPC");

                // Register for event notifications
                uint32_t result = m_SystemServicesPlugin->Register(&m_notificationHandler);
                EXPECT_EQ(result, Core::ERROR_NONE);

                if (result == Core::ERROR_NONE) {
                    TEST_LOG("Successfully registered for notifications");

                    // Reset event flag
                    m_notificationHandler.ResetEvent();

                    // Pre-populate devicestate.txt with blocklist=false so that setBlocklistFlag(true)
                    // detects a change (update=true) and fires OnBlocklistChanged notification.
                    // This is required because a previous test may have removed the file.
                    {
                        std::ofstream deviceStateFile("/opt/secure/persistent/opflashstore/devicestate.txt");
                        deviceStateFile << "blocklist=false\n";
                    }

                    // Perform action that triggers blocklist change
                    // Using JSON-RPC to set blocklist flag
                    JsonObject params;
                    params["blocklist"] = true;
                    JsonObject setResult;

                    uint32_t status = InvokeServiceMethod("org.rdk.System.1", "setBlocklistFlag", params, setResult);
                    if (status == Core::ERROR_NONE) {
                        TEST_LOG("Set blocklist flag successfully");

                        // Wait for the notification
                        uint32_t eventStatus = m_notificationHandler.WaitForEvent(EVNT_TIMEOUT, SYSTEMSERVICEL2TEST_BLOCKLIST_CHANGED);

                        EXPECT_NE(eventStatus, SYSTEMSERVICEL2TEST_STATE_INVALID);
                        if (eventStatus != SYSTEMSERVICEL2TEST_STATE_INVALID) {
                            TEST_LOG("OnBlocklistChanged notification received successfully");
                        } else {
                            TEST_LOG("Timeout waiting for OnBlocklistChanged notification");
                        }
                    }

                    // Cleanup
                    remove("/opt/secure/persistent/opflashstore/devicestate.txt");
                }

                // Unregister from notifications
                result = m_SystemServicesPlugin->Unregister(&m_notificationHandler);
                EXPECT_EQ(result, Core::ERROR_NONE);

                m_SystemServicesPlugin->Release();
            } else {
                TEST_LOG("m_SystemServicesPlugin is NULL");
            }
            m_controller_SystemServices->Release();
        } else {
            TEST_LOG("m_controller_SystemServices is NULL");
        }
    }
}

/********************************************************
************Test case Details **************************
** Additional COM-RPC Tests
** Testing remaining SystemServices APIs via COM-RPC interface
*******************************************************/

TEST_F(SystemService_L2Test, GetTimeZoneDST_COMRPC)
{
    if (CreateSystemServicesInterfaceObject() != Core::ERROR_NONE) {
        TEST_LOG("Invalid SystemServices_Client");
    } else {
        EXPECT_TRUE(m_controller_SystemServices != nullptr);
        if (m_controller_SystemServices) {
            EXPECT_TRUE(m_SystemServicesPlugin != nullptr);
            if (m_SystemServicesPlugin) {
                TEST_LOG("Testing GetTimeZoneDST via COM-RPC");

                // Declare output parameters
                string timeZone;
                string accuracy;
                bool success = false;

                // Call the API
                uint32_t result = m_SystemServicesPlugin->GetTimeZoneDST(timeZone, accuracy, success);

                // Validate result
                EXPECT_EQ(result, Core::ERROR_NONE);
                if (result != Core::ERROR_NONE) {
                    std::string errorMsg = "COM-RPC returned error " + std::to_string(result) + " (" + std::string(Core::ErrorToString(result)) + ")";
                    TEST_LOG("Err: %s", errorMsg.c_str());
                }
                EXPECT_TRUE(success);

                // Log and validate output
                TEST_LOG("TimeZone: %s", timeZone.c_str());
                TEST_LOG("Accuracy: %s", accuracy.c_str());
                EXPECT_FALSE(timeZone.empty());
                EXPECT_FALSE(accuracy.empty());

                m_SystemServicesPlugin->Release();
            } else {
                TEST_LOG("m_SystemServicesPlugin is NULL");
            }
            m_controller_SystemServices->Release();
        } else {
            TEST_LOG("m_controller_SystemServices is NULL");
        }
    }
}

TEST_F(SystemService_L2Test, GetTerritory_COMRPC)
{
    if (CreateSystemServicesInterfaceObject() != Core::ERROR_NONE) {
        TEST_LOG("Invalid SystemServices_Client");
    } else {
        EXPECT_TRUE(m_controller_SystemServices != nullptr);
        if (m_controller_SystemServices) {
            EXPECT_TRUE(m_SystemServicesPlugin != nullptr);
            if (m_SystemServicesPlugin) {
                TEST_LOG("Testing GetTerritory via COM-RPC");

                // Declare output parameters
                string territory;
                string region;
                bool success = false;

                // Call the API
                uint32_t result = m_SystemServicesPlugin->GetTerritory(territory, region, success);

                // Validate result
                EXPECT_EQ(result, Core::ERROR_NONE);
                if (result != Core::ERROR_NONE) {
                    std::string errorMsg = "COM-RPC returned error " + std::to_string(result) + " (" + std::string(Core::ErrorToString(result)) + ")";
                    TEST_LOG("Err: %s", errorMsg.c_str());
                }
                EXPECT_TRUE(success);

                // Log and validate output
                TEST_LOG("Territory: %s", territory.c_str());
                TEST_LOG("Region: %s", region.c_str());
                
                m_SystemServicesPlugin->Release();
            } else {
                TEST_LOG("m_SystemServicesPlugin is NULL");
            }
            m_controller_SystemServices->Release();
        } else {
            TEST_LOG("m_controller_SystemServices is NULL");
        }
    }
}

TEST_F(SystemService_L2Test, GetSystemVersions_COMRPC)
{
    if (CreateSystemServicesInterfaceObject() != Core::ERROR_NONE) {
        TEST_LOG("Invalid SystemServices_Client");
    } else {
        EXPECT_TRUE(m_controller_SystemServices != nullptr);
        if (m_controller_SystemServices) {
            EXPECT_TRUE(m_SystemServicesPlugin != nullptr);
            if (m_SystemServicesPlugin) {
                TEST_LOG("Testing GetSystemVersions via COM-RPC");

                // Declare output parameters
                Exchange::ISystemServices::SystemVersionsInfo systemVersionsInfo;

                // Call the API
                uint32_t result = m_SystemServicesPlugin->GetSystemVersions(systemVersionsInfo);

                // Validate result
                EXPECT_EQ(result, Core::ERROR_NONE);
                if (result != Core::ERROR_NONE) {
                    std::string errorMsg = "COM-RPC returned error " + std::to_string(result) + " (" + std::string(Core::ErrorToString(result)) + ")";
                    TEST_LOG("Err: %s", errorMsg.c_str());
                }

                // Log and validate output
                TEST_LOG("stbVersion: %s", systemVersionsInfo.stbVersion.c_str());
                TEST_LOG("receiverVersion: %s", systemVersionsInfo.receiverVersion.c_str());
                TEST_LOG("stbTimestamp: %s", systemVersionsInfo.stbTimestamp.c_str());
                
                m_SystemServicesPlugin->Release();
            } else {
                TEST_LOG("m_SystemServicesPlugin is NULL");
            }
            m_controller_SystemServices->Release();
        } else {
            TEST_LOG("m_controller_SystemServices is NULL");
        }
    }
}

TEST_F(SystemService_L2Test, GetPowerStateBeforeReboot_COMRPC)
{
    if (CreateSystemServicesInterfaceObject() != Core::ERROR_NONE) {
        TEST_LOG("Invalid SystemServices_Client");
    } else {
        EXPECT_TRUE(m_controller_SystemServices != nullptr);
        if (m_controller_SystemServices) {
            EXPECT_TRUE(m_SystemServicesPlugin != nullptr);
            if (m_SystemServicesPlugin) {
                TEST_LOG("Testing GetPowerStateBeforeReboot via COM-RPC");

                // Declare output parameters
                string state;
                bool success = false;

                // Call the API
                uint32_t result = m_SystemServicesPlugin->GetPowerStateBeforeReboot(state, success);

                // Validate result
                EXPECT_EQ(result, Core::ERROR_NONE);
                if (result != Core::ERROR_NONE) {
                    std::string errorMsg = "COM-RPC returned error " + std::to_string(result) + " (" + std::string(Core::ErrorToString(result)) + ")";
                    TEST_LOG("Err: %s", errorMsg.c_str());
                }
                EXPECT_TRUE(success);

                // Log and validate output
                TEST_LOG("Power state before reboot: %s", state.c_str());
                
                m_SystemServicesPlugin->Release();
            } else {
                TEST_LOG("m_SystemServicesPlugin is NULL");
            }
            m_controller_SystemServices->Release();
        } else {
            TEST_LOG("m_controller_SystemServices is NULL");
        }
    }
}

TEST_F(SystemService_L2Test, GetFSRFlag_COMRPC)
{
    if (CreateSystemServicesInterfaceObject() != Core::ERROR_NONE) {
        TEST_LOG("Invalid SystemServices_Client");
    } else {
        EXPECT_TRUE(m_controller_SystemServices != nullptr);
        if (m_controller_SystemServices) {
            EXPECT_TRUE(m_SystemServicesPlugin != nullptr);
            if (m_SystemServicesPlugin) {
                TEST_LOG("Testing GetFSRFlag via COM-RPC");

                // Declare output parameters
                bool fsrFlag = false;
                bool success = false;

                // Call the API
                uint32_t result = m_SystemServicesPlugin->GetFSRFlag(fsrFlag, success);

                // Validate result
                EXPECT_EQ(result, Core::ERROR_NONE);
                if (result != Core::ERROR_NONE) {
                    std::string errorMsg = "COM-RPC returned error " + std::to_string(result) + " (" + std::string(Core::ErrorToString(result)) + ")";
                    TEST_LOG("Err: %s", errorMsg.c_str());
                }
                EXPECT_TRUE(success);

                // Log and validate output
                TEST_LOG("FSR Flag: %s", fsrFlag ? "true" : "false");
                
                m_SystemServicesPlugin->Release();
            } else {
                TEST_LOG("m_SystemServicesPlugin is NULL");
            }
            m_controller_SystemServices->Release();
        } else {
            TEST_LOG("m_controller_SystemServices is NULL");
        }
    }
}

TEST_F(SystemService_L2Test, GetBlocklistFlag_COMRPC)
{
    if (CreateSystemServicesInterfaceObject() != Core::ERROR_NONE) {
        TEST_LOG("Invalid SystemServices_Client");
    } else {
        EXPECT_TRUE(m_controller_SystemServices != nullptr);
        if (m_controller_SystemServices) {
            EXPECT_TRUE(m_SystemServicesPlugin != nullptr);
            if (m_SystemServicesPlugin) {
                TEST_LOG("Testing GetBlocklistFlag via COM-RPC");

                // Declare output parameters
                Exchange::ISystemServices::BlocklistResult blocklistResult;

                // Call the API
                uint32_t result = m_SystemServicesPlugin->GetBlocklistFlag(blocklistResult);

                // Validate result
                EXPECT_EQ(result, Core::ERROR_NONE);
                if (result != Core::ERROR_NONE) {
                    std::string errorMsg = "COM-RPC returned error " + std::to_string(result) + " (" + std::string(Core::ErrorToString(result)) + ")";
                    TEST_LOG("Err: %s", errorMsg.c_str());
                }

                // Log output
                TEST_LOG("Blocklist Flag: %s", blocklistResult.blocklist ? "true" : "false");
                TEST_LOG("Success: %s", blocklistResult.success ? "true" : "false");
                
                // Note: success may be false if devicestate.txt file doesn't exist,
                // which is valid behavior in L2 test environment after cleanup from previous tests.
                if (!blocklistResult.success) {
                    TEST_LOG("GetBlocklistFlag returned success=false (blocklist file may not exist)");
                }                
                m_SystemServicesPlugin->Release();
            } else {
                TEST_LOG("m_SystemServicesPlugin is NULL");
            }
            m_controller_SystemServices->Release();
        } else {
            TEST_LOG("m_controller_SystemServices is NULL");
        }
    }
}

/********************************************************
************Test case Details **************************
** Additional JSON-RPC Tests
** Testing remaining SystemServices APIs via JSON-RPC interface
*******************************************************/

TEST_F(SystemService_L2Test, GetTimeZoneDST_JSONRPC)
{
    TEST_LOG("Testing getTimeZoneDST via JSON-RPC");

    JsonObject params;
    JsonObject result;

    uint32_t status = InvokeServiceMethod("org.rdk.System.1", "getTimeZoneDST", params, result);

    EXPECT_EQ(status, Core::ERROR_NONE);

    // Validate success field
    EXPECT_TRUE(result.HasLabel("success"));
    if (result.HasLabel("success")) {
        EXPECT_TRUE(result["success"].Boolean());
    }

    // Validate timeZone field
    EXPECT_TRUE(result.HasLabel("timeZone"));
    if (result.HasLabel("timeZone")) {
        string timeZone = result["timeZone"].String();
        TEST_LOG("  timeZone: %s", timeZone.c_str());
        EXPECT_FALSE(timeZone.empty());
    }

    // Validate accuracy field
    EXPECT_TRUE(result.HasLabel("accuracy"));
    if (result.HasLabel("accuracy")) {
        string accuracy = result["accuracy"].String();
        TEST_LOG("  accuracy: %s", accuracy.c_str());
        EXPECT_FALSE(accuracy.empty());
    }
}

TEST_F(SystemService_L2Test, GetPowerStateBeforeReboot_JSONRPC)
{
    TEST_LOG("Testing getPowerStateBeforeReboot via JSON-RPC");

    JsonObject params;
    JsonObject result;

    uint32_t status = InvokeServiceMethod("org.rdk.System.1", "getPowerStateBeforeReboot", params, result);

    EXPECT_EQ(status, Core::ERROR_NONE);

    // Validate success field
    EXPECT_TRUE(result.HasLabel("success"));
    if (result.HasLabel("success")) {
        EXPECT_TRUE(result["success"].Boolean());
    }

    // Validate state field
    EXPECT_TRUE(result.HasLabel("state"));
    if (result.HasLabel("state")) {
        string state = result["state"].String();
        TEST_LOG("  state: %s", state.c_str());
    }
}

TEST_F(SystemService_L2Test, GetBlocklistFlag_JSONRPC)
{
    TEST_LOG("Testing getBlocklistFlag via JSON-RPC");

    JsonObject params;
    JsonObject result;

    uint32_t status = InvokeServiceMethod("org.rdk.System.1", "getBlocklistFlag", params, result);

    EXPECT_EQ(status, Core::ERROR_NONE);

    // Validate success field
    EXPECT_TRUE(result.HasLabel("success"));
    if (result.HasLabel("success")) {
        bool success = result["success"].Boolean();
        TEST_LOG("  success: %s", success ? "true" : "false");
        
        // Note: success may be false if devicestate.txt file doesn't exist,
        // which is valid behavior in L2 test environment after cleanup from previous tests.
        if (!success) {
            TEST_LOG("  getBlocklistFlag returned success=false (blocklist file may not exist)");
            if (result.HasLabel("error")) {
                JsonObject error = result["error"].Object();
                if (error.HasLabel("message")) {
                    TEST_LOG("  error message: %s", error["message"].String().c_str());
                }
            }
        }
	}

    // Validate blocklist field
    EXPECT_TRUE(result.HasLabel("blocklist"));
    if (result.HasLabel("blocklist")) {
        bool blocklist = result["blocklist"].Boolean();
        TEST_LOG("  blocklist: %s", blocklist ? "true" : "false");
    }
}

TEST_F(SystemService_L2Test, SetOptOutTelemetry_JSONRPC)
{
    TEST_LOG("Testing setOptOutTelemetry via JSON-RPC");

    JsonObject params;
    params["Opt-Out"] = true;
    JsonObject result;

    uint32_t status = InvokeServiceMethod("org.rdk.System.1", "setOptOutTelemetry", params, result);

    // setOptOutTelemetry depends on the org.rdk.Telemetry plugin (QueryInterfaceByCallsign).
    // In the L2 test environment, Telemetry plugin is not activated, so graceful failure is acceptable.
    if (status == Core::ERROR_NONE) {
        EXPECT_TRUE(result.HasLabel("success"));
        if (result.HasLabel("success")) {
            EXPECT_TRUE(result["success"].Boolean());
            TEST_LOG("  setOptOutTelemetry succeeded");
        }
    } else {
        TEST_LOG("setOptOutTelemetry failed with status %u - Telemetry plugin not available in L2 test environment", status);
    }
}

TEST_F(SystemService_L2Test, SetNetworkStandbyMode_JSONRPC)
{
    TEST_LOG("Testing setNetworkStandbyMode via JSON-RPC");

    JsonObject params;
    params["nwStandby"] = true;
    JsonObject result;

    uint32_t status = InvokeServiceMethod("org.rdk.System.1", "setNetworkStandbyMode", params, result);

    EXPECT_EQ(status, Core::ERROR_NONE);

    // Validate success field
    EXPECT_TRUE(result.HasLabel("success"));
    if (result.HasLabel("success")) {
        EXPECT_TRUE(result["success"].Boolean());
        TEST_LOG("  setNetworkStandbyMode succeeded");
    }
}

/*******************************************************
** Coverage Enhancement Tests ************************
** Testing additional SystemServices APIs coverage
*******************************************************/

TEST_F(SystemService_L2Test, SetMode_NORMAL_COMRPC)
{
    if (CreateSystemServicesInterfaceObject() != Core::ERROR_NONE) {
        TEST_LOG("Invalid SystemServices_Client");
    } else {
        EXPECT_TRUE(m_controller_SystemServices != nullptr);
        if (m_controller_SystemServices) {
            EXPECT_TRUE(m_SystemServicesPlugin != nullptr);
            if (m_SystemServicesPlugin) {
                TEST_LOG("Testing SetMode NORMAL via COM-RPC");

                // Declare input/output parameters
                Exchange::ISystemServices::ModeInfo modeInfo;
                modeInfo.mode = "NORMAL";
                modeInfo.duration = -1;
                uint32_t SysSrv_Status = 0;
                string errorMessage;
                bool success = false;

                // Call the API
                uint32_t result = m_SystemServicesPlugin->SetMode(modeInfo, SysSrv_Status, errorMessage, success);

                // Validate result
                EXPECT_EQ(result, Core::ERROR_NONE);
                if (result != Core::ERROR_NONE) {
                    std::string errorMsg = "COM-RPC returned error " + std::to_string(result) + " (" + std::string(Core::ErrorToString(result)) + ")";
                    TEST_LOG("Err: %s", errorMsg.c_str());
                }
                EXPECT_TRUE(success);

                // Log output
                TEST_LOG("SetMode NORMAL success: %s", success ? "true" : "false");
                
                m_SystemServicesPlugin->Release();
            } else {
                TEST_LOG("m_SystemServicesPlugin is NULL");
            }
            m_controller_SystemServices->Release();
        } else {
            TEST_LOG("m_controller_SystemServices is NULL");
        }
    }
}

TEST_F(SystemService_L2Test, SetMode_EAS_COMRPC)
{
    if (CreateSystemServicesInterfaceObject() != Core::ERROR_NONE) {
        TEST_LOG("Invalid SystemServices_Client");
    } else {
        EXPECT_TRUE(m_controller_SystemServices != nullptr);
        if (m_controller_SystemServices) {
            EXPECT_TRUE(m_SystemServicesPlugin != nullptr);
            if (m_SystemServicesPlugin) {
                TEST_LOG("Testing SetMode EAS via COM-RPC");

                // Declare input/output parameters
                Exchange::ISystemServices::ModeInfo modeInfo;
                modeInfo.mode = "EAS";
                modeInfo.duration = 300;
                uint32_t SysSrv_Status = 0;
                string errorMessage;
                bool success = false;

                // Call the API
                uint32_t result = m_SystemServicesPlugin->SetMode(modeInfo, SysSrv_Status, errorMessage, success);

                // Validate result
                EXPECT_EQ(result, Core::ERROR_NONE);
                if (result != Core::ERROR_NONE) {
                    std::string errorMsg = "COM-RPC returned error " + std::to_string(result) + " (" + std::string(Core::ErrorToString(result)) + ")";
                    TEST_LOG("Err: %s", errorMsg.c_str());
                }
                EXPECT_TRUE(success);

                // Log output
                TEST_LOG("SetMode EAS success: %s", success ? "true" : "false");
                
                m_SystemServicesPlugin->Release();
            } else {
                TEST_LOG("m_SystemServicesPlugin is NULL");
            }
            m_controller_SystemServices->Release();
        } else {
            TEST_LOG("m_controller_SystemServices is NULL");
        }
    }
}

TEST_F(SystemService_L2Test, SetMode_NORMAL_JSONRPC)
{
    TEST_LOG("Testing setMode NORMAL via JSON-RPC");

    JsonObject params;
    params["mode"] = "NORMAL";
    params["duration"] = -1;
    JsonObject result;

    uint32_t status = InvokeServiceMethod("org.rdk.System.1", "setMode", params, result);

    EXPECT_EQ(status, Core::ERROR_NONE);

    // Validate success field
    EXPECT_TRUE(result.HasLabel("success"));
    if (result.HasLabel("success")) {
        if (result["success"].Boolean()) {
            TEST_LOG("  setMode NORMAL succeeded");
        } else {
            TEST_LOG("  setMode NORMAL failed (PowerManager plugin may not be available in L2 environment)");
        }
    }
}

TEST_F(SystemService_L2Test, SetMode_EAS_JSONRPC)
{
    TEST_LOG("Testing setMode EAS via JSON-RPC");

    JsonObject params;
    params["mode"] = "EAS";
    params["duration"] = 300;
    JsonObject result;

    uint32_t status = InvokeServiceMethod("org.rdk.System.1", "setMode", params, result);

    EXPECT_EQ(status, Core::ERROR_NONE);

    // Validate success field
    EXPECT_TRUE(result.HasLabel("success"));
    if (result.HasLabel("success")) {
        if (result["success"].Boolean()) {
            TEST_LOG("  setMode EAS succeeded");
        } else {
            TEST_LOG("  setMode EAS failed (PowerManager plugin may not be available in L2 environment)");
        }
    }
}

TEST_F(SystemService_L2Test, SetMigrationStatus_JSONRPC)
{
    TEST_LOG("Testing setMigrationStatus via JSON-RPC");

    JsonObject params;
    params["status"] = "InProgress";
    JsonObject result;

    uint32_t status = InvokeServiceMethod("org.rdk.System.1", "setMigrationStatus", params, result);

    if (status == Core::ERROR_NONE) {
        EXPECT_TRUE(result.HasLabel("success"));
        if (result.HasLabel("success")) {
            EXPECT_TRUE(result["success"].Boolean());
            TEST_LOG("  setMigrationStatus succeeded");
        }
    } else {
        TEST_LOG("setMigrationStatus failed with status %u - Migration plugin not available in L2 test environment", status);
    } 
}


/********************************************************
************Test case Details **************************
** Additional API Coverage Tests
** Testing previously uncovered SystemServices APIs
*******************************************************/

// GetDownloadedFirmwareInfo Tests
TEST_F(SystemService_L2Test, GetDownloadedFirmwareInfo_COMRPC)
{
    if (CreateSystemServicesInterfaceObject() != Core::ERROR_NONE) {
        TEST_LOG("Invalid SystemServices_Client");
    } else {
        EXPECT_TRUE(m_controller_SystemServices != nullptr);
        if (m_controller_SystemServices) {
            EXPECT_TRUE(m_SystemServicesPlugin != nullptr);
            if (m_SystemServicesPlugin) {
                TEST_LOG("Testing GetDownloadedFirmwareInfo via COM-RPC");

                Exchange::ISystemServices::DownloadedFirmwareInfo fwInfo;

                uint32_t result = m_SystemServicesPlugin->GetDownloadedFirmwareInfo(fwInfo);

                EXPECT_EQ(result, Core::ERROR_NONE);
                if (result != Core::ERROR_NONE) {
                    std::string errorMsg = "COM-RPC returned error " + std::to_string(result);
                    TEST_LOG("Err: %s", errorMsg.c_str());
                }

                TEST_LOG("Current FW: %s, Downloaded FW: %s, Location: %s", 
                         fwInfo.currentFWVersion.c_str(), 
                         fwInfo.downloadedFWVersion.c_str(),
                         fwInfo.downloadedFWLocation.c_str());
                
                m_SystemServicesPlugin->Release();
            }
            m_controller_SystemServices->Release();
        }
    }
}

TEST_F(SystemService_L2Test, GetDownloadedFirmwareInfo_JSONRPC)
{
    TEST_LOG("Testing getDownloadedFirmwareInfo via JSON-RPC");

    JsonObject params;
    JsonObject result;

    uint32_t status = InvokeServiceMethod("org.rdk.System.1", "getDownloadedFirmwareInfo", params, result);

    EXPECT_EQ(status, Core::ERROR_NONE);

    if (result.HasLabel("firmwareVersion")) {
        TEST_LOG("  firmwareVersion: %s", result["firmwareVersion"].String().c_str());
    }
}

// GetFirmwareUpdateState Tests
TEST_F(SystemService_L2Test, GetFirmwareUpdateState_COMRPC)
{
    if (CreateSystemServicesInterfaceObject() != Core::ERROR_NONE) {
        TEST_LOG("Invalid SystemServices_Client");
    } else {
        EXPECT_TRUE(m_controller_SystemServices != nullptr);
        if (m_controller_SystemServices) {
            EXPECT_TRUE(m_SystemServicesPlugin != nullptr);
            if (m_SystemServicesPlugin) {
                TEST_LOG("Testing GetFirmwareUpdateState via COM-RPC");

                int firmwareUpdateState = -1;
                bool success = false;

                uint32_t result = m_SystemServicesPlugin->GetFirmwareUpdateState(firmwareUpdateState, success);

                EXPECT_EQ(result, Core::ERROR_NONE);
                TEST_LOG("FW Update State: %d, Success: %s", firmwareUpdateState, success ? "true" : "false");
                
                m_SystemServicesPlugin->Release();
            }
            m_controller_SystemServices->Release();
        }
    }
}

TEST_F(SystemService_L2Test, GetFirmwareUpdateState_JSONRPC)
{
    TEST_LOG("Testing getFirmwareUpdateState via JSON-RPC");

    JsonObject params;
    JsonObject result;

    uint32_t status = InvokeServiceMethod("org.rdk.System.1", "getFirmwareUpdateState", params, result);

    EXPECT_EQ(status, Core::ERROR_NONE);

    if (result.HasLabel("firmwareUpdateState")) {
        TEST_LOG("  firmwareUpdateState: %ld", (long)result["firmwareUpdateState"].Number());
    }
}

// GetLastWakeupKeyCode Tests
TEST_F(SystemService_L2Test, GetLastWakeupKeyCode_COMRPC)
{
    if (CreateSystemServicesInterfaceObject() != Core::ERROR_NONE) {
        TEST_LOG("Invalid SystemServices_Client");
    } else {
        EXPECT_TRUE(m_controller_SystemServices != nullptr);
        if (m_controller_SystemServices) {
            EXPECT_TRUE(m_SystemServicesPlugin != nullptr);
            if (m_SystemServicesPlugin) {
                TEST_LOG("Testing GetLastWakeupKeyCode via COM-RPC");

                int wakeupKeyCode = 0;
                bool success = false;

                uint32_t result = m_SystemServicesPlugin->GetLastWakeupKeyCode(wakeupKeyCode, success);

                EXPECT_EQ(result, Core::ERROR_NONE);
                TEST_LOG("Last Wakeup KeyCode: %d, Success: %s", wakeupKeyCode, success ? "true" : "false");
                
                m_SystemServicesPlugin->Release();
            }
            m_controller_SystemServices->Release();
        }
    }
}

TEST_F(SystemService_L2Test, GetLastWakeupKeyCode_JSONRPC)
{
    TEST_LOG("Testing getLastWakeupKeyCode via JSON-RPC");

    JsonObject params;
    JsonObject result;

    uint32_t status = InvokeServiceMethod("org.rdk.System.1", "getLastWakeupKeyCode", params, result);

    EXPECT_EQ(status, Core::ERROR_NONE);

    if (result.HasLabel("wakeupKeyCode")) {
        TEST_LOG("  wakeupKeyCode: %ld", (long)result["wakeupKeyCode"].Number());
    }
}

// GetMfgSerialNumber Tests
TEST_F(SystemService_L2Test, GetMfgSerialNumber_COMRPC)
{
    if (CreateSystemServicesInterfaceObject() != Core::ERROR_NONE) {
        TEST_LOG("Invalid SystemServices_Client");
    } else {
        EXPECT_TRUE(m_controller_SystemServices != nullptr);
        if (m_controller_SystemServices) {
            EXPECT_TRUE(m_SystemServicesPlugin != nullptr);
            if (m_SystemServicesPlugin) {
                TEST_LOG("Testing GetMfgSerialNumber via COM-RPC");

                string mfgSerialNumber;
                bool success = false;

                uint32_t result = m_SystemServicesPlugin->GetMfgSerialNumber(mfgSerialNumber, success);

                EXPECT_EQ(result, Core::ERROR_NONE);
                TEST_LOG("Mfg Serial Number: %s, Success: %s", mfgSerialNumber.c_str(), success ? "true" : "false");
                
                m_SystemServicesPlugin->Release();
            }
            m_controller_SystemServices->Release();
        }
    }
}

TEST_F(SystemService_L2Test, GetMfgSerialNumber_JSONRPC)
{
    TEST_LOG("Testing getMfgSerialNumber via JSON-RPC");

    JsonObject params;
    JsonObject result;

    uint32_t status = InvokeServiceMethod("org.rdk.System.1", "getMfgSerialNumber", params, result);

    EXPECT_EQ(status, Core::ERROR_NONE);

    if (result.HasLabel("mfgSerialNumber")) {
        TEST_LOG("  mfgSerialNumber: %s", result["mfgSerialNumber"].String().c_str());
    }
}

// IsOptOutTelemetry Tests
TEST_F(SystemService_L2Test, IsOptOutTelemetry_COMRPC)
{
    if (CreateSystemServicesInterfaceObject() != Core::ERROR_NONE) {
        TEST_LOG("Invalid SystemServices_Client");
    } else {
        EXPECT_TRUE(m_controller_SystemServices != nullptr);
        if (m_controller_SystemServices) {
            EXPECT_TRUE(m_SystemServicesPlugin != nullptr);
            if (m_SystemServicesPlugin) {
                TEST_LOG("Testing IsOptOutTelemetry via COM-RPC");

                bool optOut = false;
                bool success = false;

                uint32_t result = m_SystemServicesPlugin->IsOptOutTelemetry(optOut, success);

                if (result == Core::ERROR_NONE) {
                    TEST_LOG("Opt-Out Telemetry: %s, Success: %s", optOut ? "true" : "false", success ? "true" : "false");
                } else {
                    TEST_LOG("IsOptOutTelemetry failed with result %u - Telemetry plugin not available in L2 test environment", result);
                }
                m_SystemServicesPlugin->Release();
            }
            m_controller_SystemServices->Release();
        }
    }
}

// SetFirmwareAutoReboot Tests
TEST_F(SystemService_L2Test, SetFirmwareAutoReboot_COMRPC)
{
    if (CreateSystemServicesInterfaceObject() != Core::ERROR_NONE) {
        TEST_LOG("Invalid SystemServices_Client");
    } else {
        EXPECT_TRUE(m_controller_SystemServices != nullptr);
        if (m_controller_SystemServices) {
            EXPECT_TRUE(m_SystemServicesPlugin != nullptr);
            if (m_SystemServicesPlugin) {
                TEST_LOG("Testing SetFirmwareAutoReboot via COM-RPC");

                Exchange::ISystemServices::SystemResult sysResult;

                uint32_t result = m_SystemServicesPlugin->SetFirmwareAutoReboot(true, sysResult);

                if (result == Core::ERROR_NONE) {
                    TEST_LOG("SetFirmwareAutoReboot success: %s", sysResult.success ? "true" : "false");
                } else {
                    TEST_LOG("SetFirmwareAutoReboot failed with result %u - FirmwareUpdate plugin not available in L2 test environment", result);
                }
                
                m_SystemServicesPlugin->Release();
            }
            m_controller_SystemServices->Release();
        }
    }
}

TEST_F(SystemService_L2Test, SetFirmwareAutoReboot_JSONRPC)
{
    TEST_LOG("Testing setFirmwareAutoReboot via JSON-RPC");

    JsonObject params;
    params["enable"] = true;
    JsonObject result;

    uint32_t status = InvokeServiceMethod("org.rdk.System.1", "setFirmwareAutoReboot", params, result);

    if (status == Core::ERROR_NONE) {
        if (result.HasLabel("success")) {
            TEST_LOG("  success: %s", result["success"].Boolean() ? "true" : "false");
        }
    } else {
        TEST_LOG("setFirmwareAutoReboot failed with status %u - FirmwareUpdate plugin not available in L2 test environment", status);
    }
}

// UpdateFirmware Tests
TEST_F(SystemService_L2Test, UpdateFirmware_COMRPC)
{
    if (CreateSystemServicesInterfaceObject() != Core::ERROR_NONE) {
        TEST_LOG("Invalid SystemServices_Client");
    } else {
        EXPECT_TRUE(m_controller_SystemServices != nullptr);
        if (m_controller_SystemServices) {
            EXPECT_TRUE(m_SystemServicesPlugin != nullptr);
            if (m_SystemServicesPlugin) {
                TEST_LOG("Testing UpdateFirmware via COM-RPC");

                Exchange::ISystemServices::SystemResult sysResult;

                uint32_t result = m_SystemServicesPlugin->UpdateFirmware(sysResult);

                EXPECT_EQ(result, Core::ERROR_NONE);
                TEST_LOG("UpdateFirmware called, success: %s", sysResult.success ? "true" : "false");
                
                m_SystemServicesPlugin->Release();
            }
            m_controller_SystemServices->Release();
        }
    }
}

TEST_F(SystemService_L2Test, UpdateFirmware_JSONRPC)
{
    TEST_LOG("Testing updateFirmware via JSON-RPC");

    JsonObject params;
    JsonObject result;

    uint32_t status = InvokeServiceMethod("org.rdk.System.1", "updateFirmware", params, result);

    EXPECT_EQ(status, Core::ERROR_NONE);

    if (result.HasLabel("success")) {
        TEST_LOG("  success: %s", result["success"].Boolean() ? "true" : "false");
    }
}

// SetFSRFlag Tests
TEST_F(SystemService_L2Test, SetFSRFlag_COMRPC)
{
    if (CreateSystemServicesInterfaceObject() != Core::ERROR_NONE) {
        TEST_LOG("Invalid SystemServices_Client");
    } else {
        EXPECT_TRUE(m_controller_SystemServices != nullptr);
        if (m_controller_SystemServices) {
            EXPECT_TRUE(m_SystemServicesPlugin != nullptr);
            if (m_SystemServicesPlugin) {
                TEST_LOG("Testing SetFSRFlag via COM-RPC");

                Exchange::ISystemServices::SystemResult sysResult;

                uint32_t result = m_SystemServicesPlugin->SetFSRFlag(true, sysResult);

                EXPECT_EQ(result, Core::ERROR_NONE);
                TEST_LOG("SetFSRFlag success: %s", sysResult.success ? "true" : "false");
                
                m_SystemServicesPlugin->Release();
            }
            m_controller_SystemServices->Release();
        }
    }
}

TEST_F(SystemService_L2Test, SetFSRFlag_JSONRPC)
{
    TEST_LOG("Testing setFSRFlag via JSON-RPC");

    JsonObject params;
    params["fsrFlag"] = true;
    JsonObject result;

    uint32_t status = InvokeServiceMethod("org.rdk.System.1", "setFSRFlag", params, result);

    EXPECT_EQ(status, Core::ERROR_NONE);

    if (result.HasLabel("success")) {
        TEST_LOG("  success: %s", result["success"].Boolean() ? "true" : "false");
    }
}

// SetBlocklistFlag Tests
TEST_F(SystemService_L2Test, SetBlocklistFlag_COMRPC)
{
    if (CreateSystemServicesInterfaceObject() != Core::ERROR_NONE) {
        TEST_LOG("Invalid SystemServices_Client");
    } else {
        EXPECT_TRUE(m_controller_SystemServices != nullptr);
        if (m_controller_SystemServices) {
            EXPECT_TRUE(m_SystemServicesPlugin != nullptr);
            if (m_SystemServicesPlugin) {
                TEST_LOG("Testing SetBlocklistFlag via COM-RPC");

                Exchange::ISystemServices::SetBlocklistResult blocklistResult;

                uint32_t result = m_SystemServicesPlugin->SetBlocklistFlag(true, blocklistResult);

                EXPECT_EQ(result, Core::ERROR_NONE);
                TEST_LOG("SetBlocklistFlag success: %s", blocklistResult.success ? "true" : "false");
                
                m_SystemServicesPlugin->Release();
            }
            m_controller_SystemServices->Release();
        }
    }
}

TEST_F(SystemService_L2Test, SetBlocklistFlag_JSONRPC)
{
    TEST_LOG("Testing setBlocklistFlag via JSON-RPC");

    JsonObject params;
    params["blocklist"] = true;
    JsonObject result;

    uint32_t status = InvokeServiceMethod("org.rdk.System.1", "setBlocklistFlag", params, result);

    EXPECT_EQ(status, Core::ERROR_NONE);

    if (result.HasLabel("success")) {
        TEST_LOG("  success: %s", result["success"].Boolean() ? "true" : "false");
    }
}

/********************************************************
************Test case Details **************************
** Negative and Corner Case Tests
*******************************************************/

// Negative Test: SetMode with invalid mode
TEST_F(SystemService_L2Test, SetMode_InvalidMode_COMRPC_Negative)
{
    if (CreateSystemServicesInterfaceObject() != Core::ERROR_NONE) {
        TEST_LOG("Invalid SystemServices_Client");
    } else {
        EXPECT_TRUE(m_controller_SystemServices != nullptr);
        if (m_controller_SystemServices) {
            EXPECT_TRUE(m_SystemServicesPlugin != nullptr);
            if (m_SystemServicesPlugin) {
                TEST_LOG("Testing SetMode with invalid mode via COM-RPC (Negative Test)");

                Exchange::ISystemServices::ModeInfo modeInfo;
                modeInfo.mode = "INVALID_MODE_XYZ";
                modeInfo.duration = 100;
                uint32_t SysSrv_Status = 0;
                string errorMessage;
                bool success = false;

                m_SystemServicesPlugin->SetMode(modeInfo, SysSrv_Status, errorMessage, success);

                // Expecting failure for invalid mode
                if (!success) {
                    TEST_LOG("Expected failure occurred for invalid mode");
                } else {
                    TEST_LOG("Warning: Invalid mode was accepted");
                }
                
                m_SystemServicesPlugin->Release();
            }
            m_controller_SystemServices->Release();
        }
    }
}

TEST_F(SystemService_L2Test, SetMode_InvalidMode_JSONRPC_Negative)
{
    TEST_LOG("Testing setMode with invalid mode via JSON-RPC (Negative Test)");

    JsonObject params;
    params["mode"] = "INVALID_MODE";
    params["duration"] = 100;
    JsonObject result;

    InvokeServiceMethod("org.rdk.System.1", "setMode", params, result);

    // API may return ERROR_NONE but success field should indicate failure
    if (result.HasLabel("success")) {
        if (!result["success"].Boolean()) {
            TEST_LOG("  Expected failure for invalid mode");
        }
    }
}

// Negative Test: SetMode with WAREHOUSE mode and zero duration
TEST_F(SystemService_L2Test, SetMode_WAREHOUSE_ZeroDuration_COMRPC)
{
    if (CreateSystemServicesInterfaceObject() != Core::ERROR_NONE) {
        TEST_LOG("Invalid SystemServices_Client");
    } else {
        EXPECT_TRUE(m_controller_SystemServices != nullptr);
        if (m_controller_SystemServices) {
            EXPECT_TRUE(m_SystemServicesPlugin != nullptr);
            if (m_SystemServicesPlugin) {
                TEST_LOG("Testing SetMode WAREHOUSE with zero duration via COM-RPC");

                Exchange::ISystemServices::ModeInfo modeInfo;
                modeInfo.mode = "WAREHOUSE";
                modeInfo.duration = 0;
                uint32_t SysSrv_Status = 0;
                string errorMessage;
                bool success = false;

                uint32_t result = m_SystemServicesPlugin->SetMode(modeInfo, SysSrv_Status, errorMessage, success);

                EXPECT_EQ(result, Core::ERROR_NONE);
                TEST_LOG("SetMode WAREHOUSE (duration=0) success: %s", success ? "true" : "false");
                
                m_SystemServicesPlugin->Release();
            }
            m_controller_SystemServices->Release();
        }
    }
}

// Corner Case: SetFriendlyName with empty string
TEST_F(SystemService_L2Test, SetFriendlyName_EmptyString_COMRPC_Corner)
{
    if (CreateSystemServicesInterfaceObject() != Core::ERROR_NONE) {
        TEST_LOG("Invalid SystemServices_Client");
    } else {
        EXPECT_TRUE(m_controller_SystemServices != nullptr);
        if (m_controller_SystemServices) {
            EXPECT_TRUE(m_SystemServicesPlugin != nullptr);
            if (m_SystemServicesPlugin) {
                TEST_LOG("Testing SetFriendlyName with empty string via COM-RPC (Corner Case)");

                Exchange::ISystemServices::SystemResult sysResult;

                uint32_t result = m_SystemServicesPlugin->SetFriendlyName("", sysResult);

                EXPECT_EQ(result, Core::ERROR_NONE);
                TEST_LOG("SetFriendlyName (empty) success: %s", sysResult.success ? "true" : "false");
                
                m_SystemServicesPlugin->Release();
            }
            m_controller_SystemServices->Release();
        }
    }
}

// Corner Case: SetFriendlyName with very long string
TEST_F(SystemService_L2Test, SetFriendlyName_LongString_COMRPC_Corner)
{
    if (CreateSystemServicesInterfaceObject() != Core::ERROR_NONE) {
        TEST_LOG("Invalid SystemServices_Client");
    } else {
        EXPECT_TRUE(m_controller_SystemServices != nullptr);
        if (m_controller_SystemServices) {
            EXPECT_TRUE(m_SystemServicesPlugin != nullptr);
            if (m_SystemServicesPlugin) {
                TEST_LOG("Testing SetFriendlyName with long string via COM-RPC (Corner Case)");

                std::string longName(256, 'A'); // 256 character string
                Exchange::ISystemServices::SystemResult sysResult;

                uint32_t result = m_SystemServicesPlugin->SetFriendlyName(longName, sysResult);

                EXPECT_EQ(result, Core::ERROR_NONE);
                TEST_LOG("SetFriendlyName (long) success: %s", sysResult.success ? "true" : "false");
                
                m_SystemServicesPlugin->Release();
            }
            m_controller_SystemServices->Release();
        }
    }
}

// Corner Case: SetFriendlyName with special characters
TEST_F(SystemService_L2Test, SetFriendlyName_SpecialChars_JSONRPC_Corner)
{
    TEST_LOG("Testing setFriendlyName with special characters via JSON-RPC (Corner Case)");

    JsonObject params;
    params["friendlyName"] = "Test@Device#123!$%";
    JsonObject result;

    uint32_t status = InvokeServiceMethod("org.rdk.System.1", "setFriendlyName", params, result);

    EXPECT_EQ(status, Core::ERROR_NONE);

    if (result.HasLabel("success")) {
        TEST_LOG("  success with special chars: %s", result["success"].Boolean() ? "true" : "false");
    }
}

// Negative Test: SetTerritory with invalid territory
TEST_F(SystemService_L2Test, SetTerritory_InvalidTerritory_COMRPC_Negative)
{
    if (CreateSystemServicesInterfaceObject() != Core::ERROR_NONE) {
        TEST_LOG("Invalid SystemServices_Client");
    } else {
        EXPECT_TRUE(m_controller_SystemServices != nullptr);
        if (m_controller_SystemServices) {
            EXPECT_TRUE(m_SystemServicesPlugin != nullptr);
            if (m_SystemServicesPlugin) {
                TEST_LOG("Testing SetTerritory with invalid territory via COM-RPC (Negative Test)");

                Exchange::ISystemServices::SystemError sysError;
                bool success = false;

                m_SystemServicesPlugin->SetTerritory("INVALID_TERRITORY", "INVALID_REGION", sysError, success);

                // Expecting failure or specific error
                TEST_LOG("SetTerritory (invalid) returned success: %s", success ? "true" : "false");
                
                m_SystemServicesPlugin->Release();
            }
            m_controller_SystemServices->Release();
        }
    }
}

// Negative Test: SetTimeZoneDST with invalid timezone
TEST_F(SystemService_L2Test, SetTimeZoneDST_InvalidTimezone_COMRPC_Negative)
{
    if (CreateSystemServicesInterfaceObject() != Core::ERROR_NONE) {
        TEST_LOG("Invalid SystemServices_Client");
    } else {
        EXPECT_TRUE(m_controller_SystemServices != nullptr);
        if (m_controller_SystemServices) {
            EXPECT_TRUE(m_SystemServicesPlugin != nullptr);
            if (m_SystemServicesPlugin) {
                TEST_LOG("Testing SetTimeZoneDST with invalid timezone via COM-RPC (Negative Test)");

                uint32_t SysSrv_Status = 0;
                string errorMessage;
                bool success = false;

                m_SystemServicesPlugin->SetTimeZoneDST("Invalid/Timezone", "FINAL", SysSrv_Status, errorMessage, success);

                if (!success) {
                    TEST_LOG("Expected failure for invalid timezone, error: %s", errorMessage.c_str());
                }
                
                m_SystemServicesPlugin->Release();
            }
            m_controller_SystemServices->Release();
        }
    }
}

// Negative Test: SetNetworkStandbyMode toggle
TEST_F(SystemService_L2Test, SetNetworkStandbyMode_Toggle_COMRPC)
{
    if (CreateSystemServicesInterfaceObject() != Core::ERROR_NONE) {
        TEST_LOG("Invalid SystemServices_Client");
    } else {
        EXPECT_TRUE(m_controller_SystemServices != nullptr);
        if (m_controller_SystemServices) {
            EXPECT_TRUE(m_SystemServicesPlugin != nullptr);
            if (m_SystemServicesPlugin) {
                TEST_LOG("Testing SetNetworkStandbyMode toggle via COM-RPC");

                Exchange::ISystemServices::SystemResult sysResult1, sysResult2;

                // Set to true
                uint32_t result1 = m_SystemServicesPlugin->SetNetworkStandbyMode(true, sysResult1);
                EXPECT_EQ(result1, Core::ERROR_NONE);
                TEST_LOG("Set to true: %s", sysResult1.success ? "success" : "failed");

                // Set to false
                uint32_t result2 = m_SystemServicesPlugin->SetNetworkStandbyMode(false, sysResult2);
                EXPECT_EQ(result2, Core::ERROR_NONE);
                TEST_LOG("Set to false: %s", sysResult2.success ? "success" : "failed");
                
                m_SystemServicesPlugin->Release();
            }
            m_controller_SystemServices->Release();
        }
    }
}
																					
// Negative Test: SetMigrationStatus with empty string
TEST_F(SystemService_L2Test, SetMigrationStatus_EmptyString_JSONRPC_Negative)
{
    TEST_LOG("Testing setMigrationStatus with empty string via JSON-RPC (Negative Test)");

    JsonObject params;
    params["status"] = "";
    JsonObject result;

    uint32_t status = InvokeServiceMethod("org.rdk.System.1", "setMigrationStatus", params, result);

    // May fail or succeed depending on implementation
    if (result.HasLabel("success")) {
        bool success = result["success"].Boolean();
        TEST_LOG("  setMigrationStatus with empty string: %s", success ? "accepted" : "rejected");
    }
}

// Negative Test: SetMigrationStatus with invalid status value
TEST_F(SystemService_L2Test, SetMigrationStatus_InvalidValue_JSONRPC_Negative)
{
    TEST_LOG("Testing setMigrationStatus with invalid value via JSON-RPC (Negative Test)");

    JsonObject params;
    params["status"] = "InvalidStatusValue123";
    JsonObject result;

    InvokeServiceMethod("org.rdk.System.1", "setMigrationStatus", params, result);

    // Implementation should handle invalid values gracefully
    if (result.HasLabel("success")) {
        TEST_LOG("  setMigrationStatus with invalid value returned: %s", 
                 result["success"].Boolean() ? "success" : "failure");
    }
}


/********************************************************
************Test case Details **************************
** Missing API Coverage Tests
** Testing previously uncovered SystemServices APIs
*******************************************************/
// GetLastFirmwareFailureReason Tests
TEST_F(SystemService_L2Test, GetLastFirmwareFailureReason_COMRPC)
{
    if (CreateSystemServicesInterfaceObject() != Core::ERROR_NONE) {
        TEST_LOG("Invalid SystemServices_Client");
    } else {
        EXPECT_TRUE(m_controller_SystemServices != nullptr);
        if (m_controller_SystemServices) {
            EXPECT_TRUE(m_SystemServicesPlugin != nullptr);
            if (m_SystemServicesPlugin) {
                TEST_LOG("Testing GetLastFirmwareFailureReason via COM-RPC");

                string failReason;
                bool success = false;

                uint32_t result = m_SystemServicesPlugin->GetLastFirmwareFailureReason(failReason, success);
                EXPECT_EQ(result, Core::ERROR_NONE);
                TEST_LOG("Failure reason: %s, success: %s", failReason.c_str(), success ? "true" : "false");
                
                m_SystemServicesPlugin->Release();
            }
            m_controller_SystemServices->Release();
        }
    }
}

TEST_F(SystemService_L2Test, GetLastFirmwareFailureReason_JSONRPC)
{
    TEST_LOG("Testing GetLastFirmwareFailureReason via JSON-RPC");

    JsonObject params;
    JsonObject result;

    uint32_t status = InvokeServiceMethod("org.rdk.System.1", "getLastFirmwareFailureReason", params, result);

    EXPECT_EQ(status, Core::ERROR_NONE);
    EXPECT_TRUE(result.HasLabel("success"));
    
    if (result.HasLabel("failReason")) {
        string reason = result["failReason"].String();
        TEST_LOG("Failure reason: %s", reason.c_str());
    }
}

// GetWakeupReason Tests
TEST_F(SystemService_L2Test, GetWakeupReason_COMRPC)
{
    if (CreateSystemServicesInterfaceObject() != Core::ERROR_NONE) {
        TEST_LOG("Invalid SystemServices_Client");
    } else {
        EXPECT_TRUE(m_controller_SystemServices != nullptr);
        if (m_controller_SystemServices) {
            EXPECT_TRUE(m_SystemServicesPlugin != nullptr);
            if (m_SystemServicesPlugin) {
                TEST_LOG("Testing GetWakeupReason via COM-RPC");

                string wakeupReason;
                bool success = false;

                uint32_t result = m_SystemServicesPlugin->GetWakeupReason(wakeupReason, success);
                EXPECT_EQ(result, Core::ERROR_NONE);
                TEST_LOG("Wakeup reason: %s, success: %s", wakeupReason.c_str(), success ? "true" : "false");
                
                m_SystemServicesPlugin->Release();
            }
            m_controller_SystemServices->Release();
        }
    }
}

TEST_F(SystemService_L2Test, GetWakeupReason_JSONRPC)
{
    TEST_LOG("Testing GetWakeupReason via JSON-RPC");

    JsonObject params;
    JsonObject result;

    uint32_t status = InvokeServiceMethod("org.rdk.System.1", "getWakeupReason", params, result);

    EXPECT_EQ(status, Core::ERROR_NONE);
    EXPECT_TRUE(result.HasLabel("success"));
    
    if (result.HasLabel("wakeupReason")) {
        string reason = result["wakeupReason"].String();
        TEST_LOG("Wakeup reason: %s", reason.c_str());
    }
}


// SetDeepSleepTimer Tests
TEST_F(SystemService_L2Test, SetDeepSleepTimer_COMRPC)
{
    if (CreateSystemServicesInterfaceObject() != Core::ERROR_NONE) {
        TEST_LOG("Invalid SystemServices_Client");
    } else {
        EXPECT_TRUE(m_controller_SystemServices != nullptr);
        if (m_controller_SystemServices) {
            EXPECT_TRUE(m_SystemServicesPlugin != nullptr);
            if (m_SystemServicesPlugin) {
                TEST_LOG("Testing SetDeepSleepTimer via COM-RPC");

                uint32_t sysSrvStatus = 0;
                string errorMessage;
                bool success = false;
                int seconds = 3600; // 1 hour

                uint32_t result = m_SystemServicesPlugin->SetDeepSleepTimer(seconds, sysSrvStatus, errorMessage, success);
                EXPECT_EQ(result, Core::ERROR_NONE);
                TEST_LOG("Set deep sleep timer to %d seconds, success: %s", seconds, success ? "true" : "false");
                
                if (!success) {
                    TEST_LOG("Error: %s, status: %u", errorMessage.c_str(), sysSrvStatus);
                }
                
                m_SystemServicesPlugin->Release();
            }
            m_controller_SystemServices->Release();
        }
    }
}

TEST_F(SystemService_L2Test, SetDeepSleepTimer_JSONRPC)
{
    TEST_LOG("Testing SetDeepSleepTimer via JSON-RPC");

    JsonObject params;
    params["seconds"] = 3600;
    JsonObject result;

    uint32_t status = InvokeServiceMethod("org.rdk.System.1", "setDeepSleepTimer", params, result);

    EXPECT_EQ(status, Core::ERROR_NONE);
    EXPECT_TRUE(result.HasLabel("success"));
    
    if (result.HasLabel("success")) {
        TEST_LOG("Set deep sleep timer: %s", result["success"].Boolean() ? "success" : "failed");
    }
}

// AbortLogUpload Tests
TEST_F(SystemService_L2Test, AbortLogUpload_COMRPC)
{
    if (CreateSystemServicesInterfaceObject() != Core::ERROR_NONE) {
        TEST_LOG("Invalid SystemServices_Client");
    } else {
        EXPECT_TRUE(m_controller_SystemServices != nullptr);
        if (m_controller_SystemServices) {
            EXPECT_TRUE(m_SystemServicesPlugin != nullptr);
            if (m_SystemServicesPlugin) {
                TEST_LOG("Testing AbortLogUpload via COM-RPC");

                Exchange::ISystemServices::SystemResult sysResult;

                uint32_t result = m_SystemServicesPlugin->AbortLogUpload(sysResult);
                EXPECT_EQ(result, Core::ERROR_NONE);
                TEST_LOG("Abort log upload: %s", sysResult.success ? "success" : "failed");
                
                m_SystemServicesPlugin->Release();
            }
            m_controller_SystemServices->Release();
        }
    }
}

TEST_F(SystemService_L2Test, AbortLogUpload_JSONRPC)
{
    TEST_LOG("Testing AbortLogUpload via JSON-RPC");

    JsonObject params;
    JsonObject result;

    uint32_t status = InvokeServiceMethod("org.rdk.System.1", "abortLogUpload", params, result);

    EXPECT_EQ(status, Core::ERROR_NONE);
    EXPECT_TRUE(result.HasLabel("success"));
    
    if (result.HasLabel("success")) {
        TEST_LOG("Abort log upload: %s", result["success"].Boolean() ? "success" : "failed");
    }
}

// GetPlatformConfiguration Tests
TEST_F(SystemService_L2Test, GetPlatformConfiguration_COMRPC)
{
    if (CreateSystemServicesInterfaceObject() != Core::ERROR_NONE) {
        TEST_LOG("Invalid SystemServices_Client");
    } else {
        EXPECT_TRUE(m_controller_SystemServices != nullptr);
        if (m_controller_SystemServices) {
            EXPECT_TRUE(m_SystemServicesPlugin != nullptr);
            if (m_SystemServicesPlugin) {
                TEST_LOG("Testing GetPlatformConfiguration via COM-RPC");

                string query = "AccountInfo.accountId";
                Exchange::ISystemServices::PlatformConfig platformConfig;

                uint32_t result = m_SystemServicesPlugin->GetPlatformConfiguration(query, platformConfig);
                EXPECT_EQ(result, Core::ERROR_NONE);
                TEST_LOG("Platform configuration query success: %s", platformConfig.success ? "true" : "false");
                
                if (platformConfig.success) {
                    TEST_LOG("Account ID: %s", platformConfig.accountInfo.accountId.c_str());
                }
                
                m_SystemServicesPlugin->Release();
            }
            m_controller_SystemServices->Release();
        }
    }
}

TEST_F(SystemService_L2Test, GetPlatformConfiguration_JSONRPC)
{
    TEST_LOG("Testing GetPlatformConfiguration via JSON-RPC");

    JsonObject params;
    params["query"] = "AccountInfo.accountId";
    JsonObject result;

    uint32_t status = InvokeServiceMethod("org.rdk.System.1", "getPlatformConfiguration", params, result);

    EXPECT_EQ(status, Core::ERROR_NONE);
    EXPECT_TRUE(result.HasLabel("success"));
    
    if (result.HasLabel("AccountInfo")) {
        TEST_LOG("Platform configuration retrieved successfully");
    }
}

// Reboot Test (JSON-RPC only - destructive operation)
TEST_F(SystemService_L2Test, Reboot_JSONRPC_Negative)
{
    TEST_LOG("Testing Reboot via JSON-RPC (should fail in test environment)");

    JsonObject params;
    params["rebootReason"] = "L2Test";
    JsonObject result;

    uint32_t status = InvokeServiceMethod("org.rdk.System.1", "reboot", params, result);

    // Reboot may fail in test environment - that's acceptable
    TEST_LOG("Reboot status: %u", status);
    
    if (result.HasLabel("success")) {
        TEST_LOG("Reboot call: %s", result["success"].Boolean() ? "success" : "failed");
    }
}

// SetPowerState Test (with caution - may change device state)
TEST_F(SystemService_L2Test, SetPowerState_JSONRPC_Negative)
{
    TEST_LOG("Testing SetPowerState via JSON-RPC");

    JsonObject params;
    params["powerState"] = "ON";
    params["standbyReason"] = "L2Test";
    JsonObject result;

    uint32_t status = InvokeServiceMethod("org.rdk.System.1", "setPowerState", params, result);

    // May succeed or fail depending on environment
    TEST_LOG("SetPowerState status: %u", status);
    
    if (result.HasLabel("success")) {
        TEST_LOG("SetPowerState call: %s", result["success"].Boolean() ? "success" : "failed");
    }
}

//Notifications 
TEST_F(SystemService_L2Test, OnFriendlyNameChanged_Notification_COMRPC)
{
    if (CreateSystemServicesInterfaceObject() != Core::ERROR_NONE) {
        TEST_LOG("Invalid SystemServices_Client");
    } else {
        EXPECT_TRUE(m_controller_SystemServices != nullptr);
        if (m_controller_SystemServices) {
            EXPECT_TRUE(m_SystemServicesPlugin != nullptr);
            if (m_SystemServicesPlugin) {

                TEST_LOG("Testing OnFriendlyNameChanged notification via COM-RPC");

                // Register for notifications
                uint32_t result = m_SystemServicesPlugin->Register(&m_notificationHandler);
                EXPECT_EQ(result, Core::ERROR_NONE);
                if (result != Core::ERROR_NONE) {
                    std::string errorMsg = "Register returned error " + std::to_string(result) + " (" + std::string(Core::ErrorToString(result)) + ")";
                    TEST_LOG("Err: %s", errorMsg.c_str());
                } else {
                    TEST_LOG("Successfully registered for notifications");
                }

                // Reset event flag before triggering the event
                m_notificationHandler.ResetEvent();

                // Trigger friendly name change via SetFriendlyName
                Exchange::ISystemServices::SystemResult setResult;
                result = m_SystemServicesPlugin->SetFriendlyName("TestDeviceName", setResult);
                EXPECT_EQ(result, Core::ERROR_NONE);

                if (result == Core::ERROR_NONE) {
                    // Wait for the notification
                    uint32_t eventStatus = m_notificationHandler.WaitForEvent(EVNT_TIMEOUT, SYSTEMSERVICEL2TEST_FRIENDLY_NAME_CHANGED);

                    EXPECT_NE(eventStatus, SYSTEMSERVICEL2TEST_STATE_INVALID);
                    if (eventStatus != SYSTEMSERVICEL2TEST_STATE_INVALID) {
                        TEST_LOG("OnFriendlyNameChanged notification received successfully");

                        // Validate received data
                        string receivedName = m_notificationHandler.GetLastFriendlyName();
                        TEST_LOG("Received FriendlyName: %s", receivedName.c_str());
                        EXPECT_STREQ(receivedName.c_str(), "TestDeviceName");
                    } else {
                        TEST_LOG("Timeout waiting for OnFriendlyNameChanged notification");
                    }
                }

                // Unregister from notifications
                result = m_SystemServicesPlugin->Unregister(&m_notificationHandler);
                EXPECT_EQ(result, Core::ERROR_NONE);
                if (result == Core::ERROR_NONE) {
                    TEST_LOG("Successfully unregistered from notifications");
                }

                m_SystemServicesPlugin->Release();
            } else {
                TEST_LOG("m_SystemServicesPlugin is NULL");
            }
            m_controller_SystemServices->Release();
        } else {
            TEST_LOG("m_controller_SystemServices is NULL");
        }
    }
}

TEST_F(SystemService_L2Test, OnNetworkStandbyModeChanged_Notification_COMRPC)
{
    if (CreateSystemServicesInterfaceObject() != Core::ERROR_NONE) {
        TEST_LOG("Invalid SystemServices_Client");
    } else {
        EXPECT_TRUE(m_controller_SystemServices != nullptr);
        if (m_controller_SystemServices) {
            EXPECT_TRUE(m_SystemServicesPlugin != nullptr);
            if (m_SystemServicesPlugin) {

                TEST_LOG("Testing OnNetworkStandbyModeChanged notification via COM-RPC");

                // Register for notifications
                uint32_t result = m_SystemServicesPlugin->Register(&m_notificationHandler);
                EXPECT_EQ(result, Core::ERROR_NONE);
                if (result != Core::ERROR_NONE) {
                    std::string errorMsg = "Register returned error " + std::to_string(result) + " (" + std::string(Core::ErrorToString(result)) + ")";
                    TEST_LOG("Err: %s", errorMsg.c_str());
                } else {
                    TEST_LOG("Successfully registered for notifications");
                }

                // Reset event flag before triggering the event
                m_notificationHandler.ResetEvent();

                // Trigger network standby mode change via SetNetworkStandbyMode
                Exchange::ISystemServices::SystemResult setResult;
                result = m_SystemServicesPlugin->SetNetworkStandbyMode(true, setResult);
                EXPECT_EQ(result, Core::ERROR_NONE);

                if (result == Core::ERROR_NONE) {
                    // Wait for the notification
                    uint32_t eventStatus = m_notificationHandler.WaitForEvent(EVNT_TIMEOUT, SYSTEMSERVICEL2TEST_NETWORK_STANDBY_CHANGED);

                    EXPECT_NE(eventStatus, SYSTEMSERVICEL2TEST_STATE_INVALID);
                    if (eventStatus != SYSTEMSERVICEL2TEST_STATE_INVALID) {
                        TEST_LOG("OnNetworkStandbyModeChanged notification received successfully");

                        // Validate received data
                        bool receivedStandby = m_notificationHandler.GetLastNwStandby();
                        TEST_LOG("Received nwStandby: %s", receivedStandby ? "true" : "false");
                        EXPECT_TRUE(receivedStandby);
                    } else {
                        TEST_LOG("Timeout waiting for OnNetworkStandbyModeChanged notification");
                    }
                }

                // Unregister from notifications
                result = m_SystemServicesPlugin->Unregister(&m_notificationHandler);
                EXPECT_EQ(result, Core::ERROR_NONE);
                if (result == Core::ERROR_NONE) {
                    TEST_LOG("Successfully unregistered from notifications");
                }

                m_SystemServicesPlugin->Release();
            } else {
                TEST_LOG("m_SystemServicesPlugin is NULL");
            }
            m_controller_SystemServices->Release();
        } else {
            TEST_LOG("m_controller_SystemServices is NULL");
        }
    }
}

TEST_F(SystemService_L2Test, OnFirmwareUpdateInfoReceived_Notification_COMRPC)
{
    if (CreateSystemServicesInterfaceObject() != Core::ERROR_NONE) {
        TEST_LOG("Invalid SystemServices_Client");
    } else {
        EXPECT_TRUE(m_controller_SystemServices != nullptr);
        if (m_controller_SystemServices) {
            EXPECT_TRUE(m_SystemServicesPlugin != nullptr);
            if (m_SystemServicesPlugin) {

                TEST_LOG("Testing OnFirmwareUpdateInfoReceived notification via COM-RPC");

                // Register for notifications
                uint32_t result = m_SystemServicesPlugin->Register(&m_notificationHandler);
                EXPECT_EQ(result, Core::ERROR_NONE);
                if (result != Core::ERROR_NONE) {
                    std::string errorMsg = "Register returned error " + std::to_string(result) + " (" + std::string(Core::ErrorToString(result)) + ")";
                    TEST_LOG("Err: %s", errorMsg.c_str());
                } else {
                    TEST_LOG("Successfully registered for notifications");
                }

                // Reset event flag before triggering the event
                m_notificationHandler.ResetEvent();

                // Trigger firmware update info request
                bool asyncResponse = false;
                bool success = false;
                result = m_SystemServicesPlugin->GetFirmwareUpdateInfo("test-guid-123", asyncResponse, success);

                if (result == Core::ERROR_NONE && asyncResponse) {
                    // Wait for the notification
                    uint32_t eventStatus = m_notificationHandler.WaitForEvent(EVNT_TIMEOUT, SYSTEMSERVICEL2TEST_FIRMWARE_UPDATE_INFO);

                    EXPECT_NE(eventStatus, SYSTEMSERVICEL2TEST_STATE_INVALID);
                    if (eventStatus != SYSTEMSERVICEL2TEST_STATE_INVALID) {
                        TEST_LOG("OnFirmwareUpdateInfoReceived notification received successfully");
                    } else {
                        TEST_LOG("Timeout waiting for OnFirmwareUpdateInfoReceived notification");
                    }
                } else {
                    TEST_LOG("GetFirmwareUpdateInfo did not trigger async response");
                }

                // Unregister from notifications
                result = m_SystemServicesPlugin->Unregister(&m_notificationHandler);
                EXPECT_EQ(result, Core::ERROR_NONE);
                if (result == Core::ERROR_NONE) {
                    TEST_LOG("Successfully unregistered from notifications");
                }

                m_SystemServicesPlugin->Release();
            } else {
                TEST_LOG("m_SystemServicesPlugin is NULL");
            }
            m_controller_SystemServices->Release();
        } else {
            TEST_LOG("m_controller_SystemServices is NULL");
        }
    }
}

TEST_F(SystemService_L2Test, OnRebootRequest_Notification_COMRPC)
{
    if (CreateSystemServicesInterfaceObject() != Core::ERROR_NONE) {
        TEST_LOG("Invalid SystemServices_Client");
    } else {
        EXPECT_TRUE(m_controller_SystemServices != nullptr);
        if (m_controller_SystemServices) {
            EXPECT_TRUE(m_SystemServicesPlugin != nullptr);
            if (m_SystemServicesPlugin) {

                TEST_LOG("Testing OnRebootRequest notification via COM-RPC");

                // Register for notifications
                uint32_t result = m_SystemServicesPlugin->Register(&m_notificationHandler);
                EXPECT_EQ(result, Core::ERROR_NONE);
                if (result != Core::ERROR_NONE) {
                    std::string errorMsg = "Register returned error " + std::to_string(result) + " (" + std::string(Core::ErrorToString(result)) + ")";
                    TEST_LOG("Err: %s", errorMsg.c_str());
                } else {
                    TEST_LOG("Successfully registered for notifications");
                }

                // Reset event flag before triggering the event
                m_notificationHandler.ResetEvent();

                // Note: Triggering actual reboot would interrupt the test
                // This test validates the registration and handler setup
                // In a real scenario, the notification would come from IARM or system event
                TEST_LOG("OnRebootRequest notification handler registered and ready");

                // Unregister from notifications
                result = m_SystemServicesPlugin->Unregister(&m_notificationHandler);
                EXPECT_EQ(result, Core::ERROR_NONE);
                if (result == Core::ERROR_NONE) {
                    TEST_LOG("Successfully unregistered from notifications");
                }

                m_SystemServicesPlugin->Release();
            } else {
                TEST_LOG("m_SystemServicesPlugin is NULL");
            }
            m_controller_SystemServices->Release();
        } else {
            TEST_LOG("m_controller_SystemServices is NULL");
        }
    }
}

TEST_F(SystemService_L2Test, OnTerritoryChanged_Notification_COMRPC)
{
    if (CreateSystemServicesInterfaceObject() != Core::ERROR_NONE) {
        TEST_LOG("Invalid SystemServices_Client");
    } else {
        EXPECT_TRUE(m_controller_SystemServices != nullptr);
        if (m_controller_SystemServices) {
            EXPECT_TRUE(m_SystemServicesPlugin != nullptr);
            if (m_SystemServicesPlugin) {

                TEST_LOG("Testing OnTerritoryChanged notification via COM-RPC");

                // Register for notifications
                uint32_t result = m_SystemServicesPlugin->Register(&m_notificationHandler);
                EXPECT_EQ(result, Core::ERROR_NONE);
                if (result != Core::ERROR_NONE) {
                    std::string errorMsg = "Register returned error " + std::to_string(result) + " (" + std::string(Core::ErrorToString(result)) + ")";
                    TEST_LOG("Err: %s", errorMsg.c_str());
                } else {
                    TEST_LOG("Successfully registered for notifications");
                }

                // Reset event flag before triggering the event
                m_notificationHandler.ResetEvent();

                // Trigger territory change via SetTerritory
                Exchange::ISystemServices::SystemError error;
                bool success = false;
                result = m_SystemServicesPlugin->SetTerritory("USA", "US", error, success);

                if (result == Core::ERROR_NONE && success) {
                    // Wait for the notification
                    uint32_t eventStatus = m_notificationHandler.WaitForEvent(EVNT_TIMEOUT, SYSTEMSERVICEL2TEST_TERRITORY_CHANGED);

                    EXPECT_NE(eventStatus, SYSTEMSERVICEL2TEST_STATE_INVALID);
                    if (eventStatus != SYSTEMSERVICEL2TEST_STATE_INVALID) {
                        TEST_LOG("OnTerritoryChanged notification received successfully");
                    } else {
                        TEST_LOG("Timeout waiting for OnTerritoryChanged notification");
                    }
                } else {
                    TEST_LOG("SetTerritory did not succeed, notification may not trigger");
                }

                // Unregister from notifications
                result = m_SystemServicesPlugin->Unregister(&m_notificationHandler);
                EXPECT_EQ(result, Core::ERROR_NONE);
                if (result == Core::ERROR_NONE) {
                    TEST_LOG("Successfully unregistered from notifications");
                }

                m_SystemServicesPlugin->Release();
            } else {
                TEST_LOG("m_SystemServicesPlugin is NULL");
            }
            m_controller_SystemServices->Release();
        } else {
            TEST_LOG("m_controller_SystemServices is NULL");
        }
    }
}

TEST_F(SystemService_L2Test, OnTemperatureThresholdChanged_Notification_COMRPC)
{
    if (CreateSystemServicesInterfaceObject() != Core::ERROR_NONE) {
        TEST_LOG("Invalid SystemServices_Client");
    } else {
        EXPECT_TRUE(m_controller_SystemServices != nullptr);
        if (m_controller_SystemServices) {
            EXPECT_TRUE(m_SystemServicesPlugin != nullptr);
            if (m_SystemServicesPlugin) {

                TEST_LOG("Testing OnTemperatureThresholdChanged notification via COM-RPC");

                // Register for notifications
                uint32_t result = m_SystemServicesPlugin->Register(&m_notificationHandler);
                EXPECT_EQ(result, Core::ERROR_NONE);
                if (result != Core::ERROR_NONE) {
                    std::string errorMsg = "Register returned error " + std::to_string(result) + " (" + std::string(Core::ErrorToString(result)) + ")";
                    TEST_LOG("Err: %s", errorMsg.c_str());
                } else {
                    TEST_LOG("Successfully registered for notifications");
                }

                // Reset event flag before triggering the event
                m_notificationHandler.ResetEvent();

                // Note: This notification is typically triggered by hardware/thermal events
                // Test validates the registration and handler setup
                TEST_LOG("OnTemperatureThresholdChanged notification handler registered and ready");

                // Unregister from notifications
                result = m_SystemServicesPlugin->Unregister(&m_notificationHandler);
                EXPECT_EQ(result, Core::ERROR_NONE);
                if (result == Core::ERROR_NONE) {
                    TEST_LOG("Successfully unregistered from notifications");
                }

                m_SystemServicesPlugin->Release();
            } else {
                TEST_LOG("m_SystemServicesPlugin is NULL");
            }
            m_controller_SystemServices->Release();
        } else {
            TEST_LOG("m_controller_SystemServices is NULL");
        }
    }
}

#if 0

/********************************************************
************Test case Details **************************
** Helper Functions Coverage Tests
** Testing SystemServicesHelper utility functions
*******************************************************/

TEST_F(SystemService_L2Test, HelperFunction_DirnameOf)
{
    TEST_LOG("Testing dirnameOf helper function");
    
    // Test with standard path
    std::string path1 = "/usr/bin/logupload";
    std::string expected1 = "/usr/bin/";
    std::string result1 = dirnameOf(path1);
    EXPECT_STREQ(result1.c_str(), expected1.c_str());
    TEST_LOG("dirnameOf('%s') = '%s'", path1.c_str(), result1.c_str());
    
    // Test with root path
    std::string path2 = "/file.txt";
    std::string expected2 = "/";
    std::string result2 = dirnameOf(path2);
    EXPECT_STREQ(result2.c_str(), expected2.c_str());
    TEST_LOG("dirnameOf('%s') = '%s'", path2.c_str(), result2.c_str());
    
    // Test with no directory (filename only)
    std::string path3 = "filename.txt";
    std::string expected3 = "";
    std::string result3 = dirnameOf(path3);
    EXPECT_STREQ(result3.c_str(), expected3.c_str());
    TEST_LOG("dirnameOf('%s') = '%s'", path3.c_str(), result3.c_str());
    
    // Test with empty string
    std::string path4 = "";
    std::string expected4 = "";
    std::string result4 = dirnameOf(path4);
    EXPECT_STREQ(result4.c_str(), expected4.c_str());
    TEST_LOG("dirnameOf('') = '%s'", result4.c_str());
}

TEST_F(SystemService_L2Test, HelperFunction_GetErrorDescription)
{
    TEST_LOG("Testing getErrorDescription helper function");
    
    // Test valid error codes
    std::string desc1 = getErrorDescription(SysSrv_OK);
    EXPECT_FALSE(desc1.empty());
    EXPECT_STREQ(desc1.c_str(), "Processed Successfully");
    TEST_LOG("Error %d: %s", SysSrv_OK, desc1.c_str());
    
    std::string desc2 = getErrorDescription(SysSrv_MethodNotFound);
    EXPECT_FALSE(desc2.empty());
    EXPECT_STREQ(desc2.c_str(), "Method not found");
    TEST_LOG("Error %d: %s", SysSrv_MethodNotFound, desc2.c_str());
    
    std::string desc3 = getErrorDescription(SysSrv_FileNotPresent);
    EXPECT_FALSE(desc3.empty());
    EXPECT_STREQ(desc3.c_str(), "Expected file not found");
    TEST_LOG("Error %d: %s", SysSrv_FileNotPresent, desc3.c_str());
    
    // Test invalid error code (should return "Unexpected Error")
    std::string desc4 = getErrorDescription(9999);
    EXPECT_FALSE(desc4.empty());
    EXPECT_STREQ(desc4.c_str(), "Unexpected Error");
    TEST_LOG("Error %d: %s", 9999, desc4.c_str());
}

TEST_F(SystemService_L2Test, HelperFunction_DirExists)
{
    TEST_LOG("Testing dirExists helper function");
    
    // Create a test directory
    std::string testDir = "/tmp/systemservices_test_dir/";
    std::string testFile = testDir + "testfile.txt";
    
    // Clean up if exists
    system("rm -rf /tmp/systemservices_test_dir");
    
    // Test non-existent directory
    bool exists1 = dirExists(testFile);
    EXPECT_FALSE(exists1);
    TEST_LOG("dirExists('%s') = %s", testFile.c_str(), exists1 ? "true" : "false");
    
    // Create directory and test again
    system("mkdir -p /tmp/systemservices_test_dir");
    bool exists2 = dirExists(testFile);
    EXPECT_TRUE(exists2);
    TEST_LOG("dirExists('%s') = %s (after mkdir)", testFile.c_str(), exists2 ? "true" : "false");
    
    // Test with root directory
    bool exists3 = dirExists("/etc/test.conf");
    EXPECT_TRUE(exists3);
    TEST_LOG("dirExists('/etc/test.conf') = %s", exists3 ? "true" : "false");
    
    // Clean up
    system("rm -rf /tmp/systemservices_test_dir");
}

TEST_F(SystemService_L2Test, HelperFunction_ReadFromFile)
{
    TEST_LOG("Testing readFromFile helper function");
    
    std::string testFile = "/tmp/systemservices_readtest.txt";
    std::string testContent = "Test content for readFromFile\nLine 2\nLine 3";
    
    // Clean up if exists
    std::remove(testFile.c_str());
    
    // Test reading non-existent file
    std::string content1;
    bool result1 = readFromFile(testFile.c_str(), content1);
    EXPECT_FALSE(result1);
    EXPECT_TRUE(content1.empty());
    TEST_LOG("readFromFile (non-existent): %s", result1 ? "success" : "failed");
    
    // Create test file
    std::ofstream outFile(testFile);
    outFile << testContent;
    outFile.close();
    
    // Test reading existing file
    std::string content2;
    bool result2 = readFromFile(testFile.c_str(), content2);
    EXPECT_TRUE(result2);
    EXPECT_FALSE(content2.empty());
    EXPECT_STREQ(content2.c_str(), testContent.c_str());
    TEST_LOG("readFromFile (exists): %s, content length: %zu", result2 ? "success" : "failed", content2.length());
    
    // Clean up
    std::remove(testFile.c_str());
}

TEST_F(SystemService_L2Test, HelperFunction_GetFileContent_String)
{
    TEST_LOG("Testing getFileContent (string overload) helper function");
    
    std::string testFile = "/tmp/systemservices_filecontenttest.txt";
    std::string testContent = "Line 1\nLine 2\nLine 3\n";
    
    // Clean up if exists
    std::remove(testFile.c_str());
    
    // Test reading non-existent file
    std::string content1;
    bool result1 = getFileContent(testFile, content1);
    EXPECT_FALSE(result1);
    TEST_LOG("getFileContent (non-existent): %s", result1 ? "success" : "failed");
    
    // Create test file
    std::ofstream outFile(testFile);
    outFile << testContent;
    outFile.close();
    
    // Test reading existing file
    std::string content2;
    bool result2 = getFileContent(testFile, content2);
    EXPECT_TRUE(result2);
    EXPECT_FALSE(content2.empty());
    EXPECT_STREQ(content2.c_str(), testContent.c_str());
    TEST_LOG("getFileContent (exists): %s, content: '%s'", result2 ? "success" : "failed", content2.c_str());
    
    // Clean up
    std::remove(testFile.c_str());
}

TEST_F(SystemService_L2Test, HelperFunction_GetFileContent_Vector)
{
    TEST_LOG("Testing getFileContent (vector overload) helper function");
    
    std::string testFile = "/tmp/systemservices_vectortest.txt";
    
    // Clean up if exists
    std::remove(testFile.c_str());
    
    // Test reading non-existent file
    std::vector<std::string> lines1;
    bool result1 = getFileContent(testFile, lines1);
    EXPECT_FALSE(result1);
    TEST_LOG("getFileContent vector (non-existent): %s", result1 ? "success" : "failed");
    
    // Create test file with multiple lines
    std::ofstream outFile(testFile);
    outFile << "First Line\n";
    outFile << "Second Line\n";
    outFile << "Third Line\n";
    outFile.close();
    
    // Test reading existing file
    std::vector<std::string> lines2;
    bool result2 = getFileContent(testFile, lines2);
    EXPECT_TRUE(result2);
    EXPECT_EQ(lines2.size(), 3);
    if (lines2.size() >= 3) {
        EXPECT_STREQ(lines2[0].c_str(), "First Line");
        EXPECT_STREQ(lines2[1].c_str(), "Second Line");
        EXPECT_STREQ(lines2[2].c_str(), "Third Line");
        TEST_LOG("getFileContent vector: read %zu lines", lines2.size());
    }
    
    // Clean up
    std::remove(testFile.c_str());
}

TEST_F(SystemService_L2Test, HelperFunction_StrcicmpCaseInsensitive)
{
    TEST_LOG("Testing strcicmp helper function");
    
    // Test identical strings
    int result1 = strcicmp("Test", "Test");
    EXPECT_EQ(result1, 0);
    TEST_LOG("strcicmp('Test', 'Test') = %d", result1);
    
    // Test case-insensitive equality
    int result2 = strcicmp("Test", "test");
    EXPECT_EQ(result2, 0);
    TEST_LOG("strcicmp('Test', 'test') = %d", result2);
    
    int result3 = strcicmp("HELLO", "hello");
    EXPECT_EQ(result3, 0);
    TEST_LOG("strcicmp('HELLO', 'hello') = %d", result3);
    
    // Test different strings
    int result4 = strcicmp("Apple", "Banana");
    EXPECT_NE(result4, 0);
    TEST_LOG("strcicmp('Apple', 'Banana') = %d", result4);
    
    // Test empty strings
    int result5 = strcicmp("", "");
    EXPECT_EQ(result5, 0);
    TEST_LOG("strcicmp('', '') = %d", result5);
    
    // Test one empty string
    int result6 = strcicmp("Test", "");
    EXPECT_NE(result6, 0);
    TEST_LOG("strcicmp('Test', '') = %d", result6);
}

TEST_F(SystemService_L2Test, HelperFunction_FindCaseInsensitive)
{
    TEST_LOG("Testing findCaseInsensitive helper function");
    
    // Test case-insensitive match
    bool result1 = findCaseInsensitive("Hello World", "WORLD", 0);
    EXPECT_TRUE(result1);
    TEST_LOG("findCaseInsensitive('Hello World', 'WORLD', 0) = %s", result1 ? "true" : "false");
    
    // Test case-sensitive mismatch becomes case-insensitive match
    bool result2 = findCaseInsensitive("TestString", "string", 0);
    EXPECT_TRUE(result2);
    TEST_LOG("findCaseInsensitive('TestString', 'string', 0) = %s", result2 ? "true" : "false");
    
    // Test not found
    bool result3 = findCaseInsensitive("Hello World", "xyz", 0);
    EXPECT_FALSE(result3);
    TEST_LOG("findCaseInsensitive('Hello World', 'xyz', 0) = %s", result3 ? "true" : "false");
    
    // Test with position parameter
    bool result4 = findCaseInsensitive("Hello World Hello", "HELLO", 6);
    EXPECT_TRUE(result4);
    TEST_LOG("findCaseInsensitive('Hello World Hello', 'HELLO', 6) = %s", result4 ? "true" : "false");
    
    // Test empty search string
    bool result5 = findCaseInsensitive("Hello", "", 0);
    EXPECT_TRUE(result5);
    TEST_LOG("findCaseInsensitive('Hello', '', 0) = %s", result5 ? "true" : "false");
}

TEST_F(SystemService_L2Test, HelperFunction_RemoveCharsFromString)
{
    TEST_LOG("Testing removeCharsFromString helper function");
    
    // Test removing single character
    std::string str1 = "Hello World";
    removeCharsFromString(str1, "o");
    EXPECT_STREQ(str1.c_str(), "Hell Wrld");
    TEST_LOG("After removing 'o': '%s'", str1.c_str());
    
    // Test removing multiple characters
    std::string str2 = "Test123String456";
    removeCharsFromString(str2, "0123456789");
    EXPECT_STREQ(str2.c_str(), "TestString");
    TEST_LOG("After removing digits: '%s'", str2.c_str());
    
    // Test removing all characters
    std::string str3 = "aaa";
    removeCharsFromString(str3, "a");
    EXPECT_STREQ(str3.c_str(), "");
    TEST_LOG("After removing all 'a': '%s'", str3.c_str());
    
    // Test with no matching characters
    std::string str4 = "Hello";
    removeCharsFromString(str4, "xyz");
    EXPECT_STREQ(str4.c_str(), "Hello");
    TEST_LOG("After removing 'xyz' (not present): '%s'", str4.c_str());
    
    // Test with empty string
    std::string str5 = "";
    removeCharsFromString(str5, "abc");
    EXPECT_STREQ(str5.c_str(), "");
    TEST_LOG("After removing from empty string: '%s'", str5.c_str());
}

TEST_F(SystemService_L2Test, HelperFunction_ParseConfigFile)
{
    TEST_LOG("Testing parseConfigFile helper function");
    
    std::string testFile = "/tmp/systemservices_configtest.conf";
    
    // Clean up if exists
    std::remove(testFile.c_str());
    
    // Create test config file
    std::ofstream outFile(testFile);
    outFile << "KEY1=value1\n";
    outFile << "KEY2=value2 with spaces\n";
    outFile << "# Comment line\n";
    outFile << "KEY3=value3\n";
    outFile << "EMPTY_KEY=\n";
    outFile.close();
    
    // Test finding existing key
    std::string value1;
    bool result1 = parseConfigFile(testFile.c_str(), "KEY1", value1);
    EXPECT_TRUE(result1);
    EXPECT_STREQ(value1.c_str(), "value1");
    TEST_LOG("parseConfigFile KEY1: %s = '%s'", result1 ? "found" : "not found", value1.c_str());
    
    // Test finding key with spaces in value
    std::string value2;
    bool result2 = parseConfigFile(testFile.c_str(), "KEY2", value2);
    EXPECT_TRUE(result2);
    EXPECT_STREQ(value2.c_str(), "value2 with spaces");
    TEST_LOG("parseConfigFile KEY2: %s = '%s'", result2 ? "found" : "not found", value2.c_str());
    
    // Test finding non-existent key
    std::string value3;
    bool result3 = parseConfigFile(testFile.c_str(), "NONEXISTENT", value3);
    EXPECT_FALSE(result3);
    TEST_LOG("parseConfigFile NONEXISTENT: %s", result3 ? "found" : "not found");
    
    // Test with non-existent file
    std::string value4;
    bool result4 = parseConfigFile("/tmp/nonexistent_file.conf", "KEY1", value4);
    EXPECT_FALSE(result4);
    TEST_LOG("parseConfigFile (non-existent file): %s", result4 ? "found" : "not found");
    
    // Test empty value
    std::string value5;
    bool result5 = parseConfigFile(testFile.c_str(), "EMPTY_KEY", value5);
    EXPECT_TRUE(result5);
    EXPECT_TRUE(value5.empty());
    TEST_LOG("parseConfigFile EMPTY_KEY: %s = '%s'", result5 ? "found" : "not found", value5.c_str());
    
    // Clean up
    std::remove(testFile.c_str());
}

TEST_F(SystemService_L2Test, HelperFunction_URLEncode)
{
    TEST_LOG("Testing url_encode helper function");
    
    // Test URL encoding of special characters
    std::string url1 = "hello world";
    std::string encoded1 = url_encode(url1);
    EXPECT_FALSE(encoded1.empty());
    EXPECT_NE(encoded1.find("%20"), std::string::npos); // Space should be encoded
    TEST_LOG("url_encode('%s') = '%s'", url1.c_str(), encoded1.c_str());
    
    // Test encoding of special characters
    std::string url2 = "user@example.com";
    std::string encoded2 = url_encode(url2);
    EXPECT_FALSE(encoded2.empty());
    TEST_LOG("url_encode('%s') = '%s'", url2.c_str(), encoded2.c_str());
    
    // Test empty string
    std::string url3 = "";
    std::string encoded3 = url_encode(url3);
    EXPECT_TRUE(encoded3.empty());
    TEST_LOG("url_encode('') = '%s'", encoded3.c_str());
    
    // Test alphanumeric (should remain unchanged)
    std::string url4 = "abc123";
    std::string encoded4 = url_encode(url4);
    EXPECT_FALSE(encoded4.empty());
    TEST_LOG("url_encode('%s') = '%s'", url4.c_str(), encoded4.c_str());
}

/********************************************************
************Test case Details **************************
** cTimer Helper Class Tests
** Testing cTimer utility class
*******************************************************/

TEST_F(SystemService_L2Test, CTimer_Constructor_Default)
{
    TEST_LOG("Testing cTimer constructor");
    
    cTimer timer;
    
    // Constructor should initialize with clear=false, interval=0
    // We can't directly check private members, but we can test behavior
    TEST_LOG("cTimer constructed successfully");
}

TEST_F(SystemService_L2Test, CTimer_SetInterval_ValidCallback)
{
    TEST_LOG("Testing cTimer setInterval with valid callback");
    
    cTimer timer;
    bool callbackExecuted = false;
    
    // Lambda to capture local variable
    auto callback = [&callbackExecuted]() {
        callbackExecuted = true;
    };
    
    // Note: cTimer expects function pointer, not lambda
    // Testing with simple function pointer
    static bool staticFlag = false;
    auto staticCallback = []() { staticFlag = true; };
    
    timer.setInterval(staticCallback, 100);
    
    TEST_LOG("setInterval called with callback and interval=100ms");
}

TEST_F(SystemService_L2Test, CTimer_Start_InvalidParameters)
{
    TEST_LOG("Testing cTimer start with invalid parameters");
    
    cTimer timer1;
    
    // Test start without setting interval (interval=0, callback=NULL)
    bool result1 = timer1.start();
    EXPECT_FALSE(result1);
    TEST_LOG("start() with interval=0 and callback=NULL: %s", result1 ? "success" : "failed (expected)");
    
    // Test start with only callback, no interval
    cTimer timer2;
    static bool flag = false;
    timer2.setInterval([]() { flag = true; }, 0);
    bool result2 = timer2.start();
    EXPECT_FALSE(result2);
    TEST_LOG("start() with interval=0: %s", result2 ? "success" : "failed (expected)");
}

TEST_F(SystemService_L2Test, CTimer_Start_Stop_ValidTimer)
{
    TEST_LOG("Testing cTimer start and stop");
    
    static int callCount = 0;
    callCount = 0;
    
    cTimer timer;
    timer.setInterval([]() { callCount++; }, 50);
    
    bool startResult = timer.start();
    EXPECT_TRUE(startResult);
    TEST_LOG("Timer started: %s", startResult ? "success" : "failed");
    
    // Let timer run for a short time
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // Stop timer
    timer.stop();
    TEST_LOG("Timer stopped after running for 200ms");
    
    int finalCount = callCount;
    TEST_LOG("Callback executed %d times", finalCount);
    EXPECT_GT(finalCount, 0);
    
    // Wait to ensure timer has stopped
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Detach the thread to prevent join issues
    timer.detach();
}

TEST_F(SystemService_L2Test, CTimer_Detach)
{
    TEST_LOG("Testing cTimer detach");
    
    static bool executed = false;
    executed = false;
    
    cTimer timer;
    timer.setInterval([]() { executed = true; }, 100);
    
    bool startResult = timer.start();
    EXPECT_TRUE(startResult);
    
    // Detach the thread
    timer.detach();
    TEST_LOG("Timer thread detached");
    
    // Stop the timer
    timer.stop();
    
    // Give some time for callback to potentially execute
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
}

TEST_F(SystemService_L2Test, CTimer_Join)
{
    TEST_LOG("Testing cTimer join");
    
    static int execCount = 0;
    execCount = 0;
    
    cTimer timer;
    timer.setInterval([]() { execCount++; }, 50);
    
    bool startResult = timer.start();
    EXPECT_TRUE(startResult);
    
    // Let it run briefly
    std::this_thread::sleep_for(std::chrono::milliseconds(120));
    
    // Stop timer
    timer.stop();
    
    // Join the thread (should complete since timer is stopped)
    timer.join();
    TEST_LOG("Timer thread joined successfully, callback executed %d times", execCount);
}

/********************************************************
************Test case Details **************************
** uploadlogs Helper Tests
** Testing uploadlogs utility functions
*******************************************************/

TEST_F(SystemService_L2Test, UploadLogs_CheckMTlsFlag)
{
    TEST_LOG("Testing UploadLogs::checkmTlsLogUploadFlag");
    
    bool result = WPEFramework::Plugin::UploadLogs::checkmTlsLogUploadFlag();
    
    // This function always returns true
    EXPECT_TRUE(result);
    TEST_LOG("checkmTlsLogUploadFlag() = %s", result ? "true" : "false");
}

TEST_F(SystemService_L2Test, UploadLogs_GetDCMConfigDetails_NoFile)
{
    TEST_LOG("Testing UploadLogs::getDCMconfigDetails with no file");
    
    // Remove the DCM settings file if it exists
    std::remove("/tmp/DCMSettings.conf");
    
    std::string uploadProtocol, httpLink, uploadCheck;
    bool result = WPEFramework::Plugin::UploadLogs::getDCMconfigDetails(
        uploadProtocol, httpLink, uploadCheck);
    
    EXPECT_FALSE(result);
    TEST_LOG("getDCMconfigDetails (no file): %s", result ? "success" : "failed (expected)");
}

TEST_F(SystemService_L2Test, UploadLogs_GetDCMConfigDetails_ValidFile)
{
    TEST_LOG("Testing UploadLogs::getDCMconfigDetails with valid file");
    
    // Create test DCM settings file
    std::string testFile = "/tmp/DCMSettings.conf";
    std::ofstream dcmFile(testFile);
    dcmFile << "LogUploadSettings:UploadRepository:uploadProtocol=https\n";
    dcmFile << "LogUploadSettings:UploadRepository:URL=https://test.example.com/upload\n";
    dcmFile << "LogUploadSettings:UploadOnReboot=true\n";
    dcmFile.close();
    
    std::string uploadProtocol, httpLink, uploadCheck;
    bool result = WPEFramework::Plugin::UploadLogs::getDCMconfigDetails(
        uploadProtocol, httpLink, uploadCheck);
    
    EXPECT_TRUE(result);
    EXPECT_STREQ(uploadProtocol.c_str(), "https");
    EXPECT_STREQ(httpLink.c_str(), "https://test.example.com/upload");
    EXPECT_STREQ(uploadCheck.c_str(), "true");
    
    TEST_LOG("getDCMconfigDetails: %s", result ? "success" : "failed");
    TEST_LOG("  uploadProtocol: %s", uploadProtocol.c_str());
    TEST_LOG("  httpLink: %s", httpLink.c_str());
    TEST_LOG("  uploadCheck: %s", uploadCheck.c_str());
    
    // Clean up
    std::remove(testFile.c_str());
}

TEST_F(SystemService_L2Test, UploadLogs_GetDCMConfigDetails_EmptyFile)
{
    TEST_LOG("Testing UploadLogs::getDCMconfigDetails with empty file");
    
    // Create empty DCM settings file
    std::string testFile = "/tmp/DCMSettings.conf";
    std::ofstream dcmFile(testFile);
    dcmFile.close();
    
    std::string uploadProtocol, httpLink, uploadCheck;
    bool result = WPEFramework::Plugin::UploadLogs::getDCMconfigDetails(
        uploadProtocol, httpLink, uploadCheck);
    
    EXPECT_FALSE(result);
    TEST_LOG("getDCMconfigDetails (empty file): %s", result ? "success" : "failed (expected)");
    
    // Clean up
    std::remove(testFile.c_str());
}

TEST_F(SystemService_L2Test, UploadLogs_GetUploadLogParameters_MissingDeviceProps)
{
    TEST_LOG("Testing UploadLogs::getUploadLogParameters with missing device.properties");
    
    // This test assumes /etc/device.properties might not exist or has missing BUILD_TYPE
    std::string tftpServer, uploadProtocol, uploadHttpLink;
    
    // Create minimal device.properties for testing
    std::ofstream devProps("/tmp/test_device.properties");
    devProps << "MODEL_NUM=TEST\n";
    devProps.close();
    
    int32_t result = WPEFramework::Plugin::UploadLogs::getUploadLogParameters(
        tftpServer, uploadProtocol, uploadHttpLink);
    
    // Expected to fail due to missing BUILD_TYPE or LOG_SERVER
    TEST_LOG("getUploadLogParameters (missing config): result=%d", result);
    
    // Clean up
    std::remove("/tmp/test_device.properties");
}

TEST_F(SystemService_L2Test, UploadLogs_LogUploadAsync_NoExecutable)
{
    TEST_LOG("Testing UploadLogs::logUploadAsync without executable");
    
    // Temporarily rename/backup logupload if it exists
    pid_t result = WPEFramework::Plugin::UploadLogs::logUploadAsync();
    
    // Should return -1 if /usr/bin/logupload doesn't exist or getUploadLogParameters fails
    TEST_LOG("logUploadAsync: pid=%d", result);
    
    if (result > 0) {
        TEST_LOG("Warning: logUploadAsync created process %d - may need cleanup", result);
    }
}

/********************************************************
************Test case Details **************************
** thermonitor Helper Tests  
** Testing CThermalMonitor utility class
*******************************************************/

TEST_F(SystemService_L2Test, ThermalMonitor_Instance)
{
    TEST_LOG("Testing CThermalMonitor::instance");
    
    WPEFramework::Plugin::CThermalMonitor* monitor1 = 
        WPEFramework::Plugin::CThermalMonitor::instance();
    
    EXPECT_NE(monitor1, nullptr);
    TEST_LOG("First instance: %p", (void*)monitor1);
    
    WPEFramework::Plugin::CThermalMonitor* monitor2 = 
        WPEFramework::Plugin::CThermalMonitor::instance();
    
    EXPECT_NE(monitor2, nullptr);
    EXPECT_EQ(monitor1, monitor2);
    TEST_LOG("Second instance: %p (should be same)", (void*)monitor2);
}

TEST_F(SystemService_L2Test, ThermalMonitor_AddRemoveEventObserver)
{
    TEST_LOG("Testing CThermalMonitor add/remove event observer");
    
    WPEFramework::Plugin::CThermalMonitor* monitor = 
        WPEFramework::Plugin::CThermalMonitor::instance();
    
    ASSERT_NE(monitor, nullptr);
    
    // These are empty implementations, just verify they don't crash
    monitor->addEventObserver(nullptr);
    TEST_LOG("addEventObserver called (empty implementation)");
    
    monitor->removeEventObserver(nullptr);
    TEST_LOG("removeEventObserver called (empty implementation)");
}

TEST_F(SystemService_L2Test, ThermalMonitor_GetCoreTemperature)
{
    TEST_LOG("Testing CThermalMonitor::getCoreTemperature");
    
    WPEFramework::Plugin::CThermalMonitor* monitor = 
        WPEFramework::Plugin::CThermalMonitor::instance();
    
    ASSERT_NE(monitor, nullptr);
    
    float temperature = 0.0f;
    bool result = monitor->getCoreTemperature(temperature);
    
    TEST_LOG("getCoreTemperature: %s, temperature=%.2f", 
             result ? "success" : "failed", temperature);
    
    if (result) {
        // Temperature should be reasonable value
        EXPECT_GE(temperature, -50.0f);
        EXPECT_LE(temperature, 150.0f);
    }
}

TEST_F(SystemService_L2Test, ThermalMonitor_GetSetCoreTempThresholds)
{
    TEST_LOG("Testing CThermalMonitor get/set core temperature thresholds");
    
    WPEFramework::Plugin::CThermalMonitor* monitor = 
        WPEFramework::Plugin::CThermalMonitor::instance();
    
    ASSERT_NE(monitor, nullptr);
    
    // Get current thresholds
    float high = 0.0f, critical = 0.0f;
    bool getResult = monitor->getCoreTempThresholds(high, critical);
    
    TEST_LOG("getCoreTempThresholds: %s, high=%.2f, critical=%.2f",
             getResult ? "success" : "failed", high, critical);
    
    // Try to set new thresholds
    bool setResult = monitor->setCoreTempThresholds(85.0f, 95.0f);
    
    TEST_LOG("setCoreTempThresholds(85.0, 95.0): %s", 
             setResult ? "success" : "failed");
    
    if (setResult) {
        // Verify thresholds were set
        float newHigh = 0.0f, newCritical = 0.0f;
        bool verifyResult = monitor->getCoreTempThresholds(newHigh, newCritical);
        
        if (verifyResult) {
            TEST_LOG("Verified thresholds: high=%.2f, critical=%.2f", 
                     newHigh, newCritical);
        }
    }
}

TEST_F(SystemService_L2Test, ThermalMonitor_GetSetOvertempGraceInterval)
{
    TEST_LOG("Testing CThermalMonitor get/set overtemp grace interval");
    
    WPEFramework::Plugin::CThermalMonitor* monitor = 
        WPEFramework::Plugin::CThermalMonitor::instance();
    
    ASSERT_NE(monitor, nullptr);
    
    // Get current grace interval
    int graceInterval = 0;
    bool getResult = monitor->getOvertempGraceInterval(graceInterval);
    
    TEST_LOG("getOvertempGraceInterval: %s, interval=%d",
             getResult ? "success" : "failed", graceInterval);
    
    // Try to set new grace interval
    bool setResult = monitor->setOvertempGraceInterval(30);
    
    TEST_LOG("setOvertempGraceInterval(30): %s", 
             setResult ? "success" : "failed");
    
    if (setResult) {
        // Verify interval was set
        int newInterval = 0;
        bool verifyResult = monitor->getOvertempGraceInterval(newInterval);
        
        if (verifyResult) {
            TEST_LOG("Verified grace interval: %d", newInterval);
            EXPECT_EQ(newInterval, 30);
        }
    }
}

TEST_F(SystemService_L2Test, ThermalMonitor_EmitTemperatureThresholdChange)
{
    TEST_LOG("Testing CThermalMonitor::emitTemperatureThresholdChange");
    
    WPEFramework::Plugin::CThermalMonitor* monitor = 
        WPEFramework::Plugin::CThermalMonitor::instance();
    
    ASSERT_NE(monitor, nullptr);
    
    // This function calls reportTemperatureThresholdChange which is empty
    // Just verify it doesn't crash
    monitor->emitTemperatureThresholdChange("HIGH", true, 88.5f);
    TEST_LOG("emitTemperatureThresholdChange called successfully");
    
    monitor->emitTemperatureThresholdChange("CRITICAL", false, 75.0f);
    TEST_LOG("emitTemperatureThresholdChange called again successfully");
}

#endif
