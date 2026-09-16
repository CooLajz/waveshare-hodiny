// Included inside ConfigurationWeb.cpp's anonymous namespace.

void handleNotification() {
  String title, message, secondsText = "0", textColorText = "#FFFFFF",
         backgroundColorText = "#000000", beepText = "0";
  String contentType = server.header("Content-Type");
  const int separator = contentType.indexOf(';');
  if (separator >= 0) contentType.remove(separator);
  contentType.trim();
  if (contentType == "application/json") {
    const char *body = server.rawPostBody();
    // cJSON represents strings as C strings. Reject encoded NUL instead of
    // silently accepting a truncated title/message.
    if (strstr(body, "\\u0000")) {
      sendError(400, F("Text nesmí obsahovat nulový znak."));
      return;
    }
    cJSON *root = cJSON_ParseWithLengthOpts(
        body, server.rawPostLength() + 1, nullptr, true);
    bool valid = cJSON_IsObject(root);
    unsigned seen = 0;
    const char *keys[] = {"title", "message", "durationSeconds",
                          "textColor", "backgroundColor", "beep"};
    if (valid) {
      for (cJSON *field = root->child; field; field = field->next) {
        int index = -1;
        for (int i = 0; i < 6; ++i)
          if (field->string && strcmp(field->string, keys[i]) == 0) index = i;
        if (index < 0 || (seen & (1U << index))) { valid = false; break; }
        seen |= 1U << index;
        if (index == 2 || index == 5) {
          const double seconds = field->valuedouble;
          if (!cJSON_IsNumber(field) || !std::isfinite(seconds) || seconds < 0 ||
              seconds > (index == 2 ? NOTIFICATION_MAX_SECONDS : NOTIFICATION_MAX_BEEP_MS) || floor(seconds) != seconds) {
            valid = false; break;
          }
          if (index == 2) secondsText = String(static_cast<uint32_t>(seconds));
          else beepText = String(static_cast<uint32_t>(seconds));
        } else {
          if (!cJSON_IsString(field)) { valid = false; break; }
          if (index == 0) title = field->valuestring;
          if (index == 1) message = field->valuestring;
          if (index == 3) textColorText = field->valuestring;
          if (index == 4) backgroundColorText = field->valuestring;
        }
      }
    }
    cJSON_Delete(root);
    if (!valid || (seen & 3) != 3) {
      sendError(400, F("Neplatný JSON nebo pole notifikace."));
      return;
    }
  } else if (contentType == "application/x-www-form-urlencoded") {
    if (server.hasArg("beep")) beepText = server.arg("beep");
    title = server.arg("title");
    message = server.arg("message");
    if (server.hasArg("durationSeconds")) secondsText = server.arg("durationSeconds");
    if (server.hasArg("textColor")) textColorText = server.arg("textColor");
    if (server.hasArg("backgroundColor")) backgroundColorText = server.arg("backgroundColor");
  } else {
    sendError(415, F("Použij application/json nebo application/x-www-form-urlencoded."));
    return;
  }
  uint32_t beep = 0;
  uint32_t seconds = 0, textColor = 0xffffff, backgroundColor = 0;
  if (beepText.length() != strlen(beepText.c_str()) ||
      !notificationParseBeep(beepText.c_str(), beep) ||
      title.length() != strlen(title.c_str()) ||
      message.length() != strlen(message.c_str()) ||
      secondsText.length() != strlen(secondsText.c_str()) ||
      textColorText.length() != strlen(textColorText.c_str()) ||
      backgroundColorText.length() != strlen(backgroundColorText.c_str()) ||
      !notificationValidText(title.c_str(), NOTIFICATION_TITLE_BYTES, false) ||
      !notificationValidText(message.c_str(), NOTIFICATION_MESSAGE_BYTES, true) ||
      !notificationParseSeconds(secondsText.c_str(), seconds) ||
      !notificationParseColor(textColorText.c_str(), textColor) ||
      !notificationParseColor(backgroundColorText.c_str(), backgroundColor)) {
    sendError(400, F("Vyplň nadpis (1–96 bajtů), zprávu (1–768 bajtů), celé sekundy 0–86400, beep 0–5000 ms a barvy #RRGGBB."));
    return;
  }
  if (firmwareUpdateServiceSnapshot().busy ||
      (currentDisplayPowerStatusCallback && currentDisplayPowerStatusCallback())) {
    sendError(409, F("Displej je vypnutý nebo probíhá práce s aktualizací firmware."));
    return;
  }
  const BuzzerSnapshot buzzer = buzzerServiceSnapshot();
  if (beep && (!buzzer.ready || !buzzer.ioOk)) {
    sendError(503, F("Bzučák není nyní dostupný."));
    return;
  }
  const bool replaced = displayNotificationActive();
  if (!displayNotificationShow(title.c_str(), message.c_str(), seconds,
                               textColor, backgroundColor, beep)) {
    sendError(503, F("Notifikaci se nepodařilo zobrazit."));
    return;
  }
  String result = F("{\"ok\":true,\"active\":true,\"durationSeconds\":");
  result += seconds;
  result += F(",\"beep\":");
  result += beep;
  result += F(",\"replaced\":");
  result += replaced ? F("true}") : F("false}");
  sendJson(200, result);
}
