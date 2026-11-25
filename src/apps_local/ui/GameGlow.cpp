#include "GameGlow.h"

#include <Logging.h>

#include "CrossPointSettings.h"

#if defined(ARDUINO_ARCH_ESP32) && !defined(SIMULATOR)
#include <HalFrontlight.h>
#include <LedManager.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <atomic>
#endif

namespace gameglow {

#if defined(ARDUINO_ARCH_ESP32) && !defined(SIMULATOR)
namespace {

std::atomic<bool> running{false};
LedManager led;

struct Saved {
  bool on;
  uint8_t brightness;
  uint8_t warmth;
};

// 0 is dark; anything else lights the panel at that percentage.
void level(const uint8_t percent) {
  if (percent == 0) {
    Frontlight.setOn(false);
    return;
  }
  Frontlight.setBrightness(percent);
  Frontlight.setOn(true);
}

// Where the light rests between pulses: the player's own level, or dark.
uint8_t rest(const Saved& s) { return s.on ? s.brightness : 0; }

void glowTask(void* arg) {
  const bool won = arg != nullptr;
  // Let the result screen land on the panel first.
  vTaskDelay(pdMS_TO_TICKS(900));
  const Saved s{Frontlight.isOn(), Frontlight.brightness(), Frontlight.warmth()};
  const bool light = Frontlight.present() && Frontlight.isReady();
  const bool hasLed = led.begin();

  if (hasLed) led.flash(won ? LedColor::green() : LedColor::red(), won ? 3 : 1, won ? 140 : 1400, won ? 140 : 0);
  if (light) {
    if (Frontlight.hasColorTemperature()) Frontlight.setWarmth(won ? 0 : 100);
    if (won) {
      // Three quick flashes.
      for (int k = 0; k < 3; ++k) {
        level(100);
        vTaskDelay(pdMS_TO_TICKS(140));
        level(rest(s));
        vTaskDelay(pdMS_TO_TICKS(140));
      }
    } else {
      // One long swell, a held glow, then a soft fade back.
      const int from = rest(s);
      const int peak = 70 > from ? 70 : from;
      for (int t = 0; t <= 10; ++t) {
        level(static_cast<uint8_t>(from + (peak - from) * t / 10));
        vTaskDelay(pdMS_TO_TICKS(30));
      }
      vTaskDelay(pdMS_TO_TICKS(700));
      for (int t = 30; t >= 0; --t) {
        level(static_cast<uint8_t>(from + (peak - from) * t / 30));
        vTaskDelay(pdMS_TO_TICKS(40));
      }
    }
    Frontlight.setBrightness(s.brightness);
    Frontlight.setOn(s.on);
    if (Frontlight.hasColorTemperature()) Frontlight.setWarmth(s.warmth);
  }
  if (hasLed) {
    while (led.isFlashing()) {
      led.update();
      vTaskDelay(pdMS_TO_TICKS(10));
    }
    led.clear();
    led.show();
  }
  running = false;
  vTaskDelete(nullptr);
}

void start(const bool won) {
  if (!SETTINGS.gameLightFx) return;
  bool idle = false;
  if (!running.compare_exchange_strong(idle, true)) return;
  // 3 KB: the pattern is a few HAL calls and delays, plus the PMIC's I2C path.
  if (xTaskCreate(&glowTask, "GameGlow", 3072, won ? reinterpret_cast<void*>(1) : nullptr, 1, nullptr) != pdPASS) {
    LOG_ERR("GLOW", "Could not start the result light");
    running = false;
  }
}

}  // namespace

void win() { start(true); }
void lose() { start(false); }

#else

void win() { LOG_DBG("GLOW", "win"); }
void lose() { LOG_DBG("GLOW", "lose"); }

#endif

}  // namespace gameglow
