#include "ConfigurationBackup.h"

#include <esp_heap_caps.h>
#include <esp_system.h>
#include <mbedtls/base64.h>
#include <mbedtls/gcm.h>
#include <mbedtls/pkcs5.h>
#include <mbedtls/platform_util.h>

namespace {
constexpr size_t SALT_SIZE = 16;
constexpr size_t NONCE_SIZE = 12;
constexpr size_t TAG_SIZE = 16;
constexpr char HEADER_FORMAT[] =
    "{\"format\":\"waveshare-hodiny-encrypted\",\"version\":1,\"project\":\"waveshare-hodiny\","
    "\"firmwareVersion\":\"%s\",\"configSchema\":%lu,\"settingsSchema\":1,\"createdAt\":%llu,"
    "\"cipher\":\"AES-256-GCM\",\"kdf\":\"PBKDF2-SHA256\",\"iterations\":%lu,"
    "\"salt\":\"%s\",\"nonce\":\"%s\"}";
void hex(const uint8_t *data, size_t size, char *output) {
  static const char digits[] = "0123456789abcdef";
  for (size_t i = 0; i < size; ++i) {
    output[i * 2] = digits[data[i] >> 4];
    output[i * 2 + 1] = digits[data[i] & 15];
  }
  output[size * 2] = 0;
}
bool unhex(const char *text, uint8_t *data, size_t size) {
  if (strlen(text) != size * 2) return false;
  for (size_t i = 0; i < size * 2; ++i) {
    const char c = text[i];
    const int v = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
    if (v < 0) return false;
    if (!(i & 1)) data[i / 2] = v << 4; else data[i / 2] |= v;
  }
  return true;
}
bool validFirmware(const char *value) {
  if (!value[0] || strlen(value) >= 64) return false;
  for (const char *p = value; *p; ++p)
    if (!((*p >= '0' && *p <= '9') || (*p >= 'a' && *p <= 'z') ||
          (*p >= 'A' && *p <= 'Z') || *p == '.' || *p == '-' || *p == '+')) return false;
  return true;
}
bool iterationsSupported(unsigned long iterations) {
  // Retain imports of the initial, slower test backups. Do not accept an
  // attacker-controlled arbitrary work factor from the unauthenticated header.
  return iterations == BACKUP_KDF_ITERATIONS || iterations == 600000;
}
bool derive(const char *password, const uint8_t *salt, unsigned iterations, uint8_t *key) {
  return mbedtls_pkcs5_pbkdf2_hmac_ext(MBEDTLS_MD_SHA256,
      reinterpret_cast<const uint8_t *>(password), strlen(password), salt, SALT_SIZE,
      iterations, 32, key) == 0;
}
}  // namespace

bool backupPasswordValid(const char *password) {
  if (!password) return false;
  const size_t length = strnlen(password, BACKUP_PASSWORD_CAPACITY);
  if (length == BACKUP_PASSWORD_CAPACITY) return false;
  size_t characters = 0;
  // Strict UTF-8; count Unicode code points, matching Array.from in the browser.
  for (size_t i = 0; i < length;) {
    uint8_t c = password[i++];
    if (c < 0x20 || c == 0x7f) return false;
    if (c < 0x80) { ++characters; continue; }
    unsigned extra = c >= 0xc2 && c <= 0xdf ? 1 : c >= 0xe0 && c <= 0xef ? 2 : c >= 0xf0 && c <= 0xf4 ? 3 : 0;
    if (!extra || i + extra > length) return false;
    uint32_t code = c & ((1u << (6 - extra)) - 1);
    for (unsigned n = 0; n < extra; ++n) {
      c = password[i++]; if ((c & 0xc0) != 0x80) return false;
      code = (code << 6) | (c & 0x3f);
    }
    if ((extra == 1 && code < 0x80) || (extra == 2 && code < 0x800) ||
        (extra == 3 && code < 0x10000) || code > 0x10ffff ||
        (code >= 0xd800 && code <= 0xdfff)) return false;
    ++characters;
  }
  return characters >= 8 && characters <= 128;
}

bool backupEncrypt(const uint8_t *plain, size_t length, const char *password,
                   const BackupMetadata &metadata, char *file, size_t capacity) {
  if (!backupPasswordValid(password) || !validFirmware(metadata.firmware) || !iterationsSupported(metadata.kdfIterations) ||
      !length || length > SETTINGS_IMAGE_CAPACITY) return false;
  uint8_t salt[SALT_SIZE], nonce[NONCE_SIZE], key[32] = {};
  esp_fill_random(salt, sizeof(salt)); esp_fill_random(nonce, sizeof(nonce));
  char saltHex[SALT_SIZE * 2 + 1], nonceHex[NONCE_SIZE * 2 + 1];
  hex(salt, sizeof(salt), saltHex); hex(nonce, sizeof(nonce), nonceHex);
  const int headerLength = snprintf(file, capacity, HEADER_FORMAT, metadata.firmware,
      static_cast<unsigned long>(metadata.configSchema),
      static_cast<unsigned long long>(metadata.createdAt), static_cast<unsigned long>(metadata.kdfIterations), saltHex, nonceHex);
  if (headerLength < 1 || size_t(headerLength) + 2 >= capacity) return false;
  auto *encrypted = static_cast<uint8_t *>(heap_caps_malloc(length + TAG_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!encrypted) return false;
  mbedtls_gcm_context context; mbedtls_gcm_init(&context);
  bool ok = derive(password, salt, metadata.kdfIterations, key) && mbedtls_gcm_setkey(&context, MBEDTLS_CIPHER_ID_AES, key, 256) == 0 &&
      mbedtls_gcm_crypt_and_tag(&context, MBEDTLS_GCM_ENCRYPT, length, nonce, sizeof(nonce),
          reinterpret_cast<const uint8_t *>(file), headerLength, plain, encrypted,
          TAG_SIZE, encrypted + length) == 0;
  size_t encoded = 0;
  if (ok) {
    file[headerLength] = '\n';
    ok = mbedtls_base64_encode(reinterpret_cast<uint8_t *>(file + headerLength + 1),
        capacity - headerLength - 1, &encoded, encrypted, length + TAG_SIZE) == 0;
    if (ok && size_t(headerLength) + 1 + encoded < capacity) file[headerLength + 1 + encoded] = 0;
    else ok = false;
  }
  mbedtls_gcm_free(&context); mbedtls_platform_zeroize(key, sizeof(key));
  mbedtls_platform_zeroize(encrypted, length + TAG_SIZE); free(encrypted);
  return ok;
}

bool backupDecrypt(const char *file, size_t length, const char *password,
                   BackupMetadata &metadata, uint8_t *plain, size_t capacity,
                   size_t &plainLength) {
  plainLength = 0;
  if (!backupPasswordValid(password) || !length || length >= BACKUP_FILE_CAPACITY || memchr(file, 0, length)) return false;
  const char *newline = static_cast<const char *>(memchr(file, '\n', length));
  if (!newline || newline - file > 768) return false;
  char header[769], canonical[769], saltHex[33] = {}, nonceHex[25] = {};
  const size_t headerLength = newline - file;
  memcpy(header, file, headerLength); header[headerLength] = 0;
  unsigned long schema = 0, iterations = 0; unsigned long long created = 0; int consumed = 0;
  const int count = sscanf(header,
      "{\"format\":\"waveshare-hodiny-encrypted\",\"version\":1,\"project\":\"waveshare-hodiny\","
      "\"firmwareVersion\":\"%63[0-9a-zA-Z.+-]\",\"configSchema\":%lu,\"settingsSchema\":1,\"createdAt\":%llu,"
      "\"cipher\":\"AES-256-GCM\",\"kdf\":\"PBKDF2-SHA256\",\"iterations\":%lu,"
      "\"salt\":\"%32[0-9a-f]\",\"nonce\":\"%24[0-9a-f]\"}%n",
      metadata.firmware, &schema, &created, &iterations, saltHex, nonceHex, &consumed);
  if (count != 6 || consumed != int(headerLength) || schema > UINT32_MAX || !iterationsSupported(iterations) || !validFirmware(metadata.firmware)) return false;
  snprintf(canonical, sizeof(canonical), HEADER_FORMAT, metadata.firmware, schema, created, iterations, saltHex, nonceHex);
  if (strcmp(canonical, header)) return false;
  uint8_t salt[SALT_SIZE], nonce[NONCE_SIZE], key[32] = {};
  if (!unhex(saltHex, salt, sizeof(salt)) || !unhex(nonceHex, nonce, sizeof(nonce))) return false;
  const size_t encodedSize = length - headerLength - 1;
  // Only the canonical base64 alphabet; no hidden suffixes or ignored whitespace.
  if (encodedSize % 4) return false;
  for (size_t i = 0; i < encodedSize; ++i) {
    const char c = newline[1 + i];
    if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
          c == '+' || c == '/' || (c == '=' && i >= encodedSize - 2))) return false;
  }
  auto *encrypted = static_cast<uint8_t *>(heap_caps_malloc(SETTINGS_IMAGE_CAPACITY + TAG_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!encrypted) return false;
  size_t decoded = 0;
  bool ok = mbedtls_base64_decode(encrypted, SETTINGS_IMAGE_CAPACITY + TAG_SIZE, &decoded,
      reinterpret_cast<const uint8_t *>(newline + 1), encodedSize) == 0 &&
      decoded > TAG_SIZE && decoded - TAG_SIZE <= capacity;
  mbedtls_gcm_context context; mbedtls_gcm_init(&context);
  ok = ok && derive(password, salt, iterations, key) && mbedtls_gcm_setkey(&context, MBEDTLS_CIPHER_ID_AES, key, 256) == 0 &&
      mbedtls_gcm_auth_decrypt(&context, decoded - TAG_SIZE, nonce, sizeof(nonce),
          reinterpret_cast<const uint8_t *>(header), headerLength, encrypted + decoded - TAG_SIZE,
          TAG_SIZE, encrypted, plain) == 0;
  if (ok) {
    plainLength = decoded - TAG_SIZE;
    metadata.configSchema = schema; metadata.createdAt = created; metadata.kdfIterations = iterations;
  } else mbedtls_platform_zeroize(plain, capacity);
  mbedtls_gcm_free(&context); mbedtls_platform_zeroize(key, sizeof(key));
  mbedtls_platform_zeroize(encrypted, SETTINGS_IMAGE_CAPACITY + TAG_SIZE); free(encrypted);
  return ok;
}
