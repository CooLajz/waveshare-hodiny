#include <cassert>
#include <cstdio>
#include <vector>
#include "backup_test_support/Preferences.h"
#include "../WaveshareHodiny/ConfigurationBackup.h"
#include "../WaveshareHodiny/ClockConfig.h"

// Linked only by this host harness, never present in firmware.
void settingsStoreTestReset();
bool clockTimezoneSupported(const char *name) {
  return !strcmp(name, "Europe/Prague") || !strcmp(name, "America/New_York");
}
uint32_t checksum(const void *data,size_t length){uint32_t h=2166136261u;const auto*p=static_cast<const uint8_t*>(data);while(length--){h^=*p++;h*=16777619u;}return h;}

void testMigration() {
  ClockConfig current; clockConfigApplyDefaults(current);
  strcpy(current.homeAssistantToken,"synthetic-test-token");
  const struct { uint32_t schema; size_t prefix; } cases[] = {
      {20,2096},{24,2108},{25,2108},{26,2108},{27,2452},{28,2688},{29,2752},{30,2756}};
  for(auto test:cases){
    std::vector<uint8_t> record(test.prefix+12);
    uint32_t magic=0x57484346;memcpy(record.data(),&magic,4);memcpy(record.data()+4,&test.schema,4);
    memcpy(record.data()+8,&current,test.prefix);memcpy(record.data()+8,&test.schema,4);
    if(test.schema==25)record[8+2106]=1;
    uint32_t hash=checksum(record.data()+8,test.prefix);memcpy(record.data()+8+test.prefix,&hash,4);
    const auto before=fakeNvs::values;
    ClockConfig decoded;assert(clockConfigDecodeRecord(record.data(),record.size(),decoded));
    assert(decoded.schemaVersion==30);assert(decoded.forecastDisplaySeconds==20);assert(!strcmp(decoded.homeAssistantToken,"synthetic-test-token"));
    if(test.schema==25)assert(decoded.language==CLOCK_LANGUAGE_ENGLISH);
    assert(fakeNvs::values==before); // Decoder must be pure, including migrations.
    record[12]^=1;assert(!clockConfigDecodeRecord(record.data(),record.size(),decoded));
  }
  uint8_t invalid[16]={};ClockConfig decoded;assert(!clockConfigDecodeRecord(invalid,sizeof(invalid),decoded));
  current.metricAColorScale.count=2;current.metricAColorScale.points[0]={1.0001f,0xffffff};current.metricAColorScale.points[1]={1.0002f,0};
  assert(clockConfigValidate(current));current.metricAColorScale.points[1].value=1.0001f;assert(!clockConfigValidate(current));
  puts("PASS: all supported migrations, checksum rejection, no migration writes, precise scales");
}

void testStorage(){
  fakeNvs::values.clear();settingsStoreTestReset();
  fakeNvs::values["nvs/wifi/ssid"]={'u','n','c','h','a','n','g','e','d',0};
  Preferences legacy;legacy.begin("web-mode",false,"clockcfg");legacy.putUChar("mode",1);
  assert(settingsStoreBegin());SettingsPreferences mode;assert(mode.begin("web-mode"));assert(mode.getUChar("mode")==1);
  assert(mode.putUChar("mode",0)==1);
  const auto baseline=fakeNvs::values;
  for(auto fault:{fakeNvs::SlotWrite,fakeNvs::SlotRead,fakeNvs::SelectorWrite,fakeNvs::PowerAfterSlot,fakeNvs::PowerAfterSelector}){
    settingsStoreTestReset();fakeNvs::values=baseline;fakeNvs::fault=fakeNvs::None;assert(settingsStoreBegin());
    assert(settingsTransactionBegin());SettingsPreferences p;p.begin("web-mode");p.putUChar("mode",2);
    SettingsPreferences receipt;receipt.begin("save-state");receipt.putString("receipt","0123456789abcdef0123456789abcdef");
    fakeNvs::fault=fault;fakeNvs::slotWritten=false;
    try{assert(!settingsTransactionCommit());}catch(const std::runtime_error&){}
    fakeNvs::fault=fakeNvs::None;settingsStoreTestReset();assert(settingsStoreBegin());p.begin("web-mode");receipt.begin("save-state");
    assert(p.getUChar("mode")== (fault==fakeNvs::PowerAfterSelector?2:0));
    assert(receipt.getString("receipt").empty()==(fault!=fakeNvs::PowerAfterSelector));
    assert(fakeNvs::values.at("nvs/wifi/ssid")==baseline.at("nvs/wifi/ssid"));
  }
  fakeNvs::fault=fakeNvs::None;settingsStoreTestReset();fakeNvs::values=baseline;assert(settingsStoreBegin());
  uint8_t bad[]={255,1,0,1};assert(!settingsImport(bad,sizeof(bad)));
  // Replacement has no target inheritance: absent credential remains absent.
  uint8_t replacement[]={26,1,0,1};assert(settingsImport(replacement,sizeof(replacement)));assert(settingsTransactionCommit());
  SettingsPreferences receipt;receipt.begin("save-state");assert(receipt.getString("receipt").empty());
  puts("PASS: atomic multi-key writes, power loss before/after selector, readback failure, receipt reboot, Wi-Fi untouched");
}

void testCrypto(){
  assert(!backupPasswordValid("1234567"));assert(backupPasswordValid("12345678"));
  assert(backupPasswordValid("ěščřžýáí"));assert(!backupPasswordValid("1234567\n"));
  assert(!backupPasswordValid("1234567\xc0\xaf"));
  BackupMetadata meta;strcpy(meta.firmware,"1.8.1");meta.configSchema=29;meta.createdAt=1788888888;
  uint8_t plain[]={0,1,2,3,255,0,55};char file[BACKUP_FILE_CAPACITY],second[BACKUP_FILE_CAPACITY];
  assert(backupEncrypt(plain,sizeof(plain),"synthetic-password",meta,file,sizeof(file)));
  assert(backupEncrypt(plain,sizeof(plain),"synthetic-password",meta,second,sizeof(second)));
  assert(strcmp(file,second));assert(strstr(file,"firmwareVersion\":\"1.8.1"));
  assert(strstr(file,"\"iterations\":10000"));
  uint8_t decoded[SETTINGS_IMAGE_CAPACITY];size_t length=0;BackupMetadata read;
  assert(backupDecrypt(file,strlen(file),"synthetic-password",read,decoded,sizeof(decoded),length));
  assert(length==sizeof(plain)&&!memcmp(plain,decoded,length));assert(read.configSchema==29);
  assert(!backupDecrypt(file,strlen(file),"wrong-password",read,decoded,sizeof(decoded),length));
  char *schema=strstr(file,"configSchema\":29");assert(schema);schema[15]='8';
  assert(!backupDecrypt(file,strlen(file),"synthetic-password",read,decoded,sizeof(decoded),length));schema[15]='9';
  strcpy(file,second);file[strlen(file)-5]^=1;
  assert(!backupDecrypt(file,strlen(file),"synthetic-password",read,decoded,sizeof(decoded),length));
  assert(!backupDecrypt(second,strlen(second)-4,"synthetic-password",read,decoded,sizeof(decoded),length));
  // Changing the work factor must not invalidate older, authentic test files.
  meta.kdfIterations=600000;
  assert(backupEncrypt(plain,sizeof(plain),"synthetic-password",meta,file,sizeof(file)));
  assert(backupDecrypt(file,strlen(file),"synthetic-password",read,decoded,sizeof(decoded),length));
  assert(read.kdfIterations==600000 && length==sizeof(plain));
  char *iterations=strstr(file,"iterations\":600000");assert(iterations);iterations[12]='9';
  assert(!backupDecrypt(file,strlen(file),"synthetic-password",read,decoded,sizeof(decoded),length));
  // Independent Python/Node interoperability check reads synthetic fixtures only.
  FILE *out=fopen("/tmp/waveshare-backup-crypto-fixture.whbackup","wb");assert(out);fwrite(second,1,strlen(second),out);fclose(out);
  puts("PASS: AES-GCM round trip, random salt/nonce, password limits, wrong password, authenticated header, corruption and truncation");
}

void testCorruptStoreRecovery() {
  fakeNvs::values.clear(); settingsStoreTestReset(); assert(settingsStoreBegin());
  SettingsPreferences mode;mode.begin("web-mode");assert(mode.putUChar("mode",1)==1);
  uint8_t image[SETTINGS_IMAGE_CAPACITY];size_t length;assert(settingsExport(image,sizeof(image),length));
  const unsigned active=fakeNvs::values.at("clockcfg/settings-v1/active")[0];
  fakeNvs::values.at("clockcfg/settings-v1/slot"+std::to_string(active))[36]^=1;
  settingsStoreTestReset();assert(!settingsStoreBegin());
  assert(!settingsTransactionBegin()); // Ordinary saves cannot overwrite corrupt storage.
  const auto corrupt=fakeNvs::values;
  assert(settingsImport(image,length));settingsTransactionAbort();
  assert(fakeNvs::values==corrupt);assert(!settingsStoreBegin());
  assert(settingsImport(image,length));mode.begin("web-mode",true);assert(mode.getUChar("mode")==1);
  assert(settingsTransactionCommit());settingsStoreTestReset();assert(settingsStoreBegin());
  mode.begin("web-mode",true);assert(mode.getUChar("mode")==1);
  puts("PASS: corrupt selected slot blocks ordinary saves but permits validated full restore; abort leaves disk untouched");
}

void testCompleteSnapshot() {
  fakeNvs::values.clear(); settingsStoreTestReset(); assert(settingsStoreBegin());
  ClockConfig source; clockConfigApplyDefaults(source);
  source.dataSource=CLOCK_DATA_SOURCE_HOME_ASSISTANT;
  strcpy(source.homeAssistantUrl,"http://synthetic.invalid:8123");
  strcpy(source.homeAssistantToken,"synthetic-complete-token");
  strcpy(source.tmepExportKey,"synthetic-tmep-key"); strcpy(source.tmepExportId,"123");
  source.clockDisplaySeconds=0;source.radarDisplaySeconds=0;source.forecastDisplaySeconds=37;
  source.metricAColorScale.count=2;
  source.metricAColorScale.points[0]={1.0001f,0xffffff};
  source.metricAColorScale.points[1]={1.0002f,0};
  ClockAppearanceConfig appearance;appearance.style=CLOCK_STYLE_RETRO_LCD;
  appearance.retroFixedWeekday=true;
  appearance.retroDateFormat=10;
  appearance.use12HourFormat=true;appearance.retroProgressMin=0.0001234567f;
  uint8_t credential[56]={}; uint32_t magic=0x57485058;memcpy(credential,&magic,4);
  for(size_t i=4;i<52;++i)credential[i]=i;
  uint32_t hash=checksum(credential,52);memcpy(credential+52,&hash,4);
  assert(settingsTransactionBegin());assert(clockConfigSave(source));assert(clockAppearanceSave(appearance));
  SettingsPreferences prefs;prefs.begin("web-auth");assert(prefs.putBytes("credential",credential,56)==56);
  prefs.begin("control-api");assert(prefs.putString("secret","0123456789abcdef0123456789abcdef")==32);
  prefs.begin("web-mode");assert(prefs.putUChar("mode",2)==1);assert(settingsTransactionCommit());
  uint8_t snapshot[SETTINGS_IMAGE_CAPACITY];size_t snapshotLength=0;assert(settingsExport(snapshot,sizeof(snapshot),snapshotLength));
  BackupMetadata meta;strcpy(meta.firmware,"1.8.1");meta.configSchema=CLOCK_CONFIG_SCHEMA_VERSION;
  char file[BACKUP_FILE_CAPACITY];assert(backupEncrypt(snapshot,snapshotLength,"synthetic-password",meta,file,sizeof(file)));
  assert(!strstr(file,"synthetic-complete-token")&&!strstr(file,"synthetic-tmep-key"));
  // Destination starts with entirely different settings and no credentials.
  ClockConfig target;clockConfigApplyDefaults(target);assert(clockConfigSave(target));
  prefs.begin("web-auth");assert(prefs.remove("credential"));
  uint8_t plain[SETTINGS_IMAGE_CAPACITY];size_t size;BackupMetadata header;
  assert(backupDecrypt(file,strlen(file),"synthetic-password",header,plain,sizeof(plain),size));
  assert(settingsImport(plain,size));ClockConfig restored;assert(clockConfigLoad(restored));assert(restored.forecastDisplaySeconds==37);assert(restored.clockDisplaySeconds==0&&restored.radarDisplaySeconds==0);
  assert(!strcmp(restored.homeAssistantToken,source.homeAssistantToken));
  assert(!strcmp(restored.tmepExportKey,source.tmepExportKey));
  assert(restored.metricAColorScale.points[0].value==source.metricAColorScale.points[0].value);
  ClockAppearanceConfig restoredAppearance;assert(clockAppearanceLoad(restoredAppearance));
  assert(restoredAppearance.use12HourFormat&&restoredAppearance.style==CLOCK_STYLE_RETRO_LCD);
  assert(restoredAppearance.retroFixedWeekday);
  assert(restoredAppearance.retroDateFormat==10);
  assert(restoredAppearance.retroProgressMin==appearance.retroProgressMin);
  prefs.begin("web-auth",true);uint8_t readCredential[56];assert(prefs.getBytes("credential",readCredential,56)==56);
  assert(!memcmp(credential,readCredential,56));assert(settingsTransactionCommit());
  settingsStoreTestReset();assert(settingsStoreBegin());assert(clockConfigLoad(restored));
  assert(!strcmp(restored.homeAssistantToken,source.homeAssistantToken));
  prefs.begin("web-mode",true);assert(prefs.getUChar("mode")==2);
  assert(clockAppearanceLoad(restoredAppearance));assert(restoredAppearance.retroFixedWeekday);
  assert(restoredAppearance.retroDateFormat==10);
  // A normal color-only save must retain the weekday option and survive reboot.
  restoredAppearance.retroForegroundColor=0x123456;
  assert(clockAppearanceSave(restoredAppearance));settingsStoreTestReset();assert(settingsStoreBegin());
  assert(clockAppearanceLoad(restoredAppearance));
  assert(restoredAppearance.retroForegroundColor==0x123456 && restoredAppearance.retroFixedWeekday);
  assert(restoredAppearance.retroDateFormat==10);
  for(uint8_t format=0;format<14;++format) {
    restoredAppearance.retroDateFormat=format;
    assert(clockAppearanceSave(restoredAppearance));settingsStoreTestReset();assert(settingsStoreBegin());
    assert(clockAppearanceLoad(restoredAppearance));assert(restoredAppearance.retroDateFormat==format);
  }
  assert(settingsTransactionBegin());prefs.begin("clock-look");
  assert(prefs.putUChar("retroDateFmt",14)==1);assert(!settingsTransactionCommit());
  settingsTransactionAbort();assert(clockAppearanceLoad(restoredAppearance));assert(restoredAppearance.retroDateFormat==13);
  // Commit the new style to disk (a read within a transaction is insufficient).
  restoredAppearance.style = CLOCK_STYLE_FORECAST;
  assert(clockAppearanceSave(restoredAppearance));
  settingsStoreTestReset();assert(settingsStoreBegin());
  assert(clockAppearanceLoad(restoredAppearance));
  assert(restoredAppearance.style == CLOCK_STYLE_DIGITAL); // legacy forecast selection migrates to a clock
  uint8_t forecastImage[SETTINGS_IMAGE_CAPACITY];size_t forecastSize=0;
  assert(settingsExport(forecastImage,sizeof(forecastImage),forecastSize));
  assert(settingsImport(forecastImage,forecastSize));assert(settingsTransactionCommit());
  assert(settingsTransactionBegin());prefs.begin("clock-look");
  assert(prefs.putUChar("style",4)==1);assert(!settingsTransactionCommit());
  // Older images do not carry the appended key and use its explicit default.
  prefs.begin("clock-look");assert(prefs.remove("retroFixedDay"));assert(prefs.remove("retroDateFmt"));
  assert(clockAppearanceLoad(restoredAppearance));assert(!restoredAppearance.retroFixedWeekday);assert(restoredAppearance.retroDateFormat==0);
  puts("PASS: encrypted full settings restore over clean target, HA/TMEP credentials, web auth, appearance, exact floats and reboot");
}
int main(){testMigration();testStorage();testCrypto();testCompleteSnapshot();testCorruptStoreRecovery();}
