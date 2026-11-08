#include "sand.h"
#include <algorithm>
#include "config.h"
#include "leds.h"

namespace Sand {

static const int W = MATRIX_W, H = MATRIX_H;
static const float G = 140.0f;        // LEDs/s² bei 1 g
static const int SUB = 2;
static const float BOUNCE = 0.35f;

struct Grain {
  float x, y, vx, vy;
  uint8_t shade;   // Helligkeitsvariante je Korn
};

static uint8_t inside[W * H], wall[W * H];
static int16_t occ[W * H];             // Korn-Nummer + 1, 0 = frei
static Grain grains[W * H];
static int nGrains = 0;
static float tokens = 0;               // Durchlass am Hals (Körner)
static float fillFrac = 0.8f;
static int upperCells[W * H][2];
static int nUpper = 0;

static inline int idx(int x, int y) { return x * H + y; }
static inline int half(int y) { return y < H / 2 ? 0 : 1; }
static inline bool isFree(int x, int y) {
  return x >= 0 && y >= 0 && x < W && y < H && inside[idx(x, y)] && !occ[idx(x, y)];
}
static inline float frand() { return (float)(esp_random() & 0xFFFF) / 65535.0f; }

static void buildGlass() {
  memset(inside, 0, sizeof(inside));
  memset(wall, 0, sizeof(wall));
  const float quarter = H / 4.0f;
  for (int y = 0; y < H; y++) {
    float d = fabsf(y + 0.5f - H / 2.0f);                     // 0,5 … H/2-0,5
    float hw = 1 + 6 * powf(fminf(1.0f, (d - 0.5f) / quarter), 0.75f);
    if (d > H / 2.0f - 1.5f) hw -= 1.5f;
    else if (d > H / 2.0f - 2.5f) hw -= 0.5f;
    for (int x = 0; x < W; x++)
      if (fabsf(x + 0.5f - W / 2.0f) < hw) inside[idx(x, y)] = 1;
  }
  for (int x = 0; x < W; x++)
    for (int y = 0; y < H; y++) {
      if (inside[idx(x, y)]) continue;
      bool near = false;
      for (int a = -1; a <= 1; a++)
        for (int b = -1; b <= 1; b++) {
          int u = x + a, v = y + b;
          if (u >= 0 && v >= 0 && u < W && v < H && inside[idx(u, v)]) near = true;
        }
      wall[idx(x, y)] = near;
    }
  nUpper = 0;
  for (int y = H / 2 - 1; y >= 0; y--)
    for (int x = 0; x < W; x++)
      if (inside[idx(x, y)]) { upperCells[nUpper][0] = x; upperCells[nUpper][1] = y; nUpper++; }
}

static void place(bool flipped) {
  memset(occ, 0, sizeof(occ));
  nGrains = (int)(nUpper * fillFrac);
  for (int k = 0; k < nGrains; k++) {
    int x = upperCells[k][0], y = upperCells[k][1];
    if (flipped) { x = W - 1 - x; y = H - 1 - y; }
    grains[k] = {x + 0.5f, y + 0.5f, 0, 0, (uint8_t)(200 + (esp_random() % 56))};
    occ[idx(x, y)] = k + 1;
  }
  tokens = 0;
}

void begin(float fill) {
  fillFrac = fill;
  buildGlass();
  place(false);
}

void refill() { place(false); }

int count() { return nGrains; }

void step(float gx, float gy, float dt, float grainsPerSecond) {
  tokens = fminf(1.5f, tokens + grainsPerSecond * dt);
  float gl = sqrtf(gx * gx + gy * gy);
  float dx = gl > 0.01f ? gx / gl : 0, dy = gl > 0.01f ? gy / gl : 1;
  gx *= G; gy *= G;
  const float sdt = dt / SUB, maxv = 0.9f / sdt;

  for (int s = 0; s < SUB; s++) {
    // untere Körner (in Fallrichtung) zuerst, damit Säulen nachrutschen
    std::sort(grains, grains + nGrains, [&](const Grain &a, const Grain &b) {
      return (a.x * dx + a.y * dy) > (b.x * dx + b.y * dy);
    });
    for (int i = 0; i < nGrains; i++) occ[idx((int)grains[i].x, (int)grains[i].y)] = i + 1;

    for (int i = 0; i < nGrains; i++) {
      Grain &g = grains[i];
      const int cx = (int)g.x, cy = (int)g.y;
      // Liegt das Korn auf, bremst die Reibung; sonst etwas Zufall für natürliches Rieseln
      int bx = cx + (int)roundf(dx), by = cy + (int)roundf(dy);
      bool bCross = by != cy && half(by) != half(cy);
      bool supported = !isFree(bx, by) || (bCross && tokens < 1);
      if (supported) { g.vx *= 0.2f; g.vy *= 0.2f; }
      else { g.vx += (frand() - 0.5f) * 0.6f; g.vy += (frand() - 0.5f) * 0.6f; }
      g.vx += gx * sdt; g.vy += gy * sdt;
      float sp = sqrtf(g.vx * g.vx + g.vy * g.vy);
      if (sp > maxv) { g.vx *= maxv / sp; g.vy *= maxv / sp; }

      float nx = g.x + g.vx * sdt, ny = g.y + g.vy * sdt;
      int tx = (int)floorf(nx), ty = (int)floorf(ny);
      if (tx == cx && ty == cy) { g.x = nx; g.y = ny; continue; }

      bool crossing = ty != cy && half(ty) != half(cy);
      bool gateOk = !crossing || tokens >= 1;
      auto go = [&](int x, int y, float px, float py) {
        if (half(y) != half(cy)) tokens -= 1;
        occ[idx(cx, cy)] = 0; occ[idx(x, y)] = i + 1; g.x = px; g.y = py;
      };
      if (gateOk && isFree(tx, ty)) { go(tx, ty, nx, ny); continue; }
      if (tx != cx && isFree(tx, cy)) { g.vy *= -BOUNCE; go(tx, cy, nx, cy + 0.5f); continue; }
      if (ty != cy && gateOk && isFree(cx, ty)) { g.vx *= -BOUNCE; go(cx, ty, cx + 0.5f, ny); continue; }
      if (tx != cx) g.vx *= -BOUNCE;
      if (ty != cy) g.vy *= -BOUNCE;
      g.x = fminf(cx + 0.999f, fmaxf(cx + 0.001f, nx));
      g.y = fminf(cy + 0.999f, fmaxf(cy + 0.001f, ny));
      // aufliegend: schräg nach unten abrollen, wenn dort Platz ist
      float a = atan2f(dy, dx);
      int side = (esp_random() & 1) ? 1 : -1;
      for (int k = 0; k < 2; k++) {
        int sgn = k == 0 ? side : -side;
        int ax = cx + (int)roundf(cosf(a + sgn * PI / 4)), ay = cy + (int)roundf(sinf(a + sgn * PI / 4));
        bool cr = ay != cy && half(ay) != half(cy);
        if ((!cr || tokens >= 1) && isFree(ax, ay) && frand() < 0.6f) {
          go(ax, ay, ax + 0.5f, ay + 0.5f);
          g.vx = dx * 3; g.vy = dy * 3;
          break;
        }
      }
    }
  }
}

void render() {
  for (int x = 0; x < W; x++)
    for (int y = 0; y < H; y++)
      if (wall[idx(x, y)]) Leds::set(x, y, CRGB(14, 20, 34));   // Glas, schwach bläulich
  for (int i = 0; i < nGrains; i++) {
    uint8_t s = grains[i].shade;
    Leds::set((int)grains[i].x, (int)grains[i].y, CRGB(scale8(255, s), scale8(150, s), scale8(30, s)));
  }
}

}  // namespace Sand
