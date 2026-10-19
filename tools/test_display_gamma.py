#!/usr/bin/env python3
"""Exercise production gamma writes, model guards and SPI-failure cleanup."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / "WaveshareHodiny/Display_ST7701.cpp").read_text()
start = source.index("static bool ST7701_WriteGamma(")
gamma = source[start:source.index("static void ST7701_InitPanel21()", start)]
start = source.index("static bool ST7701_SelectPage(")
diagnostic = source[start:source.index("\n#endif", start)]
code = r"""
#include "Display_ST7701Profiles.h"
#include <cassert>
#include <cstdio>
#include <vector>
constexpr int ESP_OK = 0, ESP_FAIL = -1;
enum class TouchController { Unknown, CST820, GT911 };
TouchController controller = TouchController::GT911;
TouchController Touch_GetController() { return controller; }
void *panel_handle = reinterpret_cast<void *>(1);
bool panelSleeping = false, storageBlanked = false, selected = false;
struct Write { bool command; uint8_t value; };
std::vector<Write> writes;
int failAt = -1;
int transfer(bool command, uint8_t value) {
  assert(selected);
  const int position = writes.size();
  writes.push_back({command, value});
  return position == failAt ? ESP_FAIL : ESP_OK;
}
int ST7701_WriteCommand(uint8_t value) { return transfer(true, value); }
int ST7701_WriteData(uint8_t value) { return transfer(false, value); }
void ST7701_CS_EN() { assert(!selected); selected = true; }
void ST7701_CS_Dis() { assert(selected); selected = false; }
""" + gamma + diagnostic + r"""
void expectPage(size_t offset, uint8_t page) {
  assert(writes[offset].command && writes[offset].value == 0xff);
  const uint8_t expected[] = {0x77, 1, 0, 0, page};
  for (size_t i = 0; i < 5; ++i)
    assert(!writes[offset + 1 + i].command && writes[offset + 1 + i].value == expected[i]);
}
int main() {
  for (bool reference21 : {false, true}) {
    writes.clear();
    assert(LCD_SetDiagnosticGamma(reference21));
    assert(!selected && writes.size() == 46);
    expectPage(0, 0x10); expectPage(40, 0);
    assert(writes[6].command && writes[6].value == 0xb0);
    assert(writes[23].command && writes[23].value == 0xb1);
    const uint8_t *positive = ST7701_PANEL_21_POSITIVE_GAMMA;
    const uint8_t *negative = ST7701_PANEL_21_NEGATIVE_GAMMA;
    if (!reference21) for (const auto &entry : ST7701_PANEL_28C) {
      if (entry.size == 16 && entry.command == 0xb0) positive = entry.data;
      if (entry.size == 16 && entry.command == 0xb1) negative = entry.data;
    }
    for (size_t i = 0; i < 16; ++i) {
      assert(!writes[7+i].command && writes[7+i].value == positive[i]);
      assert(!writes[24+i].command && writes[24+i].value == negative[i]);
    }
  }
  // No BK1 supply-voltage commands are emitted. Every possible failed SPI
  // transfer is reported; chip select is always released, including restore errors.
  for (int failure = 0; failure < 46; ++failure) {
    writes.clear(); failAt = failure;
    assert(!LCD_SetDiagnosticGamma(true));
    assert(!selected);
    if (failure < 40) expectPage(writes.size() - 6, 0);
  }
  failAt = -1;
  for (auto model : {TouchController::Unknown, TouchController::CST820}) {
    writes.clear(); controller = model;
    assert(!LCD_SetDiagnosticGamma(true) && writes.empty());
  }
  controller = TouchController::GT911;
  writes.clear(); panelSleeping = true;
  assert(!LCD_SetDiagnosticGamma(true) && writes.empty());
  panelSleeping = false; storageBlanked = true;
  assert(!LCD_SetDiagnosticGamma(true) && writes.empty());
  storageBlanked = false; panel_handle = nullptr;
  assert(!LCD_SetDiagnosticGamma(true) && writes.empty());
  puts("PASS: gamma profiles, exact register writes, unchanged voltage bank, 46 SPI failures and model/state guards");
}
"""
with tempfile.TemporaryDirectory(prefix="waveshare-gamma-") as directory:
    path = Path(directory)
    (path / "test.cpp").write_text(code)
    subprocess.run(["clang++", "-std=c++17", "-fsanitize=address,undefined",
                    "-I" + str(root / "WaveshareHodiny"), str(path / "test.cpp"),
                    "-o", str(path / "test")], check=True)
    subprocess.run([str(path / "test")], check=True)
