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
#include <interfaces/IPowerManager.h>
#include <core/core.h>
#include <core/JSON.h>
#include "mfrMgr.h"

#include "PowerManagerInterface.h"

using namespace WPEFramework::Exchange;
using PowerState = WPEFramework::Exchange::IPowerManager::PowerState;
using WakeupReason = WPEFramework::Exchange::IPowerManager::WakeupReason;
using ThermalTemperature = WPEFramework::Exchange::IPowerManager::ThermalTemperature;

namespace WPEFramework
{
    namespace Plugin
    {
        class SystemServicesImplementation : public Exchange::ISystemServices
        {
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
                        _parent.onPowerModeChanged(currentState, newState);
                    }
                    void OnNetworkStandbyModeChanged(const bool enabled) override
                    {
                        _parent.onNetworkStandbyModeChanged(enabled);
                    }
                    void OnThermalModeChanged(const ThermalTemperature currentThermalLevel, const ThermalTemperature newThermalLevel, const float currentTemperature) override
                    {
                        _parent.onThermalModeChanged(currentThermalLevel, newThermalLevel, currentTemperature);
                    }
                    void OnRebootBegin(const string &rebootReasonCustom, const string &rebootReasonOther, const string &rebootRequestor) override
                    {
                        _parent.onRebootBegin(rebootReasonCustom, rebootReasonOther, rebootRequestor);
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
#ifdef ENABLE_SYSTIMEMGR_SUPPORT
                    , SYSTEMSERVICES_EVT_ONTIMESTATUSCHANGED
#endif// ENABLE_SYSTIMEMGR_SUPPORT
                };
 
            class EXTERNAL Job : public Core::IDispatch {
            protected:
                Job(SystemServicesImplementation* systemServicesImplementation, Event event, JsonValue &params)
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
                static Core::ProxyType<Core::IDispatch> Create(SystemServicesImplementation* systemServicesImplementation, Event event, JsonValue  params) {
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
                JsonValue _params;
        };
        public:
            virtual Core::hresult Register(Exchange::ISystemServices::INotification *notification) override ;
            virtual Core::hresult Unregister(Exchange::ISystemServices::INotification *notification) override;

            Core::hresult GetDeviceInfo(IStringIterator* const& params, DeviceInfo& deviceInfo) override;
            Core::hresult GetDownloadedFirmwareInfo(DownloadedFirmwareInfo& downloadedFirmwareInfo) override;
            Core::hresult GetFirmwareDownloadPercent(uint32_t & downloadPercent, bool& success) override;
            Core::hresult GetFirmwareUpdateInfo(const string& GUID, bool &asyncResponse, bool& success) override;
            Core::hresult GetFirmwareUpdateState(int& firmwareUpdateState, bool& success) override;
            Core::hresult GetLastFirmwareFailureReason(string& failReason, bool& success) override;
#ifdef ENABLE_DEVICE_MANUFACTURER_INFO
            Core::hresult GetMfgSerialNumber(string& mfgSerialNumber, bool& success) override;
#endif /* ENABLE_DEVICE_MANUFACTURER_INFO */
            Core::hresult GetNetworkStandbyMode(bool& nwStandby, bool& success) override;
#if defined(HAS_API_SYSTEM) && defined(HAS_API_POWERSTATE)
            Core::hresult GetPowerState(string& powerState, bool& success) override;
            Core::hresult SetPowerState(const string &powerState, const string &standbyReason, SystemServicesSuccess& success) override;
#endif /* HAS_API_SYSTEM && HAS_API_POWERSTATE */
            Core::hresult GetPowerStateBeforeReboot(string& state, bool& success) override;
            Core::hresult GetRFCConfig(IStringIterator* const& rfcList, IStringIterator*& RFCConfig, bool& success) override;
            Core::hresult GetSerialNumber(string& serialNumber, bool& success) override;
            Core::hresult GetFriendlyName(string& friendlyName, bool& success) override;
            Core::hresult GetTerritory(string& territory , string& region, bool& success) override;
            Core::hresult GetTimeZones(TimeZoneInfo& timeZoneInfo) override;
            Core::hresult GetTimeZoneDST(string& timeZone, string& accuracy, bool& success) override;
#ifdef ENABLE_DEEP_SLEEP
            Core::hresult GetWakeupReason(string& WakeupReason, bool& success) override;
            Core::hresult GetLastWakeupKeyCode(string& wakeupKeyCode, bool& success) override;
#endif /* ENABLE_DEEP_SLEEP */
            Core::hresult IsOptOutTelemetry(bool& OptOut, bool& success) override;
            Core::hresult Reboot(const string& rebootReason, int& IARM_Bus_Call_STATUS, bool& success) override;
            Core::hresult SetDeepSleepTimer(const int seconds, SystemServicesSuccess& success) override;
            Core::hresult SetFirmwareAutoReboot(const bool enable, SystemServicesSuccess& success) override;
            Core::hresult SetNetworkStandbyMode(const bool nwStandby, SystemServicesSuccess& success) override;
            Core::hresult SetOptOutTelemetry(const bool OptOut, SystemServicesSuccess& success) override;
            Core::hresult SetFriendlyName(const string& friendlyName, SystemServicesSuccess& success) override;
            Core::hresult SetBootLoaderSplashScreen(const string& path, ErrorInfo& error, bool& success) override;
            Core::hresult SetTerritory(const string& territory, const string& region, SystemError& error, bool& success) override;
            Core::hresult SetTimeZoneDST(const string& timeZone, const string& accuracy, SystemServicesSuccess& success) override;
            Core::hresult UpdateFirmware(SystemServicesSuccess& success) override;
            Core::hresult GetBootTypeInfo(string &bootType, bool& success) override;
            Core::hresult SetMigrationStatus(const bool status, bool& success) override;
            Core::hresult GetMigrationStatus(string &MigrationStatus, bool& success) override;
            Core::hresult GetMacAddresses(const string& GUID, bool &asyncResponse, bool& success) override;
            Core::hresult GetPlatformConfiguration (PlatformConfig& platformConfig) override;
            Core::hresult SetWakeupSrcConfiguration(const string& powerState, ISystemServicesWakeupSourcesIterator* const& wakeupSources, SystemServicesSuccess& success) override;
            Core::hresult GetSystemVersions(SystemVersionsInfo& systemVersionsInfo) override;
            Core::hresult RequestSystemUptime(string& systemUptime, bool& success) override;
            Core::hresult SetMode(const ModeInfo& modeinfo, SystemServicesSuccess& success) override;
            Core::hresult UploadLogsAsync(SystemServicesSuccess& success) override;
            Core::hresult AbortLogUpload(SystemServicesSuccess& success) override;
            Core::hresult SetFSRFlag(const bool fsrFlag, bool& success) override;
            Core::hresult GetFSRFlag(bool &fsrFlag, bool& success) override;
            Core::hresult SetBlocklistFlag(const bool blocklist, BlocklistResult& result) override;
            Core::hresult GetBlocklistFlag(BlocklistResult& result) override;
            Core::hresult GetBuildType(string& buildType, bool& success) override;
            
#if defined(USE_IARMBUS) || defined(USE_IARM_BUS)
            void InitializeIARM();
            void DeinitializeIARM();
#endif /* defined(USE_IARMBUS) || defined(USE_IARM_BUS) */

#ifdef ENABLE_SYSTIMEMGR_SUPPORT
            void onTimeStatusChanged(string timequality,string timesource, string utctime);
            Core::hresult GetTimeStatus(string& TimeQuality, string& TimeSrc, string& Time, bool& success) override;
#endif// ENABLE_SYSTIMEMGR_SUPPORT

        private:
            mutable Core::CriticalSection _adminLock;
            PluginHost::IShell* _service;
            std::list<Exchange::ISystemServices::INotification*> _SystemServicesNotification;

#ifdef ENABLE_DEVICE_MANUFACTURER_INFO
            std::string m_MfgSerialNumber;
            bool m_MfgSerialNumberValid;
#endif

            void dispatchEvent(Event, const JsonValue &params);
            void Dispatch(Event event, const JsonValue params);
        public:
            static SystemServicesImplementation* _instance;

            friend class Job;
        };
    } // namespace Plugin
} // namespace WPEFramework
