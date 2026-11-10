#include "updater.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Update.h>
#include <ArduinoJson.h>
#include <esp_ota_ops.h>
#include <mbedtls/md.h>
#include <mbedtls/pk.h>
#include <mbedtls/base64.h>
#include <time.h>
#include "config.h"
#include "settings.h"
#include "net.h"
#include "imu.h"
#include "pubkey.h"

// Die Arduino-Grundlage bestätigt neue Versionen sonst sofort beim Start.
// So entscheidet Health:: selbst, wann die Version als gut gilt.
extern "C" bool verifyRollbackLater() { return true; }

namespace Updater {

static volatile State st = IDLE;
static volatile float prog = 0;
static volatile bool requested = false, installAnytime = false;
static char result[96] = "noch nicht geprüft";
static int lastNightDay = -1;
static uint32_t lastTestCheck = 0;

// "1.2.3" > "1.2.0" ?
static bool newer(const char *a, const char *b) {
  int x[3] = {0}, y[3] = {0};
  sscanf(a, "%d.%d.%d", &x[0], &x[1], &x[2]);
  sscanf(b, "%d.%d.%d", &y[0], &y[1], &y[2]);
  for (int i = 0; i < 3; i++) if (x[i] != y[i]) return x[i] > y[i];
  return false;
}

static void hex(const uint8_t *in, size_t n, char *out) {
  for (size_t i = 0; i < n; i++) sprintf(out + 2 * i, "%02x", in[i]);
}

// Signiert ist der Text "xmax_led|<version>|<sha256 der Firmware>": So lässt sich weder eine
// fremde Firmware unterschieben noch eine alte Version als neue ausgeben.
static bool signatureOk(const String &version, const char *shaHex, const char *sigB64) {
  uint8_t sig[128];
  size_t sigLen = 0;
  if (mbedtls_base64_decode(sig, sizeof(sig), &sigLen, (const uint8_t *)sigB64, strlen(sigB64)) != 0) return false;
  String msg = String("xmax_led|") + version + "|" + shaHex;
  uint8_t h[32];
  mbedtls_md(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), (const uint8_t *)msg.c_str(), msg.length(), h);
  mbedtls_pk_context pk;
  mbedtls_pk_init(&pk);
  bool ok = mbedtls_pk_parse_public_key(&pk, (const uint8_t *)OTA_PUBKEY_PEM, sizeof(OTA_PUBKEY_PEM)) == 0 &&
            mbedtls_pk_verify(&pk, MBEDTLS_MD_SHA256, h, sizeof(h), sig, sigLen) == 0;
  mbedtls_pk_free(&pk);
  return ok;
}

static void fail(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(result, sizeof(result), fmt, ap);
  va_end(ap);
  const char *why = result;
  Serial.printf("Update: %s\n", why);
  st = FAILED;
}

static void run() {
  st = CHECKING;
  prog = 0;
  WiFiClientSecure client;
  client.setInsecure();   // Echtheit sichert die Signatur, nicht TLS
  HTTPClient http;
  http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
  http.setTimeout(20000);

  String url = String(OTA_MANIFEST_BASE) + settings.channel + ".json?t=" + String(esp_random());
  Serial.printf("Update: suche, Kanal %s, Speicher frei %u (Block %u)\n", settings.channel.c_str(),
                ESP.getFreeHeap(), ESP.getMaxAllocHeap());
  if (!Net::connected()) return fail("kein WLAN");
  if (!http.begin(client, url)) return fail("Manifest: keine Verbindung");
  int code = http.GET();
  if (code < 0) {
    char tls[96] = "";
    int e = client.lastError(tls, sizeof(tls));
    http.end();
    return fail("Manifest: %s (TLS %d %s)", HTTPClient::errorToString(code).c_str(), e, tls);
  }
  if (code != 200) { http.end(); return fail("Manifest: HTTP %d", code); }
  JsonDocument doc;
  if (deserializeJson(doc, http.getString())) { http.end(); return fail("Manifest unlesbar"); }
  http.end();

  String version = doc["version"] | "";
  String binUrl = doc["url"] | "";
  String shaWant = doc["sha256"] | "";
  String sig = doc["sig"] | "";
  int size = doc["size"] | 0;
  if (!newer(version.c_str(), FW_VERSION)) {
    snprintf(result, sizeof(result), "aktuell (%s)", FW_VERSION);
    Serial.printf("Update: %s, Kanal %s\n", result, settings.channel.c_str());
    st = DONE;
    return;
  }
  Serial.printf("Update: %s -> %s (%d Bytes)\n", FW_VERSION, version.c_str(), size);

  st = DOWNLOADING;
  if (!http.begin(client, binUrl)) return fail("Firmware: keine Verbindung");
  code = http.GET();
  if (code != 200) { http.end(); return fail("Firmware: HTTP %d", code); }
  int len = http.getSize();
  if (len != size || size <= 0) { http.end(); return fail("Firmware: falsche Größe"); }
  if (!Update.begin(size, U_FLASH)) { http.end(); return fail("Kein Platz für das Update"); }

  mbedtls_md_context_t md;
  mbedtls_md_init(&md);
  mbedtls_md_setup(&md, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 0);
  mbedtls_md_starts(&md);
  WiFiClient *s = http.getStreamPtr();
  static uint8_t buf[4096];
  int got = 0;
  uint32_t lastData = millis();
  while (got < size) {
    int avail = s->available();
    if (avail > 0) {
      int n = s->readBytes(buf, min(avail, (int)sizeof(buf)));
      if (n <= 0) continue;
      mbedtls_md_update(&md, buf, n);
      if (Update.write(buf, n) != (size_t)n) break;
      got += n;
      prog = (float)got / size;
      lastData = millis();
    } else {
      if (millis() - lastData > 20000 || !http.connected()) break;
      delay(5);
    }
  }
  http.end();
  uint8_t shaBin[32];
  char shaHex[65];
  mbedtls_md_finish(&md, shaBin);
  mbedtls_md_free(&md);
  hex(shaBin, 32, shaHex);

  if (got != size) { Update.abort(); return fail("Download abgebrochen"); }
  if (shaWant != shaHex) { Update.abort(); return fail("Prüfsumme falsch"); }
  if (!signatureOk(version, shaHex, sig.c_str())) { Update.abort(); return fail("Signatur ungültig - nicht installiert"); }
  if (!Update.end(true)) { return fail("Aufspielen fehlgeschlagen"); }

  snprintf(result, sizeof(result), "%s installiert, Neustart", version.c_str());
  Serial.printf("Update: %s\n", result);
  st = DONE;
  delay(1500);
  ESP.restart();
}

static void task(void *) {
  for (;;) {
    if (requested) {
      requested = false;
      run();
    }
    vTaskDelay(pdMS_TO_TICKS(500));
  }
}

void begin() {
  xTaskCreatePinnedToCore(task, "update", 16384, nullptr, 1, nullptr, 0);
}

void checkNow() {
  if (st == CHECKING || st == DOWNLOADING) return;
  requested = true;
}

void loop() {
  if (st == CHECKING || st == DOWNLOADING || requested) return;
  if (!Net::connected() || !Net::timeValid() || Health::pending()) return;
  bool idle = millis() - Imu::lastMotionMs() > OTA_IDLE_MIN * 60000UL;
  if (!idle) return;

  if (settings.channel == "test") {
    if (lastTestCheck == 0 || millis() - lastTestCheck > OTA_TEST_INTERVAL_MIN * 60000UL) {
      lastTestCheck = millis();
      checkNow();
    }
    return;
  }
  // Kanal stabil: einmal pro Nacht, zur gerätetypischen Minute
  time_t now = time(nullptr);
  struct tm t;
  localtime_r(&now, &t);
  int minuteOfWindow = (t.tm_hour - OTA_NIGHT_FROM) * 60 + t.tm_min;
  bool inWindow = t.tm_hour >= OTA_NIGHT_FROM && t.tm_hour < OTA_NIGHT_TO;
  if (inWindow && minuteOfWindow >= settings.otaMinute && lastNightDay != t.tm_yday) {
    lastNightDay = t.tm_yday;
    checkNow();
  }
}

State state() { return st; }
float progress() { return prog; }
const char *lastResult() { return result; }

}  // namespace Updater

namespace Health {

static bool isPending = false;

void begin() {
  const esp_partition_t *run = esp_ota_get_running_partition();
  esp_ota_img_states_t s;
  if (esp_ota_get_state_partition(run, &s) == ESP_OK && s == ESP_OTA_IMG_PENDING_VERIFY) {
    isPending = true;
    Serial.printf("Neue Version %s läuft auf Probe\n", FW_VERSION);
  } else {
    // erste Installation per USB oder schon bestätigt
    esp_ota_mark_app_valid_cancel_rollback();
  }
}

void loop() {
  if (!isPending) return;
  uint32_t up = millis();
  // Nach 90 s ohne Absturz: gut, wenn kein WLAN eingetragen ist oder es verbunden ist
  if (up > 90000 && (!Net::configured() || Net::connected())) {
    esp_ota_mark_app_valid_cancel_rollback();
    isPending = false;
    Serial.println("Neue Version bestätigt");
    return;
  }
  // Nach 5 min: Netz ist zu sehen, aber keine Verbindung -> diese Version kann kein WLAN,
  // also zurück zur alten. Ist das Netz gar nicht zu sehen (Router aus), trotzdem behalten.
  if (up > 300000) {
    if (Net::configured() && Net::ssidVisible() && !Net::connected()) {
      Serial.println("Neue Version bekommt kein WLAN - zurück zur alten");
      esp_ota_mark_app_invalid_rollback_and_reboot();
    }
    esp_ota_mark_app_valid_cancel_rollback();
    isPending = false;
  }
}

bool pending() { return isPending; }

}  // namespace Health
