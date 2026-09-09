#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

// Rasterize canonical segments, then reflect the finished coverage. Reflections
// therefore preserve both solid pixels and antialiasing, even on tiny glyphs.
namespace retro_lcd_geometry {
struct Point { int x, y; };

template <typename Emit>
void segment(int width, int height, bool alphabet, int index, Emit emit, bool solid = false) {
  const int radius = std::max(1, static_cast<int>(std::lround(height * (alphabet ? .073f : .086f) / 2)));
  const int right = 2 * ((width - 1) / 2), bottom = 2 * ((height - 1) / 2);
  const int cx = right / 2, cy = bottom / 2;
  const int l = radius, top = radius, gap = solid ? 1 : 2;
  bool flipX = false, flipY = false;
  Point a{}, b{};
  switch (index) {
    case 0: case 3:
      a={l+gap,top}; b={right-l-gap,top}; flipY=index==3; break;
    case 1: case 2: case 4: case 5:
      a={l,top+gap}; b={l,cy-gap};
      flipX=index==1 || index==2; flipY=index==2 || index==4; break;
    case 6: case 7:
      a={l+(solid && alphabet ? 1 : gap),cy};
      b={alphabet ? cx-(solid ? 0 : gap) : right-l-gap,cy}; flipX=index==7; break;
    case 8: case 10: case 11: case 13:
      a={l+2,top+2}; b={cx-2,cy-2};
      flipX=index==10 || index==11; flipY=index==11 || index==13; break;
    case 9: case 12:
      a={cx,top+2}; b={cx,cy-2}; flipY=index==12; break;
    case 14: // Standalone minus: no inactive digit background.
      a={l,cy}; b={right-l,cy}; break;
    default: return;
  }
  const float dx=b.x-a.x, dy=b.y-a.y;
  const float length=std::sqrt(dx*dx+dy*dy);
  if(length<2*radius) return;
  const float ux=dx/length, uy=dy/length;
  const float px=-uy*radius, py=ux*radius;
  const float vertices[6][2]={
    {float(a.x),float(a.y)}, {a.x+ux*radius+px,a.y+uy*radius+py},
    {b.x-ux*radius+px,b.y-uy*radius+py}, {float(b.x),float(b.y)},
    {b.x-ux*radius-px,b.y-uy*radius-py}, {a.x+ux*radius-px,a.y+uy*radius-py}
  };
  Point points[6];
  int minX=width, maxX=0, minY=height, maxY=0;
  for(int i=0;i<6;++i) {
    // Subpixel vertices keep narrow diagonals from growing a whole pixel.
    points[i]={int(std::lround(vertices[i][0]*8)),int(std::lround(vertices[i][1]*8))};
    minX=std::min(minX,points[i].x/8); maxX=std::max(maxX,(points[i].x+7)/8);
    minY=std::min(minY,points[i].y/8); maxY=std::max(maxY,(points[i].y+7)/8);
  }
  for(int y=minY;y<=maxY;++y) {
    int runStart=minX, previous=0;
    for(int x=minX;x<=maxX+1;++x) {
      int coverage=0;
      if(x<=maxX) for(int sy=-3;sy<=3;sy+=2) for(int sx=-3;sx<=3;sx+=2) {
        bool inside=true;
        for(int i=0;i<6;++i) {
          const Point &p=points[i], &q=points[(i+1)%6];
          if((q.x-p.x)*(y*8+sy-p.y)-(q.y-p.y)*(x*8+sx-p.x)>0) {inside=false;break;}
        }
        coverage+=inside;
      }
      // Solid date/day strokes keep the same mirrored raster, without edge shades.
      if(solid) coverage = coverage >= 8 ? 16 : 0;
      if(coverage!=previous) {
        if(previous) emit(flipX ? right-(x-1) : runStart, flipY ? bottom-y : y,
                          x-runStart, static_cast<uint8_t>((previous*255+8)/16));
        runStart=x; previous=coverage;
      }
    }
  }
}
}
