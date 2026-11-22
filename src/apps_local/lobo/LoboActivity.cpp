#include "LoboActivity.h"

#include <HalStorage.h>
#include <Logging.h>
#include <Memory.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "../Shelf.h"
#include "../scoundrel/ScoundrelFonts.h"
#include "../ui/Toybox.h"
#include "../ui/ToyboxFonts.h"
#include "../ui/ToyboxSeed.h"
#include "../ui/ToyboxTheme.h"

namespace lb = lobo;
namespace ui = loboui;

namespace {
#if defined(ARDUINO_ARCH_ESP32) || defined(SIMULATOR)
constexpr char kSavePath[] = "/.crosspoint/lobo.sav";
#endif

// The settings line plus a match: fifty cards at up to three bytes each, and
// the scores and the last move, fit with room to spare.
constexpr int kSaveBytes = 1024;
constexpr int kTargetStep = 10;

namespace sf = scoundrelfonts;

// Scoundrel's faces, so Lobo's buttons and list rows match the text the
// screens draw.
toybox::Faces tableFaces() { return toybox::Faces{sf::kSmallId, sf::kLargeId, sf::kValueId}; }

int faceId(const ui::Face face) {
  switch (face) {
    case ui::Face::Small:
      return sf::kSmallId;
    case ui::Face::Body:
      return sf::kBodyId;
    case ui::Face::Large:
      return sf::kLargeId;
    case ui::Face::Value:
      return sf::kValueId;
    case ui::Face::ValueBig:
      return sf::kValueBigId;
    case ui::Face::Title:
      return sf::kTitleId;
    case ui::Face::Logo:
    default:
      return sf::kLogoId;
  }
}

struct TargetFonts final : ui::FontBinder {
  freeink::ui::GfxRendererTarget& target;
  explicit TargetFonts(freeink::ui::GfxRendererTarget& t) : target(t) {}
  void bind(const freeink::ui::FontId slot, const ui::Face face) override { target.setFont(slot, faceId(face)); }
};

bool toggle(uint8_t* cards, int& n, const uint8_t c) {
  for (int i = 0; i < n; ++i) {
    if (cards[i] != c) continue;
    std::memmove(cards + i, cards + i + 1, static_cast<size_t>(n - i - 1));
    --n;
    return true;
  }
  if (n >= ui::kMaxSelect) return false;
  cards[n++] = c;
  return true;
}
}  // namespace

std::unique_ptr<Activity> LoboActivity::create(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeUniqueNoThrow<LoboActivity>(renderer, mappedInput);
}

void LoboActivity::onEnter() {
  Activity::onEnter();
  toybox::ensureFonts(renderer);
  sf::ensure(renderer);
  load();
  if (hasMatch) {
    screen = match.phase == lb::Phase::HandOver ? ui::Screen::HandOver : ui::Screen::Table;
  } else {
    screen = ui::Screen::Menu;
  }
  requestUpdate();
}

void LoboActivity::onExit() {
  save();
  Activity::onExit();
}

// File, version 1:
//   line 1: L1 level target matches won
//   line 2: the match in progress, if any (LoboCore's own format)
void LoboActivity::load() {
#if defined(ARDUINO_ARCH_ESP32) || defined(SIMULATOR)
  hasMatch = false;
  if (!Storage.exists(kSavePath)) return;
  char buffer[kSaveBytes] = {};
  if (Storage.readFileToBuffer(kSavePath, buffer, sizeof(buffer) - 1) == 0) return;
  if (std::strncmp(buffer, "L1 ", 3) != 0) return;
  int v[4] = {};
  char* cursor = buffer + 3;
  for (int& x : v) {
    char* next = nullptr;
    const long value = std::strtol(cursor, &next, 10);
    if (next == cursor) return;
    x = static_cast<int>(value);
    cursor = next;
  }
  if (v[0] >= 0 && v[0] <= 2) level = static_cast<lb::Level>(v[0]);
  if (v[1] >= lb::kMinTarget && v[1] <= lb::kMaxTarget) targetPoints = v[1];
  matches = v[2] < 0 ? 0 : v[2];
  won = v[3] < 0 ? 0 : v[3];
  char* line = std::strchr(cursor, '\n');
  if (!line) return;
  lb::Match m;
  if (lb::parse(line + 1, m) && m.phase != lb::Phase::MatchOver) {
    match = m;
    hasMatch = true;
  }
#endif
}

void LoboActivity::save() {
#if defined(ARDUINO_ARCH_ESP32) || defined(SIMULATOR)
  char buffer[kSaveBytes];
  int used = std::snprintf(buffer, sizeof(buffer), "L1 %d %d %d %d\n", static_cast<int>(level), targetPoints, matches, won);
  if (used <= 0 || used >= static_cast<int>(sizeof(buffer))) return;
  if (hasMatch) {
    const int n = lb::serialize(match, buffer + used, static_cast<int>(sizeof(buffer)) - used - 2);
    if (n < 0) {
      LOG_ERR("LOBO", "Match did not fit %d bytes", kSaveBytes);
    } else {
      used += n;
      buffer[used++] = '\n';
      buffer[used] = '\0';
    }
  }
  Storage.writeFile(kSavePath, String(buffer));
#endif
}

void LoboActivity::goTo(const ui::Screen next) {
  screen = next;
  requestUpdate();
}

void LoboActivity::startMatch() {
  lb::start(match, toybox::seed(), level, targetPoints);
  hasMatch = true;
  recorded = false;
  sel = ui::Selection{};
  save();
  goTo(ui::Screen::Table);
}

void LoboActivity::tapCard(const int code) {
  if (match.phase != lb::Phase::Play) return;
  if (code > 0) {
    toggle(sel.p, sel.np, static_cast<uint8_t>(code - 1));
  } else {
    toggle(sel.w, sel.nw, static_cast<uint8_t>(-code - 1));
  }
  requestUpdate();
}

void LoboActivity::commit() {
  if (!lb::play(match, sel.p, sel.np, sel.w, sel.nw)) return;
  sel = ui::Selection{};
  afterHand();
}

// After a move or a fold: the table again, or the end of the hand or match.
void LoboActivity::afterHand() {
  if (match.phase == lb::Phase::MatchOver) {
    if (!recorded) {
      recorded = true;
      ++matches;
      if (lb::youWonMatch(match)) ++won;
    }
    hasMatch = false;
    save();
    goTo(ui::Screen::MatchOver);
    return;
  }
  save();
  goTo(match.phase == lb::Phase::HandOver ? ui::Screen::HandOver : ui::Screen::Table);
}

uint32_t LoboActivity::surfaceMeaning() const {
  uint32_t m = paintclock::mixMeaning(paintclock::kMeaningSeed, static_cast<uint32_t>(screen));
  // How many cards each side holds sets the columns a pixel falls in; which
  // cards they are, and what is picked, is left out (see Activity.h).
  m = paintclock::mixMeaning(m, match.wolfLen);
  m = paintclock::mixMeaning(m, match.youLen);
  return paintclock::mixMeaning(m, static_cast<uint32_t>(match.phase));
}

void LoboActivity::loop() {
  namespace fui = freeink::ui;

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    switch (screen) {
      case ui::Screen::Menu:
        save();
        shelf::leave(renderer, mappedInput);
        return;
      case ui::Screen::HowTo:
        if (howToPage > 0) {
          --howToPage;
          requestUpdate();
        } else {
          goTo(howToReturn);
        }
        return;
      case ui::Screen::Tally:
        goTo(hasMatch ? ui::Screen::Table : ui::Screen::Menu);
        return;
      case ui::Screen::Table:
        if (sel.np || sel.nw) {
          // First Back drops the pick, the next leaves the table.
          sel = ui::Selection{};
          requestUpdate();
          return;
        }
        save();
        goTo(ui::Screen::Menu);
        return;
      default:
        save();
        goTo(ui::Screen::Menu);
        return;
    }
  }

  fui::InputSnapshot input;
  int tapX = 0;
  int tapY = 0;
  if (!mappedInput.wasScreenTapped(tapX, tapY)) return;
  input.touchReleased = true;
  input.touchX = static_cast<int16_t>(tapX);
  input.touchY = static_cast<int16_t>(tapY);
  if (!interactionsReady) return;

  const fui::ActionEvent event = interactions.route(input);
  switch (event.action) {
    case ui::ActionMenu:
      save();
      goTo(ui::Screen::Menu);
      return;
    case ui::ActionMenuRow:
      switch (static_cast<ui::MenuRow>(event.value)) {
        case ui::MenuRow::Continue:
          if (hasMatch) goTo(match.phase == lb::Phase::HandOver ? ui::Screen::HandOver : ui::Screen::Table);
          return;
        case ui::MenuRow::NewMatch:
          if (hasMatch) {
            goTo(ui::Screen::Confirm);
          } else {
            setupCancelable = true;
            goTo(ui::Screen::Setup);
          }
          return;
        case ui::MenuRow::Deck:
          if (hasMatch) goTo(ui::Screen::Tally);
          return;
        case ui::MenuRow::HowTo:
          howToPage = 0;
          howToReturn = ui::Screen::Menu;
          goTo(ui::Screen::HowTo);
          return;
        case ui::MenuRow::Count:
          return;
      }
      return;
    case ui::ActionLevel:
      if (event.value >= 0 && event.value <= 2) level = static_cast<lb::Level>(event.value);
      save();
      requestUpdate();
      return;
    case ui::ActionTargetDown:
      if (targetPoints - kTargetStep >= lb::kMinTarget) targetPoints -= kTargetStep;
      save();
      requestUpdate();
      return;
    case ui::ActionTargetUp:
      if (targetPoints + kTargetStep <= lb::kMaxTarget) targetPoints += kTargetStep;
      save();
      requestUpdate();
      return;
    case ui::ActionDeal:
      startMatch();
      return;
    case ui::ActionSetupBack:
      goTo(ui::Screen::Menu);
      return;
    case ui::ActionFold:
      if (screen != ui::Screen::Table) return;
      sel = ui::Selection{};
      if (lb::fold(match)) afterHand();
      return;
    case ui::ActionPlay:
      if (screen == ui::Screen::Table) commit();
      return;
    case ui::ActionBackToTable:
      goTo(hasMatch ? ui::Screen::Table : ui::Screen::Menu);
      return;
    case ui::ActionNextHand:
      if (lb::nextHand(match)) {
        sel = ui::Selection{};
        save();
        goTo(ui::Screen::Table);
      }
      return;
    case ui::ActionNewMatch:
      setupCancelable = true;
      goTo(ui::Screen::Setup);
      return;
    case ui::ActionToMenu:
      goTo(ui::Screen::Menu);
      return;
    case ui::ActionHowToNext:
      if (howToPage + 1 < ui::howToPages()) {
        ++howToPage;
        requestUpdate();
      } else if (hasMatch) {
        goTo(ui::Screen::Table);
      } else {
        setupCancelable = true;
        goTo(ui::Screen::Setup);
      }
      return;
    case ui::ActionHowToBack:
      if (howToPage > 0) {
        --howToPage;
        requestUpdate();
      } else {
        goTo(howToReturn);
      }
      return;
    case ui::ActionAbandon:
      hasMatch = false;
      save();
      setupCancelable = true;
      goTo(ui::Screen::Setup);
      return;
    case ui::ActionKeepPlaying:
      goTo(ui::Screen::Table);
      return;
    default:
      break;
  }

  // Nothing routed: a tap on a card picks it or drops it; on the piles, the
  // deck's contents.
  if (screen != ui::Screen::Table || match.phase != lb::Phase::Play) return;
  if (!surfaceRevealed()) return;
  const fui::DeviceContext device = toybox::makeTarget(renderer, tableFaces()).deviceContext();
  ui::TableModel model;
  model.match = match;
  model.sel = sel;
  const int code = ui::cardAt(device, model, tapX, tapY);
  if (code != 0) {
    tapCard(code);
    return;
  }
  if (ui::pilesAt(device, tapX, tapY)) goTo(ui::Screen::Tally);
}

void LoboActivity::render(RenderLock&&) {
  namespace fui = freeink::ui;

  renderer.clearScreen();
  fui::GfxRendererTarget target = toybox::makeTarget(renderer, tableFaces());
  TargetFonts fonts(target);
  ui::useFonts(&fonts);
  const fui::DeviceContext device = target.deviceContext();
  const fui::InputSnapshot noInput{};
  interactionsReady = false;
  toybox::Frame frame(target, device, noInput, interactions);
  toybox::Screen surface(frame);

  switch (screen) {
    case ui::Screen::Menu: {
      ui::MenuModel model;
      model.canContinue = hasMatch;
      model.youScore = match.youScore;
      model.wolfScore = match.wolfScore;
      model.target = match.target;
      model.matches = matches;
      model.won = won;
      ui::buildMenu(surface, model);
      break;
    }
    case ui::Screen::Setup: {
      ui::SetupModel model;
      model.level = level;
      model.target = targetPoints;
      model.cancelable = setupCancelable;
      ui::buildSetup(surface, model);
      break;
    }
    case ui::Screen::Table: {
      ui::TableModel model;
      model.match = match;
      model.sel = sel;
      ui::buildTable(surface, model);
      break;
    }
    case ui::Screen::Tally:
      ui::buildTally(surface, match);
      break;
    case ui::Screen::HandOver:
      ui::buildHandOver(surface, match);
      break;
    case ui::Screen::MatchOver:
      ui::buildMatchOver(surface, match);
      break;
    case ui::Screen::HowTo: {
      ui::HowToModel model;
      model.page = howToPage;
      ui::buildHowTo(surface, model);
      break;
    }
    case ui::Screen::Confirm:
      ui::buildConfirm(surface);
      break;
  }

  ui::useFonts(nullptr);
  interactionsReady = true;
  toybox::reportOverflow(interactions, "Lobo");

  const auto labels = mappedInput.mapLabels("Back", "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
