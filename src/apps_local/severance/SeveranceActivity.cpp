#include "SeveranceActivity.h"

#include <HalStorage.h>
#include <Logging.h>
#include <Memory.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "../Shelf.h"
#include "../ui/Toybox.h"
#include "../ui/ToyboxFonts.h"
#include "../ui/ToyboxSeed.h"
#include "../ui/ToyboxTheme.h"

namespace ui = sevui;

namespace {
#if defined(ARDUINO_ARCH_ESP32) || defined(SIMULATOR)
constexpr char kSavePath[] = "/.crosspoint/severance.sav";
#endif
// The game (cards, stacks, piles) is about a kilobyte as text; the log adds
// up to 24 lines of up to 300 characters.
constexpr int kSaveBytes = 12 * 1024;

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

std::unique_ptr<Activity> SeveranceActivity::create(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeUniqueNoThrow<SeveranceActivity>(renderer, mappedInput);
}

void SeveranceActivity::onEnter() {
  Activity::onEnter();
  toybox::ensureFonts(renderer);
  game = makeUniqueNoThrow<sev::Game>();
  if (!game) {
    LOG_ERR("SEV", "OOM: game");
    shelf::leave(renderer, mappedInput);
    return;
  }
  load();
  screen = ui::Screen::Menu;
  requestUpdate();
}

void SeveranceActivity::onExit() {
  save();
  game.reset();
  Activity::onExit();
}

// File, version 1:
//   line 1: V1 lenient played won hasGame
//   then the game in SeveranceCore's own format
void SeveranceActivity::load() {
#if defined(ARDUINO_ARCH_ESP32) || defined(SIMULATOR)
  hasGame = false;
  if (!Storage.exists(kSavePath)) return;
  auto buffer = makeUniqueNoThrow<char[]>(kSaveBytes);
  if (!buffer) return;
  std::memset(buffer.get(), 0, kSaveBytes);
  if (Storage.readFileToBuffer(kSavePath, buffer.get(), kSaveBytes - 1) == 0) return;
  int v[4] = {};
  if (std::sscanf(buffer.get(), "V1 %d %d %d %d", &v[0], &v[1], &v[2], &v[3]) != 4) return;
  lenient = v[0] != 0;
  played = v[1] < 0 ? 0 : v[1];
  won = v[2] < 0 ? 0 : v[2];
  const char* line = std::strchr(buffer.get(), '\n');
  if (v[3] && line && sev::parse(line + 1, *game)) hasGame = game->over == sev::Over::No;
#endif
}

void SeveranceActivity::save() {
#if defined(ARDUINO_ARCH_ESP32) || defined(SIMULATOR)
  if (!game) return;
  auto buffer = makeUniqueNoThrow<char[]>(kSaveBytes);
  if (!buffer) {
    LOG_ERR("SEV", "OOM: save buffer");
    return;
  }
  int used = std::snprintf(buffer.get(), kSaveBytes, "V1 %d %d %d %d\n", lenient ? 1 : 0, played, won, hasGame ? 1 : 0);
  if (hasGame) {
    const int n = sev::serialize(*game, buffer.get() + used, kSaveBytes - used);
    if (n < 0) {
      LOG_ERR("SEV", "Game did not fit %d bytes", kSaveBytes);
      buffer[used] = '\0';
    }
  }
  Storage.writeFile(kSavePath, String(buffer.get()));
#endif
}

void SeveranceActivity::goTo(const ui::Screen next) {
  screen = next;
  requestUpdate();
}

void SeveranceActivity::setNote(const char* text) { std::snprintf(note, sizeof(note), "%s", text); }

void SeveranceActivity::newQuarter() {
  sev::newGame(*game, toybox::seed(), lenient);
  hasGame = true;
  nSel = 0;
  note[0] = '\0';
  annIndex = -1;
  showEnd = false;
  save();
  goTo(ui::Screen::Board);
}

bool SeveranceActivity::toggle(const int v, const int max) {
  for (int k = 0; k < nSel; ++k) {
    if (sel[k] != v) continue;
    std::memmove(sel + k, sel + k + 1, static_cast<size_t>(nSel - k - 1));
    --nSel;
    return false;
  }
  if (nSel >= max) {
    std::memmove(sel, sel + 1, static_cast<size_t>(nSel - 1));
    --nSel;
  }
  if (nSel < ui::kMaxSel) sel[nSel++] = static_cast<uint8_t>(v);
  return true;
}

int SeveranceActivity::selectedMain() const {
  if (!nSel || sev::cardDef(sel[0]).type != sev::Type::Main) return sev::kNone;
  return sel[0];
}

// Drop anything the game no longer lets you pick, as the phone version does
// before every paint.
void SeveranceActivity::pruneSelection() {
  const sev::Game& g = *game;
  uint8_t ok[sev::kCards];
  int nOk = 0;
  if (g.pending.type != sev::Pend::None) {
    nOk = sev::pendingCandidates(g, ok);
  } else {
    nOk = sev::usableNums(g, ok);
  }
  int m = 0;
  for (int k = 0; k < nSel; ++k) {
    bool keep = g.pending.type == sev::Pend::None && sev::isPairCard(g, sel[k]);
    for (int q = 0; q < nOk && !keep; ++q) keep = ok[q] == sel[k];
    if (keep) sel[m++] = sel[k];
  }
  nSel = m;
}

void SeveranceActivity::afterAction() {
  sev::Game& g = *game;
  nSel = 0;
  note[0] = '\0';
  annIndex = g.nAnn ? 0 : -1;
  if (g.over != sev::Over::No && hasGame) {
    hasGame = false;
    ++played;
    if (g.over == sev::Over::Win) ++won;
    if (annIndex < 0) showEnd = true;
  }
  save();
  requestUpdate();
}

void SeveranceActivity::tapCard(const int id, const bool mini) {
  sev::Game& g = *game;
  note[0] = '\0';
  if (g.over != sev::Over::No) return;
  if (g.pending.type != sev::Pend::None) {
    uint8_t cand[sev::kCards];
    const int n = sev::pendingCandidates(g, cand);
    bool ok = false;
    for (int k = 0; k < n; ++k) ok |= cand[k] == id;
    if (!ok) {
      setNote("That card cannot be chosen for this ability. Pick a highlighted card.");
    } else {
      toggle(id, g.pending.max);
    }
    requestUpdate();
    return;
  }
  const sev::CardDef d = sev::cardDef(id);
  const bool usable = sev::isUsable(g, id);
  const bool isMain = d.type == sev::Type::Main;
  const bool pairCard = sev::isPairCard(g, id);
  const int stack = sev::findStack(g, id);
  if (id == g.blocked) {
    setNote("Cobel is blocking this card until you refine another pair.");
  } else if (stack != sev::kNone && sev::zoneOf(stack) == g.lockZone) {
    setNote("Drummond has locked this zone until your next refined pair.");
  } else if (g.hellyPause && isMain && d.ch == 1 && !sev::isPaired(g, 1)) {
    setNote("Jame Eagan has paused Helly until your next refined pair.");
  } else if (usable && d.wild) {
    if (selectedMain() != sev::kNone) nSel = 0;
    bool in = false;
    for (int k = 0; k < nSel; ++k) in |= sel[k] == id;
    if (!in) {
      g.card[id].red = true;
      toggle(id, ui::kMaxSel);
      setNote("The Goat counts as round. Tap again for square.");
    } else if (g.card[id].red) {
      g.card[id].red = false;
      setNote("The Goat counts as square. Tap again to deselect.");
    } else {
      toggle(id, ui::kMaxSel);
    }
  } else if (pairCard && sev::mateWaiting(g, d.ch)) {
    nSel = 0;
    if (sev::actPair(g, d.ch)) afterAction();
    return;
  } else if (pairCard) {
    const int cur = selectedMain();
    if (cur == sev::kNone || sev::cardDef(cur).ch != d.ch) nSel = 0;
    toggle(id, ui::kMaxSel);
    if (nSel == 2) {
      nSel = 0;
      if (sev::actPair(g, d.ch)) afterAction();
      return;
    }
  } else if (isMain && sev::isPaired(g, d.ch) && !usable) {
    setNote("This pair is complete.");
  } else if (!usable) {
    setNote(mini ? "This card waits here for its other half." : "This character cannot be paired yet.");
  } else {
    if (selectedMain() != sev::kNone) nSel = 0;
    toggle(id, ui::kMaxSel);
  }
  requestUpdate();
}

void SeveranceActivity::tapBoard(const int x, const int y) {
  sev::Game& g = *game;
  const ui::Hit hit = ui::hitTest(g, x, y);
  switch (hit.kind) {
    case ui::HitKind::None:
      return;
    case ui::HitKind::Draw:
      if (g.over != sev::Over::No) return;
      note[0] = '\0';
      if (g.pending.type != sev::Pend::None) {
        setNote("Finish the current ability first: Confirm or Skip.");
      } else if (!g.draw.n) {
        setNote("The draw pile is empty.");
      } else {
        nSel = 0;
        if (sev::actDraw(g)) afterAction();
        return;
      }
      requestUpdate();
      return;
    case ui::HitKind::Stack: {
      if (g.pending.type == sev::Pend::Reghabi && g.over == sev::Over::No) {
        note[0] = '\0';
        if (!sev::stackHidesCard(g, hit.index)) {
          setNote("That stack hides no cards. Pick a highlighted stack.");
        } else {
          toggle(hit.index, g.pending.max);
        }
        requestUpdate();
        return;
      }
      const int top = sev::topOf(g, hit.index);
      if (top != sev::kNone && g.card[top].faceUp) tapCard(top, false);
      return;
    }
    case ui::HitKind::Hand:
      if (hit.index < g.hand.n) tapCard(g.hand.c[hit.index], false);
      return;
    case ui::HitKind::Mini: {
      const int ch = hit.index / 2;
      const bool innie = hit.index % 2 == 0;
      for (int k = 0; k < g.waiting.n; ++k) {
        const int id = g.waiting.c[k];
        if (sev::cardDef(id).ch == ch && sev::cardDef(id).red == innie) {
          tapCard(id, true);
          return;
        }
      }
      if (sev::isPaired(g, ch)) {
        setNote("This pair is complete.");
        requestUpdate();
      }
      return;
    }
  }
}

uint32_t SeveranceActivity::surfaceMeaning() const {
  uint32_t m = paintclock::mixMeaning(paintclock::kMeaningSeed, static_cast<uint32_t>(screen));
  if (!game) return m;
  // Where each stack's top card sits is fixed; what changes the board's
  // tappable shape is the hand, the panels and Reghabi's stack picking.
  m = paintclock::mixMeaning(m, game->hand.n);
  m = paintclock::mixMeaning(m, static_cast<uint32_t>(game->pending.type));
  m = paintclock::mixMeaning(m, static_cast<uint32_t>(annIndex + 1));
  return paintclock::mixMeaning(m, showEnd ? 1u : 0u);
}

void SeveranceActivity::loop() {
  namespace fui = freeink::ui;
  if (!game) return;

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    switch (screen) {
      case ui::Screen::Menu:
        save();
        shelf::leave(renderer, mappedInput);
        return;
      case ui::Screen::Rules:
        if (rulesPage > 0) {
          --rulesPage;
          requestUpdate();
        } else {
          goTo(rulesReturn);
        }
        return;
      case ui::Screen::Log:
        goTo(ui::Screen::Board);
        return;
      case ui::Screen::Board:
        if (nSel) {
          nSel = 0;
          note[0] = '\0';
          requestUpdate();
          return;
        }
        save();
        goTo(ui::Screen::Menu);
        return;
    }
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
  sev::Game& g = *game;
  switch (event.action) {
    case ui::ActionMenu:
      save();
      goTo(ui::Screen::Menu);
      return;
    case ui::ActionLog:
      goTo(ui::Screen::Log);
      return;
    case ui::ActionBack:
      goTo(ui::Screen::Board);
      return;
    case ui::ActionMenuRow:
      switch (static_cast<ui::MenuRow>(event.value)) {
        case ui::MenuRow::Continue:
          if (hasGame) goTo(ui::Screen::Board);
          return;
        case ui::MenuRow::NewQuarter:
          newQuarter();
          return;
        case ui::MenuRow::Rules:
          rulesPage = 0;
          rulesReturn = ui::Screen::Menu;
          goTo(ui::Screen::Rules);
          return;
        case ui::MenuRow::Lenient:
          lenient = !lenient;
          g.lenient = lenient;
          save();
          requestUpdate();
          return;
        case ui::MenuRow::Count:
          return;
      }
      return;
    case ui::ActionRulesNext:
      if (rulesPage + 1 < ui::rulesPages()) {
        ++rulesPage;
        requestUpdate();
      } else {
        goTo(rulesReturn);
      }
      return;
    case ui::ActionRulesBack:
      if (rulesPage > 0) {
        --rulesPage;
        requestUpdate();
      } else {
        goTo(rulesReturn);
      }
      return;
    case ui::ActionNewQuarter:
      newQuarter();
      return;
    case ui::ActionViewGrid:
      showEnd = false;
      requestUpdate();
      return;
    case ui::ActionContinue:
      if (annIndex >= 0 && ++annIndex >= g.nAnn) {
        annIndex = -1;
        if (g.over != sev::Over::No) showEnd = true;
      }
      requestUpdate();
      return;
    case ui::ActionClear:
      nSel = 0;
      note[0] = '\0';
      requestUpdate();
      return;
    case ui::ActionRefine: {
      if (!sev::selectionTotals(g, sel, nSel).ok) return;
      uint8_t fixed[ui::kMaxSel];
      std::memcpy(fixed, sel, nSel);
      if (sev::priorityFix(g, fixed, nSel)) {
        std::memcpy(sel, fixed, nSel);
        setNote("Priority rule: an identical card sits in an earlier position, so the selection moved to it. Press "
                "Refine again.");
        requestUpdate();
        return;
      }
      if (sev::actRefine(g, sel, nSel)) afterAction();
      return;
    }
    case ui::ActionConfirm:
      if (nSel && sev::actResolve(g, sel, nSel)) afterAction();
      return;
    case ui::ActionSkip:
      if (sev::actResolve(g, sel, 0)) afterAction();
      return;
    default:
      break;
  }
  if (screen == ui::Screen::Board && annIndex < 0 && !showEnd && surfaceRevealed()) tapBoard(tapX, tapY);
}

void SeveranceActivity::render(RenderLock&&) {
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

  if (game) {
    switch (screen) {
      case ui::Screen::Menu: {
        ui::MenuModel model;
        model.canContinue = hasGame;
        model.lenient = lenient;
        model.played = played;
        model.won = won;
        ui::buildMenu(surface, model);
        break;
      }
      case ui::Screen::Board: {
        pruneSelection();
        ui::BoardModel model;
        model.game = game.get();
        model.sel = sel;
        model.nSel = nSel;
        model.note = note[0] ? note : nullptr;
        model.annIndex = annIndex;
        model.showEnd = showEnd;
        ui::buildBoard(surface, model);
        break;
      }
      case ui::Screen::Log:
        ui::buildLog(surface, *game);
        break;
      case ui::Screen::Rules:
        ui::buildRules(surface, rulesPage);
        break;
    }
  }

  ui::useFonts(nullptr);
  interactionsReady = true;
  toybox::reportOverflow(interactions, "Severance");

  const auto labels = mappedInput.mapLabels("Back", "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
