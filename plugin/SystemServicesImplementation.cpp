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
#include "SystemServices.h"
#include "StateObserverHelper.h"
#include "uploadlogs.h"
#include "secure_wrapper.h"
#include <core/core.h>
#include <core/JSON.h>
#include<interfaces/entservices_errorcodes.h>

#include "SystemServicesImplementation.h"

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
        SERVICE_REGISTRATION(SystemServicesImplementation, 1, 0);
        SystemServicesImplementation* SystemServicesImplementation::_instance = nullptr;
    
        SystemServicesImplementation::SystemServicesImplementation() : _adminLock() , _service(nullptr)
        {
            LOGINFO("Create SystemServicesImplementation Instance");

#ifdef ENABLE_DEVICE_MANUFACTURER_INFO
            m_MfgSerialNumberValid = false;
#endif

            SystemServicesImplementation::_instance = this;
        }

        SystemServicesImplementation::~SystemServicesImplementation()
        {
            SystemServicesImplementation::_instance = nullptr;
            _service = nullptr;
        }

	void SystemServicesImplementation::Initialize()
        {
#if defined(USE_IARMBUS) || defined(USE_IARM_BUS)
            InitializeIARM();
#endif /* defined(USE_IARMBUS) || defined(USE_IARM_BUS) */

        }

	void SystemServicesImplementation::Deinitialize()
        {
#if defined(USE_IARMBUS) || defined(USE_IARM_BUS)
            DeinitializeIARM();
#endif /* defined(USE_IARMBUS) || defined(USE_IARM_BUS) */
        }

#if defined(USE_IARMBUS) || defined(USE_IARM_BUS)
        void SystemServicesImplementation::InitializeIARM()
        {
            if (Utils::IARM::init())
            {
                // TODO
            }
        }

	void SystemServices::DeinitializeIARM()
        {
            if (Utils::IARM::isConnected())
            {
	        // TODO
            }
        }
#endif /* defined(USE_IARMBUS) || defined(USE_IARM_BUS) */

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

        void SystemServicesImplementation::dispatchEvent(Event event, const JsonValue &params)
        {
            Core::IWorkerPool::Instance().Submit(Job::Create(this, event, params));
        }

        void SystemServicesImplementation::Dispatch(Event event, const JsonValue params)
        {
            _adminLock.Lock();
        
            std::list<Exchange::ISystemServices::INotification*>::const_iterator index(_systemServicesNotification.begin());
        
            switch(event)
            {
                case EVENT:
                    while (index != _systemServicesNotification.end()) 
                    {
                        (*index)->EventName(params.String());
                        ++index;
                    }
                    break;
 
                default:
                    LOGWARN("Event[%u] not handled", event);
                    break;
            }
            _adminLock.Unlock();
        }
    
#ifdef ENABLE_DEVICE_MANUFACTURER_INFO
	// @text getMfgSerialNumber
        // @brief Gets the Manufacturing Serial Number.
        // @param mfgSerialNumber: Manufacturing Serial Number
        // @param success: Whether the request succeeded
        // @retval ErrorCode::ERROR_NONE: Indicates success
        // @retval ErrorCode::ERROR_GENERAL: Indicates failure
        Core::hresult SystemServicesImplementation::GetMfgSerialNumber(string& mfgSerialNumber, bool& success)
        {
            LOGWARN("SystemService getMfgSerialNumber query");

            if (m_MfgSerialNumberValid) {
                mfgSerialNumber = m_MfgSerialNumber;
                LOGWARN("Got cached MfgSerialNumber %s", m_MfgSerialNumber.c_str());
                success = true;
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
            return (status ? Core::ERROR_NONE : Core::ERROR_GENERAL);
        }
#endif /* ENABLE_DEVICE_MANUFACTURER_INFO */

    } // namespace Plugin
} // namespace WPEFramework
