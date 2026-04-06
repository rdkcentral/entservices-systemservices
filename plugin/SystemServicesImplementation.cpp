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

#include <stdlib.h>
#include <errno.h>
#include <cstdio>
#include <regex>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <ctime>
#include <iomanip>
#include <bits/stdc++.h>
#include <algorithm>
#include <curl/curl.h>
#include "StateObserverHelper.h"
#include "uploadlogs.h"
#include "secure_wrapper.h"
#include <core/core.h>
#include <core/JSON.h>
#include<interfaces/entservices_errorcodes.h>

#include "SystemServicesImplementation.h"
#include <telemetry_busmessage_sender.h>

#if defined(USE_IARMBUS) || defined(USE_IARM_BUS)
#include "libIBusDaemon.h"
#include "UtilsIarm.h"
#endif /* USE_IARMBUS || USE_IARM_BUS */

#ifdef ENABLE_SYSTIMEMGR_SUPPORT
#include "systimerifc/itimermsg.h"
#endif// ENABLE_SYSTIMEMGR_SUPPORT

#ifdef ENABLE_THERMAL_PROTECTION
#include "thermonitor.h"
#endif /* ENABLE_THERMAL_PROTECTION */

#if defined(HAS_API_SYSTEM) && defined(HAS_API_POWERSTATE)
#include "libIBus.h"
#endif /* HAS_API_SYSTEM && HAS_API_POWERSTATE */

#include "mfrMgr.h"

#ifdef ENABLE_DEEP_SLEEP
#include "rdk/halif/deepsleep-manager/deepSleepMgr.h"
#endif

#include "UtilsCStr.h"
#include "UtilsIarm.h"
#include "UtilsJsonRpc.h"
#include "UtilsString.h"
#include "UtilsfileExists.h"
#include "UtilsgetFileContent.h"
#include "UtilsProcess.h"

using namespace std;
using namespace WPEFramework;
using PowerState = WPEFramework::Exchange::IPowerManager::PowerState;
using WakeupReason = WPEFramework::Exchange::IPowerManager::WakeupReason;
using ThermalTemperature = WPEFramework::Exchange::IPowerManager::ThermalTemperature;
using WakeupSrcType             = WPEFramework::Exchange::IPowerManager::WakeupSrcType;
using WakeupSrcConfig           = WPEFramework::Exchange::IPowerManager::WakeupSourceConfig;
using IWakeupSourceConfigIterator  = WPEFramework::Exchange::IPowerManager::IWakeupSourceConfigIterator;
using WakeupSourceConfigIteratorImpl = WPEFramework::Core::Service<WPEFramework::RPC::IteratorType<IWakeupSourceConfigIterator>>;

#define MAX_REBOOT_DELAY 86400 /* 24Hr = 86400 sec */
#define TR181_FW_DELAY_REBOOT "Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.AutoReboot.fwDelayReboot"
#define TR181_AUTOREBOOT_ENABLE "Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.AutoReboot.Enable"

#define RFC_PWRMGR2 "Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.Power.PwrMgr2.Enable"

#define ZONEINFO_DIR "/usr/share/zoneinfo"
#define LOCALTIME_FILE "/opt/persistent/localtime"

#define DEVICE_PROPERTIES_FILE "/etc/device.properties"

#define STATUS_CODE_NO_SWUPDATE_CONF 460 

#define OPTOUT_TELEMETRY_STATUS "/opt/tmtryoptout"

#define REGEX_UNALLOWABLE_INPUT "[^[:alnum:]_-]{1}"

#define STORE_DEMO_FILE "/opt/persistent/store-mode-video/videoFile.mp4"
#define STORE_DEMO_LINK "file:///opt/persistent/store-mode-video/videoFile.mp4"

#define RFC_LOG_UPLOAD "Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.LogUploadBeforeDeepSleep.Enable"
#define TR181_SYSTEM_FRIENDLY_NAME "Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.SystemServices.FriendlyName"

#define LOG_UPLOAD_STATUS_SUCCESS "UPLOAD_SUCCESS"
#define LOG_UPLOAD_STATUS_FAILURE "UPLOAD_FAILURE"
#define LOG_UPLOAD_STATUS_ABORTED "UPLOAD_ABORTED"
#define GET_STB_DETAILS_SCRIPT_READ_COMMAND "read"

#define OPFLASH_STORE "/opt/secure/persistent/opflashstore"
#define DEVICESTATE_FILE OPFLASH_STORE "/devicestate.txt"
#define BLOCKLIST "blocklist"
#define MIGRATIONSTATUS "/opt/secure/persistent/MigrationStatus"
#define TR181_MIGRATIONSTATUS "Device.DeviceInfo.Migration.MigrationStatus"

/**
 * @struct firmwareUpdate
 * @brief This structure contains information of firmware update.
 * @ingroup SERVMGR_SYSTEM
 */
struct firmwareUpdate {
    string firmwareUpdateVersion; // firmware Version
    int httpStatus;    //http Response code
    bool success;
};

WakeupSrcType conv(const std::string& wakeupSrc)
{
    std::string src = wakeupSrc;
    std::transform(src.begin(), src.end(), src.begin(), ::toupper);

    if (src == "WAKEUPSRC_VOICE") {
        return WakeupSrcType::WAKEUP_SRC_VOICE;
    } else if (src == "WAKEUPSRC_PRESENCE_DETECTION") {
        return WakeupSrcType::WAKEUP_SRC_PRESENCEDETECTED;
    } else if (src == "WAKEUPSRC_BLUETOOTH") {
        return WakeupSrcType::WAKEUP_SRC_BLUETOOTH;
    } else if (src == "WAKEUPSRC_WIFI") {
        return WakeupSrcType::WAKEUP_SRC_WIFI;
    } else if (src == "WAKEUPSRC_IR") {
        return WakeupSrcType::WAKEUP_SRC_IR;
    } else if (src == "WAKEUPSRC_POWER_KEY") {
        return WakeupSrcType::WAKEUP_SRC_POWERKEY;
    } else if (src == "WAKEUPSRC_TIMER") {
        return WakeupSrcType::WAKEUP_SRC_TIMER;
    } else if (src == "WAKEUPSRC_CEC") {
        return WakeupSrcType::WAKEUP_SRC_CEC;
    } else if (src == "WAKEUPSRC_LAN") {
        return WakeupSrcType::WAKEUP_SRC_LAN;
    } else if (src == "WAKEUPSRC_RF4CE") {
        return WakeupSrcType::WAKEUP_SRC_RF4CE;
    } else {
        LOGERR("Unknown wakeup source string: %s", wakeupSrc.c_str());
        return WakeupSrcType::WAKEUP_SRC_UNKNOWN;
    }
}

const char* getWakeupSrcString(uint32_t src)
{
    switch (src)
    {
    case WPEFramework::Exchange::IPowerManager::WAKEUP_SRC_VOICE:
         return "WAKEUPSRC_VOICE";
    case WPEFramework::Exchange::IPowerManager::WAKEUP_SRC_PRESENCEDETECTED:
         return "WAKEUPSRC_PRESENCE_DETECTION";
    case WPEFramework::Exchange::IPowerManager::WAKEUP_SRC_BLUETOOTH:
         return "WAKEUPSRC_BLUETOOTH";
    case WPEFramework::Exchange::IPowerManager::WAKEUP_SRC_RF4CE:
         return "WAKEUPSRC_RF4CE";
    case WPEFramework::Exchange::IPowerManager::WAKEUP_SRC_WIFI:
         return "WAKEUPSRC_WIFI";
    case WPEFramework::Exchange::IPowerManager::WAKEUP_SRC_IR:
         return "WAKEUPSRC_IR";
    case WPEFramework::Exchange::IPowerManager::WAKEUP_SRC_POWERKEY:
         return "WAKEUPSRC_POWER_KEY";
    case WPEFramework::Exchange::IPowerManager::WAKEUP_SRC_TIMER:
         return "WAKEUPSRC_TIMER";
    case WPEFramework::Exchange::IPowerManager::WAKEUP_SRC_CEC:
         return "WAKEUPSRC_CEC";
    case WPEFramework::Exchange::IPowerManager::WAKEUP_SRC_LAN:
         return "WAKEUPSRC_LAN";
    default:
         return "";
    }
}

#if defined(USE_IARMBUS) || defined(USE_IARM_BUS)

std::string iarmModeToString(IARM_Bus_Daemon_SysMode_t& iarmMode)
{
    if (IARM_BUS_SYS_MODE_WAREHOUSE == iarmMode) {
        return MODE_WAREHOUSE;
    } else if (IARM_BUS_SYS_MODE_EAS == iarmMode) {
        return MODE_EAS;
    }

    return MODE_NORMAL;
}

void stringToIarmMode(std::string mode, IARM_Bus_Daemon_SysMode_t& iarmMode)
{
    if (MODE_WAREHOUSE == mode) {
        iarmMode = IARM_BUS_SYS_MODE_WAREHOUSE;
    } else if (MODE_EAS == mode) {
        iarmMode = IARM_BUS_SYS_MODE_EAS;
    } else {
        iarmMode = IARM_BUS_SYS_MODE_NORMAL;
    }
}

#endif /* defined(USE_IARMBUS) || defined(USE_IARM_BUS) */

namespace WPEFramework
{
    namespace Plugin
    {
        //Prototypes
        std::string   SystemServicesImplementation::m_currentMode = "";
        cTimer    SystemServicesImplementation::m_operatingModeTimer;
        int       SystemServicesImplementation::m_remainingDuration = 0;
        const string SystemServicesImplementation::MODEL_NAME = "modelName";
        const string SystemServicesImplementation::HARDWARE_ID = "hardwareID";
        const string SystemServicesImplementation::FRIENDLY_ID = "friendly_id";

#ifdef ENABLE_THERMAL_PROTECTION
        static void handleThermalLevelChange(const int &currentThermalLevel, const int &newThermalLevel, const float &currentTemperature);
#endif /* ENABLE_THERMAL_PROTECTION */
#ifdef ENABLE_SYSTIMEMGR_SUPPORT
        void _timerStatusEventHandler(const char *owner, IARM_EventId_t eventId, void *data, size_t len);
#endif// ENABLE_SYSTIMEMGR_SUPPORT

#if defined(USE_IARMBUS) || defined(USE_IARM_BUS)
        static IARM_Result_t _SysModeChange(void *arg);
        static void _systemStateChanged(const char *owner,
                IARM_EventId_t eventId, void *data, size_t len);
        static void _deviceMgtUpdateReceived(const char *owner,
                IARM_EventId_t eventId, void *data, size_t len);	
#endif /* defined(USE_IARMBUS) || defined(USE_IARM_BUS) */

        SERVICE_REGISTRATION(SystemServicesImplementation, 1, 0);
        SystemServicesImplementation* SystemServicesImplementation::_instance = nullptr;
        cSettings SystemServicesImplementation::m_temp_settings(SYSTEM_SERVICE_TEMP_FILE);
    
        SystemServicesImplementation::SystemServicesImplementation() : 
            _adminLock()
            , _service(nullptr)
            , _pwrMgrNotification(*this)
            , _registeredEventHandlers(false)
        {
            LOGINFO("Create SystemServicesImplementation Instance");

            SystemServicesImplementation::_instance = this;

            m_strStandardTerritoryList =   "ABW AFG AGO AIA ALA ALB AND ARE ARG ARM ASM ATA ATF ATG AUS AUT AZE BDI BEL BEN BES BFA BGD BGR BHR BHS BIH BLM BLR BLZ BMU BOL                BRA BRB BRN BTN BVT BWA CAF CAN CCK CHE CHL CHN CIV CMR COD COG COK COL COM CPV CRI CUB Cuba CUW CXR CYM CYP CZE DEU DJI DMA DNK DOM DZA ECU EGY ERI ESH ESP                EST ETH FIN FJI FLK FRA FRO FSM GAB GBR GEO GGY GHA GIB GIN GLP GMB GNB GNQ GRC GRD GRL GTM GUF GUM GUY HKG HMD HND HRV HTI HUN IDN IMN IND IOT IRL IRN IRQ                 ISL ISR ITA JAM JEY JOR JPN KAZ KEN KGZ KHM KIR KNA KOR KWT LAO LBN LBR LBY LCA LIE LKA LSO LTU LUX LVA MAC MAF MAR MCO MDA MDG MDV MEX MHL MKD MLI MLT MMR                 MNE MNG MNP MOZ MRT MSR MTQ MUS MWI MYS MYT NAM NCL NER NFK NGA NIC NIU NLD NOR NPL NRU NZL OMN PAK PAN PCN PER PHL PLW PNG POL PRI PRK PRT PRY PSE PYF QAT                 REU ROU RUS RWA SAU SDN SEN SGP SGS SHN SJM SLB SLE SLV SMR SOM SPM SRB SSD STP SUR SVK SVN SWE SWZ SXM SYC SYR TCA TCD TGO THA TJK TKL TKM TLS TON TTO TUN                 TUR TUV TWN TZA UGA UKR UMI URY USA UZB VAT VCT VEN VGB VIR VNM VUT WLF WSM YEM ZAF ZMB ZWE";

            SystemServicesImplementation::m_FwUpdateState_LatestEvent=FirmwareUpdateStateUninitialized;

            m_networkStandbyModeValid = false;
            m_networkStandbyMode = false;
            m_powerStateBeforeRebootValid = false;
            m_friendlyName = "Living Room";

#ifdef ENABLE_DEVICE_MANUFACTURER_INFO
            m_ManufacturerDataHardwareIdValid = false;
            m_ManufacturerDataModelNameValid = false;
            m_MfgSerialNumberValid = false;
#endif
            m_uploadLogsPid = -1;

            regcomp (&m_regexUnallowedChars, REGEX_UNALLOWABLE_INPUT, REG_EXTENDED);
        }

        SystemServicesImplementation::~SystemServicesImplementation()
        {
            regfree (&m_regexUnallowedChars);

            if (_powerManagerPlugin) {
                _powerManagerPlugin->Unregister(_pwrMgrNotification.baseInterface<Exchange::IPowerManager::INetworkStandbyModeChangedNotification>());
                _powerManagerPlugin->Unregister(_pwrMgrNotification.baseInterface<Exchange::IPowerManager::IThermalModeChangedNotification>());
                _powerManagerPlugin->Unregister(_pwrMgrNotification.baseInterface<Exchange::IPowerManager::IRebootNotification>());
                _powerManagerPlugin->Unregister(_pwrMgrNotification.baseInterface<Exchange::IPowerManager::IModeChangedNotification>());		    
                _powerManagerPlugin.Reset();
            }

            _registeredEventHandlers = false;
            m_operatingModeTimer.stop();
#if defined(USE_IARMBUS) || defined(USE_IARM_BUS)
            DeinitializeIARM();
#endif /* defined(USE_IARMBUS) || defined(USE_IARM_BUS) */

            SystemServicesImplementation::_instance = nullptr;
            if (m_shellService) {
                m_shellService->Release();
                m_shellService = nullptr;
            }
            _service = nullptr;
        }

        Core::hresult SystemServicesImplementation::Register(Exchange::ISystemServices::INotification *notification)
        {
            ASSERT (nullptr != notification);

            _adminLock.Lock();

            // Make sure we can't register the same notification callback multiple times
            if (std::find(_systemServicesNotification.begin(), _systemServicesNotification.end(), notification) == _systemServicesNotification.end())
            {
                _systemServicesNotification.push_back(notification);
                notification->AddRef();
            }
            else
            {
                LOGERR("same notification is registered already");
            }

            _adminLock.Unlock();

            return Core::ERROR_NONE;
        }

        Core::hresult SystemServicesImplementation::Unregister(Exchange::ISystemServices::INotification *notification )
        {
            Core::hresult status = Core::ERROR_GENERAL;

            ASSERT (nullptr != notification);

            _adminLock.Lock();

            // we just unregister one notification once
            auto itr = std::find(_systemServicesNotification.begin(), _systemServicesNotification.end(), notification);
            if (itr != _systemServicesNotification.end())
            {
                (*itr)->Release();
                _systemServicesNotification.erase(itr);
                status = Core::ERROR_NONE;
            }
            else
            {
                LOGERR("notification not found");
            }

            _adminLock.Unlock();

            return status;
        }

        uint32_t SystemServicesImplementation::Configure(PluginHost::IShell* service)
        {
            #if defined(USE_IARMBUS) || defined(USE_IARM_BUS)
            InitializeIARM();
#endif /* defined(USE_IARMBUS) || defined(USE_IARM_BUS) */
            m_shellService = service;
            m_shellService->AddRef();
            InitializePowerManager();

            //Initialise timer with interval and callback function.
            m_operatingModeTimer.setInterval(updateDuration, MODE_TIMER_UPDATE_INTERVAL);

            //first boot? then set to NORMAL mode
            if (!m_temp_settings.contains("mode") && m_currentMode == "") {
                ModeInfo modeinfo;
                uint32_t SysSrv_Status;
                string errorMessage;
                bool success;
                modeinfo.duration = -1;
                modeinfo.mode = MODE_NORMAL;

                LOGINFO("first boot so setting mode to '%s' ('%s' does not contain(\"mode\"))\n",
                        modeinfo.mode.c_str(), SYSTEM_SERVICE_TEMP_FILE);

                SetMode(modeinfo, SysSrv_Status, errorMessage, success);
            } else if (m_currentMode.empty()) {
                ModeInfo modeinfo;
                uint32_t SysSrv_Status;
                string errorMessage;
                bool success;
                modeinfo.duration = static_cast<int>(m_temp_settings.getValue("mode_duration").Number());
                modeinfo.mode = m_temp_settings.getValue("mode");

                LOGINFO("receiver restarted so setting mode:%s duration:%d\n",
                        modeinfo.mode.c_str(), (int)modeinfo.duration);

                SetMode(modeinfo, SysSrv_Status, errorMessage, success);
            }

#ifdef DISABLE_GEOGRAPHY_TIMEZONE
            std::string timeZone = getTimeZoneDSTHelper();

            if (!timeZone.empty()) {
                std::string tzenv = ":";
                tzenv += timeZone;
                Core::SystemInfo::SetEnvironment(_T("TZ"), tzenv.c_str());
            }
#endif
            RFC_ParamData_t param = {0};
            WDMP_STATUS status = getRFCParameter((char*)"thunderapi", TR181_SYSTEM_FRIENDLY_NAME, &param);
            if(WDMP_SUCCESS == status && param.type == WDMP_STRING)
            {
                m_friendlyName = param.value;
                LOGINFO("Success Getting the friendly name value :%s \n",m_friendlyName.c_str());
            }
            return Core::ERROR_NONE;
        }
        
        void SystemServicesImplementation::InitializePowerManager()
        {
            LOGINFO("Connect the COM-RPC socket\n");
            _powerManagerPlugin = PowerManagerInterfaceBuilder(_T("org.rdk.PowerManager"))
                .withIShell(m_shellService)
                .withRetryIntervalMS(200)
                .withRetryCount(25)
                .createInterface();

            registerEventHandlers();
        }

#if defined(USE_IARMBUS) || defined(USE_IARM_BUS)
        void SystemServicesImplementation::InitializeIARM()
        {
            if (Utils::IARM::init())
            {
                IARM_Result_t res;
                IARM_CHECK( IARM_Bus_RegisterCall(IARM_BUS_COMMON_API_SysModeChange, _SysModeChange));
                IARM_CHECK( IARM_Bus_RegisterEventHandler(IARM_BUS_SYSMGR_NAME, IARM_BUS_SYSMGR_EVENT_SYSTEMSTATE, _systemStateChanged));
                IARM_CHECK( IARM_Bus_RegisterEventHandler(IARM_BUS_SYSMGR_NAME, IARM_BUS_SYSMGR_EVENT_DEVICE_UPDATE_RECEIVED, _deviceMgtUpdateReceived));
#ifdef ENABLE_SYSTIMEMGR_SUPPORT
                IARM_CHECK( IARM_Bus_RegisterEventHandler(IARM_BUS_SYSTIME_MGR_NAME, cTIMER_STATUS_UPDATE, _timerStatusEventHandler));
#endif// ENABLE_SYSTIMEMGR_SUPPORT
            }
	    
        }

        void SystemServicesImplementation::DeinitializeIARM()
        {
            if (Utils::IARM::isConnected())
            {
                IARM_Result_t res;
                IARM_CHECK( IARM_Bus_RemoveEventHandler(IARM_BUS_SYSMGR_NAME, IARM_BUS_SYSMGR_EVENT_SYSTEMSTATE, _systemStateChanged));
		        IARM_CHECK( IARM_Bus_RemoveEventHandler(IARM_BUS_SYSMGR_NAME, IARM_BUS_SYSMGR_EVENT_DEVICE_UPDATE_RECEIVED, _deviceMgtUpdateReceived));
            }
        }
#endif /* defined(USE_IARMBUS) || defined(USE_IARM_BUS) */

        IPowerManager* SystemServicesImplementation::getPwrMgrPluginInstance()
        {
            return _powerManagerPlugin.operator->();
        }

        void SystemServicesImplementation::registerEventHandlers()
        {
            ASSERT (_powerManagerPlugin);

            if(!_registeredEventHandlers && _powerManagerPlugin) {
                _registeredEventHandlers = true;
                _powerManagerPlugin->Register(_pwrMgrNotification.baseInterface<Exchange::IPowerManager::INetworkStandbyModeChangedNotification>());
                _powerManagerPlugin->Register(_pwrMgrNotification.baseInterface<Exchange::IPowerManager::IThermalModeChangedNotification>());
                _powerManagerPlugin->Register(_pwrMgrNotification.baseInterface<Exchange::IPowerManager::IRebootNotification>());
                _powerManagerPlugin->Register(_pwrMgrNotification.baseInterface<Exchange::IPowerManager::IModeChangedNotification>());
            }
        }

        void SystemServicesImplementation::OnPowerModeChanged(const PowerState currentState, const PowerState newState)
        {
            std::string curPowerState,newPowerState = "";

            curPowerState = powerModeEnumToString(currentState);
            newPowerState = powerModeEnumToString(newState);

            LOGWARN("IARM Event triggered for PowerStateChange.\
                    Old State %s, New State: %s\n",
                    curPowerState.c_str() , newPowerState.c_str());
            if (SystemServicesImplementation::_instance) {
                SystemServicesImplementation::_instance->OnSystemPowerStateChanged(std::move(curPowerState), std::move(newPowerState));
            } else {
                LOGERR("SystemServicesImplementation::_instance is NULL.\n");
            }
        }

        std::string SystemServicesImplementation::powerModeEnumToString(PowerState state)
        {
            std::string powerState = "";
            switch (state) 
            {
                case WPEFramework::Exchange::IPowerManager::POWER_STATE_ON: powerState = "ON"; break;
                case WPEFramework::Exchange::IPowerManager::POWER_STATE_OFF: powerState = "OFF"; break;
                case WPEFramework::Exchange::IPowerManager::POWER_STATE_STANDBY: powerState = "LIGHT_SLEEP"; break;
                case WPEFramework::Exchange::IPowerManager::POWER_STATE_STANDBY_LIGHT_SLEEP: powerState = "LIGHT_SLEEP"; break;
                case WPEFramework::Exchange::IPowerManager::POWER_STATE_STANDBY_DEEP_SLEEP: powerState = "DEEP_SLEEP"; break;
                default: break;
            }
            return powerState;
        }

        void SystemServicesImplementation::OnThermalModeChanged(const ThermalTemperature currentThermalLevel, const ThermalTemperature newThermalLevel, const float currentTemperature)
        {
            handleThermalLevelChange(currentThermalLevel, newThermalLevel, currentTemperature);
        }

        void SystemServicesImplementation::dispatchEvent(Event event, const JsonObject &params)
        {
            Core::IWorkerPool::Instance().Submit(Job::Create(this, event, params));
        }

        void SystemServicesImplementation::Dispatch(Event event, const JsonObject &params)
        {
            _adminLock.Lock();

            std::list<Exchange::ISystemServices::INotification*>::const_iterator index(_systemServicesNotification.begin());
        
            switch(event)
            {
                case SYSTEMSERVICES_EVT_ONFIRMWAREUPDATEINFORECEIVED:
                {
                    FirmwareUpdateInfo firmwareUpdateInfo;
                    firmwareUpdateInfo.status = static_cast<int>(params["status"].Number());
                    firmwareUpdateInfo.responseString = params["responseString"].String();
                    firmwareUpdateInfo.firmwareUpdateVersion = params["firmwareUpdateVersion"].String();
                    firmwareUpdateInfo.rebootImmediately = params["rebootImmediately"].Boolean();
                    firmwareUpdateInfo.updateAvailable = params["updateAvailable"].Boolean();
                    firmwareUpdateInfo.updateAvailableEnum = static_cast<int>(params["updateAvailableEnum"].Number());
                    firmwareUpdateInfo.success = params["success"].Boolean();

                    while (index != _systemServicesNotification.end()) 
                    {
                        (*index)->OnFirmwareUpdateInfoReceived(firmwareUpdateInfo);
                        ++index;
                    }
                    break;
                }

                case SYSTEMSERVICES_EVT_ONBLOCKLISTCHANGED:
                {
                    string oldFlag = params["oldBlocklistFlag"].String();
                    string newFlag = params["newBlocklistFlag"].String();
                    
                    while (index != _systemServicesNotification.end()) 
                    {
                        (*index)->OnBlocklistChanged(oldFlag, newFlag);
                        ++index;
                    }
                    break;
                }

                case SYSTEMSERVICES_EVT_ONLOGUPLOAD:
                {
                    string logUploadStatus = params["logUploadStatus"].String();
                   
		            while (index != _systemServicesNotification.end())
                    {
                        (*index)->OnLogUpload(logUploadStatus);
                        ++index;
                    }
                    break;
                }

                case SYSTEMSERVICES_EVT_ONFRIENDLYNAME_CHANGED:
                {
                    string friendlyName = params["friendlyName"].String();

                    while (index != _systemServicesNotification.end())
                    {
                        (*index)->OnFriendlyNameChanged(friendlyName);
                        ++index;
                    }
                    break;

                }

                case SYSTEMSERVICES_EVT_ONREBOOTREQUEST:
                {
                    string requestedApp = params["requestedApp"].String();
                    string rebootReason = params["rebootReason"].String();

                    while (index != _systemServicesNotification.end()) 
                    {
                        (*index)->OnRebootRequest(requestedApp, rebootReason);
                        ++index;
                    }
                    break;
                }

                case SYSTEMSERVICES_EVT_ONSYSTEMPOWERSTATECHANGED:
                {
                    string powerState = params["powerState"].String();
                    string currentPowerState = params["currentPowerState"].String();

                    while (index != _systemServicesNotification.end()) 
                    {
                        (*index)->OnSystemPowerStateChanged(powerState, currentPowerState);
                        ++index;
                    }
                    break;
                }
                
                case SYSTEMSERVICES_EVT_ONTERRITORYCHANGED:
                {
                    TerritoryChangedInfo territoryChangedInfo;
                    territoryChangedInfo.oldTerritory = params["oldTerritory"].String();
                    territoryChangedInfo.newTerritory = params["newTerritory"].String();
                    territoryChangedInfo.oldRegion = params["oldRegion"].String();
                    territoryChangedInfo.newRegion = params["newRegion"].String();

                    while (index != _systemServicesNotification.end()) 
                    {
                        (*index)->OnTerritoryChanged(territoryChangedInfo);
                        ++index;
                    }
                    break;
                }
                
                case SYSTEMSERVICES_EVT_ONTIMEZONEDSTCHANGED:
                {
                    TimeZoneDSTChangedInfo timeZoneDSTChangedInfo;
                    timeZoneDSTChangedInfo.oldTimeZone = params["oldTimeZone"].String();
                    timeZoneDSTChangedInfo.newTimeZone = params["newTimeZone"].String();
                    timeZoneDSTChangedInfo.oldAccuracy = params["oldAccuracy"].String();
                    timeZoneDSTChangedInfo.newAccuracy = params["newAccuracy"].String();

                    while (index != _systemServicesNotification.end()) 
                    {
                        (*index)->OnTimeZoneDSTChanged(timeZoneDSTChangedInfo);
                        ++index;
                    }
                    break;
                }
                
                case SYSTEMSERVICES_EVT_ONNETWORKSTANDBYMODECHANGED:
                {
                    bool nwStandby = params["nwStandby"].Boolean();

                    while (index != _systemServicesNotification.end()) 
                    {
                        (*index)->OnNetworkStandbyModeChanged(nwStandby);
                        ++index;
                    }
                    break;
                }
                
                case SYSTEMSERVICES_EVT_ONFIRMWAREUPDATESTATECHANGED:
                {
                    int firmwareUpdateStateChange = static_cast<int>(params["firmwareUpdateStateChange"].Number());

                    while (index != _systemServicesNotification.end()) 
                    {
                        (*index)->OnFirmwareUpdateStateChanged(firmwareUpdateStateChange);
                        ++index;
                    }
                    break;
                }
                
                case SYSTEMSERVICES_EVT_ONTEMPERATURETHRESHOLDCHANGED:
                {
                    string thresholdType = params["thresholdType"].String();
                    bool exceeded = params["exceeded"].Boolean();
                    string temperature = params["temperature"].String();

                    while (index != _systemServicesNotification.end()) 
                    {
                        (*index)->OnTemperatureThresholdChanged(thresholdType, exceeded, temperature);
                        ++index;
                    }
                    break;
                }
                
                case SYSTEMSERVICES_EVT_ON_SYSTEM_CLOCK_SET:
                {
                    while (index != _systemServicesNotification.end()) 
                    {
                        (*index)->OnSystemClockSet();
                        ++index;
                    }
                    break;
                }
                
                case SYSTEMSERVICES_EVT_ONFWPENDINGREBOOT:
                {
                    int fireFirmwarePendingReboot = static_cast<int>(params["fireFirmwarePendingReboot"].Number());

                    while (index != _systemServicesNotification.end()) 
                    {
                        (*index)->OnFirmwarePendingReboot(fireFirmwarePendingReboot);
                        ++index;
                    }
                    break;
                }
                
                case SYSTEMSERVICES_EVT_ONDEVICEMGTUPDATERECEIVED:
                {
                    string source = params["source"].String();
                    string type = params["type"].String();
                    bool success = params["success"].Boolean();

                    while (index != _systemServicesNotification.end()) 
                    {
                        (*index)->OnDeviceMgtUpdateReceived(source, type, success);
                        ++index;
                    }
                    break;
                }
                
                case SYSTEMSERVICES_EVT_ONTIMESTATUSCHANGED:
                {
                    string timeQuality = params["TimeQuality"].String();
                    string timeSrc = params["TimeSrc"].String();
                    string time = params["Time"].String();

                    while (index != _systemServicesNotification.end()) 
                    {
                        (*index)->OnTimeStatusChanged(timeQuality, timeSrc, time);
                        ++index;
                    }
                    break;
                }
                
                case SYSTEMSERVICES_EVT_ONMACADDRESSRETRIEVED:
                {
                    MacAddressesInfo macAddressesInfo;
                    macAddressesInfo.ecmMac = params["ecm_mac"].String();
                    macAddressesInfo.estbMac = params["estb_mac"].String();
                    macAddressesInfo.mocaMac = params["moca_mac"].String();
                    macAddressesInfo.ethMac = params["eth_mac"].String();
                    macAddressesInfo.wifiMac = params["wifi_mac"].String();
                    macAddressesInfo.bluetoothMac = params["bluetooth_mac"].String();
                    macAddressesInfo.rf4ceMac = params["rf4ce_mac"].String();
                    macAddressesInfo.info = params["info"].String();
                    macAddressesInfo.success = params["success"].Boolean();

                    while (index != _systemServicesNotification.end()) 
                    {
                        (*index)->OnMacAddressesRetreived(macAddressesInfo);
                        ++index;
                    }
                    break;
                }
                
                case SYSTEMSERVICES_EVT_ONSYSTEMMODECHANGED:
                {
                    string mode = params["mode"].String();

                    while (index != _systemServicesNotification.end()) 
                    {
                        (*index)->OnSystemModeChanged(mode);
                        ++index;
                    }
                    break;
                }
                
                default:
                    LOGWARN("Event[%u] not handled", event);
                    break;
            }
            _adminLock.Unlock();
        }

        void SystemServicesImplementation::startModeTimer(int duration)
        {
            m_remainingDuration = duration;
            m_operatingModeTimer.start();
            //set values in temp file so they can be restored in receiver restarts / crashes
            m_temp_settings.setValue("mode_duration", m_remainingDuration);
        }

        void SystemServicesImplementation::stopModeTimer()
        {
            m_remainingDuration = 0;
            m_operatingModeTimer.stop();

            //set values in temp file so they can be restored in receiver restarts / crashes
            // TODO: query & confirm time duration range.
            m_temp_settings.setValue("mode_duration", m_remainingDuration);
        }

        void SystemServicesImplementation::updateDuration()
        {
            if (m_remainingDuration > 0) {
                m_remainingDuration--;
            } else {
                m_operatingModeTimer.stop();
                m_operatingModeTimer.detach();
                ModeInfo modeinfo;
                uint32_t SysSrv_Status;
                string errorMessage;
                bool success;
                modeinfo.mode = "NORMAL";
                modeinfo.duration = 0;
                if (_instance) {
                    _instance->SetMode(modeinfo, SysSrv_Status, errorMessage, success);
                } else {
                    LOGERR("_instance is NULL.\n");
                }
            }

            //set values in temp file so they can be restored in receiver restarts / crashes
            m_temp_settings.setValue("mode_duration", m_remainingDuration);
        }

        uint32_t GetValueFromPropertiesFile(const char* filename, const char* key, string& response, const char *delimiter = "=")
        {
            uint32_t result = Core::ERROR_GENERAL;

            if (!Utils::fileExists(filename)) {
                return result;
            }

            char buf[1024];

            FILE *f = fopen(filename, "r");

            if(!f) {
                LOGWARN("failed to open %s:%s", filename, strerror(errno));
                return result;
            }

            std::string line;

            while(fgets(buf, sizeof(buf), f) != NULL) {
                line = buf;
                size_t eq = line.find_first_of(delimiter);

                if (std::string::npos != eq) {
                    std::string k = line.substr(0, eq);

                    if (k == key) {
                        response = line.substr(eq + strlen(delimiter));
                        Utils::String::trim(response);
                        result = Core::ERROR_NONE;
                        break;
                    }
                }
            }

            fclose(f);

            return result;
        }

        bool checkOpFlashStoreDir()
        {
            int ret = mkdir(OPFLASH_STORE, 0774);
            if (ret == 0) {
                LOGINFO(" --- Directory %s created", OPFLASH_STORE);
                return true;
            } else if (errno == EEXIST) {
                // Directory already exists, which is fine
                return true;
            } else {
                LOGERR(" --- Failed to create directory %s: %d", OPFLASH_STORE, ret);
                return false;
            }
        }

        // Function to write (update or append) parameters in the file
        bool write_parameters(const string &filename, const string &param, bool value, bool &update, bool &oldBlocklistFlag)
        {
            ifstream file_in(filename);
            vector<string> lines;
            bool param_found = false;

            // If file exists, read its content line by line
            if (file_in.is_open()) {
                string line;
                while (getline(file_in, line)) {
                    size_t pos = line.find('=');

                    // Check if the line contains the parameter we're searching for
                    if (pos != string::npos) {
                        string file_param = line.substr(0, pos);
                        string file_value = line.substr(pos + 1);
                        if (file_param == param) {
                            // check the file value and requested value same
                            if (file_value == (value ? "true" : "false")) {
                                file_in.close();
                                update = false;
                                LOGINFO("Persistence store has updated value. blocklist= %s, update=%d", (value ? "true" : "false"), update);
                                return true;
                            }
                            else {
                                update = true;
                                //store old value for notify.
                                if(file_value == "true"){
                                    oldBlocklistFlag = true;
                                }
                                else if(file_value == "false"){
                                    oldBlocklistFlag = false;
                                }
                                // Update the parameter value
                                line = param + "=" + (value ? "true" : "false");
                                param_found = true;
                            }
                        }
                    }

                    // Store the line (updated or not) in memory
                    lines.push_back(line);
                }

                file_in.close();
            }

            // If the parameter wasn't found in the file, add it
            if (!param_found) {
                lines.push_back(param + "=" + (value ? "true" : "false"));
            }

            // Rewrite the entire file with updated values
            ofstream file_out(filename);
            if (!file_out) {
                LOGERR("Error opening file for writing: %s", filename.c_str());
                return false;
            }

            for (const auto &line : lines) {
                file_out << line << '\n';
            }

            LOGINFO("%s flag stored successfully in persistent memory. update=%d, oldBlocklistFlag=%d", param.c_str(), update, oldBlocklistFlag);
            return true;
        }
        
        // Function to read a parameter from a file and update its value
        bool read_parameters(const string &filename, const string &param, bool &value)
        {
            ifstream file(filename);

            // Check if the file was successfully opened
            if (!file.is_open()) {
                LOGERR("Error opening file for reading: %s", filename.c_str());
                return false;
            }

            string line;
            bool param_found = false;
            while (getline(file, line)) {
                // Remove any trailing newline characters
                //line.erase(line.find_last_not_of("\n\r") + 1);

                // Split the line into parameter and value using '=' delimiter
                size_t pos = line.find('=');
                if (pos != string::npos) {
                    string file_param = line.substr(0, pos);
                    string file_value = line.substr(pos + 1);

                    // Check if this is the parameter we are looking for
                    if (file_param == param) {
                        param_found = true;
                        if (file_value == "true") {
                            value = true;
                        } else if (file_value == "false") {
                            value = false;
                        } else {
                            LOGERR("Error: Invalid value for parameter %s  in file: %s", param.c_str(), file_value.c_str());
                            file.close();
                            return false;  // Invalid value
                        }
                        break;
                    }
                }
            }

            // Check if there were any read errors
            if (file.fail() && !file.eof()) {
                LOGERR("Error reading from file: %s", filename.c_str());
                file.close();
                return false;
            }

            file.close();

            if (!param_found) {
                LOGERR("Parameter %s  not found in the file.", param.c_str());
                return false;
            }

            return true;
        }

        Core::hresult SystemServicesImplementation::UploadLogsAsync(SystemResult& result)
        {
            pid_t uploadLogsPid = -1;
            {
                std::lock_guard<std::mutex> lck(m_uploadLogsMutex);
                uploadLogsPid = m_uploadLogsPid;
            }

            if (-1 != uploadLogsPid) {
                LOGWARN("Another instance of log upload script is running");
                AbortLogUpload(result);
            }

            std::lock_guard<std::mutex> lck(m_uploadLogsMutex);
            m_uploadLogsPid = UploadLogs::logUploadAsync();
            result.success = true;


            return Core::ERROR_NONE;
        }

        Core::hresult SystemServicesImplementation::AbortLogUpload(SystemResult& result)
        {
            std::lock_guard<std::mutex> lck(m_uploadLogsMutex);

            if (-1 != m_uploadLogsPid) {
                std::vector<int> processIds;
                bool res = Utils::getChildProcessIDs(m_uploadLogsPid, processIds);

                if (true == res) {
                    std::vector<int>::iterator pid_iterator = processIds.begin();
                    while (pid_iterator != processIds.end()) {
                        std::string line = std::to_string(*pid_iterator);
                        line = trim(line);
                        char *end;
                        int pid = strtol(line.c_str(), &end, 10);
                        if (line.c_str() != end && 0 != pid && 1 != pid) {
                            kill(pid, SIGKILL);
                        } else {
                            LOGERR("Bad pid: %d", pid);
                        }
                        pid_iterator++;
                    }

                } else {
                    LOGERR("Cannot get the child process Ids\n");
                }

                kill(m_uploadLogsPid, SIGKILL);

                int status;
                waitpid(m_uploadLogsPid, &status, 0);

                m_uploadLogsPid = -1;

                JsonObject params;
                params["logUploadStatus"] = LOG_UPLOAD_STATUS_ABORTED;
                dispatchEvent(SYSTEMSERVICES_EVT_ONLOGUPLOAD, params);
                
                result.success = true;
                return Core::ERROR_NONE;
            }

            LOGERR("Upload logs script is not running");
            return Core::ERROR_NONE;
        }

#ifdef ENABLE_DEVICE_MANUFACTURER_INFO
        Core::hresult SystemServicesImplementation::GetMfgSerialNumber(string& mfgSerialNumber, bool& success)
        {
            LOGWARN("SystemService getMfgSerialNumber query");

            if (m_MfgSerialNumberValid) {
                mfgSerialNumber = m_MfgSerialNumber;
                LOGWARN("Got cached MfgSerialNumber %s", m_MfgSerialNumber.c_str());
                success = true;
                LOGINFO("response: mfgSerialNumber=%s, success=%d", mfgSerialNumber.c_str(), success);
                return Core::ERROR_NONE;
            }

            IARM_Bus_MFRLib_GetSerializedData_Param_t param;
            param.bufLen = 0;
            param.type = mfrSERIALIZED_TYPE_MANUFACTURING_SERIALNUMBER;
            IARM_Result_t result = IARM_Bus_Call(IARM_BUS_MFRLIB_NAME, IARM_BUS_MFRLIB_API_GetSerializedData, &param, sizeof(param));
            param.buffer[param.bufLen] = '\0';

            bool status = false;
            if (result == IARM_RESULT_SUCCESS) {
                mfgSerialNumber = string(param.buffer);
                status = true;

                m_MfgSerialNumber = string(param.buffer);
                m_MfgSerialNumberValid = true;

                LOGWARN("SystemService getMfgSerialNumber Manufacturing Serial Number: %s", param.buffer);
            } else {
                LOGERR("SystemService getMfgSerialNumber Manufacturing Serial Number: NULL");
            }

            success = status;
            LOGINFO("response: mfgSerialNumber=%s, success=%s", mfgSerialNumber.c_str(), success ? "true" : "false");
            return (status ? Core::ERROR_NONE : Core::ERROR_GENERAL);
        }
#endif /* ENABLE_DEVICE_MANUFACTURER_INFO */

        Core::hresult SystemServicesImplementation::GetFriendlyName(string& friendlyName, bool& success)
        {
            friendlyName = m_friendlyName;
            success = true;
            LOGINFO("response: friendlyName=%s, success=%s", friendlyName.c_str(), success ? "true" : "false");
            return Core::ERROR_NONE;
        }

        Core::hresult SystemServicesImplementation::SetFriendlyName(const string& friendlyName, SystemResult& result)
        {
            LOGWARN("SystemServicesImplementation::setFriendlyName  :%s \n", friendlyName.c_str());
            if(m_friendlyName != friendlyName)
            {
                m_friendlyName = friendlyName;
                JsonObject params;
                params["friendlyName"] = m_friendlyName;
                dispatchEvent(SYSTEMSERVICES_EVT_ONFRIENDLYNAME_CHANGED, params);

                //write to persistence storage
                WDMP_STATUS status = setRFCParameter((char*)"thunderapi",
                       TR181_SYSTEM_FRIENDLY_NAME,m_friendlyName.c_str(),WDMP_STRING);
                if ( WDMP_SUCCESS == status ){
                    LOGINFO("Success Setting the friendly name value\n");
                }
                else {
                    LOGINFO("Failed Setting the friendly name value %s\n",getRFCErrorString(status));
                }
            }
            result.success = true;
            return Core::ERROR_NONE;
        }

        Core::hresult SystemServicesImplementation::GetBuildType(string& buildType, bool& success)
        {
            bool result = false;
            const char* filename = "/etc/device.properties";
            string propertyName = "BUILD_TYPE";

            if (Utils::readPropertyFromFile(filename, propertyName, buildType))
            {
                LOGINFO("BUILD_TYPE '%s' ", buildType.c_str());
                result = true;
            }
            else
            {
                LOGERR("buildType is empty");
                result = false;
            }

            success = result;
            LOGINFO("response: buildType=%s, success=%s", buildType.c_str(), success ? "true" : "false");
            return Core::ERROR_NONE;
        }

        void SystemServicesImplementation::OnBlocklistChanged(bool newBlocklistFlag, bool oldBlocklistFlag)
        {
            JsonObject params;
            string newBloklistStr = (newBlocklistFlag? "true":"false");
            string oldBloklistStr = (oldBlocklistFlag? "true":"false");

            params["oldBlocklistFlag"] = oldBlocklistFlag;
            params["newBlocklistFlag"] = newBlocklistFlag;
            LOGINFO("blocklist changed from %s to '%s'\n", oldBloklistStr.c_str(), newBloklistStr.c_str());
            dispatchEvent(SYSTEMSERVICES_EVT_ONBLOCKLISTCHANGED, params);
        }

        Core::hresult SystemServicesImplementation::SetBlocklistFlag(const bool blocklist, SetBlocklistResult& result)
        {
            LOGINFO("blocklist=%d", blocklist);
            bool status = false, update = false, ret;
            bool blocklistFlag, oldBlocklistFlag;

            /*check /opt/secure/persistent/opflashstore/ dir*/
            ret = checkOpFlashStoreDir();
            if(ret == true){
                LOGINFO("checked opflashstore directory and it is exists. ret = %d",ret);
            }
            else {
                LOGWARN("failed to create opflashstore directory ret =%d", ret);
                LOGERR("Blocklist flag update failed. status %d ", status);
                result.error.message = "Blocklist flag update failed";
                result.error.code = "-32604";
                result.success = false;
                return Core::ERROR_GENERAL;
            }

            blocklistFlag = blocklist;
            status = write_parameters(DEVICESTATE_FILE, BLOCKLIST, blocklistFlag, update, oldBlocklistFlag);
            if ((status != true)) {
                LOGERR("Blocklist flag update failed. status %d ", status);
                result.error.message = "Blocklist flag update failed";
                result.error.code = "-32604";
                result.success = false;
                return Core::ERROR_GENERAL;
            }
            else {
                LOGINFO("Blocklist flag stored successfully in persistent memory");
                result.success = true;
            }

            LOGINFO("Update= %s", (update ? "true":"false"));
            if(update == true) {
                /*Send ONBLOCKLISTCHANGED event notify*/
                if (SystemServicesImplementation::_instance) {
                    SystemServicesImplementation::_instance->OnBlocklistChanged(blocklistFlag, oldBlocklistFlag);
                } else {
                    LOGERR("SystemServicesImplementation::_instance is NULL.\n");
                }
            }

            return Core::ERROR_NONE;
        }

        Core::hresult SystemServicesImplementation::SetFSRFlag(const bool fsrFlag, SystemResult& result)
        {
            IARM_Bus_MFRLib_FsrFlag_Param_t param;
            param = fsrFlag;
            LOGINFO("Param %d \n", param);
            IARM_Result_t res = IARM_Bus_Call(IARM_BUS_MFRLIB_NAME,
                                   IARM_BUS_MFRLIB_API_SetFsrFlag, (void *)&param,
                                   sizeof(param));
            if (IARM_RESULT_SUCCESS == res) {
                result.success = true;
            } else {
                result.success = false;
            }

            return Core::ERROR_NONE;
        }

        Core::hresult SystemServicesImplementation::GetFSRFlag(bool &fsrFlag, bool& success)
        {
            LOGINFO();
            success = false;
            fsrFlag = false;
            IARM_Bus_MFRLib_FsrFlag_Param_t param;
            IARM_Result_t res = IARM_Bus_Call(IARM_BUS_MFRLIB_NAME,
                                  IARM_BUS_MFRLIB_API_GetFsrFlag, (void *)&param,
                                  sizeof(param));
            if (IARM_RESULT_SUCCESS == res) {
                fsrFlag = param;
                success = true;
            } else {
                success = false;
            }

            LOGINFO("response: fsrFlag=%s, success=%s", fsrFlag ? "true" : "false", success ? "true" : "false");
            return success ? Core::ERROR_NONE : Core::ERROR_GENERAL;
        }

        Core::hresult SystemServicesImplementation::GetBlocklistFlag(BlocklistResult& result)
        {
            LOGINFO();
            bool status = false, ret = false;
            bool blocklistFlag = false;

            result.success = false;
            result.error.message = "";
            result.error.code = "";

            /*check /opt/secure/persistent/opflashstore/ dir*/
            ret = checkOpFlashStoreDir();
            if(ret == true){
                LOGINFO("checked opflashstore directory and it is exists. ret = %d",ret);
            }
            else {
                LOGWARN("Blocklist flag retrieved failed from persistent memory.");
                result.error.message = "Blocklist flag retrieved failed from persistent memory.";
                result.error.code = "-32099";
                result.success = false;
                LOGINFO("response: success=%d, error.code=%s", result.success, result.error.code.c_str());
                return Core::ERROR_GENERAL;
            }

            status = read_parameters(DEVICESTATE_FILE, BLOCKLIST, blocklistFlag);
            if (status == true) {
                LOGWARN("blocklistFlag=%d", blocklistFlag);
                result.blocklist = blocklistFlag;
                result.success = true;
                LOGINFO("Blocklist flag retrieved successfully from persistent memory.");
            }
            else{
                LOGWARN("Blocklist flag retrieved failed from persistent memory.");
                result.error.message = "Blocklist flag retrieved failed from persistent memory.";
                result.error.code = "-32099";
                result.success = false;
            }

            LOGINFO("response: blocklist=%s, success=%s", result.blocklist ? "true" : "false", result.success ? "true" : "false");
            return Core::ERROR_NONE;
        }

        Core::hresult SystemServicesImplementation::IsOptOutTelemetry(bool& OptOut, bool& success)
        {
            uint32_t result = Core::ERROR_GENERAL;
            auto telemetryObject = m_shellService->QueryInterfaceByCallsign<Exchange::ITelemetry>("org.rdk.Telemetry");
            if (telemetryObject)
            {
                result = telemetryObject->IsOptOutTelemetry(OptOut, success);
                if (Core::ERROR_NONE != result)
                {
                    LOGERR("Failed to get telemetry opt-out status\n");
                }
                telemetryObject->Release();
            }
            else
            {
                LOGERR("Telemetry plugin is not activated\n");
            }

            LOGINFO("response: OptOut=%s, success=%s", OptOut ? "true" : "false", success ? "true" : "false");
            return result;
        }

        Core::hresult SystemServicesImplementation::SetOptOutTelemetry(const bool OptOut, SystemResult& result)
        {
            LOGINFO("OptOut=%d", OptOut);
            uint32_t errCode = Core::ERROR_GENERAL;
            Exchange::ITelemetry::TelemetrySuccess teleResult;

            auto telemetryObject = m_shellService->QueryInterfaceByCallsign<Exchange::ITelemetry>("org.rdk.Telemetry");

            if (telemetryObject)
            {
                errCode = telemetryObject->SetOptOutTelemetry(OptOut, teleResult);
                if ( Core::ERROR_NONE == errCode)
                    result.success = teleResult.success;
                telemetryObject->Release();
            }
            else
            {
                LOGERR("Telemetry plugin is not activated\n");
            }

            return errCode;
        }

        Core::hresult SystemServicesImplementation::SetMigrationStatus(const string& status, bool& success)
        {
            LOGINFO("status=%s", status.c_str());
            uint32_t result = Core::ERROR_GENERAL;

            Exchange::IMigration::MigrationStatus migrationStatus = Exchange::IMigration::MIGRATION_STATUS_NOT_STARTED;
            Exchange::IMigration::MigrationResult migrationResult;


            static const std::unordered_map<std::string,
            Exchange::IMigration::MigrationStatus> stringToStatus = {
                {"NOT_STARTED",                Exchange::IMigration::MIGRATION_STATUS_NOT_STARTED},
                {"NOT_NEEDED",                 Exchange::IMigration::MIGRATION_STATUS_NOT_NEEDED},
                {"STARTED",                    Exchange::IMigration::MIGRATION_STATUS_STARTED},
                {"PRIORITY_SETTINGS_MIGRATED", Exchange::IMigration::MIGRATION_STATUS_PRIORITY_SETTINGS_MIGRATED},
                {"DEVICE_SETTINGS_MIGRATED",   Exchange::IMigration::MIGRATION_STATUS_DEVICE_SETTINGS_MIGRATED},
                {"CLOUD_SETTINGS_MIGRATED",    Exchange::IMigration::MIGRATION_STATUS_CLOUD_SETTINGS_MIGRATED},
                {"APP_DATA_MIGRATED",          Exchange::IMigration::MIGRATION_STATUS_APP_DATA_MIGRATED},
                {"MIGRATION_COMPLETED",        Exchange::IMigration::MIGRATION_STATUS_MIGRATION_COMPLETED}
            };

            auto it = stringToStatus.find(status);
            if (it != stringToStatus.end()) {
                migrationStatus = it->second;
            }

            auto migrationObject = m_shellService->QueryInterfaceByCallsign<Exchange::IMigration>("org.rdk.Migration");

            if (migrationObject)
            {
                result = migrationObject->SetMigrationStatus(migrationStatus, migrationResult);
                if (Core::ERROR_NONE != result)
                {
                    LOGERR("Failed to set migration status\n");
                }
                success = migrationResult.success;
                migrationObject->Release();
            }
            else
            {
                LOGERR("Migration plugin is not activated\n");
            }

            return result;
        }

        Core::hresult SystemServicesImplementation::GetMigrationStatus(MigrationStatus &migrationInfo)
        {
            uint32_t result = Core::ERROR_GENERAL;

            auto migrationObject = m_shellService->QueryInterfaceByCallsign<Exchange::IMigration>("org.rdk.Migration");

            if (migrationObject)
            {
                Exchange::IMigration::MigrationStatusInfo migrationStatusInfo;

                result = migrationObject->GetMigrationStatus(migrationStatusInfo);

                if (result == Core::ERROR_NONE)
                {
                    static const std::unordered_map<Exchange::IMigration::MigrationStatus, std::string> statusToString = {
                        {Exchange::IMigration::MIGRATION_STATUS_NOT_STARTED,                "NOT_STARTED"},
                        {Exchange::IMigration::MIGRATION_STATUS_NOT_NEEDED,                 "NOT_NEEDED"},
                        {Exchange::IMigration::MIGRATION_STATUS_STARTED,                    "STARTED"},
                        {Exchange::IMigration::MIGRATION_STATUS_PRIORITY_SETTINGS_MIGRATED, "PRIORITY_SETTINGS_MIGRATED"},
                        {Exchange::IMigration::MIGRATION_STATUS_DEVICE_SETTINGS_MIGRATED,   "DEVICE_SETTINGS_MIGRATED"},
                        {Exchange::IMigration::MIGRATION_STATUS_CLOUD_SETTINGS_MIGRATED,    "CLOUD_SETTINGS_MIGRATED"},
                        {Exchange::IMigration::MIGRATION_STATUS_APP_DATA_MIGRATED,          "APP_DATA_MIGRATED"},
                        {Exchange::IMigration::MIGRATION_STATUS_MIGRATION_COMPLETED,        "MIGRATION_COMPLETED"}
                    };

                    auto it = statusToString.find(migrationStatusInfo.migrationStatus);
                    if (it != statusToString.end())
                    {
                        migrationInfo.migrationStatus = it->second;
                    }
                }
                migrationObject->Release();
            }
            else
            {
                LOGERR("Migration plugin is not activated\n");
            }

            LOGINFO("response: migrationStatus=%s", migrationInfo.migrationStatus.c_str());
            return result;
        }

        Core::hresult SystemServicesImplementation::GetBootTypeInfo(BootType &bootInfo)
        {
            uint32_t result = Core::ERROR_GENERAL;

            auto migrationObject = m_shellService->QueryInterfaceByCallsign<Exchange::IMigration>("org.rdk.Migration");

            if (migrationObject)
            {
                Exchange::IMigration::BootTypeInfo bootTypeInfo;

                result = migrationObject->GetBootTypeInfo(bootTypeInfo);

                if (result == Core::ERROR_NONE)
                {
                    static const std::unordered_map<Exchange::IMigration::BootType, std::string> bootTypeToString = {
                        {Exchange::IMigration::BOOT_TYPE_INIT,      "BOOT_INIT"},
                        {Exchange::IMigration::BOOT_TYPE_NORMAL,    "BOOT_NORMAL"},
                        {Exchange::IMigration::BOOT_TYPE_MIGRATION, "BOOT_MIGRATION"},
                        {Exchange::IMigration::BOOT_TYPE_UPDATE,    "BOOT_UPDATE"}
                    };

                    auto it = bootTypeToString.find(bootTypeInfo.bootType);
                    if (it != bootTypeToString.end())
                    {
                        bootInfo.bootType = it->second;
                    }
                    
                    char value[256] = {0};
                    snprintf(value, sizeof(value), "\"bootType\":\"%s\"", bootInfo.bootType.c_str());
                    t2_event_s((char*)"WPE_INFO_MigBoottype_split", value);
                }
                migrationObject->Release();
            }
            else
            {
                LOGERR("Migration plugin is not activated\n");
            }

            LOGINFO("response: bootType=%s", bootInfo.bootType.c_str());
            return result;
        }

        Core::hresult SystemServicesImplementation::GetSerialNumber(string& serialNumber, bool& success)
        {
            uint32_t result = Core::ERROR_GENERAL;
            Exchange::IDeviceInfo::DeviceSerialNo deviceSerialNo;

            auto deviceInfoObject = m_shellService->QueryInterfaceByCallsign<Exchange::IDeviceInfo>("DeviceInfo");

            if (deviceInfoObject)
            {
                result = deviceInfoObject->SerialNumber(deviceSerialNo);
                if (Core::ERROR_NONE == result)
                {
                    serialNumber = deviceSerialNo.serialnumber;
                    success = true;
                }
                deviceInfoObject->Release();
            }
            else
            {
                LOGERR("DeviceInfo plugin is not activated\n");
            }

            LOGINFO("response: serialNumber=%s, success=%s", serialNumber.c_str(), success ? "true" : "false");
            return result;
        }

#if defined(HAS_API_SYSTEM) && defined(HAS_API_POWERSTATE)
        Core::hresult SystemServicesImplementation::GetPowerState (string& powerState, bool& success)
        {
            PowerState pwrStateCur = WPEFramework::Exchange::IPowerManager::POWER_STATE_UNKNOWN;
            PowerState pwrStatePrev = WPEFramework::Exchange::IPowerManager::POWER_STATE_UNKNOWN;
            powerState= "UNKNOWN";
            Core::hresult retStatus = Core::ERROR_GENERAL;

            ASSERT (_powerManagerPlugin);
            if (_powerManagerPlugin){
                retStatus = _powerManagerPlugin->GetPowerState(pwrStateCur, pwrStatePrev);
            }


            if (Core::ERROR_NONE == retStatus){
                if (pwrStateCur == WPEFramework::Exchange::IPowerManager::POWER_STATE_ON)
                    powerState = "ON";
                else if ((pwrStateCur == WPEFramework::Exchange::IPowerManager::POWER_STATE_STANDBY) || (pwrStateCur == WPEFramework::Exchange::IPowerManager::POWER_STATE_STANDBY_LIGHT_SLEEP) || (pwrStateCur == WPEFramework::Exchange::IPowerManager::POWER_STATE_STANDBY_DEEP_SLEEP))
                    powerState = "STANDBY";
            }

            LOGWARN("GetPowerState called, power state : %s\n", powerState.c_str());

            if (powerState != "UNKNOWN") {
                success = true;
            }

            return retStatus;
        }

        Core::hresult SystemServicesImplementation::SetPowerState(const string &powerState, const string &standbyReason, uint32_t& SysSrv_Status, string& errorMessage, bool& success)
        {
            LOGINFO("powerState=%s, standbyReason=%s", powerState.c_str(), standbyReason.c_str());
            bool retVal = false;
            string sleepMode;
            ofstream outfile;

            if (!powerState.empty()) {
                /* Power state defaults standbyReason is "application". */
                const std::string reason = standbyReason.empty() ? "application" : standbyReason;
                LOGINFO("SystemServicesImplementation::SetPowerState powerState: %s, standbyReason: %s\n", powerState.c_str(), reason.c_str());

                if (powerState == "LIGHT_SLEEP" || powerState == "DEEP_SLEEP") {
                    const device::SleepMode &mode = device::Host::getInstance().getPreferredSleepMode();
                    sleepMode = mode.toString();
                    LOGWARN("Output of getPreferredSleepMode: '%s'", sleepMode.c_str());

                    if (convert("DEEP_SLEEP", sleepMode)) {
                        retVal = setPowerStateConversion(std::move(sleepMode));
                    } else {
                        retVal = setPowerStateConversion(powerState);
                    }

                    outfile.open(STANDBY_REASON_FILE, ios::out);
                    if (outfile.is_open()) {
                        outfile << reason;
                        outfile.close();
                    } else {
                        LOGERR("Can't open file '%s' for write mode\n", STANDBY_REASON_FILE);
                        populateResponseWithError(SysSrv_FileAccessFailed, SysSrv_Status, errorMessage);
                    }

                } else {
                    retVal = setPowerStateConversion(powerState);
                }
                m_current_state = powerState;
            } else {
                populateResponseWithError(SysSrv_MissingKeyValues, SysSrv_Status, errorMessage);
            }
            success = retVal;

            return Core::ERROR_NONE;
        }

        bool SystemServicesImplementation::setPowerStateConversion(std::string powerState)
        {
            Core::hresult status = Core::ERROR_GENERAL;
            WPEFramework::Exchange::IPowerManager::PowerState pwrMgrState;
            int keyCode = 0;

            if (powerState == "STANDBY") {
                pwrMgrState = WPEFramework::Exchange::IPowerManager::POWER_STATE_STANDBY;
            } else if (powerState == "ON") {
                pwrMgrState = WPEFramework::Exchange::IPowerManager::POWER_STATE_ON;
            } else if (powerState == "DEEP_SLEEP") {
                pwrMgrState = WPEFramework::Exchange::IPowerManager::POWER_STATE_STANDBY_DEEP_SLEEP;
            } else if (powerState == "LIGHT_SLEEP") {
                pwrMgrState = WPEFramework::Exchange::IPowerManager::POWER_STATE_STANDBY;
            } else {
                return false;
            }

            ASSERT (_powerManagerPlugin);

            if (_powerManagerPlugin) {
                status = _powerManagerPlugin->SetPowerState(keyCode, pwrMgrState, "random");
            }

            if (status == Core::ERROR_GENERAL)
                return false;
            else
                return true;
        }

#endif /* HAS_API_SYSTEM && HAS_API_POWERSTATE */

        Core::hresult SystemServicesImplementation::GetNetworkStandbyMode(bool& nwStandby, bool& success)
        {
            Core::hresult retStatus = Core::ERROR_GENERAL;
            
            if (m_networkStandbyModeValid) {
                success = true;
                nwStandby = m_networkStandbyMode;
                LOGINFO("Got cached NetworkStandbyMode: '%s'", m_networkStandbyMode ? "true" : "false");
            }
            else {
                ASSERT (_powerManagerPlugin);
                if (_powerManagerPlugin){
                    retStatus = _powerManagerPlugin->GetNetworkStandbyMode(nwStandby);
                }

                LOGWARN("getNetworkStandbyMode called, current NwStandbyMode is: %s\n",
                         nwStandby?("Enabled"):("Disabled"));
                if (Core::ERROR_NONE == retStatus) {
                    success = true;
                    m_networkStandbyMode = nwStandby;
                    m_networkStandbyModeValid = true;
                } else {
                    success = false;
                }
            }

            LOGINFO("response: nwStandby=%s, success=%s", nwStandby ? "true" : "false", success ? "true" : "false");
            return Core::ERROR_NONE;
        }
        
        Core::hresult SystemServicesImplementation::GetLastFirmwareFailureReason(string& failReason, bool& success)
        {
            FwFailReason fwFailReason = FwFailReasonNone;

            std::vector<string> lines;
            if (getFileContent(FWDNLDSTATUS_FILE_NAME, lines)) {
                std::string str;
                for (auto i = lines.begin(); i != lines.end(); ++i) {
                    std::smatch m;
                    if (std::regex_match(*i, m, std::regex("^FailureReason\\|(.*)$"))) {
                        str = m.str(1);
                    }
                }

                LOGINFO("Lines read:%d. FailureReason|%s", (int) lines.size(), C_STR(str));

                auto it = find_if(FwFailReasonFromText.begin(), FwFailReasonFromText.end(),
                                  [&str](const pair<string, FwFailReason> & t) {
                                      return strcasecmp(C_STR(t.first), C_STR(str)) == 0;
                                  });
                if (it != FwFailReasonFromText.end())
                    fwFailReason = it->second;
                else if (!str.empty())
                    LOGWARN("Unrecognised FailureReason!");
            } else {
                LOGINFO("Could not read file %s", FWDNLDSTATUS_FILE_NAME);
            }

            failReason = FwFailReasonToText.at(fwFailReason);
            std::string failReasonStr = FwFailReasonToText.at(fwFailReason);
            char value[256] = {0};
            snprintf(value, sizeof(value), "getLastFirmwareFailureReason: response=%s", failReasonStr.c_str());
            t2_event_s((char*)"FWFail_split", value);
            success = true;
            LOGINFO("response: failReason=%s, success=%s", failReason.c_str(), success ? "true" : "false");
            return Core::ERROR_NONE;
        }
        
        void SystemServicesImplementation::reportFirmwareUpdateInfoReceived(string firmwareUpdateVersion, int httpStatus, bool success, string firmwareVersion, string responseString)
        {
            JsonObject params;
            params["status"] = httpStatus;
            params["responseString"] = responseString.c_str();
            params["rebootImmediately"] = false;

            JsonObject xconfResponse;
            if(!responseString.empty() && xconfResponse.FromString(responseString))
            {
                params["rebootImmediately"] = xconfResponse["rebootImmediately"];
            }

            if(httpStatus == STATUS_CODE_NO_SWUPDATE_CONF)
            {
                // Empty /opt/swupdate.conf
                params["status"] = 0;
                params["updateAvailable"] = false;
                params["updateAvailableEnum"] = static_cast<int>(FWUpdateAvailableEnum::EMPTY_SW_UPDATE_CONF);
                params["success"] = true;
            }
            else if(httpStatus == 404)
            {
                // if XCONF server returns 404 there is no FW available to download
                params["updateAvailable"] = false;
                params["updateAvailableEnum"] = static_cast<int>(FWUpdateAvailableEnum::FW_MATCH_CURRENT_VER);
                params["success"] = true;
            }
            else
            {
                FWUpdateAvailableEnum updateAvailableEnum = FWUpdateAvailableEnum::NO_FW_VERSION;
                bool bUpdateAvailable = false;
                if (firmwareUpdateVersion.length() > 0) {
                    params["firmwareUpdateVersion"] = firmwareUpdateVersion.c_str();
                    if (firmwareUpdateVersion.compare(firmwareVersion)) {
                        updateAvailableEnum = FWUpdateAvailableEnum::FW_UPDATE_AVAILABLE;
                        bUpdateAvailable = true;
                    } else {
                        updateAvailableEnum = FWUpdateAvailableEnum::FW_MATCH_CURRENT_VER;
                    }
                } else {
                    params["firmwareUpdateVersion"] = "";
                    updateAvailableEnum = FWUpdateAvailableEnum::NO_FW_VERSION;
                }
                params["updateAvailable"] = bUpdateAvailable ;
                params["updateAvailableEnum"] = static_cast<int>(updateAvailableEnum);
                params["success"] = success;
            }

            string jsonLog;
            params.ToString(jsonLog);
            LOGWARN("result: %s\n", jsonLog.c_str());
            dispatchEvent(SYSTEMSERVICES_EVT_ONFIRMWAREUPDATEINFORECEIVED, params);
        }
        
        void SystemServicesImplementation::firmwareUpdateInfoReceived(void)
        {
            string env = "";
            string firmwareVersion;
            if (_instance) {
                firmwareVersion = _instance->getStbVersionString();
            } else {
                LOGERR("_instance is NULL.\n");
            }

            LOGWARN("SystemService firmwareVersion %s\n", firmwareVersion.c_str());

            if (true == findCaseInsensitive(firmwareVersion, "DEV"))
                env = "DEV";
            else if (true == findCaseInsensitive(firmwareVersion, "VBN"))
                env = "VBN";
            else if (true == findCaseInsensitive(firmwareVersion, "PROD"))
                env = "PROD";
            else if (true == findCaseInsensitive(firmwareVersion, "CQA"))
                env = "CQA";

            std::string response;
            firmwareUpdate _fwUpdate;
            
            _fwUpdate.success = false;
            _fwUpdate.httpStatus = 0;

            bool bFileExists = false;
            string xconfOverride; 
            if(env != "PROD")
            {
                xconfOverride = getXconfOverrideUrl(bFileExists);
                if(bFileExists && xconfOverride.empty())
                {
                    // empty /opt/swupdate.conf. Don't initiate FW download
                    LOGWARN("Empty /opt/swupdate.conf. Skipping FW upgrade check with xconf");
                    if (_instance) {
                        _instance->reportFirmwareUpdateInfoReceived("",
                        STATUS_CODE_NO_SWUPDATE_CONF, true, "", std::move(response));
                    }
                    return;
                }
            }

            v_secure_system("/lib/rdk/xconfImageCheck.sh  >> /opt/logs/wpeframework.log");

            //get xconf http code
            string httpCodeStr ="";
            const char* httpCodeFile = "/tmp/xconf_httpcode_thunder.txt";
            bool httpCodeReadSuccess =  Utils::readFileContent(httpCodeFile, httpCodeStr);

            if(httpCodeReadSuccess)
            {
                LOGINFO("xconf httpCodeStr '%s'\n", httpCodeStr.c_str());
                try
                {
                    _fwUpdate.httpStatus = std::stoi(httpCodeStr);
                }
                catch(const std::exception& e)
                {
                    LOGERR("exception in converting xconf http code %s", e.what());
                }
            }

            LOGINFO("xconf http code %d\n", _fwUpdate.httpStatus);

            const char* responseFile = "/tmp/xconf_response_thunder.txt";
            bool responseReadSuccess = Utils::readFileContent(responseFile, response);

            if(responseReadSuccess)
            {
                JsonObject httpResp;
                if(httpResp.FromString(response))
                {
                    if(httpResp.HasLabel("firmwareVersion"))
                    {
                        _fwUpdate.firmwareUpdateVersion = httpResp["firmwareVersion"].String();
                        LOGWARN("fwVersion: '%s'\n", _fwUpdate.firmwareUpdateVersion.c_str());
                        _fwUpdate.success = true;
                    }
                    else
                    {
                        LOGERR("Xconf response is not valid json and/or doesn't contain firmwareVersion. '%s'\n", response.c_str());
                        response = "";
                    }
                }
                else
                {
                    LOGERR("Error in parsing xconf json response");
                }
                 
            }
            else
            {
                LOGERR("Unable to open xconf response file");
            }
            

            if (_instance) {
                _instance->reportFirmwareUpdateInfoReceived(std::move(_fwUpdate.firmwareUpdateVersion),
                        _fwUpdate.httpStatus, _fwUpdate.success, std::move(firmwareVersion), std::move(response));
            } else {
                LOGERR("_instance is NULL.\n");
            }
        }
        
        Core::hresult SystemServicesImplementation::GetFirmwareUpdateInfo(const string& GUID, bool &asyncResponse, bool& success)
        {
            LOGINFO("GUID=%s", GUID.c_str());
            try
            {
                if (m_getFirmwareInfoThread.get().joinable()) {
                    m_getFirmwareInfoThread.get().join();
                }
                m_getFirmwareInfoThread = Utils::ThreadRAII(std::thread(firmwareUpdateInfoReceived));
                asyncResponse = true;
                success = true;
                LOGINFO("response: asyncResponse=%s, success=%s", asyncResponse ? "true" : "false", success ? "true" : "false");
                return Core::ERROR_NONE;
            }
            catch(const std::system_error& e)
            {
                LOGERR("exception in GetFirmwareUpdateInfo %s", e.what());
                asyncResponse = false;
                success = false;
                LOGINFO("response: asyncResponse=%s, success=%s", asyncResponse ? "true" : "false", success ? "true" : "false");
                return Core::ERROR_GENERAL;
            }
        }
        
        Core::hresult SystemServicesImplementation::GetFirmwareDownloadPercent(int32_t& downloadPercent, bool& success)
        {
            int m_downloadPercent = -1;

            if (Utils::fileExists(DOWNLOAD_PROGRESS_FILE))
            {
                if (true == getDownloadProgress(m_downloadPercent))
                {
                    success = true;
                }
                else
                {
                    LOGERR("getDownloadProgress() failed");
                }
                downloadPercent = m_downloadPercent;
            }
            else
            {
                downloadPercent = -1;
                success = true;
            }
            LOGINFO("response: downloadPercent=%d, success=%s", downloadPercent, success ? "true" : "false");
            return Core::ERROR_NONE;
        }
        
        Core::hresult SystemServicesImplementation::GetDownloadedFirmwareInfo(DownloadedFirmwareInfo& downloadedFirmwareInfo)
        {
            string downloadedFWVersion = "";
            string downloadedFWLocation = "";
            bool isRebootDeferred = false;
            std::vector<string> lines;

            if (!Utils::fileExists(FWDNLDSTATUS_FILE_NAME)) {
                //If firmware download file doesn't exist we can still return the current version
                downloadedFirmwareInfo.downloadedFWVersion = std::move(downloadedFWVersion);
                downloadedFirmwareInfo.downloadedFWLocation = std::move(downloadedFWLocation);
                downloadedFirmwareInfo.isRebootDeferred = isRebootDeferred;
                downloadedFirmwareInfo.success = true;
                string ver =  getStbVersionString();
                if(ver == "unknown")
                {
                    downloadedFirmwareInfo.currentFWVersion = "";
                    downloadedFirmwareInfo.success = false;
                }
                else
                {
                    downloadedFirmwareInfo.currentFWVersion = std::move(ver);
                    downloadedFirmwareInfo.success = true;
                }
            }
            else if (getFileContent(FWDNLDSTATUS_FILE_NAME, lines)) {
                for (std::vector<std::string>::const_iterator i = lines.begin();
                        i != lines.end(); ++i) {
                    std::string line = *i;
                    std::string delimiter = "|";
                    size_t pos = 0;
                    std::string token;

                    std::size_t found = line.find("Reboot|");
                    if (std::string::npos != found) {
                        while ((pos = line.find(delimiter)) != std::string::npos) {
                            token = line.substr(0, pos);
                            line.erase(0, pos + delimiter.length());
                        }
                        line = std::regex_replace(line, std::regex("^ +| +$"), "$1");
                        if (line.length() > 1) {
                            if (!((strncasecmp(line.c_str(), "1", strlen("1")))
                                        && (strncasecmp(line.c_str(), "yes", strlen("yes")))
                                        && (strncasecmp(line.c_str(), "true", strlen("true"))))) {
                                isRebootDeferred = true;
                            }
                        }
                    }
                    // return DnldVersn based on IARM Firmware Update State
                    // If Firmware Update State is Downloading or above then 
                    // return DnldVersion from FWDNLDSTATUS_FILE_NAME else return empty
                    if(m_FwUpdateState_LatestEvent >=2)
                    {
                        found = line.find("DnldVersn|");
                        if (std::string::npos != found) {
                            while ((pos = line.find(delimiter)) != std::string::npos) {
                                token = line.substr(0, pos);
                                line.erase(0, pos + delimiter.length());
                            }
                            line = std::regex_replace(line, std::regex("^ +| +$"), "$1");
                            if (line.length() > 1) {
                                downloadedFWVersion = line.c_str();
                            }
                        }
                        found = line.find("DnldURL|");
                        if (std::string::npos != found) {
                            while ((pos = line.find(delimiter)) != std::string::npos) {
                                token = line.substr(0, pos);
                                line.erase(0, pos + delimiter.length());
                            }
                            line = std::regex_replace(line, std::regex("^ +| +$"), "$1");
                            if (line.length() > 1) {
                                downloadedFWLocation = line.c_str();
                            }
                        }
                    }
                }
                downloadedFirmwareInfo.currentFWVersion = getStbVersionString();
                downloadedFirmwareInfo.downloadedFWVersion = std::move(downloadedFWVersion);
                downloadedFirmwareInfo.downloadedFWLocation = std::move(downloadedFWLocation);
                downloadedFirmwareInfo.isRebootDeferred = isRebootDeferred;
                downloadedFirmwareInfo.success = true;
            } else {
                populateResponseWithError(SysSrv_FileContentUnsupported, downloadedFirmwareInfo.sysSrvStatus, downloadedFirmwareInfo.errorMessage);
            }
            LOGINFO("response: currentFWVersion=%s, downloadedFWVersion=%s, downloadedFWLocation=%s, isRebootDeferred=%s, success=%s",
                downloadedFirmwareInfo.currentFWVersion.c_str(), downloadedFirmwareInfo.downloadedFWVersion.c_str(), downloadedFirmwareInfo.downloadedFWLocation.c_str(), downloadedFirmwareInfo.isRebootDeferred ? "true" : "false", downloadedFirmwareInfo.success ? "true" : "false");
            return Core::ERROR_NONE;
        }
        
        Core::hresult SystemServicesImplementation::GetFirmwareUpdateState(int& firmwareUpdateState, bool& success)
        {
            FirmwareUpdateState fwUpdateState =(FirmwareUpdateState)m_FwUpdateState_LatestEvent;
            firmwareUpdateState = (int)fwUpdateState;
            success = true;
            LOGINFO("response: firmwareUpdateState=%d, success=%s", firmwareUpdateState, success ? "true" : "false");
            return Core::ERROR_NONE;
        }
        
        Core::hresult SystemServicesImplementation::GetPowerStateBeforeReboot(string& state, bool& success)
        {
            PowerState pwrStateBeforeReboot = WPEFramework::Exchange::IPowerManager::POWER_STATE_UNKNOWN;

            if (m_powerStateBeforeRebootValid) {
                state = m_powerStateBeforeReboot;
                success = true;
                LOGINFO("Got cached powerStateBeforeReboot: '%s'", m_powerStateBeforeReboot.c_str());
            } else {
                Core::hresult retStatus = Core::ERROR_GENERAL;
                ASSERT (_powerManagerPlugin);
                if (_powerManagerPlugin){
                    retStatus = _powerManagerPlugin->GetPowerStateBeforeReboot(pwrStateBeforeReboot);
                }
                LOGWARN("GetPowerStateBeforeReboot called, current powerStateBeforeReboot is: %d\n",
                         pwrStateBeforeReboot);
                state = powerModeEnumToString(pwrStateBeforeReboot);

                if (Core::ERROR_NONE == retStatus){
                    success = true;
                    m_powerStateBeforeReboot = state;
                    m_powerStateBeforeRebootValid = true;
                } else {
                    success = false;
                }
            }

            LOGINFO("response: state=%s, success=%s", state.c_str(), success ? "true" : "false");
            return Core::ERROR_NONE;
        }

#ifdef ENABLE_DEEP_SLEEP
        Core::hresult SystemServicesImplementation::GetWakeupReason(string& wakeupReason, bool& success)
        {
            Core::hresult retStatus = Core::ERROR_GENERAL;
            WakeupReason param = WPEFramework::Exchange::IPowerManager::WAKEUP_REASON_UNKNOWN;
            wakeupReason = "WAKEUP_REASON_UNKNOWN";

            ASSERT (_powerManagerPlugin);
            if (_powerManagerPlugin){
                retStatus = _powerManagerPlugin->GetLastWakeupReason(param);
            }


            if (Core::ERROR_NONE == retStatus)
            {
                success = true;
                wakeupReason = getWakeupReasonString(param);
            }
            else
            {
                success = false;
            }
            LOGWARN("WakeupReason : %s\n", wakeupReason.c_str());
            
            char value[256] = {0};
            snprintf(value, sizeof(value), "WakeupReason : %s", wakeupReason.c_str());
            t2_event_s((char*)"WakeUPRsn_split", value);

            LOGINFO("response: wakeupReason=%s, success=%s", wakeupReason.c_str(), success ? "true" : "false");
            return Core::ERROR_NONE;
        }

        Core::hresult SystemServicesImplementation::GetLastWakeupKeyCode(int& wakeupKeyCode, bool& success)
        {
            Core::hresult retStatus = Core::ERROR_GENERAL;
            bool status = false;
            int keyCode = 0;

            ASSERT (_powerManagerPlugin);
            if (_powerManagerPlugin){
                retStatus = _powerManagerPlugin->GetLastWakeupKeyCode(keyCode);
            }

            if (Core::ERROR_NONE == retStatus)
            {
                status = true;
                wakeupKeyCode = keyCode;
            }
            else
            {
                status = false;
            }

            LOGWARN("WakeupKeyCode : %d\n", wakeupKeyCode);

            success = status;

            LOGINFO("response: wakeupKeyCode=%d, success=%s", wakeupKeyCode, success ? "true" : "false");
            return Core::ERROR_NONE;
        }


        std::string SystemServicesImplementation::getWakeupReasonString(WakeupReason reason)
        {
            std::string reasonString = "";
            switch (reason) 
            {
                case WPEFramework::Exchange::IPowerManager::WAKEUP_REASON_IR: reasonString = "WAKEUP_REASON_IR"; break;
                case WPEFramework::Exchange::IPowerManager::WAKEUP_REASON_BLUETOOTH : reasonString = "WAKEUP_REASON_RCU_BT"; break;
                case WPEFramework::Exchange::IPowerManager::WAKEUP_REASON_RF4CE : reasonString = "WAKEUP_REASON_RCU_RF4CE"; break;
                case WPEFramework::Exchange::IPowerManager::WAKEUP_REASON_GPIO : reasonString = "WAKEUP_REASON_GPIO"; break;
                case WPEFramework::Exchange::IPowerManager::WAKEUP_REASON_LAN : reasonString = "WAKEUP_REASON_LAN"; break;
                case WPEFramework::Exchange::IPowerManager::WAKEUP_REASON_WIFI : reasonString = "WAKEUP_REASON_WLAN"; break;
                case WPEFramework::Exchange::IPowerManager::WAKEUP_REASON_TIMER : reasonString = "WAKEUP_REASON_TIMER"; break;
                case WPEFramework::Exchange::IPowerManager::WAKEUP_REASON_FRONTPANEL : reasonString = "WAKEUP_REASON_FRONT_PANEL"; break;
                case WPEFramework::Exchange::IPowerManager::WAKEUP_REASON_WATCHDOG : reasonString = "WAKEUP_REASON_WATCHDOG"; break;
                case WPEFramework::Exchange::IPowerManager::WAKEUP_REASON_SOFTWARERESET : reasonString = "WAKEUP_REASON_SOFTWARE_RESET"; break;
                case WPEFramework::Exchange::IPowerManager::WAKEUP_REASON_THERMALRESET : reasonString = "WAKEUP_REASON_THERMAL_RESET"; break;
                case WPEFramework::Exchange::IPowerManager::WAKEUP_REASON_WARMRESET : reasonString = "WAKEUP_REASON_WARM_RESET"; break;
                case WPEFramework::Exchange::IPowerManager::WAKEUP_REASON_COLDBOOT : reasonString = "WAKEUP_REASON_COLDBOOT"; break;
                case WPEFramework::Exchange::IPowerManager::WAKEUP_REASON_STRAUTHFAIL : reasonString = "WAKEUP_REASON_STR_AUTH_FAILURE"; break;
                case WPEFramework::Exchange::IPowerManager::WAKEUP_REASON_CEC : reasonString = "WAKEUP_REASON_CEC"; break;
                case WPEFramework::Exchange::IPowerManager::WAKEUP_REASON_PRESENCE : reasonString = "WAKEUP_REASON_PRESENCE"; break;
                case WPEFramework::Exchange::IPowerManager::WAKEUP_REASON_VOICE : reasonString = "WAKEUP_REASON_VOICE"; break;
                default: break;
            }
            return reasonString;
        }

#endif
        string SystemServicesImplementation::safeExtractAfterColon(const std::string& inputLine) {
            size_t pos = inputLine.find(':');
            if ((pos != std::string::npos) && (pos + 1 < inputLine.length())) {
                return inputLine.substr(pos + 1);
            } else {
                LOGERR("Territory file corrupted");  
            }
            return "";
	    }

	    bool SystemServicesImplementation::readTerritoryFromFile()
	    {
		    bool retValue = true;
            try{
		        if(Utils::fileExists(TERRITORYFILE)){
			        ifstream inFile(TERRITORYFILE);
			        string str;
			        getline (inFile, str);
			        if(str.length() > 0){
			    	    retValue = true;
			    	    m_strTerritory = safeExtractAfterColon(str);
			    	    int index = m_strStandardTerritoryList.find(m_strTerritory);
			    	    if((m_strTerritory.length() == 3) && (index >=0 && index <= 1100) ){
			    		    getline (inFile, str);
			    		    if(str.length() > 0){
			    		        m_strRegion = safeExtractAfterColon(str);
			    		        if(!isRegionValid(m_strRegion))
			    		        {
			    			        m_strTerritory = "";
			    			        m_strRegion = "";
			    			        LOGERR("Territory file corrupted  - region : %s",m_strRegion.c_str());
			    			        LOGERR("Returning empty values");
			    		        }
			    		    }
			    	    }
			    	    else{
			    		    m_strTerritory = "";
			    		    m_strRegion = "";
			    		    LOGERR("Territory file corrupted - territory : %s",m_strTerritory.c_str());
			    		    LOGERR("Returning empty values");
			    	    }
			        }
			        else{
			    	    LOGERR("Invalid territory file");
			        }
			        inFile.close();
            
		        }else{
		    	    LOGERR("Territory is not set");
		    	    t2_event_d((char*)"SYST_ERR_TerritoryNotSet", 1);
		        }
            }
            catch(...){
                LOGERR("Exception caught while reading territory file");
                retValue = false;
                m_strTerritory = "";
                m_strRegion = "";
            }
		    return retValue;
	    }

	    bool SystemServicesImplementation::isStrAlphaUpper(string strVal)
	    {
		    try{
			    long unsigned int i=0;
			    for(i=0; i<= strVal.length()-1; i++)
			    {
				    if((isalpha(strVal[i])== 0) || (isupper(strVal[i])==0))
				    {
					    LOGERR(" -- Invalid Territory ");
					    return false;
					    break;
				    }
			    }
		    }
		    catch(...){
			    LOGERR(" Exception caught");
			    return false;
		    }
		    return true;
	    }

	    bool SystemServicesImplementation::isRegionValid(string regionStr)
	    {
		    bool retVal = false;
		    if(regionStr.length() < 7){
			    string strRegion = regionStr.substr(0,regionStr.find("-"));
			    if( strRegion.length() == 2){
				    // Coverity Fix: ID 16, 31 - COPY_INSTEAD_OF_MOVE
				    if (isStrAlphaUpper(strRegion)){
					    strRegion = regionStr.substr(regionStr.find("-")+1,regionStr.length());
					    if(strRegion.length() >= 2){
						    retVal = isStrAlphaUpper(std::move(strRegion));
					    }
				    }
			    }
		    }
		    return retVal;
	    }

        Core::hresult SystemServicesImplementation::GetTerritory(string& territory , string& region, bool& success)
	    {
            bool resp = true;
            const std::lock_guard<std::mutex> lock(m_territoryMutex);
            m_strTerritory = "";
            m_strRegion = "";
            resp = readTerritoryFromFile();
            territory = m_strTerritory;
            region = m_strRegion;
            success = resp;
            LOGINFO("response: territory=%s, region=%s, success=%s", territory.c_str(), region.c_str(), success ? "true" : "false");
            return Core::ERROR_NONE;
	    }

	    Core::hresult SystemServicesImplementation::GetTimeZoneDST(string& timeZone, string& accuracy, bool& success)
        {
            bool resp = false;

            if (Utils::fileExists(TZ_FILE)) {
                if(readFromFile(TZ_FILE, timeZone)) {
                    LOGWARN("Fetch TimeZone: %s\n", timeZone.c_str());
                    resp = true;
                } else {
                    LOGERR("Unable to open %s file.\n", TZ_FILE);
                    timeZone = "null";
                    resp = false;
                }
            } else {
                LOGERR("File not found %s, returning default.\n", TZ_FILE);
                timeZone = TZ_DEFAULT;
                resp = true;
            }

            if (resp) {
                accuracy = getTimeZoneAccuracyDSTHelper();
            }

            success = resp;
            LOGINFO("response: timeZone=%s, accuracy=%s, success=%s", timeZone.c_str(), accuracy.c_str(), success ? "true" : "false");
            return Core::ERROR_NONE;
        }
        
        Core::hresult SystemServicesImplementation::Reboot(const string& rebootReason, int& IARM_Bus_Call_STATUS, bool& success)
        {
            LOGINFO("rebootReason=%s", rebootReason.c_str());
            Core::hresult status = Core::ERROR_GENERAL;
            bool nfxResult = false;
            string customReason = "No custom reason provided";
            string otherReason = "No other reason supplied";
            string requestor = "SystemServices";
            bool result = false;
            string fname = "nrdPluginApp";

            nfxResult = Utils::killProcess(fname);
            if (true == nfxResult) {
                LOGINFO("SystemService shutting down Netflix...\n");
                //give Netflix process some time to terminate gracefully.
                sleep(10);
            } else {
                LOGINFO("SystemService unable to shutdown Netflix \
                        process. nfxResult = %ld\n", (long int)nfxResult);
            }

            if (!rebootReason.empty()) {
                customReason = rebootReason;
                otherReason = customReason;
            }

            LOGINFO("Reboot: custom reason: %s, other reason: %s\n", customReason.c_str(),
                otherReason.c_str());

            ASSERT (_powerManagerPlugin);
            if (_powerManagerPlugin){
                status = _powerManagerPlugin->Reboot(requestor, customReason, otherReason);
                result = true;
            } else {
                status = Core::ERROR_ILLEGAL_STATE;
            }

            if (status != Core::ERROR_NONE){
                 LOGWARN("Reboot: powerManagerPlugin->rebooot failed\n");
            }

            IARM_Bus_Call_STATUS = static_cast <int32_t> (status);
            
            success = result;

            LOGINFO("response: IARM_Bus_Call_STATUS=%d, success=%s", IARM_Bus_Call_STATUS, success ? "true" : "false");
            return status;
        }
        
        Core::hresult SystemServicesImplementation::RequestSystemUptime(string& systemUptime, bool& success)
        {
            struct timespec time;
            bool result = false;

            if (clock_gettime(CLOCK_MONOTONIC_RAW, &time) == 0)
            {
                float uptime = (float)time.tv_sec + (float)time.tv_nsec / 1e9;
                std::string value = std::to_string(uptime);
                value = value.erase(value.find_last_not_of("0") + 1);

                if (value.back() == '.')
                    value += '0';

                systemUptime = value;
                LOGINFO("uptime is %s seconds", value.c_str());
                result = true;
                }
            else
                LOGERR("unable to evaluate uptime by clock_gettime");

            success = result;
            
            LOGINFO("response: systemUptime=%s, success=%s", systemUptime.c_str(), success ? "true" : "false");
            return Core::ERROR_NONE;
        }
        
        Core::hresult SystemServicesImplementation::GetSystemVersions(SystemVersionsInfo& systemVersionsInfo)
        {
	        systemVersionsInfo.stbVersion      = getStbVersionString();
	        string  stbBranchString     = getStbBranchString();            
            std::regex stbBranchString_regex("^[0-9]{1,3}\\.[0-9]{1,3}\\.[0-9]{1,3}\\.[0-9]{1,3}\\.[0-9]{1,3}$");
            if (std::regex_match (stbBranchString, stbBranchString_regex))
            {             
                systemVersionsInfo.receiverVersion = std::move(stbBranchString);
            }
            else
            {                    
                systemVersionsInfo.receiverVersion = getClientVersionString();
            }

            systemVersionsInfo.stbTimestamp = getStbTimestampString();

            systemVersionsInfo.success = true;
            LOGINFO("response: stbVersion=%s, receiverVersion=%s, stbTimestamp=%s, success=%s",
                systemVersionsInfo.stbVersion.c_str(), systemVersionsInfo.receiverVersion.c_str(), systemVersionsInfo.stbTimestamp.c_str(), systemVersionsInfo.success ? "true" : "false");
            return Core::ERROR_NONE;
        }
        
#ifdef ENABLE_SYSTIMEMGR_SUPPORT
        Core::hresult SystemServicesImplementation::GetTimeStatus(string& TimeQuality, string& TimeSrc, string& Time, bool& success)
        {
            IARM_Result_t ret = IARM_RESULT_SUCCESS;
            TimerMsg param;
            ret = IARM_Bus_Call(IARM_BUS_SYSTIME_MGR_NAME, TIMER_STATUS_MSG, (void*)&param, sizeof(param));
            if (ret != IARM_RESULT_SUCCESS ) {
              LOGWARN ("Query to get Timer Status Failed..\n");
              return Core::ERROR_GENERAL;
            }
           
            TimeQuality = std::string(param.message,cTIMER_STATUS_MESSAGE_LENGTH);
            TimeSrc = std::string(param.timerSrc,cTIMER_STATUS_MESSAGE_LENGTH);
            Time = std::string(param.currentTime,cTIMER_STATUS_MESSAGE_LENGTH);
            success = true;
            LOGINFO("response: TimeQuality=%s, TimeSrc=%s, Time=%s, success=%s", TimeQuality.c_str(), TimeSrc.c_str(), Time.c_str(), success ? "true" : "false");
            return Core::ERROR_NONE;
        }
#endif// ENABLE_SYSTIMEMGR_SUPPORT

        Core::hresult SystemServicesImplementation::SetDeepSleepTimer(const int seconds, uint32_t& SysSrv_Status, string& errorMessage, bool& success)
        {
            LOGINFO("seconds=%d", seconds);
            Core::hresult retStatus = Core::ERROR_GENERAL;

            if (seconds) {
                ASSERT (_powerManagerPlugin);
                if (_powerManagerPlugin){
		            int timeoutValue = seconds;
                    // if maintenence time is more then 10 days set to 0
                    if (( 0 > timeoutValue ) || ( 864000 < timeoutValue ))
                    {
                        timeoutValue = 0;
                        LOGINFO("setDeepSleepTimer updated timeout to :%d",timeoutValue);
                    }
                    retStatus = _powerManagerPlugin->SetDeepSleepTimer(timeoutValue);
                }

                if (Core::ERROR_NONE == retStatus) {
                    success = true;
                } else {
                    success = false;
                }
            } else {
                populateResponseWithError(SysSrv_MissingKeyValues, SysSrv_Status, errorMessage);
            }
            LOGINFO("response: SysSrv_Status=%u, errorMessage=%s, success=%s", SysSrv_Status, errorMessage.c_str(), success ? "true" : "false");
            return retStatus;
        }
        
        Core::hresult SystemServicesImplementation::SetFirmwareAutoReboot(const bool enable, SystemResult& result)
        {
            LOGINFO("enable=%d", enable);
            Core::hresult retStatus = Core::ERROR_GENERAL;
            Exchange::IFirmwareUpdate::Result res;

            auto firmwareupdateObject = m_shellService->QueryInterfaceByCallsign<Exchange::IFirmwareUpdate>("org.rdk.FirmwareUpdate");

            if (firmwareupdateObject)
            {
                retStatus = firmwareupdateObject->SetAutoReboot(enable, res);
                if (WPEFramework::Core::ERROR_NONE == retStatus)
                {
                    result.success = res.success;
                }
                firmwareupdateObject->Release();
            }
            else
            {
                 LOGERR("FirmwareUpdate plugin is not activated\n");
            }
            LOGINFO("response: success=%s", result.success ? "true" : "false");
            return retStatus;
        }
        
        Core::hresult SystemServicesImplementation::SetNetworkStandbyMode (const bool nwStandby, SystemResult& result)
        {
            LOGINFO("nwStandby=%d", nwStandby);
            bool status = false;
            Core::hresult retStatus = Core::ERROR_GENERAL;

            LOGWARN("setNetworkStandbyMode called, with NwStandbyMode : %s\n", (nwStandby)?("Enabled"):("Disabled"));

            if (nwStandby) {
               t2_event_d((char*)"SYST_INFO_NWenable", 1);
            } else {
               t2_event_d((char*)"SYST_INFO_NwDisable", 1);
            }

            ASSERT (_powerManagerPlugin);
            if (_powerManagerPlugin){
                retStatus = _powerManagerPlugin->SetNetworkStandbyMode(nwStandby);
            }

            if (Core::ERROR_NONE == retStatus) {
                status = true;
                m_networkStandbyModeValid = false;
            } else {
                status = false;
            }
            result.success = status;
            LOGINFO("response: success=%s", result.success ? "true" : "false");
            return retStatus;
        }
        
        Core::hresult SystemServicesImplementation::SetBootLoaderSplashScreen(const string& path, ErrorInfo& error, bool& success)
        {
            LOGINFO("path=%s", path.c_str());
            string strBLSplashScreenPath = path;
            bool fileExists = Utils::fileExists(strBLSplashScreenPath.c_str());
            if((strBLSplashScreenPath != "") && fileExists)
            {
                IARM_Bus_MFRLib_SetBLSplashScreen_Param_t mfrparam;
                std::strncpy(mfrparam.path, strBLSplashScreenPath.c_str(), sizeof(mfrparam.path));
                mfrparam.path[sizeof(mfrparam.path) - 1] = '\0';
                IARM_Result_t result = IARM_Bus_Call(IARM_BUS_MFRLIB_NAME, IARM_BUS_MFRLIB_API_SetBlSplashScreen, (void *)&mfrparam, sizeof(mfrparam));
                if (result != IARM_RESULT_SUCCESS){
                    LOGERR("Update failed. path: %s, fileExists %s, IARM result %d ",strBLSplashScreenPath.c_str(),fileExists ? "true" : "false",result);
                    error.message = "Update failed";
                    error.code = "-32002";
                    success = false;
                }
                else 
                {
                    LOGINFO("BootLoaderSplashScreen updated successfully");
                    success =true;
                }
            }
            else
            {
                LOGERR("Invalid path. path: %s, fileExists %s ",strBLSplashScreenPath.c_str(),fileExists ? "true" : "false");
                error.message = "Invalid path";
                error.code = "-32001";
                success = false;
            }
            LOGINFO("response: error.code=%s, error.message=%s, success=%s", error.code.c_str(), error.message.c_str(), success ? "true" : "false");
            return Core::ERROR_NONE;
        }
        
        Core::hresult SystemServicesImplementation::SetTerritory(const string& territory, const string& region, SystemError& error, bool& success)
	    {
            LOGINFO("territory=%s, region=%s", territory.c_str(), region.c_str());
            bool resp = false;
            const std::lock_guard<std::mutex> lock(m_territoryMutex);
            if(!territory.empty()){
                makePersistentDir();
                string regionStr = "";
                readTerritoryFromFile();//Read existing territory and Region from file
                string territoryStr = territory;
                LOGWARN(" Territory Value : %s ", territoryStr.c_str());
                try{
                    int index = m_strStandardTerritoryList.find(territoryStr);
                    if((territoryStr.length() == 3) && (index >=0 && index <= 1100) ){
                        if(!region.empty()){
                            regionStr = region;
                            if(regionStr != ""){
                                if(isRegionValid(regionStr)){
                                    resp = writeTerritory(territoryStr,regionStr);
                                    LOGWARN(" territory name %s ", territoryStr.c_str());
                                    LOGWARN(" region name %s", regionStr.c_str());
                                }else{
                                    error.message = "Invalid region";
                                    LOGWARN("Please enter valid region");
                                    return Core::ERROR_GENERAL;
                                }
                            }
                        }else{
                            resp = writeTerritory(territoryStr,regionStr);
                            LOGWARN(" Region is empty, only territory is updated. territory name %s ", territoryStr.c_str());
                        }
                    }else{
                        error.message =  "Invalid territory";
                        LOGWARN("Please enter valid territory Parameter value.");
                        return Core::ERROR_GENERAL;;
                    }
                    if(resp == true){
                        //call event on Territory changed
                        if (SystemServicesImplementation::_instance)
                            SystemServicesImplementation::_instance->OnTerritoryChanged(m_strTerritory,std::move(territoryStr),m_strRegion,std::move(regionStr));
                    }
                }
                catch(...){
                    LOGWARN(" caught exception...");
                }
            }else{
                error.message =  "Invalid territory name";
                LOGWARN("Please enter valid territory Parameter name.");
                resp = false;
            }
            success = resp;
            LOGINFO("response: error.message=%s, success=%s", error.message.c_str(), success ? "true" : "false");
            return Core::ERROR_NONE;
	    }

	    uint32_t SystemServicesImplementation::writeTerritory(string territory, string region)
	    {
		    bool resp = false;
		    ofstream outdata(TERRITORYFILE);
		    if(!outdata){
			    LOGWARN(" Territory : Failed to open the file");
			    return resp;
		    }
		    if (territory != ""){
			    outdata << "territory:" + territory+"\n";
			    resp = true;
		    }
		    if (region != ""){
			    outdata << "region:" + region+"\n";
			    resp = true;
		    }
		    outdata.close();
		    return resp;
	    }
	
	    Core::hresult SystemServicesImplementation::SetTimeZoneDST(const string& timeZone, const string& accuracy, uint32_t& SysSrv_Status, string& errorMessage, bool& success)
        {
            LOGINFO("timeZone=%s, accuracy=%s", timeZone.c_str(), accuracy.c_str());
            bool resp = true;
            bool isUniversal = false, isOlson = true;

            if (!timeZone.empty()) {
                std::string dir = dirnameOf(TZ_FILE);
                try {
                    size_t pos = timeZone.find("/");
                    if (timeZone.empty() || (timeZone == "null")) {
                        LOGERR("Empty timeZone received.");
                    }
                    
                    if( (timeZone.compare("Universal")) == 0) {
                        isUniversal = true;
                        isOlson = false;
                    }
                    
                    if(isOlson) {
                        
                        if( (pos == string::npos) ||  ( (pos != string::npos) &&  (pos+1 == timeZone.length())  )   )
                        {
                            LOGERR("Invalid timezone format received : %s . Timezone should be in either Universal or  Olson format  Ex : America/New_York . \n", timeZone.c_str());
                        }
                    }
                    
                    if( (isUniversal == true) || (isOlson == true)) {
                        std::string path =ZONEINFO_DIR;
                        path += "/";
                        std::string country = timeZone.substr(0,pos);
                        std::string city = path+timeZone;
                        if( dirExists(path+country)  && Utils::fileExists(city.c_str()) )
                        {
                            if (!dirExists(dir)) {
                                if (Core::Directory(dir.c_str()).CreatePath() == false)
                                {
                                    LOGERR("Error creating dir");
                                }
                            } else {
                                //Do nothing//
                            }
                            std::string oldTimeZoneDST = getTimeZoneDSTHelper();
                            if (oldTimeZoneDST != timeZone) {
                                FILE *f = fopen(TZ_FILE, "w");
                                if (f) {
                                    if (timeZone.size() != fwrite(timeZone.c_str(), 1, timeZone.size(), f))
                                    {
                                        LOGERR("Failed to write %s", TZ_FILE);
                                        resp = false;
                                    }

                                    fflush(f);
                                    fsync(fileno(f));
                                    fclose(f);
#ifdef ENABLE_LINK_LOCALTIME
                                    // Now create the linux link back to the zone info file to our writeable localtime
                                    if (Utils::fileExists(LOCALTIME_FILE)) {
                                        if (remove(LOCALTIME_FILE) != 0) {
                                            LOGERR("Failed to remove %s: %s\n", LOCALTIME_FILE, strerror(errno));
                                        }
                                    }

                                    LOGWARN("Linux localtime linked to %s\n", city.c_str());
                                    symlink(city.c_str(), LOCALTIME_FILE);
#endif
                                } else {
                                    LOGERR("Unable to open %s file.\n", TZ_FILE);
                                    populateResponseWithError(SysSrv_FileAccessFailed, SysSrv_Status, errorMessage);
                                    resp = false;
                                }
                            }

                            std::string oldAccuracy = getTimeZoneAccuracyDSTHelper();
                            std::string currentAccuracy = accuracy;

                            if (!currentAccuracy.empty()) {
                                if (currentAccuracy != TZ_ACCURACY_INITIAL && currentAccuracy != TZ_ACCURACY_INTERIM && currentAccuracy != TZ_ACCURACY_FINAL) {
                                    LOGERR("Wrong TimeZone Accuracy: %s", currentAccuracy.c_str());
                                    currentAccuracy = oldAccuracy;
                                }
                            }

                            if (currentAccuracy != oldAccuracy) {
                                FILE *f = fopen(TZ_ACCURACY_FILE, "w");
                                if (f) {
                                    if (currentAccuracy.size() != fwrite(currentAccuracy.c_str(), 1, currentAccuracy.size(), f))
                                    {
                                        LOGERR("Failed to write %s", TZ_ACCURACY_FILE);
                                        resp = false;
                                    }

                                    fflush(f);
                                    fsync(fileno(f));
                                    fclose(f);
                                }
                            }

                            if (SystemServicesImplementation::_instance && (oldTimeZoneDST != timeZone || oldAccuracy != currentAccuracy))
                                SystemServicesImplementation::_instance->OnTimeZoneDSTChanged(std::move(oldTimeZoneDST),timeZone,std::move(oldAccuracy), std::move(currentAccuracy));

                        }
                        else{
                            LOGERR("Invalid timeZone  %s received. Timezone not supported in TZ Database. \n", timeZone.c_str());
                            populateResponseWithError(SysSrv_FileNotPresent, SysSrv_Status, errorMessage);
                            resp = false;
                        }

#ifdef DISABLE_GEOGRAPHY_TIMEZONE
                        std::string tzenv = ":";
                        tzenv += timeZone;
                        Core::SystemInfo::SetEnvironment(_T("TZ"), tzenv.c_str());
#endif
                    }
                } catch (...) {
                    LOGERR("catch block : parameters[\"timeZone\"]...");
                }
            } else {
                populateResponseWithError(SysSrv_MissingKeyValues, SysSrv_Status, errorMessage);
            }
            success = resp;
            LOGINFO("response: SysSrv_Status=%u, errorMessage=%s, success=%s", SysSrv_Status, errorMessage.c_str(), success ? "true" : "false");
            return Core::ERROR_NONE;
        }
        
        Core::hresult SystemServicesImplementation::UpdateFirmware(SystemResult& result)
        {
            LOGWARN("SystemService updatingFirmware\n");
            FILE* pipe = v_secure_popen("r", "/usr/bin/rdkvfwupgrader 0 4 >> /opt/logs/swupdate.log &");
            if(pipe){
               v_secure_pclose(pipe);
            }
            result.success = true;
            LOGINFO("response: success=%s", result.success ? "true" : "false");
            return Core::ERROR_NONE;
        }
        
        Core::hresult SystemServicesImplementation::GetMacAddresses(const string& GUID, bool &asyncResponse, uint32_t& SysSrv_Status, string& errorMessage, bool& success)
        {
            LOGINFO("GUID=%s", GUID.c_str());
            if (!Utils::fileExists("/lib/rdk/getDeviceDetails.sh")) {
                populateResponseWithError(SysSrv_FileNotPresent, SysSrv_Status, errorMessage);
            } else {
                try
                {
                    if (thread_getMacAddresses.get().joinable())
                        thread_getMacAddresses.get().join();

                    thread_getMacAddresses = Utils::ThreadRAII(std::thread(getMacAddressesAsync, this));
                    asyncResponse = true;
                    success = true;
                }
                catch(const std::system_error& e)
                {
                    LOGERR("exception in GetMacAddresses %s", e.what());
                    asyncResponse = false;
                    success = false;
                }
            }
            LOGINFO("response: asyncResponse=%s, SysSrv_Status=%u, errorMessage=%s, success=%s", asyncResponse ? "true" : "false", SysSrv_Status, errorMessage.c_str(), success ? "true" : "false");
            return Core::ERROR_NONE;
        }
        
        void SystemServicesImplementation::getMacAddressesAsync(SystemServicesImplementation *pSs)
        {
            long unsigned int i=0;
            long unsigned int listLength = 0;
            JsonObject params;
            string macTypeList[] = {"ecm_mac", "estb_mac", "moca_mac",
                "eth_mac", "wifi_mac", "bluetooth_mac", "rf4ce_mac"};
            string tempBuffer;

            for (i = 0; i < sizeof(macTypeList)/sizeof(macTypeList[0]); i++) {
                tempBuffer.clear();
                FILE* pipe = v_secure_popen("r","/lib/rdk/getDeviceDetails.sh %s %s",GET_STB_DETAILS_SCRIPT_READ_COMMAND, macTypeList[i].c_str());
                if (pipe) {
                    char buff[1024] = { '\0' };
                    while (fgets(buff, sizeof(buff), pipe)) {
                           tempBuffer += buff;
                           memset(buff, 0, sizeof(buff));
                    }
                     v_secure_pclose(pipe);

               }

                removeCharsFromString(tempBuffer, "\n\r");
                LOGWARN("resp = %s\n", tempBuffer.c_str());
                params[macTypeList[i].c_str()] = (tempBuffer.empty()? "00:00:00:00:00:00" : tempBuffer.c_str());
                listLength++;
            }
            if (listLength != i) {
                params["info"] = "Details fetch: all are not success";
            }
            if (listLength) {
                params["success"] = true;
            } else {
                params["success"] = false;
            }
            if (pSs) {
                pSs->dispatchEvent(SYSTEMSERVICES_EVT_ONMACADDRESSRETRIEVED, params);
            } else {
                LOGERR("SystemServicesImplementation *pSs is NULL\n");
            }
        }
        
        Core::hresult SystemServicesImplementation::SetWakeupSrcConfiguration(const string& powerState, ISystemServicesWakeupSourcesIterator* const& wakeupSources, SystemResult& result)
        {
            LOGINFO("powerState=%s", powerState.c_str());
            Core::hresult retStatus = Core::ERROR_NONE;

            if(wakeupSources != nullptr)
            {
                std::list<WakeupSrcConfig> configs = {};
                WakeupSources src{};
                while(wakeupSources->Next(src))
                {
                    // Check each wakeup source field and add to configs if enabled
                    if(src.voice) {
                        configs.emplace_back(WakeupSrcConfig{WakeupSrcType::WAKEUP_SRC_VOICE, true});
                    }
                    if(src.presenceDetection) {
                        configs.emplace_back(WakeupSrcConfig{WakeupSrcType::WAKEUP_SRC_PRESENCEDETECTED, true});
                    }
                    if(src.bluetooth) {
                        configs.emplace_back(WakeupSrcConfig{WakeupSrcType::WAKEUP_SRC_BLUETOOTH, true});
                    }
                    if(src.wifi) {
                        configs.emplace_back(WakeupSrcConfig{WakeupSrcType::WAKEUP_SRC_WIFI, true});
                    }
                    if(src.ir) {
                        configs.emplace_back(WakeupSrcConfig{WakeupSrcType::WAKEUP_SRC_IR, true});
                    }
                    if(src.powerKey) {
                        configs.emplace_back(WakeupSrcConfig{WakeupSrcType::WAKEUP_SRC_POWERKEY, true});
                    }
                    if(src.cec) {
                        configs.emplace_back(WakeupSrcConfig{WakeupSrcType::WAKEUP_SRC_CEC, true});
                    }
                    if(src.lan) {
                        configs.emplace_back(WakeupSrcConfig{WakeupSrcType::WAKEUP_SRC_LAN, true});
                    }
                    if(src.timer) {
                        configs.emplace_back(WakeupSrcConfig{WakeupSrcType::WAKEUP_SRC_TIMER, true});
                    }
                }
                LOGWARN("configs size :%zu", configs.size());

                if(!configs.empty())
                {
                    ASSERT (_powerManagerPlugin);
                    if(_powerManagerPlugin)
                    {
                        auto iter = WakeupSourceConfigIteratorImpl::Create<IWakeupSourceConfigIterator>(configs);
                        retStatus = _powerManagerPlugin->SetWakeupSourceConfig(iter);
                        iter->Release();
                    }

                    result.success = (retStatus == Core::ERROR_NONE);
                }
            }
            LOGINFO("response: success=%s", result.success ? "true" : "false");
            return retStatus;
        }
        
        Core::hresult SystemServicesImplementation::SetMode(const ModeInfo& modeInfo, uint32_t& SysSrv_Status, string& errorMessage, bool& success)
        {
            bool changeMode = true;
            std::string oldMode = m_currentMode;
            std::string newMode = modeInfo.mode;
            int duration = modeInfo.duration;

            LOGWARN("Request to switch mode from %s to %s with duration %d", oldMode.c_str(), newMode.c_str(), duration);

            if(newMode.empty())
            {
                populateResponseWithError(SysSrv_MissingKeyValues, SysSrv_Status, errorMessage);
                return Core::ERROR_NONE;
            }

            if(newMode != MODE_NORMAL && newMode != MODE_WAREHOUSE && newMode != MODE_EAS)
            {
                LOGERR("value of new mode is incorrect, therefore \
                    current mode '%s' not changed.\n", oldMode.c_str());
                success = false;
                return Core::ERROR_NONE;
            }

            if(MODE_NORMAL == m_currentMode && (duration == 0 || (duration != 0 && MODE_NORMAL == newMode))) {
                changeMode = false;
            } else if(MODE_NORMAL != newMode && duration != 0) {
                m_currentMode = newMode;
                duration < 0 ? stopModeTimer() : startModeTimer(duration);
            } else {
                m_currentMode = MODE_NORMAL;
                stopModeTimer();
            }

            if(changeMode)
            {
                IARM_Bus_CommonAPI_SysModeChange_Param_t modeParam;
                stringToIarmMode(std::move(oldMode), modeParam.oldMode);
                stringToIarmMode(m_currentMode, modeParam.newMode);

                if(IARM_RESULT_SUCCESS == IARM_Bus_Call(IARM_BUS_DAEMON_NAME,
                    "DaemonSysModeChange", &modeParam, sizeof(modeParam)))
                {
                    LOGWARN("Mode switched to %s", m_currentMode.c_str());

                    if (MODE_NORMAL != m_currentMode && duration < 0) {
                        LOGWARN("duration is negative, therefore \
                            mode timer stopped and Receiver will keep \
                            mode '%s', untill changing it in next call",
                            m_currentMode.c_str());
                    }
                    success = true;
                }
                else
                {
                    stopModeTimer();
                    m_currentMode = MODE_NORMAL;
                    LOGERR("failed to switch to mode '%s'. Receiver \
                        forced to switch to '%s'", newMode.c_str(), m_currentMode.c_str());
                    success = false;
                }

                if(MODE_WAREHOUSE == m_currentMode)
                {
                    FILE *fp = fopen(WAREHOUSE_MODE_FILE,"w+");

                    if(fp){
                        fclose(fp);
                    } else {
                        LOGWARN("Warehouse file create failed");
                    }
                }
                else
                {
                    if(Utils::fileExists(WAREHOUSE_MODE_FILE))
                    {
                        if(0 != unlink(WAREHOUSE_MODE_FILE)) {
                            LOGWARN("Unlink is failed for [%s] file",WAREHOUSE_MODE_FILE);
                        }
                    }
                }       

                m_temp_settings.setValue("mode", m_currentMode);
                m_temp_settings.setValue("mode_duration", m_remainingDuration);
                success = true;
            }
            else
            {
                LOGWARN("Current mode '%s' not changed", m_currentMode.c_str());
                success = true;
            }
            LOGINFO("response={success: %s}", success ? "true" : "false");
            return Core::ERROR_NONE;
        }

        void SystemServicesImplementation::OnSystemPowerStateChanged(string currentPowerState, string powerState)
        {
            if ("LIGHT_SLEEP" == powerState || "STANDBY" == powerState) {
                if ("ON" == currentPowerState) {
                    RFC_ParamData_t param = {0};
                    WDMP_STATUS status = getRFCParameter(NULL, RFC_LOG_UPLOAD, &param);
                    if(WDMP_SUCCESS == status && param.type == WDMP_BOOLEAN && (strncasecmp(param.value,"true",4) == 0))
                    {
                        SystemResult result;
                        UploadLogsAsync(result);
                    }
                }
            } else if ("DEEP_SLEEP" == powerState) {

                pid_t uploadLogsPid = -1;
                {
                    lock_guard<mutex> lck(m_uploadLogsMutex);
                    uploadLogsPid = m_uploadLogsPid;
                }

                if (-1 != uploadLogsPid)
                {
                    SystemResult result;
                    AbortLogUpload(result);
                }
            }

            JsonObject params;
            params["powerState"] = powerState;
            params["currentPowerState"] = currentPowerState;
            LOGWARN("power state changed from '%s' to '%s'", currentPowerState.c_str(), powerState.c_str());

            char value[256] = {0};
            snprintf(value, sizeof(value), "power state changed from");
            t2_event_s((char*)"PwrStateChng_split", value);

            if (currentPowerState == "ON" && powerState == "LIGHT_SLEEP")
            {
                t2_event_d((char*)"SYST_INFO_ThunderSleep1", 1);
            }
            else if (currentPowerState == "LIGHT_SLEEP" && powerState == "DEEP_SLEEP")
            {
                t2_event_d((char*)"SYST_INFO_ThunderSleep2", 1);
            }
            else if (currentPowerState == "DEEP_SLEEP" && powerState == "LIGHT_SLEEP")
            {
                t2_event_d((char*)"SYST_INFO_ThunderWake1", 1);
            }
            else if (currentPowerState == "LIGHT_SLEEP" && powerState == "ON")
            {
                t2_event_d((char*)"SYST_INFO_ThunderWake2", 1);
            }
            dispatchEvent(SYSTEMSERVICES_EVT_ONSYSTEMPOWERSTATECHANGED, params);
        }
        
        void SystemServicesImplementation::OnSystemModeChanged(string mode)
        {
            JsonObject params;
            params["mode"] = mode;
            LOGINFO("mode changed to '%s'\n", mode.c_str());
            dispatchEvent(SYSTEMSERVICES_EVT_ONSYSTEMMODECHANGED, params);
        }
        
        void SystemServicesImplementation::OnNetworkStandbyModeChanged(const bool enabled)
        {
            m_networkStandbyMode = enabled;
            m_networkStandbyModeValid = true;
            JsonObject params;
            params["nwStandby"] = enabled;
            dispatchEvent(SYSTEMSERVICES_EVT_ONNETWORKSTANDBYMODECHANGED, params);
        }
        
        void SystemServicesImplementation::OnFirmwareUpdateStateChange(int newState)
        {
            if (newState != m_FwUpdateState_LatestEvent) {
                JsonObject params;
                const FirmwareUpdateState firmwareUpdateState = (FirmwareUpdateState)newState;
                m_FwUpdateState_LatestEvent=(int)firmwareUpdateState;
                params["firmwareUpdateStateChange"] = (int)firmwareUpdateState;
                LOGINFO("New firmwareUpdateState = %d\n", (int)firmwareUpdateState);
                dispatchEvent(SYSTEMSERVICES_EVT_ONFIRMWAREUPDATESTATECHANGED, params);

            } else {
                LOGINFO("Got event with same irmwareUpdateState = %d\n", newState);
            }
        }
        
        void SystemServicesImplementation::OnTemperatureThresholdChanged(string thresholdType, bool exceed, float temperature)
        {
            JsonObject params;
            params["thresholdType"] = thresholdType;
            params["exceeded"] = exceed;
            params["temperature"] = to_string(temperature);
            LOGWARN("thresholdType = %s exceed = %d temperature = %f\n", thresholdType.c_str(), exceed, temperature);
            dispatchEvent(SYSTEMSERVICES_EVT_ONTEMPERATURETHRESHOLDCHANGED, params);
        }
        
        void SystemServicesImplementation::OnRebootBegin(const string &rebootReasonCustom, const string &rebootReasonOther, const string &rebootRequestor)
        {
            if (SystemServicesImplementation::_instance) {
                SystemServicesImplementation::_instance->OnPwrMgrReboot(rebootRequestor, rebootReasonOther);
            } else {
                LOGERR("SystemServicesImplementation::_instance is NULL.\n");
            }
        }
        
        void SystemServicesImplementation::OnPwrMgrReboot(string requestedApp, string rebootReason)
        {
            JsonObject params;
            params["requestedApp"] = requestedApp;
            params["rebootReason"] = rebootReason;

            dispatchEvent(SYSTEMSERVICES_EVT_ONREBOOTREQUEST, params);
        }
        
        void SystemServicesImplementation::OnClockSet()
        {
            JsonObject params;
            dispatchEvent(SYSTEMSERVICES_EVT_ON_SYSTEM_CLOCK_SET, params);
        }
        
#if defined(USE_IARMBUS) || defined(USE_IARM_BUS)
        IARM_Result_t _SysModeChange(void *arg)
        {
            IARM_Bus_CommonAPI_SysModeChange_Param_t *param =
                (IARM_Bus_CommonAPI_SysModeChange_Param_t *)arg;

            std::string mode = iarmModeToString(param->newMode);

#ifdef HAS_API_POWERSTATE
            if (SystemServicesImplementation::_instance) {
                SystemServicesImplementation::_instance->OnSystemModeChanged(std::move(mode));
            } else {
                LOGERR("SystemServicesImplementation::_instance is NULL.\n");
            }
#else
            LOGINFO("HAS_API_POWERSTATE is not defined.\n");
#endif /* HAS_API_POWERSTATE */
            return IARM_RESULT_SUCCESS;
        }

        void _deviceMgtUpdateReceived(const char *owner, IARM_EventId_t eventId, void *data, size_t len)
        {
            if (!strcmp(IARM_BUS_SYSMGR_NAME, owner)) {
                if (IARM_BUS_SYSMGR_EVENT_DEVICE_UPDATE_RECEIVED  == eventId) {
                    LOGWARN("%s:%d IARM_BUS_SYSMGR_EVENT_DEVICE_UPDATE_RECEIVED event received\n",__FUNCTION__, __LINE__);
                    if (SystemServicesImplementation::_instance) {
                        LOGWARN("%s:%d Invoke OnDeviceMgtUpdateReceived to notify\n", __FUNCTION__, __LINE__);
                        SystemServicesImplementation::_instance->OnDeviceMgtUpdateReceived((IARM_BUS_SYSMGR_DeviceMgtUpdateInfo_Param_t *)data);
                    } else {
                        LOGERR("%s:%d SystemServicesImplementation::_instance is NULL.\n", __FUNCTION__, __LINE__);
                    }
                }
            }
        }	

        void _systemStateChanged(const char *owner, IARM_EventId_t eventId, void *data, size_t len)
        {
            int seconds = 600; /* 10 Minutes to Reboot */

            LOGINFO("len = %zu\n", len);
            /* Only handle state events */
            if (eventId != IARM_BUS_SYSMGR_EVENT_SYSTEMSTATE) return;

            IARM_Bus_SYSMgr_EventData_t *sysEventData = (IARM_Bus_SYSMgr_EventData_t*)data;
            IARM_Bus_SYSMgr_SystemState_t stateId = sysEventData->data.systemStates.stateId;
            int state = sysEventData->data.systemStates.state;

            switch (stateId) {
                case IARM_BUS_SYSMGR_SYSSTATE_FIRMWARE_UPDATE_STATE:
                    {
                        LOGWARN("IARMEvt: IARM_BUS_SYSMGR_SYSSTATE_FIRMWARE_UPDATE_STATE = '%d'\n", state);
                        if (SystemServicesImplementation::_instance)
                        {
                            if (IARM_BUS_SYSMGR_FIRMWARE_UPDATE_STATE_CRITICAL_REBOOT == state) {
                                LOGWARN(" Critical reboot is required. \n ");
                                SystemServicesImplementation::_instance->OnFirmwarePendingReboot(seconds);
                            } else {
                                SystemServicesImplementation::_instance->OnFirmwareUpdateStateChange(state);
                            }
                        } else {
                            LOGERR("SystemServicesImplementation::_instance is NULL.\n");
                        }
                    } break;

                case IARM_BUS_SYSMGR_SYSSTATE_TIME_SOURCE:
                    {
                        if (sysEventData->data.systemStates.state)
                        {
                            LOGWARN("Clock is set.");
                            if (SystemServicesImplementation::_instance) {
                                SystemServicesImplementation::_instance->OnClockSet();
                            } else {
                                LOGERR("SystemServicesImplementation::_instance is NULL.\n");
                            }
                        }
                    } break;
                case IARM_BUS_SYSMGR_SYSSTATE_LOG_UPLOAD:
                    {
                        LOGWARN("IARMEvt: IARM_BUS_SYSMGR_SYSSTATE_LOG_UPLOAD = '%d'", state);
                        if (SystemServicesImplementation::_instance)
                        {
                            SystemServicesImplementation::_instance->OnLogUpload(state);
                        } else {
                            LOGERR("SystemServicesImplementation::_instance is NULL.\n");
                        }
                    } break;


                default:
                    /* Nothing to do. */;
            }
        }

#endif /* defined(USE_IARMBUS) || defined(USE_IARM_BUS) */

#ifdef ENABLE_SYSTIMEMGR_SUPPORT
        void _timerStatusEventHandler(const char *owner, IARM_EventId_t eventId,
                void *data, size_t len)
        {
            if ((!strcmp(IARM_BUS_SYSTIME_MGR_NAME, owner)) && (0 == eventId)) {
                    LOGWARN("IARM_BUS_SYSTIME_MGR_NAME event received\n");
                    TimerMsg* pMsg = (TimerMsg*)data;
                    string timequality = std::string(pMsg->message,cTIMER_STATUS_MESSAGE_LENGTH);
                    string timersrc = std::string(pMsg->timerSrc,cTIMER_STATUS_MESSAGE_LENGTH);
                    string timerStr = std::string(pMsg->currentTime,cTIMER_STATUS_MESSAGE_LENGTH);

                if (SystemServicesImplementation::_instance) {
                    SystemServicesImplementation::_instance->OnTimeStatusChanged(std::move(timequality),std::move(timersrc),std::move(timerStr));
                } else {
                    LOGERR("SystemServicesImplementation::_instance is NULL.\n");
                }
            }
        }
#endif// ENABLE_SYSTIMEMGR_SUPPORT

#ifdef ENABLE_THERMAL_PROTECTION
        static void handleThermalLevelChange(const int &currentThermalLevel, const int &newThermalLevel, const float &currentTemperature)
        {
            bool crossOver;
            bool validparams = true;
            std::string thermLevel;

            switch (newThermalLevel) {
                case WPEFramework::Exchange::IPowerManager::THERMAL_TEMPERATURE_NORMAL:
                    {
                        switch (currentThermalLevel) {
                            case WPEFramework::Exchange::IPowerManager::THERMAL_TEMPERATURE_HIGH:
                            case WPEFramework::Exchange::IPowerManager::THERMAL_TEMPERATURE_CRITICAL:
                                crossOver = false;
                                thermLevel = "WARN";
                                break;
                            default:
                                validparams = false;
                                LOGERR("[%s] Invalid temperature levels \n", __FUNCTION__);
                        }

                    }
                    break;
                case WPEFramework::Exchange::IPowerManager::THERMAL_TEMPERATURE_HIGH:
                    {
                        switch (currentThermalLevel) {
                            case WPEFramework::Exchange::IPowerManager::THERMAL_TEMPERATURE_NORMAL:
                                crossOver = true;
                                thermLevel = "WARN";
                                break;
                            case WPEFramework::Exchange::IPowerManager::THERMAL_TEMPERATURE_CRITICAL:
                                crossOver = false;
                                thermLevel = "MAX";
                                break;
                            default:
                                validparams = false;
                                LOGERR("Invalid temperature levels \n");
                        }

                    }
                    break;
                case WPEFramework::Exchange::IPowerManager::THERMAL_TEMPERATURE_CRITICAL:
                    {
                        switch (currentThermalLevel) {
                            case WPEFramework::Exchange::IPowerManager::THERMAL_TEMPERATURE_HIGH:
                            case WPEFramework::Exchange::IPowerManager::THERMAL_TEMPERATURE_NORMAL:
                                crossOver = true;
                                thermLevel = "MAX";
                                break;
                            default:
                                validparams = false;
                                LOGERR("Invalid temperature levels \n");
                        }

                    }
                    break;
                default:
                    validparams = false;
                    LOGERR("Invalid temperature levels \n");
            }
            if (validparams) {
                LOGWARN("Processing temperature threshold change\n");
                if (SystemServicesImplementation::_instance) {
                    SystemServicesImplementation::_instance->OnTemperatureThresholdChanged(std::move(thermLevel),
                            crossOver, currentTemperature);
                } else {
                    LOGERR("SystemServicesImplementation::_instance is NULL.\n");
                }
            }
        }
#endif /* ENABLE_THERMAL_PROTECTION */

        void SystemServicesImplementation::OnFirmwarePendingReboot(int seconds)
        {
            JsonObject params;
            params["fireFirmwarePendingReboot"] = seconds;
            LOGINFO("Notifying OnFirmwarePendingReboot received \n");
            dispatchEvent(SYSTEMSERVICES_EVT_ONFWPENDINGREBOOT, params);
        }
        
        void SystemServicesImplementation::OnTerritoryChanged(string oldTerritory, string newTerritory, string oldRegion, string newRegion)
	    {
		    JsonObject params;
		    params["oldTerritory"] = oldTerritory;
		    params["newTerritory"] = newTerritory;
		    LOGWARN(" Notifying Territory changed - oldTerritory: %s - newTerritory: %s",oldTerritory.c_str(),newTerritory.c_str());
		    if(newRegion != ""){
			    params["oldRegion"] = oldRegion;
			    params["newRegion"] = newRegion;
			    LOGWARN(" Notifying Region changed - oldRegion: %s - newRegion: %s",oldRegion.c_str(),newRegion.c_str());
		    }
		    dispatchEvent(SYSTEMSERVICES_EVT_ONTERRITORYCHANGED, params);
	    }
	
	    void SystemServicesImplementation::OnTimeZoneDSTChanged(string oldTimeZone, string newTimeZone, string oldAccuracy, string newAccuracy)
	    {
		    JsonObject params;
		    params["oldTimeZone"] = oldTimeZone;
		    params["newTimeZone"] = newTimeZone;
		    params["oldAccuracy"] = oldAccuracy;
		    params["newAccuracy"] = newAccuracy;
		    LOGWARN(" Notifying TimeZone changed - oldTimeZone: %s - newTimeZone: %s, oldAccuracy: %s - newAccuracy %s",oldTimeZone.c_str(),newTimeZone.c_str(),oldAccuracy.c_str(),newAccuracy.c_str());
		    dispatchEvent(SYSTEMSERVICES_EVT_ONTIMEZONEDSTCHANGED, params);
	    }
	
	    void SystemServicesImplementation::OnLogUpload(int newState)
        {
            lock_guard<mutex> lck(m_uploadLogsMutex);

            if (-1 != m_uploadLogsPid) {
                JsonObject params;

                params["logUploadStatus"] = newState == IARM_BUS_SYSMGR_LOG_UPLOAD_SUCCESS ? LOG_UPLOAD_STATUS_SUCCESS :
                    newState == IARM_BUS_SYSMGR_LOG_UPLOAD_ABORTED ? LOG_UPLOAD_STATUS_ABORTED : LOG_UPLOAD_STATUS_FAILURE;

                dispatchEvent(SYSTEMSERVICES_EVT_ONLOGUPLOAD, params);

                pid_t wp;
                int status;

                if ((wp = waitpid(m_uploadLogsPid, &status, 0)) != m_uploadLogsPid) {
                    LOGERR("Waitpid for failed: %d, status: %d", m_uploadLogsPid, status);
                }

                m_uploadLogsPid = -1;
            } else {
                LOGERR("Upload Logs script isn't runing");
            }
        }
        
        void SystemServicesImplementation::OnDeviceMgtUpdateReceived(IARM_BUS_SYSMGR_DeviceMgtUpdateInfo_Param_t *config)
        {
            JsonObject params;
            params["source"] = std::string(config->source);
            params["type"] = std::string(config->type);
            params["success"] = config->status;
            LOGWARN("OnDeviceMgtUpdateReceived: source = %s type = %s success = %d\n", config->source, config->type, config->status);
            dispatchEvent(SYSTEMSERVICES_EVT_ONDEVICEMGTUPDATERECEIVED, params);
        }
        
#ifdef ENABLE_SYSTIMEMGR_SUPPORT
        void SystemServicesImplementation::OnTimeStatusChanged(string timequality,string timesource, string utctime)
        {
            JsonObject params;
            params["TimeQuality"] = timequality;
            params["TimeSrc"] = timesource;
            params["Time"] = utctime;
            LOGWARN("TimeQuality = %s TimeSrc = %s Time = %s\n",timequality.c_str(),timesource.c_str(),utctime.c_str());
            dispatchEvent(SYSTEMSERVICES_EVT_ONTIMESTATUSCHANGED, params);
        }
#endif// ENABLE_SYSTIMEMGR_SUPPORT

        bool SystemServicesImplementation::processTimeZones(std::string entry, JsonObject& out)
        {
            bool ret = true;

            std::string cmd = "zdump ";
            cmd += entry;
            
            if (0 != access(entry.c_str(), F_OK))
            {
                LOGERR("Timezone is not in olson format ('%s')", entry.c_str());
                return false;
            }

            struct stat deStat;
            if (0 == stat(entry.c_str(), &deStat))
            {
                if (S_ISDIR(deStat.st_mode))
                {
                    cmd += "/*";
                }
            }
            else
            {
                LOGERR("stat() failed: %s", strerror(errno));
            }

            FILE *p = popen(cmd.c_str(), "r");

            if(!p)
            {
                LOGERR("failed to start %s: %s", cmd.c_str(), strerror(errno));
                return false;
            }

            std::vector <std::string> dirs;

            char buf[4096];
            while(fgets(buf, sizeof(buf), p) != NULL)
            {
                std::string line(buf);

                line.erase(0, line.find_first_not_of(" \n\r\t"));
                line.erase(line.find_last_not_of(" \n\r\t") + 1);

                size_t fileEnd = line.find_first_of(" \t");

                std::string fullName;

                if (std::string::npos == fileEnd)
                {
                    LOGERR("Failed to parse '%s'", line.c_str());
                    continue;
                }

                fullName = line.substr(0, fileEnd);

                if (stat(fullName.c_str(), &deStat))
                {
                    LOGERR("stat() failed: %s", strerror(errno));
                    continue;
                }

                if (S_ISDIR(deStat.st_mode))
                {
                    // Coverity Fix: ID 50 - COPY_INSTEAD_OF_MOVE: Use std::move when adding to vector
                    dirs.push_back(std::move(fullName));
                }
                else
                {

                    std::string name = fullName;

                    size_t pathEnd = fullName.find_last_of("/") + 1;
                    if (std::string::npos != pathEnd)
                        name = fullName.substr(pathEnd);
                    else
                        LOGWARN("No '/' in %s", fullName.c_str());

                    line.erase(0, line.find_first_of(" \t"));
                    line.erase(0, line.find_first_not_of(" \n\r\t"));

                    out[name.c_str()] = line;
                }
            }

            int err = pclose(p);

            if (0 != err)
            {    
                LOGERR("%s failed with code %d", cmd.c_str(), err);
                return false;
            }

	        long unsigned int n=0;
            for (n = 0 ; n < dirs.size(); n++) {
                std::string name = dirs[n];

                size_t pathEnd = name.find_last_of("/") + 1;

                if (std::string::npos != pathEnd)
                    name = name.substr(pathEnd);
                else
                    LOGWARN("No '/' in %s", name.c_str());

                JsonObject dirObject;
                processTimeZones(dirs[n], dirObject);
                out[name.c_str()] = dirObject;
            }

            return ret;
        }

        Core::hresult SystemServicesImplementation::GetTimeZones(IStringIterator* const& timeZones, string& zoneinfo, bool& success)
        {
            if (timeZones == nullptr || timeZones->Count() == 0)
            {
                LOGINFO("No timezone list provided, processing all");
            }

            JsonObject dirObject;

            if (timeZones && (timeZones->Count() != 0))
            {
                string tz;
                while (timeZones->Next(tz))
                {
                    if (tz.empty())
                        continue;

                    std::string path = std::string(ZONEINFO_DIR) + "/" + tz;
                    bool status = processTimeZones(std::move(path), dirObject);
                    success = status;

                    if (!status)
                    {
                        LOGERR("Failed timezone %s", tz.c_str());
                    }
                }
            }
            else
            {
                success = processTimeZones(ZONEINFO_DIR, dirObject);
            }

            if (!dirObject.IsNull()) {
                dirObject.ToString(zoneinfo);
            }

            return success ? Core::ERROR_NONE : Core::ERROR_GENERAL;
        }
        
        Core::hresult SystemServicesImplementation::GetRFCConfig( IStringIterator* const& rfcList, string& RFCConfig, uint32_t& SysSrv_Status, string& errorMessage, bool& success)
        {
            LOGINFO("called");

            if (rfcList == nullptr || rfcList->Count() == 0)
            {
                populateResponseWithError(SysSrv_MissingKeyValues, SysSrv_Status, errorMessage);

                return Core::ERROR_NONE;
            }

            const std::regex re("(\\w|-|\\.)+");
            JsonObject hash;
            string rfcName;
            string cmdResponse;

            while (rfcList->Next(rfcName))
            {
                LOGINFO("RFC Name = %s", rfcName.c_str());

                if (!std::regex_match(rfcName, re))
                {
                    LOGERR("Invalid charset in %s", rfcName.c_str());
                    hash[rfcName.c_str()] = "Invalid charset found";
                    continue;
                }

                cmdResponse ="";
                WDMP_STATUS wdmpStatus;
                RFC_ParamData_t rfcParam;
                char sysServices[] = "SystemServices";
                memset(&rfcParam, 0, sizeof(rfcParam));

                wdmpStatus = getRFCParameter(sysServices, rfcName.c_str(), &rfcParam);

                if ((wdmpStatus == WDMP_SUCCESS) || (wdmpStatus == WDMP_ERR_DEFAULT_VALUE))
                {
                    cmdResponse = rfcParam.value;
                    removeCharsFromString(cmdResponse, "\n\r");

                    if (!cmdResponse.empty())
                    {
                        hash[rfcName.c_str()] = cmdResponse;
                        success = true;
                    }
                    else
                    {
                        hash[rfcName.c_str()] = "Empty response received";
                    }
                }
                else
                {
                    LOGERR("Failed to get %s status %d", rfcName.c_str(), wdmpStatus);

                    hash[rfcName.c_str()] = "Failed to read RFC";
                }
            }

            if (!hash.IsNull()) {
                hash.ToString(RFCConfig);
            }

            if (!success)
            {
                populateResponseWithError(SysSrv_UnSupportedFormat, SysSrv_Status, errorMessage);
            }

            LOGINFO("response: SysSrv_Status=%u, errorMessage=%s, success=%s", SysSrv_Status, errorMessage.c_str(), success ? "true" : "false");
            return Core::ERROR_NONE;
        }

        Core::hresult SystemServicesImplementation::GetDeviceInfo( IStringIterator* const& params, DeviceInfo& deviceInfo)
        {
            string queryParam;

            if (params != nullptr)
            {
                if (!params->IsValid())
                    queryParam = "";
                else
                    params->Next(queryParam);
            }

            removeCharsFromString(queryParam,"[\" ]");
            regmatch_t match[1];

            if (!queryParam.empty() && REG_NOERROR == regexec( &m_regexUnallowedChars, queryParam.c_str(), 1, match, 0))
            {
                LOGERR("Input has unallowable characters: '%s'", queryParam.c_str());
                deviceInfo.message = "Input has unallowable characters";
                return Core::ERROR_NONE;
            }

            LOGWARN("SystemService getDeviceInfo query %s", queryParam.c_str());

            auto deviceInfoObject = m_shellService->QueryInterfaceByCallsign<Exchange::IDeviceInfo>("DeviceInfo");

            if (deviceInfoObject == nullptr)
            {
                LOGERR("DeviceInfo plugin is not activated\n");
                deviceInfo.message = "DeviceInfo plugin is not activated";
                return Core::ERROR_NONE;
            }

            if (queryParam.empty() || queryParam == "make")
            {
                std::string deviceName;
                GetValueFromPropertiesFile(DEVICE_PROPERTIES_FILE, "DEVICE_NAME", deviceName);
                if (deviceName == "PLATCO")
                {
                    uint32_t result = Core::ERROR_GENERAL;
                    Exchange::IDeviceInfo::DeviceMake deviceMake;

                    if (deviceInfoObject)
                    {
                        result = deviceInfoObject->Make(deviceMake);
                        if (Core::ERROR_NONE == result)
                        {
                            deviceInfo.make = deviceMake.make;
                            deviceInfo.success = true;
                            LOGINFO("Device Make: %s", deviceInfo.make.c_str());
                        }
                    }
                }
                else
                {
                    std::string make;
                    GetValueFromPropertiesFile(DEVICE_PROPERTIES_FILE, "MFG_NAME", make);

                    if (make.size() > 0)
                    {
                        deviceInfo.make = std::move(make);
                        deviceInfo.success = true;
                    }
                    else
                    {
                        LOGERR("Failed to read make from properties file");
                    }
                }

                if (!queryParam.empty())
                {
                    deviceInfoObject->Release();
                    return Core::ERROR_NONE;
                }
            }

            if (queryParam.empty() || queryParam == "model_number")
            {
                uint32_t result = Core::ERROR_GENERAL;
                Exchange::IDeviceInfo::DeviceModelNo modelNo;

                if (deviceInfoObject)
                {
                    result = deviceInfoObject->Sku(modelNo);
                    if (Core::ERROR_NONE == result)
                    {
                        deviceInfo.modelNumber = modelNo.sku;
                        deviceInfo.success = true;
                    }
	            }
            }

            if (queryParam.empty() || queryParam == "imageVersion")
            {
                uint32_t result = Core::ERROR_GENERAL;
                Exchange::IDeviceInfo::FirmwareversionInfo firmwareVersionInfo;

                if (deviceInfoObject)
                {
                    result = deviceInfoObject->FirmwareVersion(firmwareVersionInfo);
                    if (Core::ERROR_NONE == result)
                    {
                        deviceInfo.imageVersion = firmwareVersionInfo.imagename;
                        deviceInfo.version = firmwareVersionInfo.imagename;
                        deviceInfo.softwareVersion = firmwareVersionInfo.imagename;
                        deviceInfo.success = true;
                    }
	            }
            }

            if (queryParam.empty() || queryParam == "build_type")
            {
                uint32_t result = Core::ERROR_GENERAL;
                string buildType = "";
                bool success = false;

                result = GetBuildType(buildType, success);

                if (Core::ERROR_NONE == result)
                {
                    Utils::String::toUpper(buildType);
                    deviceInfo.buildType = std::move(buildType);
                }
                    deviceInfo.success = success;
            }

            if (queryParam.empty() || queryParam == "device_type")
            {
                uint32_t result = Core::ERROR_GENERAL;
                Exchange::IDeviceInfo::DeviceTypeInfos deviceTypeInfos;

                if (deviceInfoObject)
                {
                    result = deviceInfoObject->DeviceType(deviceTypeInfos);
                    if (Core::ERROR_NONE == result)
                    {
                        static const std::unordered_map<Exchange::IDeviceInfo::DeviceTypeInfo, std::string> statusToString = {
                            {Exchange::IDeviceInfo::DEVICE_TYPE_IPTV,     "IpTv"},
                            {Exchange::IDeviceInfo::DEVICE_TYPE_IPSTB,    "IpStb"},
                            {Exchange::IDeviceInfo::DEVICE_TYPE_QAMIPSTB, "QamIpStb"},
                        };

                        auto it = statusToString.find(deviceTypeInfos.devicetype);
                        if (it != statusToString.end())
                        {
                           deviceInfo.deviceType = it->second;
                        }
                        deviceInfo.success = true;
                    }
	            }
            }

#ifdef ENABLE_DEVICE_MANUFACTURER_INFO
            if (queryParam.empty() || queryParam == MODEL_NAME)
            {
                if (m_ManufacturerDataModelNameValid)
                {
                    deviceInfo.modelName = m_ManufacturerDataModelName;
                    deviceInfo.success = true;
                }
                else
                {
                    IARM_Bus_MFRLib_GetSerializedData_Param_t param;
                    param.bufLen = 0;
                    param.type = mfrSERIALIZED_TYPE_PROVISIONED_MODELNAME;

                    IARM_Result_t result = IARM_Bus_Call(IARM_BUS_MFRLIB_NAME, IARM_BUS_MFRLIB_API_GetSerializedData, &param, sizeof(param));
                    param.buffer[param.bufLen] = '\0';

                    if (result == IARM_RESULT_SUCCESS) {
                        deviceInfo.modelName = string(param.buffer);
                        m_ManufacturerDataModelName = param.buffer;
                        m_ManufacturerDataModelNameValid = true;
                        deviceInfo.success = true;
                    }
                }
            }

            if (queryParam.empty() || queryParam == HARDWARE_ID)
            {
                if (m_ManufacturerDataHardwareIdValid)
                {
                    deviceInfo.hardwareID = m_ManufacturerDataHardwareID;
                    deviceInfo.success = true;
                }
                else
                {
                    IARM_Bus_MFRLib_GetSerializedData_Param_t param;
                    param.bufLen = 0;
                    param.type = mfrSERIALIZED_TYPE_HWID;

                    IARM_Result_t result = IARM_Bus_Call(IARM_BUS_MFRLIB_NAME, IARM_BUS_MFRLIB_API_GetSerializedData, &param, sizeof(param));
                    param.buffer[param.bufLen] = '\0';

                    if (result == IARM_RESULT_SUCCESS) {
                        deviceInfo.hardwareID = string(param.buffer);
                        m_ManufacturerDataHardwareID = param.buffer;
			            m_ManufacturerDataHardwareIdValid = true;
			            deviceInfo.success = true;
                    }
                }
            }

            if (queryParam.empty() || queryParam == FRIENDLY_ID)
            {
                uint32_t result = Core::ERROR_GENERAL;
                Exchange::IDeviceInfo::DeviceModel deviceModel;

                if (deviceInfoObject)
                {
                    result = deviceInfoObject->Model(deviceModel);
                    if (Core::ERROR_NONE == result)
                    {
                        deviceInfo.friendlyId = deviceModel.model;
                        deviceInfo.success = true;
                    }
	            }
            }
#endif
            if (queryParam.empty() || queryParam == "bluetooth_mac")
            {
                FILE* fp = v_secure_popen("r", "/lib/rdk/getDeviceDetails.sh read bluetooth_mac");
                if (!fp) {
                    return Core::ERROR_GENERAL;
                }

                std::ostringstream oss;
                char buffer[256];
                while (fgets(buffer, sizeof(buffer), fp) != nullptr) {
                    oss << buffer;
                }
                v_secure_pclose(fp);

                deviceInfo.bluetoothMac = oss.str();

                // Remove trailing newline if present
                if (!deviceInfo.bluetoothMac.empty() && deviceInfo.bluetoothMac.back() == '\n') {
                    deviceInfo.bluetoothMac.pop_back();
                }
            }
            
            if (queryParam.empty() || queryParam == "boxIP")
            {
                uint32_t result = Core::ERROR_GENERAL;
                Exchange::IDeviceInfo::StbIp stbIp;

                if (deviceInfoObject)
                {
                    result = deviceInfoObject->EstbIp(stbIp);
                    if (Core::ERROR_NONE == result)
                    {
                        deviceInfo.boxIP = stbIp.estbIp;
                        deviceInfo.success = true;
                    }
	            }
            }
            
            if (queryParam.empty() || queryParam == "estb_mac")
            {
                uint32_t result = Core::ERROR_GENERAL;
                Exchange::IDeviceInfo::StbMac stbMac;

                if (deviceInfoObject)
                {
                    result = deviceInfoObject->EstbMac(stbMac);
                    if (Core::ERROR_NONE == result)
                    {
                        deviceInfo.estbMac = stbMac.estbMac;
                        deviceInfo.success = true;
                    }
	            }
            }
            
            if (queryParam.empty() || queryParam == "eth_mac")
            {
                uint32_t result = Core::ERROR_GENERAL;
                Exchange::IDeviceInfo::EthernetMac ethernetMac;

                if (deviceInfoObject)
                {
                    result = deviceInfoObject->EthMac(ethernetMac);
                    if (Core::ERROR_NONE == result)
                    {
                        deviceInfo.ethMac = ethernetMac.ethMac;
                        deviceInfo.success = true;
                    }
	            }
            }
            
            if (queryParam.empty() || queryParam == "wifi_mac")
            {
                uint32_t result = Core::ERROR_GENERAL;
                Exchange::IDeviceInfo::WiFiMac wiFiMac;

                if (deviceInfoObject)
                {
                    result = deviceInfoObject->WifiMac(wiFiMac);
                    if (Core::ERROR_NONE == result)
                    {
                        deviceInfo.wifiMac = wiFiMac.wifiMac;
                        deviceInfo.success = true;
                    }
                }
            }
            deviceInfoObject->Release();
            LOGINFO("response={make:'%s', bluetooth_mac:'%s', boxIP:'%s', build_type:'%s', device_type:'%s', estb_mac:'%s', eth_mac:'%s', friendlyId:'%s', wifi_mac:'%s', model_number:'%s', imageVersion:'%s', modelName:'%s', hardwareID:'%s'}\n",
                     deviceInfo.make.c_str(), deviceInfo.bluetoothMac.c_str(), deviceInfo.boxIP.c_str(), deviceInfo.buildType.c_str(), deviceInfo.deviceType.c_str(), deviceInfo.estbMac.c_str(), deviceInfo.ethMac.c_str(), deviceInfo.friendlyId.c_str(), deviceInfo.wifiMac.c_str(),
                    deviceInfo.modelNumber.c_str(), deviceInfo.imageVersion.c_str(), deviceInfo.modelName.c_str(), deviceInfo.hardwareID.c_str());

            return Core::ERROR_NONE;
        }
        
        Core::hresult SystemServicesImplementation::GetPlatformConfiguration (const string &query, PlatformConfig& platformConfig)
        {
            LOGINFO("query %s", query.c_str());
            PlatformCaps response;
            response.Load(m_shellService, query, platformConfig);
            return Core::ERROR_NONE;
        }

        string SystemServicesImplementation::getStbVersionString()
        {
            uint32_t result = Core::ERROR_GENERAL;
            Exchange::IDeviceInfo::FirmwareversionInfo firmwareVersionInfo;

            auto deviceInfoObject = m_shellService->QueryInterfaceByCallsign<Exchange::IDeviceInfo>("DeviceInfo");

            if (deviceInfoObject)
            {
                result = deviceInfoObject->FirmwareVersion(firmwareVersionInfo);
                if (Core::ERROR_NONE == result)
                {
                    m_stbVersionString = firmwareVersionInfo.imagename;
                    deviceInfoObject->Release();
                    return m_stbVersionString;
                }
                deviceInfoObject->Release();
	        }

#ifdef STB_VERSION_STRING
            {
                m_stbVersionString = string(STB_VERSION_STRING);
            }
#else /* !STB_VERSION_STRING */
            {
                m_stbVersionString = "unknown";
            }
#endif /* !STB_VERSION_STRING */
            LOGWARN("stb version assigned to: %s\n", m_stbVersionString.c_str());
            return m_stbVersionString;
        }

        string SystemServicesImplementation::getClientVersionString()
        {
            static string clientVersionStr;
            if (clientVersionStr.length())
                return clientVersionStr;

            std::string str;
            std::string str2 = "VERSION=";
            vector<string> lines;

            if (getFileContent(VERSION_FILE_NAME, lines)) {
                for (int i = 0; i < (int)lines.size(); ++i) {
                    string line = lines.at(i);

                    std::string trial = line.c_str();
                    if (!trial.compare(0, 8, str2)) {
                        std::string gp = trial.c_str();
                        std::string delimiter = "=";
                        clientVersionStr = gp.substr((gp.find(delimiter)+1), 12);
                        break;
                    }
                }
                if (clientVersionStr.length()) {
                    LOGWARN("getClientVersionString::client \
                            version found in file: '%s'\n", clientVersionStr.c_str());
                    return clientVersionStr;
                } else {
                    LOGWARN("getClientVersionString::could \
                            not find 'client_version:' in '%s'\n", VERSION_FILE_NAME);
                }
            } else {
                LOGERR("file %s open failed\n", VERSION_FILE_NAME);
            }
#ifdef CLIENT_VERSION_STRING
            return string(CLIENT_VERSION_STRING);
#else
            return "unknown";
#endif
        }

        string SystemServicesImplementation::getStbTimestampString()
        {
            static string dateTimeStr;

            if (dateTimeStr.length()) {
                return dateTimeStr;
            }

            std::string  buildTimeStr;
            std::string str2 = "BUILD_TIME=";
            vector<string> lines;

            if (getFileContent(VERSION_FILE_NAME, lines)) {
                for (int i = 0; i < (int)lines.size(); ++i) {
                    string line = lines.at(i);
                    std::string trial = line.c_str();

                    if (trial.compare(0, 11, str2) == 0) {
                        std::string gp = trial.c_str();
                        std::string delimiter = "=";
                        buildTimeStr = gp.substr((gp.find(delimiter)+2), 19);
                        char *t1= (char *)buildTimeStr.c_str();
                        dateTimeStr = stringTodate(t1);
                        LOGWARN("versionFound : %s\n", dateTimeStr.c_str());
                        break;
                    }
                }

                if (dateTimeStr.length()) {
                    LOGWARN("getStbTimestampString::stb timestamp found in file: '%s'\n",
                            dateTimeStr.c_str());
                    return dateTimeStr;
                } else {
                    LOGWARN("getStbTimestampString::could not parse BUILD_TIME from '%s' - '%s'\n",
                            VERSION_FILE_NAME, buildTimeStr.c_str());
                }
            } else {
                LOGERR("file %s open failed\n", VERSION_FILE_NAME);
            }

#ifdef STB_TIMESTAMP_STRING
            return string(STB_TIMESTAMP_STRING);
#else
            return "unknown";
#endif
        }

        string SystemServicesImplementation::getStbBranchString()
	    {
		    static string stbBranchStr;
		    if (stbBranchStr.length())
			    return stbBranchStr;

		    std::string str;
		    std::string str2 = "BRANCH=";
		    vector<string> lines;

		    if (getFileContent(VERSION_FILE_NAME, lines)) {
			    for (int i = 0; i < (int)lines.size(); ++i) {
				    string line = lines.at(i);

				    std::string trial = line.c_str();
				    if (!trial.compare(0, 7, str2)) {
					    std::string temp = trial.c_str();
					    std::string delimiter = "=";
					    temp = temp.substr((temp.find(delimiter)+1));
					    delimiter = "_";
					    stbBranchStr = temp.substr((temp.find(delimiter)+1));
					    break;
				    }
			    }
			    if (stbBranchStr.length()) {
				    LOGWARN("getStbBranchString::STB's branch found in file: '%s'\n", stbBranchStr.c_str());
				    return stbBranchStr;
			    } else {
				    LOGWARN("getStbBranchString::could not find 'BRANCH=' in '%s'\n", VERSION_FILE_NAME);
				    return "unknown";
			    }
		    } else {
			    LOGERR("file %s open failed\n", VERSION_FILE_NAME);
			    return "unknown";
		    }
	    }

        bool SystemServicesImplementation::makePersistentDir()
        {
            int ret = mkdir("/opt/secure/persistent/System", 0700);
            if (ret == 0) {
                LOGWARN(" --- Directory /opt/secure/persistent/System created");
                return true;
            } else if (errno == EEXIST) {
                // Directory already exists, which is fine
                return true;
            } else {
                LOGERR(" --- Failed to create directory: %d", ret);
                return false;
            }
        }

    } // namespace Plugin
} // namespace WPEFramework
