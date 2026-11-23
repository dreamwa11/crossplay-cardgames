#include "SeveranceScreens.h"

#include <cstdio>
#include <cstring>
#include <initializer_list>

namespace sevui {

namespace {

namespace s = sev;

const fui::Paint kBlack = fui::Paint::solid(fui::Color::Black);
const fui::Paint kWhite = fui::Paint::solid(fui::Color::White);
const fui::Paint kGrey = fui::Paint::dither(fui::Color::DarkGray);
const fui::Paint kLight = fui::Paint::dither(fui::Color::LightGray);
constexpr fui::FontId kTextSlot = 3;
FontBinder* gFonts = nullptr;

// ---- geometry ----

constexpr int kW = 480;
constexpr int kTopH = 48;
constexpr int kTrackY = 56;
constexpr int kTrackH = 66;
constexpr int kBoxW = 107;
constexpr int kBoxGap = 6;
constexpr int kMiniW = 44;
constexpr int kMiniH = 36;
constexpr int kCardW = 96;
constexpr int kCardH = 92;
constexpr int kCardGap = 6;
constexpr int kZoneW = 2 * kCardW + kCardGap;
constexpr int kZoneGapX = 16;
constexpr int kGridX = (kW - 2 * kZoneW - kZoneGapX) / 2;
constexpr int kGridY = 130;
constexpr int kZoneLabelH = 22;
constexpr int kZoneH = kZoneLabelH + 2 * kCardH + kCardGap;
constexpr int kZoneGapY = 12;
// The message bar, then the tray, each with air around it. The draw pile is on
// the right, under the right thumb; the drawn cards on the left.
constexpr int kStatusY = kGridY + 2 * kZoneH + kZoneGapY + 14;  // 580
constexpr int kTrayY = kStatusY + 64;
constexpr int kTrayCardY = kTrayY + 6;
constexpr int kTrayCardW = 62;
constexpr int kTrayCardH = 72;
constexpr int kDrawX = 480 - 16 - kTrayCardW - 4;
constexpr int kButtonsY = 752;
constexpr int kButtonH = 42;
constexpr int kM = 16;
constexpr int kPanelY = 540;  // a special figure's announcement and the result
constexpr int kChoiceY = kStatusY - 4;

fui::Rect rect(const int x, const int y, const int w, const int h) {
  return fui::makeRect(static_cast<int16_t>(x), static_cast<int16_t>(y), static_cast<int16_t>(w),
                       static_cast<int16_t>(h));
}
bool inside(const fui::Rect& r, const int x, const int y) {
  return x >= r.x && x < r.x + r.width && y >= r.y && y < r.y + r.height;
}

int zoneTop(const int row) { return kGridY + row * (kZoneH + kZoneGapY); }
int zoneLeft(const int col) { return kGridX + col * (kZoneW + kZoneGapX); }
fui::Rect stackRect(const int i) {
  const int z = i / 4;
  const int k = i % 4;
  return rect(zoneLeft(z % 2) + (k % 2) * (kCardW + kCardGap), zoneTop(z / 2) + kZoneLabelH + (k / 2) * (kCardH + kCardGap),
              kCardW, kCardH);
}
fui::Rect boxRect(const int ch) { return rect(kM + ch * (kBoxW + kBoxGap), kTrackY, kBoxW, kTrackH); }
fui::Rect miniRect(const int ch, const bool outie) {
  const fui::Rect b = boxRect(ch);
  return rect(b.x + 6 + (outie ? kMiniW + 7 : 0), b.y + 24, kMiniW, kMiniH);
}
fui::Rect drawRect() { return rect(kDrawX, kTrayCardY, kTrayCardW, kTrayCardH); }
// The drawn cards sit just left of the pile, the newest nearest it; three or
// more overlap so the totals keep their room on the left.
int handStep(const int n) { return n > 3 ? 40 : n > 2 ? 50 : kTrayCardW + 6; }
fui::Rect handRect(const int k, const int n) {
  return rect(kDrawX - 12 - kTrayCardW - (n - 1 - k) * handStep(n), kTrayCardY, kTrayCardW, kTrayCardH);
}

// ---- text ----

const toybox::CutMetrics& cutOf(const Face f) {
  switch (f) {
    case Face::Tile:
      return toybox::kTileCut;
    case Face::Button:
      return toybox::kButtonCut;
    case Face::Ui:
      return toybox::kUiCut;
    case Face::Display:
      return toybox::kDisplayCut;
    case Face::Large:
      return toybox::kLargeCut;
    case Face::Huge:
    default:
      return toybox::kHugeCut;
  }
}

fui::TextStyle face(const Face f, const fui::TextAlign align = fui::TextAlign::Center, const bool paper = false) {
  if (gFonts) gFonts->bind(kTextSlot, f);
  fui::TextStyle t;
  t.font = kTextSlot;
  t.align = align;
  if (paper) t.color = fui::Color::White;
  return t;
}

void say(toybox::Screen& screen, const fui::Rect& box, const char* text, const Face f,
         const fui::TextAlign align = fui::TextAlign::Center, const bool paper = false) {
  screen.target().text(toybox::inkCentred(box, cutOf(f)), text, face(f, align, paper));
}

int widthOf(toybox::Screen& screen, const char* text, const Face f) {
  const fui::TextStyle t = face(f);
  return screen.target().measureText(t.font, text, t).width;
}

Face fit(toybox::Screen& screen, const char* text, const int width, std::initializer_list<Face> faces) {
  Face last = Face::Tile;
  for (const Face f : faces) {
    last = f;
    if (widthOf(screen, text, f) <= width) return f;
  }
  return last;
}

// How many lines the words take at this width, wrapping at spaces.
int linesFor(toybox::Screen& screen, const char* text, const Face f, const int width) {
  const fui::TextStyle t = face(f);
  const int space = screen.target().measureText(t.font, " ", t).width;
  int lines = 1;
  int x = 0;
  char word[64];
  const char* p = text;
  while (*p) {
    while (*p == ' ') ++p;
    int n = 0;
    while (p[n] && p[n] != ' ' && n < static_cast<int>(sizeof(word)) - 1) ++n;
    if (!n) break;
    std::memcpy(word, p, n);
    word[n] = '\0';
    p += n;
    const int w = screen.target().measureText(t.font, word, t).width;
    if (x && x + space + w > width) {
      ++lines;
      x = w;
    } else {
      x += (x ? space : 0) + w;
    }
  }
  return lines;
}

// Wrapped lines, from the top of the box (the toolkit centres a short block in
// its rect, so the rect is cut to the lines the words take).
void prose(toybox::Screen& screen, const fui::Rect& box, const char* text, const Face f, const uint8_t lines,
           const fui::TextAlign align = fui::TextAlign::Left, const bool paper = false) {
  fui::TextStyle t = face(f, align, paper);
  int need = linesFor(screen, text, f, box.width);
  if (need > lines) need = lines;
  // One line of slack, in case the toolkit breaks a line earlier than this count.
  t.maxLines = static_cast<uint8_t>(need + 1 <= lines ? need + 1 : lines);
  const int h = need * cutOf(f).lineHeight;
  screen.target().text(rect(box.x, box.y, box.width, h < box.height ? h : box.height), text, t);
}

// Grey type: black words with every other pixel knocked back to white.
void knock(toybox::Screen& screen, const fui::Rect& r) {
  for (int y = r.y; y < r.y + r.height; ++y)
    for (int x = r.x + (y & 1); x < r.x + r.width; x += 2) screen.target().fill(rect(x, y, 1, 1), kWhite);
}
void sayGrey(toybox::Screen& screen, const fui::Rect& box, const char* text, const Face f,
             const fui::TextAlign align = fui::TextAlign::Center) {
  say(screen, box, text, f, align);
  knock(screen, box);
}

// ---- marks ----

void disc(toybox::Screen& screen, const int cx, const int cy, const int r, const fui::Paint& paint) {
  for (int dy = -r; dy <= r; ++dy) {
    int w = 0;
    while ((w + 1) * (w + 1) + dy * dy <= r * r) ++w;
    screen.target().fill(rect(cx - w, cy + dy, 2 * w + 1, 1), paint);
  }
}

// Green numbers wear a dot, blue ones a square, as on the phone.
void colourMark(toybox::Screen& screen, const int cx, const int cy, const int size, const bool green,
                const bool paper = false) {
  const fui::Paint& p = paper ? kWhite : kBlack;
  if (green) {
    disc(screen, cx, cy, size / 2, p);
  } else {
    screen.target().fill(rect(cx - size / 2, cy - size / 2, size, size), p);
  }
}

void dashed(toybox::Screen& screen, const fui::Rect& r, const int t = 2) {
  for (int x = r.x; x < r.x + r.width; x += 10) {
    const int w = x + 6 <= r.x + r.width ? 6 : r.x + r.width - x;
    screen.target().fill(rect(x, r.y, w, t), kBlack);
    screen.target().fill(rect(x, r.y + r.height - t, w, t), kBlack);
  }
  for (int y = r.y; y < r.y + r.height; y += 10) {
    const int h = y + 6 <= r.y + r.height ? 6 : r.y + r.height - y;
    screen.target().fill(rect(r.x, y, t, h), kBlack);
    screen.target().fill(rect(r.x + r.width - t, y, t, h), kBlack);
  }
}

// Every other row back to white: how a card out of use reads.
void stripes(toybox::Screen& screen, const fui::Rect& r) {
  for (int y = r.y + 1; y < r.y + r.height - 1; y += 2) screen.target().fill(rect(r.x + 1, y, r.width - 2, 1), kWhite);
}

// A button drawn here, in the UI cut, over a blank hotspot: the toolkit's own
// label face is a size too big for these words.
void button(toybox::Screen& screen, const fui::Rect& r, const char* label, const fui::ActionId action,
            const bool solid = true, const int value = 0) {
  fui::ButtonProps b;
  b.label = "";
  b.action = action;
  b.value = static_cast<int16_t>(value);
  b.styles = toybox::rowStyles();
  b.radius = 6;
  screen.button(b, r);
  screen.target().fill(r, solid ? kBlack : kWhite, 6);
  if (!solid) screen.target().stroke(r, kBlack, 2, 6);
  say(screen, r, label, fit(screen, label, r.width - 16, {Face::Ui, Face::Button}), fui::TextAlign::Center, solid);
}
// A small outlined button in the button cut, drawn over a blank hotspot.
void smallPill(toybox::Screen& screen, const fui::Rect& r, const char* label, const fui::ActionId action) {
  fui::ButtonProps b;
  b.label = "";
  b.action = action;
  b.styles = toybox::rowStyles();
  b.radius = 6;
  screen.button(b, r);
  screen.target().fill(r, kWhite, 6);
  screen.target().stroke(r, kBlack, 2, 6);
  say(screen, r, label, Face::Button);
}
void pillOff(toybox::Screen& screen, const fui::Rect& r, const char* label) {
  screen.target().stroke(r, kGrey, 2, 6);
  sayGrey(screen, r, label, Face::Ui);
}

// ---- cards ----

struct Look {
  bool selected = false;
  bool candidate = false;
  bool blocked = false;
  bool outOfUse = false;  // in Drummond's locked zone
  bool lockedZone = false;
  bool goatGreen = false;  // the Goat, chosen as green (else blue) while selected
};

void cardBack(toybox::Screen& screen, const fui::Rect& r, const bool locked) {
  auto& t = screen.target();
  t.fill(r, kWhite, 4);
  t.stroke(r, kBlack, 2, 4);
  const fui::Rect inner = rect(r.x + 5, r.y + 5, r.width - 10, r.height - 10);
  t.fill(inner, locked ? kLight : kGrey, 2);
  t.stroke(inner, kBlack, 1, 2);
}

void numFace(toybox::Screen& screen, const fui::Rect& r, const s::Game& g, const int id, const Look& look,
             const bool small) {
  const s::CardDef d = s::cardDef(id);
  char v[8];
  if (d.wild && !g.card[id].val) {
    std::snprintf(v, sizeof(v), "?");
  } else {
    std::snprintf(v, sizeof(v), "%d", g.card[id].val);
  }
  const int valueH = r.height * 58 / 100;
  say(screen, rect(r.x, r.y + 4, r.width, valueH), v, small ? Face::Ui : Face::Display);
  const int cy = r.y + 4 + valueH + (r.height - valueH - 4) / 2 - 2;
  const int mark = small ? 10 : 14;
  if (d.wild) {
    if (look.selected) {
      colourMark(screen, r.x + r.width / 2, cy, mark, look.goatGreen);
    } else {
      const fui::Rect tag = rect(r.x + r.width / 2 - 24, cy - 10, 48, 20);
      screen.target().stroke(tag, kBlack, 1, 0);
      say(screen, tag, "GOAT", Face::Tile);
    }
    return;
  }
  colourMark(screen, r.x + r.width / 2, cy, mark, g.card[id].red);
}

void mainFace(toybox::Screen& screen, const fui::Rect& r, const int id, const bool small) {
  const s::CardDef d = s::cardDef(id);
  const bool innie = d.red;
  if (innie) screen.target().fill(rect(r.x + 2, r.y + 2, r.width - 4, r.height - 4), kBlack, 3);
  if (small) {
    say(screen, r, innie ? "IN" : "OUT", Face::Tile, fui::TextAlign::Center, innie);
    return;
  }
  // The name on one line in the button cut if it fits, else two smaller ones.
  const char* name = s::cardName(id);
  const Face f = widthOf(screen, name, Face::Button) <= r.width - 12 ? Face::Button : Face::Tile;
  prose(screen, rect(r.x + 6, r.y + 14, r.width - 12, 44), name, f, 2, fui::TextAlign::Center, innie);
  // Bottom left, clear of the stack count in the other corner.
  const fui::Rect tag = rect(r.x + 7, r.y + r.height - 28, 54, 20);
  screen.target().stroke(tag, innie ? kWhite : kBlack, 1, 0);
  say(screen, tag, innie ? "INNIE" : "OUTIE", Face::Tile, fui::TextAlign::Center, innie);
}

void specialFace(toybox::Screen& screen, const fui::Rect& r, const int id) {
  const bool bad = s::isBad(id);
  if (bad) screen.target().fill(rect(r.x + 2, r.y + 2, r.width - 4, r.height - 4), kBlack, 3);
  screen.target().stroke(rect(r.x + 5, r.y + 5, r.width - 10, r.height - 10), bad ? kWhite : kBlack, 1, 0);
  prose(screen, rect(r.x + 7, r.y + 14, r.width - 14, 60), s::cardName(id), Face::Tile, 3, fui::TextAlign::Center, bad);
  const char* tagText = s::ability(id);
  int tw = widthOf(screen, tagText, Face::Tile) + 8;
  if (tw > r.width - 8) tw = r.width - 8;
  const fui::Rect tag = rect(r.x + (r.width - tw) / 2, r.y + r.height - 34, tw, 20);
  screen.target().stroke(tag, bad ? kWhite : kBlack, 1, 0);
  say(screen, tag, tagText, Face::Tile, fui::TextAlign::Center, bad);
}

void cardFace(toybox::Screen& screen, fui::Rect r, const s::Game& g, const int id, const Look& look,
              const bool small = false) {
  auto& t = screen.target();
  if (!g.card[id].faceUp) {
    cardBack(screen, r, look.lockedZone);
    return;
  }
  if (look.selected) r.y = static_cast<int16_t>(r.y - 6);
  t.fill(r, kWhite, 4);
  const s::Type type = s::cardDef(id).type;
  if (type == s::Type::Num) {
    numFace(screen, r, g, id, look, small);
  } else if (type == s::Type::Main) {
    mainFace(screen, r, id, small);
  } else {
    specialFace(screen, r, id);
  }
  t.stroke(r, kBlack, look.selected ? 4 : 2, 4);
  if (look.blocked) {
    const fui::Rect band = rect(r.x - 1, r.y + r.height / 2 - 12, r.width + 2, 24);
    t.fill(band, kBlack);
    say(screen, band, "COBEL", Face::Button, fui::TextAlign::Center, true);
  }
  if (look.outOfUse) stripes(screen, r);
  if (look.candidate) dashed(screen, rect(r.x - 5, r.y - 5, r.width + 10, r.height + 10));
}

bool selected(const BoardModel& m, const int v) {
  for (int k = 0; k < m.nSel; ++k)
    if (m.sel[k] == v) return true;
  return false;
}

// ---- the board's parts ----

void topBar(toybox::Screen& screen) {
  auto& t = screen.target();
  sayGrey(screen, rect(kM, 2, 240, 18), "MACRODATA REFINEMENT", Face::Tile, fui::TextAlign::Left);
  say(screen, rect(kM, 18, 240, 30), "SEVERANCE", Face::Ui, fui::TextAlign::Left);
  smallPill(screen, rect(kW - kM - 70 - 8 - 80, 6, 70, 36), "LOG", ActionLog);
  smallPill(screen, rect(kW - kM - 80, 6, 80, 36), "MENU", ActionMenu);
  t.fill(rect(0, kTopH, kW, 2), kBlack);
}

void track(toybox::Screen& screen, const BoardModel& m) {
  const s::Game& g = *m.game;
  auto& t = screen.target();
  for (int ch = 0; ch < s::kChars; ++ch) {
    const fui::Rect b = boxRect(ch);
    const bool paired = s::isPaired(g, ch);
    t.fill(b, kWhite);
    t.stroke(b, kBlack, paired ? 3 : 1, 0);
    // A paired employee's name goes white on black.
    char name[24];
    std::snprintf(name, sizeof(name), "%s", s::charName(ch));
    const fui::Rect band = rect(b.x, b.y, b.width, 20);
    if (paired) t.fill(band, kBlack);
    say(screen, rect(b.x + 6, b.y, b.width - 12, 20), name, Face::Tile, fui::TextAlign::Left, paired);
    // The two halves: from the pair, or waiting aside.
    int cards[2] = {s::kNone, s::kNone};
    for (int p = 0; p < g.nPaired; ++p)
      if (g.paired[p].ch == ch)
        for (int k = 0; k < g.paired[p].n; ++k) cards[s::cardDef(g.paired[p].cards[k]).red ? 0 : 1] = g.paired[p].cards[k];
    for (int k = 0; k < g.waiting.n; ++k)
      if (s::cardDef(g.waiting.c[k]).ch == ch) cards[s::cardDef(g.waiting.c[k]).red ? 0 : 1] = g.waiting.c[k];
    for (int half = 0; half < 2; ++half) {
      const fui::Rect r = miniRect(ch, half == 1);
      if (cards[half] == s::kNone) {
        dashed(screen, r, 1);
        sayGrey(screen, rect(r.x + 2, r.y + 2, r.width - 4, r.height - 4), half ? "OUT" : "IN", Face::Tile);
        continue;
      }
      Look look;
      look.selected = !paired && selected(m, cards[half]);
      cardFace(screen, r, g, cards[half], look, true);
    }
  }
}

void grid(toybox::Screen& screen, const BoardModel& m, const uint8_t* cand, const int nCand) {
  const s::Game& g = *m.game;
  auto& t = screen.target();
  const bool reghabi = g.pending.type == s::Pend::Reghabi;
  for (int z = 0; z < 4; ++z) {
    const int x = zoneLeft(z % 2);
    const int y = zoneTop(z / 2);
    char label[40];
    if (g.lockZone == z) {
      std::snprintf(label, sizeof(label), "ZONE %d  LOCKED BY DRUMMOND", z + 1);
    } else {
      std::snprintf(label, sizeof(label), "ZONE %d", z + 1);
    }
    const fui::Rect lr = rect(x, y, kZoneW, 18);
    if (g.unlocked[z]) {
      say(screen, lr, label, Face::Tile, fui::TextAlign::Left);
      t.fill(rect(x, y + 19, kZoneW, 2), kBlack);
    } else {
      sayGrey(screen, lr, label, Face::Tile, fui::TextAlign::Left);
      for (int dx = 0; dx < kZoneW; dx += 4) t.fill(rect(x + dx, y + 20, 2, 1), kBlack);
    }
  }
  for (int i = 0; i < s::kStacks; ++i) {
    const fui::Rect r = stackRect(i);
    const s::Stack& st = g.stacks[i];
    if (!st.n) {
      dashed(screen, r, 1);
    } else {
      // Depth: a line under the card for each card beneath, up to two.
      if (st.n > 1) t.fill(rect(r.x + 3, r.y + r.height + 1, r.width - 6, 1), kBlack);
      if (st.n > 2) t.fill(rect(r.x + 6, r.y + r.height + 3, r.width - 12, 1), kBlack);
      const int top = st.top();
      Look look;
      look.lockedZone = !g.unlocked[s::zoneOf(i)];
      look.selected = !reghabi && selected(m, top) && g.card[top].faceUp;
      look.blocked = top == g.blocked;
      look.outOfUse = s::inLockedZone(g, i) && g.card[top].faceUp;
      look.goatGreen = g.card[top].red;
      if (!reghabi && g.card[top].faceUp)
        for (int k = 0; k < nCand; ++k) look.candidate |= cand[k] == top;
      cardFace(screen, r, g, top, look);
      if (st.n > 1) {
        char n[8];
        std::snprintf(n, sizeof(n), "x%d", st.n);
        const fui::Rect badge = rect(r.x + r.width - 30, r.y + r.height - 22, 26, 18);
        t.fill(badge, kWhite);
        t.stroke(badge, kBlack, 1, 0);
        say(screen, badge, n, Face::Tile);
      }
    }
    if (reghabi) {
      bool isCand = false;
      for (int k = 0; k < nCand; ++k) isCand |= cand[k] == i;
      if (selected(m, i)) {
        t.stroke(rect(r.x - 5, r.y - 5, r.width + 10, r.height + 10), kBlack, 3, 4);
      } else if (isCand) {
        dashed(screen, rect(r.x - 5, r.y - 5, r.width + 10, r.height + 10));
      }
    }
  }
}

// The words under the grid: the hint after a tap, the quarter's end, or what
// to do now, with the standing effects after it.
void status(toybox::Screen& screen, const BoardModel& m) {
  const s::Game& g = *m.game;
  auto& t = screen.target();
  char line[s::kLogLen + 64];
  if (m.note) {
    std::snprintf(line, sizeof(line), "%s", m.note);
  } else if (g.over == s::Over::Win) {
    std::snprintf(line, sizeof(line), "Quota reached. Every innie has found their outie.");
  } else if (g.over == s::Over::Lose) {
    std::snprintf(line, sizeof(line), "The quarter is lost. Start a new one from the menu.");
  } else {
    int n = 0;
    auto add = [&](const char* text) {
      if (n < static_cast<int>(sizeof(line)) - 1) n += std::snprintf(line + n, sizeof(line) - n, "%s%s", n ? " · " : "", text);
    };
    if (g.hallPass) add("HALL PASS ACTIVE");
    if (g.hallPending) add("HALL PASS AFTER NEXT PAIR");
    if (g.lockZone != s::kNone) add("DRUMMOND'S LOCK");
    if (g.hellyPause) add("HELLY CANNOT PAIR");
    if (g.blocked != s::kNone) add("COBEL BLOCKING");
    bool helly = false;
    for (int k = 0; k < g.waiting.n; ++k) helly |= s::cardDef(g.waiting.c[k]).ch == 1;
    if (!g.defianceUsed && helly) add("DEFIANCE READY");
    if (!n) {
      // The newest thing that happened this turn, except a plain draw report:
      // the drawn card is on the table for anyone to see.
      const char* said = nullptr;
      for (int k = g.nLog - 1; k >= 0 && g.log[k].turn == g.turn && !said; --k) {
        const char* x = g.log[k].text;
        if (std::strncmp(x, "You drew ", 9) != 0 && std::strncmp(x, "The unused draw", 15) != 0) said = x;
      }
      add(said ? said : "Refine round and square numbers with equal totals, pair revealed characters, or draw.");
    }
  }
  prose(screen, rect(kM, kStatusY - 1, kW - 2 * kM, 44), line, Face::Tile, 2);
}

void tray(toybox::Screen& screen, const BoardModel& m, const uint8_t* cand, const int nCand) {
  const s::Game& g = *m.game;
  auto& t = screen.target();
  t.fill(rect(kM, kTrayY - 8, kW - 2 * kM, 1), kBlack);
  const fui::Rect pile = drawRect();
  if (g.draw.n) {
    t.fill(rect(pile.x + 4, pile.y + 4, pile.width, pile.height), kBlack, 4);
    cardBack(screen, pile, false);
    // What is left, in a pill on the pile's bottom-right corner.
    char left[8];
    std::snprintf(left, sizeof(left), "%d", g.draw.n);
    const int pw = widthOf(screen, left, Face::Button) + 16;
    const fui::Rect pillR = rect(pile.x + pile.width - pw + 10, pile.y + pile.height - 14, pw, 26);
    t.fill(pillR, kWhite, 13);
    t.stroke(pillR, kBlack, 2, 13);
    say(screen, pillR, left, Face::Button);
  } else {
    dashed(screen, pile, 1);
  }
  uint8_t hand[s::kCards];
  const int nh = s::playableHand(g, hand);
  const int handLeft = handRect(0, nh ? nh : 1).x;
  if (!nh) dashed(screen, handRect(0, 1), 1);
  for (int k = 0; k < nh; ++k) {
    const int id = hand[k];
    Look look;
    look.selected = selected(m, id);
    look.goatGreen = g.card[id].red;
    for (int q = 0; q < nCand; ++q) look.candidate |= cand[q] == id;
    const fui::Rect r = handRect(k, nh);
    // Earlier draws wait under the top one: show them as a card's edge behind it.
    if (!g.card[id].keep && s::wasteDepth(g) > 1) {
      t.fill(rect(r.x + 4, r.y + 4, r.width, r.height), kBlack, 4);
    }
    cardFace(screen, r, g, id, look, true);
  }
  // The selection's totals on the left: round over square, or what a chosen
  // employee needs.
  const int sx = kM;
  const int sw = handLeft - 12 - sx;
  const s::Totals tot = s::selectionTotals(g, m.sel, m.nSel);
  bool main = false;
  for (int k = 0; k < m.nSel; ++k) main |= s::cardDef(m.sel[k]).type == s::Type::Main;
  if (g.pending.type == s::Pend::None && m.nSel && !main) {
    char v[24];
    const Face f = fit(screen, "SQUARE 88", sw - 22, {Face::Button, Face::Tile});
    colourMark(screen, sx + 8, kTrayCardY + 18, 14, true);
    std::snprintf(v, sizeof(v), "ROUND %d", tot.green);
    say(screen, rect(sx + 22, kTrayCardY + 4, sw - 22, 28), v, f, fui::TextAlign::Left);
    colourMark(screen, sx + 8, kTrayCardY + 52, 14, false);
    std::snprintf(v, sizeof(v), "SQUARE %d", tot.blue);
    say(screen, rect(sx + 22, kTrayCardY + 38, sw - 22, 28), v, f, fui::TextAlign::Left);
  } else if (g.pending.type == s::Pend::None && main) {
    prose(screen, rect(sx, kTrayCardY + 6, sw, 64), "Tap the other half to pair", Face::Tile, 3);
  }
  // Buttons.
  const int half = (kW - 2 * kM - 10) / 2;
  const fui::Rect left = rect(kM, kButtonsY, half, kButtonH);
  const fui::Rect right = rect(kW - kM - half, kButtonsY, half, kButtonH);
  if (g.over != s::Over::No) {
    button(screen, rect(kM, kButtonsY, kW - 2 * kM, kButtonH), "NEW QUARTER", ActionNewQuarter);
    return;
  }
  if (m.nSel) {
    button(screen, left, "CLEAR", ActionClear, false);
  } else {
    pillOff(screen, left, "CLEAR");
  }
  if (tot.ok && !main) {
    button(screen, right, "REFINE", ActionRefine);
  } else {
    pillOff(screen, right, "REFINE");
  }
}

// A special figure's moment, over the bottom of the board.
void announcement(toybox::Screen& screen, const s::Game& g, const s::Ann& a) {
  auto& t = screen.target();
  const fui::Rect panel = rect(0, kPanelY, kW, 800 - kPanelY);
  t.fill(panel, kWhite);
  t.fill(rect(0, kPanelY, kW, 3), kBlack);
  const fui::Rect card = rect(kM, kPanelY + 18, 112, 132);
  t.fill(card, kWhite, 4);
  specialFace(screen, card, a.card);
  t.stroke(card, kBlack, 2, 4);
  const int tx = card.x + card.width + 14;
  const int tw = kW - kM - tx;
  // The figure's name large, the ability's title small and grey under it.
  sayGrey(screen, rect(tx, kPanelY + 44, tw, 18), a.title, Face::Tile, fui::TextAlign::Left);
  say(screen, rect(tx, kPanelY + 12, tw, 32), s::cardName(a.card), fit(screen, s::cardName(a.card), tw, {Face::Ui, Face::Button}),
      fui::TextAlign::Left);
  t.fill(rect(tx, kPanelY + 64, tw, 1), kBlack);
  prose(screen, rect(tx, kPanelY + 70, tw, 130), a.text, Face::Tile, 6);
  (void)g;
  button(screen, rect(kW - kM - 200, kButtonsY, 200, kButtonH), "CONTINUE", ActionContinue);
}

// A choice for Kier, Ms. Casey, Gemma or Reghabi, under the grid.
void choice(toybox::Screen& screen, const BoardModel& m) {
  const s::Game& g = *m.game;
  auto& t = screen.target();
  const fui::Rect panel = rect(0, kChoiceY, kW, 800 - kChoiceY);
  t.fill(panel, kWhite);
  t.fill(rect(0, kChoiceY, kW, 3), kBlack);
  char title[48], text[s::kLogLen];
  s::pendingText(g, title, sizeof(title), text, sizeof(text));
  say(screen, rect(kM, kChoiceY + 10, kW - 2 * kM, 30), title, Face::Ui, fui::TextAlign::Left);
  prose(screen, rect(kM, kChoiceY + 46, kW - 2 * kM, 70), m.note ? m.note : text, Face::Tile, 3);
  char count[24];
  std::snprintf(count, sizeof(count), "%d OF %d CHOSEN", m.nSel, g.pending.max);
  say(screen, rect(kM, kChoiceY + 112, 200, 20), count, Face::Button, fui::TextAlign::Left);
  const int half = (kW - 2 * kM - 10) / 2;
  button(screen, rect(kM, kButtonsY, half, kButtonH), "SKIP", ActionSkip, false);
  if (m.nSel) {
    button(screen, rect(kW - kM - half, kButtonsY, half, kButtonH), "CONFIRM", ActionConfirm);
  } else {
    pillOff(screen, rect(kW - kM - half, kButtonsY, half, kButtonH), "CONFIRM");
  }
}

void ending(toybox::Screen& screen, const s::Game& g) {
  auto& t = screen.target();
  const fui::Rect panel = rect(0, kPanelY, kW, 800 - kPanelY);
  t.fill(panel, kWhite);
  t.fill(rect(0, kPanelY, kW, 3), kBlack);
  const bool won = g.over == s::Over::Win;
  say(screen, rect(kM, kPanelY + 24, kW - 2 * kM, 50), won ? "You hit quota!" : "The quarter is lost", Face::Display);
  prose(screen, rect(kM + 10, kPanelY + 90, kW - 2 * kM - 20, 90),
        won ? "All main characters are paired. You receive a caricature portrait."
            : "No valid moves were left before all main characters were paired. Please remain seated while we "
              "process your transfer.",
        Face::Button, 3, fui::TextAlign::Center);
  const int half = (kW - 2 * kM - 10) / 2;
  button(screen, rect(kM, kButtonsY, half, kButtonH), "VIEW GRID", ActionViewGrid, false);
  button(screen, rect(kW - kM - half, kButtonsY, half, kButtonH), "NEW QUARTER", ActionNewQuarter);
}

// ---- rules ----

struct RulesPage {
  const char* title;
  const char* body;
};
const RulesPage kRules[] = {
    {"Objective",
     "Reunite each employee's innie with their outie: Mark S., Helly R. (Helena), Dylan G. and Irving B. Pair all "
     "four to hit quota.\n\nInnies are the black cards, outies the white ones. Numbers are round (marked with a dot) "
     "or square (marked with a square)."},
    {"Cards",
     "The grid holds 48 cards in layers of 1, 3, 4 and 4 per zone: the numbers 2 to 10 four times each, the eight "
     "employees, Kier, Mr. Drummond, Ms. Casey and Emile the Goat.\n\nAdministration, the draw pile, holds 33: 26 "
     "numbers and Dieter, Gemma, Doug Graner, Milchick, Cobel, Reghabi and Jame Eagan."},
    {"Refining",
     "Tap face-up numbers so the round total equals the square total (7 + 3 round with 10 square), then REFINE.\n\nWhen "
     "identical cards are free, the one in the earliest position (Zone 1 first, left to right) must be used.\n\n"
     "Removing a card turns over the one beneath it. Tap the draw pile when you like. Unused draws pile up "
     "beside it; only the top one can be used, and once it is, the one under it can."},
    {"Pairing",
     "Tap an employee's innie and outie when both are on the grid and they pair at once. If one half already waits "
     "aside, one tap on the other half pairs them.\n\nWhen the draw pile runs out while Zone 3 still hides cards, "
     "its used cards are reshuffled once. After your next pair, employees get the Hall Pass and step aside when they "
     "appear.\n\nYou lose if no move is left and the pile is empty."},
    {"Zones",
     "Zone 1 starts open. Zone 2 opens when two number cards remain in Zone 1.\n\nZones 3 and 4 open when only one "
     "stack in the zone before still hides a card.\n\nIf no number is face up anywhere, the next locked zone "
     "opens."},
    {"Special figures",
     "Kier: move up to two numbers away. Dieter: draws two; his numbers stay in hand. Ms. Casey and Gemma: set an "
     "unmatched employee aside, or move a number away. Helly leaves her stack on her own and can cancel Milchick or "
     "Cobel once. Drummond reverses the pile and Zone 4, and locks his zone until your next pair if another open "
     "zone has a face-up number (never Zone 1)."},
    {"More figures",
     "Graner sends a revealed employee to the bottom of the last Zone 4 stack. Jame cancels Helly's Defiance, pauses "
     "her, and replays Milchick and Cobel. Reghabi searches stacks you choose for hidden employees. Milchick returns "
     "your last refined cards to the grid. Cobel splits a pair, or blocks a number. Emile the Goat is wild: tap "
     "once for round, again for square."},
    {"Credits",
     "Severance: The Card Game, designed by u/DREAMWALL, CC BY-NC-SA 4.0. An unofficial fan version of the Version 2 "
     "test rules, shared under the same licence.\n\nThe same rules as the phone version, checked against it game by "
     "game."},
};
constexpr int kRulesPages = static_cast<int>(sizeof(kRules) / sizeof(kRules[0]));

}  // namespace

void useFonts(FontBinder* binder) { gFonts = binder; }

int rulesPages() { return kRulesPages; }

Hit hitTest(const s::Game& g, const int x, const int y) {
  Hit h;
  for (int i = 0; i < s::kStacks; ++i) {
    const fui::Rect r = stackRect(i);
    if (inside(rect(r.x - 3, r.y - 8, r.width + 6, r.height + 11), x, y)) return Hit{HitKind::Stack, i};
  }
  for (int ch = 0; ch < s::kChars; ++ch)
    for (int half = 0; half < 2; ++half)
      if (inside(miniRect(ch, half == 1), x, y)) return Hit{HitKind::Mini, ch * 2 + half};
  uint8_t hand[s::kCards];
  const int nh = s::playableHand(g, hand);
  for (int k = nh - 1; k >= 0; --k)
    if (inside(handRect(k, nh), x, y)) return Hit{HitKind::Hand, k};
  if (inside(drawRect(), x, y)) return Hit{HitKind::Draw, 0};
  return h;
}

void buildBoard(toybox::Screen& screen, const BoardModel& m) {
  const s::Game& g = *m.game;
  toybox::absoluteChrome(screen);
  uint8_t cand[s::kCards];
  const int nCand = m.annIndex < 0 && !m.showEnd ? s::pendingCandidates(g, cand) : 0;
  topBar(screen);
  track(screen, m);
  grid(screen, m, cand, nCand);
  if (m.annIndex >= 0 && m.annIndex < g.nAnn) {
    announcement(screen, g, g.ann[m.annIndex]);
  } else if (m.showEnd) {
    ending(screen, g);
  } else if (g.pending.type != s::Pend::None) {
    choice(screen, m);
  } else {
    status(screen, m);
    tray(screen, m, cand, nCand);
  }
}

void buildMenu(toybox::Screen& screen, const MenuModel& model) {
  auto& t = screen.target();
  toybox::absoluteChrome(screen);
  sayGrey(screen, rect(0, 60, kW, 24), "MACRODATA REFINEMENT", Face::Button);
  say(screen, rect(0, 92, kW, 70), "SEVERANCE", Face::Large);
  t.fill(rect(60, 176, kW - 120, 2), kBlack);
  say(screen, rect(0, 186, kW, 26), "The card game", Face::Button);
  // Four employees, innie over outie, as an ornament made of the game's cards.
  for (int ch = 0; ch < s::kChars; ++ch) {
    const fui::Rect r = rect(48 + ch * 100, 236, 84, 96);
    const fui::Rect o = rect(r.x + 10, r.y + 14, r.width, r.height);
    t.fill(o, kWhite, 4);
    t.stroke(o, kBlack, 2, 4);
    t.fill(r, kBlack, 4);
    prose(screen, rect(r.x + 4, r.y + 16, r.width - 8, 44), s::charName(ch), Face::Tile, 2, fui::TextAlign::Center, true);
  }
  if (model.played) {
    char line[48];
    std::snprintf(line, sizeof(line), "%d QUARTERS  %d AT QUOTA", model.played, model.won);
    say(screen, rect(0, 372, kW, 24), line, Face::Button);
  }
  const char* labels[static_cast<int>(MenuRow::Count)] = {"CONTINUE", "NEW QUARTER", "RULES",
                                                          model.lenient ? "EASIER MILCHICK: ON" : "EASIER MILCHICK: OFF"};
  int y = 800 - kM - static_cast<int>(MenuRow::Count) * (58 + 10) + 10;
  for (int i = 0; i < static_cast<int>(MenuRow::Count); ++i) {
    const fui::Rect r = rect(kM, y, kW - 2 * kM, 58);
    if (i == static_cast<int>(MenuRow::Continue) && !model.canContinue) {
      pillOff(screen, r, labels[i]);
    } else {
      const bool loud = i == static_cast<int>(MenuRow::Continue) ||
                        (i == static_cast<int>(MenuRow::NewQuarter) && !model.canContinue);
      button(screen, r, labels[i], ActionMenuRow, loud, i);
    }
    y += 68;
  }
}

void buildLog(toybox::Screen& screen, const s::Game& g) {
  auto& t = screen.target();
  toybox::absoluteChrome(screen);
  say(screen, rect(kM, 8, 300, 36), "THE LOG", Face::Ui, fui::TextAlign::Left);
  smallPill(screen, rect(kW - kM - 90, 6, 90, 36), "BACK", ActionBack);
  t.fill(rect(0, kTopH, kW, 2), kBlack);
  int y = kTopH + 10;
  // Newest first, until the page is full.
  for (int k = g.nLog - 1; k >= 0 && y < 780; --k) {
    const s::LogLine& l = g.log[k];
    const int len = static_cast<int>(std::strlen(l.text));
    const int lines = len / 50 + 1 > 4 ? 4 : len / 50 + 1;
    const int h = lines * 21 + 6;
    if (y + h > 792) break;
    const fui::Rect mark = rect(kM, y + 6, 7, 7);
    if (l.kind == s::Kind::Good) {
      t.fill(mark, kBlack);
    } else if (l.kind == s::Kind::Bad) {
      t.stroke(mark, kBlack, 2, 0);
    } else {
      t.fill(rect(kM + 2, y + 8, 3, 3), kBlack);
    }
    prose(screen, rect(kM + 16, y, kW - 2 * kM - 16, h), l.text, Face::Tile, static_cast<uint8_t>(lines));
    if (k > 0 && l.turn != g.log[k - 1].turn) t.fill(rect(kM + 16, y + h - 2, kW - 2 * kM - 16, 1), kLight);
    y += h + 4;
  }
}

void buildRules(toybox::Screen& screen, const int page) {
  auto& t = screen.target();
  toybox::absoluteChrome(screen);
  const RulesPage& p = kRules[page < 0 ? 0 : page >= kRulesPages ? kRulesPages - 1 : page];
  sayGrey(screen, rect(kM, 14, 300, 20), "REFINER'S HANDBOOK", Face::Button, fui::TextAlign::Left);
  char n[16];
  std::snprintf(n, sizeof(n), "%d / %d", page + 1, kRulesPages);
  say(screen, rect(kW - kM - 100, 14, 100, 20), n, Face::Button, fui::TextAlign::Right);
  say(screen, rect(kM, 46, kW - 2 * kM, 44), p.title, Face::Display, fui::TextAlign::Left);
  t.fill(rect(kM, 96, kW - 2 * kM, 2), kBlack);
  // Paragraphs are separated by blank lines in the text.
  char buf[700];
  std::snprintf(buf, sizeof(buf), "%s", p.body);
  int y = 112;
  char* para = buf;
  while (para && *para && y < 700) {
    char* next = std::strstr(para, "\n\n");
    if (next) {
      *next = '\0';
      next += 2;
    }
    const int len = static_cast<int>(std::strlen(para));
    const int lines = len / 34 + 1;
    const int h = lines * 30;
    prose(screen, rect(kM, y, kW - 2 * kM, h + 8), para, Face::Button, static_cast<uint8_t>(lines + 1));
    y += h + 22;
    para = next;
  }
  const int half = (kW - 2 * kM - 10) / 2;
  button(screen, rect(kM, kButtonsY, half, kButtonH), "BACK", ActionRulesBack, false);
  button(screen, rect(kW - kM - half, kButtonsY, half, kButtonH), page + 1 < kRulesPages ? "NEXT" : "DONE", ActionRulesNext);
}

}  // namespace sevui
