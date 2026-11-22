// Lobo rules tests: hand-made cases for every move, then many random matches
// checked against brute force (every subset of both hands) and for card
// conservation, plus the save line round trip.
#include <cstdio>
#include <cstring>
#include <vector>

#include "LoboCore.h"

namespace l = lobo;

static int checks = 0, failures = 0;
#define CHECK(c)                                                    \
  do {                                                              \
    ++checks;                                                       \
    if (!(c)) {                                                     \
      ++failures;                                                   \
      if (failures < 20) std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); \
    }                                                               \
  } while (0)

static uint8_t C(int value, int suit = 0) { return static_cast<uint8_t>(suit * 10 + value - 1); }

// A match with the given hands and the rest of the deck in order.
static l::Match setup(std::vector<uint8_t> wolf, std::vector<uint8_t> you, l::Level level = l::Level::Hard) {
  l::Match m;
  l::start(m, 1, level);
  bool used[50] = {};
  m.wolfLen = 0;
  m.youLen = 0;
  for (uint8_t c : wolf) { m.wolf[m.wolfLen++] = c; used[c] = true; }
  for (uint8_t c : you) { m.you[m.youLen++] = c; used[c] = true; }
  m.deckLen = 0;
  m.playedLen = 0;
  for (int c = 49; c >= 0; --c)
    if (!used[c] && l::valueOf(static_cast<uint8_t>(c)) <= l::maxValue(level)) m.deck[m.deckLen++] = static_cast<uint8_t>(c);
  return m;
}

static int countAll(const l::Match& m) { return m.deckLen + m.playedLen + m.wolfLen + m.youLen; }

static void testMoves() {
  // Perfect: your 7 takes the 7, you draw the top card.
  l::Match m = setup({C(7), C(3), C(9), C(2)}, {C(7, 1), C(4), C(5), C(1)});
  uint8_t top = m.deck[m.deckLen - 1];
  uint8_t p[] = {C(7, 1)}, w[] = {C(7)};
  l::Verdict v = l::classify(m, p, 1, w, 1);
  CHECK(v.ok && v.move == l::Move::Perfect);
  CHECK(l::play(m, p, 1, w, 1));
  CHECK(m.youLen == 4 && m.you[3] == top && m.wolfLen == 3);
  CHECK(m.drewYou && m.drew == 1 && (m.fresh >> top & 1));
  CHECK(m.playedLen == 2);

  // Sum: 4 + 5 take the 9; you draw.
  uint8_t p2[] = {C(4), C(5)}, w2[] = {C(9)};
  v = l::classify(m, p2, 2, w2, 1);
  CHECK(v.ok && v.move == l::Move::Sum);
  uint8_t bad[] = {C(4), C(1)};
  CHECK(!l::classify(m, bad, 2, w2, 1).ok);
  CHECK(l::play(m, p2, 2, w2, 1));
  CHECK(m.youLen == 3 && m.wolfLen == 2);

  // Split: one card against several adding up to it; the Wolf draws one.
  m = setup({C(2), C(3), C(8), C(10)}, {C(5), C(1), C(6), C(4)});
  uint8_t p3[] = {C(5)}, w3[] = {C(2), C(3)};
  v = l::classify(m, p3, 1, w3, 2);
  CHECK(v.ok && v.move == l::Move::Split);
  CHECK(l::play(m, p3, 1, w3, 2));
  CHECK(m.wolfLen == 3 && m.youLen == 3 && !m.drewYou && m.drew == 1);

  // Over: your 6 takes a 2; the Wolf draws 4.
  m = setup({C(2), C(9), C(8), C(10)}, {C(6), C(1), C(1, 1), C(1, 2)});
  uint8_t p4[] = {C(6)}, w4[] = {C(2)};
  v = l::classify(m, p4, 1, w4, 1);
  CHECK(v.ok && v.move == l::Move::Over && v.over == 4);
  CHECK(l::play(m, p4, 1, w4, 1));
  CHECK(m.wolfLen == 7 && m.drew == 4);
  // A smaller card cannot take a bigger one; two against two is never legal.
  uint8_t p5[] = {C(1)}, w5[] = {C(9)};
  CHECK(!l::classify(m, p5, 1, w5, 1).ok);
  uint8_t p6[] = {C(1), C(1, 1)}, w6[] = {C(9), C(8)};
  CHECK(!l::classify(m, p6, 2, w6, 2).ok);
  // Cards not in the right hands are refused.
  CHECK(!l::classify(m, w5, 1, p5, 1).ok);

  // Clearing the Wolf with a Perfect: you draw first, then score your hand.
  m = setup({C(3)}, {C(3, 1), C(5), C(2)});
  top = m.deck[m.deckLen - 1];
  uint8_t p7[] = {C(3, 1)}, w7[] = {C(3)};
  CHECK(l::play(m, p7, 1, w7, 1));
  CHECK(m.phase == l::Phase::HandOver && m.youTookHand);
  CHECK(m.handPoints == 5 + 2 + l::valueOf(top));
  CHECK(m.youScore == m.handPoints && m.wolfScore == 0);

  // A Split that empties the Wolf's hand: it draws, so the hand goes on.
  m = setup({C(3), C(4)}, {C(7), C(1)});
  uint8_t p8[] = {C(7)}, w8[] = {C(3), C(4)};
  CHECK(l::play(m, p8, 1, w8, 2));
  CHECK(m.phase == l::Phase::Play && m.wolfLen == 1);

  // Fold: the Wolf scores its hand.
  m = setup({C(9), C(10), C(4)}, {C(1), C(2)});
  CHECK(!l::anyMove(m));  // 1 and 2 against 9, 10 and 4: nothing to play
  CHECK(l::fold(m));
  CHECK(m.phase == l::Phase::HandOver && !m.youTookHand && m.wolfScore == 23);

  // Over with a dry deck: the played pile is shuffled in to pay.
  m = setup({C(1), C(9)}, {C(10), C(2)});
  m.playedLen = 0;
  for (int i = 0; i < m.deckLen - 2; ++i) m.played[m.playedLen++] = m.deck[i];
  std::memmove(m.deck, m.deck + m.deckLen - 2, 2);
  m.deckLen = 2;
  uint8_t p9[] = {C(10)}, w9[] = {C(1)};
  CHECK(l::play(m, p9, 1, w9, 1));
  CHECK(m.drew == 9 && m.reshuffled && countAll(m) == 50);
}

static void testMatchFlow() {
  l::Match m;
  l::start(m, 42, l::Level::Medium, 60);
  CHECK(m.hand == 1 && m.since == 1 && m.wolfLen == 4 && m.youLen == 4 && m.deckLen == 37);
  for (int i = 0; i < 50; ++i) CHECK(l::valueOf(static_cast<uint8_t>(i)) == i % 10 + 1);
  // Hands 2 and 3 use the same deck; hand 4 reshuffles everything.
  l::fold(m);
  m.phase = l::Phase::HandOver;
  m.wolfScore = 0;
  CHECK(l::nextHand(m) && m.hand == 2 && m.since == 2 && m.playedLen == 8 && m.deckLen == 29);
  l::fold(m);
  m.wolfScore = 0;
  m.phase = l::Phase::HandOver;
  CHECK(l::nextHand(m) && m.since == 3 && m.deckLen == 21);
  CHECK(l::reshuffleAfter(m) == 3);
  l::fold(m);
  m.wolfScore = 0;
  m.phase = l::Phase::HandOver;
  CHECK(l::nextHand(m) && m.since == 1 && m.playedLen == 0 && m.deckLen == 45 - 8);
  // Reaching the target ends the match.
  m.wolfScore = 58;
  l::fold(m);
  CHECK(m.phase == l::Phase::MatchOver && !l::youWonMatch(m));
  CHECK(!l::nextHand(m));
  // Easy has no 9s or 10s.
  l::start(m, 7, l::Level::Easy);
  for (int i = 0; i < m.deckLen; ++i) CHECK(l::valueOf(m.deck[i]) <= 8);
  CHECK(countAll(m) == 40);
}

// Brute force: is there a legal move containing the selection?
static bool bruteExtendable(const l::Match& m, const std::vector<uint8_t>& p, const std::vector<uint8_t>& w) {
  const int ny = m.youLen, nw = m.wolfLen;
  for (int a = 1; a < (1 << ny); ++a) {
    uint8_t P[12]; int np = 0; bool okp = true;
    for (int i = 0; i < ny; ++i) if (a >> i & 1) { if (np >= 10) { okp = false; break; } P[np++] = m.you[i]; }
    if (!okp) continue;
    bool hasAll = true;
    for (uint8_t c : p) { bool f = false; for (int i = 0; i < np; ++i) f |= P[i] == c; hasAll &= f; }
    if (!hasAll) continue;
    for (int b = 1; b < (1 << nw); ++b) {
      uint8_t W[12]; int k = 0; bool okw = true;
      for (int j = 0; j < nw; ++j) if (b >> j & 1) { if (k >= 10) { okw = false; break; } W[k++] = m.wolf[j]; }
      if (!okw) continue;
      bool all = true;
      for (uint8_t c : w) { bool f = false; for (int j = 0; j < k; ++j) f |= W[j] == c; all &= f; }
      if (!all) continue;
      if (l::classify(m, P, np, W, k).ok) return true;
    }
  }
  return false;
}

static uint32_t rnd(uint32_t& s) { s = s * 1664525u + 1013904223u; return s >> 8; }

static void testSoak() {
  for (uint32_t seed = 1; seed <= 3000; ++seed) {
    l::Match m;
    l::start(m, seed, static_cast<l::Level>(seed % 3), 60 + seed % 50);
    const int total = 5 * l::maxValue(m.level);
    uint32_t r = seed * 2654435761u;
    for (int step = 0; step < 400 && m.phase != l::Phase::MatchOver; ++step) {
      CHECK(countAll(m) == total);
      if (m.phase == l::Phase::HandOver) { CHECK(l::nextHand(m)); continue; }
      // a random selection: check extendable against brute force when small
      if (m.wolfLen <= 12 && m.youLen <= 8) {
        std::vector<uint8_t> p, w;
        for (int i = 0; i < m.youLen; ++i) if (rnd(r) % 3 == 0) p.push_back(m.you[i]);
        for (int j = 0; j < m.wolfLen; ++j) if (rnd(r) % 4 == 0) w.push_back(m.wolf[j]);
        if (p.size() <= 10 && w.size() <= 10)
          CHECK(l::extendable(m, p.data(), static_cast<int>(p.size()), w.data(), static_cast<int>(w.size())) ==
                bruteExtendable(m, p, w));
        CHECK(l::anyMove(m) == bruteExtendable(m, {}, {}));
      }
      // Find a legal move by growing a selection the way a player taps.
      std::vector<uint8_t> p, w;
      bool moved = false;
      for (int tries = 0; tries < 60 && l::anyMove(m); ++tries) {
        l::Verdict v = l::classify(m, p.data(), static_cast<int>(p.size()), w.data(), static_cast<int>(w.size()));
        if (v.ok && rnd(r) % 2 == 0) {
          const int before = countAll(m);
          CHECK(l::play(m, p.data(), static_cast<int>(p.size()), w.data(), static_cast<int>(w.size())));
          CHECK(countAll(m) == before);
          moved = true;
          break;
        }
        // add a random card that keeps the selection extendable
        std::vector<std::pair<bool, uint8_t>> options;
        for (int i = 0; i < m.youLen; ++i) {
          bool in = false; for (uint8_t c : p) in |= c == m.you[i];
          if (in) continue;
          p.push_back(m.you[i]);
          if (l::extendable(m, p.data(), static_cast<int>(p.size()), w.data(), static_cast<int>(w.size()))) options.push_back({true, m.you[i]});
          p.pop_back();
        }
        for (int j = 0; j < m.wolfLen; ++j) {
          bool in = false; for (uint8_t c : w) in |= c == m.wolf[j];
          if (in) continue;
          w.push_back(m.wolf[j]);
          if (l::extendable(m, p.data(), static_cast<int>(p.size()), w.data(), static_cast<int>(w.size()))) options.push_back({false, m.wolf[j]});
          w.pop_back();
        }
        if (options.empty()) {
          // only possible when the selection is already a complete move
          CHECK(v.ok);
          if (v.ok) { CHECK(l::play(m, p.data(), static_cast<int>(p.size()), w.data(), static_cast<int>(w.size()))); moved = true; }
          break;
        }
        auto o = options[rnd(r) % options.size()];
        (o.first ? p : w).push_back(o.second);
      }
      if (!moved && (!l::anyMove(m) || rnd(r) % 20 == 0)) {
        const int wolfPts = l::sum(m.wolf, m.wolfLen), before = m.wolfScore;
        CHECK(l::fold(m));
        CHECK(m.wolfScore == before + wolfPts);
      }
      // save round trip
      char buf[1024], again[1024];
      const int n = l::serialize(m, buf, sizeof(buf));
      CHECK(n > 0);
      l::Match back;
      CHECK(l::parse(buf, back));
      CHECK(l::serialize(back, again, sizeof(again)) == n && std::strcmp(buf, again) == 0);
    }
  }
  l::Match junk;
  CHECK(!l::parse("", junk));
  CHECK(!l::parse("2", junk));
}

int main() {
  testMoves();
  testMatchFlow();
  testSoak();
  std::printf("lobo: %d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
