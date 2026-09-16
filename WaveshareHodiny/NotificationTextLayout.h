#pragma once

#include <lvgl.h>
#include <cmath>
#include <cstring>
#include "ClockFonts.h"

namespace notification_layout {
constexpr int SIZE = 480;
constexpr int RADIUS = 224;
constexpr int GAP = 18;
constexpr int MAX_LINES = 20;

struct Line {
  const char *text = nullptr;
  int length = 0;
  int y = 0;
  int width = 0;
  bool title = false;
  bool ellipsis = false;
};
struct Layout {
  Line lines[MAX_LINES];
  int count = 0;
  int height = 0;
  int messageBytes = 0;
  bool titleComplete = false;
  bool complete = false;
};
inline const lv_font_t *font(bool title) {
  return title ? &clock_notification_30 : &clock_notification_22;
}
inline int lineHeight(bool title) { return lv_font_get_line_height(font(title)); }

// Use the narrower chord at either edge of the line, not just its center.
// Thus the whole line rectangle stays inside the circle with a 16 px margin.
inline int lineWidth(int y, int height) {
  const int top = std::abs(y - SIZE / 2);
  const int bottom = std::abs(y + height - SIZE / 2);
  const int distance = top > bottom ? top : bottom;
  if (distance >= RADIUS) return 0;
  return 2 * static_cast<int>(std::sqrt(RADIUS * RADIUS - distance * distance));
}

inline bool append(Layout &layout, const char *text, bool title,
                   int &y, int bottom, int &consumed) {
  const int start = layout.count;
  const int height = lineHeight(title);
  consumed = 0;
  while (text[consumed] && layout.count < MAX_LINES && y + height <= bottom) {
    const int width = lineWidth(y, height);
    if (width < 64) break;
    const int length = _lv_txt_get_next_line(text + consumed, font(title),
                                            0, width, nullptr, LV_TEXT_FLAG_NONE);
    if (length <= 0) break;
    Line &line = layout.lines[layout.count++];
    line.text = text + consumed;
    line.length = length;
    // Break characters belong to the input stream but not the visible line.
    while (line.length && (line.text[line.length - 1] == '\n' ||
                          line.text[line.length - 1] == ' ')) --line.length;
    line.title = title;
    line.y = y;
    line.width = width;
    consumed += length;
    y += height;
  }
  if (text[consumed] && layout.count > start) {
    Line &last = layout.lines[layout.count - 1];
    last.ellipsis = true;
    last.length = strlen(last.text);
  }
  return !text[consumed];
}

inline bool center(Layout &layout) {
  if (layout.count < 2) return false;
  const Line &last = layout.lines[layout.count - 1];
  layout.height = last.y + lineHeight(last.title) - layout.lines[0].y;
  const int offset = (SIZE - layout.height) / 2 - layout.lines[0].y;
  for (int i = 0; i < layout.count; ++i) {
    Line &line = layout.lines[i];
    line.y += offset;
    line.width = lineWidth(line.y, lineHeight(line.title));
    if (line.width < 64) return false;
    // Moving to the exact vertical center may narrow a row. Reject layouts
    // whose already wrapped text would no longer fit that row.
    if (!line.ellipsis && lv_txt_get_width(line.text, line.length, font(line.title),
                                           0, LV_TEXT_FLAG_NONE) > line.width)
      return false;
  }
  return true;
}

inline Layout fit(const char *title, const char *message) {
  Layout best;
  // Try starting heights, wrap using the actual LVGL font metrics, then
  // recenter and verify every row. This bounded search runs only on arrival.
  for (int top = SIZE / 2 - RADIUS; top < SIZE / 2; ++top) {
    Layout candidate;
    int y = top;
    int titleBytes = 0;
    candidate.titleComplete = append(candidate, title, true, y,
        SIZE / 2 + RADIUS - GAP - lineHeight(false), titleBytes);
    if (!candidate.count) continue;
    const int titleLines = candidate.count;
    y += GAP;
    candidate.complete = append(candidate, message, false, y, SIZE / 2 + RADIUS,
                                  candidate.messageBytes) && candidate.titleComplete;
    if (candidate.count == titleLines || !center(candidate)) continue;
    const bool better = !best.count ||
        (candidate.complete && !best.complete) ||
        (candidate.complete && best.complete && candidate.height < best.height) ||
        (!candidate.complete && !best.complete &&
          ((candidate.titleComplete && !best.titleComplete) ||
           (candidate.titleComplete == best.titleComplete &&
             (candidate.messageBytes > best.messageBytes ||
              (candidate.messageBytes == best.messageBytes && candidate.height < best.height)))));
    if (better) best = candidate;
  }
  return best;
}
} // namespace notification_layout
