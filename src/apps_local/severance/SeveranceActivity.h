#pragma once

// Severance on the device. The thin layer: renderer, storage, input, shelf.
// Rules live in SeveranceCore, pixels in SeveranceScreens.

#include <memory>

#include "../../activities/Activity.h"
#include "../ui/ToyboxScreen.h"
#include "SeveranceCore.h"
#include "SeveranceScreens.h"

class SeveranceActivity final : public Activity {
 public:
  SeveranceActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Severance", renderer, mappedInput) {}
  ~SeveranceActivity() override = default;

  static std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  void goTo(sevui::Screen next);
  void newQuarter();
  void tapBoard(int x, int y);
  void tapCard(int id, bool mini);
  void afterAction();
  void pruneSelection();
  void setNote(const char* text);
  bool toggle(int v, int max);
  int selectedMain() const;
  void load();
  void save();

  sevui::Screen screen = sevui::Screen::Menu;
  std::unique_ptr<sev::Game> game;
  bool hasGame = false;
  uint8_t sel[sevui::kMaxSel] = {};
  int nSel = 0;
  char note[200] = {};
  int annIndex = -1;
  bool showEnd = false;
  bool lenient = false;
  int played = 0;
  int won = 0;
  int rulesPage = 0;
  sevui::Screen rulesReturn = sevui::Screen::Menu;

  toybox::Interactions interactions;
  bool interactionsReady = false;

  uint32_t surfaceMeaning() const override;
};
