// Plays seeded quarters with a fixed policy and prints the state after every
// action. phone_trace.js does the same with the phone version's engine; the two
// outputs must match line for line (see run.sh).
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "SeveranceCore.h"

using namespace sev;

static std::string state(const Game& g, int& logSeen) {
  std::string s;
  char b[64];
  for (int i = 0; i < kStacks; ++i) {
    s += "|";
    for (int k = 0; k < g.stacks[i].n; ++k) {
      const int c = g.stacks[i].c[k];
      std::snprintf(b, sizeof(b), "%d%s,", c, g.card[c].faceUp ? "u" : "");
      s += b;
    }
  }
  auto list = [&](const char* name, const List& l) {
    s += name;
    for (int k = 0; k < l.n; ++k) {
      std::snprintf(b, sizeof(b), "%d,", l.c[k]);
      s += b;
    }
  };
  list(" D:", g.draw);
  list(" H:", g.hand);
  list(" W:", g.waiting);
  list(" G:", g.gone);
  s += " P:";
  for (int p = 0; p < g.nPaired; ++p) {
    std::snprintf(b, sizeof(b), "%d=", g.paired[p].ch);
    s += b;
    for (int k = 0; k < g.paired[p].n; ++k) {
      std::snprintf(b, sizeof(b), "%d,", g.paired[p].cards[k]);
      s += b;
    }
    s += ";";
  }
  std::snprintf(b, sizeof(b), " U:%d%d%d%d B:%d L:%d F:%d%d%d%d%d K:%d,%d O:%d V:%d%d", g.unlocked[0], g.unlocked[1],
                g.unlocked[2], g.unlocked[3], g.blocked, g.lockZone, g.hellyPause, g.hallPending, g.hallPass,
                g.defianceUsed, g.reshuffled, static_cast<int>(g.pending.type), g.pending.max, static_cast<int>(g.over),
                g.card[70].val, g.card[70].red);
  s += b;
  s += "\n";
  // Log lines since the last step: the log is a ring, so count from the end.
  for (int k = 0; k < g.nLog; ++k) {
    if (g.log[k].turn < logSeen) continue;
    s += "  ";
    s += g.log[k].text;
    s += "\n";
  }
  logSeen = g.turn + 1;
  return s;
}

int main(int argc, char** argv) {
  const int seeds = argc > 1 ? std::atoi(argv[1]) : 50;
  const bool lenient = argc > 2 && argv[2][0] == '1';
  static Game g;
  for (int seed = 1; seed <= seeds; ++seed) {
    newGame(g, static_cast<uint32_t>(seed * 2654435761u), lenient);
    int logSeen = 0;
    std::printf("SEED %d\nstart%s", seed, state(g, logSeen).c_str());
    for (int step = 0; step < 600 && g.over == Over::No; ++step) {
      char act[64];
      if (g.pending.type != Pend::None) {
        uint8_t cand[kCards];
        const int n = pendingCandidates(g, cand);
        const int take = g.turn % 4 == 3 ? 0 : (n < g.pending.max ? n : g.pending.max);
        std::snprintf(act, sizeof(act), "resolve %d", take);
        actResolve(g, cand, take);
      } else {
        uint8_t pc[kChars];
        if (pairable(g, pc)) {
          std::snprintf(act, sizeof(act), "pair %d", pc[0]);
          actPair(g, pc[0]);
        } else {
          uint8_t u[kCards];
          const int nu = usableNums(g, u);
          bool done = false;
          for (int size = 2; size <= 4 && !done; ++size) {
            int ix[4];
            for (int k = 0; k < size; ++k) ix[k] = k;
            while (!done) {
              if (ix[size - 1] >= nu) break;
              uint8_t ids[4];
              int wild = -1;
              for (int k = 0; k < size; ++k) {
                ids[k] = u[ix[k]];
                if (cardDef(ids[k]).wild) wild = ids[k];
              }
              for (int t = 0; t < (wild >= 0 ? 2 : 1) && !done; ++t) {
                if (wild >= 0) g.card[wild].red = t == 0;
                if (selectionTotals(g, ids, size).ok) {
                  priorityFix(g, ids, size);
                  std::snprintf(act, sizeof(act), "refine");
                  actRefine(g, ids, size);
                  done = true;
                }
              }
              if (!done && wild >= 0) g.card[wild].red = true;
              // next combination
              int k = size - 1;
              while (k >= 0 && ix[k] == nu - size + k) --k;
              if (k < 0) break;
              ++ix[k];
              for (int q = k + 1; q < size; ++q) ix[q] = ix[q - 1] + 1;
            }
          }
          if (!done) {
            if (!canDraw(g)) break;
            std::snprintf(act, sizeof(act), "draw");
            actDraw(g);
          }
        }
      }
      // The save file must give back the same game.
      static char a[16384], b[16384];
      static Game back;
      const int na = serialize(g, a, sizeof(a));
      if (na < 0 || !parse(a, back) || serialize(back, b, sizeof(b)) != na || std::memcmp(a, b, na) != 0) {
        std::fprintf(stderr, "save round trip failed at seed %d step %d (%d bytes)\n", seed, step, na);
        return 1;
      }
      std::printf("%s%s", act, state(g, logSeen).c_str());
    }
    std::printf("END %d\n", static_cast<int>(g.over));
  }
  return 0;
}
