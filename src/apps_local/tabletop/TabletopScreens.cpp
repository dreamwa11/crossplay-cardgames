#include "TabletopScreens.h"

#include <FreeInkUIIcon.h>

#include <cstdio>
#include <cstring>
#include <initializer_list>

#include <InflateStream.h>
#include <Logging.h>
#include <Memory.h>

#include "TabletopArt.h"
#include "TabletopRws.h"

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

// Grey words on paper: black type with every other pixel knocked back to
// white. The renderer has no dithered text, and a checkerboard over the ink
// is what a grey glyph would be anyway. `box` must hold nothing but the words.
void sayGrey(toybox::Screen& screen, const fui::Rect& box, const char* text, const Face f) {
  say(screen, box, text, f);
  for (int y = box.y; y < box.bottom(); ++y) {
    for (int x = box.x + (y & 1); x < box.right(); x += 2) screen.target().fill(rect(x, y, 1, 1), kWhite);
  }
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

// A card back: one of The Pale Motel's three patterns, picked when the deck
// was shuffled, inside the card's frame.
struct BackArt {
  uint8_t back;
  const freeink::Icon* art;
};
const BackArt kBackArt[] = {
    {0, &icon_tt_back0_224x318}, {0, &icon_tt_back0_70x104}, {1, &icon_tt_back1_224x318},
    {1, &icon_tt_back1_70x104},  {2, &icon_tt_back2_224x348},
};

void cardBack(toybox::Screen& screen, const fui::Rect& r, const int back) {
  cardFrame(screen, r);
  const fui::Rect field = rect(r.x + 6, r.y + 6, r.width - 12, r.height - 12);
  for (const BackArt& b : kBackArt) {
    if (b.back == back && b.art->w == field.width && b.art->h == field.height) {
      icon(screen, field, *b.art);
      return;
    }
  }
}

void bigPlaying(toybox::Screen& screen, const fui::Rect& r, const uint8_t id) {
  cardFrame(screen, r);
  if (tt::isJoker(id)) {
    say(screen, rect(r.x, r.y + 14, r.width, 40), "JOKER", Face::Ui);
    iconAt(screen, r.x + r.width / 2, r.y + 54 + (r.height - 54 - 220) / 2, icon_tt_joker_220);
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

// A major arcanum: Pamela Colman Smith's drawing, inflated from flash into a
// buffer for the one draw, and turned there when the card came up reversed.
// The drawing carries its own number and title, as the printed card does.
void drawRws(toybox::Screen& screen, const int x, const int y, const tabletopart::Packed& p, const bool turned) {
  const int stride = (p.w + 7) / 8;
  const size_t bytes = static_cast<size_t>(stride) * p.h;
  auto ink = makeUniqueNoThrow<uint8_t[]>(bytes);
  auto bits = makeUniqueNoThrow<uint8_t[]>(bytes);
  if (!ink || !bits) {
    LOG_ERR("TABLETOP", "OOM: %d bytes for a card", static_cast<int>(bytes * 2));
    return;
  }
  InflateStream inflate;
  if (!inflate.init(false)) {
    LOG_ERR("TABLETOP", "OOM: inflate state");
    return;
  }
  inflate.setSource(p.data, p.size);
  inflate.setZlibWrapped();
  if (!inflate.read(ink.get(), bytes)) {
    LOG_ERR("TABLETOP", "Card art did not inflate");
    return;
  }
  // freeink::Icon is 1 for paper, MSB first; the packed art is 1 for ink.
  if (!turned) {
    for (size_t i = 0; i < bytes; ++i) bits[i] = static_cast<uint8_t>(~ink[i]);
  } else {
    std::memset(bits.get(), 0xFF, bytes);
    for (int yy = 0; yy < p.h; ++yy) {
      const uint8_t* row = ink.get() + yy * stride;
      uint8_t* out = bits.get() + (p.h - 1 - yy) * stride;
      for (int xx = 0; xx < p.w; ++xx) {
        if (row[xx >> 3] & (0x80 >> (xx & 7))) {
          const int tx = p.w - 1 - xx;
          out[tx >> 3] = static_cast<uint8_t>(out[tx >> 3] & ~(0x80 >> (tx & 7)));
        }
      }
    }
  }
  const freeink::Icon art = {static_cast<uint16_t>(p.w), static_cast<uint16_t>(p.h), static_cast<uint16_t>(p.h / 2),
                             bits.get()};
  icon(screen, rect(x, y, p.w, p.h), art);
}

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
  if (tt::isMajor(d.id)) {
    const tabletopart::Packed& p = (big ? tabletopart::kRws_big : tabletopart::kRws_spread)[d.id];
    drawRws(screen, r.x + (r.width - p.w) / 2, r.y + (r.height - p.h) / 2, p, d.reversed);
    return;
  }
  const fui::Rect inner = rect(r.x + 7, r.y + 7, r.width - 14, r.height - 14);
  screen.target().stroke(inner, kBlack, 1, 8);
  const Face top = big ? Face::Ui : Face::Button;
  const int topH = big ? 40 : 30;
  char name[32];
  tt::cardName(d.id, true, name, sizeof(name));
  say(screen, rect(inner.x, inner.y + 4, inner.width, topH), tt::rankWord(tt::tarotRank(d.id)), top);
  const int nameH = big ? 56 : 44;
  const int nameTop = inner.bottom() - nameH - 2;
  const freeink::Icon& art = tarotSuitArt(tt::tarotSuit(d.id), big ? 128 : 64, d.reversed);
  const int artTop = inner.y + 4 + topH;
  const int room = nameTop - artTop;
  iconAt(screen, r.x + r.width / 2, artTop + (room - art.h) / 2, art);
  const Face nameFace = big ? fit(screen, name, inner.width - 8, {Face::Ui, Face::Button}) : Face::Tile;
  prose(screen, rect(inner.x + 4, nameTop, inner.width - 8, nameH), name, nameFace, 2);
}

void miniTarot(toybox::Screen& screen, const fui::Rect& r, const tt::Drawn& d) {
  cardFrame(screen, r, 2);
  if (tt::isMajor(d.id)) {
    say(screen, rect(r.x + 2, r.y + 8, r.width - 4, 28), tt::roman(d.id),
        fit(screen, tt::roman(d.id), r.width - 8, {Face::Ui, Face::Button}));
    char name[32];
    tt::cardName(d.id, true, name, sizeof(name));
    const char* shortName = std::strncmp(name, "THE ", 4) == 0 ? name + 4 : name;
    prose(screen, rect(r.x + 3, r.y + 40, r.width - 6, 44), shortName, Face::Tile, 3);
  } else {
    say(screen, rect(r.x + 2, r.y + 4, r.width - 4, 22), tt::rankWord(tt::tarotRank(d.id)), Face::Tile);
    iconAt(screen, r.x + r.width / 2, r.y + 30, tarotSuitArt(tt::tarotSuit(d.id), 40, false));
  }
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
  // The triangle's middle sits low and narrow: a smaller number, lower down.
  const int drop = kind == 0 ? 11 : 0;
  // One size for every number on every die.
  const Face f = Face::Ui;
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
  constexpr int kGap = 12;
  constexpr int kTileH = 300;
  const int tw = (w - 2 * toybox::kMargin - kGap) / 2;
  char line[40];
  char sub[40];
  for (int i = 0; i < 4; ++i) {
    const fui::Rect r = rect(toybox::kMargin + (i % 2) * (tw + kGap), 100 + (i / 2) * (kTileH + kGap), tw, kTileH);
    hotspot(screen, r, ActionOpen, i + 1);
    screen.target().fill(r, kWhite, 14);
    screen.target().stroke(r, kBlack, 2, 14);
    const int cx = r.x + r.width / 2;
    const char* title = i == 0 ? "CARDS" : i == 1 ? "TAROT" : i == 2 ? "DICE" : "ORACLE";
    line[0] = sub[0] = '\0';
    if (i == 0) {
      // Two cards, one behind the other.
      cardBack(screen, rect(cx - 60, r.y + 26, 82, 116), t.cards.back);
      cardFrame(screen, rect(cx - 22, r.y + 50, 82, 116));
      say(screen, rect(cx - 16, r.y + 58, 30, 26), "A", Face::Ui);
      iconAt(screen, cx + 19, r.y + 98, icon_tt_spades_40);
      std::snprintf(line, sizeof(line), "%d of %d left", t.cards.left, t.cards.size);
      std::snprintf(sub, sizeof(sub), "%s", t.jokers ? "With jokers" : "No jokers");
    } else if (i == 1) {
      const tabletopart::Packed& fool = tabletopart::kRws_menu[0];
      const fui::Rect c = rect(cx - 54, r.y + 14, 108, 168);
      cardFrame(screen, c);
      drawRws(screen, c.x + (c.width - fool.w) / 2, c.y + (c.height - fool.h) / 2, fool, false);
      std::snprintf(line, sizeof(line), "%d of %d left", t.tarot.left, t.tarot.size);
      std::snprintf(sub, sizeof(sub), "%s", t.spread ? "Three-card spread" : "One at a time");
    } else if (i == 2) {
      die(screen, cx - 74, r.y + 30, 5, "20");
      icon(screen, rect(cx + 22, r.y + 112, 48, 48), icon_tt_d6_48);
      say(screen, rect(cx + 22, r.y + 112, 48, 48), "6", Face::Ui);
      const tt::Dice& d = t.dice;
      formula(d, sub, sizeof(sub));
      if (d.rolled) {
        std::snprintf(line, sizeof(line), "Last roll: %d", tt::total(d));
      } else {
        std::snprintf(line, sizeof(line), "%s", d.count ? "Ready to roll" : "No dice picked");
      }
    } else {
      iconAt(screen, cx, r.y + 38, icon_tt_crystal_ball_120);
      const tt::Oracle& o = t.oracle;
      if (o.len) {
        tt::title(o.log[o.len - 1], line, sizeof(line));
      } else {
        std::snprintf(line, sizeof(line), "Ask a question");
      }
      std::snprintf(sub, sizeof(sub), "%s",
                    o.genre == tt::Genre::SciFi     ? "Sci-fi"
                    : o.genre == tt::Genre::Mystery ? "Mystery"
                                                    : "Fantasy");
    }
    say(screen, rect(r.x + 8, r.y + 194, r.width - 16, 44), title, Face::Display);
    say(screen, rect(r.x + 8, r.y + 238, r.width - 16, 28), line, fit(screen, line, r.width - 16, {Face::Ui, Face::Button, Face::Tile}));
    say(screen, rect(r.x + 8, r.y + 266, r.width - 16, 24), sub, fit(screen, sub, r.width - 16, {Face::Button, Face::Tile}));
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
    cardBack(screen, card, deck.back);
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
  chip(screen, rect(m, 96, 84, 44), "ONE", !t.spread, ActionSpread, 0);
  chip(screen, rect(m + 92, 96, 96, 44), "THREE", t.spread, ActionSpread, 1);
  // Which arcana the deck holds, a tap stepping through all, major and minor.
  const char* arcana = t.arcana == tt::Arcana::Major ? "MAJOR" : t.arcana == tt::Arcana::Minor ? "MINOR" : "ALL";
  chip(screen, rect(m + 196, 96, 104, 44), arcana, t.arcana != tt::Arcana::All, ActionArcana);
  chip(screen, rect(w - m - 140, 96, 140, 44), "REVERSED", t.reversals, ActionReversals);

  if (deck.shown == 0) {
    const fui::Rect card = rect((w - 236) / 2, 156, 236, 360);
    cardBack(screen, card, deck.back);
    say(screen, rect(0, card.bottom() + 6, w, 30), deck.left ? "Shuffled. Tap DRAW." : "The deck is empty.",
        Face::Button);
  } else if (deck.shown == 1) {
    const fui::Rect card = rect((w - 236) / 2, 156, 236, 360);
    tarotCard(screen, card, deck.current[0], true);
    if (deck.current[0].reversed) say(screen, rect(0, card.bottom() + 6, w, 30), "REVERSED", Face::Button);
  } else {
    const char* const places[3] = {"PAST", "PRESENT", "FUTURE"};
    constexpr int kW = 140;
    constexpr int kH = 226;
    const int gap = (w - 3 * kW - 2 * m) / 2;
    // The cards and their labels (place, then REVERSED) centred between the
    // chips and the earlier cards' strip, as the single card is.
    constexpr int kBlockH = kH + 8 + 26 + 4 + 22;
    const int top = 148 + (552 - 148 - kBlockH) / 2;
    for (int i = 0; i < deck.shown; ++i) {
      const int x = m + i * (kW + gap);
      const fui::Rect card = rect(x, top, kW, kH);
      tarotCard(screen, card, deck.current[i], false);
      // REVERSED right under its card, the place below that.
      if (deck.current[i].reversed) say(screen, rect(x, card.bottom() + 8, kW, 22), "REVERSED", Face::Tile);
      sayGrey(screen, rect(x, card.bottom() + 34, kW, 26), places[i], Face::Button);
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

// ---- oracle --------------------------------------------------------------------------

namespace {

constexpr tt::Kind kTables[] = {tt::Kind::Spark,  tt::Kind::Detail, tt::Kind::Twist,  tt::Kind::Person, tt::Kind::Place,
                                tt::Kind::Clue,   tt::Kind::Danger, tt::Kind::Chance, tt::Kind::Fight};
constexpr int kTableCount = sizeof(kTables) / sizeof(kTables[0]);

// A button with its label shrunk to fit.
void key(toybox::Screen& screen, const fui::Rect& r, const char* label, const bool on, const fui::ActionId action,
         const int value) {
  hotspot(screen, r, action, value);
  screen.target().fill(r, on ? kBlack : kWhite, 10);
  screen.target().stroke(r, kBlack, 2, 10);
  say(screen, r, label, fit(screen, label, r.width - 10, {Face::Button, Face::Tile}), fui::TextAlign::Center, on);
}

// A section label with a rule running to the right edge.
void label(toybox::Screen& screen, const int y, const char* text) {
  const int16_t w = screen.device().width;
  const int tw = widthOf(screen, text, Face::Tile);
  say(screen, rect(toybox::kMargin, y, tw + 4, 22), text, Face::Tile, fui::TextAlign::Left);
  screen.target().fill(rect(toybox::kMargin + tw + 12, y + 10, w - 2 * toybox::kMargin - tw - 12, 2), kBlack);
}

// The newest reading, large: its title, the sentence, the dice under it.
void readingCard(toybox::Screen& screen, const fui::Rect& r, const tt::Oracle& o) {
  screen.target().fill(r, kWhite, 14);
  screen.target().stroke(r, kBlack, 2, 14);
  const fui::Rect in = rect(r.x + 16, r.y, r.width - 32, r.height);
  if (o.len == 0) {
    prose(screen, rect(in.x, r.y + r.height / 2 - 40, in.width, 80), "Ask a question, or roll on a table.", Face::Ui,
          2);
    return;
  }
  const tt::Reading& last = o.log[o.len - 1];
  char t[64];
  char b[256];
  char m[48];
  tt::title(last, t, sizeof(t));
  tt::body(last, b, sizeof(b));
  tt::meta(last, m, sizeof(m));
  if (tt::named(last)) {
    const char* kind = tt::kindName(static_cast<tt::Kind>(last.kind));
    sayGrey(screen, rect(in.x, r.y + 12, widthOf(screen, kind, Face::Tile) + 2, 22), kind, Face::Tile);
  }
  const Face tf = fit(screen, t, in.width, {Face::Large, Face::Display, Face::Ui});
  say(screen, rect(in.x, r.y + 38, in.width, 56), t, tf, fui::TextAlign::Left);
  constexpr int kLine = 27;
  const int lines = (r.height - 104 - (m[0] ? 34 : 8)) / kLine;
  fui::TextStyle st = face(Face::Button, fui::TextAlign::Left);
  st.maxLines = static_cast<uint8_t>(lines > 0 ? lines : 1);
  screen.target().text(rect(in.x, r.y + 100, in.width, lines * kLine), b, st);
  if (m[0]) sayGrey(screen, rect(in.x, r.bottom() - 34, widthOf(screen, m, Face::Tile) + 2, 22), m, Face::Tile);
}

// One reading on one line: its brief.
void logLine(toybox::Screen& screen, const fui::Rect& r, const tt::Reading& rd, const Face f) {
  char line[128];
  tt::brief(rd, line, sizeof(line));
  fui::TextStyle st = face(f, fui::TextAlign::Left);
  st.maxLines = 1;
  screen.target().text(r, line, st);
}

void oddsRow(toybox::Screen& screen, const tt::Oracle& o, const int y, const int h) {
  const int16_t w = screen.device().width;
  constexpr int kGap = 8;
  const int cw = (w - 2 * toybox::kMargin - (tt::kOddsKinds - 1) * kGap) / tt::kOddsKinds;
  for (int i = 0; i < tt::kOddsKinds; ++i) {
    const tt::Odds odds = static_cast<tt::Odds>(i);
    const fui::Rect r = rect(toybox::kMargin + i * (cw + kGap), y, cw, h);
    const bool on = o.odds == odds && o.len > 0;
    hotspot(screen, r, ActionAsk, i);
    screen.target().fill(r, on ? kBlack : kWhite, 10);
    screen.target().stroke(r, kBlack, 2, 10);
    char pct[8];
    std::snprintf(pct, sizeof(pct), "%d%%", tt::chanceOf(odds));
    say(screen, rect(r.x, r.y + 4, r.width, r.height / 2), tt::oddsShort(odds), Face::Tile, fui::TextAlign::Center, on);
    say(screen, rect(r.x, r.y + r.height / 2 - 2, r.width, r.height / 2), pct, Face::Button, fui::TextAlign::Center, on);
  }
}

void tableGrid(toybox::Screen& screen, const int y, const int cols, const int h) {
  const int16_t w = screen.device().width;
  constexpr int kGap = 8;
  const int cw = (w - 2 * toybox::kMargin - (cols - 1) * kGap) / cols;
  for (int i = 0; i < kTableCount; ++i) {
    const fui::Rect r = rect(toybox::kMargin + (i % cols) * (cw + kGap), y + (i / cols) * (h + kGap), cw, h);
    key(screen, r, tt::kindName(kTables[i]), false, ActionConsult, static_cast<int>(kTables[i]));
  }
}

}  // namespace

void buildOracle(toybox::Screen& screen, const tt::Table& t) {
  const tt::Oracle& o = t.oracle;
  header(screen, "ORACLE", nullptr);
  const int16_t w = screen.device().width;
  const int x = toybox::kMargin;
  const int cw = w - 2 * x;
  // The genre: paper on the black band, so it reads as a switch.
  const char* genre = tt::genreName(o.genre);
  const int gw = widthOf(screen, genre, Face::Button) + 36;
  const fui::Rect g = rect(w - x - gw, 16, gw, 44);
  hotspot(screen, g, ActionGenre);
  screen.target().fill(g, kWhite, 10);
  say(screen, g, genre, Face::Button);
  // The answer, then a row to ask and a grid to roll, then the last two.
  readingCard(screen, rect(x, 96, cw, 244), o);
  label(screen, 352, "ASK");
  oddsRow(screen, o, 378, 52);
  label(screen, 444, "ROLL");
  tableGrid(screen, 470, 3, 46);
  // The last two, under the grid; tapping them opens the log.
  if (o.len > 1) {
    const fui::Rect last = rect(x, 638, cw, 72);
    hotspot(screen, last, ActionLog);
    screen.target().fill(last, kWhite);
  }
  for (int i = 0; i < 2 && i + 1 < o.len; ++i) {
    logLine(screen, rect(x, 643 + i * 30, cw, 30), o.log[o.len - 2 - i], Face::Button);
  }
  bottomBar(screen, "LOG", ActionLog, "AGAIN", ActionAgain, o.len > 0);
}

constexpr int kCreditY = kButtonsY - 32;

namespace {

// Left-aligned lines broken at spaces. Draws them when `draw` and returns how
// many there are, so a caller can measure first.
int wrapped(toybox::Screen& screen, const int x, const int y, const int width, const int lineH, const char* text,
            const Face f, const bool draw) {
  char line[128];
  int lines = 0;
  const char* p = text;
  while (*p) {
    // The longest run of whole words that fits; one word alone if none does.
    int len = 0;
    int fitLen = 0;
    while (true) {
      while (p[len] && p[len] != ' ') ++len;
      if (len >= static_cast<int>(sizeof(line))) break;
      std::memcpy(line, p, len);
      line[len] = '\0';
      if (fitLen && widthOf(screen, line, f) > width) break;
      fitLen = len;
      if (!p[len]) break;
      ++len;
    }
    if (fitLen == 0) fitLen = len < static_cast<int>(sizeof(line)) ? len : static_cast<int>(sizeof(line)) - 1;
    std::memcpy(line, p, fitLen);
    line[fitLen] = '\0';
    if (draw) say(screen, rect(x, y + lines * lineH, width, lineH), line, f, fui::TextAlign::Left);
    ++lines;
    p += fitLen;
    while (*p == ' ') ++p;
  }
  return lines;
}

constexpr int kLogTop = 96;
constexpr int kHeadH = 28;
constexpr int kBodyLine = 21;
constexpr int kEntryGap = 14;

// One log entry: what it is, then everything it said. Returns its height.
int logEntry(toybox::Screen& screen, const tt::Reading& rd, const int x, const int y, const int width,
             const bool draw) {
  char t[64];
  char b[256];
  char m[48];
  tt::title(rd, t, sizeof(t));
  tt::body(rd, b, sizeof(b));
  tt::meta(rd, m, sizeof(m));
  if (draw) {
    char line[96];
    if (tt::named(rd) || static_cast<tt::Kind>(rd.kind) == tt::Kind::Ask) {
      std::snprintf(line, sizeof(line), "%s  %s", tt::kindName(static_cast<tt::Kind>(rd.kind)), t);
    } else {
      std::snprintf(line, sizeof(line), "%s", t);
    }
    say(screen, rect(x, y, width, kHeadH), line, fit(screen, line, width, {Face::Button, Face::Tile}),
        fui::TextAlign::Left);
  }
  int lines = b[0] ? wrapped(screen, x, y + kHeadH, width, kBodyLine, b, Face::Tile, draw) : 0;
  if (m[0]) {
    if (draw) say(screen, rect(x, y + kHeadH + lines * kBodyLine, width, kBodyLine), m, Face::Tile,
                  fui::TextAlign::Left);
    ++lines;
  }
  return kHeadH + lines * kBodyLine + kEntryGap;
}

}  // namespace

void buildOracleLog(toybox::Screen& screen, const tt::Table& t, const int page) {
  const tt::Oracle& o = t.oracle;
  const int16_t w = screen.device().width;
  const int x = toybox::kMargin;
  const int width = w - 2 * x;
  const int room = kCreditY - 8 - kLogTop;
  // Newest first, as many whole entries as fit a page.
  int starts[tt::kOracleLog + 1] = {};
  int pages = 0;
  for (int i = 0, used = room; i < o.len; ++i) {
    const int h = logEntry(screen, o.log[o.len - 1 - i], x, 0, width, false);
    if (used + h > room && (used > 0 || pages == 0)) {
      starts[pages++] = i;
      used = 0;
    }
    used += h;
  }
  starts[pages] = o.len;
  const int shown = pages ? page % pages : 0;
  // The page in the corner when there is more than one, else the count.
  char right[16];
  if (pages > 1) {
    std::snprintf(right, sizeof(right), "%d/%d", shown + 1, pages);
  } else {
    std::snprintf(right, sizeof(right), "%d", o.len);
  }
  header(screen, "ORACLE LOG", right);
  if (o.len == 0) {
    prose(screen, rect(x, 300, width, 80), "Nothing asked yet.", Face::Ui, 2);
  }
  int y = kLogTop;
  for (int i = pages ? starts[shown] : 0; i < (pages ? starts[shown + 1] : 0); ++i) {
    const int h = logEntry(screen, o.log[o.len - 1 - i], x, y, width, true);
    screen.target().fill(rect(x, y + h - kEntryGap / 2, width, 1), kGrey);
    y += h;
  }
  // CC BY asks for the credit where the tables are used.
  say(screen, rect(x, kCreditY, width, 22), "Oracle tables: Ironsworn by Shawn Tomkin, CC BY 4.0", Face::Tile);
  if (pages > 1) {
    // The loud key turns the page; from the last it goes back to the first.
    bottomBar(screen, "CLEAR", ActionClear, shown + 1 < pages ? "NEXT" : "FIRST", ActionLogPage, true);
  } else {
    bottomBar(screen, "CLEAR", ActionClear, "DONE", ActionBack, true);
  }
}

}  // namespace tabletopui
