#include "TabletopActivity.h"

#include <HalStorage.h>
#include <Logging.h>
#include <Memory.h>

#include <cstring>

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
// Two decks of up to 78 cards at four bytes each, the dice, the settings and
// the oracle's log of twelve readings.
constexpr int kSaveBytes = 2048;

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
  // On the heap: 2 KB is too much for the loop task's stack.
  auto buffer = makeUniqueNoThrow<char[]>(kSaveBytes);
  if (!buffer) {
    LOG_ERR("TABLETOP", "OOM: %d bytes for the save", kSaveBytes);
    return;
  }
  std::memset(buffer.get(), 0, kSaveBytes);
  if (Storage.readFileToBuffer(kSavePath, buffer.get(), kSaveBytes - 1) == 0) return;
  tt::Table saved;
  if (tt::parse(buffer.get(), saved)) {
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
  auto buffer = makeUniqueNoThrow<char[]>(kSaveBytes);
  if (!buffer) {
    LOG_ERR("TABLETOP", "OOM: %d bytes for the save", kSaveBytes);
    return;
  }
  if (tt::serialize(table, buffer.get(), kSaveBytes) < 0) {
    LOG_ERR("TABLETOP", "Table did not fit %d bytes", kSaveBytes);
    return;
  }
  Storage.writeFile(kSavePath, String(buffer.get()));
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
      goTo(screen == ui::Screen::OracleLog ? ui::Screen::Oracle : ui::Screen::Menu);
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
      if (event.value >= 1 && event.value <= 4) goTo(static_cast<ui::Screen>(event.value));
      return;
    case ui::ActionBack:
      save();
      goTo(screen == ui::Screen::OracleLog ? ui::Screen::Oracle : ui::Screen::Menu);
      return;
    case ui::ActionAsk:
      if (event.value < 0 || event.value >= tt::kOddsKinds) return;
      table.oracle.odds = static_cast<tt::Odds>(event.value);
      tt::consult(table.oracle, table.rng, tt::Kind::Ask);
      break;
    case ui::ActionConsult:
      if (event.value < 0 || event.value >= tt::kKinds) return;
      tt::consult(table.oracle, table.rng, static_cast<tt::Kind>(event.value));
      break;
    case ui::ActionAgain:
      if (table.oracle.len == 0) return;
      tt::consult(table.oracle, table.rng, static_cast<tt::Kind>(table.oracle.log[table.oracle.len - 1].kind));
      break;
    case ui::ActionGenre:
      table.oracle.genre = static_cast<tt::Genre>((static_cast<int>(table.oracle.genre) + 1) % tt::kGenres);
      break;
    case ui::ActionLog:
      logPage = 0;
      goTo(ui::Screen::OracleLog);
      return;
    case ui::ActionLogPage:
      ++logPage;
      requestUpdate();
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
    case ui::ActionArcana:
      table.arcana = static_cast<tt::Arcana>((static_cast<int>(table.arcana) + 1) % static_cast<int>(tt::Arcana::Count));
      // The deck changes, so it is gathered and shuffled.
      tt::shuffle(table, table.tarot, true);
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
      if (screen == ui::Screen::OracleLog) {
        table.oracle.len = 0;
        logPage = 0;
      } else {
        tt::clearDice(table.dice);
      }
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
    case ui::Screen::Oracle:
      ui::buildOracle(surface, table);
      break;
    case ui::Screen::OracleLog:
      ui::buildOracleLog(surface, table, logPage);
      break;
  }

  ui::useFonts(nullptr);
  interactionsReady = true;
  toybox::reportOverflow(interactions, "Tabletop");

  const auto labels = mappedInput.mapLabels("Back", "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
