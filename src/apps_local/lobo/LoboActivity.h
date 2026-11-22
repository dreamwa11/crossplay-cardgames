#pragma once

// Lobo on the device. The thin layer: renderer, storage, input, shelf.
// Rules live in LoboCore, pixels in LoboScreens.

#include <memory>

#include "../../activities/Activity.h"
#include "../ui/ToyboxScreen.h"
#include "LoboCore.h"
#include "LoboScreens.h"

class LoboActivity final : public Activity {
 public:
  LoboActivity(GfxRenderer& renderer, MappedInputManager& mappedInput) : Activity("Lobo", renderer, mappedInput) {}
  ~LoboActivity() override = default;

  static std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  void goTo(loboui::Screen next);
  void startMatch();
  void tapCard(int code);
  void commit();
  void afterHand();
  void load();
  void save();

  loboui::Screen screen = loboui::Screen::Menu;
  lobo::Match match{};
  bool hasMatch = false;  // a match in progress, resumable from the menu
  loboui::Selection sel{};
  // Settings for the next match, and the record.
  lobo::Level level = lobo::Level::Hard;
  int targetPoints = lobo::kDefaultTarget;
  int matches = 0;
  int won = 0;
  bool recorded = false;
  int howToPage = 0;
  loboui::Screen howToReturn = loboui::Screen::Menu;
  bool setupCancelable = false;

  toybox::Interactions interactions;
  bool interactionsReady = false;

  // The table is hit-tested against geometry; see Activity::surfaceMeaning().
  uint32_t surfaceMeaning() const override;
};
