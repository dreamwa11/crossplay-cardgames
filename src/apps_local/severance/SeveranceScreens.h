#pragma once

// Severance on screen: freestanding builders over the Game.
//
// The board follows the phone version top to bottom: the four employees with
// an innie and an outie slot each, the grid of four zones (two by two stacks
// each), a status line, then the draw pile, the drawn cards, the totals of the
// selection and the buttons. Nothing moves on e-ink, so a special figure's
// moment is a panel over the tray with Continue, and the log keeps the rest.
//
// The grid, the employee slots, the hand and the draw pile are hit-tested by
// the geometry here (there are more of them than the interaction table holds);
// buttons register as usual.

#include "../ui/ToyboxScreen.h"
#include "SeveranceCore.h"

namespace sevui {

namespace fui = freeink::ui;

enum : fui::ActionId {
  ActionMenu = 1,
  ActionLog = 2,
  ActionRefine = 3,
  ActionClear = 4,
  ActionConfirm = 5,
  ActionSkip = 6,
  ActionContinue = 7,
  ActionMenuRow = 8,
  ActionBack = 9,
  ActionRulesNext = 10,
  ActionRulesBack = 11,
  ActionNewQuarter = 12,
  ActionViewGrid = 13,
};

enum class Screen : uint8_t { Menu = 0, Board, Log, Rules };
enum class MenuRow : int { Continue = 0, NewQuarter, Rules, Lenient, Count };

constexpr int kMaxSel = 24;

// What the board shows on top of the game.
struct BoardModel {
  const sev::Game* game = nullptr;
  const uint8_t* sel = nullptr;  // selected card ids, or stack numbers while Reghabi searches
  int nSel = 0;
  const char* note = nullptr;   // a hint after a tap, in place of the status line
  int annIndex = -1;            // an announcement being shown, or -1
  bool showEnd = false;         // the quarter's result panel
};

struct MenuModel {
  bool canContinue = false;
  bool lenient = false;
  int played = 0;
  int won = 0;
};

// The six Rubik cuts, smallest first.
enum class Face : uint8_t { Tile = 0, Button, Ui, Display, Large, Huge };
struct FontBinder {
  virtual ~FontBinder() = default;
  virtual void bind(fui::FontId slot, Face face) = 0;
};
void useFonts(FontBinder* binder);

// Hit tests, against the geometry the board is drawn with.
enum class HitKind : uint8_t { None = 0, Stack, Mini, Hand, Draw };
struct Hit {
  HitKind kind = HitKind::None;
  int index = 0;  // stack, hand slot, or employee * 2 + (outie ? 1 : 0)
};
Hit hitTest(const sev::Game& g, int x, int y);

void buildBoard(toybox::Screen& screen, const BoardModel& model);
void buildMenu(toybox::Screen& screen, const MenuModel& model);
void buildLog(toybox::Screen& screen, const sev::Game& g);
void buildRules(toybox::Screen& screen, int page);
int rulesPages();

}  // namespace sevui
