#include "RetroLcd.h"
#include "ClockFonts.h"
#include "RetroLcdFormat.h"
#include <cmath>
#include <cstring>
#include <algorithm>

namespace {
lv_obj_t *face = nullptr;
tm clockTime = {};
bool timeAvailable = false;
bool english = false, night = false, wifi = false, web = false, ha = false;
char names[2][CLOCK_METRIC_NAME_LENGTH] = {};
char units[2][CLOCK_METRIC_SUFFIX_LENGTH] = {};
char numbers[2][32] = {"--", "--"};
uint8_t digitPlaces[2] = {3, 4};
bool negativeValues[2] = {false, false};
uint8_t ghostOpacity = 5;
float progress = NAN;
bool progressEnabled = false;
uint8_t progressSegments = 10;
char progressText[160] = {};
uint32_t backgroundColor = 0xB7C1A5, foregroundColor = 0x20261C;

lv_color_t ink() { return lv_color_hex(night ? 0xC34436 : foregroundColor); }
lv_color_t background() { return lv_color_hex(night ? 0x080A08 : backgroundColor); }
lv_color_t ghost(bool subtle = false) {
  // Stejný jemný kontrast pro světlé i tmavé uživatelské pozadí.
  const unsigned opacity = (ghostOpacity * 255U + 50U) / 100U;
  return lv_color_mix(ink(), background(), subtle ? (opacity * 5U + 7U) / 14U : opacity);
}

void invalidate(int x, int y, int w, int h) {
  if (!face) return;
  lv_area_t area;
  lv_obj_get_coords(face, &area);
  area.x1 += x;
  area.y1 += y;
  area.x2 = area.x1 + w - 1;
  area.y2 = area.y1 + h - 1;
  lv_obj_invalidate_area(face, &area);
}

struct Painter {
  lv_draw_ctx_t *ctx;
  int ox, oy;

  void rect(int x, int y, int w, int h, lv_color_t color) {
    lv_draw_rect_dsc_t d;
    lv_draw_rect_dsc_init(&d);
    d.bg_color = color;
    lv_area_t area = {static_cast<lv_coord_t>(ox+x), static_cast<lv_coord_t>(oy+y),
                     static_cast<lv_coord_t>(ox+x+w-1), static_cast<lv_coord_t>(oy+y+h-1)};
    lv_draw_rect(ctx, &d, &area);
  }

  // Šestiboký segment se seříznutými konci, společný pro čísla i písmena.
  void segment(float x1, float y1, float x2, float y2, float thickness,
               lv_color_t color) {
    const float dx = x2-x1, dy = y2-y1;
    const float length = std::sqrt(dx*dx+dy*dy);
    if (length <= thickness) return;
    const float ux = dx/length, uy = dy/length;
    const float r = thickness/2;
    const float px = -uy*r, py = ux*r;
    const float coordinates[6][2] = {
      {x1,y1}, {x1+ux*r+px,y1+uy*r+py}, {x2-ux*r+px,y2-uy*r+py},
      {x2,y2}, {x2-ux*r-px,y2-uy*r-py}, {x1+ux*r-px,y1+uy*r-py}
    };
    lv_point_t points[6];
    for (int i=0;i<6;++i) points[i] = {
      static_cast<lv_coord_t>(std::lround(ox+coordinates[i][0])),
      static_cast<lv_coord_t>(std::lround(oy+coordinates[i][1]))};
    lv_draw_rect_dsc_t d;
    lv_draw_rect_dsc_init(&d);
    d.bg_color = color;
    lv_draw_polygon(ctx, &d, points, 6);
  }

  void label(const char *text, int x, int y, int w, const lv_font_t *font,
             lv_color_t color, lv_text_align_t align = LV_TEXT_ALIGN_CENTER) {
    lv_draw_label_dsc_t d;
    lv_draw_label_dsc_init(&d);
    d.font = font;
    d.color = color;
    d.align = align;
    lv_area_t area = {static_cast<lv_coord_t>(ox+x),static_cast<lv_coord_t>(oy+y),
                     static_cast<lv_coord_t>(ox+x+w-1),static_cast<lv_coord_t>(oy+y+font->line_height-1)};
    // Jeden řádek, dlouhé vlastní názvy jsou oříznuté na šířku slotu.
    lv_area_t clip;
    if (!_lv_area_intersect(&clip,ctx->clip_area,&area)) return;
    const lv_area_t *old = ctx->clip_area;
    ctx->clip_area = &clip;
    d.flag = LV_TEXT_FLAG_EXPAND;
    lv_draw_label(ctx,&d,&area,text,nullptr);
    ctx->clip_area = old;
  }

  void digit(char c, int x, int y, int w, int h, bool alphabet=false) {
    static const uint8_t digits[] = {0x3F,0x06,0x5B,0x4F,0x66,0x6D,0x7D,0x07,0x7F,0x6F};
    uint16_t mask = c>='0' && c<='9' ? digits[c-'0'] : c=='-' ? 0x40 : 0;
    // A B C D E F G1 G2 H I J K L M; diagonály a středové svislice.
    if (alphabet) {
      switch(c) {
        case 'A': mask=0xF7; break;
        case 'B': mask=0x128F; break;
        case 'C': mask=0x39; break;
        case 'D': mask=0x120F; break;
        case 'E': mask=0xF9; break;
        case 'F': mask=0xF1; break;
        case 'G': mask=0xBD; break;
        case 'H': mask=0xF6; break;
        case 'I': mask=0x1209; break;
        case 'K': mask=0xC30; break;
        case 'L': mask=0x38; break;
        case 'M': mask=0x536; break;
        case 'N': mask=0x936; break;
        case 'O': mask=0x3F; break;
        case 'P': mask=0xF3; break;
        case 'R': mask=0x8F3; break;
        case 'S': mask=0xED; break;
        case 'T': mask=0x1201; break;
        case 'U': mask=0x3E; break;
        case 'V': mask=0x2430; break;
        case 'W': mask=0x2836; break;
        case 'Y': mask=0x1500; break;
      }
    }
    const float t = std::max(2.0f, h*(alphabet ? .073f : .086f));
    const float l=x+t/2, r=x+w-t/2;
    // Shodná pixelová fáze všech vodorovných tahů; jinak měl střed data
    // kvůli půlpixelové souřadnici čtyři řádky místo tří.
    const float top=std::round(y+t/2), mid=std::round(y+h/2.0f), bot=std::round(y+h-t/2);
    const float cx=(l+r)/2, gap=1.4f;
    const float lines[14][4] = {
      {l+gap,top,r-gap,top}, {r,top+gap,r,mid-gap}, {r,mid+gap,r,bot-gap},
      {l+gap,bot,r-gap,bot}, {l,mid+gap,l,bot-gap}, {l,top+gap,l,mid-gap},
      {l+gap,mid,alphabet?cx-gap:r-gap,mid}, {cx+gap,mid,r-gap,mid},
      {l+gap,top+gap,cx-gap,mid-gap}, {cx,top+gap,cx,mid-gap},
      {r-gap,top+gap,cx+gap,mid-gap}, {cx+gap,mid+gap,r-gap,bot-gap},
      {cx,mid+gap,cx,bot-gap}, {cx-gap,mid+gap,l+gap,bot-gap}
    };
    for(int i=0;i<(alphabet?14:7);++i)
      segment(lines[i][0],lines[i][1],lines[i][2],lines[i][3],t,
              mask&(1U<<i)?ink():ghost(alphabet || h <= 32));
  }

  int width(const char *text,int w,int gap) {
    int total=0;
    for(const char *c=text;*c;++c) total+=(*c=='.'||*c==':'?std::max(4,w/3):w)+gap;
    return std::max(0,total-gap);
  }

  void text(const char *value,int x,int y,int w,int h,int gap,bool alphabet=false) {
    lv_area_t bounds={static_cast<lv_coord_t>(ox+x),static_cast<lv_coord_t>(oy+y),
                     static_cast<lv_coord_t>(ox+x+width(value,w,gap)),static_cast<lv_coord_t>(oy+y+h)};
    lv_area_t clip;
    if(!_lv_area_intersect(&clip,ctx->clip_area,&bounds)) return;
    for(const char *c=value;*c;++c) {
      int cw=w;
      if(*c=='.'||*c==':') {
        cw=std::max(4,w/3);
        int s=std::max(3,h/12);
        if(*c==':') {
          rect(x+(cw-s)/2,y+h/3-s/2,s,s,ink());
          rect(x+(cw-s)/2,y+2*h/3-s/2,s,s,ink());
        } else rect(x+(cw-s)/2,y+h-s,s,s,ink());
      } else digit(*c,x,y,w,h,alphabet);
      x+=cw+gap;
    }
  }
};

void draw(lv_event_t *event) {
  lv_area_t area;
  lv_obj_get_coords(face,&area);
  Painter p{lv_event_get_draw_ctx(event),area.x1,area.y1};
  static const char *cz[] = {"NEDELE","PONDELI","UTERY","STREDA","CTVRTEK","PATEK","SOBOTA"};
  static const char *en[] = {"SUNDAY","MONDAY","TUESDAY","WEDNESDAY","THURSDAY","FRIDAY","SATURDAY"};
  const char *day=timeAvailable?(english?en:cz)[clockTime.tm_wday]:"-------";
  p.text(day,(480-p.width(day,18,6))/2,57,18,30,6,true);
  char date[16]="--.--.----", time[8]="--:--", seconds[4]="--";
  if(timeAvailable) {
    snprintf(date,sizeof(date),"%02d.%02d.%04d",clockTime.tm_mday,clockTime.tm_mon+1,clockTime.tm_year+1900);
    snprintf(time,sizeof(time),"%02d:%02d",clockTime.tm_hour,clockTime.tm_min);
    snprintf(seconds,sizeof(seconds),"%02d",clockTime.tm_sec);
  }
  p.text(date,(480-p.width(date,16,5))/2,109,16,29,5);
  p.text(time,48,154,60,112,12);
  p.text(seconds,370,216,27,50,7);
  p.rect(38,278,404,1,ink());
  p.rect(240,291,1,69,ink());
  for(int i=0;i<2;++i) {
    const int center=i?339:141;
    p.label(names[i],center-88,289,176,&clock_czech_16,ink());
    int w=24,h=46,gap=5;
    lv_point_t unitSize;
    lv_txt_get_size(&unitSize,units[i],&clock_unit_24,0,0,200,LV_TEXT_FLAG_EXPAND);
    const int unitWidth=std::min(112,static_cast<int>(unitSize.x));
    while(w>10 && p.width(numbers[i],w,gap)+std::max(7,w/2)+4+unitWidth+6>182) { --w; h-=2; }
    while(gap>1 && p.width(numbers[i],w,gap)+std::max(7,w/2)+4+unitWidth+6>182) --gap;
    const int numberWidth=p.width(numbers[i],w,gap);
    const int signWidth = std::max(7,w/2);
    const int left=center-(signWidth+4+numberWidth+unitWidth+(unitWidth?6:0))/2;
    const int numberLeft=left+signWidth+4;
    const float thickness=std::max(2.0f,h*.086f);
    const float signY=std::round(315+46-h+h/2.0f);
    if (negativeValues[i]) p.segment(left+thickness/2,signY,left+signWidth-thickness/2,signY,thickness,ink());
    p.text(numbers[i],numberLeft,315+46-h,w,h,gap);
    if(unitWidth) p.label(units[i],numberLeft+numberWidth+6,338,unitWidth,&clock_unit_24,ink());
  }
  if (progressEnabled) {
    const int filled = std::isfinite(progress) ? static_cast<int>(std::floor(progress * progressSegments / 100.0f)) : 0;
    const int gap = std::max(1, std::min(5, 50 / static_cast<int>(progressSegments)));
    // Whole-pixel widths keep every segment and gap identical. Center the remainder.
    const int segmentWidth = (265 - gap * (progressSegments - 1)) / progressSegments;
    const int totalWidth = segmentWidth * progressSegments + gap * (progressSegments - 1);
    const int left = 108 + (265 - totalWidth) / 2;
    for (int i = 0; i < progressSegments; ++i) {
      p.rect(left + i * (segmentWidth + gap), 386, segmentWidth, 22, i < filled ? ink() : ghost());
    }
    p.label(progressText,97,416,286,&clock_czech_16,ink());
  }
  p.label(LV_SYMBOL_WIFI,192,445,24,&lv_font_montserrat_16,wifi?ink():ghost());
  p.label(LV_SYMBOL_HOME,264,445,24,&lv_font_montserrat_16,ha?ink():ghost());
  if(web) p.label(LV_SYMBOL_SETTINGS,228,445,24,&lv_font_montserrat_16,ink());
}
}

bool retroLcdEnabled() { return face!=nullptr; }

void retroLcdSetDigitPlaces(uint8_t a, uint8_t b) {
  digitPlaces[0]=std::max(1, std::min(4,static_cast<int>(a)));
  digitPlaces[1]=std::max(1, std::min(4,static_cast<int>(b)));
}

void retroLcdSetGhostOpacity(uint8_t percent) {
  percent = std::min<uint8_t>(percent, 50);
  if (ghostOpacity == percent) return;
  ghostOpacity = percent;
  if (face) lv_obj_invalidate(face);
}

void retroLcdSetColors(uint32_t bg, uint32_t fg) {
  bg &= 0xFFFFFF; fg &= 0xFFFFFF;
  if (backgroundColor == bg && foregroundColor == fg) return;
  backgroundColor = bg; foregroundColor = fg;
  if (face) { lv_obj_set_style_bg_color(face, background(), 0); lv_obj_invalidate(face); }
}

void retroLcdRaise() {
  if(!face) return;
  lv_obj_set_pos(lv_obj_get_parent(face),0,0);
  lv_obj_move_foreground(face);
}

void retroLcdEnable(lv_obj_t *parent,bool enabled) {
  if(enabled==retroLcdEnabled()) return;
  if(!enabled) { lv_obj_del(face); face=nullptr; return; }
  face=lv_obj_create(parent);
  lv_obj_remove_style_all(face);
  lv_obj_set_size(face,480,480);
  lv_obj_set_pos(face,0,0);
  lv_obj_set_style_radius(face,LV_RADIUS_CIRCLE,0);
  lv_obj_set_style_bg_color(face,background(),0);
  lv_obj_set_style_bg_opa(face,LV_OPA_COVER,0);
  lv_obj_clear_flag(face,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(face,draw,LV_EVENT_DRAW_MAIN,nullptr);
}

void retroLcdSetProgress(const ClockMetricConfig *config, float value, float minimum, float maximum, uint8_t segments) {
  segments = std::max<uint8_t>(5, std::min<uint8_t>(50, segments));
  const bool enabled = config != nullptr;
  const float next = enabled ? retroLcdProgressPercent(value, minimum, maximum) : NAN;
  char label[160] = {};
  if (enabled) {
    if (config->suffix[0]) snprintf(label, sizeof(label), "%s (%s)%s", config->name, config->suffix, std::isfinite(value) ? "" : "  --");
    else snprintf(label, sizeof(label), "%s%s", config->name, std::isfinite(value) ? "" : "  --");
  }
  if (segments != progressSegments || enabled != progressEnabled || strcmp(label, progressText) ||
      std::isfinite(next) != std::isfinite(progress) || (std::isfinite(next) && next != progress)) {
    progressEnabled = enabled;
    progressSegments = segments;
    progress = next;
    strlcpy(progressText, label, sizeof(progressText));
    invalidate(97,380,286,54);
  }
}

void retroLcdSetTime(const tm &value) {
  if(!timeAvailable || value.tm_yday!=clockTime.tm_yday || value.tm_year!=clockTime.tm_year)
    invalidate(65,50,350,92);
  if(!timeAvailable || value.tm_hour!=clockTime.tm_hour || value.tm_min!=clockTime.tm_min)
    invalidate(43,149,320,122);
  if(!timeAvailable || value.tm_sec!=clockTime.tm_sec) invalidate(365,211,73,59);
  clockTime=value;
  timeAvailable=true;
}

void retroLcdUpdate(const ClockValues &values,const ClockMetricConfig &a,
                    const ClockMetricConfig &b,bool useEnglish,bool useNight,
                    bool connected,bool webActive) {
  if(english!=useEnglish || night!=useNight) {
    english=useEnglish; night=useNight;
    if(face) { lv_obj_set_style_bg_color(face,background(),0); lv_obj_invalidate(face); }
  }
  if(wifi!=connected || web!=webActive || ha!=values.homeAssistantOnline) {
    wifi=connected; web=webActive; ha=values.homeAssistantOnline;
    invalidate(185,440,110,24);
  }
  const ClockMetricConfig *configs[]={&a,&b};
  const float numbersIn[]={values.metricAValue,values.metricBValue};
  bool changed=false;
  for(int i=0;i<2;++i) {
    char formatted[32];
    bool negative = false;
    retroLcdFormatValue(formatted,sizeof(formatted),numbersIn[i],digitPlaces[i],configs[i]->decimals,&negative);
    if(negativeValues[i]!=negative || strcmp(numbers[i],formatted) || strcmp(names[i],configs[i]->name) || strcmp(units[i],configs[i]->suffix)) {
      negativeValues[i]=negative;
      strlcpy(numbers[i],formatted,sizeof(numbers[i]));
      strlcpy(names[i],configs[i]->name,sizeof(names[i]));
      strlcpy(units[i],configs[i]->suffix,sizeof(units[i]));
      changed=true;
    }
  }
  if (changed) invalidate(48,285,386,151);
}
