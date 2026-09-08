#pragma once

// Included inside ConfigurationWeb.cpp's namespace. Crypto runs on a worker;
// NVS, validation, callbacks and WebServer stay on the main task.
struct BackupJob {
  std::atomic<bool> done{false};
  bool importing = false;
  bool cryptoOk = false;
  bool finished = false;
  bool success = false;
  unsigned long touchedAt = 0;
  char id[33] = {};
  char password[BACKUP_PASSWORD_CAPACITY] = {};
  char file[BACKUP_FILE_CAPACITY] = {};
  size_t fileLength = 0;
  uint8_t plain[SETTINGS_IMAGE_CAPACITY] = {};
  size_t plainLength = 0;
  BackupMetadata metadata;
  String error;
};
BackupJob *backupJob = nullptr;
unsigned long backupRestartAt = 0;
String backupBootId;

void backupWorker(void *argument) {
  auto *job = static_cast<BackupJob *>(argument);
  job->cryptoOk = job->importing
      ? backupDecrypt(job->file, job->fileLength, job->password, job->metadata,
                      job->plain, sizeof(job->plain), job->plainLength)
      : backupEncrypt(job->plain, job->plainLength, job->password, job->metadata,
                      job->file, sizeof(job->file));
  mbedtls_platform_zeroize(job->password, sizeof(job->password));
  if (!job->importing) mbedtls_platform_zeroize(job->plain, sizeof(job->plain));
  job->done.store(true, std::memory_order_release);
  vTaskDeleteWithCaps(nullptr);
}

void clearBackupJob() {
  if (!backupJob || !backupJob->done.load(std::memory_order_acquire)) return;
  backupJob->~BackupJob();
  mbedtls_platform_zeroize(backupJob, sizeof(BackupJob));
  free(backupJob);
  backupJob = nullptr;
}

bool prepareBackupSnapshot(BackupJob &job) {
  if (!settingsTransactionBegin()) return false;
  ClockConfig &config = configBuffer;
  ClockAppearanceConfig appearance;
  bool ok = clockConfigLoad(config) && clockConfigValidate(config) &&
      clockAppearanceLoad(appearance, config.leftWeatherIconColor, config.dateFormat, config.dateColor) &&
      clockAppearanceSave(appearance) && clockConfigSave(config) && persistWebMode(selectedWebMode) &&
      settingsExport(job.plain, sizeof(job.plain), job.plainLength);
  // Materialize defaults for a portable snapshot; export never changes NVS.
  settingsTransactionAbort();
  clockConfigCopy(job.metadata.firmware, sizeof(job.metadata.firmware), FIRMWARE_VERSION);
  job.metadata.configSchema = CLOCK_CONFIG_SCHEMA_VERSION;
  const time_t now = time(nullptr);
  job.metadata.createdAt = now > 1577836800 ? static_cast<uint64_t>(now) : 0;
  return ok;
}

bool validateAndRestoreBackup(BackupJob &job) {
  if (!clockConfigSchemaSupported(job.metadata.configSchema)) {
    job.error = F("Schéma zálohy není podporované. Použij kompatibilní firmware."); return false;
  }
  if (!settingsImport(job.plain, job.plainLength)) {
    job.error = F("Struktura zálohy není platná nebo obsahuje nepodporované položky."); return false;
  }
  SettingsPreferences configPrefs, authPrefs, controlPrefs, modePrefs, lookPrefs, receipt;
  auto *record = reinterpret_cast<uint8_t *>(job.file);
  constexpr size_t recordCapacity = 4096;
  ClockConfig &config = configBuffer;
  ClockAppearanceConfig appearance;
  WebPasswordRecord password;
  configPrefs.begin("clock-config", true);
  const size_t size = configPrefs.getBytes("config", record, recordCapacity);
  uint32_t schema = 0;
  if (size >= 8) memcpy(&schema, record + 4, 4);
  bool valid = size >= 8 && schema == job.metadata.configSchema &&
      clockConfigDecodeRecord(record, size, config) && clockConfigValidate(config);
  mbedtls_platform_zeroize(record, recordCapacity);
  authPrefs.begin("web-auth", true);
  const size_t passwordSize = authPrefs.getBytesLength("credential");
  valid = valid && (passwordSize == 0 || (passwordSize == sizeof(password) &&
      authPrefs.getBytes("credential", &password, sizeof(password)) == sizeof(password) &&
      password.magic == WEB_PASSWORD_MAGIC && password.checksum == webPasswordChecksum(password)));
  controlPrefs.begin("control-api", true);
  valid = valid && validControlSecret(controlPrefs.getString("secret"));
  modePrefs.begin("web-mode", true);
  valid = valid && modePrefs.getBytesLength("mode") == 1 && modePrefs.getUChar("mode", 255) <= 2;
  lookPrefs.begin("clock-look", true);
  valid = valid && lookPrefs.getFloat("retroBarMin", 0) < lookPrefs.getFloat("retroBarMax", 100);
  for (const ClockTmepSlotConfig &slot : config.tmepSlots)
    if (slot.enabled && (!validTmepSensorId(slot.sensorId) || !tmepFieldSupported(slot.field) ||
                        !validTmepUnit(slot.unit))) valid = false;
  valid = valid && clockAppearanceLoad(appearance, config.leftWeatherIconColor, config.dateFormat, config.dateColor);
  if (!valid) {
    settingsTransactionAbort(); job.error = F("Záloha obsahuje neplatné nastavení. Nic nebylo změněno."); return false;
  }
  // Store the migrated record, all defaults and receipt in the SAME commit.
  const bool staged = clockConfigSave(config) && clockAppearanceSave(appearance) &&
      receipt.begin("save-state") && receipt.putString("receipt", job.id) == strlen(job.id);
  if (!staged) settingsTransactionAbort();
  if (!staged || !settingsTransactionCommit()) {
    job.error = F("Obnovu se nepodařilo uložit. Původní nastavení zůstalo zachované."); return false;
  }
  lastSaveConfirmationId = job.id;
  // Adopt the new config immediately: a queued timezone/device-settings save
  // must not write the old runtime snapshot over the restore before reboot.
  if (committedSettingsCallback) committedSettingsCallback();
  // Restart loads the committed configuration and auth together. Until then,
  // keep the current session alive so a lost status response can be retried.
  backupRestartAt = millis() + 1000;
  return true;
}

void finishBackupJob() {
  if (!backupJob || backupJob->finished || !backupJob->done.load(std::memory_order_acquire)) return;
  backupJob->success = backupJob->cryptoOk;
  if (!backupJob->cryptoOk) {
    backupJob->error = backupJob->importing
        ? F("Nesprávné heslo nebo poškozená záloha.") : F("Šifrování zálohy se nepodařilo.");
  } else if (backupJob->importing) {
    backupJob->success = validateAndRestoreBackup(*backupJob);
  }
  mbedtls_platform_zeroize(backupJob->plain, sizeof(backupJob->plain));
  if (backupJob->importing) mbedtls_platform_zeroize(backupJob->file, sizeof(backupJob->file));
  backupJob->finished = true;
  backupJob->touchedAt = millis();
}

bool backupBusy() {
  return backupRestartAt || (backupJob && (!backupJob->done.load(std::memory_order_acquire) || !backupJob->finished));
}

void handleBackupStart(bool importing) {
  if (!requireConfigurationAccess()) return;
  const String id = server.arg("saveConfirmationId");
  if (!validSaveConfirmationId(id) || id.length() != 32) {
    sendError(400, F("Identifikátor uložení není platný.")); return;
  }
  if (backupJob && id == backupJob->id) {
    sendJson(202, F("{\"ok\":true}")); return; // Safe retry, never a second import.
  }
  if (backupBusy()) { sendError(409, F("Zálohování nebo obnova právě probíhá.")); return; }
  String password = server.arg("password");
  if (password.length() != strlen(password.c_str()) || !backupPasswordValid(password.c_str())) {
    sendError(400, F("Heslo zálohy musí mít 8 až 128 znaků a nejvýše 256 bajtů.")); return;
  }
  clearBackupJob();
  void *memory = heap_caps_malloc(sizeof(BackupJob), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!memory) { sendError(503, F("Pro zálohu není dostatek paměti.")); return; }
  backupJob = new (memory) BackupJob;
  backupJob->importing = importing;
  backupJob->touchedAt = millis();
  clockConfigCopy(backupJob->id, sizeof(backupJob->id), id);
  clockConfigCopy(backupJob->password, sizeof(backupJob->password), password);
  if (password.length()) mbedtls_platform_zeroize(&password[0], password.length());
  bool ready = true;
  if (importing) {
    String file = server.arg("backup");
    ready = file.length() > 0 && file.length() < sizeof(backupJob->file);
    if (ready) { memcpy(backupJob->file, file.c_str(), file.length() + 1); backupJob->fileLength = file.length(); }
  } else ready = prepareBackupSnapshot(*backupJob);
  if (!ready || xTaskCreatePinnedToCoreWithCaps(backupWorker, "backup-crypto", 8192, backupJob, 1,
                                               nullptr, 0, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) != pdPASS) {
    backupJob->done.store(true); clearBackupJob();
    sendError(503, F("Zálohu se nepodařilo připravit. Nic nebylo změněno.")); return;
  }
  sendJson(202, F("{\"ok\":true}"));
}

void handleBackupStatus() {
  const String id = server.arg("id");
  if (!validSaveConfirmationId(id)) { sendError(400, F("Identifikátor uložení není platný.")); return; }
  const bool current = backupJob && id == backupJob->id;
  // A 128-bit operation ID grants only a receipt, never config or credentials.
  // This remains readable after restored auth/web mode or a restart changes access.
  if (id == lastSaveConfirmationId && id.length() == 32 && (!current || backupJob->success)) {
    String result = F("{\"ok\":true,\"state\":\"committed\",\"id\":\"");
    result += id; result += F("\",\"bootId\":\""); result += backupBootId;
    result += F("\",\"restarting\":"); result += backupRestartAt ? F("true") : F("false");
    result += F(",\"webMode\":\""); result += selectedWebMode == CONFIGURATION_WEB_DISABLED ? "disabled" : "enabled";
    result += F("\"}"); sendJson(200, result); return;
  }
  if (!current) { sendError(404, F("Operace nebyla nalezena. Výsledek obnovy zatím není potvrzen.")); return; }
  backupJob->touchedAt = millis();
  if (!backupJob->finished) { sendJson(200, F("{\"ok\":true,\"state\":\"processing\"}")); return; }
  if (!backupJob->success) { sendError(400, backupJob->error); return; }
  // Export result includes only encrypted bytes, but still requires normal access.
  if (!requireConfigurationAccess()) return;
  String result = F("{\"ok\":true,\"state\":\"ready\",\"file\":\"");
  result += jsonEscape(backupJob->file); result += F("\"}"); sendJson(200, result);
}
