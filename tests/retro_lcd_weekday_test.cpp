#include "RetroLcdFormat.h"
#include <cassert>
#include <cstring>
int main() {
  const char *expected[2][7] = {
    {"NEDELE ","PONDELI"," UTERY ","STREDA ","CTVRTEK"," PATEK ","SOBOTA "},
    {" SUNDAY  "," MONDAY  "," TUESDAY ","WEDNESDAY","THURSDAY "," FRIDAY  ","SATURDAY "}
  };
  for(int language=0;language<2;++language) for(int day=0;day<7;++day) {
    char out[10]; retroLcdFormatWeekday(out,sizeof(out),day,language);
    assert(std::strcmp(out,expected[language][day])==0);
    retroLcdFormatWeekday(out,sizeof(out),day,language,false);
    assert(out[0]!=' ' && out[std::strlen(out)-1]!=' ');
    assert(std::strstr(expected[language][day],out)!=nullptr);
  }
  char out[10]; retroLcdFormatWeekday(out,sizeof(out),-1,true);
  assert(std::strcmp(out,"---------")==0);
  retroLcdFormatWeekday(out,sizeof(out),7,false);
  assert(std::strcmp(out,"-------")==0);
  retroLcdFormatWeekday(out,2,0,true); assert(out[0]=='\0');
}
