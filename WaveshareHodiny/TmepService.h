#pragma once

#include <Arduino.h>

#include "NetworkDiagnostics.h"
#include "TmepParser.h"

void tmepServiceBegin();
bool tmepFetchCatalog(const char *exportId, const char *exportKey,
                      TmepCatalog &catalog,
                      NetworkDiagnosticKind diagnosticKind, int &httpStatus,
                      String &error);
bool tmepGetCachedCatalog(const char *exportId, const char *exportKey,
                          TmepCatalog &catalog);
void tmepClearCachedCatalog();
