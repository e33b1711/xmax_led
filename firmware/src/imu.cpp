#include "imu.h"
#include <Wire.h>
#include <ICM_20948.h>
#include "config.h"

namespace Imu {

static ICM_20948_I2C icm;
static bool found = false;
static float a[3] = {0, 0, 0};        // geglättete Beschleunigung in g (Sensorachsen)
static float m[3] = {0, 1, 0};        // Schwerkraft in Matrix-Richtung
static float last[3] = {0, 0, 0};
static uint32_t motionMs = 0;

bool begin() {
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(400000);
  // Adresse 0x69 (AD0 hoch, SparkFun/Adafruit) oder 0x68 (viele günstige Platinen)
  int used = -1;
  for (int ad0 = 1; ad0 >= 0 && !found; ad0--) {
    icm.begin(Wire, ad0);
    if (icm.status == ICM_20948_Stat_Ok) { found = true; used = ad0; }
  }
  if (found) {
    Serial.printf("ICM-20948 gefunden (Adresse 0x%02X)\n", 0x68 | used);
  } else {
    Serial.println("Kein ICM-20948 gefunden - Sanduhr läuft wie senkrecht stehend");
  }
  motionMs = millis();
  return found;
}

static float axis(int i, int sign) { return sign * a[i]; }

void update() {
  if (!found || !icm.dataReady()) return;
  icm.getAGMT();
  float raw[3] = {icm.accX() / 1000.0f, icm.accY() / 1000.0f, icm.accZ() / 1000.0f};   // mg -> g
  for (int i = 0; i < 3; i++) a[i] += 0.35f * (raw[i] - a[i]);
  // Der Sensor misst die Gegenkraft zur Schwerkraft (+1 g nach oben) -> Vorzeichen umdrehen
  m[0] = -axis(IMU_MX_AXIS, IMU_MX_SIGN);
  m[1] = -axis(IMU_MY_AXIS, IMU_MY_SIGN);
  int zAxis = 3 - IMU_MX_AXIS - IMU_MY_AXIS;
  m[2] = -a[zAxis];
  float d = fabsf(raw[0] - last[0]) + fabsf(raw[1] - last[1]) + fabsf(raw[2] - last[2]);
  if (d > 0.08f) motionMs = millis();
  for (int i = 0; i < 3; i++) last[i] = raw[i];
}

bool ok() { return found; }
float gx() { return m[0]; }
float gy() { return m[1]; }
float gz() { return m[2]; }
uint32_t lastMotionMs() { return motionMs; }

void debugPrint() {
  if (!found) return;
  Serial.printf("Sensor X=%+.2f Y=%+.2f Z=%+.2f g  ->  Matrix gx=%+.2f gy=%+.2f\n", a[0], a[1], a[2], m[0], m[1]);
}

}  // namespace Imu
