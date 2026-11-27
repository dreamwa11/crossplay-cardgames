#pragma once

// The oracle: yes/no questions at five odds, and generators for a spark, a
// detail, a person, a place, a danger, a chance, a twist, a clue and a fight
// move. The tables are Ironsworn's, Delve's and Starforged's (OracleTables.h);
// what this adds is the reading: each result comes back as a short sentence
// rather than a row number to look up.
//
// A reading stores only its dice, so the log is small and the words are made
// again from the tables whenever it is drawn.

#include <cstdint>

namespace tabletop {

struct Rng;

enum class Odds : uint8_t { Small = 0, Unlikely, Even, Likely, Sure, Count };
constexpr int kOddsKinds = static_cast<int>(Odds::Count);

// Which books the tables come from: Ironsworn and Delve, or Starforged.
enum class Genre : uint8_t { Fantasy = 0, SciFi, Count };
constexpr int kGenres = static_cast<int>(Genre::Count);

enum class Kind : uint8_t { Ask = 0, Spark, Detail, Person, Place, Danger, Chance, Twist, Clue, Fight, Count };
constexpr int kKinds = static_cast<int>(Kind::Count);

// How an answer lands. "and" and "but" are the far ends of each side.
enum class Answer : uint8_t { NoAnd = 0, No, NoBut, YesBut, Yes, YesAnd };

constexpr int kOracleLog = 12;
constexpr int kPicks = 8;

struct Reading {
  uint8_t genre = 0;
  uint8_t kind = 0;
  uint8_t odds = 0;  // Ask only
  uint8_t roll = 0;  // Ask: the d100, 1..100
  uint8_t pick[kPicks] = {};  // row per table, which tables depends on kind
};

struct Oracle {
  Odds odds = Odds::Even;
  Genre genre = Genre::Fantasy;
  Reading log[kOracleLog] = {};  // newest last
  uint8_t len = 0;
};

int chanceOf(Odds odds);           // 10, 25, 50, 75, 90
const char* oddsName(Odds odds);   // "Small chance" .. "Almost certain"
const char* oddsShort(Odds odds);  // "SMALL" .. "SURE", for chips
const char* kindName(Kind kind);   // "SPARK", for chips and titles
const char* genreName(Genre genre);  // "FANTASY", "SCI-FI"

// Roll a reading of `kind` (Ask uses the oracle's odds) from the oracle's
// genre and log it. A reading keeps its genre, so the log reads the same after
// a switch.
const Reading& consult(Oracle& o, Rng& rng, Kind kind);

Answer answerOf(const Reading& r);
// Whether the title is made from the tables (a name, a place) rather than
// being the kind's own name or the answer.
bool named(const Reading& r);
// A d100 of doubles (11, 22 .. 99) brings something else into the scene.
bool hasEvent(const Reading& r);

// The words. title is the big line ("YES, BUT...", "SCHEME / ALLY",
// "Solana"), body the sentence under it, brief one line for the log.
void title(const Reading& r, char* out, int size);
void body(const Reading& r, char* out, int size);
void brief(const Reading& r, char* out, int size);
// The dice behind an answer: "50/50, rolled 64". Empty for the generators.
void meta(const Reading& r, char* out, int size);

int serializeOracle(const Oracle& o, char* out, int size);
bool parseOracle(const char*& p, Oracle& o);

}  // namespace tabletop
