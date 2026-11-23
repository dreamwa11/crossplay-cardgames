#pragma once

// Severance: The Card Game (Version 2 rules), designed by u/DREAMWALL. The
// rules, freestanding: no screen, no storage, so host-tests can run whole
// quarters. A line-for-line port of the phone version's engine, so the two
// play the same game; where the phone version animates, this one leaves an
// announcement for the screen to show and a line in the log.
//
// Cards are indexes into a fixed table of 81, built in the same order every
// game (see cardDef); only their state (value, colour, face, keep) changes.

#include <cstdint>

namespace sev {

constexpr int kCards = 81;
constexpr int kStacks = 16;
constexpr int kStackCap = 32;
constexpr int kChars = 4;  // Mark S., Helly R., Dylan G., Irving B.
constexpr int kLogLines = 24;
constexpr int kLogLen = 300;
constexpr int kAnnMax = 8;
constexpr int kNone = -1;

enum class Type : uint8_t { Num = 0, Main, Special };
enum class Fx : uint8_t { None = 0, Kier, Dieter, Casey, Drummond, Graner, Milchick, Cobel, Reghabi, Jame };

// The fixed part of a card.
struct CardDef {
  Type type;
  int8_t val;   // numbers: 1..10, the Goat 0 until revealed
  bool red;     // numbers: green (true) or blue; employees: innie (true) or outie
  bool wild;    // Emile the Goat
  bool adm;     // belongs to Administration (the draw pile)
  uint8_t ch;   // employees: 0..3
  Fx fx;        // specials
  bool gemma;   // the Casey card that lives in the pile
};
CardDef cardDef(int id);

struct CardState {
  int8_t val;
  bool red;
  bool faceUp;
  bool keep;  // drawn by Dieter: stays in hand until refined
};

struct List {
  uint8_t c[kCards];
  uint8_t n = 0;
  void push(int id) { c[n++] = static_cast<uint8_t>(id); }
  int pop() { return c[--n]; }
  bool has(int id) const;
  bool remove(int id);
  void insertFront(int id);
};

struct Stack {
  uint8_t c[kStackCap];
  uint8_t n = 0;
  int top() const { return n ? c[n - 1] : kNone; }
};

struct Pair {
  uint8_t ch;
  uint8_t cards[2];
  uint8_t n;
};

enum class Pend : uint8_t { None = 0, Kier, CaseyMain, CaseyNum, Reghabi };
struct Pending {
  Pend type = Pend::None;
  uint8_t max = 0;
  uint8_t who = 0;  // the Casey or Gemma card, for her session
};

enum class Kind : uint8_t { Plain = 0, Good, Bad };
struct LogLine {
  uint16_t turn;
  Kind kind;
  char text[kLogLen];
};

// A special figure's moment: shown with Continue before the board.
struct Ann {
  uint8_t card;
  Kind kind;
  char title[48];
  char text[kLogLen];
};

struct Rng {
  uint32_t s = 0x2545F491u;
  void seed(uint32_t v) { s = v ? v : 0x2545F491u; }
  uint32_t next() {
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    return s;
  }
  // Always draws, even for one choice: the phone version does, and the two
  // stay in step that way (host-tests/severance replays both).
  int below(int n) {
    const uint32_t v = next();
    return n <= 1 ? 0 : static_cast<int>(v % static_cast<uint32_t>(n));
  }
};

enum class Over : uint8_t { No = 0, Win, Lose };

struct Game {
  CardState card[kCards];
  Stack stacks[kStacks];
  bool unlocked[4];
  List draw, hand, waiting, gone, lastPair;
  bool hasLastPair = false;
  Pair paired[kChars];
  uint8_t nPaired = 0;
  int8_t blocked = kNone;
  int8_t lockZone = kNone;
  bool hellyPause = false, hallPending = false, hallPass = false, defianceUsed = false, reshuffled = false;
  bool seenMilchick = false, seenCobel = false;
  Pending pending;
  uint8_t queue[8];  // draws owed: 1 and 2 are Dieter's
  uint8_t nQueue = 0;
  Over over = Over::No;
  uint16_t turn = 0;
  bool dealing = false;
  bool lenient = false;  // Easier Milchick
  Rng rng;
  // Shown, not rules: the log and this action's announcements.
  LogLine log[kLogLines];
  uint8_t nLog = 0;
  Ann ann[kAnnMax];
  uint8_t nAnn = 0;
};

const char* charName(int ch);         // "Mark S."
const char* cardName(int id);         // "Helena", "Ms. Casey", "Gemma"
const char* ability(int id);          // the special's tag: "PROVIDES"
bool isBad(int id);                   // the specials that hurt
void label(const Game& g, int id, char* out, int size);  // "7 green", "Mark S. (innie)"

inline int zoneOf(int stack) { return stack / 4; }

void newGame(Game& g, uint32_t seed, bool lenient);

// Queries for the screens.
int topOf(const Game& g, int stack);
bool inLockedZone(const Game& g, int stack);
int usableNums(const Game& g, uint8_t* out);  // grid numbers in use, then the playable hand
// Drawn number cards stay in hand: Dieter's are all playable, the rest form a
// pile whose top card is playable.
int playableHand(const Game& g, uint8_t* out);
int wasteDepth(const Game& g);
bool isUsable(const Game& g, int id);
bool hasValidPair(const Game& g);
bool canDraw(const Game& g);
bool isPaired(const Game& g, int ch);
int pairable(const Game& g, uint8_t* chars);
int mainCards(const Game& g, int ch, uint8_t* out);
bool mateWaiting(const Game& g, int ch);
bool isPairCard(const Game& g, int id);  // a revealed employee whose pair can be made now
struct Totals {
  int green, blue;
  bool ok;
};
Totals selectionTotals(const Game& g, const uint8_t* ids, int n);
// The priority rule: an identical card in an earlier position must be used.
// Rewrites ids in place; returns true if anything moved.
bool priorityFix(const Game& g, uint8_t* ids, int n);
// What a pending ability can take: card ids, or stack numbers for Reghabi.
int pendingCandidates(const Game& g, uint8_t* out);
bool stackHidesCard(const Game& g, int stack);
int findStack(const Game& g, int id);

// Player actions. Each clears the announcements and returns false if it was
// not allowed.
bool actDraw(Game& g);
bool actRefine(Game& g, const uint8_t* ids, int n);
bool actPair(Game& g, int ch);
bool actResolve(Game& g, const uint8_t* ids, int n);  // empty = Skip

// The text for a pending choice, built from its type.
void pendingText(const Game& g, char* title, int titleSize, char* text, int textSize);

int cardCount(const Game& g);

// Text form for the save file. serialize returns bytes written or -1.
int serialize(const Game& g, char* out, int size);
bool parse(const char* in, Game& g);

}  // namespace sev
