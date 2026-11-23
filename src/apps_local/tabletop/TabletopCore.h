#pragma once

// Tabletop: a deck of playing cards, a tarot deck and a bag of dice. No rules,
// no screen: the state and the randomness, freestanding so host-tests can run
// it. TabletopScreens draws it, TabletopActivity saves it.
//
// Card ids:
//   playing: 0..51 is suit * 13 + rank - 1 (spades, hearts, diamonds, clubs;
//            ace = 1 .. king = 13), 52 and 53 are the jokers.
//   tarot:   0..21 the major arcana by number, 22..77 the minor arcana,
//            22 + suit * 14 + rank - 1 (wands, cups, swords, pentacles;
//            ace = 1 .. 10, page 11, knight 12, queen 13, king 14).

#include <cstdint>

namespace tabletop {

constexpr int kPlayingCards = 52;
constexpr int kJokers = 2;
constexpr int kTarotCards = 78;
constexpr int kMajors = 22;
constexpr int kMaxDeck = kTarotCards;
constexpr int kHistory = 5;     // earlier draws kept for the strip under the card
constexpr int kSpread = 3;      // past, present, future
constexpr int kBacks = 3;       // The Pale Motel's diamonds, chevrons and rays
constexpr int kTarotBack = 2;   // the rays are tarot's; the other two are the playing deck's

enum class Die : uint8_t { D4 = 0, D6, D8, D10, D12, D20, D100, Count };
constexpr int kDieKinds = static_cast<int>(Die::Count);
constexpr int kMaxDice = 12;
constexpr int kRollHistory = 6;

int sidesOf(Die die);

struct Rng {
  uint32_t s = 0x9E3779B9u;
  void seed(uint32_t v) { s = v ? v : 0x9E3779B9u; }
  uint32_t next() {
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    return s;
  }
  int below(int n) { return n <= 1 ? 0 : static_cast<int>(next() % static_cast<uint32_t>(n)); }
};

// A drawn card. `reversed` only ever set on tarot.
struct Drawn {
  uint8_t id = 0;
  bool reversed = false;
};

// One deck: the cards still to draw, in order (the next card is last), and
// what has been drawn, newest last.
struct Deck {
  uint8_t order[kMaxDeck] = {};
  uint8_t left = 0;
  uint8_t size = 0;  // the full deck: 52, 54 or 78
  uint8_t back = 0;  // which of kBacks the deck wears, picked at each shuffle
  Drawn current[kSpread] = {};
  uint8_t shown = 0;  // cards on the table from the last draw: 0, 1 or 3
  Drawn history[kHistory] = {};
  uint8_t historyLen = 0;
};

struct Dice {
  uint8_t pick[kMaxDice] = {};  // Die per slot
  uint8_t count = 0;
  uint8_t value[kMaxDice] = {};
  bool rolled = false;
  uint16_t totals[kRollHistory] = {};  // earlier totals, newest last
  uint8_t totalsLen = 0;
};

struct Table {
  Rng rng;
  Deck cards;
  Deck tarot;
  Dice dice;
  bool jokers = false;
  bool reversals = true;
  bool spread = false;  // tarot draws three at a time
};

// A fresh table: both decks full and shuffled, two six-sided dice.
void start(Table& t, uint32_t seed);

// Gather every card back and shuffle. The playing deck takes the jokers
// setting as it stands.
void shuffle(Table& t, Deck& deck, bool tarot);

// Draw `n` cards (1 or 3). The cards on the table move to the history strip.
// Fewer are drawn if the deck runs short; returns how many.
int draw(Table& t, Deck& deck, int n, bool tarot);

bool addDie(Dice& d, Die die);
void removeDie(Dice& d, int slot);
void clearDice(Dice& d);
void roll(Table& t);
int total(const Dice& d);

// Names for the screens.
const char* majorName(int n);        // "THE STAR"
const char* roman(int n);            // 0 -> "0", 17 -> "XVII"
const char* rankWord(int rank);      // tarot ranks 1..14: "ACE" .. "KING"
const char* rankShort(int rank);     // playing 1..13: "A", "2" .. "K"
const char* tarotSuitName(int suit); // "WANDS"
const char* playingSuitName(int suit);
// "SEVEN OF CUPS", "THE STAR", "QUEEN OF HEARTS", "JOKER"
void cardName(uint8_t id, bool tarot, char* out, int size);

inline bool isMajor(uint8_t id) { return id < kMajors; }
inline int tarotSuit(uint8_t id) { return (id - kMajors) / 14; }
inline int tarotRank(uint8_t id) { return (id - kMajors) % 14 + 1; }
inline bool isJoker(uint8_t id) { return id >= kPlayingCards; }
inline int playingSuit(uint8_t id) { return id / 13; }
inline int playingRank(uint8_t id) { return id % 13 + 1; }

// Text form for the save file. serialize returns bytes written or -1.
int serialize(const Table& t, char* out, int size);
bool parse(const char* in, Table& t);

}  // namespace tabletop
