#include "LoboCore.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace lobo {

namespace {

uint32_t nextRandom(uint32_t& state) {
  state += 0x6D2B79F5u;
  uint32_t t = state;
  t = (t ^ (t >> 15)) * (t | 1u);
  t ^= t + (t ^ (t >> 7)) * (t | 61u);
  return t ^ (t >> 14);
}

void shuffle(uint8_t* cards, const int n, uint32_t& rng) {
  for (int i = n - 1; i > 0; --i) {
    const int j = static_cast<int>(nextRandom(rng) % static_cast<uint32_t>(i + 1));
    const uint8_t t = cards[i];
    cards[i] = cards[j];
    cards[j] = t;
  }
}

bool contains(const uint8_t* cards, const int n, const uint8_t c) {
  for (int i = 0; i < n; ++i)
    if (cards[i] == c) return true;
  return false;
}

void removeFrom(uint8_t* cards, uint8_t& n, const uint8_t c) {
  for (int i = 0; i < n; ++i) {
    if (cards[i] != c) continue;
    std::memmove(cards + i, cards + i + 1, static_cast<size_t>(n - i - 1));
    --n;
    return;
  }
}

void append(uint8_t* to, uint8_t& n, const uint8_t* from, const int count) {
  for (int i = 0; i < count && n < kCards; ++i) to[n++] = from[i];
}

// The played pile shuffled back into the deck (under what is left of it).
void reshuffleIn(Match& m) {
  append(m.deck, m.deckLen, m.played, m.playedLen);
  m.playedLen = 0;
  shuffle(m.deck, m.deckLen, m.rng);
}

// One card from the deck to `you` or the Wolf. False when there is none.
bool draw(Match& m, const bool toYou) {
  if (m.deckLen == 0) {
    if (m.playedLen == 0) return false;
    reshuffleIn(m);
    m.reshuffled = true;
  }
  const uint8_t c = m.deck[--m.deckLen];
  if (toYou) {
    m.you[m.youLen++] = c;
  } else {
    m.wolf[m.wolfLen++] = c;
  }
  m.fresh |= 1ull << c;
  return true;
}

void endHand(Match& m, const bool youTook) {
  m.youTookHand = youTook;
  if (youTook) {
    m.handPoints = static_cast<int16_t>(sum(m.you, m.youLen));
    m.youScore = static_cast<int16_t>(m.youScore + m.handPoints);
  } else {
    m.handPoints = static_cast<int16_t>(sum(m.wolf, m.wolfLen));
    m.wolfScore = static_cast<int16_t>(m.wolfScore + m.handPoints);
  }
  m.phase = (m.youScore >= m.target || m.wolfScore >= m.target) ? Phase::MatchOver : Phase::HandOver;
}

void deal(Match& m) {
  append(m.played, m.playedLen, m.wolf, m.wolfLen);
  append(m.played, m.playedLen, m.you, m.youLen);
  m.wolfLen = 0;
  m.youLen = 0;
  if (m.since >= kReshuffleEvery || m.deckLen < 2 * kHandSize + 1) {
    reshuffleIn(m);
    m.since = 0;
  }
  ++m.since;
  ++m.hand;
  m.last = Move::None;
  m.lastPLen = 0;
  m.lastWLen = 0;
  m.drew = 0;
  m.reshuffled = false;
  m.fresh = 0;
  for (int i = 0; i < 2 * kHandSize && m.deckLen > 0; ++i) {
    const uint8_t c = m.deck[--m.deckLen];
    if (i % 2) {
      m.you[m.youLen++] = c;
    } else {
      m.wolf[m.wolfLen++] = c;
    }
  }
  m.phase = Phase::Play;
}

// Can a subset of `pool` (minus the cards in `skip`) add up to exactly
// `target`, using at least `minCount` cards? Values are 1..10, so a table of
// reachable sums with the count capped at 2 is enough.
bool canMake(const int target, const uint8_t* pool, const int n, const uint8_t* skip, const int nskip,
             const int minCount) {
  if (target < 0 || target > 10) return false;
  if (target == 0) return minCount <= 0;
  bool reach[11][3] = {};
  reach[0][0] = true;
  for (int i = 0; i < n; ++i) {
    if (contains(skip, nskip, pool[i])) continue;
    const int v = valueOf(pool[i]);
    for (int s = 10 - v; s >= 0; --s) {
      for (int c = 2; c >= 0; --c) {
        if (!reach[s][c]) continue;
        const int nc = c < 2 ? c + 1 : 2;
        reach[s + v][nc] = true;
      }
    }
  }
  for (int c = minCount < 0 ? 0 : (minCount > 2 ? 2 : minCount); c <= 2; ++c)
    if (reach[target][c]) return true;
  return false;
}

bool allIn(const uint8_t* cards, const int n, const uint8_t* hand, const int len) {
  for (int i = 0; i < n; ++i) {
    if (!contains(hand, len, cards[i])) return false;
    for (int j = 0; j < i; ++j)
      if (cards[j] == cards[i]) return false;
  }
  return true;
}

}  // namespace

int sum(const uint8_t* cards, const int count) {
  int s = 0;
  for (int i = 0; i < count; ++i) s += valueOf(cards[i]);
  return s;
}

void start(Match& m, const uint32_t seed, const Level level, int target) {
  if (target < kMinTarget || target > kMaxTarget) target = kDefaultTarget;
  std::memset(&m, 0, sizeof(m));
  m.rng = seed;
  m.level = level;
  m.target = static_cast<int16_t>(target);
  const int top = maxValue(level);
  for (int s = 0; s < 5; ++s)
    for (int v = 1; v <= top; ++v) m.deck[m.deckLen++] = static_cast<uint8_t>(s * 10 + v - 1);
  shuffle(m.deck, m.deckLen, m.rng);
  deal(m);
}

bool nextHand(Match& m) {
  if (m.phase != Phase::HandOver) return false;
  deal(m);
  return true;
}

Verdict classify(const Match& m, const uint8_t* p, const int np, const uint8_t* w, const int nw) {
  Verdict v{false, Move::None, 0};
  if (np <= 0 || nw <= 0 || np > kMaxMoveCards || nw > kMaxMoveCards) return v;
  if (!allIn(p, np, m.you, m.youLen) || !allIn(w, nw, m.wolf, m.wolfLen)) return v;
  const int ps = sum(p, np);
  const int ws = sum(w, nw);
  if (np == 1 && nw == 1) {
    if (ps == ws) return Verdict{true, Move::Perfect, 0};
    if (ps > ws) return Verdict{true, Move::Over, ps - ws};
    return v;
  }
  if (np > 1 && nw == 1) return ps == ws ? Verdict{true, Move::Sum, 0} : v;
  if (np == 1 && nw > 1) return ps == ws ? Verdict{true, Move::Split, 0} : v;
  return v;
}

bool extendable(const Match& m, const uint8_t* p, const int np, const uint8_t* w, const int nw) {
  if (m.phase != Phase::Play) return false;
  if (np > kMaxMoveCards || nw > kMaxMoveCards) return false;
  if ((np && !allIn(p, np, m.you, m.youLen)) || (nw && !allIn(w, nw, m.wolf, m.wolfLen))) return false;
  // Perfect or Over: one of yours against one of the Wolf's, yours no smaller.
  if (np <= 1 && nw <= 1) {
    for (int i = 0; i < m.youLen; ++i) {
      if (np == 1 && m.you[i] != p[0]) continue;
      for (int j = 0; j < m.wolfLen; ++j) {
        if (nw == 1 && m.wolf[j] != w[0]) continue;
        if (valueOf(m.you[i]) >= valueOf(m.wolf[j])) return true;
      }
    }
  }
  // Sum: two or more of yours against one of the Wolf's.
  if (nw <= 1) {
    const int have = sum(p, np);
    for (int j = 0; j < m.wolfLen; ++j) {
      if (nw == 1 && m.wolf[j] != w[0]) continue;
      if (canMake(valueOf(m.wolf[j]) - have, m.you, m.youLen, p, np, 2 - np)) return true;
    }
  }
  // Split: one of yours against two or more of the Wolf's.
  if (np <= 1) {
    const int have = sum(w, nw);
    for (int i = 0; i < m.youLen; ++i) {
      if (np == 1 && m.you[i] != p[0]) continue;
      if (canMake(valueOf(m.you[i]) - have, m.wolf, m.wolfLen, w, nw, 2 - nw)) return true;
    }
  }
  return false;
}

bool play(Match& m, const uint8_t* p, const int np, const uint8_t* w, const int nw) {
  if (m.phase != Phase::Play) return false;
  const Verdict v = classify(m, p, np, w, nw);
  if (!v.ok) return false;
  m.last = v.move;
  m.lastPLen = static_cast<uint8_t>(np);
  m.lastWLen = static_cast<uint8_t>(nw);
  std::memcpy(m.lastP, p, static_cast<size_t>(np));
  std::memcpy(m.lastW, w, static_cast<size_t>(nw));
  m.fresh = 0;
  m.reshuffled = false;
  m.drew = 0;
  for (int i = 0; i < np; ++i) removeFrom(m.you, m.youLen, p[i]);
  for (int i = 0; i < nw; ++i) removeFrom(m.wolf, m.wolfLen, w[i]);
  append(m.played, m.playedLen, w, nw);
  append(m.played, m.playedLen, p, np);
  // The price.
  m.drewYou = v.move == Move::Perfect || v.move == Move::Sum;
  const int n = v.move == Move::Over ? v.over : 1;
  for (int i = 0; i < n; ++i) {
    if (!draw(m, m.drewYou)) break;
    ++m.drew;
  }
  if (m.wolfLen == 0) endHand(m, true);
  return true;
}

bool fold(Match& m) {
  if (m.phase != Phase::Play) return false;
  m.last = Move::None;
  endHand(m, false);
  return true;
}

bool youWonMatch(const Match& m) { return m.youScore >= m.target && m.youScore >= m.wolfScore; }

void tally(const Match& m, int counts[11]) {
  for (int v = 0; v <= 10; ++v) counts[v] = 0;
  for (int i = 0; i < m.deckLen; ++i) ++counts[valueOf(m.deck[i])];
}

int reshuffleAfter(const Match& m) {
  const int left = kReshuffleEvery - m.since;
  return m.hand + (left > 0 ? left : 0);
}

// ---- the save line ------------------------------------------------------------

int serialize(const Match& m, char* buffer, const int size) {
  int used = 0;
  auto put = [&](const long v) {
    if (used < 0 || used >= size) {
      used = -1;
      return;
    }
    const int n = std::snprintf(buffer + used, static_cast<size_t>(size - used), used ? " %ld" : "%ld", v);
    used = (n < 0 || used + n >= size) ? -1 : used + n;
  };
  auto pile = [&](const uint8_t* cards, const int n) {
    put(n);
    for (int i = 0; i < n; ++i) put(cards[i]);
  };
  put(1);
  put(static_cast<long>(m.level));
  put(m.target);
  put(m.youScore);
  put(m.wolfScore);
  put(m.hand);
  put(m.since);
  put(static_cast<long>(m.phase));
  put(static_cast<long>(m.rng >> 16));
  put(static_cast<long>(m.rng & 0xFFFF));
  pile(m.deck, m.deckLen);
  pile(m.played, m.playedLen);
  pile(m.wolf, m.wolfLen);
  pile(m.you, m.youLen);
  put(static_cast<long>(m.last));
  pile(m.lastP, m.lastPLen);
  pile(m.lastW, m.lastWLen);
  put(m.drew);
  put(m.drewYou);
  put(m.reshuffled);
  put(static_cast<long>(m.fresh >> 25));
  put(static_cast<long>(m.fresh & 0x1FFFFFF));
  put(m.youTookHand);
  put(m.handPoints);
  return used;
}

bool parse(const char* text, Match& out) {
  if (!text) return false;
  const char* cursor = text;
  bool ok = true;
  auto take = [&](const long lo, const long hi) -> long {
    if (!ok) return 0;
    char* next = nullptr;
    const long v = std::strtol(cursor, &next, 10);
    if (next == cursor || v < lo || v > hi) {
      ok = false;
      return 0;
    }
    cursor = next;
    return v;
  };
  bool seen[kCards] = {};
  auto pile = [&](uint8_t* cards, uint8_t& n, const int cap, const bool unique) {
    n = static_cast<uint8_t>(take(0, cap));
    for (int i = 0; ok && i < n; ++i) {
      cards[i] = static_cast<uint8_t>(take(0, kCards - 1));
      if (!unique) continue;
      if (seen[cards[i]]) ok = false;
      seen[cards[i]] = true;
    }
  };
  Match m;
  std::memset(&m, 0, sizeof(m));
  if (take(1, 1) != 1) return false;
  m.level = static_cast<Level>(take(0, 2));
  m.target = static_cast<int16_t>(take(kMinTarget, kMaxTarget));
  m.youScore = static_cast<int16_t>(take(0, 1000));
  m.wolfScore = static_cast<int16_t>(take(0, 1000));
  m.hand = static_cast<uint16_t>(take(1, 60000));
  m.since = static_cast<uint8_t>(take(0, kReshuffleEvery));
  m.phase = static_cast<Phase>(take(0, 2));
  m.rng = static_cast<uint32_t>(take(0, 0xFFFF)) << 16;
  m.rng |= static_cast<uint32_t>(take(0, 0xFFFF));
  pile(m.deck, m.deckLen, kCards, true);
  pile(m.played, m.playedLen, kCards, true);
  pile(m.wolf, m.wolfLen, kCards, true);
  pile(m.you, m.youLen, kCards, true);
  m.last = static_cast<Move>(take(0, 4));
  pile(m.lastP, m.lastPLen, kMaxMoveCards, false);
  pile(m.lastW, m.lastWLen, kMaxMoveCards, false);
  m.drew = static_cast<uint8_t>(take(0, kCards));
  m.drewYou = take(0, 1) != 0;
  m.reshuffled = take(0, 1) != 0;
  m.fresh = static_cast<uint64_t>(take(0, 0x1FFFFFF)) << 25;
  m.fresh |= static_cast<uint64_t>(take(0, 0x1FFFFFF));
  m.youTookHand = take(0, 1) != 0;
  m.handPoints = static_cast<int16_t>(take(0, 500));
  if (!ok) return false;
  // Every card of the level's deck exactly once across the four piles.
  int total = 0;
  for (int c = 0; c < kCards; ++c) {
    if (!seen[c]) continue;
    if (valueOf(static_cast<uint8_t>(c)) > maxValue(m.level)) return false;
    ++total;
  }
  if (total != 5 * maxValue(m.level)) return false;
  out = m;
  return true;
}

}  // namespace lobo
