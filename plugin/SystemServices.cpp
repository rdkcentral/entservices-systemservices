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

#include "SystemServices.h"

#define API_VERSION_NUMBER_MAJOR 1
#define API_VERSION_NUMBER_MINOR 0
#define API_VERSION_NUMBER_PATCH 0

namespace WPEFramework
{

    namespace {

        static Plugin::Metadata<Plugin::SystemServices> metadata(
            // Version (Major, Minor, Patch)
            API_VERSION_NUMBER_MAJOR, API_VERSION_NUMBER_MINOR, API_VERSION_NUMBER_PATCH,
            // Preconditions
            {},
            // Terminations
            {},
            // Controls
            {}
        );
    }

    namespace Plugin
    {

    /*
     *Register SystemServices module as wpeframework plugin
     **/
    SERVICE_REGISTRATION(SystemServices, API_VERSION_NUMBER_MAJOR, API_VERSION_NUMBER_MINOR, API_VERSION_NUMBER_PATCH);

    SystemServices::SystemServices() : _service(nullptr), _connectionId(0), _systemServices(nullptr), _systemServicesNotification(this)
    {
        SYSLOG(Logging::Startup, (_T("Systemservices Constructor")));
    }

    SystemServices::~SystemServices()
    {
        SYSLOG(Logging::Shutdown, (string(_T("SystemServices Destructor"))));
    }

    const string SystemServices::Initialize(PluginHost::IShell* service)
    {
        string message="";

        ASSERT(nullptr != service);
        ASSERT(nullptr == _service);
        ASSERT(nullptr == _systemServices);
        ASSERT(0 == _connectionId);

        SYSLOG(Logging::Startup, (_T("SystemServices::Initialize: PID=%u"), getpid()));

        _service = service;
        _service->AddRef();
        _service->Register(&_systemServicesNotification);
        _systemServices = _service->Root<Exchange::ISystemServices>(_connectionId, 5000, _T("SystemServicesImplementation"));

        if(nullptr != _systemServices)
        {
            _configure = _systemServices->QueryInterface<Exchange::IConfiguration>();
            if (_configure != nullptr)
            {
                uint32_t result = _configure->Configure(_service);
                if(result != Core::ERROR_NONE)
                {
                    message = _T("SystemServices could not be configured");
                }
            }
            else
            {
                message = _T("SystemServices implementation did not provide a configuration interface");
            }
            // Register for notifications
            _systemServices->Register(&_systemServicesNotification);
            // Invoking Plugin API register to wpeframework
            Exchange::JSystemServices::Register(*this, _systemServices);
        }
        else
        {
            SYSLOG(Logging::Startup, (_T("SystemServices::Initialize: Failed to initialise SystemServices plugin")));
            message = _T("SystemServices plugin could not be initialised");
        }

        return message;
    }

    void SystemServices::Deinitialize(PluginHost::IShell* service)
    {
        ASSERT(_service == service);

        SYSLOG(Logging::Shutdown, (string(_T("SystemServices::Deinitialize"))));

        // Make sure the Activated and Deactivated are no longer called before we start cleaning up..
        _service->Unregister(&_systemServicesNotification);

        if (nullptr != _systemServices)
        {
            _systemServices->Unregister(&_systemServicesNotification);
            Exchange::JSystemServices::Unregister(*this);

            if (_configure != nullptr) {
                _configure->Release();
                _configure = nullptr;
            }

            // Stop processing:
            RPC::IRemoteConnection* connection = service->RemoteConnection(_connectionId);
            VARIABLE_IS_NOT_USED uint32_t result = _systemServices->Release();

            _systemServices = nullptr;

            // It should have been the last reference we are releasing,
            // so it should endup in a DESTRUCTION_SUCCEEDED, if not we
            // are leaking...
            ASSERT(result == Core::ERROR_DESTRUCTION_SUCCEEDED);

            // If this was running in a (container) process...
            if (nullptr != connection)
            {
               // Lets trigger the cleanup sequence for
               // out-of-process code. Which will guard
               // that unwilling processes, get shot if
               // not stopped friendly :-)
               try
               {
                   connection->Terminate();
                   // Log success if needed
                   LOGWARN("Connection terminated successfully.");
                case WPEFramework::Exchange::IPowerManager::POWER_STATE_ON: powerState = "ON"; break;
                case WPEFramework::Exchange::IPowerManager::POWER_STATE_OFF: powerState = "OFF"; break;
                case WPEFramework::Exchange::IPowerManager::POWER_STATE_STANDBY: powerState = "LIGHT_SLEEP"; break;
                case WPEFramework::Exchange::IPowerManager::POWER_STATE_STANDBY_LIGHT_SLEEP: powerState = "LIGHT_SLEEP"; break;
                case WPEFramework::Exchange::IPowerManager::POWER_STATE_STANDBY_DEEP_SLEEP: powerState = "DEEP_SLEEP"; break;
                default: break;
            }
            return powerState;
        }


        void SystemServices::onNetworkStandbyModeChanged(const bool enabled)
        {
            if (SystemServices::_instance) {
                SystemServices::_instance->onNetworkModeChanged(enabled);
            } else {
                LOGERR("SystemServices::_instance is NULL.\n");
            }
        }

        void SystemServices::onThermalModeChanged(const ThermalTemperature currentThermalLevel, const ThermalTemperature newThermalLevel, const float currentTemperature)
        {
            handleThermalLevelChange(currentThermalLevel, newThermalLevel, currentTemperature);
        }

        void SystemServices::onRebootBegin(const string &rebootReasonCustom, const string &rebootReasonOther, const string &rebootRequestor)
        {
            if (SystemServices::_instance) {
                SystemServices::_instance->onPwrMgrReboot(rebootRequestor, rebootReasonOther);
            } else {
                LOGERR("SystemServices::_instance is NULL.\n");
            }
        }

        uint32_t SystemServices::requestSystemReboot(const JsonObject& parameters,
                JsonObject& response)
        {
            Core::hresult status = Core::ERROR_GENERAL;
            bool nfxResult = false;
            string customReason = "No custom reason provided";
            string otherReason = "No other reason supplied";
            string requestor = "SystemServices";
            bool result = false;
            string fname = "nrdplugin";

            nfxResult = Utils::killProcess(fname);
            if (true == nfxResult) {
                LOGINFO("SystemService shutting down Netflix...\n");
                //give Netflix process some time to terminate gracefully.
                sleep(5);
            } else {
                LOGINFO("SystemService unable to shutdown Netflix \
                        process. nfxResult = %ld\n", (long int)nfxResult);
            }

            if (parameters.HasLabel("rebootReason")) {
                customReason = parameters["rebootReason"].String();
                otherReason = customReason;
            }

            LOGINFO("requestSystemReboot: custom reason: %s, other reason: %s\n", customReason.c_str(),
                otherReason.c_str());

            ASSERT (_powerManagerPlugin);
            if (_powerManagerPlugin){
                status = _powerManagerPlugin->Reboot(requestor, customReason, otherReason);
                result = true;
            } else {
                status = Core::ERROR_ILLEGAL_STATE;
            }

            if (status != Core::ERROR_NONE){
                 LOGWARN("requestSystemReboot: powerManagerPlugin->rebooot failed\n");
            }

            response["IARM_Bus_Call_STATUS"] = static_cast <int32_t> (status);

            returnResponse(result);
        }//end of requestSystemReboot

#if 0
        /*
         * @brief This function delays the reboot in seconds.
         * This will internally sets the tr181 fwDelayReboot parameter.
         * @param1[in]: {"jsonrpc":"2.0","id":"3","method":"org.rdk.System.2.setFirmwareRebootDelay",
         *                  "params":{"delaySeconds": int seconds}}''
         * @param2[out]: {"jsonrpc":"2.0","id":3,"result":{"success":<bool>}}
         * @return: Core::<StatusCode>
         */

        uint32_t SystemServices::setFirmwareRebootDelay(const JsonObject& parameters,
                JsonObject& response)
        {
            bool result = false;
            uint32_t delay_in_sec = 0;

            if ( parameters.HasLabel("delaySeconds") ){
                /* get the value */
                delay_in_sec = static_cast<unsigned int>(parameters["delaySeconds"].Number());

                /* we can delay with max 24 Hrs = 86400 sec */
                if (delay_in_sec > 0 && delay_in_sec <= MAX_REBOOT_DELAY ){

                    std::string delaySeconds = parameters["delaySeconds"].String();
                    const char * set_rfc_val = delaySeconds.c_str();

                    LOGINFO("set_rfc_value %s\n",set_rfc_val);

                    /*set tr181Set command from here*/
                    WDMP_STATUS status = setRFCParameter((char*)"thunderapi",
                            TR181_FW_DELAY_REBOOT, set_rfc_val, WDMP_INT);
                    if ( WDMP_SUCCESS == status ){
                        result=true;
                        LOGINFO("Success Setting setFirmwareRebootDelay value\n");
                    }
                    else {
                        LOGINFO("Failed Setting setFirmwareRebootDelay value %s\n",getRFCErrorString(status));
                    }
                }
                else {
                    /* we didnt get a valid Auto Reboot delay */
                    LOGERR("Invalid setFirmwareRebootDelay Value Max.Value is 86400 sec\n");
                }
            }
            else {
                /* havent got the correct label */
                LOGERR("setFirmwareRebootDelay Missing Key Values\n");
                populateResponseWithError(SysSrv_MissingKeyValues,response);
            }
            returnResponse(result);
        }
#endif

        /*
         * @brief This function Enable/Disable the AutReboot Feature.
         * This will internally sets the tr181 AutoReboot.Enable to True/False.
         * @param1[in]: {"jsonrpc":"2.0","id":"3","method":"org.rdk.System.2.setFirmwareAutoReboot",
         *                  "params":{"enable": bool }}''
         * @param2[out]: {"jsonrpc":"2.0","id":3,"result":{"success":<bool>}}
         * @return: Core::<StatusCode>
         */

        uint32_t SystemServices::setFirmwareAutoReboot(const JsonObject& parameters,
                JsonObject& response)
        {
            bool result = false;
            bool enableFwAutoreboot = false;

           if ( parameters.HasLabel("enable") ){
               /* get the value */
               enableFwAutoreboot = (parameters["enable"].Boolean());
               LOGINFO("setFirmwareAutoReboot : %s\n",(enableFwAutoreboot)? "true":"false");

               std::string enable = parameters["enable"].String();
               const char * set_rfc_val = enable.c_str();

               /* set tr181Set command from here */
               WDMP_STATUS status = setRFCParameter((char*)"thunderapi",
                       TR181_AUTOREBOOT_ENABLE,set_rfc_val,WDMP_BOOLEAN);
               if ( WDMP_SUCCESS == status ){
                   result=true;
                   LOGINFO("Success Setting the setFirmwareAutoReboot value\n");
               }
               catch (const std::exception& e)
               {
                   std::string errorMessage = "Failed to terminate connection: ";
                   errorMessage += e.what();
                   LOGWARN("%s",errorMessage.c_str());
               }

               connection->Release();
            }
        }

        _connectionId = 0;
        _service->Release();
        _service = nullptr;
        SYSLOG(Logging::Shutdown, (string(_T("SystemServices de-initialised"))));
    }

    string SystemServices::Information() const
    {
       return "The SystemServices plugin is used to manage various system-level features.";
    }

    void SystemServices::Deactivated(RPC::IRemoteConnection* connection)
    {
        if (connection->Id() == _connectionId) {
            ASSERT(nullptr != _service);
            Core::IWorkerPool::Instance().Submit(PluginHost::IShell::Job::Create(_service, PluginHost::IShell::DEACTIVATED, PluginHost::IShell::FAILURE));
        }
    }
} // namespace Plugin
} // namespace WPEFramework
