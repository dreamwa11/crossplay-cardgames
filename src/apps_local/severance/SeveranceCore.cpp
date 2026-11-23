#include "SeveranceCore.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <new>

namespace sev {

namespace {

const char* const kCharNames[kChars] = {"Mark S.", "Helly R.", "Dylan G.", "Irving B."};
constexpr int kLayers[4] = {1, 3, 4, 4};
constexpr int kAdmin[11] = {0, 2, 2, 2, 4, 4, 4, 2, 2, 2, 2};
constexpr int kKierStack = 9;

// Card table order: 36 grid numbers, 26 Administration numbers, 8 employees,
// the Goat, Gemma, the six other Administration specials, Drummond, Casey, Kier.
constexpr int kGridNums = 0;
constexpr int kAdminNums = 36;
constexpr int kMains = 62;
constexpr int kGoat = 70;
constexpr int kGemma = 71;
constexpr int kDieter = 72;
constexpr int kGraner = 73;
constexpr int kMilchick = 74;
constexpr int kCobel = 75;
constexpr int kReghabi = 76;
constexpr int kJame = 77;
constexpr int kDrummond = 78;
constexpr int kCasey = 79;
constexpr int kKier = 80;

// Small text builder: appends, never overflows.
struct Text {
  char* p;
  int size;
  int n = 0;
  Text(char* buf, int s) : p(buf), size(s) { p[0] = '\0'; }
  Text& add(const char* fmt, ...) {
    if (n >= size - 1) return *this;
    va_list ap;
    va_start(ap, fmt);
    const int w = std::vsnprintf(p + n, size - n, fmt, ap);
    va_end(ap);
    if (w > 0) n = n + w < size - 1 ? n + w : size - 1;
    return *this;
  }
};

bool endsWithMark(const char* s) {
  const size_t n = std::strlen(s);
  return n && (s[n - 1] == '!' || s[n - 1] == '?');
}

void logLine(Game& g, Kind kind, const char* fmt, ...) {
  if (g.nLog == kLogLines) {
    std::memmove(g.log, g.log + 1, sizeof(LogLine) * (kLogLines - 1));
    --g.nLog;
  }
  LogLine& l = g.log[g.nLog++];
  l.turn = g.turn;
  l.kind = kind;
  va_list ap;
  va_start(ap, fmt);
  std::vsnprintf(l.text, sizeof(l.text), fmt, ap);
  va_end(ap);
}

void shuffle(Game& g, uint8_t* a, int n) {
  for (int i = n - 1; i > 0; --i) {
    const int j = g.rng.below(i + 1);
    const uint8_t t = a[i];
    a[i] = a[j];
    a[j] = t;
  }
}

const char* lab(const Game& g, int id) {
  static char buf[4][64];
  static int k = 0;
  char* out = buf[k++ & 3];
  label(g, id, out, 64);
  return out;
}

bool isNum(int id) { return cardDef(id).type == Type::Num; }
bool isMain(int id) { return cardDef(id).type == Type::Main; }

// ---- queries ----

struct Top {
  uint8_t c;
  uint8_t i;
};

int rawTops(const Game& g, Top* out) {
  int n = 0;
  for (int i = 0; i < kStacks; ++i) {
    const int c = g.stacks[i].top();
    if (c != kNone && g.card[c].faceUp) out[n++] = Top{static_cast<uint8_t>(c), static_cast<uint8_t>(i)};
  }
  return n;
}
// Drummond's locked zone is out of use.
int gridTops(const Game& g, Top* out) {
  Top all[kStacks];
  const int n = rawTops(g, all);
  int m = 0;
  for (int k = 0; k < n; ++k)
    if (zoneOf(all[k].i) != g.lockZone) out[m++] = all[k];
  return m;
}
int gridNums(const Game& g, Top* out) {
  Top all[kStacks];
  const int n = gridTops(g, all);
  int m = 0;
  for (int k = 0; k < n; ++k)
    if (isNum(all[k].c) && all[k].c != g.blocked) out[m++] = all[k];
  return m;
}
int gridMains(const Game& g, Top* out) {
  Top all[kStacks];
  const int n = gridTops(g, all);
  int m = 0;
  for (int k = 0; k < n; ++k)
    if (isMain(all[k].c)) out[m++] = all[k];
  return m;
}

// Every non-empty subset sum, as bits.
struct Sums {
  uint64_t b[4] = {0, 0, 0, 0};
  bool has(int v) const { return v >= 0 && v < 256 && (b[v >> 6] >> (v & 63)) & 1; }
  void set(int v) {
    if (v >= 0 && v < 256) b[v >> 6] |= 1ull << (v & 63);
  }
};
Sums sums(const int* vals, int n) {
  Sums s;
  s.set(0);
  for (int k = 0; k < n; ++k) {
    Sums next = s;
    for (int v = 255; v >= 0; --v)
      if (s.has(v)) next.set(v + vals[k]);
    s = next;
  }
  s.b[0] &= ~1ull;
  return s;
}

int pickStack(Game& g) {
  int idx[kStacks];
  int n = 0;
  for (int i = 0; i < kStacks; ++i)
    if (g.stacks[i].n) idx[n++] = i;
  if (!n)
    for (int i = 0; i < kStacks; ++i) idx[n++] = i;
  return idx[g.rng.below(n)];
}

bool takeFromGrid(Game& g, int id) {
  for (int i = 0; i < kStacks; ++i) {
    Stack& s = g.stacks[i];
    for (int j = 0; j < s.n; ++j) {
      if (s.c[j] != id) continue;
      std::memmove(s.c + j, s.c + j + 1, static_cast<size_t>(s.n - j - 1));
      --s.n;
      return true;
    }
  }
  return false;
}

void pushTop(Stack& s, int id) {
  if (s.n < kStackCap) s.c[s.n++] = static_cast<uint8_t>(id);
}
void insertAt(Stack& s, int at, int id) {
  if (s.n >= kStackCap) return;
  std::memmove(s.c + at + 1, s.c + at, static_cast<size_t>(s.n - at));
  s.c[at] = static_cast<uint8_t>(id);
  ++s.n;
}

void insertHidden(Game& g, int id, int k) {
  g.card[id].faceUp = false;
  Stack& st = g.stacks[k];
  if (g.unlocked[zoneOf(k)] && st.n && g.card[st.top()].faceUp) {
    insertAt(st, st.n - 1, id);
  } else {
    pushTop(st, id);
  }
}

void stackList(const int* ks, int n, char* out, int size) {
  Text t(out, size);
  if (n > 1 && ks[0] == ks[1]) {
    t.add("the same stack in Zone %d", zoneOf(ks[0]) + 1);
    return;
  }
  int z[4];
  int nz = 0;
  for (int k = 0; k < n; ++k) {
    const int v = zoneOf(ks[k]) + 1;
    bool seen = false;
    for (int q = 0; q < nz; ++q) seen |= z[q] == v;
    if (!seen) z[nz++] = v;
  }
  for (int a = 1; a < nz; ++a)
    for (int b = a; b > 0 && z[b - 1] > z[b]; --b) {
      const int x = z[b];
      z[b] = z[b - 1];
      z[b - 1] = x;
    }
  t.add("%s in Zone%s", n > 1 ? "stacks" : "a stack", nz > 1 ? "s " : " ");
  for (int q = 0; q < nz; ++q) t.add("%s%d", q ? " and " : "", z[q]);
}

// Log an ability, and keep it for the screen unless it waits on a choice.
void say(Game& g, int cardId, const char* title, const char* text, Kind kind, bool choice = false) {
  logLine(g, kind, "%s%s%s", title, endsWithMark(title) ? " " : ": ", text);
  if (choice || g.nAnn >= kAnnMax) return;
  Ann& a = g.ann[g.nAnn++];
  a.card = static_cast<uint8_t>(cardId);
  a.kind = kind;
  std::snprintf(a.title, sizeof(a.title), "%s", title);
  std::snprintf(a.text, sizeof(a.text), "%s", text);
}

void toWaiting(Game& g, int id, const char* why) {
  takeFromGrid(g, id);
  g.card[id].faceUp = true;
  g.waiting.push(id);
  logLine(g, Kind::Good, "%s %s and waits aside for %s.", lab(g, id), why,
          g.card[id].red ? "the outie" : "the innie");
}

int caseyMains(const Game& g, Top* out) {
  Top all[kStacks];
  const int n = gridMains(g, all);
  int m = 0;
  for (int k = 0; k < n; ++k)
    if (!mateWaiting(g, cardDef(all[k].c).ch)) out[m++] = all[k];
  return m;
}

int revealedMains(const Game& g, uint8_t* out) {
  Top t[kStacks];
  const int n = gridMains(g, t);
  int m = 0;
  for (int k = 0; k < n; ++k) out[m++] = t[k].c;
  for (int k = 0; k < g.waiting.n; ++k) out[m++] = g.waiting.c[k];
  return m;
}

void removePair(Game& g, int index) {
  for (int k = index; k + 1 < g.nPaired; ++k) g.paired[k] = g.paired[k + 1];
  --g.nPaired;
}

void settle(Game& g);
void applySpecial(Game& g, int id, int at);

bool updateUnlocks(Game& g) {
  bool changed = false;
  int z1 = 0;
  for (int i = 0; i < 4; ++i)
    for (int j = 0; j < g.stacks[i].n; ++j)
      if (isNum(g.stacks[i].c[j])) ++z1;
  if (!g.unlocked[1] && z1 <= 2) {
    g.unlocked[1] = true;
    changed = true;
    logLine(g, Kind::Good, "Zone 2 unlocked: two number cards or fewer remain in Zone 1.");
  }
  for (int z = 2; z < 4; ++z) {
    if (g.unlocked[z] || !g.unlocked[z - 1]) continue;
    int n = 0;
    for (int i = (z - 1) * 4; i < z * 4; ++i)
      if (stackHidesCard(g, i)) ++n;
    if (n <= 1) {
      g.unlocked[z] = true;
      changed = true;
      logLine(g, Kind::Good, "Zone %d unlocked: at most one stack in Zone %d still hides a card.", z + 1, z);
    }
  }
  // No number card is face up anywhere: the next locked zone opens so play can go on.
  if (!changed && !g.dealing) {
    bool pendingFlip = false;
    for (int i = 0; i < kStacks; ++i) {
      const int c = g.stacks[i].top();
      if (c != kNone && !g.card[c].faceUp && g.unlocked[zoneOf(i)]) pendingFlip = true;
    }
    int z = kNone;
    for (int q = 0; q < 4; ++q)
      if (!g.unlocked[q]) {
        z = q;
        break;
      }
    Top tops[kStacks];
    const int nt = rawTops(g, tops);
    bool anyNum = false;
    for (int k = 0; k < nt; ++k) anyNum |= isNum(tops[k].c);
    if (!pendingFlip && z > 0 && !anyNum) {
      g.unlocked[z] = true;
      changed = true;
      logLine(g, Kind::Good, "Zone %d unlocked: no number card is left face up on the grid.", z + 1);
    }
  }
  return changed;
}

void rollGoat(Game& g, int id, char* how, int size) {
  uint8_t d[kCards];
  int n = 0;
  for (int k = 0; k < g.gone.n; ++k)
    if (isNum(g.gone.c[k]) && !cardDef(g.gone.c[k]).wild) d[n++] = g.gone.c[k];
  if (n) {
    const int s = d[g.rng.below(n)];
    g.card[id].val = g.card[s].val;
    std::snprintf(how, size, "a random card from the discard pile (%s)", lab(g, s));
    return;
  }
  g.card[id].val = static_cast<int8_t>(1 + g.rng.below(6));
  std::snprintf(how, size, "a die roll, as the discard pile is empty");
}

void onReveal(Game& g, int id, int i) {
  const CardDef d = cardDef(id);
  const int zone = zoneOf(i) + 1;
  if (d.wild) {
    char how[96];
    rollGoat(g, id, how, sizeof(how));
    logLine(g, Kind::Good,
            "Emile the Goat is revealed in Zone %d. He is worth %d, set by %s, and counts as green or blue, whichever "
            "you choose.",
            zone, g.card[id].val, how);
    return;
  }
  if (d.type == Type::Num) {
    if (!g.dealing) logLine(g, Kind::Plain, "Revealed %s in Zone %d.", lab(g, id), zone);
    return;
  }
  if (d.type == Type::Main) {
    logLine(g, Kind::Plain, "%s is revealed in Zone %d.", lab(g, id), zone);
    if (mateWaiting(g, d.ch)) return;  // stays on the grid; the player taps it to pair
    if (d.ch == 1) {
      toWaiting(g, id, "exits the stack by Free Will");
    } else if (g.hallPass) {
      toWaiting(g, id, "uses the Hall Pass");
    }
    return;
  }
  logLine(g, d.fx == Fx::Kier || d.fx == Fx::Casey ? Kind::Good : Kind::Bad, "%s is revealed in Zone %d.",
          cardName(id), zone);
  g.stacks[i].n--;
  g.gone.push(id);
  applySpecial(g, id, i);
}

bool revealOne(Game& g) {
  for (int i = 0; i < kStacks; ++i) {
    const int c = g.stacks[i].top();
    if (c != kNone && !g.card[c].faceUp && g.unlocked[zoneOf(i)]) {
      g.card[c].faceUp = true;
      onReveal(g, c, i);
      return true;
    }
  }
  return updateUnlocks(g);
}

void applySpecial(Game& g, const int id, const int at) {
  const CardDef d = cardDef(id);
  char text[kLogLen];
  Top tops[kStacks];
  switch (d.fx) {
    case Fx::Kier: {
      if (gridNums(g, tops)) {
        g.pending = Pending{Pend::Kier, 2, 0};
        say(g, id, "Lumon always provides!", "You may move up to two numerical cards away from their stacks.",
            Kind::Good, true);
      } else {
        say(g, id, "Lumon always provides!", "But no numerical card is face up. No effect.", Kind::Plain);
      }
      return;
    }
    case Fx::Casey: {
      const int ms = caseyMains(g, tops);
      Top nums[kStacks];
      const int n2 = gridNums(g, nums);
      const char* who = cardName(id);
      if (ms == 1) {
        const int m = tops[0].c;
        std::snprintf(text, sizeof(text), "%s escorts %s out of the grid to wait for %s.", who, lab(g, m),
                      g.card[m].red ? "the outie" : "the innie");
        say(g, id, "Wellness Session", text, Kind::Good);
        char why[64];
        std::snprintf(why, sizeof(why), "is escorted out by %s", who);
        toWaiting(g, m, why);
      } else if (ms) {
        g.pending = Pending{Pend::CaseyMain, 1, static_cast<uint8_t>(id)};
        std::snprintf(text, sizeof(text), "%s can set one unmatched character aside to wait for their pair.", who);
        say(g, id, "Wellness Session", text, Kind::Good, true);
      } else if (n2) {
        g.pending = Pending{Pend::CaseyNum, 1, static_cast<uint8_t>(id)};
        std::snprintf(text, sizeof(text),
                      "No unmatched character is on the grid, so %s can move one numerical card away.", who);
        say(g, id, "Wellness Session", text, Kind::Good, true);
      } else {
        std::snprintf(text, sizeof(text), "%s finds nobody to help. No effect.", who);
        say(g, id, "Wellness Session", text, Kind::Plain);
      }
      return;
    }
    case Fx::Drummond: {
      const int z = at < 0 ? -1 : zoneOf(at);
      auto zoneHasCards = [&](const int q) {
        for (int s = q * 4; s < q * 4 + 4; ++s)
          if (g.stacks[s].n) return true;
        return false;
      };
      int inUse = 0;
      for (int q = 0; q < 4; ++q)
        if (g.unlocked[q] && zoneHasCards(q)) ++inUse;
      const bool lock = z > 0 && inUse > 1 && zoneHasCards(z);
      Text t(text, sizeof(text));
      if (lock) {
        t.add("Zone %d is locked until your next refined pair. ", z + 1);
      } else if (z == 0) {
        t.add("He cannot lock Zone 1. ");
      } else if (z > 0) {
        t.add("He cannot lock the only zone in use. ");
      }
      t.add("The order of the draw pile and of every Zone 4 stack is reversed.");
      say(g, id, "Workplace Reforms", text, Kind::Bad);
      if (lock) g.lockZone = static_cast<int8_t>(z);
      for (int a = 0, b = g.draw.n - 1; a < b; ++a, --b) {
        const uint8_t x = g.draw.c[a];
        g.draw.c[a] = g.draw.c[b];
        g.draw.c[b] = x;
      }
      for (int i = 12; i < 16; ++i) {
        Stack& st = g.stacks[i];
        if (st.n > 1) {
          for (int a = 0, b = st.n - 1; a < b; ++a, --b) {
            const uint8_t x = st.c[a];
            st.c[a] = st.c[b];
            st.c[b] = x;
          }
          for (int k = 0; k < st.n; ++k) g.card[st.c[k]].faceUp = false;
        }
      }
      return;
    }
    case Fx::Graner: {
      uint8_t cand[kCards];
      int n = revealedMains(g, cand);
      for (int p = 0; p < g.nPaired; ++p)
        for (int k = 0; k < g.paired[p].n; ++k) cand[n++] = g.paired[p].cards[k];
      if (!n) {
        say(g, id, "Break Room", "No main character has been revealed yet. No effect.", Kind::Plain);
        return;
      }
      const int v = cand[g.rng.below(n)];
      const int ch = cardDef(v).ch;
      const bool wasPaired = isPaired(g, ch);
      Text t(text, sizeof(text));
      t.add("Graner sends %s face down to the bottom of the last stack in Zone 4.", lab(g, v));
      if (wasPaired) t.add(" The %s pair is broken; the other half waits aside.", charName(ch));
      say(g, id, "Break Room", text, Kind::Bad);
      if (wasPaired) {
        for (int p = 0; p < g.nPaired; ++p) {
          if (g.paired[p].ch != ch) continue;
          const Pair pr = g.paired[p];
          removePair(g, p);
          for (int k = 0; k < pr.n; ++k)
            if (pr.cards[k] != v) g.waiting.push(pr.cards[k]);
          break;
        }
      } else {
        takeFromGrid(g, v);
        g.waiting.remove(v);
      }
      g.card[v].faceUp = false;
      insertAt(g.stacks[15], 0, v);
      return;
    }
    case Fx::Dieter: {
      say(g, id, "Dieter Eagan",
          "Two cards are drawn from the draw pile. Number cards he draws stay in your hand until you refine them.",
          Kind::Good);
      if (g.nQueue + 2 <= static_cast<int>(sizeof(g.queue))) {
        std::memmove(g.queue + 2, g.queue, g.nQueue);
        g.queue[0] = 1;
        g.queue[1] = 2;
        g.nQueue += 2;
      }
      return;
    }
    case Fx::Jame: {
      Text t(text, sizeof(text));
      t.add("Helly's Defiance is cancelled for the rest of the quarter, and she cannot be paired until your next "
            "refined pair.");
      const bool m = g.seenMilchick;
      const bool c = g.seenCobel;
      if (m || c) {
        t.add(" %s%s%s ha%s already appeared, so their abilit%s again.", m ? "Seth Milchick" : "",
              m && c ? " and " : "", c ? "Harmony Cobel" : "", m && c ? "ve" : "s", m && c ? "ies play" : "y plays");
      }
      say(g, id, "Jame Eagan", text, Kind::Bad);
      g.defianceUsed = true;
      g.hellyPause = true;
      if (m && g.over == Over::No && g.pending.type == Pend::None) applySpecial(g, kMilchick, -1);
      if (c && g.over == Over::No && g.pending.type == Pend::None) applySpecial(g, kCobel, -1);
      return;
    }
    case Fx::Reghabi: {
      int hidden = 0;
      for (int s = 0; s < kStacks; ++s)
        if (stackHidesCard(g, s)) ++hidden;
      uint8_t rv[kCards];
      const int nrv = revealedMains(g, rv);
      if (!hidden) {
        say(g, id, "Reintegration", "No stack hides a card for Reghabi to search. No effect.", Kind::Plain);
        return;
      }
      g.pending = Pending{Pend::Reghabi, static_cast<uint8_t>(nrv ? 2 : 4), 0};
      say(g, id, "Reintegration",
          nrv ? "Choose up to two stacks. Reghabi searches their hidden cards and frees any match of a revealed main "
                "character."
              : "No main character is revealed, so choose up to four stacks. Reghabi frees the first hidden main "
                "character she finds.",
          Kind::Good, true);
      return;
    }
    case Fx::Cobel:
    case Fx::Milchick: {
      if (d.fx == Fx::Milchick) g.seenMilchick = true;
      if (d.fx == Fx::Cobel) g.seenCobel = true;
      int helly = kNone;
      for (int k = 0; k < g.waiting.n; ++k)
        if (cardDef(g.waiting.c[k]).ch == 1) {
          helly = g.waiting.c[k];
          break;
        }
      if (!g.defianceUsed && helly != kNone) {
        g.defianceUsed = true;
        std::snprintf(text, sizeof(text), "Helly cancels %s's effect. She cannot do this again.", cardName(id));
        say(g, id, "Defiance!", text, Kind::Good);
        return;
      }
      if (d.fx == Fx::Cobel) {
        if (g.nPaired) {
          const int pi = g.rng.below(g.nPaired);
          const Pair sp = g.paired[pi];
          int ks[2] = {pickStack(g), pickStack(g)};
          char where[80];
          stackList(ks, 2, where, sizeof(where));
          std::snprintf(text, sizeof(text), "Cobel splits the %s pair and hides both cards face down in %s.",
                        charName(sp.ch), where);
          say(g, id, "Reintegration Check", text, Kind::Bad);
          removePair(g, pi);
          for (int k = 0; k < sp.n; ++k) insertHidden(g, sp.cards[k], ks[k]);
          return;
        }
        const int nn = gridNums(g, tops);
        if (nn) {
          const Top o = tops[g.rng.below(nn)];
          std::snprintf(text, sizeof(text), "Nobody is paired, so Cobel blocks %s in Zone %d until your next refined pair.",
                        lab(g, o.c), zoneOf(o.i) + 1);
          say(g, id, "Kier, Chosen One, Kier!", text, Kind::Bad);
          g.blocked = static_cast<int8_t>(o.c);
        } else {
          say(g, id, "Kier, Chosen One, Kier!", "Cobel finds no pair to split and no numerical card to block. No effect.",
              Kind::Plain);
        }
        return;
      }
      // Milchick: the last refined cards go back into the grid.
      uint8_t cards[kCards];
      int n = 0;
      if (g.hasLastPair)
        for (int k = 0; k < g.lastPair.n; ++k)
          if (g.gone.has(g.lastPair.c[k])) cards[n++] = g.lastPair.c[k];
      if (!n) {
        say(g, id, "AGAIN!", "There is no refined pair to return. No effect.", Kind::Plain);
        return;
      }
      int dest[kCards] = {};
      if (g.lenient) {
        int open[kStacks];
        int no = 0;
        for (int s = 0; s < kStacks; ++s)
          if (g.unlocked[zoneOf(s)]) open[no++] = s;
        for (int k = 0; k < n; ++k) dest[k] = open[g.rng.below(no)];
      } else {
        for (int k = 0; k < n; ++k) dest[k] = pickStack(g);
      }
      Text t(text, sizeof(text));
      t.add("Milchick returns your last refined cards (");
      for (int k = 0; k < n; ++k) t.add("%s%s", k ? ", " : "", lab(g, cards[k]));
      char where[80];
      stackList(dest, n, where, sizeof(where));
      t.add(") %s to %s.", g.lenient ? "face up" : "face down", where);
      say(g, id, "AGAIN!", text, Kind::Bad);
      g.hasLastPair = false;
      g.lastPair.n = 0;
      for (int k = 0; k < n; ++k) {
        g.gone.remove(cards[k]);
        g.card[cards[k]].keep = false;
        if (g.lenient) {
          g.card[cards[k]].faceUp = true;
          pushTop(g.stacks[dest[k]], cards[k]);
        } else {
          insertHidden(g, cards[k], dest[k]);
        }
      }
      return;
    }
    case Fx::None:
      return;
  }
}

bool canReshuffle(const Game& g) {
  if (g.reshuffled) return false;
  for (int s = 8; s < 12; ++s)
    if (stackHidesCard(g, s)) return true;
  return false;
}

void reshuffle(Game& g) {
  uint8_t back[kCards];
  int n = 0;
  for (int k = 0; k < g.gone.n; ++k)
    if (cardDef(g.gone.c[k]).adm) back[n++] = g.gone.c[k];
  char text[kLogLen];
  Text t(text, sizeof(text));
  t.add("The draw pile ran out while Zone 3 still hides cards, so its %d used cards, Gemma among them, are reshuffled "
        "and used once more.",
        n);
  if (!g.hallPass)
    t.add(" After your next refined pair, main characters get the Hall Pass and exit their stacks when they appear.");
  say(g, kGemma, "Administration reshuffled", text, Kind::Good);
  for (int k = 0; k < n; ++k) {
    g.gone.remove(back[k]);
    g.card[back[k]].faceUp = false;
    g.card[back[k]].keep = false;
  }
  shuffle(g, back, n);
  g.draw.n = 0;
  for (int k = 0; k < n; ++k) g.draw.push(back[k]);
  g.reshuffled = true;
  if (!g.hallPass) g.hallPending = true;
}

void drawOne(Game& g, const int dieter) {
  char dt[32] = "";
  if (dieter) std::snprintf(dt, sizeof(dt), "Dieter's %s draw", dieter == 1 ? "first" : "second");
  if (!g.draw.n) {
    logLine(g, Kind::Plain, "%s%sthe draw pile is empty; nothing to draw.", dt, dieter ? ": " : "");
    return;
  }
  const int c = g.draw.pop();
  g.card[c].faceUp = true;
  if (isNum(c)) {
    g.hand.push(c);
    if (dieter) {
      g.card[c].keep = true;
      logLine(g, Kind::Good, "%s is %s. It stays in your hand until you refine it.", dt, lab(g, c));
    } else {
      logLine(g, Kind::Plain, "You drew %s.%s", lab(g, c),
              g.hand.n > 1 ? " It can be used together with Dieter's cards." : " Only this most recent draw can be used.");
    }
    return;
  }
  const Fx fx = cardDef(c).fx;
  const Kind kind = fx == Fx::Cobel || fx == Fx::Milchick || fx == Fx::Jame ? Kind::Bad : Kind::Good;
  if (dieter) {
    logLine(g, kind, "%s is %s, so its ability applies instead of a number card.", dt, lab(g, c));
  } else {
    logLine(g, kind, "You drew %s.", lab(g, c));
  }
  g.gone.push(c);
  applySpecial(g, c, -1);
}

void settle(Game& g) {
  for (;;) {
    if (g.over != Over::No) return;
    if (g.pending.type != Pend::None) return;
    if (revealOne(g)) continue;
    if (!g.draw.n && canReshuffle(g)) {
      reshuffle(g);
      continue;
    }
    if (g.nQueue) {
      const int q = g.queue[0];
      std::memmove(g.queue, g.queue + 1, --g.nQueue);
      drawOne(g, q);
      continue;
    }
    uint8_t pc[kChars];
    if (!hasValidPair(g) && !g.draw.n && !pairable(g, pc) && (g.lockZone != kNone || g.hellyPause)) {
      // Nothing else can be done, so the one-round restrictions end early.
      if (g.lockZone != kNone) {
        logLine(g, Kind::Plain, "No other move is possible, so Drummond's lock on Zone %d is lifted.", g.lockZone + 1);
        g.lockZone = kNone;
      }
      if (g.hellyPause) {
        logLine(g, Kind::Plain, "No other move is possible, so Helly may be paired again.");
        g.hellyPause = false;
      }
      continue;
    }
    break;
  }
  uint8_t pc[kChars];
  if (!hasValidPair(g) && !g.draw.n && !pairable(g, pc)) {
    g.over = Over::Lose;
    logLine(g, Kind::Bad, "No valid moves are left and the draw pile is empty. The quarter is lost.");
  }
}

void begin(Game& g) {
  ++g.turn;
  g.nAnn = 0;
}

}  // namespace

// ---- the card table ----

CardDef cardDef(const int id) {
  CardDef d{Type::Num, 0, false, false, false, 0, Fx::None, false};
  if (id < kAdminNums) {
    d.val = static_cast<int8_t>(2 + id / 4);
    d.red = id % 4 < 2;
    return d;
  }
  if (id < kMains) {
    int k = id - kAdminNums;
    for (int v = 1; v <= 10; ++v) {
      if (k < kAdmin[v]) {
        d.val = static_cast<int8_t>(v);
        d.red = k % 2 == 0;
        d.adm = true;
        return d;
      }
      k -= kAdmin[v];
    }
  }
  if (id < kGoat) {
    d.type = Type::Main;
    d.ch = static_cast<uint8_t>((id - kMains) / 2);
    d.red = (id - kMains) % 2 == 0;
    return d;
  }
  if (id == kGoat) {
    d.wild = true;
    d.red = true;
    return d;
  }
  d.type = Type::Special;
  switch (id) {
    case kGemma:
      d.fx = Fx::Casey;
      d.gemma = true;
      d.adm = true;
      break;
    case kDieter:
      d.fx = Fx::Dieter;
      d.adm = true;
      break;
    case kGraner:
      d.fx = Fx::Graner;
      d.adm = true;
      break;
    case kMilchick:
      d.fx = Fx::Milchick;
      d.adm = true;
      break;
    case kCobel:
      d.fx = Fx::Cobel;
      d.adm = true;
      break;
    case kReghabi:
      d.fx = Fx::Reghabi;
      d.adm = true;
      break;
    case kJame:
      d.fx = Fx::Jame;
      d.adm = true;
      break;
    case kDrummond:
      d.fx = Fx::Drummond;
      break;
    case kCasey:
      d.fx = Fx::Casey;
      break;
    default:
      d.fx = Fx::Kier;
      break;
  }
  return d;
}

const char* charName(const int ch) { return kCharNames[ch & 3]; }

const char* cardName(const int id) {
  const CardDef d = cardDef(id);
  if (d.type == Type::Main) return d.ch == 1 && !d.red ? "Helena" : charName(d.ch);
  if (d.wild) return "Emile";
  if (d.gemma) return "Gemma";
  switch (d.fx) {
    case Fx::Kier:
      return "Kier Eagan";
    case Fx::Dieter:
      return "Dieter Eagan";
    case Fx::Casey:
      return "Ms. Casey";
    case Fx::Drummond:
      return "Mr. Drummond";
    case Fx::Graner:
      return "Doug Graner";
    case Fx::Milchick:
      return "Seth Milchick";
    case Fx::Cobel:
      return "Harmony Cobel";
    case Fx::Reghabi:
      return "Reghabi";
    case Fx::Jame:
      return "Jame Eagan";
    default:
      return "";
  }
}

const char* ability(const int id) {
  const CardDef d = cardDef(id);
  if (d.gemma) return "WELLNESS";
  switch (d.fx) {
    case Fx::Kier:
      return "PROVIDES";
    case Fx::Dieter:
      return "DRAW TWO";
    case Fx::Casey:
      return "WELLNESS";
    case Fx::Drummond:
      return "REFORMS";
    case Fx::Graner:
      return "BREAK ROOM";
    case Fx::Milchick:
      return "AGAIN!";
    case Fx::Cobel:
      return "CHECK";
    case Fx::Reghabi:
      return "SEARCH";
    case Fx::Jame:
      return "FOUNDER";
    default:
      return "";
  }
}

bool isBad(const int id) {
  const Fx fx = cardDef(id).fx;
  return fx == Fx::Cobel || fx == Fx::Milchick || fx == Fx::Drummond || fx == Fx::Graner || fx == Fx::Jame;
}

void label(const Game& g, const int id, char* out, const int size) {
  const CardDef d = cardDef(id);
  const CardState& s = g.card[id];
  if (d.wild) {
    if (s.faceUp && s.val) {
      std::snprintf(out, size, "Emile the Goat (worth %d)", s.val);
    } else {
      std::snprintf(out, size, "Emile the Goat");
    }
  } else if (d.type == Type::Num) {
    std::snprintf(out, size, "%d %s", s.val, s.red ? "green" : "blue");
  } else if (d.type == Type::Main) {
    std::snprintf(out, size, "%s (%s)", cardName(id), d.red ? "innie" : "outie");
  } else {
    std::snprintf(out, size, "%s", cardName(id));
  }
}

bool List::has(const int id) const {
  for (int k = 0; k < n; ++k)
    if (c[k] == id) return true;
  return false;
}
bool List::remove(const int id) {
  for (int k = 0; k < n; ++k) {
    if (c[k] != id) continue;
    std::memmove(c + k, c + k + 1, static_cast<size_t>(n - k - 1));
    --n;
    return true;
  }
  return false;
}
void List::insertFront(const int id) {
  std::memmove(c + 1, c, n);
  c[0] = static_cast<uint8_t>(id);
  ++n;
}

// ---- a new quarter ----

void newGame(Game& g, const uint32_t seed, const bool lenient) {
  // Reset in place: a Game is several kilobytes, too big for a temporary on
  // the device's stack.
  std::memset(static_cast<void*>(&g), 0, sizeof(Game));
  g.blocked = kNone;
  g.lockZone = kNone;
  g.rng.seed(seed);
  g.lenient = lenient;
  for (int id = 0; id < kCards; ++id) {
    const CardDef d = cardDef(id);
    g.card[id] = CardState{d.val, d.red, false, false};
  }
  uint8_t draw[kCards];
  int nd = 0;
  for (int id = kAdminNums; id < kMains; ++id) draw[nd++] = static_cast<uint8_t>(id);
  for (const int id : {kDieter, kGemma, kGraner, kMilchick, kCobel, kReghabi, kJame}) draw[nd++] = static_cast<uint8_t>(id);
  shuffle(g, draw, nd);
  for (int k = 0; k < nd; ++k) g.draw.push(draw[k]);
  uint8_t rest[kCards];
  int nr = 0;
  for (int id = kGridNums; id < kAdminNums; ++id) rest[nr++] = static_cast<uint8_t>(id);
  for (int id = kMains; id < kGoat; ++id) rest[nr++] = static_cast<uint8_t>(id);
  for (const int id : {kGoat, kDrummond, kCasey}) rest[nr++] = static_cast<uint8_t>(id);
  shuffle(g, rest, nr);
  pushTop(g.stacks[kKierStack], kKier);  // Kier is placed first, at position 10
  for (int i = 0; i < kStacks; ++i)
    while (g.stacks[i].n < kLayers[zoneOf(i)]) pushTop(g.stacks[i], rest[--nr]);
  g.unlocked[0] = true;
  g.dealing = true;
  logLine(g, Kind::Plain, "A new quarter begins. Zone 1 is open; Zones 2, 3 and 4 are locked.");
  settle(g);
  g.dealing = false;
}

// ---- queries ----

int topOf(const Game& g, const int stack) { return g.stacks[stack].top(); }
bool inLockedZone(const Game& g, const int stack) { return zoneOf(stack) == g.lockZone; }

int usableNums(const Game& g, uint8_t* out) {
  Top t[kStacks];
  const int n = gridNums(g, t);
  int m = 0;
  for (int k = 0; k < n; ++k) out[m++] = t[k].c;
  for (int k = 0; k < g.hand.n; ++k) out[m++] = g.hand.c[k];
  return m;
}

bool isUsable(const Game& g, const int id) {
  uint8_t u[kCards];
  const int n = usableNums(g, u);
  for (int k = 0; k < n; ++k)
    if (u[k] == id) return true;
  return false;
}

bool hasValidPair(const Game& g) {
  uint8_t u[kCards];
  const int n = usableNums(g, u);
  int wild = kNone;
  for (int k = 0; k < n; ++k)
    if (cardDef(u[k]).wild) wild = u[k];
  const int tries = wild != kNone ? 2 : 1;
  for (int t = 0; t < tries; ++t) {
    int r[kCards], b[kCards];
    int nr = 0, nb = 0;
    for (int k = 0; k < n; ++k) {
      const bool side = cardDef(u[k]).wild ? t == 0 : g.card[u[k]].red;
      (side ? r[nr++] : b[nb++]) = g.card[u[k]].val;
    }
    const Sums sr = sums(r, nr);
    const Sums sb = sums(b, nb);
    for (int q = 0; q < 4; ++q)
      if (sr.b[q] & sb.b[q]) return true;
  }
  return false;
}

bool canDraw(const Game& g) { return g.over == Over::No && g.pending.type == Pend::None && g.draw.n > 0; }

bool isPaired(const Game& g, const int ch) {
  for (int p = 0; p < g.nPaired; ++p)
    if (g.paired[p].ch == ch) return true;
  return false;
}

int mainCards(const Game& g, const int ch, uint8_t* out) {
  Top t[kStacks];
  const int n = gridMains(g, t);
  int m = 0;
  for (int k = 0; k < n; ++k)
    if (cardDef(t[k].c).ch == ch) out[m++] = t[k].c;
  for (int k = 0; k < g.waiting.n; ++k)
    if (cardDef(g.waiting.c[k]).ch == ch) out[m++] = g.waiting.c[k];
  return m;
}

bool mateWaiting(const Game& g, const int ch) {
  for (int k = 0; k < g.waiting.n; ++k)
    if (cardDef(g.waiting.c[k]).ch == ch) return true;
  return false;
}

int pairable(const Game& g, uint8_t* chars) {
  int n = 0;
  for (int ch = 0; ch < kChars; ++ch) {
    if (isPaired(g, ch) || (ch == 1 && g.hellyPause)) continue;
    uint8_t m[kCards];
    if (mainCards(g, ch, m) == 2) chars[n++] = static_cast<uint8_t>(ch);
  }
  return n;
}

bool isPairCard(const Game& g, const int id) {
  if (cardDef(id).type != Type::Main) return false;
  uint8_t chars[kChars];
  const int n = pairable(g, chars);
  for (int k = 0; k < n; ++k) {
    if (chars[k] != cardDef(id).ch) continue;
    uint8_t m[kCards];
    const int nm = mainCards(g, chars[k], m);
    for (int q = 0; q < nm; ++q)
      if (m[q] == id) return true;
  }
  return false;
}

Totals selectionTotals(const Game& g, const uint8_t* ids, const int n) {
  Totals t{0, 0, false};
  int nr = 0, nb = 0;
  uint8_t u[kCards];
  const int nu = usableNums(g, u);
  for (int k = 0; k < nu; ++k) {
    bool in = false;
    for (int q = 0; q < n; ++q) in |= ids[q] == u[k];
    if (!in) continue;
    if (g.card[u[k]].red) {
      t.green += g.card[u[k]].val;
      ++nr;
    } else {
      t.blue += g.card[u[k]].val;
      ++nb;
    }
  }
  t.ok = nr > 0 && nb > 0 && t.green == t.blue;
  return t;
}

bool priorityFix(const Game& g, uint8_t* ids, const int n) {
  Top gn[kStacks];
  const int ng = gridNums(g, gn);
  bool changed = false;
  auto in = [&](const int id) {
    for (int q = 0; q < n; ++q)
      if (ids[q] == id) return q;
    return -1;
  };
  for (int k = 0; k < ng; ++k) {
    const int o = gn[k].c;
    const int at = in(o);
    if (cardDef(o).wild || at < 0) continue;
    for (int f = 0; f < ng; ++f) {
      const int q = gn[f].c;
      if (cardDef(q).wild || g.card[q].val != g.card[o].val || g.card[q].red != g.card[o].red || in(q) >= 0 ||
          gn[f].i >= gn[k].i)
        continue;
      ids[at] = static_cast<uint8_t>(q);
      changed = true;
      break;
    }
  }
  return changed;
}

bool stackHidesCard(const Game& g, const int stack) {
  const Stack& s = g.stacks[stack];
  for (int k = 0; k < s.n; ++k)
    if (!g.card[s.c[k]].faceUp) return true;
  return false;
}

int findStack(const Game& g, const int id) {
  for (int i = 0; i < kStacks; ++i)
    for (int k = 0; k < g.stacks[i].n; ++k)
      if (g.stacks[i].c[k] == id) return i;
  return kNone;
}

int pendingCandidates(const Game& g, uint8_t* out) {
  int n = 0;
  Top t[kStacks];
  switch (g.pending.type) {
    case Pend::None:
      return 0;
    case Pend::Reghabi:
      for (int s = 0; s < kStacks; ++s)
        if (stackHidesCard(g, s)) out[n++] = static_cast<uint8_t>(s);
      return n;
    case Pend::CaseyMain: {
      const int m = caseyMains(g, t);
      for (int k = 0; k < m; ++k) out[n++] = t[k].c;
      return n;
    }
    default: {
      const int m = gridNums(g, t);
      for (int k = 0; k < m; ++k) out[n++] = t[k].c;
      return n;
    }
  }
}

// ---- player actions ----

bool actPair(Game& g, const int ch) {
  if (g.over != Over::No || g.pending.type != Pend::None) return false;
  uint8_t pc[kChars];
  const int np = pairable(g, pc);
  bool ok = false;
  for (int k = 0; k < np; ++k) ok |= pc[k] == ch;
  if (!ok) return false;
  begin(g);
  uint8_t found[kCards];
  const int nf = mainCards(g, ch, found);
  Pair p{static_cast<uint8_t>(ch), {0, 0}, 0};
  for (int k = 0; k < nf && k < 2; ++k) {
    takeFromGrid(g, found[k]);
    g.waiting.remove(found[k]);
    g.card[found[k]].faceUp = true;
    p.cards[p.n++] = found[k];
  }
  g.paired[g.nPaired++] = p;
  logLine(g, Kind::Good, "%s is paired: innie and outie are reunited (%d of 4).", charName(ch), g.nPaired);
  if (g.nPaired == kChars) {
    g.over = Over::Win;
    g.nQueue = 0;
    logLine(g, Kind::Good, "You hit quota! All main characters are paired.");
    return true;
  }
  settle(g);
  return true;
}

bool actDraw(Game& g) {
  if (!canDraw(g)) return false;
  begin(g);
  uint8_t drop[kCards];
  int nd = 0;
  for (int k = 0; k < g.hand.n; ++k)
    if (!g.card[g.hand.c[k]].keep) drop[nd++] = g.hand.c[k];
  if (nd) {
    char text[kLogLen];
    Text t(text, sizeof(text));
    for (int k = 0; k < nd; ++k) t.add("%s%s", k ? ", " : "", lab(g, drop[k]));
    logLine(g, Kind::Plain, "The unused draw (%s) is discarded.", text);
    for (int k = 0; k < nd; ++k) {
      g.hand.remove(drop[k]);
      g.gone.push(drop[k]);
    }
  }
  drawOne(g, 0);
  settle(g);
  return true;
}

bool actRefine(Game& g, const uint8_t* idsIn, const int n) {
  if (g.over != Over::No || g.pending.type != Pend::None || n <= 0 || n > kCards) return false;
  const Totals t = selectionTotals(g, idsIn, n);
  uint8_t ids[kCards];
  std::memcpy(ids, idsIn, n);
  if (!t.ok || priorityFix(g, ids, n)) return false;
  begin(g);
  uint8_t u[kCards], cards[kCards];
  const int nu = usableNums(g, u);
  int nc = 0;
  for (int k = 0; k < nu; ++k)
    for (int q = 0; q < n; ++q)
      if (ids[q] == u[k]) {
        cards[nc++] = u[k];
        break;
      }
  char reds[kLogLen], blues[kLogLen];
  Text tr(reds, sizeof(reds)), tb(blues, sizeof(blues));
  for (int k = 0; k < nc; ++k) {
    Text& x = g.card[cards[k]].red ? tr : tb;
    x.add("%s%s", x.n ? " + " : "", lab(g, cards[k]));
  }
  g.lastPair.n = 0;
  for (int k = 0; k < nc; ++k) {
    if (!takeFromGrid(g, cards[k])) g.hand.remove(cards[k]);
    g.card[cards[k]].keep = false;
    g.gone.push(cards[k]);
    g.lastPair.push(cards[k]);
  }
  g.hasLastPair = true;
  logLine(g, Kind::Good, "Refined %s with %s (both total %d).", reds, blues, t.green);
  if (g.blocked != kNone) {
    g.blocked = kNone;
    logLine(g, Kind::Good, "Cobel steps away; her blocked card is free again.");
  }
  if (g.lockZone != kNone) {
    logLine(g, Kind::Good, "Drummond's lock on Zone %d ends.", g.lockZone + 1);
    g.lockZone = kNone;
  }
  if (g.hellyPause) {
    g.hellyPause = false;
    logLine(g, Kind::Good, "Helly may be paired again.");
  }
  if (g.hallPending && !g.hallPass) {
    g.hallPass = true;
    g.hallPending = false;
    logLine(g, Kind::Good, "Hall Pass is now active.");
    Top m[kStacks];
    const int nm = gridMains(g, m);
    for (int k = 0; k < nm; ++k)
      if (!mateWaiting(g, cardDef(m[k].c).ch)) toWaiting(g, m[k].c, "uses the Hall Pass");
  }
  settle(g);
  return true;
}

bool actResolve(Game& g, const uint8_t* idsIn, const int nIn) {
  const Pending p = g.pending;
  if (p.type == Pend::None || g.over != Over::No) return false;
  uint8_t cand[kCards];
  const int nc = pendingCandidates(g, cand);
  uint8_t ids[kCards];
  int n = 0;
  for (int k = 0; k < nIn && n < p.max; ++k)
    for (int q = 0; q < nc; ++q)
      if (cand[q] == idsIn[k]) {
        ids[n++] = idsIn[k];
        break;
      }
  begin(g);
  g.pending = Pending{};
  char who[48];
  if (p.type == Pend::Kier) {
    std::snprintf(who, sizeof(who), "Kier's offer");
  } else if (p.type == Pend::Reghabi) {
    std::snprintf(who, sizeof(who), "Reghabi's search");
  } else {
    std::snprintf(who, sizeof(who), "%s's session", cardName(p.who));
  }
  if (!n) {
    logLine(g, Kind::Plain, "%s is declined.", who);
  } else if (p.type == Pend::Reghabi) {
    uint8_t rv[kCards];
    const int nrv = revealedMains(g, rv);
    bool want[kChars] = {};
    for (int k = 0; k < nrv; ++k) want[cardDef(rv[k]).ch] = true;
    const bool anyMode = nrv == 0;
    uint8_t freed[kCards];
    int nf = 0;
    for (int k = 0; k < n; ++k) {
      const Stack& st = g.stacks[ids[k]];
      for (int q = 0; q < st.n; ++q) {
        const int x = st.c[q];
        if (g.card[x].faceUp || !isMain(x)) continue;
        if (anyMode ? nf == 0 : want[cardDef(x).ch]) freed[nf++] = static_cast<uint8_t>(x);
      }
    }
    if (!nf)
      logLine(g, Kind::Plain, "Reghabi searches %d stack%s and finds no %s.", n, n > 1 ? "s" : "",
              anyMode ? "main character" : "match");
    for (int k = 0; k < nf; ++k) toWaiting(g, freed[k], "is found by Reghabi");
  } else if (p.type == Pend::Kier || p.type == Pend::CaseyNum) {
    char names[kLogLen];
    Text t(names, sizeof(names));
    for (int k = 0; k < n; ++k) {
      takeFromGrid(g, ids[k]);
      g.gone.push(ids[k]);
      t.add("%s%s", k ? " and " : "", lab(g, ids[k]));
    }
    logLine(g, Kind::Good, "%s moves away %s.", p.type == Pend::Kier ? "Kier" : cardName(p.who), names);
  } else {
    char why[64];
    std::snprintf(why, sizeof(why), "is escorted out by %s", cardName(p.who));
    toWaiting(g, ids[0], why);
  }
  settle(g);
  return true;
}

void pendingText(const Game& g, char* title, const int titleSize, char* text, const int textSize) {
  const Pending& p = g.pending;
  switch (p.type) {
    case Pend::Kier:
      std::snprintf(title, titleSize, "Lumon always provides!");
      std::snprintf(text, textSize, "Tap up to two numerical cards for Kier to move away, then Confirm. Or Skip.");
      return;
    case Pend::CaseyMain:
      std::snprintf(title, titleSize, "Wellness Session");
      std::snprintf(text, textSize, "Tap one unmatched character for %s to set aside, then Confirm. Or Skip.",
                    cardName(p.who));
      return;
    case Pend::CaseyNum:
      std::snprintf(title, titleSize, "Wellness Session");
      std::snprintf(text, textSize, "No unmatched character is on the grid. Tap one numerical card for %s to move away, then Confirm. Or Skip.",
                    cardName(p.who));
      return;
    case Pend::Reghabi:
      std::snprintf(title, titleSize, "Reintegration");
      if (p.max == 2) {
        std::snprintf(text, textSize,
                      "Tap up to two stacks. Reghabi frees any hidden match of a revealed main character. Then Confirm, or Skip.");
      } else {
        std::snprintf(text, textSize,
                      "No main character is revealed. Tap up to four stacks; Reghabi frees the first hidden one she finds. Then Confirm, or Skip.");
      }
      return;
    case Pend::None:
      title[0] = text[0] = '\0';
      return;
  }
}

int cardCount(const Game& g) {
  int n = g.draw.n + g.hand.n + g.waiting.n + g.gone.n;
  for (int i = 0; i < kStacks; ++i) n += g.stacks[i].n;
  for (int p = 0; p < g.nPaired; ++p) n += g.paired[p].n;
  return n;
}

// ---- save file ----
//
// S1 rng turn over flags... then the card states, the stacks, the piles, the
// pairs and the queue as runs of numbers, then the log, one line each.

namespace {
struct Writer {
  char* out;
  int size;
  int n = 0;
  bool ok = true;
  void num(const long v) {
    if (!ok) return;
    const int w = std::snprintf(out + n, size - n, "%ld ", v);
    if (w < 0 || w >= size - n) {
      ok = false;
      return;
    }
    n += w;
  }
  void list(const uint8_t* c, const int count) {
    num(count);
    for (int k = 0; k < count; ++k) num(c[k]);
  }
  void line(const char* s) {
    if (!ok) return;
    const int w = std::snprintf(out + n, size - n, "%s\n", s);
    if (w < 0 || w >= size - n) {
      ok = false;
      return;
    }
    n += w;
  }
};
struct Reader {
  const char* p;
  bool ok = true;
  long num(const long lo, const long hi) {
    if (!ok) return lo;
    char* end = nullptr;
    const long v = std::strtol(p, &end, 10);
    if (end == p || v < lo || v > hi) {
      ok = false;
      return lo;
    }
    p = end;
    return v;
  }
  int list(uint8_t* c, const int cap) {
    const int n = static_cast<int>(num(0, cap));
    for (int k = 0; k < n && ok; ++k) c[k] = static_cast<uint8_t>(num(0, kCards - 1));
    return ok ? n : 0;
  }
};
}  // namespace

int serialize(const Game& g, char* out, const int size) {
  Writer w{out, size};
  w.line("S1");
  w.num(g.rng.s);
  w.num(g.turn);
  w.num(static_cast<int>(g.over));
  w.num(g.lenient);
  w.num(g.unlocked[1] | g.unlocked[2] << 1 | g.unlocked[3] << 2);
  w.num(g.blocked);
  w.num(g.lockZone);
  w.num(g.hellyPause | g.hallPending << 1 | g.hallPass << 2 | g.defianceUsed << 3 | g.reshuffled << 4 |
        g.seenMilchick << 5 | g.seenCobel << 6 | g.hasLastPair << 7);
  w.num(static_cast<int>(g.pending.type));
  w.num(g.pending.max);
  w.num(g.pending.who);
  for (int id = 0; id < kCards; ++id) {
    const CardState& c = g.card[id];
    w.num(c.val);
    w.num(c.red | c.faceUp << 1 | c.keep << 2);
  }
  for (int i = 0; i < kStacks; ++i) w.list(g.stacks[i].c, g.stacks[i].n);
  for (const List* l : {&g.draw, &g.hand, &g.waiting, &g.gone, &g.lastPair}) w.list(l->c, l->n);
  w.num(g.nPaired);
  for (int p = 0; p < g.nPaired; ++p) {
    w.num(g.paired[p].ch);
    w.list(g.paired[p].cards, g.paired[p].n);
  }
  w.list(g.queue, g.nQueue);
  w.num(g.nLog);
  w.line("");
  for (int k = 0; k < g.nLog; ++k) {
    char head[16];
    std::snprintf(head, sizeof(head), "%u %d ", g.log[k].turn, static_cast<int>(g.log[k].kind));
    if (!w.ok) break;
    const int x = std::snprintf(out + w.n, size - w.n, "%s%s\n", head, g.log[k].text);
    if (x < 0 || x >= size - w.n) {
      w.ok = false;
      break;
    }
    w.n += x;
  }
  return w.ok ? w.n : -1;
}

bool parse(const char* in, Game& out) {
  if (std::strncmp(in, "S1\n", 3) != 0) return false;
  Reader r{in + 3};
  Game* gp = new (std::nothrow) Game;
  if (!gp) return false;
  std::memset(static_cast<void*>(gp), 0, sizeof(Game));
  Game& g = *gp;
  g.rng.s = static_cast<uint32_t>(r.num(0, 0xFFFFFFFFL));
  g.turn = static_cast<uint16_t>(r.num(0, 65535));
  g.over = static_cast<Over>(r.num(0, 2));
  g.lenient = r.num(0, 1) != 0;
  const long u = r.num(0, 7);
  g.unlocked[0] = true;
  g.unlocked[1] = u & 1;
  g.unlocked[2] = u & 2;
  g.unlocked[3] = u & 4;
  g.blocked = static_cast<int8_t>(r.num(-1, kCards - 1));
  g.lockZone = static_cast<int8_t>(r.num(-1, 3));
  const long f = r.num(0, 255);
  g.hellyPause = f & 1;
  g.hallPending = f & 2;
  g.hallPass = f & 4;
  g.defianceUsed = f & 8;
  g.reshuffled = f & 16;
  g.seenMilchick = f & 32;
  g.seenCobel = f & 64;
  g.hasLastPair = f & 128;
  g.pending.type = static_cast<Pend>(r.num(0, 4));
  g.pending.max = static_cast<uint8_t>(r.num(0, 4));
  g.pending.who = static_cast<uint8_t>(r.num(0, kCards - 1));
  for (int id = 0; id < kCards; ++id) {
    g.card[id].val = static_cast<int8_t>(r.num(0, 10));
    const long b = r.num(0, 7);
    g.card[id].red = b & 1;
    g.card[id].faceUp = b & 2;
    g.card[id].keep = b & 4;
  }
  for (int i = 0; i < kStacks; ++i) g.stacks[i].n = static_cast<uint8_t>(r.list(g.stacks[i].c, kStackCap));
  for (List* l : {&g.draw, &g.hand, &g.waiting, &g.gone, &g.lastPair}) l->n = static_cast<uint8_t>(r.list(l->c, kCards));
  g.nPaired = static_cast<uint8_t>(r.num(0, kChars));
  for (int p = 0; p < g.nPaired; ++p) {
    g.paired[p].ch = static_cast<uint8_t>(r.num(0, kChars - 1));
    g.paired[p].n = static_cast<uint8_t>(r.list(g.paired[p].cards, 2));
  }
  g.nQueue = static_cast<uint8_t>(r.list(g.queue, 8));
  const int nLog = static_cast<int>(r.num(0, kLogLines));
  const bool ok = r.ok && cardCount(g) == kCards;
  if (ok) {
    const char* p = std::strchr(r.p, '\n');
    for (int k = 0; k < nLog && p && p[1]; ++k) {
      ++p;  // start of the line
      char* end = nullptr;
      const long turn = std::strtol(p, &end, 10);
      if (end == p) break;
      const long kind = std::strtol(end, &end, 10);
      const char* text = end;
      if (*text == ' ') ++text;
      const char* nl = std::strchr(text, '\n');
      const int len = nl ? static_cast<int>(nl - text) : static_cast<int>(std::strlen(text));
      LogLine& l = g.log[g.nLog++];
      l.turn = static_cast<uint16_t>(turn);
      l.kind = static_cast<Kind>(kind < 0 || kind > 2 ? 0 : kind);
      std::snprintf(l.text, sizeof(l.text), "%.*s", len < kLogLen - 1 ? len : kLogLen - 1, text);
      p = nl;
    }
    out = g;
  }
  delete gp;
  return ok;
}

}  // namespace sev
