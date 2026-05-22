/*
 * Stub header for systimerifc/itimermsg.h
 * Required when ENABLE_SYSTIMEMGR_SUPPORT is defined in test builds
 * where the real systimemgr package is not installed.
 */
#pragma once

#ifndef IARM_BUS_SYSTIME_MGR_NAME
#define IARM_BUS_SYSTIME_MGR_NAME    "SYSTEMTIME"
#endif

#ifndef TIMER_STATUS_MSG
#define TIMER_STATUS_MSG             "TimerStatus"
#endif

#ifndef cTIMER_STATUS_UPDATE
#define cTIMER_STATUS_UPDATE         0
#endif

#ifndef cTIMER_STATUS_MESSAGE_LENGTH
#define cTIMER_STATUS_MESSAGE_LENGTH 256
#endif

typedef struct {
    char message[cTIMER_STATUS_MESSAGE_LENGTH];
    char timerSrc[cTIMER_STATUS_MESSAGE_LENGTH];
    char currentTime[cTIMER_STATUS_MESSAGE_LENGTH];
} TimerMsg;
