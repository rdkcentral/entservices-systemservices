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
    SYSTEMSERVICEL2TEST_STATE_INVALID = 0x00000000
}SystemServiceL2test_async_events_t;
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

    SystemService_L2Test();
    virtual ~SystemService_L2Test() override;

    public:
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
{
        uint32_t status = Core::ERROR_GENERAL;
        m_event_signalled = SYSTEMSERVICEL2TEST_STATE_INVALID;

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

         /* Activate plugin in constructor */
         status = ActivateService("org.rdk.PowerManager");
         EXPECT_EQ(Core::ERROR_NONE, status);

         /* Set all the asynchronouse event handler with IARM bus to handle various events*/
         ON_CALL(*p_iarmBusImplMock, IARM_Bus_RegisterEventHandler(::testing::_, ::testing::_, ::testing::_))
         .WillByDefault(::testing::Invoke(
             [&](const char* ownerName, IARM_EventId_t eventId, IARM_EventHandler_t handler) {
                 if ((string(IARM_BUS_SYSMGR_NAME) == string(ownerName)) && (eventId == IARM_BUS_SYSMGR_EVENT_SYSTEMSTATE)) {
                     systemStateChanged = handler;
                 }
                 return IARM_RESULT_SUCCESS;
         }));

         status = ActivateService("org.rdk.System");
         EXPECT_EQ(Core::ERROR_NONE, status);

}

/**
 * @brief Destructor for SystemServices L2 test class
 */
SystemService_L2Test::~SystemService_L2Test()
{
    uint32_t status = Core::ERROR_GENERAL;
    m_event_signalled = SYSTEMSERVICEL2TEST_STATE_INVALID;

    status = DeactivateService("org.rdk.System");
    EXPECT_EQ(Core::ERROR_NONE, status);

    EXPECT_CALL(*p_powerManagerHalMock, PLAT_TERM())
        .WillOnce(::testing::Return(PWRMGR_SUCCESS));

    EXPECT_CALL(*p_powerManagerHalMock, PLAT_DS_TERM())
        .WillOnce(::testing::Return(DEEPSLEEPMGR_SUCCESS));

    status = DeactivateService("org.rdk.PowerManager");
    EXPECT_EQ(Core::ERROR_NONE, status);
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
    uint32_t signalled = SYSTEMSERVICEL2TEST_STATE_INVALID;
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
    uint32_t signalled = SYSTEMSERVICEL2TEST_STATE_INVALID;
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
    uint32_t signalled = SYSTEMSERVICEL2TEST_STATE_INVALID;
    std::string message;
    JsonObject expected_status;
    uint32_t file_status = -1;

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

    /* Request status for Onlogupload. */
    message = "{\"oldBlocklistFlag\": true,\"newBlocklistFlag\": false}";
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
