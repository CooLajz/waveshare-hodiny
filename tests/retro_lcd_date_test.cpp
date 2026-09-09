#include "RetroLcdFormat.h"
#include <cassert>
#include <cstring>
int main() {
  const char *expected[]={"06.07.2024","06-07-2024","06/07/2024","07-06-2024","07/06/2024","2024-07-06","2024/07/06",
                         " 6. 7.2024"," 6- 7-2024"," 6/ 7/2024"," 7- 6-2024"," 7/ 6/2024","2024- 7- 6","2024/ 7/ 6"};
  for(unsigned format=0;format<14;++format) {
    char out[16];retroLcdFormatDate(out,sizeof(out),6,7,2024,format);
    assert(std::strcmp(out,expected[format])==0);
    for(int day=1;day<=31;++day)for(int month=1;month<=12;++month) {
      char padded[16],blank[16];
      retroLcdFormatDate(padded,sizeof(padded),day,month,2024,format%7);
      retroLcdFormatDate(blank,sizeof(blank),day,month,2024,format%7+7);
      assert(std::strlen(padded)==10 && std::strlen(blank)==10);
      for(int i=0;i<10;++i)assert(padded[i]==blank[i] || (padded[i]=='0' && blank[i]==' '));
    }
    retroLcdFormatDate(out,sizeof(out),29,2,2024,format);
    assert(std::strstr(out,"29"));
    retroLcdFormatDate(out,sizeof(out),31,12,2024,format);
    assert(std::strstr(out,"31") && std::strstr(out,"12"));
    retroLcdFormatDate(out,sizeof(out),1,1,2024,format,false);
    assert(std::strlen(out)==10 && !std::strchr(out,' '));
  }
  char out[16];retroLcdFormatDate(out,sizeof(out),6,7,2024,255);assert(std::strcmp(out,expected[0])==0);
  retroLcdFormatDate(out,10,6,7,2024,0);assert(out[0]=='\0');
}
