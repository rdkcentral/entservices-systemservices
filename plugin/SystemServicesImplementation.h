/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
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
*/

#pragma once

#include "Module.h"
#include <interfaces/Ids.h>
#include <interfaces/ISystemServices.h>

#include <com/com.h>
#include <core/core.h>

#include <memory>
#include <stdint.h>
#include <thread>
#include <regex.h>
#include <cctype>
#include <fstream>
#include <cstring>
using std::ofstream;
#include <cstdlib>
#include <iostream>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <mutex>

#include "Module.h"
#include "tracing/Logging.h"
#include "UtilsThreadRAII.h"
#include "SystemServicesHelper.h"
#include "platformcaps/platformcaps.h"
#if defined(USE_IARMBUS) || defined(USE_IARM_BUS)
#include "libIARM.h"
#include "host.hpp"
#include "sleepMode.hpp"
#endif /* USE_IARMBUS || USE_IARM_BUS */

#include "sysMgr.h"
#include "cSettings.h"
#include "cTimer.h"
#include "rfcapi.h"
#include "uploadlogs.h"
#include <interfaces/IPowerManager.h>
#include <interfaces/IDeviceInfo.h>
#include <interfaces/IFirmwareUpdate.h>
#include <interfaces/IMigration.h>
#include <interfaces/ITelemetry.h>
#include <interfaces/IConfiguration.h>
#include <core/core.h>
#include <core/JSON.h>
#include "mfrMgr.h"

#include "PowerManagerInterface.h"

using namespace WPEFramework::Exchange;
using PowerState = WPEFramework::Exchange::IPowerManager::PowerState;
using WakeupReason = WPEFramework::Exchange::IPowerManager::WakeupReason;
using ThermalTemperature = WPEFramework::Exchange::IPowerManager::ThermalTemperature;

#define TERRITORYFILE "/opt/secure/persistent/System/Territory.txt"

namespace WPEFramework
{
    namespace Plugin
    {
        class SystemServicesImplementation : public Exchange::ISystemServices, public Exchange::IConfiguration
        {
            private:
                class PowerManagerNotification : public Exchange::IPowerManager::INetworkStandbyModeChangedNotification,
                                                     public Exchange::IPowerManager::IThermalModeChangedNotification,
                                                     public Exchange::IPowerManager::IRebootNotification,
                                                     public Exchange::IPowerManager::IModeChangedNotification {
                private:
                    PowerManagerNotification(const PowerManagerNotification&) = delete;
                    PowerManagerNotification& operator=(const PowerManagerNotification&) = delete;

                public:
                    explicit PowerManagerNotification(SystemServicesImplementation& parent)
                        : _parent(parent)
                    {
                    }
                    ~PowerManagerNotification() override = default;

                public:
                    void OnPowerModeChanged(const PowerState currentState, const PowerState newState) override
                    {
                        _parent.OnPowerModeChanged(currentState, newState);
                    }
                    void OnNetworkStandbyModeChanged(const bool enabled) override
                    {
                        _parent.OnNetworkStandbyModeChanged(enabled);
                    }
                    void OnThermalModeChanged(const ThermalTemperature currentThermalLevel, const ThermalTemperature newThermalLevel, const float currentTemperature) override
                    {
                        _parent.OnThermalModeChanged(currentThermalLevel, newThermalLevel, currentTemperature);
                    }
                    void OnRebootBegin(const string &rebootReasonCustom, const string &rebootReasonOther, const string &rebootRequestor) override
                    {
                        _parent.OnRebootBegin(rebootReasonCustom, rebootReasonOther, rebootRequestor);
                    }

                    template <typename T>
                    T* baseInterface()
                    {
                        static_assert(std::is_base_of<T, PowerManagerNotification>(), "base type mismatch");
                        return static_cast<T*>(this);
                    }

                    BEGIN_INTERFACE_MAP(PowerManagerNotification)
                    INTERFACE_ENTRY(Exchange::IPowerManager::INetworkStandbyModeChangedNotification)
                    INTERFACE_ENTRY(Exchange::IPowerManager::IThermalModeChangedNotification)
                    INTERFACE_ENTRY(Exchange::IPowerManager::IRebootNotification)
                    INTERFACE_ENTRY(Exchange::IPowerManager::IModeChangedNotification)
                    END_INTERFACE_MAP

                    private:
                    SystemServicesImplementation& _parent;
                };

            public:
                // We do not allow this plugin to be copied !!
                SystemServicesImplementation();
                ~SystemServicesImplementation() override;

                // We do not allow this plugin to be copied !!
                SystemServicesImplementation(const SystemServicesImplementation&) = delete;
                SystemServicesImplementation& operator=(const SystemServicesImplementation&) = delete;

                BEGIN_INTERFACE_MAP(SystemServicesImplementation)
                INTERFACE_ENTRY(Exchange::ISystemServices)
                INTERFACE_ENTRY(Exchange::IConfiguration)
                END_INTERFACE_MAP

            public:
                enum Event
                {
                    SYSTEMSERVICES_EVT_ONFIRMWAREUPDATEINFORECEIVED
                    , SYSTEMSERVICES_EVT_ONREBOOTREQUEST
                    , SYSTEMSERVICES_EVT_ONSYSTEMPOWERSTATECHANGED
                    , SYSTEMSERVICES_EVT_ONTERRITORYCHANGED
                    , SYSTEMSERVICES_EVT_ONTIMEZONEDSTCHANGED
                    , SYSTEMSERVICES_EVT_ONMACADDRESSRETRIEVED
                    , SYSTEMSERVICES_EVT_ONSYSTEMMODECHANGED
                    , SYSTEMSERVICES_EVT_ONLOGUPLOAD
		            , SYSTEMSERVICES_EVT_ONFRIENDLYNAME_CHANGED
                    , SYSTEMSERVICES_EVT_ONNETWORKSTANDBYMODECHANGED
                    , SYSTEMSERVICES_EVT_ONBLOCKLISTCHANGED
                    , SYSTEMSERVICES_EVT_ONFIRMWAREUPDATESTATECHANGED
                    , SYSTEMSERVICES_EVT_ONTEMPERATURETHRESHOLDCHANGED
                    , SYSTEMSERVICES_EVT_ON_SYSTEM_CLOCK_SET
                    , SYSTEMSERVICES_EVT_ONFWPENDINGREBOOT
                    , SYSTEMSERVICES_EVT_ONDEVICEMGTUPDATERECEIVED
#ifdef ENABLE_SYSTIMEMGR_SUPPORT
                    , SYSTEMSERVICES_EVT_ONTIMESTATUSCHANGED
#endif// ENABLE_SYSTIMEMGR_SUPPORT
                };
 
            class EXTERNAL Job : public Core::IDispatch {
            protected:
                Job(SystemServicesImplementation* systemServicesImplementation, Event event, JsonObject &params)
                    : _systemServicesImplementation(systemServicesImplementation)
                    , _event(event)
                    , _params(params) {
                    if (_systemServicesImplementation != nullptr) {
                        _systemServicesImplementation->AddRef();
                    }
                }

            public:
                Job() = delete;
                Job(const Job&) = delete;
                Job& operator=(const Job&) = delete;
                ~Job() {
                    if (_systemServicesImplementation != nullptr) {
                        _systemServicesImplementation->Release();
                    }
                }

            public:
                static Core::ProxyType<Core::IDispatch> Create(SystemServicesImplementation* systemServicesImplementation, Event event, JsonObject  params) {
#ifndef USE_THUNDER_R4
                    return (Core::proxy_cast<Core::IDispatch>(Core::ProxyType<Job>::Create(systemServicesImplementation, event, params)));
#else
                    return (Core::ProxyType<Core::IDispatch>(Core::ProxyType<Job>::Create(systemServicesImplementation, event, params)));
#endif
                }

                virtual void Dispatch() {
                    _systemServicesImplementation->Dispatch(_event, _params);
                }

            private:
                SystemServicesImplementation *_systemServicesImplementation;
                const Event _event;
                JsonObject _params;
        };
        public:
            virtual Core::hresult Register(Exchange::ISystemServices::INotification *notification) override ;
            virtual Core::hresult Unregister(Exchange::ISystemServices::INotification *notification) override;

            Core::hresult GetDeviceInfo(IStringIterator* const& params, DeviceInfo& deviceInfo) override;
            Core::hresult GetDownloadedFirmwareInfo(DownloadedFirmwareInfo& downloadedFirmwareInfo) override;
            Core::hresult GetFirmwareDownloadPercent(int32_t & downloadPercent, bool& success) override;
            Core::hresult GetFirmwareUpdateInfo(const string& GUID, bool &asyncResponse, bool& success) override;
            Core::hresult GetFirmwareUpdateState(int& firmwareUpdateState, bool& success) override;
            Core::hresult GetLastFirmwareFailureReason(string& failReason, bool& success) override;
#ifdef ENABLE_DEVICE_MANUFACTURER_INFO
            Core::hresult GetMfgSerialNumber(string& mfgSerialNumber, bool& success) override;
#endif /* ENABLE_DEVICE_MANUFACTURER_INFO */
            Core::hresult GetNetworkStandbyMode(bool& nwStandby, bool& success) override;
#if defined(HAS_API_SYSTEM) && defined(HAS_API_POWERSTATE)
            Core::hresult GetPowerState(string& powerState, bool& success) override;
            Core::hresult SetPowerState(const string &powerState, const string &standbyReason, uint32_t& SysSrv_Status, string& errorMessage, bool& success) override;
#endif /* HAS_API_SYSTEM && HAS_API_POWERSTATE */
            Core::hresult GetPowerStateBeforeReboot(string& state, bool& success) override;
            Core::hresult GetRFCConfig(IStringIterator* const& rfcList, IStringIterator*& RFCConfig, uint32_t& SysSrv_Status, string& errorMessage, bool& success) override;
            Core::hresult GetSerialNumber(string& serialNumber, bool& success) override;
            Core::hresult GetFriendlyName(string& friendlyName, bool& success) override;
            Core::hresult GetTerritory(string& territory , string& region, bool& success) override;
            Core::hresult GetTimeZones(IStringIterator* const& timeZones, string& zoneinfo) override;
            Core::hresult GetTimeZoneDST(string& timeZone, string& accuracy, bool& success) override;
#ifdef ENABLE_DEEP_SLEEP
            Core::hresult GetWakeupReason(string& wakeupReason, bool& success) override;
            Core::hresult GetLastWakeupKeyCode(int& wakeupKeyCode, bool& success) override;
            std::string getWakeupReasonString(WakeupReason reason);
#endif /* ENABLE_DEEP_SLEEP */
            Core::hresult IsOptOutTelemetry(bool& OptOut, bool& success) override;
            Core::hresult Reboot(const string& rebootReason, int& IARM_Bus_Call_STATUS, bool& success) override;
            Core::hresult RequestSystemUptime(string& systemUptime, bool& success) override;
            Core::hresult SetDeepSleepTimer(const int seconds, uint32_t& SysSrv_Status, string& errorMessage, bool& success) override;
            Core::hresult SetFirmwareAutoReboot(const bool enable, SystemResult& result) override;
            Core::hresult SetNetworkStandbyMode(const bool nwStandby, SystemResult& result) override;
            Core::hresult SetOptOutTelemetry(const bool OptOut, SystemResult& result) override;
            Core::hresult SetFriendlyName(const string& friendlyName, SystemResult& result) override;
            Core::hresult SetBootLoaderSplashScreen(const string& path, ErrorInfo& error, bool& success) override;
            Core::hresult SetTerritory(const string& territory, const string& region, SystemError& error, bool& success) override;
            Core::hresult SetTimeZoneDST(const string& timeZone, const string& accuracy, uint32_t& SysSrv_Status, string& errorMessage, bool& success) override;
            Core::hresult UpdateFirmware(SystemResult& result) override;
            Core::hresult GetBootTypeInfo(BootType &bootInfo) override;
            Core::hresult SetMigrationStatus(const string& status, bool& success) override;
            Core::hresult GetMigrationStatus(MigrationStatus &migrationInfo) override;
            Core::hresult GetMacAddresses(const string& GUID, bool &asyncResponse, uint32_t& SysSrv_Status, string& errorMessage, bool& success) override;
            Core::hresult GetPlatformConfiguration (const string &query, PlatformConfig& platformConfig) override;
            Core::hresult SetWakeupSrcConfiguration(const string& powerState, ISystemServicesWakeupSourcesIterator* const& wakeupSources, SystemResult& result) override;
            Core::hresult GetSystemVersions(SystemVersionsInfo& systemVersionsInfo) override;
            Core::hresult SetMode(const ModeInfo& modeInfo, uint32_t& SysSrv_Status, string& errorMessage, bool& success) override;
            Core::hresult UploadLogsAsync(SystemResult& result) override;
            Core::hresult AbortLogUpload(SystemResult& result) override;
            Core::hresult SetFSRFlag(const bool fsrFlag, SystemResult& result) override;
            Core::hresult GetFSRFlag(bool &fsrFlag, bool& success) override;
            Core::hresult SetBlocklistFlag(const bool blocklist, SetBlocklistResult& result) override;
            Core::hresult GetBlocklistFlag(BlocklistResult& result) override;
            Core::hresult GetBuildType(string& buildType, bool& success) override;

            // IConfiguration interface
            uint32_t Configure(PluginHost::IShell* service) override;

#ifdef ENABLE_SYSTIMEMGR_SUPPORT
            void OnTimeStatusChanged(string timequality,string timesource, string utctime);
            Core::hresult GetTimeStatus(string& TimeQuality, string& TimeSrc, string& Time, bool& success) override;
#endif// ENABLE_SYSTIMEMGR_SUPPORT
            IPowerManager* getPwrMgrPluginInstance();
            void OnFirmwareUpdateStateChange(int state);
            void OnTemperatureThresholdChanged(string thresholdType, bool exceed, float temperature);
            void OnLogUpload(int newState);
            void OnClockSet();
            void OnFirmwarePendingReboot(int seconds); /* Event handler for Pending Reboot */
            void OnDeviceMgtUpdateReceived(IARM_BUS_SYSMGR_DeviceMgtUpdateInfo_Param_t *config);
            void OnSystemModeChanged(string mode);
            static void firmwareUpdateInfoReceived(void);
            void reportFirmwareUpdateInfoReceived(string firmwareUpdateVersion,
                        int httpStatus, bool success, string firmwareVersion, string responseString);

        private:
            mutable Core::CriticalSection _adminLock;
            PluginHost::IShell* _service;
            std::list<Exchange::ISystemServices::INotification*> _systemServicesNotification;

#ifdef ENABLE_DEVICE_MANUFACTURER_INFO
            std::string m_MfgSerialNumber;
            bool m_MfgSerialNumberValid;
            std::string m_ManufacturerDataHardwareID;
            bool m_ManufacturerDataHardwareIdValid;
#endif
            pid_t m_uploadLogsPid;
            std::mutex m_uploadLogsMutex;
            std::mutex m_territoryMutex;
            std::string m_friendlyName;
            bool m_networkStandbyModeValid;
            bool m_networkStandbyMode;
            string m_stbVersionString;
            static cSettings m_temp_settings;
            PowerManagerInterfaceRef _powerManagerPlugin;
            Core::Sink<PowerManagerNotification> _pwrMgrNotification;
            bool _registeredEventHandlers;
            Utils::ThreadRAII thread_getMacAddresses;
            static const string MODEL_NAME;
            static const string HARDWARE_ID;
            static const string FRIENDLY_ID;
            enum class FWUpdateAvailableEnum { FW_UPDATE_AVAILABLE, FW_MATCH_CURRENT_VER, NO_FW_VERSION, EMPTY_SW_UPDATE_CONF };
            static std::string m_currentMode;
            std::string m_current_state;
            static cTimer m_operatingModeTimer;
            static int m_remainingDuration;
            Utils::ThreadRAII m_getFirmwareInfoThread;
            PluginHost::IShell* m_shellService { nullptr };
            regex_t m_regexUnallowedChars;
            int m_FwUpdateState_LatestEvent;
            std::string m_powerStateBeforeReboot;
            bool m_powerStateBeforeRebootValid;
            std::string m_strTerritory;
            std::string m_strRegion;
            std::string  m_strStandardTerritoryList;

            void dispatchEvent(Event, const JsonObject &params);
            void Dispatch(Event event, const JsonObject &params);

            static void getMacAddressesAsync(SystemServicesImplementation *p);
            static void startModeTimer(int duration);
            static void stopModeTimer();
            static void updateDuration();
            bool processTimeZones(std::string entry, JsonObject& out);
            void InitializePowerManager();
            void registerEventHandlers();
            void OnPowerModeChanged(const PowerState currentState, const PowerState newState);
            std::string powerModeEnumToString(PowerState state);
            void OnNetworkStandbyModeChanged(const bool enabled);
            void OnThermalModeChanged(const ThermalTemperature currentThermalLevel, const ThermalTemperature newThermalLevel, const float currentTemperature);
            void OnRebootBegin(const string &rebootReasonCustom, const string &rebootReasonOther, const string &rebootRequestor);
            std::string getStbVersionString();
            std::string getClientVersionString();
            std::string getStbTimestampString();
            std::string getStbBranchString();
            bool makePersistentDir();
            std::string safeExtractAfterColon(const std::string& inputLine);
            bool readTerritoryFromFile();
            bool isStrAlphaUpper(string strVal);
            bool isRegionValid(string regionStr);
            uint32_t writeTerritory(string territory, string region);
            bool setPowerStateConversion(std::string powerState);

#if defined(USE_IARMBUS) || defined(USE_IARM_BUS)
            void InitializeIARM();
            void DeinitializeIARM();
#endif /* defined(USE_IARMBUS) || defined(USE_IARM_BUS) */

            /* Events : Begin */
            void OnFirmwareUpdateInfoRecieved(string CallGUID);
            void OnSystemPowerStateChanged(string currentPowerState, string powerState);
            void OnPwrMgrReboot(string requestedApp, string rebootReason);
            void OnRebootRequest(string reason);
            void OnTerritoryChanged(string oldTerritory, string newTerritory, string oldRegion="", string newRegion="");
            void OnTimeZoneDSTChanged(string oldTimeZone, string newTimeZone, string oldAccuracy, string newAccuracy);
            void OnBlocklistChanged(bool newBlocklistFlag, bool oldBlocklistFlag);
            /* Events : End */

        public:
            static SystemServicesImplementation* _instance;

            friend class Job;
        };
    } // namespace Plugin
} // namespace WPEFramework
