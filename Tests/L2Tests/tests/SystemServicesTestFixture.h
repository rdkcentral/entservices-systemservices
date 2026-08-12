/**
* If not stated otherwise in this file or this component's LICENSE
* file the following copyright and licenses apply:
*
* Copyright 2026 RDK Management
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
#pragma once
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "L2Tests.h"
#include "L2TestsMock.h"
#include <mutex>
#include <condition_variable>
#include <fstream>
#include <sstream>
#include <interfaces/ISystemServices.h>
#include "deepSleepMgr.h"
#include "PowerManagerHalMock.h"
#include "MfrMock.h"
#include "../../../plugin/SystemServicesHelper.h"
#include "../../../plugin/thermonitor.h"
#include "../../../plugin/uploadlogs.h"
#include "../../../plugin/SystemServicesImplementation.h"
#ifdef ENABLE_SYSTIMEMGR_SUPPORT
#include "systimerifc/itimermsg.h"
#endif



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
    SYSTEMSERVICEL2TEST_CLOCK_SET = 0x00000400,
    SYSTEMSERVICEL2TEST_TIMEZONEDST_CHANGED = 0x00000800,
    SYSTEMSERVICEL2TEST_MACADDRESSES_RETREIVED = 0x00001000,
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
     // FirmwareUpdateInfo parameters
    int m_fwStatus;
    string m_fwResponseString;
    string m_fwUpdateVersion;
    bool m_fwRebootImmediately;
    bool m_fwUpdateAvailable;
    int m_fwUpdateAvailableEnum;
    bool m_fwSuccess;
    // TerritoryChanged parameters
    string m_territoryOld;
    string m_territoryNew;
    string m_territoryOldRegion;
    string m_territoryNewRegion;
    // TimeZoneDSTChanged parameters
    string m_tzOldTimeZone;
    string m_tzNewTimeZone;
    string m_tzOldAccuracy;
    string m_tzNewAccuracy;
    // MacAddresses parameters
    string m_macEcm;
    string m_macEstb;
    string m_macMoca;
    string m_macEth;
    string m_macWifi;
    string m_macBluetooth;
    string m_macRf4ce;
    string m_macInfo;
    bool m_macSuccess;

public:
    SystemServicesNotificationHandler()
        : m_event_signalled(SYSTEMSERVICEL2TEST_STATE_INVALID)
        , m_lastNwStandby(false)
        , m_fwStatus(0)
        , m_fwRebootImmediately(false)
        , m_fwUpdateAvailable(false)
        , m_fwUpdateAvailableEnum(0)
        , m_fwSuccess(false)
        , m_macSuccess(false)
    {
    }

    virtual ~SystemServicesNotificationHandler() = default;

    BEGIN_INTERFACE_MAP(SystemServicesNotificationHandler)
    INTERFACE_ENTRY(Exchange::ISystemServices::INotification)
    END_INTERFACE_MAP

    void OnFirmwareUpdateInfoReceived(const int status, const string& responseString, const string& firmwareUpdateVersion, const bool rebootImmediately, const bool updateAvailable, const int updateAvailableEnum, const bool success) override
    {
        TEST_LOG("OnFirmwareUpdateInfoReceived notification received");
        std::unique_lock<std::mutex> lock(m_mutex);
        m_fwStatus = status;
        m_fwResponseString = responseString;
        m_fwUpdateVersion = firmwareUpdateVersion;
        m_fwRebootImmediately = rebootImmediately;
        m_fwUpdateAvailable = updateAvailable;
        m_fwUpdateAvailableEnum = updateAvailableEnum;
        m_fwSuccess = success;
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

    void OnTerritoryChanged(const string& oldTerritory, const string& newTerritory, const string& oldRegion, const string& newRegion) override
    {
        TEST_LOG("OnTerritoryChanged notification received");
        TEST_LOG("  oldTerritory: %s, newTerritory: %s, oldRegion: %s, newRegion: %s",
                 oldTerritory.c_str(), newTerritory.c_str(), oldRegion.c_str(), newRegion.c_str());
        std::unique_lock<std::mutex> lock(m_mutex);
        m_territoryOld = oldTerritory;
        m_territoryNew = newTerritory;
        m_territoryOldRegion = oldRegion;
        m_territoryNewRegion = newRegion;
        m_event_signalled |= SYSTEMSERVICEL2TEST_TERRITORY_CHANGED;
        m_condition_variable.notify_one();
    }

    void OnTimeZoneDSTChanged(const string& oldTimeZone, const string& newTimeZone, const string& oldAccuracy, const string& newAccuracy) override
    {
        TEST_LOG("OnTimeZoneDSTChanged notification received");
        TEST_LOG("  oldTimeZone: %s, newTimeZone: %s, oldAccuracy: %s, newAccuracy: %s",
                 oldTimeZone.c_str(), newTimeZone.c_str(), oldAccuracy.c_str(), newAccuracy.c_str());
        std::unique_lock<std::mutex> lock(m_mutex);
        m_tzOldTimeZone = oldTimeZone;
        m_tzNewTimeZone = newTimeZone;
        m_tzOldAccuracy = oldAccuracy;
        m_tzNewAccuracy = newAccuracy;
		m_event_signalled |= SYSTEMSERVICEL2TEST_TIMEZONEDST_CHANGED;
        m_condition_variable.notify_one();
    }

    void OnMacAddressesRetreived(const string& ecmMac, const string& estbMac, const string& mocaMac, const string& ethMac, const string& wifiMac, const string& bluetoothMac, const string& rf4ceMac, const string& info, const bool success) override
    {
        TEST_LOG("OnMacAddressesRetreived notification received");
        TEST_LOG("  ecmMac: %s, estbMac: %s, success: %d", ecmMac.c_str(), estbMac.c_str(), success);
        std::unique_lock<std::mutex> lock(m_mutex);
        m_macEcm = ecmMac;
        m_macEstb = estbMac;
        m_macMoca = mocaMac;
        m_macEth = ethMac;
        m_macWifi = wifiMac;
        m_macBluetooth = bluetoothMac;
        m_macRf4ce = rf4ceMac;
        m_macInfo = info;
        m_macSuccess = success;
        m_event_signalled |= SYSTEMSERVICEL2TEST_MACADDRESSES_RETREIVED;
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
        m_event_signalled |= SYSTEMSERVICEL2TEST_CLOCK_SET;
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

    // FirmwareUpdateInfo getters
    int GetFwStatus() { 
        std::unique_lock<std::mutex> lock(m_mutex);
        return m_fwStatus;
    }
    string GetFwResponseString() {
        std::unique_lock<std::mutex> lock(m_mutex);
        return m_fwResponseString;
    }
    string GetFwUpdateVersion() {
        std::unique_lock<std::mutex> lock(m_mutex);
        return m_fwUpdateVersion;
    }
    bool GetFwRebootImmediately() {
        std::unique_lock<std::mutex> lock(m_mutex);
        return m_fwRebootImmediately;
    }
    bool GetFwUpdateAvailable() {
        std::unique_lock<std::mutex> lock(m_mutex);
        return m_fwUpdateAvailable;
    }
    int GetFwUpdateAvailableEnum() {
        std::unique_lock<std::mutex> lock(m_mutex);
        return m_fwUpdateAvailableEnum;
    }
    bool GetFwSuccess() {
        std::unique_lock<std::mutex> lock(m_mutex);
        return m_fwSuccess;
    }

    // TerritoryChanged getters
    string GetTerritoryOld() {
        std::unique_lock<std::mutex> lock(m_mutex);
        return m_territoryOld;
    }
    string GetTerritoryNew() {
        std::unique_lock<std::mutex> lock(m_mutex);
        return m_territoryNew;
    }
    string GetTerritoryOldRegion() {
        std::unique_lock<std::mutex> lock(m_mutex);
        return m_territoryOldRegion;
    }
    string GetTerritoryNewRegion() {
        std::unique_lock<std::mutex> lock(m_mutex);
        return m_territoryNewRegion;
    }

    // TimeZoneDST getters
    string GetTzOldTimeZone() {
        std::unique_lock<std::mutex> lock(m_mutex);
        return m_tzOldTimeZone;
    }
    string GetTzNewTimeZone() {
        std::unique_lock<std::mutex> lock(m_mutex);
        return m_tzNewTimeZone;
    }
    string GetTzOldAccuracy() {
        std::unique_lock<std::mutex> lock(m_mutex);
        return m_tzOldAccuracy;
    }
    string GetTzNewAccuracy() {
        std::unique_lock<std::mutex> lock(m_mutex);
        return m_tzNewAccuracy;
    }

    // MacAddresses getters
    string GetMacEcm() {
        std::unique_lock<std::mutex> lock(m_mutex);
        return m_macEcm;
    }
    string GetMacEstb() {
        std::unique_lock<std::mutex> lock(m_mutex);
        return m_macEstb;
    }
    string GetMacMoca() {
        std::unique_lock<std::mutex> lock(m_mutex);
        return m_macMoca;
    }
    string GetMacEth() {
        std::unique_lock<std::mutex> lock(m_mutex);
        return m_macEth;
    }
    string GetMacWifi() {
        std::unique_lock<std::mutex> lock(m_mutex);
        return m_macWifi;
    }
    string GetMacBluetooth() {
        std::unique_lock<std::mutex> lock(m_mutex);
        return m_macBluetooth;
    }
    string GetMacRf4ce() {
        std::unique_lock<std::mutex> lock(m_mutex);
        return m_macRf4ce;
    }
    string GetMacInfo() {
        std::unique_lock<std::mutex> lock(m_mutex);
        return m_macInfo;
    }
    bool GetMacSuccess() {
        std::unique_lock<std::mutex> lock(m_mutex);
        return m_macSuccess;
    }
};

/* Systemservice L2 test class declaration */
class SystemService_L2Test : public L2TestMocks {
protected:
    IARM_EventHandler_t systemStateChanged = nullptr;
    IARM_EventHandler_t sysMgrEventHandler = nullptr;
    IARM_BusCall_t sysModeChangeHandler = nullptr;
    IARM_EventHandler_t pwrMgrEventHandler = nullptr;
    IARM_EventHandler_t sysMgrDeviceHandler = nullptr;
    IARM_EventHandler_t timerStatusEventHandler = nullptr;

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

