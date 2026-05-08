/*
 * Stub header for systimerifc/itimermsg.h
 * Required when ENABLE_SYSTIMEMGR_SUPPORT is defined in test builds
 * where the real systimemgr package is not installed.
 */
#pragma once

#define IARM_BUS_SYSTIME_MGR_NAME    "SYSTIME_MGR"
#define TIMER_STATUS_MSG             "TimerStatus"
#define cTIMER_STATUS_UPDATE         0
#define cTIMER_STATUS_MESSAGE_LENGTH 128

typedef struct {
    char message[cTIMER_STATUS_MESSAGE_LENGTH];
    char timerSrc[cTIMER_STATUS_MESSAGE_LENGTH];
    char currentTime[cTIMER_STATUS_MESSAGE_LENGTH];
} TimerMsg;
