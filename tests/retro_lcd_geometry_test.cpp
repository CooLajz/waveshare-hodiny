#include "RetroLcdGeometry.h"
#include <cassert>
#include <vector>
using Raster=std::vector<int>;
Raster raster(int w,int h,bool alpha,int segment,bool solid=false) {
  Raster out(w*h);
  retro_lcd_geometry::segment(w,h,alpha,segment,[&](int x,int y,int n,uint8_t a){
    assert(x>=0 && x+n<=w && y>=0 && y<h);
    for(int i=0;i<n;++i) {assert(out[y*w+x+i]==0);out[y*w+x+i]=a;}
  },solid);
  int sum=0;for(int a:out)sum+=a;assert(sum>0);return out;
}
void mirror(const Raster &a,const Raster &b,int w,int h,bool mx,bool my){
  int right=2*((w-1)/2),bottom=2*((h-1)/2);
  for(int y=0;y<=bottom;++y)for(int x=0;x<=right;++x)
    assert(a[y*w+x]==b[(my?bottom-y:y)*w+(mx?right-x:x)]);
}
int main(){
  for(bool alpha:{false,true}) {
    int w=alpha?18:16,h=alpha?30:29;
    std::vector<Raster> r;
    for(int i=0;i<(alpha?14:7);++i) {
      r.push_back(raster(w,h,alpha,i,true));
      for(int pixel:r.back()) assert(pixel==0 || pixel==255);
    }
    mirror(r[0],r[3],w,h,false,true);mirror(r[5],r[1],w,h,true,false);
    mirror(r[5],r[4],w,h,false,true);mirror(r[6],r[6],w,h,false,true);
    if(alpha) {
      mirror(r[6],r[7],w,h,true,false);
      int cy=2*((h-1)/2)/2, lit=0;
      for(int x=0;x<w;++x)lit+=r[6][cy*w+x]==255;
      assert(lit==5); // Extend inward by one pixel only; retain a center gap.
      assert(r[6][cy*w+8]==0 && r[7][cy*w+8]==0);
    }
  }

  for(int w=10;w<=24;++w) {
    int h=2*w-2;std::vector<Raster> r;
    for(int i=0;i<7;++i)r.push_back(raster(w,h,false,i));
    mirror(r[0],r[3],w,h,false,true);mirror(r[5],r[1],w,h,true,false);
    mirror(r[5],r[4],w,h,false,true);mirror(r[6],r[6],w,h,false,true);
  }
  for(int h=18;h<=46;h+=2) {
    int w=std::max(7,(h-46)/4+12);auto r=raster(w,h,false,14);
    mirror(r,r,w,h,true,false);mirror(r,r,w,h,false,true);
  }
  for(auto size: {std::pair<int,int>{16,29},{18,30},{18,32},{27,50},{60,112},{20,40},{30,46}}) {
    int w=size.first,h=size.second;
    for(bool alpha:{false,true}){
      std::vector<Raster> r;for(int i=0;i<(alpha?14:7);++i)r.push_back(raster(w,h,alpha,i));
      mirror(r[0],r[3],w,h,false,true);
      mirror(r[5],r[1],w,h,true,false);mirror(r[5],r[4],w,h,false,true);
      mirror(r[1],r[2],w,h,false,true);mirror(r[6],r[6],w,h,false,true);
      if(alpha){mirror(r[6],r[7],w,h,true,false);mirror(r[8],r[10],w,h,true,false);
        mirror(r[8],r[13],w,h,false,true);mirror(r[10],r[11],w,h,false,true);
        mirror(r[9],r[12],w,h,false,true);
      }else mirror(r[6],r[6],w,h,true,false);
    }
  }
}
