#pragma once

// Lobo's rules. Freestanding C++17: no renderer, no storage, no clock.
//
// A port of Fotis's phone version, which follows James Ernest's solitaire
// (Crab Fragment Labs, 2022):
//
//  - 50 cards, 1 to 10 in five suits; suits do not matter in play. Medium
//    takes the 10s out, Easy the 9s and 10s as well.
//  - Each hand deals four cards to the Wolf and four to you, alternately,
//    starting with the Wolf. The top card of the deck is always face up.
//  - The Wolf never acts. Every move is yours, and every move has a price:
//      Perfect  one of your cards takes a Wolf card of the same value; you
//               draw the top card.
//      Sum      several of your cards that add up exactly to one Wolf card
//               take it; you draw the top card.
//      Split    one of your cards takes several Wolf cards that add up
//               exactly to it; the Wolf draws the top card.
//      Over     one of your cards takes one smaller Wolf card; the Wolf draws
//               one card per point of difference. Its hand has no limit.
//    The cards taken, yours and the Wolf's, go to the played pile first, so a
//    deck that runs dry mid-hand is refilled by shuffling the played pile in.
//  - Empty the Wolf's hand (after the draw your move earns) and you score the
//    values left in your hand. Fold, and the Wolf scores the values left in
//    its hand. First to the target (100 by default) wins the match.
//  - Between hands the leftovers go to the played pile; the deck is only
//    reshuffled every third hand, or when fewer than nine cards are left.

#include <cstdint>

namespace lobo {

constexpr uint8_t kNoCard = 0xFF;
constexpr int kCards = 50;
constexpr int kHandSize = 4;
constexpr int kReshuffleEvery = 3;
constexpr int kMinTarget = 30;
constexpr int kMaxTarget = 300;
constexpr int kDefaultTarget = 100;
// A move uses at most this many cards from either side (every value is at
// least 1 and no card is above 10).
constexpr int kMaxMoveCards = 10;

// A card is its id 0..49: suit * 10 + value - 1.
inline int valueOf(const uint8_t card) { return card % 10 + 1; }
inline int suitOf(const uint8_t card) { return card / 10; }

enum class Level : uint8_t { Hard = 0, Medium, Easy };
inline int maxValue(const Level level) { return level == Level::Easy ? 8 : level == Level::Medium ? 9 : 10; }

enum class Move : uint8_t { None = 0, Perfect, Sum, Split, Over };
enum class Phase : uint8_t { Play = 0, HandOver, MatchOver };

struct Match {
  // The deck's top card is deck[deckLen - 1], and it is face up.
  uint8_t deck[kCards];
  uint8_t deckLen;
  uint8_t played[kCards];
  uint8_t playedLen;
  uint8_t wolf[kCards];
  uint8_t wolfLen;
  uint8_t you[kCards];
  uint8_t youLen;

  int16_t youScore;
  int16_t wolfScore;
  int16_t target;
  uint16_t hand;   // hands dealt so far, the current one included
  uint8_t since;   // hands dealt from the deck since its last full reshuffle
  Level level;
  Phase phase;
  uint32_t rng;

  // What the last move did, for the message line.
  Move last;
  uint8_t lastP[kMaxMoveCards];
  uint8_t lastPLen;
  uint8_t lastW[kMaxMoveCards];
  uint8_t lastWLen;
  uint8_t drew;     // cards drawn as its price
  bool drewYou;     // true: you drew; false: the Wolf did
  bool reshuffled;  // the played pile went back into the deck on the way
  uint64_t fresh;   // one bit per card id: arrived with the last move or deal

  // The end of a hand.
  bool youTookHand;
  int16_t handPoints;
};

// A fresh match: shuffles a deck for `level` from `seed` and deals hand 1.
void start(Match& match, uint32_t seed, Level level = Level::Hard, int target = kDefaultTarget);

// Deals the next hand (only from Phase::HandOver).
bool nextHand(Match& match);

int sum(const uint8_t* cards, int count);

struct Verdict {
  bool ok;
  Move move;
  int over;  // for Over: the Wolf's draw
};
// What playing your cards `p` against the Wolf's cards `w` would be.
Verdict classify(const Match& match, const uint8_t* p, int np, const uint8_t* w, int nw);

// Whether a selection could still grow into a legal move. With nothing
// selected: whether you have any move at all.
bool extendable(const Match& match, const uint8_t* p, int np, const uint8_t* w, int nw);
inline bool anyMove(const Match& match) { return extendable(match, nullptr, 0, nullptr, 0); }

// Plays a legal move and pays its price. Returns false and changes nothing
// for an illegal one.
bool play(Match& match, const uint8_t* p, int np, const uint8_t* w, int nw);

// Ends the hand: the Wolf scores what is left in its hand.
bool fold(Match& match);

// Whether you won the match (meaningful in Phase::MatchOver).
bool youWonMatch(const Match& match);

// Cards of each value still in the deck, values 1..10 at [1..10].
void tally(const Match& match, int counts[11]);
// The hand after which the deck will next be reshuffled in full.
int reshuffleAfter(const Match& match);

// Text round trip for the save file: space-separated integers.
int serialize(const Match& match, char* buffer, int size);
bool parse(const char* text, Match& out);

}  // namespace lobo
