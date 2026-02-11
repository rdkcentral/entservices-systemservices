/**
* If not stated otherwise in this file or this component's LICENSE
* file the following copyright and licenses apply:
*
* Copyright 2019 RDK Management
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

/**
 * @defgroup servicemanager
 * @{
 * @defgroup src
 * @{
 **/
#include <plugins/plugins.h>
#include <tracing/tracing.h>
#include <stdlib.h>

#include "thermonitor.h"
#include "libIARM.h"

#include "libIBus.h"

/**
Requirement: a generic IARMBus thermal monitoring class
api to get coretemperature  and notifications when thresholds are crossed.
 */


namespace WPEFramework {
    namespace Plugin {

        static CThermalMonitor s_instance;

        CThermalMonitor * CThermalMonitor::instance()
        {
            LOGWARN("%s: Requested thermal manager instance.\n", __FUNCTION__);
            return &s_instance;
        }

        void CThermalMonitor::addEventObserver(WPEFramework::Plugin::SystemServices* service)
        {
        }

        void CThermalMonitor::removeEventObserver(WPEFramework::Plugin::SystemServices* service)
        {
            LOGWARN("%s: Removing event observer for temperature threshold change.", __FUNCTION__);
        }

        bool CThermalMonitor::getCoreTemperature(float& temperature) const
        {
            Core::hresult retStatus = Core::ERROR_GENERAL;
            temperature = 0;
            bool result = false;
            WPEFramework::Exchange::IPowerManager* _powerManagerPlugin = SystemServices::_instance->getPwrMgrPluginInstance();

            ASSERT (nullptr != _powerManagerPlugin);
            if (nullptr != _powerManagerPlugin){
                retStatus = _powerManagerPlugin->GetThermalState(temperature);
            }

            if (Core::ERROR_NONE == retStatus) {
                LOGWARN("Current core temperature is : %f ",temperature);
                result = true;
            } else {
                LOGWARN("[%s] IARM Call failed.", __FUNCTION__);
            }

            return result;
        }

        bool CThermalMonitor::getCoreTempThresholds(float& high, float& critical) const
        {
            Core::hresult retStatus = Core::ERROR_GENERAL;
            bool result = false;
            WPEFramework::Exchange::IPowerManager* _powerManagerPlugin = SystemServices::_instance->getPwrMgrPluginInstance();

            ASSERT (nullptr != _powerManagerPlugin);
            if (nullptr != _powerManagerPlugin){
                retStatus = _powerManagerPlugin->GetTemperatureThresholds(high, critical);
            }

            if (Core::ERROR_NONE == retStatus) {
                LOGWARN("Got current temperature thresholds: high: %f, critical: %f", high, critical);
                result = true;
            } else {
                high = critical = 0;
                LOGWARN("[%s] IARM Call failed.", __FUNCTION__);
            }

            return result;
        }

        bool CThermalMonitor::setCoreTempThresholds(float high, float critical) const
        {
            Core::hresult retStatus = Core::ERROR_GENERAL;
            bool result = false;
            WPEFramework::Exchange::IPowerManager* _powerManagerPlugin = SystemServices::_instance->getPwrMgrPluginInstance();

            ASSERT (nullptr != _powerManagerPlugin);
            if (nullptr != _powerManagerPlugin){
                retStatus = _powerManagerPlugin->SetTemperatureThresholds(high, critical);
            }


            if (Core::ERROR_NONE == retStatus) {
                LOGWARN("Set new temperature thresholds: high: %f, critical: %f", high, critical);
                result = true;
            } else {
                LOGWARN("[%s] IARM Call failed.", __FUNCTION__);
            }

            return result;
        }

        bool CThermalMonitor::getOvertempGraceInterval(int& graceInterval) const
        {
            Core::hresult retStatus = Core::ERROR_GENERAL;
            bool result = false;
            WPEFramework::Exchange::IPowerManager* _powerManagerPlugin = SystemServices::_instance->getPwrMgrPluginInstance();

            ASSERT (nullptr != _powerManagerPlugin);
            if (nullptr != _powerManagerPlugin){
                retStatus = _powerManagerPlugin->GetOvertempGraceInterval(graceInterval);
            }

            if (Core::ERROR_NONE == retStatus) {
                LOGWARN("Got current overtemparature grace inetrval: %d", graceInterval);
                result = true;
            } else {
                graceInterval = 0;
                LOGWARN("[%s] IARM Call failed.", __FUNCTION__);
            }

            return result;
        }

        bool CThermalMonitor::setOvertempGraceInterval(int graceInterval) const
        {
            bool result = false;
            Core::hresult retStatus = Core::ERROR_GENERAL;
            WPEFramework::Exchange::IPowerManager* _powerManagerPlugin = SystemServices::_instance->getPwrMgrPluginInstance();

            ASSERT (nullptr != _powerManagerPlugin);
            if (nullptr != _powerManagerPlugin){
                retStatus = _powerManagerPlugin->SetOvertempGraceInterval(graceInterval);
            }

            if (Core::ERROR_NONE == retStatus) {
                LOGWARN("Set new overtemparature grace interval: %d", graceInterval);
                result = true;
            } else {
                LOGWARN("[%s] IARM Call failed.", __FUNCTION__);
            }

            return result;
        }

        void CThermalMonitor::emitTemperatureThresholdChange(std::string thresholdType, bool isAboveThreshold, float temperature)
        {
            LOGWARN("emitTemperatureThresholdChange invoked.");
            reportTemperatureThresholdChange(thresholdType, isAboveThreshold, temperature);
        }

        void CThermalMonitor::reportTemperatureThresholdChange(std::string thresholdType, bool isAboveThreshold, float temperature){
            LOGWARN("reportTemperatureThresholdChange called.");
        }
    }
}
/** @} */
/** @} */
