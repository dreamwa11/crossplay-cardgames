#pragma once

// Lobo on screen. Freestanding builders over plain models, like Scoundrel's.
//
// The table follows the phone version: a score bar, the Wolf sitting across
// the table in a beamed cellar with his paws on the edge, his row of cards and
// yours below it (yours re-sorted so equal values sit under each other, joined
// by a dotted thread), the deck and the played pile on the left, the message
// bar, then FOLD and the move button. Nothing moves on e-ink, so the message
// bar says what the last move did and newly arrived cards carry a mark.

#include "../ui/ToyboxScreen.h"
#include "LoboCore.h"

namespace loboui {

namespace fui = freeink::ui;

enum : fui::ActionId {
  ActionMenu = 1,
  ActionFold = 2,
  ActionPlay = 3,
  ActionDeck = 4,  // the deck or the played pile: what is left
  ActionMenuRow = 5,
  ActionLevel = 6,       // value = Level
  ActionTargetDown = 7,
  ActionTargetUp = 8,
  ActionDeal = 9,        // start the match from the setup sheet
  ActionSetupBack = 10,
  ActionNextHand = 11,
  ActionNewMatch = 12,   // from the match's end
  ActionToMenu = 13,
  ActionBackToTable = 14,
  ActionHowToNext = 15,
  ActionHowToBack = 16,
  ActionAbandon = 17,
  ActionKeepPlaying = 18,
};

enum class Screen : uint8_t { Menu = 0, Setup, Table, Tally, HandOver, MatchOver, HowTo, Confirm };

enum class MenuRow : int { Continue = 0, NewMatch, Deck, HowTo, Count };

// Up to this many cards selected on either side.
constexpr int kMaxSelect = lobo::kMaxMoveCards;

struct Selection {
  uint8_t p[kMaxSelect];
  int np = 0;
  uint8_t w[kMaxSelect];
  int nw = 0;
};

struct TableModel {
  lobo::Match match{};
  Selection sel{};
  bool hints = true;
};

struct MenuModel {
  bool canContinue = false;
  int youScore = 0;
  int wolfScore = 0;
  int target = lobo::kDefaultTarget;
  int matches = 0;
  int won = 0;
};

struct SetupModel {
  lobo::Level level = lobo::Level::Hard;
  int target = lobo::kDefaultTarget;
  bool cancelable = false;
};

struct HowToModel {
  int page = 0;
};

// Text faces: Scoundrel's cuts (Space Grotesk, Scoundrel Gothic, Astloch).
enum class Face : uint8_t { Small = 0, Body, Large, Value, ValueBig, Logo, Title };

struct FontBinder {
  virtual ~FontBinder() = default;
  virtual void bind(fui::FontId slot, Face face) = 0;
};
void useFonts(FontBinder* binder);

// The card under a point on the table: +1..+50 for one of yours (card id + 1),
// -1..-50 for one of the Wolf's, 0 for none. Uses the geometry that drew them.
int cardAt(const fui::DeviceContext& device, const TableModel& model, int x, int y);
// The deck and played pile, also hit-tested by geometry.
bool pilesAt(const fui::DeviceContext& device, int x, int y);

// The message bar's words for the table as it stands.
void messageText(const TableModel& model, char* out, int size);

void buildTable(toybox::Screen& screen, const TableModel& model);
void buildMenu(toybox::Screen& screen, const MenuModel& model);
void buildSetup(toybox::Screen& screen, const SetupModel& model);
void buildTally(toybox::Screen& screen, const lobo::Match& match);
void buildHandOver(toybox::Screen& screen, const lobo::Match& match);
void buildMatchOver(toybox::Screen& screen, const lobo::Match& match);
void buildHowTo(toybox::Screen& screen, const HowToModel& model);
void buildConfirm(toybox::Screen& screen);

int howToPages();

}  // namespace loboui
