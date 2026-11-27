#include "TabletopOracle.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "OracleTables.h"
#include "TabletopCore.h"

namespace tabletop {

namespace {

namespace ot = oracle_tables;

constexpr int kChance[kOddsKinds] = {10, 25, 50, 75, 90};
constexpr const char* kOddsName[kOddsKinds] = {"Small chance", "Unlikely", "50/50", "Likely", "Almost certain"};
constexpr const char* kOddsShort[kOddsKinds] = {"SMALL", "UNLIKELY", "50/50", "LIKELY", "SURE"};
constexpr const char* kGenreName[kGenres] = {"FANTASY", "SCI-FI", "MYSTERY"};
constexpr const char* kKindName[kKinds] = {"ASK",    "SPARK",  "DETAIL", "PERSON", "PLACE",
                                           "DANGER", "CHANCE", "TWIST",  "CLUE",   "FIGHT"};

// What a spark asks of the scene. "{v}" is the action, "{t}" the theme.
constexpr const char* kSpark[] = {
    "Someone moves to %s the %s.", "Look for a way to %s the %s.", "A chance to %s the %s.",
    "Someone wants to %s the %s.", "You must %s the %s.",
};
constexpr int kSparks = sizeof(kSpark) / sizeof(kSpark[0]);

// What a detail asks of you; the detail itself is the title.
constexpr const char* kDetailAsk[] = {
    "What is it doing here?", "Who wants it, and why?", "What does it change?", "Who left it behind?",
    "What would it cost to reach it?",
};
constexpr int kDetailAsks = sizeof(kDetailAsk) / sizeof(kDetailAsk[0]);

// The tables a genre swaps. People and places differ in shape too, so they
// are drawn genre by genre below.
struct Set {
  const ot::List& action;
  const ot::List& verb;
  const ot::List& theme;
  const ot::List& danger;
  const ot::List& chance;
  const ot::List& detailA;  // a detail is A + B: "ancient threshold", "abandoned archive"
  const ot::List& detailB;
  const ot::List& twist;
  const ot::List& clue;
  const ot::List& fight;
};
const Set kFantasy = {ot::kAction, ot::kVerb,   ot::kTheme, ot::kDanger, ot::kChance,
                      ot::kAspect, ot::kFeature, ot::kTwist, ot::kClue,   ot::kFight};
const Set kSciFi = {ot::kSfAction,   ot::kSfVerb, ot::kSfTheme, ot::kSfDanger, ot::kSfChance,
                    ot::kDescriptor, ot::kFocus,  ot::kTwist,   ot::kClue,     ot::kFight};
const Set kMystery = {ot::kMAction,  ot::kMVerb,    ot::kMTheme, ot::kMDanger, ot::kMChance,
                      ot::kMDetailA, ot::kMDetailB, ot::kMTwist, ot::kMClue,   ot::kMFight};
const Set& setOf(const uint8_t genre) {
  switch (static_cast<Genre>(genre)) {
    case Genre::SciFi:
      return kSciFi;
    case Genre::Mystery:
      return kMystery;
    default:
      return kFantasy;
  }
}

// A sci-fi place is one of three things, by its first pick (0..99).
enum class SfPlace : uint8_t { Settlement, Planet, Starship };
SfPlace sfPlace(const uint8_t which) {
  return which < 40 ? SfPlace::Settlement : which < 75 ? SfPlace::Planet : SfPlace::Starship;
}

// Ask picks: action, theme (the "and"), danger, chance (the "but"s), the
// event's action and theme, then the two sentence shapes.
enum : uint8_t { AskVerb = 0, AskTheme, AskDanger, AskChance, EventVerb, EventTheme, AskShape, EventShape };

const char* at(const ot::List& list, const uint8_t i) { return list.text[i < list.size ? i : 0]; }

// Row `i` among the rows of `group` (for the per-planet tables).
const char* inGroup(const ot::List& list, const uint8_t group, const uint8_t i) {
  int seen = 0;
  for (int r = 0; r < list.size; ++r) {
    if (list.group[r] != group) continue;
    if (seen++ == i) return list.text[r];
  }
  return list.text[0];
}

uint8_t pickInGroup(Rng& rng, const ot::List& list, const uint8_t group) {
  int n = 0;
  for (int r = 0; r < list.size; ++r) n += list.group[r] == group;
  return static_cast<uint8_t>(rng.below(n));
}

uint8_t pickFrom(Rng& rng, const ot::List& list) {
  const int die = list.cum[list.size - 1];
  const int roll = rng.below(die) + 1;
  for (int i = 0; i < list.size; ++i)
    if (roll <= list.cum[i]) return static_cast<uint8_t>(i);
  return static_cast<uint8_t>(list.size - 1);
}

// The name table a person's first pick points at: Ironlander A, B, or elf.
const ot::List& nameList(const uint8_t which) {
  return which < 45 ? ot::kNameA : which < 90 ? ot::kNameB : ot::kNameElf;
}

// Append to out, keeping it terminated.
void add(char* out, const int size, const char* fmt, const char* a = "", const char* b = "", const char* c = "") {
  const int used = static_cast<int>(std::strlen(out));
  if (used >= size - 1) return;
  std::snprintf(out + used, size - used, fmt, a, b, c);
}

// Copy with the first letter lowered: "You face..." reads "you face..." after "But: ".
void lowered(const char* in, char* out, const int size) {
  std::snprintf(out, size, "%s", in);
  if (out[0]) out[0] = static_cast<char>(std::tolower(static_cast<unsigned char>(out[0])));
}

// Every letter lowered, for a table row used mid-phrase ("river ford").
void lowerAll(const char* in, char* out, const int size) {
  std::snprintf(out, size, "%s", in);
  for (char* c = out; *c; ++c) *c = static_cast<char>(std::tolower(static_cast<unsigned char>(*c)));
}

void spark(const Set& set, const uint8_t verb, const uint8_t theme, const uint8_t shape, char* out, const int size) {
  char t[32];
  lowered(at(set.theme, theme), t, sizeof(t));
  add(out, size, kSpark[shape % kSparks], at(set.verb, verb), t);
}

}  // namespace

int chanceOf(const Odds odds) { return kChance[static_cast<int>(odds) % kOddsKinds]; }
const char* oddsName(const Odds odds) { return kOddsName[static_cast<int>(odds) % kOddsKinds]; }
const char* oddsShort(const Odds odds) { return kOddsShort[static_cast<int>(odds) % kOddsKinds]; }
const char* kindName(const Kind kind) { return kKindName[static_cast<int>(kind) % kKinds]; }
const char* genreName(const Genre genre) { return kGenreName[static_cast<int>(genre) % kGenres]; }

const Reading& consult(Oracle& o, Rng& rng, const Kind kind) {
  Reading r;
  r.genre = static_cast<uint8_t>(o.genre);
  r.kind = static_cast<uint8_t>(kind);
  r.odds = static_cast<uint8_t>(o.odds);
  r.roll = static_cast<uint8_t>(rng.below(100) + 1);
  const Set& set = setOf(r.genre);
  const bool sf = o.genre == Genre::SciFi;
  const bool mystery = o.genre == Genre::Mystery;
  uint8_t* p = r.pick;
  switch (kind) {
    case Kind::Ask:
      p[AskVerb] = pickFrom(rng, set.verb);
      p[AskTheme] = pickFrom(rng, set.theme);
      p[AskDanger] = pickFrom(rng, set.danger);
      p[AskChance] = pickFrom(rng, set.chance);
      p[EventVerb] = pickFrom(rng, set.verb);
      p[EventTheme] = pickFrom(rng, set.theme);
      p[AskShape] = static_cast<uint8_t>(rng.below(kSparks));
      p[EventShape] = static_cast<uint8_t>(rng.below(kSparks));
      break;
    case Kind::Spark:
      p[0] = pickFrom(rng, set.verb);
      p[1] = pickFrom(rng, set.theme);
      p[2] = static_cast<uint8_t>(rng.below(kSparks));
      break;
    case Kind::Detail:
      p[0] = pickFrom(rng, set.detailA);
      p[1] = pickFrom(rng, set.detailB);
      break;
    case Kind::Person:
      if (mystery) {
        p[0] = pickFrom(rng, ot::kMGiven);
        p[1] = pickFrom(rng, ot::kMFamily);
        p[2] = pickFrom(rng, ot::kMRole);
        p[3] = pickFrom(rng, ot::kMLook);
        p[4] = pickFrom(rng, ot::kMDisposition);
        p[5] = pickFrom(rng, ot::kMGoal);
        p[6] = pickFrom(rng, ot::kMSecret);
      } else if (sf) {
        p[0] = pickFrom(rng, ot::kSfGiven);
        p[1] = pickFrom(rng, ot::kSfFamily);
        p[2] = pickFrom(rng, ot::kSfRole);
        p[3] = pickFrom(rng, ot::kSfLook);
        p[4] = pickFrom(rng, ot::kSfDisposition);
        p[5] = pickFrom(rng, ot::kSfGoal);
        p[6] = pickFrom(rng, ot::kSfCallsign);
      } else {
        p[0] = static_cast<uint8_t>(rng.below(100));
        p[1] = pickFrom(rng, nameList(p[0]));
        p[2] = pickFrom(rng, ot::kTrait);
        p[3] = pickFrom(rng, ot::kRole);
        p[4] = pickFrom(rng, ot::kActivity);
        p[5] = pickFrom(rng, ot::kDisposition);
        p[6] = pickFrom(rng, ot::kGoal);
      }
      break;
    case Kind::Place:
      if (mystery) {
        p[0] = pickFrom(rng, ot::kMPlaceDesc);
        p[1] = pickFrom(rng, ot::kMPlace);
        p[2] = pickFrom(rng, ot::kMWrong);
      } else if (sf) {
        p[0] = static_cast<uint8_t>(rng.below(100));
        switch (sfPlace(p[0])) {
          case SfPlace::Settlement:
            p[1] = pickFrom(rng, ot::kSetName);
            p[2] = pickFrom(rng, ot::kSetWhere);
            p[3] = pickFrom(rng, ot::kSetLook);
            p[4] = pickFrom(rng, ot::kSetTrouble);
            break;
          case SfPlace::Planet:
            p[1] = pickFrom(rng, ot::kPlanetClass);
            p[2] = pickInGroup(rng, ot::kPlanetName, p[1]);
            p[3] = pickInGroup(rng, ot::kPlanetView, p[1]);
            break;
          case SfPlace::Starship:
            p[1] = pickFrom(rng, ot::kShipName);
            p[2] = pickFrom(rng, ot::kShipType);
            p[3] = pickFrom(rng, ot::kShipLook);
            p[4] = pickFrom(rng, ot::kShipContact);
            break;
        }
      } else {
        p[0] = pickFrom(rng, ot::kPlaceDesc);
        p[1] = pickFrom(rng, ot::kLocation);
        p[2] = pickFrom(rng, ot::kAspect);
        p[3] = pickFrom(rng, ot::kFeature);
      }
      break;
    case Kind::Danger:
      p[0] = pickFrom(rng, set.danger);
      break;
    case Kind::Chance:
      p[0] = pickFrom(rng, set.chance);
      break;
    case Kind::Twist:
      p[0] = pickFrom(rng, set.twist);
      break;
    case Kind::Clue:
      p[0] = pickFrom(rng, set.clue);
      break;
    case Kind::Fight:
    default:
      p[0] = pickFrom(rng, set.fight);
      break;
  }
  if (o.len == kOracleLog) {
    for (int i = 1; i < kOracleLog; ++i) o.log[i - 1] = o.log[i];
    --o.len;
  }
  o.log[o.len++] = r;
  return o.log[o.len - 1];
}

// Yes on the top `chance` of the d100. The fifth of each side farthest from
// the line is its "and"; the fifth nearest is its "but".
Answer answerOf(const Reading& r) {
  const int chance = chanceOf(static_cast<Odds>(r.odds));
  const int line = 100 - chance;  // no on 1..line, yes above
  const int roll = r.roll;
  if (roll > line) {
    const int band = chance / 5 > 0 ? chance / 5 : 1;
    if (roll > 100 - band) return Answer::YesAnd;
    if (roll <= line + band) return Answer::YesBut;
    return Answer::Yes;
  }
  const int band = line / 5 > 0 ? line / 5 : 1;
  if (roll <= band) return Answer::NoAnd;
  if (roll > line - band) return Answer::NoBut;
  return Answer::No;
}

bool named(const Reading& r) {
  const Kind k = static_cast<Kind>(r.kind);
  return k == Kind::Spark || k == Kind::Detail || k == Kind::Person || k == Kind::Place;
}

bool hasEvent(const Reading& r) { return r.roll < 100 && r.roll % 11 == 0; }

void title(const Reading& r, char* out, const int size) {
  out[0] = '\0';
  const uint8_t* p = r.pick;
  const Set& set = setOf(r.genre);
  const bool sf = r.genre == static_cast<uint8_t>(Genre::SciFi);
  const bool mystery = r.genre == static_cast<uint8_t>(Genre::Mystery);
  char a[48];
  switch (static_cast<Kind>(r.kind)) {
    case Kind::Ask: {
      static constexpr const char* kWords[] = {"NO, AND...", "NO", "NO, BUT...", "YES, BUT...", "YES", "YES, AND..."};
      add(out, size, "%s", kWords[static_cast<int>(answerOf(r))]);
      return;
    }
    case Kind::Spark:
      add(out, size, "%s + %s", at(set.action, p[0]), at(set.theme, p[1]));
      return;
    case Kind::Detail:
      lowerAll(at(set.detailB, p[1]), a, sizeof(a));
      add(out, size, "%s %s", at(set.detailA, p[0]), a);
      return;
    case Kind::Person:
      if (mystery) {
        add(out, size, "%s %s", at(ot::kMGiven, p[0]), at(ot::kMFamily, p[1]));
      } else if (sf) {
        add(out, size, "%s %s", at(ot::kSfGiven, p[0]), at(ot::kSfFamily, p[1]));
      } else {
        add(out, size, "%s", at(nameList(p[0]), p[1]));
      }
      return;
    case Kind::Place:
      if (mystery) {
        lowerAll(at(ot::kMPlace, p[1]), a, sizeof(a));
        add(out, size, "%s %s", at(ot::kMPlaceDesc, p[0]), a);
        return;
      }
      if (!sf) {
        lowerAll(at(ot::kLocation, p[1]), a, sizeof(a));
        add(out, size, "%s %s", at(ot::kPlaceDesc, p[0]), a);
        return;
      }
      switch (sfPlace(p[0])) {
        case SfPlace::Settlement:
          add(out, size, "%s", at(ot::kSetName, p[1]));
          return;
        case SfPlace::Planet:
          add(out, size, "%s", inGroup(ot::kPlanetName, p[1], p[2]));
          return;
        case SfPlace::Starship:
        default:
          add(out, size, "%s", at(ot::kShipName, p[1]));
          return;
      }
    default:
      add(out, size, "%s", kindName(static_cast<Kind>(r.kind)));
      return;
  }
}

void body(const Reading& r, char* out, const int size) {
  out[0] = '\0';
  const uint8_t* p = r.pick;
  const Set& set = setOf(r.genre);
  const bool sf = r.genre == static_cast<uint8_t>(Genre::SciFi);
  const bool mystery = r.genre == static_cast<uint8_t>(Genre::Mystery);
  char a[96];
  char b[48];
  switch (static_cast<Kind>(r.kind)) {
    case Kind::Ask:
      switch (answerOf(r)) {
        case Answer::YesAnd: {
          char s[96] = "";
          spark(set, p[AskVerb], p[AskTheme], p[AskShape], s, sizeof(s));
          lowered(s, a, sizeof(a));
          add(out, size, "And: %s", a);
          break;
        }
        case Answer::YesBut:
          lowered(at(set.danger, p[AskDanger]), a, sizeof(a));
          add(out, size, "But: %s.", a);
          break;
        case Answer::NoAnd:
          lowered(at(set.danger, p[AskDanger]), a, sizeof(a));
          add(out, size, "And worse: %s.", a);
          break;
        case Answer::NoBut:
          lowered(at(set.chance, p[AskChance]), a, sizeof(a));
          add(out, size, "But: %s.", a);
          break;
        default:
          break;
      }
      if (hasEvent(r)) {
        char s[96] = "";
        spark(set, p[EventVerb], p[EventTheme], p[EventShape], s, sizeof(s));
        lowered(s, a, sizeof(a));
        add(out, size, "%sMeanwhile: %s", out[0] ? " " : "", a);
      }
      return;
    case Kind::Spark:
      spark(set, p[0], p[1], p[2], out, size);
      return;
    case Kind::Detail:
      add(out, size, "%s", kDetailAsk[(p[0] + p[1]) % kDetailAsks]);
      return;
    case Kind::Person:
      if (mystery) {
        // "Motel clerk, too friendly. Seems evasive. Wants to be forgiven.
        //  Secret: they set the fire."
        lowered(at(ot::kMLook, p[3]), b, sizeof(b));
        add(out, size, "%s, %s. Seems ", at(ot::kMRole, p[2]), b);
        lowered(at(ot::kMDisposition, p[4]), a, sizeof(a));
        add(out, size, "%s. Wants to ", a);
        lowered(at(ot::kMGoal, p[5]), a, sizeof(a));
        add(out, size, "%s. Secret: ", a);
        lowered(at(ot::kMSecret, p[6]), a, sizeof(a));
        add(out, size, "%s.", a);
        return;
      }
      if (sf) {
        // "Engineer, augmented. Goes by Anvil. Seems wary. Wants to claim a resource."
        lowered(at(ot::kSfLook, p[3]), b, sizeof(b));
        add(out, size, "%s, %s. ", at(ot::kSfRole, p[2]), b);
        add(out, size, "Goes by %s. Seems ", at(ot::kSfCallsign, p[6]));
        lowered(at(ot::kSfDisposition, p[4]), a, sizeof(a));
        add(out, size, "%s. Wants to ", a);
        lowered(at(ot::kSfGoal, p[5]), a, sizeof(a));
        add(out, size, "%s.", a);
        return;
      }
      lowered(at(ot::kRole, p[3]), b, sizeof(b));
      add(out, size, "%s %s, ", at(ot::kTrait, p[2]), b);
      lowered(at(ot::kActivity, p[4]), a, sizeof(a));
      add(out, size, "%s. Seems ", a);
      lowered(at(ot::kDisposition, p[5]), a, sizeof(a));
      add(out, size, "%s. Wants to ", a);
      lowered(at(ot::kGoal, p[6]), a, sizeof(a));
      add(out, size, "%s.", a);
      return;
    case Kind::Place:
      if (mystery) {
        lowered(at(ot::kMWrong, p[2]), a, sizeof(a));
        add(out, size, "Something is off: %s.", a);
        return;
      }
      if (!sf) {
        lowerAll(at(ot::kAspect, p[2]), a, sizeof(a));
        lowerAll(at(ot::kFeature, p[3]), b, sizeof(b));
        add(out, size, "Look closer: %s %s.", a, b);
        return;
      }
      switch (sfPlace(p[0])) {
        case SfPlace::Settlement:
          // "Orbital settlement. Built from random scrap. Trouble: feuding factions."
          lowerAll(at(ot::kSetWhere, p[2]), b, sizeof(b));
          b[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(b[0])));
          add(out, size, "%s settlement. ", b);
          add(out, size, "%s. Trouble: ", at(ot::kSetLook, p[3]));
          lowered(at(ot::kSetTrouble, p[4]), a, sizeof(a));
          add(out, size, "%s.", a);
          return;
        case SfPlace::Planet:
          // "Jungle world. From orbit: vast river systems."
          lowerAll(at(ot::kPlanetClass, p[1]), b, sizeof(b));
          b[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(b[0])));
          lowered(inGroup(ot::kPlanetView, p[1], p[3]), a, sizeof(a));
          add(out, size, "%s. From orbit: %s.", b, a);
          return;
        case SfPlace::Starship:
        default:
          // "Starship, a hauler. Heavy armor. Contact: wary."
          lowered(at(ot::kShipType, p[2]), b, sizeof(b));
          add(out, size, "Starship, %s %s. ", std::strchr("aeiou", b[0]) ? "an" : "a", b);
          add(out, size, "%s. Contact: ", at(ot::kShipLook, p[3]));
          lowered(at(ot::kShipContact, p[4]), a, sizeof(a));
          add(out, size, "%s.", a);
          return;
      }
    case Kind::Danger:
      add(out, size, "%s.", at(set.danger, p[0]));
      return;
    case Kind::Chance:
      add(out, size, "%s.", at(set.chance, p[0]));
      return;
    case Kind::Twist:
      add(out, size, "%s.", at(set.twist, p[0]));
      return;
    case Kind::Clue:
      add(out, size, "%s.", at(set.clue, p[0]));
      return;
    case Kind::Fight:
    default:
      add(out, size, "%s.", at(set.fight, p[0]));
      return;
  }
}

void brief(const Reading& r, char* out, const int size) {
  out[0] = '\0';
  char t[64];
  title(r, t, sizeof(t));
  const bool sf = r.genre == static_cast<uint8_t>(Genre::SciFi);
  switch (static_cast<Kind>(r.kind)) {
    case Kind::Ask:
      add(out, size, "%s: %s%s", oddsName(static_cast<Odds>(r.odds)), t, hasEvent(r) ? " +event" : "");
      return;
    case Kind::Spark:
    case Kind::Detail:
      add(out, size, "%s", t);
      return;
    case Kind::Place:
      if (sf) {
        // The kind of place after the name: "Bulwark, settlement".
        static constexpr const char* kWhat[] = {"settlement", "planet", "starship"};
        add(out, size, "%s, %s", t, kWhat[static_cast<int>(sfPlace(r.pick[0]))]);
      } else {
        add(out, size, "%s", t);
      }
      return;
    case Kind::Person: {
      char b[32];
      const bool mystery = r.genre == static_cast<uint8_t>(Genre::Mystery);
      lowered(mystery ? at(ot::kMRole, r.pick[2]) : sf ? at(ot::kSfRole, r.pick[2]) : at(ot::kRole, r.pick[3]), b,
              sizeof(b));
      add(out, size, "%s, %s", t, b);
      return;
    }
    default: {
      char s[128];
      body(r, s, sizeof(s));
      char k[16];
      std::snprintf(k, sizeof(k), "%.15s", t);
      for (char* c = k + 1; *c; ++c) *c = static_cast<char>(std::tolower(static_cast<unsigned char>(*c)));
      add(out, size, "%s: %s", k, s);
      return;
    }
  }
}

void meta(const Reading& r, char* out, const int size) {
  out[0] = '\0';
  if (static_cast<Kind>(r.kind) != Kind::Ask) return;
  char roll[8];
  std::snprintf(roll, sizeof(roll), "%d", r.roll);
  add(out, size, "%s, rolled %s", oddsName(static_cast<Odds>(r.odds)), roll);
}

// P odds genre len, then one reading a line: genre kind odds roll picks...
// (O, the first form, had no genres: everything in it is fantasy.)
int serializeOracle(const Oracle& o, char* out, const int size) {
  int used = std::snprintf(out, size, "P %d %d %d\n", static_cast<int>(o.odds), static_cast<int>(o.genre), o.len);
  if (used < 0 || used >= size) return -1;
  for (int i = 0; i < o.len; ++i) {
    const Reading& r = o.log[i];
    int n = std::snprintf(out + used, size - used, "%d %d %d %d", r.genre, r.kind, r.odds, r.roll);
    if (n < 0 || n >= size - used) return -1;
    used += n;
    for (const uint8_t v : r.pick) {
      n = std::snprintf(out + used, size - used, " %d", v);
      if (n < 0 || n >= size - used) return -1;
      used += n;
    }
    if (used + 1 >= size) return -1;
    out[used++] = '\n';
    out[used] = '\0';
  }
  return used;
}

bool parseOracle(const char*& p, Oracle& o) {
  const auto num = [&p](long& v) {
    while (*p == ' ' || *p == '\n' || *p == '\r') ++p;
    char* end = nullptr;
    v = std::strtol(p, &end, 10);
    if (end == p) return false;
    p = end;
    return true;
  };
  while (*p == ' ' || *p == '\n' || *p == '\r') ++p;
  if (*p != 'O' && *p != 'P') return false;
  const bool genres = *p == 'P';
  ++p;
  long odds, genre = 0, len;
  if (!num(odds) || odds < 0 || odds >= kOddsKinds) return false;
  if (genres && (!num(genre) || genre < 0 || genre >= kGenres)) return false;
  if (!num(len) || len < 0 || len > kOracleLog) return false;
  Oracle t;
  t.odds = static_cast<Odds>(odds);
  t.genre = static_cast<Genre>(genre);
  t.len = static_cast<uint8_t>(len);
  for (int i = 0; i < t.len; ++i) {
    Reading& r = t.log[i];
    long g = 0;
    if (genres && (!num(g) || g < 0 || g >= kGenres)) return false;
    long v[3 + kPicks];
    for (long& x : v)
      if (!num(x) || x < 0 || x > 255) return false;
    if (v[0] >= kKinds || v[1] >= kOddsKinds || v[2] < 1 || v[2] > 100) return false;
    r.genre = static_cast<uint8_t>(g);
    r.kind = static_cast<uint8_t>(v[0]);
    r.odds = static_cast<uint8_t>(v[1]);
    r.roll = static_cast<uint8_t>(v[2]);
    for (int k = 0; k < kPicks; ++k) r.pick[k] = static_cast<uint8_t>(v[3 + k]);
  }
  o = t;
  return true;
}

}  // namespace tabletop
