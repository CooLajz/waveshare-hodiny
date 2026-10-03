#!/usr/bin/env python3
"""Run the production clock-only transition function with a minimal LVGL model.

Only parenting, hidden flags and GIF timers are modeled; pixel rendering remains
covered by device screenshots. No copy of the transition implementation lives here.
"""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'WaveshareHodiny/ClockDashboard.cpp').read_text()
start = source.index('void updateClockOnlyPresentation() {')
end = source.index('\nvoid clockDashboardLoop()', start)
model = r'''
#include <cassert>
#include <initializer_list>
struct lv_obj_t { lv_obj_t *parent = nullptr; bool hidden = false; };
struct lv_timer_t { bool paused = false; };
struct lv_gif_t : lv_obj_t { lv_timer_t *timer; };
constexpr int LV_OBJ_FLAG_HIDDEN = 1;
constexpr int CLOCK_STYLE_ANALOG=1, CLOCK_STYLE_DIGITAL=0, CLOCK_STYLE_FORECAST=3;
lv_obj_t page, content, dial, hands;
lv_obj_t *clockPage=&page, *dashboardContent=&content, *analogDialLayer=&dial, *analogHandsLayer=&hands;
lv_timer_t leftTimer, rightTimer;
lv_gif_t leftGif, rightGif;
lv_obj_t *weatherAnimation=&leftGif, *roomWeatherAnimation=&rightGif;
bool requested=false, radarVisible=false, settingsVisible=false, retro=false, animatedWeatherIconsEnabled=true;
int activeClockStyle=CLOCK_STYLE_ANALOG, currentValues=0;
char leftWeatherDecoderKey[]="loaded", rightWeatherDecoderKey[]="loaded";
bool analogLayoutEnabled(){return activeClockStyle==CLOCK_STYLE_ANALOG;}
bool retroLcdEnabled(){return retro;}
bool analogClockOnly(){return requested && analogLayoutEnabled() && !retro;}
lv_obj_t *lv_obj_get_parent(lv_obj_t *o){return o->parent;}
void lv_obj_set_parent(lv_obj_t *o,lv_obj_t *p){o->parent=p;}
void lv_obj_add_flag(lv_obj_t *o,int){o->hidden=true;}
void lv_obj_clear_flag(lv_obj_t *o,int){o->hidden=false;}
void lv_timer_resume(lv_timer_t *t){t->paused=false;}
void lv_timer_pause(lv_timer_t *t){t->paused=true;}
void lv_obj_move_background(lv_obj_t*){}
void lv_obj_invalidate(lv_obj_t*){}
void updateAnalogValueLayerOrder(){}
void retroLcdRaise(){}
void clockDashboardUpdate(int){}
void releaseShadowCache(){}
void reset(){
 page={};content={&page,false};dial={&content,false};hands={&content,false};
 leftTimer={};rightTimer={};leftGif.timer=&leftTimer;rightGif.timer=&rightTimer;
 requested=radarVisible=settingsVisible=retro=false;activeClockStyle=CLOCK_STYLE_ANALOG;
}
'''
cases = r'''
int main(){
 reset();requested=true;updateClockOnlyPresentation();
 assert(content.hidden && dial.parent==&page && hands.parent==&page);
 assert(leftTimer.paused && rightTimer.paused);
 settingsVisible=true;updateClockOnlyPresentation();assert(dial.hidden && hands.hidden);
 requested=false;updateClockOnlyPresentation();
 assert(!content.hidden && !dial.hidden && !hands.hidden && dial.parent==&content);
 assert(!leftTimer.paused && !rightTimer.paused);
 settingsVisible=false;updateClockOnlyPresentation();assert(!content.hidden && !hands.hidden);
 reset();requested=true;updateClockOnlyPresentation();
 radarVisible=true;updateClockOnlyPresentation();assert(dial.hidden);
 requested=false;updateClockOnlyPresentation();assert(content.hidden && !hands.hidden);
 radarVisible=false;lv_obj_clear_flag(&content,LV_OBJ_FLAG_HIDDEN);updateClockOnlyPresentation();
 assert(!content.hidden && !hands.hidden);
 reset();requested=true;updateClockOnlyPresentation();
 activeClockStyle=CLOCK_STYLE_FORECAST;content.hidden=dial.hidden=hands.hidden=true;
 updateClockOnlyPresentation();assert(content.hidden && dial.hidden && hands.hidden);
 reset();requested=true;updateClockOnlyPresentation();
 activeClockStyle=CLOCK_STYLE_DIGITAL;dial.hidden=hands.hidden=true;
 updateClockOnlyPresentation();assert(!content.hidden && dial.hidden && hands.hidden);
}
'''
with tempfile.TemporaryDirectory(prefix='clock-only-test-') as folder:
    cpp = Path(folder) / 'test.cpp'
    binary = Path(folder) / 'test'
    cpp.write_text(model + source[start:end] + cases)
    subprocess.run(['c++', '-std=c++17', '-fsanitize=address,undefined', str(cpp), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
print('PASS: clock-only/settings/radar/digital/forecast transitions and GIF pause/resume')
