#pragma once

// Tabletop on screen: freestanding builders over the plain Table model.
//
// Six screens. The menu is four doors. Cards and Tarot share a shape: one
// big card in the middle (or three for a tarot spread), the earlier draws in a
// strip under it, then back, shuffle and draw. Dice is a row of die kinds to
// add, the total, the tray of rolled dice (tap one to take it away), the
// earlier totals, then back, clear and roll. The oracle is the newest reading
// in a card (its genre a chip in the header), a row of odds to ask at, a grid of tables to roll on, the last two
// readings, then back, log and again; its log lists the readings newest first, in full, a page at a time.

#include "../ui/ToyboxScreen.h"
#include "TabletopCore.h"

namespace tabletopui {

namespace fui = freeink::ui;

enum : fui::ActionId {
  ActionOpen = 1,  // value = Screen
  ActionBack = 2,
  ActionDraw = 3,
  ActionShuffle = 4,
  ActionJokers = 5,
  ActionReversals = 6,
  ActionSpread = 7,  // value 0 one card, 1 three
  ActionAddDie = 8,  // value = Die
  ActionRemoveDie = 9,  // value = slot
  ActionRoll = 10,
  ActionClear = 11,
  ActionArcana = 12,  // the tarot deck: all, the majors, the minors, in turn
  ActionAsk = 13,      // value = Odds
  ActionConsult = 14,  // value = Kind
  ActionAgain = 15,    // the last kind again, at the same odds
  ActionLog = 16,
  ActionGenre = 17,    // fantasy and sci-fi, in turn
  ActionLogPage = 18,  // the log's next page, wrapping to the first
};

enum class Screen : uint8_t { Menu = 0, Cards, Tarot, Dice, Oracle, OracleLog };

// The six Rubik cuts, smallest first.
enum class Face : uint8_t { Tile = 0, Button, Ui, Display, Large, Huge };

struct FontBinder {
  virtual ~FontBinder() = default;
  virtual void bind(fui::FontId slot, Face face) = 0;
};
void useFonts(FontBinder* binder);

void buildMenu(toybox::Screen& screen, const tabletop::Table& table);
void buildCards(toybox::Screen& screen, const tabletop::Table& table);
void buildTarot(toybox::Screen& screen, const tabletop::Table& table);
void buildDice(toybox::Screen& screen, const tabletop::Table& table);
void buildOracle(toybox::Screen& screen, const tabletop::Table& table);
// `page` counts from 0 and wraps past the last.
void buildOracleLog(toybox::Screen& screen, const tabletop::Table& table, int page);

}  // namespace tabletopui
