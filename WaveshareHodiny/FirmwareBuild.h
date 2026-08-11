#pragma once

#ifndef FIRMWARE_RELEASE
#define FIRMWARE_RELEASE 0
#endif

#if __has_include("local/firmware_config.h")
#include "local/firmware_config.h"
#endif

#if FIRMWARE_RELEASE && __has_include("local/release_version.h")
#include "local/release_version.h"
#endif

#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "development"
#endif

#ifndef FIRMWARE_SERVER_URL
#define FIRMWARE_SERVER_URL ""
#endif

#ifndef FIRMWARE_PROJECT_SLUG
#define FIRMWARE_PROJECT_SLUG ""
#endif

constexpr bool IS_RELEASE_FIRMWARE = FIRMWARE_RELEASE != 0;
constexpr char FIRMWARE_NAME[] = "Waveshare Hodiny";
constexpr char FIRMWARE_DEVICE_NAME[] = "Waveshare Hodiny";
constexpr char FIRMWARE_CHIP_VARIANT[] = "ESP32-S3";
