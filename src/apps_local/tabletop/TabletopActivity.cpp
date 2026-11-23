#include "TabletopActivity.h"

#include <HalStorage.h>
#include <Logging.h>
#include <Memory.h>

#include "../Shelf.h"
#include "../ui/Toybox.h"
#include "../ui/ToyboxFonts.h"
#include "../ui/ToyboxSeed.h"
#include "../ui/ToyboxTheme.h"

namespace tt = tabletop;
namespace ui = tabletopui;

namespace {
#if defined(ARDUINO_ARCH_ESP32) || defined(SIMULATOR)
constexpr char kSavePath[] = "/.crosspoint/tabletop.sav";
#endif
// Two decks of up to 78 cards at four bytes each, the dice and the settings.
constexpr int kSaveBytes = 1024;

int faceId(const ui::Face face) {
  switch (face) {
    case ui::Face::Tile:
      return toybox::kTileFontId;
    case ui::Face::Button:
      return toybox::kButtonFontId;
    case ui::Face::Ui:
      return toybox::kUiFontId;
    case ui::Face::Display:
      return toybox::kDisplayFontId;
    case ui::Face::Large:
      return toybox::kLargeFontId;
    case ui::Face::Huge:
    default:
      return toybox::kHugeFontId;
  }
}

struct TargetFonts final : ui::FontBinder {
  freeink::ui::GfxRendererTarget& target;
  explicit TargetFonts(freeink::ui::GfxRendererTarget& t) : target(t) {}
  void bind(const freeink::ui::FontId slot, const ui::Face face) override { target.setFont(slot, faceId(face)); }
};
}  // namespace

std::unique_ptr<Activity> TabletopActivity::create(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeUniqueNoThrow<TabletopActivity>(renderer, mappedInput);
}

void TabletopActivity::onEnter() {
  Activity::onEnter();
  toybox::ensureFonts(renderer);
  load();
  screen = ui::Screen::Menu;
  requestUpdate();
}

void TabletopActivity::onExit() {
  save();
  Activity::onExit();
}

void TabletopActivity::load() {
  tt::start(table, toybox::seed());
#if defined(ARDUINO_ARCH_ESP32) || defined(SIMULATOR)
  if (!Storage.exists(kSavePath)) return;
  char buffer[kSaveBytes] = {};
  if (Storage.readFileToBuffer(kSavePath, buffer, sizeof(buffer) - 1) == 0) return;
  tt::Table saved;
  if (tt::parse(buffer, saved)) {
    table = saved;
    // A fresh seed each visit, so a saved table does not replay the same luck.
    table.rng.seed(table.rng.s ^ toybox::seed());
  } else {
    LOG_ERR("TABLETOP", "Save file did not parse; starting fresh");
  }
#endif
}

void TabletopActivity::save() {
#if defined(ARDUINO_ARCH_ESP32) || defined(SIMULATOR)
  char buffer[kSaveBytes];
  if (tt::serialize(table, buffer, sizeof(buffer)) < 0) {
    LOG_ERR("TABLETOP", "Table did not fit %d bytes", kSaveBytes);
    return;
  }
  Storage.writeFile(kSavePath, String(buffer));
#endif
}

void TabletopActivity::goTo(const ui::Screen next) {
  screen = next;
  requestUpdate();
}

void TabletopActivity::loop() {
  namespace fui = freeink::ui;

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    if (screen == ui::Screen::Menu) {
      save();
      shelf::leave(renderer, mappedInput);
    } else {
      save();
      goTo(ui::Screen::Menu);
    }
    return;
  }

  int tapX = 0;
  int tapY = 0;
  if (!mappedInput.wasScreenTapped(tapX, tapY)) return;
  if (!interactionsReady) return;
  fui::InputSnapshot input;
  input.touchReleased = true;
  input.touchX = static_cast<int16_t>(tapX);
  input.touchY = static_cast<int16_t>(tapY);
  const fui::ActionEvent event = interactions.route(input);
  switch (event.action) {
    case ui::ActionOpen:
      if (event.value >= 1 && event.value <= 3) goTo(static_cast<ui::Screen>(event.value));
      return;
    case ui::ActionBack:
      save();
      goTo(ui::Screen::Menu);
      return;
    case ui::ActionDraw:
      if (screen == ui::Screen::Cards) {
        tt::draw(table, table.cards, 1, false);
      } else if (screen == ui::Screen::Tarot) {
        tt::draw(table, table.tarot, table.spread ? tt::kSpread : 1, true);
      }
      break;
    case ui::ActionShuffle:
      if (screen == ui::Screen::Cards) {
        tt::shuffle(table, table.cards, false);
      } else if (screen == ui::Screen::Tarot) {
        tt::shuffle(table, table.tarot, true);
      }
      break;
    case ui::ActionJokers:
      table.jokers = !table.jokers;
      // The deck changes size, so it is gathered and shuffled.
      tt::shuffle(table, table.cards, false);
      break;
    case ui::ActionReversals:
      table.reversals = !table.reversals;
      break;
    case ui::ActionSpread:
      table.spread = event.value == 1;
      break;
    case ui::ActionAddDie:
      if (event.value >= 0 && event.value < tt::kDieKinds) tt::addDie(table.dice, static_cast<tt::Die>(event.value));
      break;
    case ui::ActionRemoveDie:
      tt::removeDie(table.dice, event.value);
      break;
    case ui::ActionClear:
      tt::clearDice(table.dice);
      break;
    case ui::ActionRoll:
      tt::roll(table);
      break;
    default:
      return;
  }
  save();
  requestUpdate();
}

void TabletopActivity::render(RenderLock&&) {
  namespace fui = freeink::ui;

  renderer.clearScreen();
  fui::GfxRendererTarget target = toybox::makeTarget(renderer);
  TargetFonts fonts(target);
  ui::useFonts(&fonts);
  const fui::DeviceContext device = target.deviceContext();
  const fui::InputSnapshot noInput{};
  interactionsReady = false;
  toybox::Frame frame(target, device, noInput, interactions);
  toybox::Screen surface(frame);

  switch (screen) {
    case ui::Screen::Menu:
      ui::buildMenu(surface, table);
      break;
    case ui::Screen::Cards:
      ui::buildCards(surface, table);
      break;
    case ui::Screen::Tarot:
      ui::buildTarot(surface, table);
      break;
    case ui::Screen::Dice:
      ui::buildDice(surface, table);
      break;
  }

  ui::useFonts(nullptr);
  interactionsReady = true;
  toybox::reportOverflow(interactions, "Tabletop");

  const auto labels = mappedInput.mapLabels("Back", "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
