#include "TabletopScreens.h"

#include <FreeInkUIIcon.h>

#include <cstdio>
#include <cstring>
#include <initializer_list>

#include "TabletopArt.h"

namespace tabletopui {

namespace {

namespace tt = tabletop;

const fui::Paint kBlack = fui::Paint::solid(fui::Color::Black);
const fui::Paint kWhite = fui::Paint::solid(fui::Color::White);
const fui::Paint kGrey = fui::Paint::dither(fui::Color::DarkGray);
constexpr fui::FontId kTextSlot = 3;
constexpr int kButtonsY = 732;
FontBinder* gFonts = nullptr;

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

fui::Rect rect(const int x, const int y, const int w, const int h) {
  return fui::makeRect(static_cast<int16_t>(x), static_cast<int16_t>(y), static_cast<int16_t>(w),
                       static_cast<int16_t>(h));
}

void say(toybox::Screen& screen, const fui::Rect& box, const char* text, const Face f,
         const fui::TextAlign align = fui::TextAlign::Center, const bool paper = false) {
  screen.target().text(toybox::inkCentred(box, cutOf(f)), text, face(f, align, paper));
}

int widthOf(toybox::Screen& screen, const char* text, const Face f) {
  const fui::TextStyle t = face(f);
  return screen.target().measureText(t.font, text, t).width;
}

// The largest of `faces` the text fits in, falling back to the last.
Face fit(toybox::Screen& screen, const char* text, const int width, std::initializer_list<Face> faces) {
  Face last = Face::Tile;
  for (const Face f : faces) {
    last = f;
    if (widthOf(screen, text, f) <= width) return f;
  }
  return last;
}

// Centred lines, up to `lines`.
void prose(toybox::Screen& screen, const fui::Rect& box, const char* text, const Face f, const uint8_t lines) {
  fui::TextStyle t = face(f);
  t.maxLines = lines;
  screen.target().text(box, text, t);
}

void icon(toybox::Screen& screen, const fui::Rect& box, const freeink::Icon& art, const bool paper = false) {
  screen.target().bitmap(box, fui::bitmapFromIcon(art), fui::BitmapMode::Center, paper ? kWhite : kBlack);
}

void iconAt(toybox::Screen& screen, const int cx, const int y, const freeink::Icon& art) {
  icon(screen, rect(cx - art.w / 2, y, art.w, art.h), art);
}

fui::ButtonProps pill(const char* label, const fui::ActionId action, const bool solid = true, const int value = 0) {
  fui::ButtonProps b;
  b.label = label;
  b.action = action;
  b.value = static_cast<int16_t>(value);
  b.radius = 10;
  if (!solid) b.styles = toybox::rowStyles();
  return b;
}

// A tappable region drawn by hand afterwards.
void hotspot(toybox::Screen& screen, const fui::Rect& r, const fui::ActionId action, const int value = 0) {
  fui::ButtonProps b;
  b.label = "";
  b.action = action;
  b.value = static_cast<int16_t>(value);
  b.styles = toybox::rowStyles();
  b.radius = 10;
  screen.button(b, r);
}

void header(toybox::Screen& screen, const char* title, const char* right) {
  fui::HeaderProps h;
  h.title = title;
  h.rightLabel = right;
  h.subtitleText = fui::TextStyle{};
  h.subtitleText.font = toybox::kUiFont;
  h.subtitleText.color = fui::Color::White;
  h.subtitleText.align = fui::TextAlign::Right;
  h.borderEdges = fui::EdgesNone;
  toybox::absoluteChrome(screen);
  toybox::headerBand(screen, h);
}

// A chip that is on (black) or off (outlined).
void chip(toybox::Screen& screen, const fui::Rect& r, const char* label, const bool on, const fui::ActionId action,
          const int value = 0) {
  hotspot(screen, r, action, value);
  screen.target().fill(r, on ? kBlack : kWhite, 10);
  screen.target().stroke(r, kBlack, 2, 10);
  say(screen, r, label, Face::Button, fui::TextAlign::Center, on);
}

// Back, then two pills sharing the rest of the row; the last is the loud one.
void bottomBar(toybox::Screen& screen, const char* middle, const fui::ActionId middleAction, const char* last,
               const fui::ActionId lastAction, const bool lastEnabled) {
  const int16_t w = screen.device().width;
  const fui::Rect back = rect(toybox::kMargin, kButtonsY, 64, toybox::kPillHeight);
  hotspot(screen, back, ActionBack);
  screen.target().fill(back, kWhite, 10);
  screen.target().stroke(back, kBlack, 2, 10);
  // A chevron, drawn: two short bars meeting at a point.
  const int cx = back.x + back.width / 2;
  const int cy = back.y + back.height / 2;
  for (int i = 0; i < 12; ++i) {
    screen.target().fill(rect(cx - 6 + i, cy - i - 1, 4, 3), kBlack);
    screen.target().fill(rect(cx - 6 + i, cy + i - 1, 4, 3), kBlack);
  }
  const int x = back.right() + toybox::kGutter;
  const int room = w - toybox::kMargin - x;
  const int half = (room - toybox::kGutter) / 2;
  screen.button(pill(middle, middleAction, false), rect(x, kButtonsY, half, toybox::kPillHeight));
  const fui::Rect loud = rect(x + half + toybox::kGutter, kButtonsY, room - half - toybox::kGutter, toybox::kPillHeight);
  if (lastEnabled) {
    screen.button(pill(last, lastAction), loud);
  } else {
    screen.target().stroke(loud, kGrey, 2, 10);
    say(screen, loud, last, Face::Ui);
  }
}

// ---- playing cards ------------------------------------------------------------

const freeink::Icon& playingSuitArt(const int suit, const int size) {
  const bool big = size >= 96;
  const bool mid = size >= 40 && !big;
  switch (suit & 3) {
    case 0:
      return big ? icon_tt_spades_96 : mid ? icon_tt_spades_40 : icon_tt_spades_24;
    case 1:
      return big ? icon_tt_hearts_96 : mid ? icon_tt_hearts_40 : icon_tt_hearts_24;
    case 2:
      return big ? icon_tt_diamonds_96 : mid ? icon_tt_diamonds_40 : icon_tt_diamonds_24;
    default:
      return big ? icon_tt_clubs_96 : mid ? icon_tt_clubs_40 : icon_tt_clubs_24;
  }
}

void cardFrame(toybox::Screen& screen, const fui::Rect& r, const int edge = 3) {
  screen.target().fill(r, kWhite, 12);
  screen.target().stroke(r, kBlack, edge, 12);
}

// A card back: a diamond lattice inside a double frame, and the deck's mark in
// a white disc in the middle.
void cardBack(toybox::Screen& screen, const fui::Rect& r, const freeink::Icon& mark) {
  auto& target = screen.target();
  cardFrame(screen, r);
  const fui::Rect inner = rect(r.x + 9, r.y + 9, r.width - 18, r.height - 18);
  target.stroke(inner, kBlack, 2, 8);
  const fui::Rect field = rect(inner.x + 6, inner.y + 6, inner.width - 12, inner.height - 12);
  // Both diagonals, one row at a time: where each family of lines crosses
  // the row, a two-pixel dash.
  const int step = field.width >= 150 ? 18 : 12;
  for (int y = 0; y < field.height; ++y) {
    for (int x = (step - y % step) % step; x < field.width; x += step) {
      target.fill(rect(field.x + x, field.y + y, x + 2 <= field.width ? 2 : 1, 1), kBlack);
    }
    for (int x = y % step; x < field.width; x += step) {
      target.fill(rect(field.x + x, field.y + y, x + 2 <= field.width ? 2 : 1, 1), kBlack);
    }
  }
  target.stroke(field, kBlack, 1, 4);
  const int16_t radius = static_cast<int16_t>((mark.w > mark.h ? mark.w : mark.h) / 2 + 12);
  const int16_t cx = static_cast<int16_t>(r.x + r.width / 2);
  const int16_t cy = static_cast<int16_t>(r.y + r.height / 2);
  toybox::disc(screen, cx, cy, static_cast<int16_t>(radius + 3), fui::Color::Black);
  toybox::disc(screen, cx, cy, radius, fui::Color::White);
  icon(screen, rect(cx - mark.w / 2, cy - mark.h / 2, mark.w, mark.h), mark);
}

void bigPlaying(toybox::Screen& screen, const fui::Rect& r, const uint8_t id) {
  cardFrame(screen, r);
  if (tt::isJoker(id)) {
    say(screen, rect(r.x, r.y + 14, r.width, 40), "JOKER", Face::Ui);
    iconAt(screen, r.x + r.width / 2, r.y + (r.height - 96) / 2 + 10, icon_tt_joker_96);
    return;
  }
  const char* rank = tt::rankShort(tt::playingRank(id));
  const int suit = tt::playingSuit(id);
  // Corners: the rank over a small suit, top left and bottom right.
  say(screen, rect(r.x + 10, r.y + 10, 60, 44), rank, Face::Display);
  iconAt(screen, r.x + 40, r.y + 58, playingSuitArt(suit, 40));
  say(screen, rect(r.right() - 70, r.bottom() - 54, 60, 44), rank, Face::Display);
  iconAt(screen, r.right() - 40, r.bottom() - 98, playingSuitArt(suit, 40));
  iconAt(screen, r.x + r.width / 2, r.y + (r.height - 96) / 2, playingSuitArt(suit, 96));
}

void miniPlaying(toybox::Screen& screen, const fui::Rect& r, const uint8_t id) {
  cardFrame(screen, r, 2);
  if (tt::isJoker(id)) {
    say(screen, rect(r.x, r.y + 6, r.width, 26), "JK", Face::Button);
    iconAt(screen, r.x + r.width / 2, r.bottom() - 52, icon_tt_joker_40);
    return;
  }
  say(screen, rect(r.x + 4, r.y + 8, r.width - 8, 28), tt::rankShort(tt::playingRank(id)), Face::Ui);
  iconAt(screen, r.x + r.width / 2, r.bottom() - 52, playingSuitArt(tt::playingSuit(id), 40));
}

// ---- tarot ----------------------------------------------------------------------

const freeink::Icon* const kMajor128[tt::kMajors] = {
    &icon_tt_major_00_128, &icon_tt_major_01_128, &icon_tt_major_02_128, &icon_tt_major_03_128,
    &icon_tt_major_04_128, &icon_tt_major_05_128, &icon_tt_major_06_128, &icon_tt_major_07_128,
    &icon_tt_major_08_128, &icon_tt_major_09_128, &icon_tt_major_10_128, &icon_tt_major_11_128,
    &icon_tt_major_12_128, &icon_tt_major_13_128, &icon_tt_major_14_128, &icon_tt_major_15_128,
    &icon_tt_major_16_128, &icon_tt_major_17_128, &icon_tt_major_18_128, &icon_tt_major_19_128,
    &icon_tt_major_20_128, &icon_tt_major_21_128};
const freeink::Icon* const kMajor128r[tt::kMajors] = {
    &icon_tt_major_00_128_r, &icon_tt_major_01_128_r, &icon_tt_major_02_128_r, &icon_tt_major_03_128_r,
    &icon_tt_major_04_128_r, &icon_tt_major_05_128_r, &icon_tt_major_06_128_r, &icon_tt_major_07_128_r,
    &icon_tt_major_08_128_r, &icon_tt_major_09_128_r, &icon_tt_major_10_128_r, &icon_tt_major_11_128_r,
    &icon_tt_major_12_128_r, &icon_tt_major_13_128_r, &icon_tt_major_14_128_r, &icon_tt_major_15_128_r,
    &icon_tt_major_16_128_r, &icon_tt_major_17_128_r, &icon_tt_major_18_128_r, &icon_tt_major_19_128_r,
    &icon_tt_major_20_128_r, &icon_tt_major_21_128_r};
const freeink::Icon* const kMajor88[tt::kMajors] = {
    &icon_tt_major_00_88, &icon_tt_major_01_88, &icon_tt_major_02_88, &icon_tt_major_03_88, &icon_tt_major_04_88,
    &icon_tt_major_05_88, &icon_tt_major_06_88, &icon_tt_major_07_88, &icon_tt_major_08_88, &icon_tt_major_09_88,
    &icon_tt_major_10_88, &icon_tt_major_11_88, &icon_tt_major_12_88, &icon_tt_major_13_88, &icon_tt_major_14_88,
    &icon_tt_major_15_88, &icon_tt_major_16_88, &icon_tt_major_17_88, &icon_tt_major_18_88, &icon_tt_major_19_88,
    &icon_tt_major_20_88, &icon_tt_major_21_88};
const freeink::Icon* const kMajor88r[tt::kMajors] = {
    &icon_tt_major_00_88_r, &icon_tt_major_01_88_r, &icon_tt_major_02_88_r, &icon_tt_major_03_88_r,
    &icon_tt_major_04_88_r, &icon_tt_major_05_88_r, &icon_tt_major_06_88_r, &icon_tt_major_07_88_r,
    &icon_tt_major_08_88_r, &icon_tt_major_09_88_r, &icon_tt_major_10_88_r, &icon_tt_major_11_88_r,
    &icon_tt_major_12_88_r, &icon_tt_major_13_88_r, &icon_tt_major_14_88_r, &icon_tt_major_15_88_r,
    &icon_tt_major_16_88_r, &icon_tt_major_17_88_r, &icon_tt_major_18_88_r, &icon_tt_major_19_88_r,
    &icon_tt_major_20_88_r, &icon_tt_major_21_88_r};
const freeink::Icon* const kMajor48[tt::kMajors] = {
    &icon_tt_major_00_48, &icon_tt_major_01_48, &icon_tt_major_02_48, &icon_tt_major_03_48, &icon_tt_major_04_48,
    &icon_tt_major_05_48, &icon_tt_major_06_48, &icon_tt_major_07_48, &icon_tt_major_08_48, &icon_tt_major_09_48,
    &icon_tt_major_10_48, &icon_tt_major_11_48, &icon_tt_major_12_48, &icon_tt_major_13_48, &icon_tt_major_14_48,
    &icon_tt_major_15_48, &icon_tt_major_16_48, &icon_tt_major_17_48, &icon_tt_major_18_48, &icon_tt_major_19_48,
    &icon_tt_major_20_48, &icon_tt_major_21_48};

const freeink::Icon& tarotSuitArt(const int suit, const int size, const bool turned) {
#define SUIT(n) \
  (size >= 128 ? (turned ? icon_tt_##n##_128_r : icon_tt_##n##_128) \
              : size >= 64 ? (turned ? icon_tt_##n##_64_r : icon_tt_##n##_64) : icon_tt_##n##_40)
  switch (suit & 3) {
    case 0:
      return SUIT(wands);
    case 1:
      return SUIT(cups);
    case 2:
      return SUIT(swords);
    default:
      return SUIT(pentacles);
  }
#undef SUIT
}

// A tarot card at full size (one card) or spread size: the number or rank at
// the top, the picture, the name at the foot. Reversed turns the picture.
void tarotCard(toybox::Screen& screen, const fui::Rect& r, const tt::Drawn& d, const bool big) {
  cardFrame(screen, r);
  const fui::Rect inner = rect(r.x + 7, r.y + 7, r.width - 14, r.height - 14);
  screen.target().stroke(inner, kBlack, 1, 8);
  const Face top = big ? Face::Ui : Face::Button;
  const int topH = big ? 40 : 30;
  char name[32];
  tt::cardName(d.id, true, name, sizeof(name));
  const char* label = tt::isMajor(d.id) ? tt::roman(d.id) : tt::rankWord(tt::tarotRank(d.id));
  say(screen, rect(inner.x, inner.y + 4, inner.width, topH), label, top);
  const int nameH = big ? 56 : 44;
  const int nameTop = inner.bottom() - nameH - 2;
  const freeink::Icon& art = tt::isMajor(d.id)
                                 ? *(big ? (d.reversed ? kMajor128r : kMajor128) : (d.reversed ? kMajor88r : kMajor88))[d.id]
                                 : tarotSuitArt(tt::tarotSuit(d.id), big ? 128 : 64, d.reversed);
  const int artTop = inner.y + 4 + topH;
  const int room = nameTop - artTop;
  iconAt(screen, r.x + r.width / 2, artTop + (room - art.h) / 2, art);
  const Face nameFace = big ? fit(screen, name, inner.width - 8, {Face::Ui, Face::Button}) : Face::Tile;
  prose(screen, rect(inner.x + 4, nameTop, inner.width - 8, nameH), name, nameFace, 2);
}

void miniTarot(toybox::Screen& screen, const fui::Rect& r, const tt::Drawn& d) {
  cardFrame(screen, r, 2);
  const char* label = tt::isMajor(d.id) ? tt::roman(d.id) : tt::rankWord(tt::tarotRank(d.id));
  say(screen, rect(r.x + 2, r.y + 4, r.width - 4, 22), label, Face::Tile);
  const freeink::Icon& art = tt::isMajor(d.id) ? *kMajor48[d.id] : tarotSuitArt(tt::tarotSuit(d.id), 40, false);
  iconAt(screen, r.x + r.width / 2, r.y + 30, art);
  if (d.reversed) say(screen, rect(r.x + 2, r.bottom() - 26, r.width - 4, 22), "REV", Face::Tile);
}

// The strip of earlier draws, newest on the left.
void earlier(toybox::Screen& screen, const tt::Deck& deck, const bool tarot, const int top) {
  if (deck.historyLen == 0) return;
  const int16_t w = screen.device().width;
  say(screen, rect(toybox::kMargin, top, w - 2 * toybox::kMargin, 22), "EARLIER", Face::Tile, fui::TextAlign::Left);
  screen.target().fill(rect(toybox::kMargin + 76, top + 10, w - 2 * toybox::kMargin - 76, 2), kBlack);
  constexpr int kW = 76;
  const int h = tarot ? 112 : 106;
  for (int i = 0; i < deck.historyLen; ++i) {
    const tt::Drawn& d = deck.history[deck.historyLen - 1 - i];
    const fui::Rect r = rect(toybox::kMargin + 10 + i * (kW + 12), top + 30, kW, h);
    if (tarot) {
      miniTarot(screen, r, d);
    } else {
      miniPlaying(screen, r, d.id);
    }
  }
}

// ---- dice -------------------------------------------------------------------------

const freeink::Icon& dieArt(const int kind, const bool big) {
  switch (kind) {
    case 0:
      return big ? icon_tt_d4_96 : icon_tt_d4_48;
    case 1:
      return big ? icon_tt_d6_96 : icon_tt_d6_48;
    case 2:
      return big ? icon_tt_d8_96 : icon_tt_d8_48;
    case 3:
      return big ? icon_tt_d10_96 : icon_tt_d10_48;
    case 4:
      return big ? icon_tt_d12_96 : icon_tt_d12_48;
    case 5:
      return big ? icon_tt_d20_96 : icon_tt_d20_48;
    default:
      return big ? icon_tt_d100_96 : icon_tt_d100_48;
  }
}

const char* const kDieNames[tt::kDieKinds] = {"d4", "d6", "d8", "d10", "d12", "d20", "d100"};

// A die with a number in it. The triangle's middle sits low.
void die(toybox::Screen& screen, const int x, const int y, const int kind, const char* text) {
  const freeink::Icon& art = dieArt(kind, true);
  icon(screen, rect(x, y, art.w, art.h), art);
  const int drop = kind == 0 ? 16 : 0;
  const Face f = fit(screen, text, 64, {Face::Display, Face::Ui});
  say(screen, rect(x, y + drop, art.w, art.h), text, f);
}

// "2d6 + d20"
void formula(const tt::Dice& d, char* out, const int size) {
  int counts[tt::kDieKinds] = {};
  for (int i = 0; i < d.count; ++i) ++counts[d.pick[i]];
  int used = 0;
  out[0] = '\0';
  for (int k = 0; k < tt::kDieKinds && used < size; ++k) {
    if (!counts[k]) continue;
    const int n = std::snprintf(out + used, size - used, "%s%s%d%s", used ? " + " : "", "", counts[k], kDieNames[k]);
    if (n < 0) return;
    used += n;
  }
}

}  // namespace

void useFonts(FontBinder* binder) { gFonts = binder; }

// ---- menu ---------------------------------------------------------------------------

void buildMenu(toybox::Screen& screen, const tt::Table& t) {
  header(screen, "TABLETOP", nullptr);
  const int16_t w = screen.device().width;
  constexpr int kTileH = 196;
  const int x = toybox::kMargin;
  const int tw = w - 2 * toybox::kMargin;
  char line[40];
  for (int i = 0; i < 3; ++i) {
    const fui::Rect r = rect(x, 100 + i * (kTileH + 12), tw, kTileH);
    hotspot(screen, r, ActionOpen, i + 1);
    screen.target().fill(r, kWhite, 14);
    screen.target().stroke(r, kBlack, 2, 14);
    const int artW = 170;
    const fui::Rect text = rect(r.x + artW, r.y, r.width - artW - 16, r.height);
    const char* title = i == 0 ? "CARDS" : i == 1 ? "TAROT" : "DICE";
    say(screen, rect(text.x, r.y + 48, text.width, 48), title, Face::Display, fui::TextAlign::Left);
    if (i == 0) {
      // Two cards, one behind the other.
      cardBack(screen, rect(r.x + 26, r.y + 28, 82, 116), icon_tt_spades_24);
      cardFrame(screen, rect(r.x + 62, r.y + 52, 82, 116));
      say(screen, rect(r.x + 66, r.y + 58, 40, 34), "A", Face::Display);
      iconAt(screen, r.x + 103, r.y + 100, icon_tt_spades_40);
      std::snprintf(line, sizeof(line), "%d of %d left", t.cards.left, t.cards.size);
      say(screen, rect(text.x, r.y + 100, text.width, 30), line, Face::Ui, fui::TextAlign::Left);
      say(screen, rect(text.x, r.y + 132, text.width, 26), t.jokers ? "With jokers" : "No jokers", Face::Button,
          fui::TextAlign::Left);
    } else if (i == 1) {
      const fui::Rect c = rect(r.x + 36, r.y + 18, 108, 160);
      cardFrame(screen, c);
      say(screen, rect(c.x, c.y + 6, c.width, 26), "XVII", Face::Button);
      iconAt(screen, c.x + c.width / 2, c.y + 40, icon_tt_major_17_88);
      say(screen, rect(c.x, c.bottom() - 30, c.width, 24), "THE STAR", Face::Tile);
      std::snprintf(line, sizeof(line), "%d of %d left", t.tarot.left, t.tarot.size);
      say(screen, rect(text.x, r.y + 100, text.width, 30), line, Face::Ui, fui::TextAlign::Left);
      say(screen, rect(text.x, r.y + 132, text.width, 26), t.spread ? "Three-card spread" : "One card at a time",
          Face::Button, fui::TextAlign::Left);
    } else {
      die(screen, r.x + 24, r.y + 24, 5, "20");
      icon(screen, rect(r.x + 96, r.y + 120, 48, 48), icon_tt_d6_48);
      say(screen, rect(r.x + 96, r.y + 120, 48, 48), "6", Face::Ui);
      const tt::Dice& d = t.dice;
      char f[40];
      formula(d, f, sizeof(f));
      if (d.rolled) {
        std::snprintf(line, sizeof(line), "Last roll: %d", tt::total(d));
      } else {
        std::snprintf(line, sizeof(line), "%s", d.count ? "Ready to roll" : "No dice picked");
      }
      say(screen, rect(text.x, r.y + 100, text.width, 30), line, Face::Ui, fui::TextAlign::Left);
      say(screen, rect(text.x, r.y + 132, text.width, 26), f, Face::Button, fui::TextAlign::Left);
    }
  }
}

// ---- cards ---------------------------------------------------------------------------

void buildCards(toybox::Screen& screen, const tt::Table& t) {
  const tt::Deck& deck = t.cards;
  char right[16];
  std::snprintf(right, sizeof(right), "%d LEFT", deck.left);
  header(screen, "CARDS", right);
  const int16_t w = screen.device().width;
  char line[40];
  std::snprintf(line, sizeof(line), "DECK OF %d", deck.size);
  say(screen, rect(toybox::kMargin, 96, 200, 44), line, Face::Button, fui::TextAlign::Left);
  chip(screen, rect(w - toybox::kMargin - 190, 96, 190, 44), t.jokers ? "JOKERS ON" : "JOKERS OFF", t.jokers,
       ActionJokers);

  const fui::Rect card = rect((w - 236) / 2, 156, 236, 330);
  if (deck.shown) {
    bigPlaying(screen, card, deck.current[0].id);
    tt::cardName(deck.current[0].id, false, line, sizeof(line));
    say(screen, rect(0, card.bottom() + 8, w, 30), line, Face::Button);
  } else {
    cardBack(screen, card, icon_tt_spades_40);
    say(screen, rect(0, card.bottom() + 8, w, 30), deck.left ? "Shuffled. Tap DRAW." : "The deck is empty.",
        Face::Button);
  }
  earlier(screen, deck, false, 540);
  bottomBar(screen, "SHUFFLE", ActionShuffle, deck.left ? "DRAW" : "EMPTY", ActionDraw, deck.left > 0);
}

// ---- tarot ---------------------------------------------------------------------------

void buildTarot(toybox::Screen& screen, const tt::Table& t) {
  const tt::Deck& deck = t.tarot;
  char right[16];
  std::snprintf(right, sizeof(right), "%d LEFT", deck.left);
  header(screen, "TAROT", right);
  const int16_t w = screen.device().width;
  const int m = toybox::kMargin;
  chip(screen, rect(m, 96, 104, 44), "ONE", !t.spread, ActionSpread, 0);
  chip(screen, rect(m + 112, 96, 104, 44), "THREE", t.spread, ActionSpread, 1);
  chip(screen, rect(w - m - 200, 96, 200, 44), t.reversals ? "REVERSED ON" : "REVERSED OFF", t.reversals,
       ActionReversals);

  if (deck.shown == 0) {
    const fui::Rect card = rect((w - 236) / 2, 156, 236, 360);
    cardBack(screen, card, icon_tt_major_10_88);
    say(screen, rect(0, card.bottom() + 6, w, 30), deck.left ? "Shuffled. Tap DRAW." : "The deck is empty.",
        Face::Button);
  } else if (deck.shown == 1) {
    const fui::Rect card = rect((w - 236) / 2, 156, 236, 360);
    tarotCard(screen, card, deck.current[0], true);
    if (deck.current[0].reversed) say(screen, rect(0, card.bottom() + 6, w, 30), "REVERSED", Face::Button);
  } else {
    const char* const places[3] = {"PAST", "PRESENT", "FUTURE"};
    constexpr int kW = 140;
    const int gap = (w - 3 * kW - 2 * m) / 2;
    for (int i = 0; i < deck.shown; ++i) {
      const int x = m + i * (kW + gap);
      say(screen, rect(x, 150, kW, 28), places[i], Face::Button);
      const fui::Rect card = rect(x, 182, kW, 262);
      tarotCard(screen, card, deck.current[i], false);
      if (deck.current[i].reversed) say(screen, rect(x, card.bottom() + 6, kW, 26), "REVERSED", Face::Tile);
    }
  }
  earlier(screen, deck, true, 556);
  const int want = t.spread ? 3 : 1;
  const char* label = deck.left >= want ? (t.spread ? "DRAW 3" : "DRAW") : deck.left ? "DRAW" : "EMPTY";
  bottomBar(screen, "SHUFFLE", ActionShuffle, label, ActionDraw, deck.left > 0);
}

// ---- dice ----------------------------------------------------------------------------

void buildDice(toybox::Screen& screen, const tt::Table& t) {
  const tt::Dice& d = t.dice;
  char right[16];
  std::snprintf(right, sizeof(right), "%d DICE", d.count);
  header(screen, "DICE", d.count ? right : nullptr);
  const int16_t w = screen.device().width;
  const int m = toybox::kMargin;

  // The kinds: tap to add one.
  int counts[tt::kDieKinds] = {};
  for (int i = 0; i < d.count; ++i) ++counts[d.pick[i]];
  constexpr int kCellW = 58;
  const int gap = (w - 2 * m - tt::kDieKinds * kCellW) / (tt::kDieKinds - 1);
  for (int k = 0; k < tt::kDieKinds; ++k) {
    const fui::Rect cell = rect(m + k * (kCellW + gap), 96, kCellW, 86);
    hotspot(screen, cell, ActionAddDie, k);
    screen.target().fill(cell, kWhite, 10);
    screen.target().stroke(cell, kBlack, 2, 10);
    icon(screen, rect(cell.x + (kCellW - 48) / 2, cell.y + 6, 48, 48), dieArt(k, false));
    say(screen, rect(cell.x, cell.y + 56, kCellW, 26), kDieNames[k],
        fit(screen, kDieNames[k], kCellW - 8, {Face::Button, Face::Tile}));
    if (counts[k]) {
      char n[4];
      std::snprintf(n, sizeof(n), "%d", counts[k]);
      const fui::Rect dot = rect(cell.right() - 22, cell.y - 8, 28, 28);
      screen.target().fill(dot, kBlack, 14);
      say(screen, dot, n, Face::Tile, fui::TextAlign::Center, true);
    }
  }

  // The total and what made it.
  char line[48];
  char f[40];
  formula(d, f, sizeof(f));
  if (d.count == 0) {
    say(screen, rect(0, 204, w, 60), "Pick some dice", Face::Display);
    say(screen, rect(0, 266, w, 26), "Tap a die above to add it", Face::Button);
  } else if (d.rolled) {
    std::snprintf(line, sizeof(line), "%d", tt::total(d));
    say(screen, rect(0, 196, w, 70), line, Face::Large);
    say(screen, rect(0, 266, w, 26), f, Face::Button);
  } else {
    say(screen, rect(0, 204, w, 60), f, Face::Display);
    say(screen, rect(0, 266, w, 26), "Tap ROLL", Face::Button);
  }

  // The tray: tap a die to take it away.
  constexpr int kDie = 96;
  constexpr int kStep = 16;
  for (int i = 0; i < d.count; ++i) {
    const int row = i / 4;
    const int col = i % 4;
    const int inRow = (d.count - row * 4) < 4 ? d.count - row * 4 : 4;
    const int rowW = inRow * kDie + (inRow - 1) * kStep;
    const int x = (w - rowW) / 2 + col * (kDie + kStep);
    const int y = 306 + row * (kDie + kStep);
    hotspot(screen, rect(x, y, kDie, kDie), ActionRemoveDie, i);
    screen.target().fill(rect(x - 2, y - 2, kDie + 4, kDie + 4), kWhite);
    char v[8];
    if (d.rolled) {
      std::snprintf(v, sizeof(v), "%d", d.value[i]);
    } else {
      std::snprintf(v, sizeof(v), "?");
    }
    die(screen, x, y, d.pick[i], v);
  }

  if (d.totalsLen) {
    int used = std::snprintf(line, sizeof(line), "EARLIER ");
    for (int i = d.totalsLen - 1; i >= 0 && used > 0 && used < static_cast<int>(sizeof(line)); --i) {
      const int n = std::snprintf(line + used, sizeof(line) - used, " %d", d.totals[i]);
      if (n < 0) break;
      used += n;
    }
    say(screen, rect(m, 650, w - 2 * m, 30), line, Face::Button);
  }
  bottomBar(screen, "CLEAR", ActionClear, "ROLL", ActionRoll, d.count > 0);
}

}  // namespace tabletopui
