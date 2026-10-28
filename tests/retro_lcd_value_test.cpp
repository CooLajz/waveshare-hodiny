#include "RetroLcdFormat.h"
#include <cassert>
#include <cstring>
void check(float value,unsigned places,unsigned decimals,const char *expected,bool extra=false) {
  char out[32];bool external=true;
  retroLcdFormatValue(out,sizeof(out),value,places,decimals,&external);
  assert(std::strcmp(out,expected)==0);assert(external==extra);
}
int main() {
  check(-.6f,2,1,"-0.6");check(-1,2,0,"-1");check(-1,2,1,"-1.0");
  check(-9.9f,2,1,"-9.9");check(-10,2,1,"10.0",true);check(-10,2,0,"10",true);
  check(-11,2,0,"11",true);check(-99,2,0,"99",true);
  check(-9.96f,2,1,"10.0",true);check(-99.96f,2,1,"--.-");
  check(-.04f,2,1," 0.0");check(-0.0f,2,1," 0.0");check(.6f,2,1," 0.6");
  check(-1,1,0,"1",true);check(-1,3,0," -1");check(-10,3,0,"-10");
  check(-1,4,2,"  -1.00");check(-100,4,0,"-100");check(-1000,4,0,"1000",true);
  check(NAN,2,1,"--.-");check(INFINITY,2,0,"--");
  char shortOut[3];bool external=true;retroLcdFormatValue(shortOut,sizeof(shortOut),-10,2,1,&external);
  assert(shortOut[0]=='\0' && !external);
}
