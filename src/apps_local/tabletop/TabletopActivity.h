#pragma once

// Tabletop on the device: playing cards, tarot, dice and the oracle. The thin layer:
// renderer, storage, input, shelf. State lives in TabletopCore, pixels in
// TabletopScreens.

#include <memory>

#include "../../activities/Activity.h"
#include "../ui/ToyboxScreen.h"
#include "TabletopCore.h"
#include "TabletopScreens.h"

class TabletopActivity final : public Activity {
 public:
  TabletopActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Tabletop", renderer, mappedInput) {}
  ~TabletopActivity() override = default;

  static std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  void goTo(tabletopui::Screen next);
  void load();
  void save();

  tabletopui::Screen screen = tabletopui::Screen::Menu;
  tabletop::Table table{};

  toybox::Interactions interactions;
  bool interactionsReady = false;
};
