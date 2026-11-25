#include "TabletopCore.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

namespace tabletop {

namespace {

const char* const kMajorNames[kMajors] = {
    "THE FOOL",  "THE MAGICIAN", "THE HIGH PRIESTESS", "THE EMPRESS",    "THE EMPEROR", "THE HIEROPHANT",
    "THE LOVERS", "THE CHARIOT", "STRENGTH",           "THE HERMIT",     "WHEEL OF FORTUNE", "JUSTICE",
    "THE HANGED MAN", "DEATH",   "TEMPERANCE",         "THE DEVIL",      "THE TOWER",   "THE STAR",
    "THE MOON",  "THE SUN",      "JUDGEMENT",          "THE WORLD"};
const char* const kRoman[kMajors] = {"0",   "I",   "II",  "III", "IV",  "V",    "VI",  "VII",
                                     "VIII", "IX", "X",   "XI",  "XII", "XIII", "XIV", "XV",
                                     "XVI", "XVII", "XVIII", "XIX", "XX", "XXI"};
const char* const kRankWords[14] = {"ACE", "TWO",  "THREE", "FOUR",   "FIVE",  "SIX",   "SEVEN",
                                    "EIGHT", "NINE", "TEN",  "PAGE", "KNIGHT", "QUEEN", "KING"};
const char* const kRankShort[13] = {"A", "2", "3", "4", "5", "6", "7", "8", "9", "10", "J", "Q", "K"};
const char* const kPlayingRanks[13] = {"ACE", "TWO",  "THREE", "FOUR", "FIVE",  "SIX", "SEVEN",
                                       "EIGHT", "NINE", "TEN", "JACK", "QUEEN", "KING"};
const char* const kTarotSuits[4] = {"WANDS", "CUPS", "SWORDS", "PENTACLES"};
const char* const kPlayingSuits[4] = {"SPADES", "HEARTS", "DIAMONDS", "CLUBS"};
constexpr int kSides[kDieKinds] = {4, 6, 8, 10, 12, 20, 100};

void fill(Table& t, Deck& deck, const bool tarot) {
  int first = 0;
  if (tarot) {
    first = t.arcana == Arcana::Minor ? kMajors : 0;
    const int last = t.arcana == Arcana::Major ? kMajors : kTarotCards;
    deck.size = static_cast<uint8_t>(last - first);
  } else {
    deck.size = static_cast<uint8_t>(kPlayingCards + (t.jokers ? kJokers : 0));
  }
  for (int i = 0; i < deck.size; ++i) deck.order[i] = static_cast<uint8_t>(first + i);
  for (int i = deck.size - 1; i > 0; --i) {
    const int j = t.rng.below(i + 1);
    const uint8_t c = deck.order[i];
    deck.order[i] = deck.order[j];
    deck.order[j] = c;
  }
  deck.left = deck.size;
}

void remember(Deck& deck, const Drawn& d) {
  if (deck.historyLen == kHistory) {
    std::memmove(deck.history, deck.history + 1, sizeof(Drawn) * (kHistory - 1));
    --deck.historyLen;
  }
  deck.history[deck.historyLen++] = d;
}

// Small reader for parse: the next integer, or false.
bool num(const char*& p, long& out) {
  char* end = nullptr;
  out = std::strtol(p, &end, 10);
  if (end == p) return false;
  p = end;
  return true;
}

int writeDeck(const Deck& d, char* out, int size) {
  int used = std::snprintf(out, size, "%d %d %d %d", d.size, d.left, d.shown, d.back);
  if (used < 0 || used >= size) return -1;
  for (int i = 0; i < d.left; ++i) {
    const int n = std::snprintf(out + used, size - used, " %d", d.order[i]);
    if (n < 0 || n >= size - used) return -1;
    used += n;
  }
  for (int i = 0; i < d.shown; ++i) {
    const int n = std::snprintf(out + used, size - used, " %d", d.current[i].id * 2 + (d.current[i].reversed ? 1 : 0));
    if (n < 0 || n >= size - used) return -1;
    used += n;
  }
  int n = std::snprintf(out + used, size - used, " %d", d.historyLen);
  if (n < 0 || n >= size - used) return -1;
  used += n;
  for (int i = 0; i < d.historyLen; ++i) {
    n = std::snprintf(out + used, size - used, " %d", d.history[i].id * 2 + (d.history[i].reversed ? 1 : 0));
    if (n < 0 || n >= size - used) return -1;
    used += n;
  }
  return used;
}

bool readDeck(const char*& p, Deck& d, const int maxSize, const bool hasBack) {
  long v[3];
  for (long& x : v)
    if (!num(p, x)) return false;
  long back = 0;
  if (hasBack && (!num(p, back) || back < 0 || back >= kBacks)) return false;
  d.back = static_cast<uint8_t>(back);
  if (v[0] < 1 || v[0] > maxSize || v[1] < 0 || v[1] > v[0] || !(v[2] == 0 || v[2] == 1 || v[2] == kSpread))
    return false;
  d.size = static_cast<uint8_t>(v[0]);
  d.left = static_cast<uint8_t>(v[1]);
  d.shown = static_cast<uint8_t>(v[2]);
  long c;
  for (int i = 0; i < d.left; ++i) {
    if (!num(p, c) || c < 0 || c >= maxSize) return false;
    d.order[i] = static_cast<uint8_t>(c);
  }
  for (int i = 0; i < d.shown; ++i) {
    if (!num(p, c) || c < 0 || c >= 2 * maxSize) return false;
    d.current[i] = Drawn{static_cast<uint8_t>(c / 2), (c & 1) != 0};
  }
  long h;
  if (!num(p, h) || h < 0 || h > kHistory) return false;
  d.historyLen = static_cast<uint8_t>(h);
  for (int i = 0; i < d.historyLen; ++i) {
    if (!num(p, c) || c < 0 || c >= 2 * maxSize) return false;
    d.history[i] = Drawn{static_cast<uint8_t>(c / 2), (c & 1) != 0};
  }
  return true;
}

}  // namespace

int sidesOf(const Die die) { return kSides[static_cast<int>(die) % kDieKinds]; }

void start(Table& t, const uint32_t seed) {
  t = Table{};
  t.rng.seed(seed);
  shuffle(t, t.cards, false);
  shuffle(t, t.tarot, true);
  addDie(t.dice, Die::D6);
  addDie(t.dice, Die::D6);
}

void shuffle(Table& t, Deck& deck, const bool tarot) {
  deck.shown = 0;
  deck.historyLen = 0;
  // Tarot always wears the rays; playing cards get the diamonds or the chevrons.
  deck.back = static_cast<uint8_t>(tarot ? kTarotBack : t.rng.below(kTarotBack));
  fill(t, deck, tarot);
}

int draw(Table& t, Deck& deck, int n, const bool tarot) {
  if (n > kSpread) n = kSpread;
  if (n > deck.left) n = deck.left;
  if (n <= 0) return 0;
  for (int i = 0; i < deck.shown; ++i) remember(deck, deck.current[i]);
  for (int i = 0; i < n; ++i) {
    Drawn d;
    d.id = deck.order[--deck.left];
    d.reversed = tarot && t.reversals && t.rng.below(2) == 1;
    deck.current[i] = d;
  }
  deck.shown = static_cast<uint8_t>(n);
  return n;
}

bool addDie(Dice& d, const Die die) {
  if (d.count >= kMaxDice) return false;
  d.pick[d.count] = static_cast<uint8_t>(die);
  d.value[d.count] = 0;
  ++d.count;
  d.rolled = false;
  return true;
}

void removeDie(Dice& d, const int slot) {
  if (slot < 0 || slot >= d.count) return;
  std::memmove(d.pick + slot, d.pick + slot + 1, static_cast<size_t>(d.count - slot - 1));
  std::memmove(d.value + slot, d.value + slot + 1, static_cast<size_t>(d.count - slot - 1));
  --d.count;
  if (d.count == 0) d.rolled = false;
}

void clearDice(Dice& d) {
  d.count = 0;
  d.rolled = false;
}

void roll(Table& t) {
  Dice& d = t.dice;
  if (d.count == 0) return;
  if (d.rolled) {
    // The last result joins the earlier totals before the new one replaces it.
    if (d.totalsLen == kRollHistory) {
      std::memmove(d.totals, d.totals + 1, sizeof(uint16_t) * (kRollHistory - 1));
      --d.totalsLen;
    }
    d.totals[d.totalsLen++] = static_cast<uint16_t>(total(d));
  }
  for (int i = 0; i < d.count; ++i) {
    d.value[i] = static_cast<uint8_t>(1 + t.rng.below(sidesOf(static_cast<Die>(d.pick[i]))));
  }
  d.rolled = true;
}

int total(const Dice& d) {
  int sum = 0;
  for (int i = 0; i < d.count; ++i) sum += d.value[i];
  return sum;
}

const char* majorName(const int n) { return kMajorNames[(n % kMajors + kMajors) % kMajors]; }
const char* roman(const int n) { return kRoman[(n % kMajors + kMajors) % kMajors]; }
const char* rankWord(const int rank) { return kRankWords[(rank - 1 + 14) % 14]; }
const char* rankShort(const int rank) { return kRankShort[(rank - 1 + 13) % 13]; }
const char* tarotSuitName(const int suit) { return kTarotSuits[suit & 3]; }
const char* playingSuitName(const int suit) { return kPlayingSuits[suit & 3]; }

void cardName(const uint8_t id, const bool tarot, char* out, const int size) {
  if (tarot) {
    if (isMajor(id)) {
      std::snprintf(out, size, "%s", majorName(id));
    } else {
      std::snprintf(out, size, "%s OF %s", rankWord(tarotRank(id)), tarotSuitName(tarotSuit(id)));
    }
    return;
  }
  if (isJoker(id)) {
    std::snprintf(out, size, "JOKER");
    return;
  }
  std::snprintf(out, size, "%s OF %s", kPlayingRanks[playingRank(id) - 1], playingSuitName(playingSuit(id)));
}

// T3 seed jokers reversals spread arcana (T2 has no arcana, T1 no card backs)
// <cards deck>
// <tarot deck>
// count picks... rolled values... totalsLen totals...
int serialize(const Table& t, char* out, const int size) {
  int used = std::snprintf(out, size, "T3 %lu %d %d %d %d\n", static_cast<unsigned long>(t.rng.s), t.jokers ? 1 : 0,
                           t.reversals ? 1 : 0, t.spread ? 1 : 0, static_cast<int>(t.arcana));
  if (used < 0 || used >= size) return -1;
  for (const Deck* d : {&t.cards, &t.tarot}) {
    const int n = writeDeck(*d, out + used, size - used - 1);
    if (n < 0) return -1;
    used += n;
    out[used++] = '\n';
    out[used] = '\0';
  }
  const Dice& d = t.dice;
  int n = std::snprintf(out + used, size - used, "%d", d.count);
  if (n < 0 || n >= size - used) return -1;
  used += n;
  for (int i = 0; i < d.count; ++i) {
    n = std::snprintf(out + used, size - used, " %d", d.pick[i]);
    if (n < 0 || n >= size - used) return -1;
    used += n;
  }
  n = std::snprintf(out + used, size - used, " %d", d.rolled ? 1 : 0);
  if (n < 0 || n >= size - used) return -1;
  used += n;
  for (int i = 0; d.rolled && i < d.count; ++i) {
    n = std::snprintf(out + used, size - used, " %d", d.value[i]);
    if (n < 0 || n >= size - used) return -1;
    used += n;
  }
  n = std::snprintf(out + used, size - used, " %d", d.totalsLen);
  if (n < 0 || n >= size - used) return -1;
  used += n;
  for (int i = 0; i < d.totalsLen; ++i) {
    n = std::snprintf(out + used, size - used, " %d", d.totals[i]);
    if (n < 0 || n >= size - used) return -1;
    used += n;
  }
  n = std::snprintf(out + used, size - used, "\n");
  if (n < 0 || n >= size - used) return -1;
  return used + n;
}

bool parse(const char* in, Table& out) {
  // T2 is the same file without the arcana, T1 without the card backs too.
  const bool hasArcana = std::strncmp(in, "T3 ", 3) == 0;
  const bool hasBack = hasArcana || std::strncmp(in, "T2 ", 3) == 0;
  if (!hasBack && std::strncmp(in, "T1 ", 3) != 0) return false;
  const char* p = in + 3;
  Table t;
  long v[4];
  for (long& x : v)
    if (!num(p, x)) return false;
  t.rng.seed(static_cast<uint32_t>(static_cast<unsigned long>(v[0])));
  t.jokers = v[1] != 0;
  t.reversals = v[2] != 0;
  t.spread = v[3] != 0;
  if (hasArcana) {
    long a;
    if (!num(p, a) || a < 0 || a >= static_cast<long>(Arcana::Count)) return false;
    t.arcana = static_cast<Arcana>(a);
  }
  if (!readDeck(p, t.cards, kPlayingCards + kJokers, hasBack)) return false;
  if (!readDeck(p, t.tarot, kTarotCards, hasBack)) return false;
  if (t.cards.size != kPlayingCards && t.cards.size != kPlayingCards + kJokers) return false;
  const int tarotSize = t.arcana == Arcana::Major   ? kMajors
                        : t.arcana == Arcana::Minor ? kTarotCards - kMajors
                                                    : kTarotCards;
  if (t.tarot.size != tarotSize) return false;
  t.tarot.back = kTarotBack;
  if (t.cards.back == kTarotBack) t.cards.back = 0;
  long c;
  if (!num(p, c) || c < 0 || c > kMaxDice) return false;
  t.dice.count = static_cast<uint8_t>(c);
  for (int i = 0; i < t.dice.count; ++i) {
    long k;
    if (!num(p, k) || k < 0 || k >= kDieKinds) return false;
    t.dice.pick[i] = static_cast<uint8_t>(k);
  }
  long rolled;
  if (!num(p, rolled)) return false;
  t.dice.rolled = rolled != 0 && t.dice.count > 0;
  for (int i = 0; rolled && i < t.dice.count; ++i) {
    long k;
    if (!num(p, k) || k < 1 || k > sidesOf(static_cast<Die>(t.dice.pick[i]))) return false;
    t.dice.value[i] = static_cast<uint8_t>(k);
  }
  long h;
  if (!num(p, h) || h < 0 || h > kRollHistory) return false;
  t.dice.totalsLen = static_cast<uint8_t>(h);
  for (int i = 0; i < t.dice.totalsLen; ++i) {
    long k;
    if (!num(p, k) || k < 0 || k > 100 * kMaxDice) return false;
    t.dice.totals[i] = static_cast<uint16_t>(k);
  }
  out = t;
  return true;
}

}  // namespace tabletop
