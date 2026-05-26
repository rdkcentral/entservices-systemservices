/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
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
*/

#pragma once

#include "Module.h"
#include <interfaces/ISystemServices.h>
#include <interfaces/json/JSystemServices.h>
#include <interfaces/json/JsonData_SystemServices.h>
#include <interfaces/IConfiguration.h>
#include "UtilsLogging.h"
#include "tracing/Logging.h"

namespace WPEFramework 
{
    namespace Plugin
    {
        class SystemServices : public PluginHost::IPlugin, public PluginHost::JSONRPC 
        {
            private:
                class Notification : public RPC::IRemoteConnection::INotification, public Exchange::ISystemServices::INotification
                {
                    private:
                        Notification() = delete;
                        Notification(const Notification&) = delete;
                        Notification& operator=(const Notification&) = delete;

                    public:
                    explicit Notification(SystemServices* parent) 
                        : _parent(*parent)
                        {
                            ASSERT(parent != nullptr);
                        }

                        virtual ~Notification()
                        {
                        }

                        BEGIN_INTERFACE_MAP(Notification)
                        INTERFACE_ENTRY(Exchange::ISystemServices::INotification)
                        INTERFACE_ENTRY(RPC::IRemoteConnection::INotification)
                        END_INTERFACE_MAP

                        void Activated(RPC::IRemoteConnection*) override
                        {
                           
                        }

                        void Deactivated(RPC::IRemoteConnection *connection) override
                        {
                            _parent.Deactivated(connection);
                        }

                        void OnFirmwareUpdateInfoReceived(const Exchange::ISystemServices::FirmwareUpdateInfo &firmwareUpdateInfo) override
                        {
                            LOGINFO("FirmwareUpdateInfo");
                            Exchange::JSystemServices::Event::OnFirmwareUpdateInfoReceived(_parent, firmwareUpdateInfo);
                        }

                        void OnRebootRequest(const string& requestedApp, const string& rebootReason) override
                        {
                            LOGINFO("requestedApp:%s, rebootReason:%s", requestedApp.c_str(), rebootReason.c_str());
                            Exchange::JSystemServices::Event::OnRebootRequest(_parent, requestedApp, rebootReason);
                        }

                        void OnSystemPowerStateChanged(const string& powerState, const string& currentPowerState) override
                        {
                            LOGINFO("powerState:%s, currentPowerState:%s", powerState.c_str(), currentPowerState.c_str());
                            Exchange::JSystemServices::Event::OnSystemPowerStateChanged(_parent, powerState, currentPowerState);
                        }

                        void OnTerritoryChanged(const Exchange::ISystemServices::TerritoryChangedInfo &territoryChangedInfo) override
                        {
                            LOGINFO("territoryChangedInfo");
                            Exchange::JSystemServices::Event::OnTerritoryChanged(_parent, territoryChangedInfo);
                        }

                        void OnTimeZoneDSTChanged(const Exchange::ISystemServices::TimeZoneDSTChangedInfo& timeZoneDSTChangedInfo) override
                        {
                            LOGINFO("timeZoneDSTChangedInfo");
                            Exchange::JSystemServices::Event::OnTimeZoneDSTChanged(_parent, timeZoneDSTChangedInfo);
                        }

                        void OnMacAddressesRetreived(const Exchange::ISystemServices::MacAddressesInfo& macAddressesInfo) override
                        {
                            LOGINFO("macAddressesInfo");
                            Exchange::JSystemServices::Event::OnMacAddressesRetreived(_parent, macAddressesInfo);
                        }

                        void OnSystemModeChanged(const string& mode) override
                        {
                            LOGINFO("mode:%s", mode.c_str());
                            Exchange::JSystemServices::Event::OnSystemModeChanged(_parent, mode);
                        }

                        void OnLogUpload(const string& logUploadStatus) override
                        {
                            LOGINFO("mode:%s", logUploadStatus.c_str());
                            Exchange::JSystemServices::Event::OnLogUpload(_parent, logUploadStatus);
                        }
                        
                        void OnFirmwareUpdateStateChanged(const int firmwareUpdateStateChange) override
                        {
                            LOGINFO("firmwareUpdateStateChange:%d", firmwareUpdateStateChange);
                            Exchange::JSystemServices::Event::OnFirmwareUpdateStateChanged(_parent, firmwareUpdateStateChange);
                        }
                        
                        void OnTemperatureThresholdChanged(const string& thresholdType, const bool exceeded, const string& temperature) override
                        {
                            LOGINFO("thresholdType:%s exceeded:%d temperature:%s", thresholdType.c_str(), exceeded, temperature.c_str());
                            Exchange::JSystemServices::Event::OnTemperatureThresholdChanged(_parent, thresholdType, exceeded, temperature);
                        }
                        
                        void OnSystemClockSet() override
                        {
                            LOGINFO("OnSystemClockSet");
                            Exchange::JSystemServices::Event::OnSystemClockSet(_parent);
                        }
                        
                        void OnFirmwarePendingReboot(const int fireFirmwarePendingReboot) override
                        {
                            LOGINFO("fireFirmwarePendingReboot:%d", fireFirmwarePendingReboot);
                            Exchange::JSystemServices::Event::OnFirmwarePendingReboot(_parent, fireFirmwarePendingReboot);
                        }
                        
                        void OnFriendlyNameChanged(const string& friendlyName) override
                        {
                            LOGINFO("friendlyName:%s", friendlyName.c_str());
                            Exchange::JSystemServices::Event::OnFriendlyNameChanged(_parent, friendlyName);
                        }
                        
                        void OnDeviceMgtUpdateReceived(const string& source, const string& type, const bool success) override
                        {
                            LOGINFO("source:%s type:%s success:%d", source.c_str(), type.c_str(), success);
                            Exchange::JSystemServices::Event::OnDeviceMgtUpdateReceived(_parent, source, type, success);
                        }
                        
                        void OnBlocklistChanged(const string& oldBlocklistFlag, const string& newBlocklistFlag) override
                        {
                            LOGINFO("oldBlocklistFlag:%s newBlocklistFlag:%s", oldBlocklistFlag.c_str(), newBlocklistFlag.c_str());
                            Exchange::JSystemServices::Event::OnBlocklistChanged(_parent, oldBlocklistFlag, newBlocklistFlag);
                        }
                        
                        void OnTimeStatusChanged(const string& TimeQuality, const string& TimeSrc, const string& Time) override
                        {
                            LOGINFO("TimeQuality:%s TimeSrc:%s Time:%s", TimeQuality.c_str(), TimeSrc.c_str(), Time.c_str());
                            Exchange::JSystemServices::Event::OnTimeStatusChanged(_parent, TimeQuality, TimeSrc, Time);
                        }
                        
                        void OnNetworkStandbyModeChanged(const bool nwStandby) override
                        {
                            LOGINFO("nwStandby:%d", nwStandby);
                            Exchange::JSystemServices::Event::OnNetworkStandbyModeChanged(_parent, nwStandby);
                        }

                    private:
                        SystemServices& _parent;
                };

                public:
                    SystemServices(const SystemServices&) = delete;
                    SystemServices& operator=(const SystemServices&) = delete;

                    SystemServices();
                    virtual ~SystemServices();

                    BEGIN_INTERFACE_MAP(SystemServices)
                    INTERFACE_ENTRY(PluginHost::IPlugin)
                    INTERFACE_ENTRY(PluginHost::IDispatcher)
                    INTERFACE_AGGREGATE(Exchange::ISystemServices, _systemServices)
                    END_INTERFACE_MAP

                    //  IPlugin methods
                    // -------------------------------------------------------------------------------------------------------
                    const string Initialize(PluginHost::IShell* service) override;
                    void Deinitialize(PluginHost::IShell* service) override;
                    string Information() const override;

                private:
                    void Deactivated(RPC::IRemoteConnection* connection);

                private:
                    PluginHost::IShell* _service{};
                    uint32_t _connectionId{};
                    Exchange::ISystemServices* _systemServices{};
                    Core::Sink<Notification> _systemServicesNotification;
                    Exchange::IConfiguration* _configure{};
       };
    } // namespace Plugin
} // namespace WPEFramework
