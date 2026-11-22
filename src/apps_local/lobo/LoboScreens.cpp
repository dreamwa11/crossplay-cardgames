#include "LoboScreens.h"

#include <FreeInkUIIcon.h>

#include <cctype>
#include <cstdio>
#include <cstring>

#include "../cards/CardArt.h"
#include "../scoundrel/fonts/ScoundrelCuts.h"
#include "LoboArt.h"

namespace loboui {

namespace {

namespace l = lobo;
namespace sf = scoundrelfonts;

const fui::Paint kBlack = fui::Paint::solid(fui::Color::Black);
const fui::Paint kWhite = fui::Paint::solid(fui::Color::White);
const fui::Paint kGrey = fui::Paint::dither(fui::Color::DarkGray);
const fui::Paint kLight = fui::Paint::dither(fui::Color::LightGray);
constexpr int16_t kRadius = 8;
constexpr fui::FontId kTextSlot = 3;
FontBinder* gFonts = nullptr;

const char* const kSuitNames[5] = {"MOON", "PAW", "FANG", "PINE", "BONE"};

const toybox::CutMetrics& cutOf(const Face face) {
  switch (face) {
    case Face::Small:
      return sf::kSmallCut;
    case Face::Body:
      return sf::kBodyCut;
    case Face::Large:
      return sf::kLargeCut;
    case Face::Value:
      return sf::kValueCut;
    case Face::ValueBig:
      return sf::kValueBigCut;
    case Face::Title:
      return sf::kTitleCut;
    case Face::Logo:
    default:
      return sf::kLogoCut;
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
  const fui::TextStyle t = face(f, align, paper);
  screen.target().text(toybox::inkCentred(box, cutOf(f)), text, t);
}

int16_t widthOf(toybox::Screen& screen, const char* text, const Face f) {
  const fui::TextStyle t = face(f);
  return screen.target().measureText(t.font, text, t).width;
}

// Wrapped prose, centred, up to `lines` lines.
void prose(toybox::Screen& screen, const fui::Rect& box, const char* text, const Face f, const uint8_t lines,
           const bool paper = false) {
  fui::TextStyle t = face(f, fui::TextAlign::Center, paper);
  t.maxLines = lines;
  screen.target().text(box, text, t);
}

fui::Rect rect(const int x, const int y, const int w, const int h) {
  return fui::makeRect(static_cast<int16_t>(x), static_cast<int16_t>(y), static_cast<int16_t>(w),
                       static_cast<int16_t>(h));
}

void icon(toybox::Screen& screen, const fui::Rect& box, const freeink::Icon& art, const bool paper = false) {
  screen.target().bitmap(box, fui::bitmapFromIcon(art), fui::BitmapMode::Center, paper ? kWhite : kBlack);
}

// A picture with an opaque silhouette: the mask in white, then the ink.
void figure(toybox::Screen& screen, const int x, const int y, const freeink::Icon& mask, const freeink::Icon& ink) {
  const fui::Rect box = rect(x, y, ink.w, ink.h);
  icon(screen, box, mask, true);
  icon(screen, box, ink);
}

const freeink::Icon& suitArt(const uint8_t card, const int size) {
  const bool b = size >= 96;
  const bool m = size >= 64 && !b;
#define ART(name) (b ? icon_lobo_##name##_96 : m ? icon_lobo_##name##_64 : icon_lobo_##name##_48)
  switch (l::suitOf(card)) {
    case 0:
      return ART(moon);
    case 1:
      return ART(paw);
    case 2:
      return ART(fang);
    case 3:
      return ART(pine);
    default:
      return ART(bone);
  }
#undef ART
}

// ---- cards ------------------------------------------------------------------

struct CardLook {
  bool selected = false;
  bool faded = false;
  bool fresh = false;
  bool tight = false;  // overlapped hard: a smaller value tucked into the corner
};

// A face-up card: the value top-right in Scoundrel Gothic, the suit's picture,
// the suit's name along the bottom. White, as on the PaperMono Scoundrel.
void drawCard(toybox::Screen& screen, const fui::Rect& r, const uint8_t card, const CardLook look = {}) {
  auto& target = screen.target();
  target.fill(r, kWhite, kRadius);
  target.stroke(r, kBlack, look.selected ? 5 : 2, kRadius);
  char value[8];
  std::snprintf(value, sizeof(value), "%d", l::valueOf(card));
  if (look.tight) {
    say(screen, rect(r.right() - 3 - 64, r.y + 4, 64, 30), value, Face::Large, fui::TextAlign::Right);
  } else {
    say(screen, rect(r.right() - 8 - 64, r.y + 4, 64, 34), value, Face::Value, fui::TextAlign::Right);
  }
  const int art = r.height >= 160 ? 64 : 48;
  const int nameH = sf::kSmallCut.lineHeight;
  const int nameTop = r.bottom() - nameH - 4;
  const int room = nameTop - r.y - 36;
  const int artTop = r.y + 36 + (room > art ? (room - art) / 2 : 0);
  icon(screen, rect(r.x + (r.width - art) / 2, artTop, art, art), suitArt(card, art));
  say(screen, rect(r.x + 2, nameTop, r.width - 4, nameH), kSuitNames[l::suitOf(card)], Face::Small);
  if (look.fresh) {
    // Just arrived: a black dot where the phone would have flown it in. Bottom
    // right, the corner an overlapping neighbour never covers.
    target.fill(rect(r.right() - 19, r.bottom() - 19, 12, 12), kBlack, 6);
  }
  if (look.faded && r.width == lobo_fade.w && r.height == lobo_fade.h) icon(screen, r, lobo_fade, true);
}

// A card back: a dithered panel with the howling wolf in a white disc.
void drawBack(toybox::Screen& screen, const fui::Rect& r) {
  auto& target = screen.target();
  target.fill(r, kWhite, kRadius);
  target.stroke(r, kBlack, 2, kRadius);
  const fui::Rect inner = rect(r.x + 7, r.y + 7, r.width - 14, r.height - 14);
  target.fill(inner, kGrey, kRadius / 2);
  target.stroke(inner, kBlack, 2, kRadius / 2);
  const fui::Rect disc = rect(r.x + r.width / 2 - 24, r.y + r.height / 2 - 24, 48, 48);
  target.fill(disc, kWhite, 24);
  target.stroke(disc, kBlack, 2, 24);
  icon(screen, rect(disc.x + 8, disc.y + 8, 32, 32), icon_lobo_howl_32);
}

// A white pill hanging off the bottom of a pile: its count.
void badge(toybox::Screen& screen, const fui::Rect& slot, const char* text) {
  const fui::Rect pill = rect(slot.x + slot.width / 2 - 28, slot.bottom() - 18, 56, 34);
  screen.target().fill(pill, kWhite, 14);
  screen.target().stroke(pill, kBlack, 2, 14);
  say(screen, pill, text, Face::Value);
}

fui::ButtonProps pill(const char* label, const fui::ActionId action, const bool solid = true) {
  fui::ButtonProps b;
  b.label = label;
  b.action = action;
  b.radius = 10;
  if (!solid) b.styles = toybox::rowStyles();
  return b;
}

// A pill drawn as a picture: grey, nothing to press (the how-to's examples).
void pillPicture(toybox::Screen& screen, const fui::Rect& b, const char* label, const freeink::Icon* art) {
  auto& target = screen.target();
  target.fill(b, kLight, 10);
  target.stroke(b, kGrey, 2, 10);
  const int tw = widthOf(screen, label, Face::Large);
  const int iw = art ? 32 : 0;
  const int x = b.x + (b.width - iw - tw) / 2;
  if (art) icon(screen, rect(x, b.y + (b.height - 24) / 2, 24, 24), *art);
  say(screen, rect(x + iw, b.y, tw + 4, b.height), label, Face::Large, fui::TextAlign::Left);
}

// A pill that cannot be pressed right now: a grey frame and the words.
void pillDisabled(toybox::Screen& screen, const fui::Rect& b, const char* label) {
  screen.target().stroke(b, kGrey, 2, 10);
  say(screen, b, label, Face::Large);
}

// The title of a sheet: Astloch, centred, with a rule under it.
void title(toybox::Screen& screen, const char* text) {
  const int16_t w = screen.device().width;
  say(screen, rect(0, 14, w, 64), text, Face::Logo);
  screen.target().fill(rect(24, 88, w - 48, 2), kBlack);
  screen.setContentMarginAbsolute(fui::Insets{104, toybox::kMargin, toybox::kMargin, toybox::kMargin});
}

// A big headline in the death-screen face, or the smaller one if it is long.
void headline(toybox::Screen& screen, const char* text) {
  const int16_t w = screen.device().width;
  const Face f = widthOf(screen, text, Face::Title) <= w - 24 ? Face::Title : Face::Logo;
  say(screen, rect(0, 14, w, 92), text, f);
}

// ---- the table ----------------------------------------------------------------

constexpr int kTopBarH = 58;
constexpr int kBackdropTop = kTopBarH + 2;   // the cellar, 230 tall
constexpr int kEdgeY = 266;                  // the table's back edge, at his waist
constexpr int kWolfBottom = 318;             // his paws rest on the table
constexpr int kCardW = 86;
constexpr int kCardH = 124;
constexpr int kWolfRowY = 326;
constexpr int kYouRowY = 474;
constexpr int kLift = 12;
constexpr int kRowX = 102;
constexpr int kRowW = 370;
constexpr int kPileX = 8;
constexpr int kPileW = 84;
constexpr int kStripTop = 620;
constexpr int kStripH = 54;
constexpr int kButtonsTop = 724;

struct Placed {
  uint8_t card;
  int16_t x;
  int16_t y;
  bool mine;
};

bool selectedIn(const uint8_t* cards, const int n, const uint8_t c) {
  for (int i = 0; i < n; ++i)
    if (cards[i] == c) return true;
  return false;
}

void sortByValue(uint8_t* cards, const int n) {
  for (int i = 1; i < n; ++i) {
    const uint8_t c = cards[i];
    int j = i - 1;
    while (j >= 0 && (l::valueOf(cards[j]) > l::valueOf(c) || (l::valueOf(cards[j]) == l::valueOf(c) && cards[j] > c))) {
      cards[j + 1] = cards[j];
      --j;
    }
    cards[j + 1] = c;
  }
}

// Columns: the Wolf's cards in value order, each of yours under a Wolf card of
// the same value where there is one, the rest in the free columns and then in
// new ones on the right. Too many columns overlap, the left card on top, so
// every card still shows its value in its top-right corner.
struct Layout {
  Placed cards[2 * l::kCards];
  int n = 0;
  int16_t linkX[l::kCards];
  int links = 0;
  int16_t step = kCardW + 8;
};

void layoutTable(const TableModel& model, Layout& out) {
  const l::Match& m = model.match;
  uint8_t wolf[l::kCards];
  uint8_t you[l::kCards];
  std::memcpy(wolf, m.wolf, m.wolfLen);
  std::memcpy(you, m.you, m.youLen);
  sortByValue(wolf, m.wolfLen);
  sortByValue(you, m.youLen);
  int cols = m.wolfLen;
  int8_t mineAt[2 * l::kCards];
  for (int i = 0; i < 2 * l::kCards; ++i) mineAt[i] = -1;
  bool placed[l::kCards] = {};
  for (int i = 0; i < m.youLen; ++i) {
    for (int c = 0; c < m.wolfLen; ++c) {
      if (mineAt[c] < 0 && l::valueOf(wolf[c]) == l::valueOf(you[i])) {
        mineAt[c] = static_cast<int8_t>(i);
        placed[i] = true;
        break;
      }
    }
  }
  int free = 0;
  for (int i = 0; i < m.youLen; ++i) {
    if (placed[i]) continue;
    while (free < cols && mineAt[free] >= 0) ++free;
    if (free < cols) {
      mineAt[free] = static_cast<int8_t>(i);
    } else {
      mineAt[cols++] = static_cast<int8_t>(i);
      free = cols;
    }
  }
  if (cols < 1) cols = 1;
  out.step = static_cast<int16_t>(cols > 1 ? (kRowW - kCardW) / (cols - 1) : 0);
  if (out.step > kCardW + 8) out.step = kCardW + 8;
  const int total = kCardW + out.step * (cols - 1);
  const int x0 = kRowX + (kRowW - total) / 2;
  out.n = 0;
  out.links = 0;
  for (int c = 0; c < cols; ++c) {
    const int16_t x = static_cast<int16_t>(x0 + c * out.step);
    if (c < m.wolfLen) {
      const bool sel = selectedIn(model.sel.w, model.sel.nw, wolf[c]);
      out.cards[out.n++] = Placed{wolf[c], x, static_cast<int16_t>(kWolfRowY + (sel ? kLift : 0)), false};
    }
    if (mineAt[c] >= 0) {
      const uint8_t mine = you[mineAt[c]];
      const bool sel = selectedIn(model.sel.p, model.sel.np, mine);
      out.cards[out.n++] = Placed{mine, x, static_cast<int16_t>(kYouRowY - (sel ? kLift : 0)), true};
      if (c < m.wolfLen && l::valueOf(wolf[c]) == l::valueOf(mine)) {
        const int visible = (c == cols - 1 || out.step >= kCardW) ? kCardW : out.step;
        out.linkX[out.links++] = static_cast<int16_t>(x + visible / 2);
      }
    }
  }
}

void scoreBar(toybox::Screen& screen, const l::Match& m) {
  auto& target = screen.target();
  const int16_t w = screen.device().width;
  char line[24];
  int x = 12;
  const int midY = kTopBarH / 2;
  // The scores, and under them what they are racing to.
  constexpr int kScoreH = 36;
  auto counter = [&](const freeink::Icon& art, const int n) {
    icon(screen, rect(x, kScoreH / 2 - 10, 24, 24), art);
    std::snprintf(line, sizeof(line), "%d", n);
    const int tw = widthOf(screen, line, Face::Large);
    say(screen, rect(x + 28, 2, tw + 4, kScoreH), line, Face::Large, fui::TextAlign::Left);
    x += 28 + tw + 14;
  };
  counter(icon_lobo_wolf_24, m.wolfScore);
  counter(icon_lobo_you_24, m.youScore);
  std::snprintf(line, sizeof(line), "PLAYING TO %d", m.target);
  say(screen, rect(12, kScoreH, 160, 18), line, Face::Small, fui::TextAlign::Left);
  std::snprintf(line, sizeof(line), "Hand %d", m.hand);
  say(screen, rect(0, 0, w, kTopBarH), line, Face::Value);
  fui::ButtonProps menu;
  menu.icon = fui::bitmapFromIcon(icon_lobo_menu_24);
  menu.action = ActionMenu;
  menu.styles = toybox::rowStyles();
  menu.radius = 8;
  screen.button(menu, rect(w - 12 - 46, midY - 23, 46, 46));
  target.fill(rect(0, kTopBarH, w, 2), kBlack);
}

// The table's wood: dotted grain and a few plank joints.
void planks(toybox::Screen& screen, const int top, const int bottom) {
  auto& target = screen.target();
  const int16_t w = screen.device().width;
  int row = 0;
  for (int y = top + 22; y < bottom; y += 26, ++row) {
    target.fill(rect(0, y, w, 1), kGrey);
    for (int x = (row % 2) ? 70 : 170; x < w; x += 200) target.fill(rect(x, y - 25, 1, 25), kGrey);
  }
}

// The cellar, the table's edge and the Wolf with his paws on it.
void wolfScene(toybox::Screen& screen, const int top, const int tableBottom, const bool withWolf) {
  auto& target = screen.target();
  const int16_t w = screen.device().width;
  icon(screen, rect(0, top, lobo_backdrop.w, lobo_backdrop.h), lobo_backdrop);
  const int edge = top + (kEdgeY - kBackdropTop);
  target.fill(rect(0, edge, w, tableBottom - edge), kWhite);
  planks(screen, edge, tableBottom);
  target.fill(rect(0, edge, w, 8), kLight);
  target.fill(rect(0, edge, w, 2), kBlack);
  target.fill(rect(0, edge + 8, w, 1), kBlack);
  if (withWolf) {
    const int bottom = top + (kWolfBottom - kBackdropTop);
    figure(screen, (w - wolf_small_ink.w) / 2, bottom - wolf_small_ink.h, wolf_small_mask, wolf_small_ink);
  }
}

// The deck, face up, halfway between the two rows. The played pile is not
// shown; the deck's tally says how many cards have gone.
constexpr int kDeckY = (kWolfRowY + kYouRowY) / 2;

void piles(toybox::Screen& screen, const l::Match& m) {
  char line[8];
  const fui::Rect deck = rect(kPileX, kDeckY, kPileW, kCardH);
  if (m.deckLen > 0) {
    if (m.deckLen > 1) drawBack(screen, rect(deck.x + 3, deck.y - 3, deck.width, deck.height));
    drawCard(screen, deck, m.deck[m.deckLen - 1]);
    std::snprintf(line, sizeof(line), "%d", m.deckLen);
    badge(screen, deck, line);
  } else {
    cardart::drawCardSlot(screen, deck);
    say(screen, deck, "EMPTY", Face::Small);
  }
}

void appendf(char* out, const int size, const char* fmt, const int a = 0, const int b = 0) {
  const size_t used = std::strlen(out);
  if (static_cast<int>(used) >= size - 1) return;
  std::snprintf(out + used, static_cast<size_t>(size) - used, fmt, a, b);
}

// "3 + 4" for a set of cards, in value order.
void plusList(char* out, const int size, const uint8_t* cards, const int n) {
  uint8_t sorted[kMaxSelect];
  const int k = n > kMaxSelect ? kMaxSelect : n;
  std::memcpy(sorted, cards, static_cast<size_t>(k));
  sortByValue(sorted, k);
  for (int i = 0; i < k; ++i) appendf(out, size, i ? " + %d" : "%d", l::valueOf(sorted[i]));
}

const char* moveName(const l::Move m) {
  switch (m) {
    case l::Move::Perfect:
      return "Perfect";
    case l::Move::Sum:
      return "Sum";
    case l::Move::Split:
      return "Split";
    case l::Move::Over:
      return "Over";
    default:
      return "Play";
  }
}

void nextCard(char* out, const int size, const l::Match& m, const bool you) {
  const char* who = you ? "you take" : "the Wolf takes";
  if (m.deckLen > 0) {
    appendf(out, size, "");
    const size_t used = std::strlen(out);
    std::snprintf(out + used, static_cast<size_t>(size) - used, "%s the next card, a %d.", who,
                  l::valueOf(m.deck[m.deckLen - 1]));
  } else if (m.playedLen > 0) {
    const size_t used = std::strlen(out);
    std::snprintf(out + used, static_cast<size_t>(size) - used, "%s a card from the reshuffled pile.", who);
  } else {
    appendf(out, size, "no cards are left to draw.");
  }
}

}  // namespace

void useFonts(FontBinder* binder) { gFonts = binder; }

void messageText(const TableModel& model, char* out, const int size) {
  const l::Match& m = model.match;
  const Selection& s = model.sel;
  out[0] = '\0';
  if (m.phase != l::Phase::Play) return;
  if (!l::anyMove(m)) {
    std::snprintf(out, static_cast<size_t>(size), "No legal move left. You must fold.");
    return;
  }
  if (s.np == 0 && s.nw == 0) {
    // What the last move did, since nothing flew across the table.
    if (m.last != l::Move::None) {
      char a[40] = "";
      char b[40] = "";
      plusList(a, sizeof(a), m.lastP, m.lastPLen);
      plusList(b, sizeof(b), m.lastW, m.lastWLen);
      std::snprintf(out, static_cast<size_t>(size), "%s: %s took %s. ", moveName(m.last), a, b);
      if (m.drew == 0) {
        appendf(out, size, "No cards were left to draw.");
      } else if (m.drewYou) {
        appendf(out, size, "You drew a card.");
      } else {
        appendf(out, size, m.drew == 1 ? "The Wolf drew %d card." : "The Wolf drew %d cards.", m.drew);
      }
      if (m.reshuffled) appendf(out, size, " The played cards were shuffled in.");
      return;
    }
    std::snprintf(out, static_cast<size_t>(size), "Pick your card, then the Wolf's.");
    return;
  }
  const l::Verdict v = l::classify(m, s.p, s.np, s.w, s.nw);
  char a[40] = "";
  char b[40] = "";
  plusList(a, sizeof(a), s.p, s.np);
  plusList(b, sizeof(b), s.w, s.nw);
  if (v.ok) {
    std::snprintf(out, static_cast<size_t>(size), "%s: %s takes %s, ", moveName(v.move), a, b);
    if (v.move == l::Move::Over) {
      appendf(out, size, v.over == 1 ? "the Wolf takes %d card." : "the Wolf takes %d cards.", v.over);
    } else {
      nextCard(out, size, m, v.move == l::Move::Perfect || v.move == l::Move::Sum);
    }
    if (s.nw == m.wolfLen && (v.move == l::Move::Perfect || v.move == l::Move::Sum)) {
      const int left = l::sum(m.you, m.youLen) - l::sum(s.p, s.np) + (m.deckLen ? l::valueOf(m.deck[m.deckLen - 1]) : 0);
      appendf(out, size, " You clear the Wolf for %d.", left);
    }
    return;
  }
  const int ps = l::sum(s.p, s.np);
  const int ws = l::sum(s.w, s.nw);
  if (s.np > 0 && s.nw > 0) {
    if (s.np > 1 && s.nw > 1) {
      std::snprintf(out, static_cast<size_t>(size), "Play one card against several, or several against one.");
    } else if (s.np == 1 && s.nw == 1) {
      std::snprintf(out, static_cast<size_t>(size), "Your %d can't take a %d.", ps, ws);
    } else if (s.np > 1) {
      std::snprintf(out, static_cast<size_t>(size), "%s = %d, not %d.", a, ps, ws);
    } else {
      std::snprintf(out, static_cast<size_t>(size), "%s = %d, not %d.", b, ws, ps);
    }
    return;
  }
  if (s.np == 1) {
    std::snprintf(out, static_cast<size_t>(size), "Your %d: now pick the Wolf's card or cards.", ps);
  } else if (s.np > 1) {
    std::snprintf(out, static_cast<size_t>(size), "%s = %d: now pick a Wolf %d.", a, ps, ps);
  } else if (s.nw == 1) {
    std::snprintf(out, static_cast<size_t>(size), "The Wolf's %d: now pick your card or cards.", ws);
  } else {
    std::snprintf(out, static_cast<size_t>(size), "%s = %d: now pick your %d.", b, ws, ws);
  }
}

int cardAt(const fui::DeviceContext&, const TableModel& model, const int x, const int y) {
  // Static: a layout is ~700 bytes, too much for the stack of the input task.
  static Layout lay;
  layoutTable(model, lay);
  // Left cards lie on top, so search from the left.
  for (int i = 0; i < lay.n; ++i) {
    const Placed& p = lay.cards[i];
    if (x >= p.x && x < p.x + kCardW && y >= p.y && y < p.y + kCardH) {
      // Under an overlap only the visible strip counts.
      bool covered = false;
      for (int j = 0; j < i; ++j) {
        const Placed& q = lay.cards[j];
        if (q.mine == p.mine && x >= q.x && x < q.x + kCardW && y >= q.y && y < q.y + kCardH) covered = true;
      }
      if (covered) continue;
      return p.mine ? p.card + 1 : -(p.card + 1);
    }
  }
  return 0;
}

bool pilesAt(const fui::DeviceContext&, const int x, const int y) {
  return x >= kPileX && x < kPileX + kPileW && y >= kDeckY - 4 && y < kDeckY + kCardH + 18;
}

void buildTable(toybox::Screen& screen, const TableModel& model) {
  const l::Match& m = model.match;
  auto& target = screen.target();
  const int16_t w = screen.device().width;
  toybox::absoluteChrome(screen);

  wolfScene(screen, kBackdropTop, kStripTop, true);
  scoreBar(screen, m);
  piles(screen, m);

  static Layout lay;  // see cardAt
  layoutTable(model, lay);
  // Threads between equal values, in the gap between the rows.
  for (int i = 0; i < lay.links; ++i) {
    for (int y = kWolfRowY + kCardH + 2; y < kYouRowY - 2; y += 4)
      target.fill(rect(lay.linkX[i] - 1, y, 3, 2), kBlack);
  }
  // Fades for cards that cannot join the pick.
  const bool picking = model.hints && (model.sel.np > 0 || model.sel.nw > 0);
  // Right to left, so the left card lies on top.
  for (int i = lay.n - 1; i >= 0; --i) {
    const Placed& p = lay.cards[i];
    CardLook look;
    look.selected = p.mine ? selectedIn(model.sel.p, model.sel.np, p.card) : selectedIn(model.sel.w, model.sel.nw, p.card);
    look.fresh = (m.fresh >> p.card) & 1;
    look.tight = lay.step < 44;
    if (picking && !look.selected) {
      Selection s = model.sel;
      if (p.mine && s.np < kMaxSelect) {
        s.p[s.np++] = p.card;
      } else if (!p.mine && s.nw < kMaxSelect) {
        s.w[s.nw++] = p.card;
      }
      look.faded = !l::extendable(m, s.p, s.np, s.w, s.nw);
    }
    drawCard(screen, rect(p.x, p.y, kCardW, kCardH), p.card, look);
  }

  // The message bar.
  const fui::Rect strip = rect(0, kStripTop, w, kStripH);
  target.fill(strip, kBlack);
  char msg[160];
  messageText(model, msg, sizeof(msg));
  prose(screen, rect(12, strip.y + 4, w - 24, strip.height - 8), msg, Face::Body, 2, true);

  // FOLD and the move.
  const int side = 12;
  const int half = (w - side * 3) / 2;
  const fui::Rect foldRect = rect(side, kButtonsTop, half, toybox::kPillHeight);
  const fui::Rect playRect = rect(side * 2 + half, kButtonsTop, half, toybox::kPillHeight);
  if (m.phase != l::Phase::Play) return;
  const bool must = !l::anyMove(m);
  fui::ButtonProps foldButton = pill("Fold", ActionFold, must);
  foldButton.icon = fui::bitmapFromIcon(icon_lobo_forearm_24);
  foldButton.gap = 8;
  screen.button(foldButton, foldRect);
  const l::Verdict v = l::classify(m, model.sel.p, model.sel.np, model.sel.w, model.sel.nw);
  if (v.ok) {
    screen.button(pill(moveName(v.move), ActionPlay), playRect);
  } else {
    pillDisabled(screen, playRect, "Play");
  }
}

// ---- menu, setup, deck ------------------------------------------------------------

void buildMenu(toybox::Screen& screen, const MenuModel& model) {
  toybox::absoluteChrome(screen);
  title(screen, "Lobo");
  char record[64];
  if (model.canContinue) {
    std::snprintf(record, sizeof(record), "Wolf %d    You %d    playing to %d", model.wolfScore, model.youScore,
                  model.target);
  } else {
    std::snprintf(record, sizeof(record), "%d matches    %d won", model.matches, model.won);
  }
  const fui::Rect line = screen.takeTop(26);
  say(screen, line, record, Face::Body);

  fui::ListItem rows[static_cast<int>(MenuRow::Count)] = {};
  int16_t count = 0;
  if (model.canContinue) {
    rows[count].label = "Back to the table";
    rows[count++].actionValue = static_cast<int16_t>(MenuRow::Continue);
  }
  rows[count].label = "New match";
  rows[count++].actionValue = static_cast<int16_t>(MenuRow::NewMatch);
  rows[count].label = "How to play";
  rows[count++].actionValue = static_cast<int16_t>(MenuRow::HowTo);
  fui::ListProps list;
  list.items = rows;
  list.count = static_cast<uint16_t>(count);
  list.selectedIndex = 0;
  list.action = ActionMenuRow;
  const int16_t listHeight =
      static_cast<int16_t>(count * toybox::kRowHeight + (count - 1) * toybox::kGutter / 2 + toybox::kGutter);
  const fui::Rect content = screen.contentRect();
  const int creditH = 30;
  const int listTop = content.bottom() - listHeight - creditH;
  say(screen, rect(content.x, content.bottom() - creditH + 6, content.width, 24),
      "Lobo by James Ernest, Crab Fragment Labs, 2022", Face::Small);
  screen.takeBottom(creditH, 0);
  screen.list(list, listHeight, fui::LayoutAnchor::Bottom);

  // The Wolf, waiting at his table: as large as the room allows.
  const int areaTop = line.bottom() + 8;
  const int room = listTop - areaTop;
  const bool big = room >= wolf_big_ink.h;
  const freeink::Icon& ink = big ? wolf_big_ink : wolf_small_ink;
  const freeink::Icon& mask = big ? wolf_big_mask : wolf_small_mask;
  if (room >= ink.h) {
    const int16_t w = screen.device().width;
    const int y = areaTop + (room - ink.h) / 2;
    const int edge = y + ink.h * 79 / 100;  // his waist
    screen.target().fill(rect(24, edge, w - 48, 2), kBlack);
    figure(screen, (w - ink.w) / 2, y, mask, ink);
  }
}

void buildSetup(toybox::Screen& screen, const SetupModel& model) {
  auto& target = screen.target();
  toybox::absoluteChrome(screen);
  title(screen, "Sit with the Wolf");
  const fui::Rect c = screen.contentRect();
  say(screen, rect(c.x, 108, c.width, 28), "Choose how hard the Wolf plays.", Face::Body);
  const char* names[3] = {"Hard", "Medium", "Easy"};
  const char* notes[3] = {"All 50 cards", "The 10s are removed", "The 9s and 10s are removed"};
  int y = 150;
  for (int i = 0; i < 3; ++i) {
    const fui::Rect r = rect(c.x, y, c.width, 64);
    const bool on = static_cast<int>(model.level) == i;
    fui::ButtonProps b;
    b.label = "";
    b.action = ActionLevel;
    b.value = static_cast<int16_t>(i);
    b.styles = toybox::rowStyles();
    b.state = on ? fui::StateSelected : fui::StateNormal;
    b.radius = 10;
    screen.button(b, r);
    target.fill(r, on ? kBlack : kWhite, 10);
    target.stroke(r, kBlack, 2, 10);
    say(screen, rect(r.x + 16, r.y, 160, r.height), names[i], Face::Value, fui::TextAlign::Left, on);
    say(screen, rect(r.right() - 16 - 260, r.y, 260, r.height), notes[i], Face::Small, fui::TextAlign::Right, on);
    y += 76;
  }
  // Play to: - 100 +
  y += 10;
  say(screen, rect(c.x, y, 140, 56), "Play to", Face::Large, fui::TextAlign::Left);
  screen.button(pill("-", ActionTargetDown, false), rect(c.right() - 56 - 120 - 56, y, 56, 56));
  char t[8];
  std::snprintf(t, sizeof(t), "%d", model.target);
  say(screen, rect(c.right() - 56 - 120, y, 120, 56), t, Face::ValueBig);
  screen.button(pill("+", ActionTargetUp, false), rect(c.right() - 56, y, 56, 56));
  y += 70;
  prose(screen, rect(c.x, y, c.width, 60),
        "The original game is played to 100. Pick a lower number for a shorter match.", Face::Body, 2);

  const fui::Rect bar = screen.takeBottom(toybox::kPillHeight, toybox::kGutter);
  if (model.cancelable) {
    const int half = (bar.width - toybox::kGutter) / 2;
    screen.button(pill("Back", ActionSetupBack, false), rect(bar.x, bar.y, half, bar.height));
    screen.button(pill("Deal the cards", ActionDeal), rect(bar.right() - half, bar.y, half, bar.height));
  } else {
    screen.button(pill("Deal the cards", ActionDeal), bar);
  }
}

void buildTally(toybox::Screen& screen, const l::Match& m) {
  auto& target = screen.target();
  toybox::absoluteChrome(screen);
  title(screen, "What is left");
  int counts[11];
  l::tally(m, counts);
  const int top = l::maxValue(m.level);
  const fui::Rect c = screen.contentRect();
  const int bw = 80, bh = 104, gap = 10;
  const int perRow = 5;
  const int x0 = c.x + (c.width - perRow * bw - (perRow - 1) * gap) / 2;
  for (int v = 1; v <= top; ++v) {
    const int i = v - 1;
    const fui::Rect r = rect(x0 + (i % perRow) * (bw + gap), 130 + (i / perRow) * (bh + gap), bw, bh);
    char num[8];
    char left[12];
    std::snprintf(num, sizeof(num), "%d", v);
    if (counts[v]) {
      target.fill(r, kWhite, kRadius);
      target.stroke(r, kBlack, 2, kRadius);
      std::snprintf(left, sizeof(left), "%d LEFT", counts[v]);
    } else {
      target.fill(r, kLight, kRadius);
      target.stroke(r, kGrey, 2, kRadius);
      std::snprintf(left, sizeof(left), "GONE");
    }
    say(screen, rect(r.x, r.y + 10, r.width, 56), num, Face::ValueBig);
    say(screen, rect(r.x, r.bottom() - 30, r.width, 22), left, Face::Small);
  }
  char line[96];
  const int rows = (top + perRow - 1) / perRow;
  int y = 130 + rows * (bh + gap) + 20;
  std::snprintf(line, sizeof(line), "%d cards in the deck, %d played.", m.deckLen, m.playedLen);
  say(screen, rect(c.x, y, c.width, 30), line, Face::Large);
  std::snprintf(line, sizeof(line), "The deck is reshuffled after hand %d.", l::reshuffleAfter(m));
  say(screen, rect(c.x, y + 34, c.width, 30), line, Face::Large);
  prose(screen, rect(c.x, y + 80, c.width, 60),
        "Until then every hand is dealt from what is left, so the cards played are gone.", Face::Body, 2);
  screen.button(pill("Back to the table", ActionBackToTable), screen.takeBottom(toybox::kPillHeight, toybox::kGutter));
}

void buildConfirm(toybox::Screen& screen) {
  toybox::absoluteChrome(screen);
  title(screen, "Leave this match?");
  screen.button(pill("Keep playing", ActionKeepPlaying, false), screen.takeBottom(toybox::kPillHeight, toybox::kGutter));
  screen.button(pill("Start a new match", ActionAbandon), screen.takeBottom(toybox::kPillHeight, toybox::kGutter));
  const fui::Rect area = screen.body();
  prose(screen, rect(area.x, area.y + 30, area.width, 60), "The scores of this match will be lost.", Face::Large, 2);
}

// ---- the ends of hands and matches --------------------------------------------------

namespace {

// The cellar and the table; the Wolf at it, or only his hat left behind.
void endPicture(toybox::Screen& screen, const int top, const bool wolfThere) {
  const int16_t w = screen.device().width;
  const int tableBottom = top + 300;
  wolfScene(screen, top, tableBottom, wolfThere);
  if (!wolfThere) figure(screen, (w - wolf_hat_ink.w) / 2, tableBottom - 14 - wolf_hat_ink.h, wolf_hat_mask, wolf_hat_ink);
  screen.target().fill(rect(0, tableBottom, w, 2), kBlack);
}

void scoresLine(toybox::Screen& screen, const int y, const l::Match& m) {
  char line[48];
  std::snprintf(line, sizeof(line), "Wolf %d    You %d", m.wolfScore, m.youScore);
  say(screen, rect(0, y, screen.device().width, 32), line, Face::Large);
}

}  // namespace

void buildHandOver(toybox::Screen& screen, const l::Match& m) {
  toybox::absoluteChrome(screen);
  const int16_t w = screen.device().width;
  headline(screen, m.youTookHand ? "You beat the Wolf" : "The Wolf scores");
  endPicture(screen, 108, !m.youTookHand);
  char line[160];
  std::snprintf(line, sizeof(line), "+%d", m.handPoints);
  say(screen, rect(0, 416, w, 60), line, Face::ValueBig);
  scoresLine(screen, 480, m);
  if (m.youTookHand) {
    std::snprintf(line, sizeof(line), "%s",
                  m.handPoints ? "The points left in your hand go to you." : "An empty hand of your own: clean, but worth nothing.");
  } else {
    std::snprintf(line, sizeof(line), "%s",
                  m.handPoints <= 6 ? "A cheap fold. The Wolf gets little." : "Every point still in the Wolf's hand is his.");
  }
  const int left = l::kReshuffleEvery - m.since;
  const size_t used = std::strlen(line);
  if (left > 0 && m.deckLen + m.youLen + m.wolfLen >= 9) {
    std::snprintf(line + used, sizeof(line) - used, " The next hand uses the same deck.");
  } else {
    std::snprintf(line + used, sizeof(line) - used, " The deck will be reshuffled.");
  }
  const fui::Rect c = screen.contentRect();
  prose(screen, rect(c.x, 516, c.width, 60), line, Face::Body, 2);
  screen.setContentMarginAbsolute(fui::Insets{590, toybox::kMargin, toybox::kMargin, toybox::kMargin});
  screen.button(pill("Menu", ActionToMenu, false), screen.takeBottom(toybox::kPillHeight, toybox::kGutter));
  screen.button(pill("Next hand", ActionNextHand), screen.takeBottom(toybox::kPillHeight, toybox::kGutter));
}

void buildMatchOver(toybox::Screen& screen, const l::Match& m) {
  toybox::absoluteChrome(screen);
  const int16_t w = screen.device().width;
  const bool won = l::youWonMatch(m);
  headline(screen, won ? "The Wolf retreats" : "The Wolf wins");
  endPicture(screen, 108, !won);
  char line[96];
  std::snprintf(line, sizeof(line), "%d", won ? m.youScore : m.wolfScore);
  say(screen, rect(0, 416, w, 60), line, Face::ValueBig);
  scoresLine(screen, 480, m);
  std::snprintf(line, sizeof(line), m.hand == 1 ? "Match to %d over %d hand." : "Match to %d over %d hands.", m.target,
                m.hand);
  say(screen, rect(0, 516, w, 30), line, Face::Body);
  screen.setContentMarginAbsolute(fui::Insets{590, toybox::kMargin, toybox::kMargin, toybox::kMargin});
  screen.button(pill("Menu", ActionToMenu, false), screen.takeBottom(toybox::kPillHeight, toybox::kGutter));
  screen.button(pill("New match", ActionNewMatch), screen.takeBottom(toybox::kPillHeight, toybox::kGutter));
}

// ---- how to play ----------------------------------------------------------------------

namespace {

const char* const kLessons[] = {
    "Clear every card from the Wolf's hand. The Wolf never acts: every move is yours, and every move has a price.",
    "Perfect: one of your cards takes a Wolf card of the same value. You draw the top card.",
    "Sum: several of your cards that add up exactly to one Wolf card take it. You draw the top card.",
    "Split: one of your cards takes several Wolf cards that add up exactly to it. The Wolf draws the top card.",
    "Over: one of your cards takes one smaller Wolf card. The Wolf draws one card for every point of difference.",
    "Tap your card or cards, then the Wolf's. The bar says what will happen and the button plays it. Cards that "
    "cannot join your pick fade.",
    "Empty the Wolf's hand and you score what is left in yours. Fold and the Wolf scores what is left in its hand. "
    "First to the target wins.",
    "The top card of the deck is always face up. The deck is reshuffled only every third hand, so remember what has "
    "gone. Tap the deck to see what is left.",
};

void caption(toybox::Screen& screen, const int cx, const int y, const char* text) {
  say(screen, rect(cx - 220, y, 440, sf::kSmallCut.lineHeight), text, Face::Small);
}

// The Wolf's cards above, yours below, a thread between equal values.
void exampleRows(toybox::Screen& screen, const fui::Rect& area, const uint8_t* wolf, const int nw, const uint8_t* you,
                 const int ny, const char* note) {
  const int cw = 96, ch = 136, gap = 12;
  const int n = nw > ny ? nw : ny;
  const int x0 = area.x + (area.width - n * cw - (n - 1) * gap) / 2;
  const int wy = area.y;
  const int yy = area.y + ch + 34;
  screen.target().fill(rect(area.x, wy - 6, area.width, 1), kGrey);
  say(screen, rect(area.x, wy + ch / 2 - 10, 60, 20), "WOLF", Face::Small, fui::TextAlign::Left);
  say(screen, rect(area.x, yy + ch / 2 - 10, 60, 20), "YOU", Face::Small, fui::TextAlign::Left);
  for (int i = 0; i < nw; ++i) drawCard(screen, rect(x0 + i * (cw + gap), wy, cw, ch), wolf[i]);
  for (int i = 0; i < ny; ++i) drawCard(screen, rect(x0 + i * (cw + gap), yy, cw, ch), you[i]);
  if (nw == 1 && ny == 1 && l::valueOf(wolf[0]) == l::valueOf(you[0])) {
    for (int y = wy + ch + 4; y < yy - 3; y += 4) screen.target().fill(rect(x0 + cw / 2 - 1, y, 3, 2), kBlack);
  }
  caption(screen, area.x + area.width / 2, yy + ch + 16, note);
}

void lessonTap(toybox::Screen& screen, const fui::Rect& area) {
  // A small table: the Wolf's 4 picked, a 9 of yours picked, a card that
  // cannot join faded; the bar and the button as they would read.
  const int cw = kCardW, ch = kCardH;
  const int x0 = area.x + (area.width - 3 * cw - 2 * 14) / 2;
  const uint8_t wolf[3] = {3, 17, 28};   // 4 moon, 8 paw, 9 fang
  const uint8_t you[2] = {38, 1};        // 9 pine, 2 moon
  for (int i = 0; i < 3; ++i) {
    CardLook look;
    look.selected = i == 0;
    look.faded = i == 2;
    drawCard(screen, rect(x0 + i * (cw + 14), area.y + (look.selected ? kLift : 0), cw, ch), wolf[i], look);
  }
  for (int i = 0; i < 2; ++i) {
    CardLook look;
    look.selected = i == 0;
    look.faded = i == 1;
    drawCard(screen, rect(x0 + i * (cw + 14), area.y + ch + 30 - (look.selected ? kLift : 0), cw, ch), you[i], look);
  }
  const fui::Rect strip = rect(area.x, area.y + 2 * ch + 46, area.width, 48);
  screen.target().fill(strip, kBlack, 6);
  prose(screen, rect(strip.x + 8, strip.y + 4, strip.width - 16, 40), "Over: 9 takes 4, the Wolf takes 5 cards.",
        Face::Body, 2, true);
  pillPicture(screen, rect(area.x + area.width - 180, strip.bottom() + 14, 180, toybox::kPillHeight), "Over", nullptr);
  caption(screen, area.x + (area.width - 180) / 2, strip.bottom() + 30, "THE BUTTON PLAYS IT");
}

void lessonScore(toybox::Screen& screen, const fui::Rect& area) {
  const int cw = 84, ch = 120, gap = 10;
  const int colW = area.width / 2;
  // Left: the Wolf cleared, your 2 and 5 are yours.
  const uint8_t mine[2] = {11, 4};  // 2 paw, 5 moon
  int x = area.x + (colW - 2 * cw - gap) / 2;
  say(screen, rect(area.x, area.y, colW, 24), "WOLF CLEARED", Face::Small);
  for (int i = 0; i < 2; ++i) drawCard(screen, rect(x + i * (cw + gap), area.y + 30, cw, ch), mine[i]);
  say(screen, rect(area.x, area.y + 30 + ch + 10, colW, 40), "+7 to you", Face::Large);
  // Right: you fold with the Wolf holding 8 and 9.
  const uint8_t his[2] = {27, 48};  // 8 fang, 9 bone
  x = area.x + colW + (colW - 2 * cw - gap) / 2;
  say(screen, rect(area.x + colW, area.y, colW, 24), "YOU FOLD", Face::Small);
  for (int i = 0; i < 2; ++i) drawCard(screen, rect(x + i * (cw + gap), area.y + 30, cw, ch), his[i]);
  say(screen, rect(area.x + colW, area.y + 30 + ch + 10, colW, 40), "+17 to the Wolf", Face::Large);
  pillPicture(screen, rect(area.x + (area.width - 220) / 2, area.y + ch + 100, 220, toybox::kPillHeight), "Fold",
              &icon_lobo_forearm_24);
  caption(screen, area.x + area.width / 2, area.y + ch + 100 + toybox::kPillHeight + 12,
          "FOLDING EARLY KEEPS THE WOLF'S SCORE DOWN");
}

void lessonDeck(toybox::Screen& screen, const fui::Rect& area) {
  const int cw = 96, ch = 136;
  const fui::Rect deck = rect(area.x + 40, area.y + 10, cw, ch);
  drawBack(screen, rect(deck.x + 3, deck.y - 3, cw, ch));
  drawCard(screen, deck, 36);  // 7 pine on top
  badge(screen, deck, "31");
  caption(screen, deck.x + cw / 2, deck.bottom() + 24, "THE NEXT CARD");
  // A sample of the tally beside it.
  auto& target = screen.target();
  const int bx = deck.right() + 40;
  const int vals[3] = {1, 7, 10};
  const int left[3] = {2, 3, 0};
  for (int i = 0; i < 3; ++i) {
    const fui::Rect r = rect(bx + i * 86, area.y + 24, 76, 96);
    if (left[i]) {
      target.fill(r, kWhite, kRadius);
      target.stroke(r, kBlack, 2, kRadius);
    } else {
      target.fill(r, kLight, kRadius);
      target.stroke(r, kGrey, 2, kRadius);
    }
    char num[8];
    char note[12];
    std::snprintf(num, sizeof(num), "%d", vals[i]);
    if (left[i]) {
      std::snprintf(note, sizeof(note), "%d LEFT", left[i]);
    } else {
      std::snprintf(note, sizeof(note), "GONE");
    }
    say(screen, rect(r.x, r.y + 8, r.width, 50), num, Face::ValueBig);
    say(screen, rect(r.x, r.bottom() - 28, r.width, 22), note, Face::Small);
  }
  caption(screen, bx + 124, area.y + 24 + 96 + 24, "WHAT IS LEFT");
}

void lessonSuits(toybox::Screen& screen, const fui::Rect& area) {
  const uint8_t cards[5] = {2, 16, 21, 34, 48};  // 3 moon, 7 paw, 2 fang, 5 pine, 9 bone
  const int cw = 82, ch = 118, gap = 8;
  const int x0 = area.x + (area.width - 5 * cw - 4 * gap) / 2;
  for (int i = 0; i < 5; ++i) drawCard(screen, rect(x0 + i * (cw + gap), area.y, cw, ch), cards[i]);
  caption(screen, area.x + area.width / 2, area.y + ch + 14, "50 CARDS, 1 TO 10 IN FIVE SUITS");
  caption(screen, area.x + area.width / 2, area.y + ch + 36, "SUITS DON'T MATTER IN PLAY");
  // Four each, the Wolf's first.
  const int y = area.y + ch + 76;
  for (int i = 0; i < 4; ++i) drawBack(screen, rect(area.x + 40 + i * 30, y, 70, 100));
  for (int i = 0; i < 4; ++i) drawBack(screen, rect(area.x + area.width - 40 - 70 - i * 30, y, 70, 100));
  caption(screen, area.x + 40 + 80, y + 112, "THE WOLF'S FOUR");
  caption(screen, area.x + area.width - 120, y + 112, "YOUR FOUR");
}

}  // namespace

int howToPages() { return static_cast<int>(sizeof(kLessons) / sizeof(kLessons[0])); }

void buildHowTo(toybox::Screen& screen, const HowToModel& model) {
  const int pages = howToPages();
  const int page = model.page < 0 ? 0 : (model.page >= pages ? pages - 1 : model.page);
  toybox::absoluteChrome(screen);
  title(screen, "How to play");
  const fui::Rect bar = screen.takeBottom(toybox::kPillHeight, toybox::kGutter);
  const int half = (bar.width - toybox::kGutter) / 2;
  screen.button(pill("Back", ActionHowToBack, false), rect(bar.x, bar.y, half, bar.height));
  screen.button(pill(page + 1 < pages ? "Next" : "To the table", ActionHowToNext),
                rect(bar.right() - half, bar.y, half, bar.height));
  char progress[32];
  std::snprintf(progress, sizeof(progress), "%d of %d", page + 1, pages);
  say(screen, screen.takeBottom(28, 4), progress, Face::Small);

  const fui::Rect area = screen.body();
  prose(screen, rect(area.x, area.y, area.width, 130), kLessons[page], Face::Large, 4);
  const fui::Rect ex = rect(area.x, area.y + 140, area.width, area.height - 140);
  const uint8_t seven = 6, sevenB = 36, three = 12, four = 23, five = 44, nine = 38, fourW = 3;
  switch (page) {
    case 0:
      lessonSuits(screen, ex);
      break;
    case 1: {
      const uint8_t w[1] = {seven};
      const uint8_t y[1] = {sevenB};
      exampleRows(screen, ex, w, 1, y, 1, "7 TAKES 7, YOU DRAW");
      break;
    }
    case 2: {
      const uint8_t w[1] = {seven};
      const uint8_t y[2] = {three, four};
      exampleRows(screen, ex, w, 1, y, 2, "3 + 4 TAKE 7, YOU DRAW");
      break;
    }
    case 3: {
      const uint8_t w[2] = {fourW, five};
      const uint8_t y[1] = {nine};
      exampleRows(screen, ex, w, 2, y, 1, "9 TAKES 4 + 5, THE WOLF DRAWS ONE");
      break;
    }
    case 4: {
      const uint8_t w[1] = {fourW};
      const uint8_t y[1] = {nine};
      exampleRows(screen, ex, w, 1, y, 1, "9 TAKES 4, THE WOLF DRAWS FIVE");
      break;
    }
    case 5:
      lessonTap(screen, ex);
      break;
    case 6:
      lessonScore(screen, ex);
      break;
    default:
      lessonDeck(screen, ex);
      break;
  }
}

}  // namespace loboui
