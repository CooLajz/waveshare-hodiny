#pragma once
#include "AlarmRules.h"
using AlarmSaveCallback = bool (*)(const AlarmSettings &);
void alarmServiceBegin(AlarmSaveCallback save);
void alarmServiceApply(const AlarmSettings &settings, bool english);
void alarmServiceLoop();
bool alarmServiceActive();
// Also consume late duplicate touch events immediately after dismissal.
bool alarmServiceBlocksTouch();
void alarmServiceDismiss();
bool alarmServiceSetEnabled(bool enabled);
bool alarmServiceSkipNext();
const AlarmSettings &alarmServiceSettings();
void alarmServiceDescribeNext(char *text, unsigned size);
