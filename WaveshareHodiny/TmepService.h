#pragma once

#include <Arduino.h>

#include "NetworkDiagnostics.h"
#include "TmepParser.h"

using TmepCatalogVisitor = void (*)(const TmepCatalog &catalog, void *context);

void tmepServiceBegin();
bool tmepFetchCatalog(const char *exportId, const char *exportKey,
                      TmepCatalogVisitor visitor, void *visitorContext,
                      NetworkDiagnosticKind diagnosticKind, int &httpStatus,
                      String &error);
bool tmepVisitCachedCatalog(const char *exportId, const char *exportKey,
                            TmepCatalogVisitor visitor, void *visitorContext);
void tmepClearCachedCatalog();
