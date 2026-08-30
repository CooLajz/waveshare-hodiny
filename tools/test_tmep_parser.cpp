#include <cassert>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>

#include "TmepParser.h"

int main(int argc, char **argv) {
  if (argc == 2) {
    std::ifstream input(argv[1], std::ios::binary);
    assert(input.good());
    const std::string livePayload((std::istreambuf_iterator<char>(input)),
                                  std::istreambuf_iterator<char>());
    TmepCatalog liveCatalog;
    char liveError[160];
    assert(tmepParseExport(livePayload.c_str(), livePayload.size(),
                           liveCatalog, liveError, sizeof(liveError)));
    assert(liveCatalog.count > 0);
    return 0;
  }
  const char payload[] = R"JSON({
    "6993": {
      "teplota": 29.1,
      "vlhkost": 39,
      "tlak": 1012.8,
      "teplota_jednotka": "°C",
      "vlhkost_jednotka": "%",
      "tlak_jednotka": "hPa",
      "cas": "2026-08-30 18:22:00",
      "umisteni": "Nebovidy u Brna",
      "nadpis": "Nebovidy u Brna",
      "domena": "nebovidyubrna.tmep.cz",
      "rssi": null,
      "napeti": null,
      "ignored": {"array": [1, true, null]}
    },
    "8241": {
      "teplota": 0.7,
      "vlhkost": 2.5,
      "tlak": null,
      "teplota_jednotka": "\u00b5g/m\u00b3",
      "vlhkost_jednotka": "µg/m³",
      "tlak_jednotka": "hPa",
      "cas": "2026-08-30 18:22:01",
      "umisteni": null,
      "nadpis": null,
      "domena": "nebovidyubrnaprach.tmep.cz",
      "rssi": -89,
      "napeti": null
    }
  })JSON";

  TmepCatalog catalog;
  char error[160];
  assert(tmepParseExport(payload, strlen(payload), catalog, error,
                         sizeof(error)));
  assert(catalog.count == 2);
  assert(!catalog.truncated);

  const TmepSensor *weather = tmepFindSensor(catalog, "6993");
  assert(weather != nullptr);
  assert(strcmp(weather->domain, "nebovidyubrna.tmep.cz") == 0);
  assert(weather->temperature.available);
  assert(std::fabs(weather->temperature.value - 29.1f) < 0.001f);
  assert(weather->temperature.decimals == 1);
  assert(strcmp(weather->temperature.unit, "°C") == 0);
  assert(weather->humidity.decimals == 0);
  assert(!weather->rssi.available);

  const TmepSensor *dust = tmepFindSensor(catalog, "8241");
  assert(dust != nullptr);
  assert(dust->title[0] == '\0');
  assert(strcmp(dust->temperature.unit, "µg/m³") == 0);
  assert(strcmp(dust->humidity.unit, "µg/m³") == 0);
  assert(!dust->pressure.available);
  assert(dust->rssi.available && dust->rssi.value == -89.0f);
  assert(strcmp(dust->rssi.unit, "dBm") == 0);
  assert(tmepFindValue(*dust, "teplota") == &dust->temperature);
  assert(tmepFindValue(*dust, "unknown") == nullptr);

  const char denied[] =
      "Nemáte oprávnění na toto čidlo, nebo je špatný export_key.";
  assert(!tmepParseExport(denied, strlen(denied), catalog, error,
                          sizeof(error)));
  assert(strstr(error, "nevrátil JSON") != nullptr);

  const char empty[] = "{}";
  assert(!tmepParseExport(empty, strlen(empty), catalog, error,
                          sizeof(error)));
  assert(strstr(error, "žádná čidla") != nullptr);
  return 0;
}
