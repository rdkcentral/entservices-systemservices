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
bool checkOpFlashStoreDir()
{
    int ret = 0;
    if (access(OPFLASH_STORE, F_OK) == -1) {
        ret = mkdir(OPFLASH_STORE, 0774);
        LOGINFO(" --- SubDirectories created from mkdir %d ", ret);
    }
    return 0 == ret;
}

// Function to write (update or append) parameters in the file
bool write_parameters(const string &filename, const string &param, bool value, bool &update, bool &oldBlocklistFlag)
{
    ifstream file_in(filename);
    vector<string> lines;
    bool param_found = false, status = false;

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
    if (!file_out.is_open()) {
        LOGERR("Error opening file for writing:%s ", filename.c_str());
        status = false;
    }

    for (const string &line : lines) {
        file_out << line << endl;
    }
    status = true;

    file_out.close();
    LOGINFO("%s flag stored successfully in persistent memory. status= %d, update=%d, oldBlocklistFlag=%d", param.c_str(), status, update, oldBlocklistFlag);
    return status;
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
        
            JsonObject obj;
            obj.FromString(params.String());
            std::list<Exchange::ISystemServices::INotification*>::const_iterator index(_systemServicesNotification.begin());
        
            switch(event)
            {
                case SYSTEMSERVICES_EVT_ONBLOCKLISTCHANGED:
                {
                    bool oldFlag = obj["oldBlocklistFlag"].Boolean();
                    bool newFlag = obj["newBlocklistFlag"].Boolean();
                    
                    while (index != _systemServicesNotification.end()) 
                    {
                        (*index)->OnBlocklistChanged(oldFlag, newFlag);
                        ++index;
                    }
                    break;
                }

		case SYSTEMSERVICES_EVT_ONLOGUPLOAD:
                {
                    string logUploadStatus = obj["logUploadStatus"].String();
                   
		    while (index != _systemServicesNotification.end())
                    {
                        (*index)->OnLogUpload(logUploadStatus);
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

         /***
         * @brief : Starts background process to upload logs
         * @param1[out] : success (SystemServicesSuccess structure)
         * @return     : Core::hresult
         */
        Core::hresult SystemServicesImplementation::UploadLogsAsync(SystemServicesSuccess& success)
        {
            LOGWARN("");
            success.success = false;

            pid_t uploadLogsPid = -1;

            {
                std::lock_guard<std::mutex> lck(m_uploadLogsMutex);
                uploadLogsPid = m_uploadLogsPid;
            }

            if (-1 != uploadLogsPid) {
                LOGWARN("Another instance of log upload script is running");
                AbortLogUpload(success);
            }

            std::lock_guard<std::mutex> lck(m_uploadLogsMutex);
            m_uploadLogsPid = UploadLogs::logUploadAsync();
            success.success = true;

            return Core::ERROR_NONE;
        }

       /***
         * @brief : Stops background process to upload logs
         * @param1[out] : success (SystemServicesSuccess structure)
         * @return     : Core::hresult
         */
        Core::hresult SystemServicesImplementation::AbortLogUpload(SystemServicesSuccess& success)
        {
            success.success = false;

            std::lock_guard<std::mutex> lck(m_uploadLogsMutex);

            if (-1 != m_uploadLogsPid) {
                std::vector<int> processIds;
                bool result = Utils::getChildProcessIDs(m_uploadLogsPid, processIds);

                if (true == result) {
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

                if (SystemServicesImplementation::_instance) {
                    JsonObject params;
                    params["logUploadStatus"] = LOG_UPLOAD_STATUS_ABORTED;
                    SystemServicesImplementation::_instance->dispatchEvent(SYSTEMSERVICES_EVT_ONLOGUPLOAD, params);
                }

                success.success = true;
                return Core::ERROR_NONE;
            }

            LOGERR("Upload logs script is not running");
            return Core::ERROR_GENERAL;
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

	// @text requestSystemUptime
        // @brief Returns the device uptime.
        // @param systemUptime: The uptime, in seconds, of the device
        // @param success: Whether the request succeeded
        // @retval ErrorCode::ERROR_NONE: Indicates success
        // @retval ErrorCode::ERROR_GENERAL: Indicates failure
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
            } else {
                LOGERR("unable to evaluate uptime by clock_gettime");
	    }

            success = result;
            return (result ? Core::ERROR_NONE : Core::ERROR_GENERAL);
        }

	/**
         * @brief : API to query BuildType details
         *
         * @param1[in]  : {"params":{}}}
         * @param2[out] : "result":{<key>:<BuildType Info Details>,"success":<bool>}
         * @return      : Core::<StatusCode>
         */
        Core::hresult SystemServicesImplementation::GetBuildType(string& buildType, bool& success)
        {
            LOGINFOMETHOD();
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
            return (result ? Core::ERROR_NONE : Core::ERROR_GENERAL);
        }

        /***
         * @brief : sends notification when blocklist flag has changed.
         *
         * @param1[in]  : blocklist flag
         * @param2[out] : {"blocklist": <blocklist_flag>}
         */
        void SystemServicesImplementation::onBlocklistChanged(bool newBlocklistFlag, bool oldBlocklistFlag)
        {
            JsonObject params;
            string newBloklistStr = (newBlocklistFlag? "true":"false");
            string oldBloklistStr = (oldBlocklistFlag? "true":"false");

            params["oldBlocklistFlag"] = oldBlocklistFlag;
            params["newBlocklistFlag"] = newBlocklistFlag;
            LOGINFO("blocklist changed from %s to '%s'\n", oldBloklistStr.c_str(), newBloklistStr.c_str());
            dispatchEvent(SYSTEMSERVICES_EVT_ONBLOCKLISTCHANGED, params);
        }

       /***
         * @brief : To update Blocklist flag.
         * @param1[in]  : {"blocklist":"<true/false>"}
         * @param2[out] : {"result":{"success":<bool>}}
         * @return              : Core::<StatusCode>
         */
        Core::hresult SystemServicesImplementation::SetBlocklistFlag(const bool blocklist, BlocklistResult& result)
        {                
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
                    SystemServicesImplementation::_instance->onBlocklistChanged(blocklistFlag, oldBlocklistFlag);
                } else {
                    LOGERR("SystemServicesImplementation::_instance is NULL.\n");
                }
            }

            return (result.success ? Core::ERROR_NONE : Core::ERROR_GENERAL);
        }

        /***
         * @brief : To set the fsr flag into the emmc raw area.
         * @param1[in] : fsrFlag (bool)
         * @param2[out] : success (bool)
         * @return     : Core::hresult
         */
        Core::hresult SystemServicesImplementation::SetFSRFlag(const bool fsrFlag, bool& success)
        {
            LOGINFOMETHOD();
            success = false;

            IARM_Bus_MFRLib_FsrFlag_Param_t param;
            param = fsrFlag;
            LOGINFO("Param %d \n", param);
            IARM_Result_t res = IARM_Bus_Call(IARM_BUS_MFRLIB_NAME,
                                   IARM_BUS_MFRLIB_API_SetFsrFlag, (void *)&param,
                                   sizeof(param));
            if (IARM_RESULT_SUCCESS == res) {
                success = true;
            } else {
                success = false;
            }

            return (success ? Core::ERROR_NONE : Core::ERROR_GENERAL);
        }

        /***
         * @brief : To get the fsr flag from emmc
         * @param1[out] : fsrFlag (bool)
         * @param2[out] : success (bool)
         * @return     : Core::hresult
         */
        Core::hresult SystemServicesImplementation::GetFSRFlag(bool &fsrFlag, bool& success)
        {
            LOGINFOMETHOD();
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

            return (success ? Core::ERROR_NONE : Core::ERROR_GENERAL);
        }

        /***
         * @brief : To retrieve blocklist flag from persistent memory
         * @param1[out] : result (BlocklistResult)
         * @return     : Core::hresult
         */
        Core::hresult SystemServicesImplementation::GetBlocklistFlag(BlocklistResult& result)
        {
            LOGINFOMETHOD();
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

            return (result.success ? Core::ERROR_NONE : Core::ERROR_GENERAL);
        }

    } // namespace Plugin
} // namespace WPEFramework
