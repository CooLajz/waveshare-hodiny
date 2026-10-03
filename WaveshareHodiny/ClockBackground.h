#pragma once
#include <Arduino.h>
namespace fs { class File; }
using fs::File;
constexpr size_t CLOCK_BACKGROUND_BYTES = 480U * 480U * 2U;
struct ClockBackgroundOptions {
  uint32_t magic = 0x42474B31;
  uint32_t checksum = 0;
  uint8_t slot = 0;
  uint8_t present = 0;
  // Starší firmware tuto volbu neukládal. Chybějící záznam po OTA proto musí
  // znamenat vypnuto; nový obrázek se zapne až explicitním potvrzením uploadu.
  uint8_t enabled = 0;
  uint8_t opacity = 35;
  uint8_t shadow = 70;
  uint8_t shadowSize = 2; // Pixel offset, including zero for a centered outline.
  uint8_t shadowSpread = 0;
  uint8_t clockOnly = 0; // Analog face: hide information, retain live data updates.
};
void clockBackgroundBegin();
const ClockBackgroundOptions &clockBackgroundOptions();
const ClockBackgroundOptions &clockBackgroundActiveOptions();
bool clockBackgroundPreview(bool enabled, uint8_t opacity, uint8_t shadow, uint8_t shadowSize, uint8_t shadowSpread, bool clockOnly = false);
const uint8_t *clockBackgroundPixels();
uint32_t clockBackgroundRevision();
bool clockBackgroundStart(String &token);
bool clockBackgroundChunk(const String &token, size_t offset, const String &hex);
bool clockBackgroundCommit(const String &token, const ClockBackgroundOptions *requested = nullptr);
bool clockBackgroundCancel(const String &token);
bool clockBackgroundSaveOptions(bool enabled, uint8_t opacity, uint8_t shadow, uint8_t shadowSize, uint8_t shadowSpread, bool remove, bool clockOnly = false);
void clockBackgroundLoop();

// Bounded backup I/O: never allocates another framebuffer or selects a slot.
bool clockBackgroundOptionsValid(const ClockBackgroundOptions &value);
bool clockBackgroundBackupRead(size_t offset, uint8_t *data, size_t length);
bool clockBackgroundRestoreWrite(size_t offset, const uint8_t *data, size_t length);
bool clockBackgroundStageRestore(ClockBackgroundOptions next, size_t imageBytes);
void clockBackgroundRestoreEnd();

File clockBackgroundOpenImage();
void clockBackgroundSetResident(bool wanted);
bool clockBackgroundStageOptions(const ClockBackgroundOptions &next);
void clockBackgroundAdoptOptions(const ClockBackgroundOptions &next);
