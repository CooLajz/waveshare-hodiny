# Configuration backup contract

This format backs up application settings, not flash partitions. It deliberately
excludes Wi-Fi, firmware binaries, OTA state, web sessions, cached weather and
the Firmware Hub publishing API key. Normal `/api/config` responses never gain
HA tokens, TMEP keys or password verification records.

## Bundle format 2 (`.whbackup`)

New exports use a JSON `waveshare-hodiny-bundle` envelope with `version: 2`,
`settings` (the encrypted format-1 file below), and `image`: a 56-character
lowercase hex `header` (fresh 16-byte salt + 12-byte nonce), `bytes` (0 or
460800), and base64 `parts`. The image is the stored 480×480 RGB565 crop,
including when disabled; the original source photograph is not retained.
The saved 16-byte `clock-bg/options` record belongs to the encrypted settings.

The image uses a separate AES-256-GCM stream with PBKDF2-SHA256/10,000 and the
same password. Its 32-byte AAD is SHA-256 of the exact 56-byte image header followed by the
exact encrypted settings file. A single AAD call avoids ESP-IDF accelerated GCM
padding differences between incremental calls. This binds the image to its settings; mixing files,
reordering/truncating blocks or altering bytes fails authentication. Blocks
contain 4096 ciphertext bytes, except the last (possibly shorter), which also
contains the 16-byte tag. An absent image has one tag-only block. Firmware
keeps two 4096-byte working blocks, never another full image in PSRAM.

The browser transfers authenticated-session blocks through `/api/backup/part`
with operation ID and strictly sequential byte offsets; a failed or interrupted
transfer must restart. Mutating settings are locked while the snapshot is in
flight. Inactive jobs time out after 60 seconds; completed jobs after 5 minutes.
`/api/backup/cancel` releases staging and display suspension. Passwords are
cleared after key derivation; GCM state is freed when the job is discarded.

Restore writes only the inactive image slot, with the display temporarily off.
After the final authentication tag, configuration/options validation and file
CRC readback, the image selector and all settings commit in the same SettingsStore
transaction. A failure or power loss before selection leaves the previous image
and settings selected. Unselected bytes need not be deleted. Restoring a backup
without an image clears image presence; importing an old settings-only backup
uses default background options. Wi-Fi remains excluded and untouched.
The browser shows progress and explains temporary display blanking.

## Encrypted settings format 1 (also accepted as a standalone `.whbackup`)

Export filenames include the source firmware version and export date/time,
for example `waveshare-hodiny-1.8.1-2026-09-08T19-45-00.whbackup`. The version
is read from the exported header, not an unrelated browser display value.

The first line is a canonical UTF-8 JSON header. The second line is base64 of
the encrypted settings image followed by its 16-byte authentication tag. There
is no trailing newline. Firmware limits the whole file to fewer than 10,000
bytes and the plaintext to 6,144 bytes.

The header identifies `waveshare-hodiny-encrypted`, file version 1, project
`waveshare-hodiny`, source `firmwareVersion`, `configSchema`, `settingsSchema`
1 and `createdAt` (Unix UTC seconds, 0 if time was not synchronized). It also
declares AES-256-GCM, PBKDF2-SHA256 with 10,000 iterations, a fresh random
16-byte salt and a fresh random 12-byte nonce, encoded as lowercase hex.

The key is derived from the exact UTF-8 password bytes. Passwords have 8–128
Unicode code points and at most 256 UTF-8 bytes; controls, malformed UTF-8 and
NULs are rejected. Passwords are neither normalized nor persisted. The exact
header bytes, excluding the line separator, are GCM additional authenticated
data. Editing a supported header field therefore fails authentication too.
All lengths and KDF parameters are bounded before cryptographic work begins.
Imports also accept 600,000 iterations from the initial test format; no arbitrary
work factor is accepted. The export work factor is deliberately calibrated for
under five seconds on ESP32-S3, per the product's local-backup threat model.
Use a strong password and keep the file out of untrusted hands.

Password derivation and settings crypto run on a dedicated PSRAM-backed task.
Image crypto advances one bounded block per request on the main task, which
also owns storage and WebServer. Worker passwords, plaintext and temporary keys
are cleared after use. Transport remains local HTTP by explicit product choice;
this protects the downloaded file, not the password's network transport.

## Payload and migrations

Settings image version 1 is a sequence of `(keyId:u8, byteLength:u16le, bytes)`.
The append-only key registry is in `SettingsStore.cpp`. Unknown keys, duplicate
keys, oversized values, invalid typed values and truncated images are rejected.
Only these namespaces are representable: `clock-config`, `clock-look`,
`web-mode`, `web-auth`, `control-api`, `save-state`, `clock-bg`. Strings include one terminal
NUL. Byte values, u32 colors and IEEE-754 binary32 values use the ESP32-S3
little-endian representation. Wi-Fi cannot be encoded by this registry.

`clock-config/config` retains the established checked binary record. Its source
schema must match the authenticated header. `clockConfigDecodeRecord()` is a
pure decoder/migrator for schemas 20, 24, 25, 26, 27, 28, 29 and current 30. It never
writes or falls back to a default configuration on malformed input. Main config,
appearance ranges, web mode, web password record and control secret are checked
before commit. Export materializes saved appearance defaults; import replaces
the settings image instead of merging with the destination. New fields on a
supported old image receive explicit migration/loader defaults, never target
device values. Add migration fixtures whenever this contract changes.

Firmware SemVer is descriptive: a newer source FW with a supported schema can
be restored. An unsupported schema or container version cannot. Schema policy
is exposed to the UI by firmware rather than duplicated there.

Legacy public JSON format 2 is still accepted by an explicitly identified
compatibility path. It has no credentials or authenticated source metadata.
Its form conversion uses defined defaults and the same atomic settings save.
Missing TMEP credentials are allowed as pending configuration. Existing HA tokens
are retained only when the HA URL is unchanged. Old JSON cannot provide the
completeness or authentication guarantees of the new encrypted format.

## Atomic storage

`SettingsPreferences` provides application-only reads/writes over a complete
settings image. The first startup copies the known legacy values into RAM;
existing namespaces are left intact. Two blobs (`settings-v1/slot0`, `slot1`)
and one selector (`active`) live in the existing 64 KiB `clockcfg` partition.
No partition offsets, sizes or Wi-Fi persistence paths change.

A transaction stages changes in PSRAM, writes the inactive slot with a SHA-256
checksum, verifies its full contents by reading it back, then switches the
selector. The main config, appearance, web settings and receipt use one commit.
The old selected slot is never erased to retry a failed write. A failed slot
write/readback or selector write leaves the original configuration selected.
A power cut before selector activation loads the old image; after activation,
it loads the complete new image. On boot a corrupt selected image is reported
as a load failure; it must not silently activate an uncommitted slot. Ordinary saves remain blocked.
A validated complete restore can recover a corrupt selected blob if the NVS
partition is still accessible: it stages a replacement without reading the old
image and activates it only after all validation and readback checks succeed.
Aborting this recovery does not write anything.

**Downgrade note:** older firmware does not know this storage format. It can
read only the legacy values present before first use of the new store. Do not
assume that downgrading firmware also migrates newer settings backwards. Keep
a suitable backup before any downgrade. OTA/flash authorization is separate.

## Operation receipts

Export/import POSTs return an accepted operation, identified by a browser-created
128-bit random ID. The browser polls `/api/backup/status?id=…`; it never retries
the import POST after an uncertain result. A restore stores the receipt in the
same atomic commit, adopts the restored runtime settings and restarts after
one second. Success is shown only after a receipt confirms the restored boot.

The ID grants access only to the status receipt. It permits post-restart checking
even if the restored web password or web mode changes access. Reading an export
result still requires the normal configuration authorization. Normal settings
saves also use durable receipts, including when they disable web access. The
receipt endpoint returns no configuration or credentials.

## Verification

`tools/test_configuration_backup.cpp` compiles the production crypto, storage
and migration code on the host using mbedTLS and an in-memory Preferences fake.
It tests supported binary migrations without writes, wrong passwords, header
authentication, corrupt/truncated ciphertext, random salt/nonce, precise scale
values, multi-key commit failure, readback failure and power cuts on both sides
of selector activation. The fake contains a Wi-Fi key whose value must remain
unchanged. Address/undefined-behavior sanitizers are enabled by the runner.

Run `tools/test_configuration_backup.sh /path/to/mbedtls-3.x` with a locally
built `library/libmbedcrypto.a`. Test fixtures contain synthetic credentials
only. Tests do not access a physical clock or a server. Browser QA uses the
actual embedded page with synthetic API responses; physical NVS, radio timing
and end-to-end restore still require an explicitly authorized device test.


## Audit findings addressed

- The old JSON was a partial, unencrypted form export. It omitted HA/TMEP
  credentials, web password and control secret, and some appearance settings.
  The encrypted export now snapshots the complete saved application image.
- Import inherited values and credential state from the destination. Encrypted
  restore now replaces the image; missing fields in supported older schemas
  receive migration defaults. The legacy JSON path is explicitly partial.
- Config, appearance and web mode previously used separate writes. Failures
  could leave a mixture, and retry-by-removing the old config risked losing it.
  A verified inactive slot plus one selector now commits them together.
- A network error could occur after persistence/runtime application. The old
  confirmation was only in RAM, and the browser could apply a mismatched reload
  before checking it. Receipts now persist with the transaction, and the browser
  verifies before changing its form or reporting success.
- The raw POST parser requested a full final buffer and waited for bytes beyond
  Content-Length. The bounded receiver now reads exactly the declared length,
  tolerates split packets and rejects truncated/oversized bodies before mutation.
- Validation tied configured TMEP slots to already-present credentials, blocking
  recovery from partial backups. Slot syntax is still validated; credentials
  may be supplied later.
- Color-scale serialization rounded values to three decimals, potentially
  collapsing distinct points. Normal JSON now preserves float round trips;
  encrypted backups preserve the original binary values.

## Device validation (2026-09-08)

The final development build was installed with the existing **home** Wi-Fi
profile on the connected Waveshare ESP32-S3 through CH343 USB–UART. Flash/NVS
partition layout and Wi-Fi credentials were not changed. The user authorized
these reversible device tests; no release was published.

| Scenario | Evidence |
| --- | --- |
| Firmware update with saved HA/Retro LCD settings | All compared persisted API fields unchanged; token remains configured |
| Encrypted export | 1.96 s through the actual dialog/download; filename contains `development`; 10,000 PBKDF2 iterations |
| Fragmented POST (0.6 s artificial gap) | Accepted; full export completed in 2.31 s |
| Short password, embedded NUL, truncated/oversized POST | Rejected before mutation |
| Wrong password / changed authenticated header | Rejected in about 1.4 s; config and receipt unchanged |
| Export interoperability | Actual device file decrypted independently by Node AES-GCM |
| Restore web password after removing it | Login required after reboot; original password works; receipt readable without a session |
| Restore a snapshot with no web password | Removes the destination password; all original application settings match |
| Decrypt, validate and commit | 1.54 s; 7.71 s including restart and reconnection |
| Real browser restore after clearing HA URL/token and changing source/style | Original HA token, source and Retro LCD restored; all compared settings identical |
| Real POST response deliberately discarded | Browser sent the import exactly once; reported success from the post-reboot receipt (11 s total) |

Host sanitizer tests cover all supported binary schemas, HA/TMEP credentials,
web auth, control secret, appearance, exact floats, no target inheritance, and
simulated write/readback/power-loss failures and explicit recovery from a corrupt
selected blob without enabling ordinary saves or changing data on abort. Power was not physically cut while
writing the device. Web-disabled restoration is covered at the storage/protocol
and browser-fixture level; no physical web-disabled test was performed.

## Background backup verification (2026-10-03)

Development device tests verified export while the stored background is disabled,
independent Node AES-GCM decryption with byte-identical 460800-byte image output,
wrong-password rejection, cancellation after the first block, and corrupted final
image-tag rejection without changing the selected image or options. A full restore
and reboot recovered the image, disabled state and 80% visibility. A format-1
settings-only backup restored with no background; tag-only format-2 export was
independently authenticated and restored on-device. The original photograph and
options were restored after testing. No physical power-cut test was performed.

The host sanitizer suite also exercises empty/full image streams, binding to the
settings file, header/ciphertext corruption and truncation. Simulated NVS write,
readback and power failures now include the background slot/options in the same
atomic transaction. Browser tests cover full/empty bundles, progress, malformed
parts, repeated dialog opening and cancelled network transfers.

The final development build also passed actual Chrome dialog export/download,
independent decryption, and dialog import with a verified post-reboot receipt.
Final device checks confirmed the original image enabled at 80%, with storage
transfer inactive and the display awake.
