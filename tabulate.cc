/**
 * Copyright 2022 Kiran Nowak(kiran.nowak@gmail.com)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <locale>
#include <algorithm>
#include <regex>
#include <map>
#include <string>
#include "tabulate.h"

namespace tabulate::symbols
{
// clang-format off
static const std::map<std::string, std::vector<std::string>> graph_symbols = {
  {
    "braille_up", {
      " ", "⢀", "⢠", "⢰", "⢸",
      "⡀", "⣀", "⣠", "⣰", "⣸",
      "⡄", "⣄", "⣤", "⣴", "⣼",
      "⡆", "⣆", "⣦", "⣶", "⣾",
      "⡇", "⣇", "⣧", "⣷", "⣿"
    }
  },
  {
    "braille_down", {
      " ", "⠈", "⠘", "⠸", "⢸",
      "⠁", "⠉", "⠙", "⠹", "⢹",
      "⠃", "⠋", "⠛", "⠻", "⢻",
      "⠇", "⠏", "⠟", "⠿", "⢿",
      "⡇", "⡏", "⡟", "⡿", "⣿"
    }
  },
  {
    "block_up", {
      " ", "▗", "▗", "▐", "▐",
      "▖", "▄", "▄", "▟", "▟",
      "▖", "▄", "▄", "▟", "▟",
      "▌", "▙", "▙", "█", "█",
      "▌", "▙", "▙", "█", "█"
    }
  },
  {
    "block_down", {
      " ", "▝", "▝", "▐", "▐",
      "▘", "▀", "▀", "▜", "▜",
      "▘", "▀", "▀", "▜", "▜",
      "▌", "▛", "▛", "█", "█",
      "▌", "▛", "▛", "█", "█"
    }
  },
  {
    "tty_up", {
      " ", "░", "░", "▒", "▒",
      "░", "░", "▒", "▒", "█",
      "░", "▒", "▒", "▒", "█",
      "▒", "▒", "▒", "█", "█",
      "▒", "█", "█", "█", "█"
    }
  },
  {
    "tty_down", {
      " ", "░", "░", "▒", "▒",
      "░", "░", "▒", "▒", "█",
      "░", "▒", "▒", "▒", "█",
      "▒", "▒", "▒", "█", "█",
      "▒", "█", "█", "█", "█"
    }
  }
};
// clang-format on

const std::vector<std::string> &get_graph_symbols(const std::string &name)
{
  static const std::vector<std::string> empty;
  auto it = graph_symbols.find(name);
  return it == graph_symbols.end() ? empty : it->second;
}
} // namespace tabulate::symbols

namespace tabulate
{
#define BYTEn(v, n) (((v) >> ((n) * 8)) & 0xFF)

// TrueColor implementation
TrueColor::TrueColor() : hex(DEFAULT), color(Color::none) {}

TrueColor::TrueColor(int hex) : hex(hex), color(Color::none) {}

TrueColor::TrueColor(Color color) : color(color)
{
  switch (color) {
    case Color::black:
      hex = 0x808080;
      break;
    case Color::red:
      hex = 0xFF0000;
      break;
    case Color::green:
      hex = 0x008000;
      break;
    case Color::yellow:
      hex = 0xFFFF00;
      break;
    case Color::blue:
      hex = 0x0000FF;
      break;
    case Color::magenta:
      hex = 0xFF00FF;
      break;
    case Color::cyan:
      hex = 0x00FFFF;
      break;
    case Color::white:
      hex = 0xFFFFFF;
      break;
    case Color::none:
    default:
      hex = DEFAULT;
      break;
  }
}

std::tuple<unsigned char, unsigned char, unsigned char> TrueColor::RGB() const
{
  return std::make_tuple(BYTEn(hex, 2), BYTEn(hex, 1), BYTEn(hex, 0));
}

TrueColor TrueColor::merge(const TrueColor &a, const TrueColor &b)
{
  int rr = (BYTEn(a.hex, 2) + BYTEn(b.hex, 2) + 1) / 2;
  int gg = (BYTEn(a.hex, 1) + BYTEn(b.hex, 1) + 1) / 2;
  int bb = (BYTEn(a.hex, 0) + BYTEn(b.hex, 0) + 1) / 2;

  return TrueColor((rr << 16) | (gg << 8) | bb);
}

double TrueColor::similarity(const TrueColor &a, const TrueColor &b)
{
  // d = sqrt((r2-r1)^2 + (g2-g1)^2 + (b2-b1)^2)
  int dr = BYTEn(a.hex, 2) - BYTEn(b.hex, 2);
  int dg = BYTEn(a.hex, 1) - BYTEn(b.hex, 1);
  int db = BYTEn(a.hex, 0) - BYTEn(b.hex, 0);

  const double r = sqrt(static_cast<double>(255 * 255 * 3));
  double d = sqrt(static_cast<double>(dr * dr + dg * dg + db * db));

  return d / r;
}

Color TrueColor::most_similar(const TrueColor &a)
{
  struct color_distance {
    Color color;
    double distance;

    color_distance(const TrueColor &color, Color base) : color(base)
    {
      distance = similarity(color, base);
    }

    bool operator<(const color_distance &other) const
    {
      return (distance < other.distance);
    }
  };

  // clang-format off
  std::vector<color_distance> distances = {
    color_distance(a, Color::black),
    color_distance(a, Color::red),
    color_distance(a, Color::green),
    color_distance(a, Color::yellow),
    color_distance(a, Color::blue),
    color_distance(a, Color::magenta),
    color_distance(a, Color::cyan),
    color_distance(a, Color::white),
    color_distance(a, Color::none),
  };
  // clang-format on
  std::sort(distances.begin(), distances.end());

  return distances[0].color;
}

Format::Format()
{
  cell.width = 0;
  cell.height = 0;
  cell.align = Align::left;
  cell.color = Color::none;
  cell.background_color = Color::none;

  // border-left
  borders.left.visiable = true;
  borders.left.padding = 1;
  borders.left.content = symbols::vline;
  borders.left.color = Color::none;
  borders.left.background_color = Color::none;
  borders.left.style = Border::Style::solid;
  borders.left.draw_outer = true;

  // border-right
  borders.right.visiable = true;
  borders.right.padding = 1;
  borders.right.content = symbols::vline;
  borders.right.color = Color::none;
  borders.right.background_color = Color::none;
  borders.right.style = Border::Style::solid;
  borders.right.draw_outer = true;

  // border-top
  borders.top.visiable = true;
  borders.top.padding = 0;
  borders.top.content = symbols::hline;
  borders.top.color = Color::none;
  borders.top.background_color = Color::none;
  borders.top.style = Border::Style::solid;
  borders.top.draw_outer = true;

  // border-bottom
  borders.bottom.visiable = true;
  borders.bottom.padding = 0;
  borders.bottom.content = symbols::hline;
  borders.bottom.color = Color::none;
  borders.bottom.background_color = Color::none;
  borders.bottom.style = Border::Style::solid;
  borders.bottom.draw_outer = true;

  // corner-top_left
  corners.top_left.visiable = true;
  corners.top_left.content = symbols::left_up;
  corners.top_left.color = Color::none;
  corners.top_left.background_color = Color::none;
  corners.top_left.style = Corner::Style::normal;
  corners.top_left.draw_outer = true;

  // corner-top_right
  corners.top_right.visiable = true;
  corners.top_right.content = symbols::right_up;
  corners.top_right.color = Color::none;
  corners.top_right.background_color = Color::none;
  corners.top_right.style = Corner::Style::normal;
  corners.top_right.draw_outer = true;

  // corner-bottom_left
  corners.bottom_left.visiable = true;
  corners.bottom_left.content = symbols::left_down;
  corners.bottom_left.color = Color::none;
  corners.bottom_left.background_color = Color::none;
  corners.bottom_left.style = Corner::Style::normal;
  corners.bottom_left.draw_outer = true;

  // corner-bottom_right
  corners.bottom_right.visiable = true;
  corners.bottom_right.content = symbols::right_down;
  corners.bottom_right.color = Color::none;
  corners.bottom_right.background_color = Color::none;
  corners.bottom_right.style = Corner::Style::normal;
  corners.bottom_right.draw_outer = true;

  // Cross junction (all four directions)
  corners.cross.visiable = true;
  corners.cross.content = symbols::cross;
  corners.cross.color = Color::none;
  corners.cross.background_color = Color::none;
  corners.cross.style = Corner::Style::normal;
  corners.cross.draw_outer = true;

  // Tee-north junction (left, right, up)
  corners.bottom_middle.visiable = true;
  corners.bottom_middle.content = symbols::div_down;
  corners.bottom_middle.color = Color::none;
  corners.bottom_middle.background_color = Color::none;
  corners.bottom_middle.style = Corner::Style::normal;
  corners.bottom_middle.draw_outer = true;

  // Tee-south junction (left, right, down)
  corners.top_middle.visiable = true;
  corners.top_middle.content = symbols::div_up;
  corners.top_middle.color = Color::none;
  corners.top_middle.background_color = Color::none;
  corners.top_middle.style = Corner::Style::normal;
  corners.top_middle.draw_outer = true;

  // Tee-west junction (up, down, left)
  corners.middle_right.visiable = true;
  corners.middle_right.content = symbols::div_right;
  corners.middle_right.color = Color::none;
  corners.middle_right.background_color = Color::none;
  corners.middle_right.style = Corner::Style::normal;
  corners.middle_right.draw_outer = true;

  // Tee-east junction (up, down, right)
  corners.middle_left.visiable = true;
  corners.middle_left.content = symbols::div_left;
  corners.middle_left.color = Color::none;
  corners.middle_left.background_color = Color::none;
  corners.middle_left.style = Corner::Style::normal;
  corners.middle_left.draw_outer = true;

  // internationlization
  internationlization.locale = "";
  internationlization.multi_bytes_character = true;
}

// Border methods
Format &Format::border(const std::string &value)
{
  borders.left.content = value;
  borders.right.content = value;
  borders.top.content = value;
  borders.bottom.content = value;
  return *this;
}

Format &Format::border_padding(size_t value)
{
  borders.left.padding = value;
  borders.right.padding = value;
  borders.top.padding = value;
  borders.bottom.padding = value;
  return *this;
}

Format &Format::border_color(TrueColor value)
{
  borders.left.color = value;
  borders.right.color = value;
  borders.top.color = value;
  borders.bottom.color = value;
  return *this;
}

Format &Format::border_background_color(TrueColor value)
{
  borders.left.background_color = value;
  borders.right.background_color = value;
  borders.top.background_color = value;
  borders.bottom.background_color = value;
  return *this;
}

Format &Format::border_left(const std::string &value)
{
  borders.left.content = value;
  return *this;
}

Format &Format::border_left_color(TrueColor value)
{
  borders.left.color = value;
  return *this;
}

Format &Format::border_left_background_color(TrueColor value)
{
  borders.left.background_color = value;
  return *this;
}

Format &Format::border_left_padding(size_t value)
{
  borders.left.padding = value;
  return *this;
}

Format &Format::border_right(const std::string &value)
{
  borders.right.content = value;
  return *this;
}

Format &Format::border_right_color(TrueColor value)
{
  borders.right.color = value;
  return *this;
}

Format &Format::border_right_background_color(TrueColor value)
{
  borders.right.background_color = value;
  return *this;
}

Format &Format::border_right_padding(size_t value)
{
  borders.right.padding = value;
  return *this;
}

Format &Format::border_top(const std::string &value)
{
  borders.top.content = value;
  return *this;
}

Format &Format::border_top_color(TrueColor value)
{
  borders.top.color = value;
  return *this;
}

Format &Format::border_top_background_color(TrueColor value)
{
  borders.top.background_color = value;
  return *this;
}

Format &Format::border_top_padding(size_t value)
{
  borders.top.padding = value;
  return *this;
}

Format &Format::border_bottom(const std::string &value)
{
  borders.bottom.content = value;
  return *this;
}

Format &Format::border_bottom_color(TrueColor value)
{
  borders.bottom.color = value;
  return *this;
}

Format &Format::border_bottom_background_color(TrueColor value)
{
  borders.bottom.background_color = value;
  return *this;
}

Format &Format::show_border()
{
  borders.left.visiable = true;
  borders.right.visiable = true;
  borders.top.visiable = true;
  borders.bottom.visiable = true;
  return *this;
}

Format &Format::hide_border()
{
  borders.left.visiable = false;
  borders.right.visiable = false;
  borders.top.visiable = false;
  borders.bottom.visiable = false;
  return *this;
}

Format &Format::show_border_top()
{
  borders.top.visiable = true;
  return *this;
}

Format &Format::hide_border_top()
{
  borders.top.visiable = false;
  return *this;
}

Format &Format::show_border_bottom()
{
  borders.bottom.visiable = true;
  return *this;
}

Format &Format::hide_border_bottom()
{
  borders.bottom.visiable = false;
  return *this;
}

Format &Format::show_border_left()
{
  borders.left.visiable = true;
  return *this;
}

Format &Format::hide_border_left()
{
  borders.left.visiable = false;
  return *this;
}

Format &Format::show_border_right()
{
  borders.right.visiable = true;
  return *this;
}

Format &Format::hide_border_right()
{
  borders.right.visiable = false;
  return *this;
}

// Corner methods
Format &Format::corner(const std::string &value)
{
  corners.top_left.content = value;
  corners.top_right.content = value;
  corners.bottom_left.content = value;
  corners.bottom_right.content = value;
  return *this;
}

Format &Format::corner_color(TrueColor value)
{
  corners.top_left.color = value;
  corners.top_right.color = value;
  corners.bottom_left.color = value;
  corners.bottom_right.color = value;
  return *this;
}

Format &Format::corner_background_color(TrueColor value)
{
  corners.top_left.background_color = value;
  corners.top_right.background_color = value;
  corners.bottom_left.background_color = value;
  corners.bottom_right.background_color = value;
  return *this;
}

Format &Format::corner_top_left(const std::string &value)
{
  corners.top_left.content = value;
  return *this;
}

Format &Format::corner_top_left_color(TrueColor value)
{
  corners.top_left.color = value;
  return *this;
}

Format &Format::corner_top_left_background_color(TrueColor value)
{
  corners.top_left.background_color = value;
  return *this;
}

Format &Format::corner_top_right(const std::string &value)
{
  corners.top_right.content = value;
  return *this;
}

Format &Format::corner_top_right_color(TrueColor value)
{
  corners.top_right.color = value;
  return *this;
}

Format &Format::corner_top_right_background_color(TrueColor value)
{
  corners.top_right.background_color = value;
  return *this;
}

Format &Format::corner_bottom_left(const std::string &value)
{
  corners.bottom_left.content = value;
  return *this;
}

Format &Format::corner_bottom_left_color(TrueColor value)
{
  corners.bottom_left.color = value;
  return *this;
}

Format &Format::corner_bottom_left_background_color(TrueColor value)
{
  corners.bottom_left.background_color = value;
  return *this;
}

Format &Format::corner_bottom_right(const std::string &value)
{
  corners.bottom_right.content = value;
  return *this;
}

Format &Format::corner_bottom_right_color(TrueColor value)
{
  corners.bottom_right.color = value;
  return *this;
}

Format &Format::corner_bottom_right_background_color(TrueColor value)
{
  corners.bottom_right.background_color = value;
  return *this;
}

// Internationalization methods
const std::string &Format::locale() const
{
  return internationlization.locale;
}

Format &Format::locale(const std::string &value)
{
  internationlization.locale = value;
  return *this;
}

bool Format::multi_bytes_character() const
{
  return internationlization.multi_bytes_character;
}

Format &Format::multi_bytes_character(bool value)
{
  internationlization.multi_bytes_character = value;
  return *this;
}

// New border style methods
Format &Format::border_style(Border::Style style)
{
  borders.left.style = style;
  borders.right.style = style;
  borders.top.style = style;
  borders.bottom.style = style;
  return *this;
}

Format &Format::border_left_style(Border::Style style)
{
  borders.left.style = style;
  return *this;
}

Format &Format::border_right_style(Border::Style style)
{
  borders.right.style = style;
  return *this;
}

Format &Format::border_top_style(Border::Style style)
{
  borders.top.style = style;
  return *this;
}

Format &Format::border_bottom_style(Border::Style style)
{
  borders.bottom.style = style;
  return *this;
}

// New border outer drawing methods
Format &Format::draw_outer_border(bool value)
{
  borders.left.draw_outer = value;
  borders.right.draw_outer = value;
  borders.top.draw_outer = value;
  borders.bottom.draw_outer = value;
  return *this;
}

Format &Format::draw_outer_left_border(bool value)
{
  borders.left.draw_outer = value;
  return *this;
}

Format &Format::draw_outer_right_border(bool value)
{
  borders.right.draw_outer = value;
  return *this;
}

Format &Format::draw_outer_top_border(bool value)
{
  borders.top.draw_outer = value;
  return *this;
}

Format &Format::draw_outer_bottom_border(bool value)
{
  borders.bottom.draw_outer = value;
  return *this;
}

// New corner style methods
Format &Format::corner_style(Corner::Style style)
{
  corners.top_left.style = style;
  corners.top_right.style = style;
  corners.bottom_left.style = style;
  corners.bottom_right.style = style;
  return *this;
}

Format &Format::corner_top_left_style(Corner::Style style)
{
  corners.top_left.style = style;
  return *this;
}

Format &Format::corner_top_right_style(Corner::Style style)
{
  corners.top_right.style = style;
  return *this;
}

Format &Format::corner_bottom_left_style(Corner::Style style)
{
  corners.bottom_left.style = style;
  return *this;
}

Format &Format::corner_bottom_right_style(Corner::Style style)
{
  corners.bottom_right.style = style;
  return *this;
}

// New corner outer drawing methods
Format &Format::draw_outer_corner(bool value)
{
  corners.top_left.draw_outer = value;
  corners.top_right.draw_outer = value;
  corners.bottom_left.draw_outer = value;
  corners.bottom_right.draw_outer = value;
  return *this;
}

Format &Format::draw_outer_top_left_corner(bool value)
{
  corners.top_left.draw_outer = value;
  return *this;
}

Format &Format::draw_outer_top_right_corner(bool value)
{
  corners.top_right.draw_outer = value;
  return *this;
}

Format &Format::draw_outer_bottom_left_corner(bool value)
{
  corners.bottom_left.draw_outer = value;
  return *this;
}

Format &Format::draw_outer_bottom_right_corner(bool value)
{
  corners.bottom_right.draw_outer = value;
  return *this;
}

// Format methods for setting corner cross junction properties
Format &Format::corner_cross(const std::string &value)
{
  corners.cross.content = value;
  return *this;
}

Format &Format::corner_bottom_middle(const std::string &value)
{
  corners.bottom_middle.content = value;
  return *this;
}

Format &Format::corner_top_middle(const std::string &value)
{
  corners.top_middle.content = value;
  return *this;
}

Format &Format::corner_middle_right(const std::string &value)
{
  corners.middle_right.content = value;
  return *this;
}

Format &Format::corner_middle_left(const std::string &value)
{
  corners.middle_left.content = value;
  return *this;
}

Format &Format::corner_cross_color(TrueColor value)
{
  corners.cross.color = value;
  return *this;
}

Format &Format::corner_bottom_middle_color(TrueColor value)
{
  corners.bottom_middle.color = value;
  return *this;
}

Format &Format::corner_top_middle_color(TrueColor value)
{
  corners.top_middle.color = value;
  return *this;
}

Format &Format::corner_middle_right_color(TrueColor value)
{
  corners.middle_right.color = value;
  return *this;
}

Format &Format::corner_middle_left_color(TrueColor value)
{
  corners.middle_left.color = value;
  return *this;
}

Format &Format::corner_cross_background_color(TrueColor value)
{
  corners.cross.background_color = value;
  return *this;
}

Format &Format::corner_bottom_middle_background_color(TrueColor value)
{
  corners.bottom_middle.background_color = value;
  return *this;
}

Format &Format::corner_top_middle_background_color(TrueColor value)
{
  corners.top_middle.background_color = value;
  return *this;
}

Format &Format::corner_middle_right_background_color(TrueColor value)
{
  corners.middle_right.background_color = value;
  return *this;
}

Format &Format::corner_middle_left_background_color(TrueColor value)
{
  corners.middle_left.background_color = value;
  return *this;
}

// Method for setting bottom border padding
Format &Format::border_bottom_padding(size_t value)
{
  borders.bottom.padding = value;
  return *this;
}

// BatchFormat
BatchFormat::BatchFormat(std::vector<std::shared_ptr<Cell>> cells) : cells(std::move(cells)) {}

// Basic property methods
size_t BatchFormat::size()
{
  return cells.size();
}

BatchFormat &BatchFormat::width(size_t value)
{
  for (auto &cell : cells) {
    cell->format().width(value);
  }
  return *this;
}

BatchFormat &BatchFormat::align(Align value)
{
  for (auto &cell : cells) {
    cell->format().align(value);
  }
  return *this;
}

BatchFormat &BatchFormat::color(TrueColor value)
{
  for (auto &cell : cells) {
    cell->format().color(value);
  }
  return *this;
}

BatchFormat &BatchFormat::background_color(TrueColor value)
{
  for (auto &cell : cells) {
    cell->format().background_color(value);
  }
  return *this;
}

BatchFormat &BatchFormat::styles(Style value)
{
  for (auto &cell : cells) {
    cell->format().styles(value);
  }
  return *this;
}

// Border methods
BatchFormat &BatchFormat::border_padding(size_t value)
{
  for (auto &cell : cells) {
    cell->format().border_padding(value);
  }
  return *this;
}

BatchFormat &BatchFormat::border_left_padding(size_t value)
{
  for (auto &cell : cells) {
    cell->format().border_left_padding(value);
  }
  return *this;
}

BatchFormat &BatchFormat::border_right_padding(size_t value)
{
  for (auto &cell : cells) {
    cell->format().border_right_padding(value);
  }
  return *this;
}

BatchFormat &BatchFormat::border_top_padding(size_t value)
{
  for (auto &cell : cells) {
    cell->format().border_top_padding(value);
  }
  return *this;
}

BatchFormat &BatchFormat::border_bottom_padding(size_t value)
{
  for (auto &cell : cells) {
    cell->format().border_bottom_padding(value);
  }
  return *this;
}

BatchFormat &BatchFormat::border(const std::string &value)
{
  for (auto &cell : cells) {
    cell->format().border(value);
  }
  return *this;
}

BatchFormat &BatchFormat::border_color(TrueColor value)
{
  for (auto &cell : cells) {
    cell->format().border_color(value);
  }
  return *this;
}

BatchFormat &BatchFormat::border_background_color(TrueColor value)
{
  for (auto &cell : cells) {
    cell->format().border_background_color(value);
  }
  return *this;
}

BatchFormat &BatchFormat::border_left(const std::string &value)
{
  for (auto &cell : cells) {
    cell->format().border_left(value);
  }
  return *this;
}

BatchFormat &BatchFormat::border_left_color(TrueColor value)
{
  for (auto &cell : cells) {
    cell->format().border_left_color(value);
  }
  return *this;
}

BatchFormat &BatchFormat::border_left_background_color(TrueColor value)
{
  for (auto &cell : cells) {
    cell->format().border_left_background_color(value);
  }
  return *this;
}

BatchFormat &BatchFormat::border_right(const std::string &value)
{
  for (auto &cell : cells) {
    cell->format().border_right(value);
  }
  return *this;
}

BatchFormat &BatchFormat::border_right_color(TrueColor value)
{
  for (auto &cell : cells) {
    cell->format().border_right_color(value);
  }
  return *this;
}

BatchFormat &BatchFormat::border_right_background_color(TrueColor value)
{
  for (auto &cell : cells) {
    cell->format().border_right_background_color(value);
  }
  return *this;
}

BatchFormat &BatchFormat::border_top(const std::string &value)
{
  for (auto &cell : cells) {
    cell->format().border_top(value);
  }
  return *this;
}

BatchFormat &BatchFormat::border_top_color(TrueColor value)
{
  for (auto &cell : cells) {
    cell->format().border_top_color(value);
  }
  return *this;
}

BatchFormat &BatchFormat::border_top_background_color(TrueColor value)
{
  for (auto &cell : cells) {
    cell->format().border_top_background_color(value);
  }
  return *this;
}

BatchFormat &BatchFormat::border_bottom(const std::string &value)
{
  for (auto &cell : cells) {
    cell->format().border_bottom(value);
  }
  return *this;
}

BatchFormat &BatchFormat::border_bottom_color(TrueColor value)
{
  for (auto &cell : cells) {
    cell->format().border_bottom_color(value);
  }
  return *this;
}

BatchFormat &BatchFormat::border_bottom_background_color(TrueColor value)
{
  for (auto &cell : cells) {
    cell->format().border_bottom_background_color(value);
  }
  return *this;
}

// Border visibility methods
BatchFormat &BatchFormat::show_border()
{
  for (auto &cell : cells) {
    cell->format().show_border();
  }
  return *this;
}

BatchFormat &BatchFormat::hide_border()
{
  for (auto &cell : cells) {
    cell->format().hide_border();
  }
  return *this;
}

BatchFormat &BatchFormat::show_border_top()
{
  for (auto &cell : cells) {
    cell->format().show_border_top();
  }
  return *this;
}

BatchFormat &BatchFormat::hide_border_top()
{
  for (auto &cell : cells) {
    cell->format().hide_border_top();
  }
  return *this;
}

BatchFormat &BatchFormat::show_border_bottom()
{
  for (auto &cell : cells) {
    cell->format().show_border_bottom();
  }
  return *this;
}

BatchFormat &BatchFormat::hide_border_bottom()
{
  for (auto &cell : cells) {
    cell->format().hide_border_bottom();
  }
  return *this;
}

BatchFormat &BatchFormat::show_border_left()
{
  for (auto &cell : cells) {
    cell->format().show_border_left();
  }
  return *this;
}

BatchFormat &BatchFormat::hide_border_left()
{
  for (auto &cell : cells) {
    cell->format().hide_border_left();
  }
  return *this;
}

BatchFormat &BatchFormat::show_border_right()
{
  for (auto &cell : cells) {
    cell->format().show_border_right();
  }
  return *this;
}

BatchFormat &BatchFormat::hide_border_right()
{
  for (auto &cell : cells) {
    cell->format().hide_border_right();
  }
  return *this;
}

// Corner methods
BatchFormat &BatchFormat::corner(const std::string &value)
{
  for (auto &cell : cells) {
    cell->format().corner(value);
  }
  return *this;
}

BatchFormat &BatchFormat::corner_color(TrueColor value)
{
  for (auto &cell : cells) {
    cell->format().corner_color(value);
  }
  return *this;
}

BatchFormat &BatchFormat::corner_background_color(TrueColor value)
{
  for (auto &cell : cells) {
    cell->format().corner_background_color(value);
  }
  return *this;
}

BatchFormat &BatchFormat::corner_top_left(const std::string &value)
{
  for (auto &cell : cells) {
    cell->format().corner_top_left(value);
  }
  return *this;
}

BatchFormat &BatchFormat::corner_top_left_color(TrueColor value)
{
  for (auto &cell : cells) {
    cell->format().corner_top_left_color(value);
  }
  return *this;
}

BatchFormat &BatchFormat::corner_top_left_background_color(TrueColor value)
{
  for (auto &cell : cells) {
    cell->format().corner_top_left_background_color(value);
  }
  return *this;
}

BatchFormat &BatchFormat::corner_top_right(const std::string &value)
{
  for (auto &cell : cells) {
    cell->format().corner_top_right(value);
  }
  return *this;
}

BatchFormat &BatchFormat::corner_top_right_color(TrueColor value)
{
  for (auto &cell : cells) {
    cell->format().corner_top_right_color(value);
  }
  return *this;
}

BatchFormat &BatchFormat::corner_top_right_background_color(TrueColor value)
{
  for (auto &cell : cells) {
    cell->format().corner_top_right_background_color(value);
  }
  return *this;
}

BatchFormat &BatchFormat::corner_bottom_left(const std::string &value)
{
  for (auto &cell : cells) {
    cell->format().corner_bottom_left(value);
  }
  return *this;
}

BatchFormat &BatchFormat::corner_bottom_left_color(TrueColor value)
{
  for (auto &cell : cells) {
    cell->format().corner_bottom_left_color(value);
  }
  return *this;
}

BatchFormat &BatchFormat::corner_bottom_left_background_color(TrueColor value)
{
  for (auto &cell : cells) {
    cell->format().corner_bottom_left_background_color(value);
  }
  return *this;
}

BatchFormat &BatchFormat::corner_bottom_right(const std::string &value)
{
  for (auto &cell : cells) {
    cell->format().corner_bottom_right(value);
  }
  return *this;
}

BatchFormat &BatchFormat::corner_bottom_right_color(TrueColor value)
{
  for (auto &cell : cells) {
    cell->format().corner_bottom_right_color(value);
  }
  return *this;
}

BatchFormat &BatchFormat::corner_bottom_right_background_color(TrueColor value)
{
  for (auto &cell : cells) {
    cell->format().corner_bottom_right_background_color(value);
  }
  return *this;
}

// Internationalization methods
BatchFormat &BatchFormat::locale(const std::string &value)
{
  for (auto &cell : cells) {
    cell->format().locale(value);
  }
  return *this;
}

BatchFormat &BatchFormat::multi_bytes_character(bool value)
{
  for (auto &cell : cells) {
    cell->format().multi_bytes_character(value);
  }
  return *this;
}

// New border style methods for BatchFormat
BatchFormat &BatchFormat::border_style(Border::Style style)
{
  for (auto &cell : cells) {
    cell->format().border_style(style);
  }
  return *this;
}

BatchFormat &BatchFormat::border_left_style(Border::Style style)
{
  for (auto &cell : cells) {
    cell->format().border_left_style(style);
  }
  return *this;
}

BatchFormat &BatchFormat::border_right_style(Border::Style style)
{
  for (auto &cell : cells) {
    cell->format().border_right_style(style);
  }
  return *this;
}

BatchFormat &BatchFormat::border_top_style(Border::Style style)
{
  for (auto &cell : cells) {
    cell->format().border_top_style(style);
  }
  return *this;
}

BatchFormat &BatchFormat::border_bottom_style(Border::Style style)
{
  for (auto &cell : cells) {
    cell->format().border_bottom_style(style);
  }
  return *this;
}

// New border outer drawing methods for BatchFormat
BatchFormat &BatchFormat::draw_outer_border(bool value)
{
  for (auto &cell : cells) {
    cell->format().draw_outer_border(value);
  }
  return *this;
}

BatchFormat &BatchFormat::draw_outer_left_border(bool value)
{
  for (auto &cell : cells) {
    cell->format().draw_outer_left_border(value);
  }
  return *this;
}

BatchFormat &BatchFormat::draw_outer_right_border(bool value)
{
  for (auto &cell : cells) {
    cell->format().draw_outer_right_border(value);
  }
  return *this;
}

BatchFormat &BatchFormat::draw_outer_top_border(bool value)
{
  for (auto &cell : cells) {
    cell->format().draw_outer_top_border(value);
  }
  return *this;
}

BatchFormat &BatchFormat::draw_outer_bottom_border(bool value)
{
  for (auto &cell : cells) {
    cell->format().draw_outer_bottom_border(value);
  }
  return *this;
}

// New corner style methods for BatchFormat
BatchFormat &BatchFormat::corner_style(Corner::Style style)
{
  for (auto &cell : cells) {
    cell->format().corner_style(style);
  }
  return *this;
}

BatchFormat &BatchFormat::corner_top_left_style(Corner::Style style)
{
  for (auto &cell : cells) {
    cell->format().corner_top_left_style(style);
  }
  return *this;
}

BatchFormat &BatchFormat::corner_top_right_style(Corner::Style style)
{
  for (auto &cell : cells) {
    cell->format().corner_top_right_style(style);
  }
  return *this;
}

BatchFormat &BatchFormat::corner_bottom_left_style(Corner::Style style)
{
  for (auto &cell : cells) {
    cell->format().corner_bottom_left_style(style);
  }
  return *this;
}

BatchFormat &BatchFormat::corner_bottom_right_style(Corner::Style style)
{
  for (auto &cell : cells) {
    cell->format().corner_bottom_right_style(style);
  }
  return *this;
}

// New corner outer drawing methods for BatchFormat
BatchFormat &BatchFormat::draw_outer_corner(bool value)
{
  for (auto &cell : cells) {
    cell->format().draw_outer_corner(value);
  }
  return *this;
}

BatchFormat &BatchFormat::draw_outer_top_left_corner(bool value)
{
  for (auto &cell : cells) {
    cell->format().draw_outer_top_left_corner(value);
  }
  return *this;
}

BatchFormat &BatchFormat::draw_outer_top_right_corner(bool value)
{
  for (auto &cell : cells) {
    cell->format().draw_outer_top_right_corner(value);
  }
  return *this;
}

BatchFormat &BatchFormat::draw_outer_bottom_left_corner(bool value)
{
  for (auto &cell : cells) {
    cell->format().draw_outer_bottom_left_corner(value);
  }
  return *this;
}

BatchFormat &BatchFormat::draw_outer_bottom_right_corner(bool value)
{
  for (auto &cell : cells) {
    cell->format().draw_outer_bottom_right_corner(value);
  }
  return *this;
}
} // namespace tabulate

namespace tabulate
{
// Cell class methods implementation
Cell::Cell(const std::string &content) : content_(content) {}

const std::string &Cell::get() const
{
  return content_;
}

void Cell::set(const std::string &content)
{
  content_ = content;
}

size_t Cell::size()
{
  return display_width_of(content_, m_format.locale(), m_format.multi_bytes_character());
}

Format &Cell::format()
{
  return m_format;
}

const Format &Cell::format() const
{
  return m_format;
}

size_t Cell::width() const
{
  if (m_format.width() != 0) {
    return m_format.width();
  } else {
    if (content_.empty()) {
      return 0;
    } else {
      std::string line;
      std::stringstream ss(content_.c_str());

      size_t max_width = 0;
      while (std::getline(ss, line, '\n')) {
        max_width = std::max(max_width, display_width_of(line, m_format.locale(), true));
      }

      return max_width;
    }
  }
}

Align Cell::align() const
{
  return m_format.align();
}

TrueColor Cell::color() const
{
  return m_format.color();
}

TrueColor Cell::background_color() const
{
  return m_format.background_color();
}

Styles Cell::styles() const
{
  return m_format.styles();
}
} // namespace tabulate

namespace tabulate
{
size_t display_width_of(const std::string &text, const std::string &locale, bool wchar_enabled)
{
  // delete ansi escape sequences
  std::regex e("\x1b(?:[@-Z\\-_]|\\[[0-?]*[ -/]*[@-~])");
  std::string str = std::regex_replace(text, e, "");

  if (!wchar_enabled) {
    return str.length();
  }

  if (str.size() == 0) {
    return 0;
  }

  // XXX: Markus Kuhn's open-source wcswidth.c
#if defined(__unix__) || defined(__unix) || defined(__APPLE__)
  {
    // The behavior of wcswidth() depends on the LC_CTYPE category of the current locale.
    // Set the current locale based on cell properties before computing width
    auto old_locale = std::locale::global(std::locale(locale));

    // Convert from narrow std::string to wide string
    wchar_t stackbuff[128], *wstr = stackbuff;
    if (str.size() > 128) {
      wstr = new wchar_t[str.size()];
    }

    std::mbstowcs(wstr, str.c_str(), str.size());

    // Compute display width of wide string
    int len = wcswidth(wstr, str.size());

    if (wstr != stackbuff) {
      delete[] wstr;
    }

    // Restore old locale
    std::locale::global(old_locale);

    if (len >= 0) {
      return len;
    }
  }
#endif

  {
    return (str.length() - std::count_if(str.begin(), str.end(), [](char c) -> bool { return (c & 0xC0) == 0x80; }));
  }
}

std::string lstrip(const std::string &s)
{
  std::string trimed = s;
  trimed.erase(trimed.begin(), std::find_if(trimed.begin(), trimed.end(), [](int ch) { return !std::isspace(ch); }));
  return trimed;
}

std::string replace_all(std::string str, const std::string &from, const std::string &to)
{
  size_t curr = 0;
  while ((curr = str.find(from, curr)) != std::string::npos) {
    str.replace(curr, from.length(), to);
    curr += to.length(); // Handles case where 'to' is a substring of 'from'
  }
  return str;
}

std::vector<std::string> explode_string(const std::string &input, const std::vector<std::string> &separators)
{
  auto first_of = [](const std::string &input, size_t start, const std::vector<std::string> &separators) -> size_t {
    std::vector<size_t> indices;
    for (auto &c : separators) {
      auto index = input.find(c, start);
      if (index != std::string::npos) {
        indices.push_back(index);
      }
    }
    if (indices.size() > 0) {
      return *std::min_element(indices.begin(), indices.end());
    } else {
      return std::string::npos;
    }
  };

  std::vector<std::string> segments;

  size_t start = 0;
  while (true) {
    auto index = first_of(input, start, separators);

    if (index == std::string::npos) {
      segments.push_back(input.substr(start));
      return segments;
    }

    std::string word = input.substr(start, index - start);
    char next_character = input.substr(index, 1)[0];
    // Unlike whitespace, dashes and the like should stick to the word occurring before it.
    if (isspace(next_character)) {
      segments.push_back(word);
      segments.push_back(std::string(1, next_character));
    } else {
      segments.push_back(word + next_character);
    }
    start = index + 1;
  }

  return segments;
}

std::vector<std::string> wrap_lines(const std::string &str, size_t width, const std::string &locale, bool multi_bytes_character)
{
  std::vector<std::string> lines;
  {
    std::string line;
    std::stringstream ss(str);
    while (std::getline(ss, line, '\n')) {
      if (display_width_of(line, locale, multi_bytes_character) <= width) {
        lines.push_back(line);
        continue;
      }
      std::string wrapped;
      auto words = explode_string(line, {" ", "-", "\t"});
      for (auto &word : words) {
        if (display_width_of(wrapped, locale, multi_bytes_character) + display_width_of(word, locale, multi_bytes_character) > width) {
          if (display_width_of(wrapped, locale, multi_bytes_character) > 0) {
            lines.push_back(wrapped);
            wrapped = "";
          }

          while (display_width_of(word, locale, multi_bytes_character) > width) {
            wrapped = word.substr(0, width - 1) + "-";
            lines.push_back(wrapped);
            wrapped = "";
            word = word.substr(width - 1);
          }

          word = lstrip(word);
        }

        wrapped += word;
      }
      lines.push_back(wrapped);
    }
  }

  return lines;
}

std::string expand_to_size(const std::string &s, size_t len, bool multi_bytes_character)
{
  std::string r;
  if (s == "") {
    return std::string(' ', len);
  }
  if (len == 0) {
    return s;
  }
  size_t swidth = display_width_of(s, "", multi_bytes_character);
  for (size_t i = 0; i < len;) {
    if (swidth > len - i) {
      r += s.substr(0, len - i);
    } else {
      r += s;
    }
    i += swidth;
  }
  return r;
}
} // namespace tabulate

namespace tabulate
{
// Row implementation
// Basic access methods
Cell &Row::operator[](size_t index)
{
  if (index >= cells.size()) {
    size_t size = index - cells.size() + 1;
    for (size_t i = 0; i < size; i++) {
      add("");
    }
  }
  return *cells[index];
}

const Cell &Row::operator[](size_t index) const
{
  return *cells[index];
}

std::shared_ptr<Cell> &Row::cell(size_t index)
{
  return cells[index];
}

size_t Row::size() const
{
  return cells.size();
}

// Format methods
BatchFormat Row::format()
{
  return BatchFormat(cells);
}

BatchFormat Row::format(size_t from, size_t to)
{
  std::vector<std::shared_ptr<Cell>> selected_cells;
  for (size_t i = std::min(from, to); i <= std::max(from, to); i++) {
    selected_cells.push_back(cells[i]);
  }
  return BatchFormat(selected_cells);
}

BatchFormat Row::format(std::initializer_list<std::tuple<size_t, size_t>> ranges)
{
  std::vector<std::shared_ptr<Cell>> selected_cells;
  for (auto range : ranges) {
    for (size_t i = std::min(std::get<0>(range), std::get<1>(range)); i < std::max(std::get<0>(range), std::get<1>(range)); i++) {
      selected_cells.push_back(cells[i]);
    }
  }
  return BatchFormat(selected_cells);
}

// Iterator methods
Row::iterator Row::begin()
{
  return iterator(cells.begin());
}

Row::iterator Row::end()
{
  return iterator(cells.end());
}

Row::const_iterator Row::begin() const
{
  return const_iterator(cells.cbegin());
}

Row::const_iterator Row::end() const
{
  return const_iterator(cells.cend());
}

// Display methods
std::vector<std::string> Row::dump(StringFormatter stringformatter, BorderFormatter borderformatter, CornerFormatter cornerformatter, size_t row_index,
                                   size_t header_count, size_t total_rows) const
{
  size_t max_height = 0;
  std::vector<std::vector<std::string>> dumplines;

  // Determine format directly based on row_index and total_rows
  bool showtop = true;
  bool showbottom = (row_index == total_rows - 1);
  bool is_middle_row = (row_index > 0 && row_index < total_rows - 1);

  // Special case for single row in to_string<Row> context
  if (total_rows <= 1) {
    showbottom = true;
    is_middle_row = false;
  }

  for (auto const &cell : cells) {
    // #ifdef __DEBUG__
    //       std::cout << "cell: " << cell->get() << std::endl;
    //       std::cout << "\tcolor: " << to_string(cell->color()) << std::endl;
    //       std::cout << "\tbackground_color: " << to_string(cell->background_color()) << std::endl;
    //       if (!cell->styles().empty()) {
    //         std::cout << "\tstyles: ";
    //         for (auto &style : cell->styles()) {
    //           std::cout << to_string(style);
    //           if (&style != &cell->styles().back()) {
    //             std::cout << " ";
    //           }
    //         }
    //         std::cout << std::endl;
    //       }
    // #endif
    std::vector<std::string> wrapped;
    if (cell->width() == 0) {
      wrapped.push_back(cell->get());
    } else {
      wrapped = wrap_lines(cell->get(), cell->width(), cell->format().locale(), cell->format().multi_bytes_character());
      // #ifdef __DEBUG__
      //         std::cout << "wrap to " << wrapped.size() << " lines" << std::endl;
      // #endif
    }
    max_height = std::max(wrapped.size(), max_height);
    dumplines.push_back(wrapped);
  }

  std::vector<std::string> lines;
  if (showtop && cells.size() > 0 && cells[0]->format().borders.top.visiable) {
    std::string line;

    // Start with appropriate left corner type based on row position
    Which left_corner_type;
    if (is_middle_row) {
      // For middle rows, use middle_left (├) at the left edge
      left_corner_type = Which::middle_left;
    } else if (row_index == total_rows - 1) {
      // For the last row, use bottom_left if it's also the first row, otherwise use middle_left
      left_corner_type = (row_index == 0) ? Which::top_left : Which::middle_left;
    } else {
      // For the first row, use top_left
      left_corner_type = Which::top_left;
    }

    line += cornerformatter(left_corner_type, cells[0].get(), nullptr, nullptr, nullptr, nullptr, stringformatter);

    for (size_t i = 0; i < cells.size(); i++) {
      auto cell = cells[i].get();
      auto left = i > 0 ? cells[i - 1].get() : nullptr;
      auto right = (i + 1 < cells.size()) ? cells[i + 1].get() : nullptr;

      auto &borders = cell->format().borders;
      size_t size = borders.left.padding + cell->width() + borders.right.padding;

      // Use border formatter with the appropriate border type
      line += borderformatter(Which::top, cell, left, right, nullptr, nullptr, size, stringformatter);

      // Select appropriate corner type for the junction to the right of the current cell
      Which corner_type;
      if (i < cells.size() - 1) {
        // For internal junctions
        if (is_middle_row) {
          // Middle rows use cross (┼) for internal junctions
          corner_type = Which::cross;
        } else if (row_index == total_rows - 1) {
          // Last row uses bottom_middle (┴) for internal junctions if not the first row
          corner_type = (row_index == 0) ? Which::top_middle : Which::cross;
        } else {
          // First row uses top_middle (┬) for internal junctions
          corner_type = Which::top_middle;
        }
      } else {
        // For the right edge
        if (is_middle_row) {
          // Middle rows use middle_right (┤) at the right edge
          corner_type = Which::middle_right;
        } else if (row_index == total_rows - 1) {
          // Last row uses bottom_right if it's also the first row, otherwise use middle_right
          corner_type = (row_index == 0) ? Which::top_right : Which::middle_right;
        } else {
          // First row uses top_right at the right edge
          corner_type = Which::top_right;
        }
      }

      line += cornerformatter(corner_type, cell, nullptr, nullptr, nullptr, nullptr, stringformatter);
    }
    lines.push_back(line);
  }

  // padding lines on top
  if (cells[0]->format().borders.top.padding > 0) {
    std::string padline;
    for (size_t i = 0; i < cells.size(); i++) {
      auto cell = cells[i].get();
      auto &borders = cell->format().borders;
      auto left = i > 0 ? cells[i - 1].get() : nullptr;
      auto right = (i + 1 < cells.size()) ? cells[i + 1].get() : nullptr;
      size_t size = borders.left.padding + cell->width() + borders.right.padding;

      padline += borderformatter(Which::left, cell, left, right, nullptr, nullptr, 1, stringformatter);
      padline += stringformatter(std::string(size, ' '), Color::none, cell->background_color(), {});
    }
    padline += borderformatter(Which::right, cells.back().get(), nullptr, nullptr, nullptr, nullptr, 1, stringformatter);

    for (size_t i = 0; i < cells[0]->format().borders.top.padding; i++) {
      lines.push_back(padline);
    }
  }

  // border padding words padding sepeartor padding words padding sepeartor border
  for (size_t i = 0; i < max_height; i++) {
    std::string line;
    line += borderformatter(Which::left, cells[0].get(), nullptr, cells.size() >= 2 ? cells[1].get() : nullptr, nullptr, nullptr, 1, stringformatter);
    for (size_t j = 0; j < cells.size(); j++) {
      auto cell = cells[j].get();
      auto left = j > 0 ? cells[j - 1].get() : nullptr;
      auto right = (j + 1 < cells.size()) ? cells[j + 1].get() : nullptr;
      auto foreground_color = cell->color();
      auto background_color = cell->background_color();
      size_t cell_offset = 0, empty_lines = max_height - dumplines[j].size();
      auto alignment = cell->format().align();
      if (alignment & Align::bottom) {
        cell_offset = empty_lines;
      } else if (alignment & Align::top) {
        cell_offset = 0;
      } else { // DEFAULT: align center in vertical
        cell_offset = empty_lines / 2;
      }
      line += stringformatter(std::string(cell->format().borders.left.padding, ' '), Color::none, background_color, {});
      if (i < cell_offset || i >= dumplines[j].size() + cell_offset) {
        line += stringformatter(std::string(cell->width(), ' '), Color::none, background_color, cell->styles());
      } else {
        auto align_line_by = [&](const std::string &str, size_t width, Align align, const std::string &locale, bool multi_bytes_character) -> std::string {
          size_t linesize = display_width_of(str, locale, multi_bytes_character);
          if (linesize >= width) {
            return stringformatter(str, foreground_color, background_color, cell->styles());
          }
          if (align & Align::hcenter) {
            size_t remains = width - linesize;
            return stringformatter(std::string(remains / 2, ' '), Color::none, background_color, {})
                   + stringformatter(str, foreground_color, background_color, cell->styles())
                   + stringformatter(std::string((remains + 1) / 2, ' '), Color::none, background_color, {});
          } else if (align & Align::right) {
            return stringformatter(std::string(width - linesize, ' '), Color::none, background_color, {})
                   + stringformatter(str, foreground_color, background_color, cell->styles());
          } else { // DEFAULT: align left in horizontal
            return stringformatter(str, foreground_color, background_color, cell->styles())
                   + stringformatter(std::string(width - linesize, ' '), Color::none, background_color, {});
          }
        };

        line += align_line_by(dumplines[j][i - cell_offset], cell->width(), alignment, cell->format().locale(), cell->format().multi_bytes_character());
      }

      line += stringformatter(std::string(cell->format().borders.right.padding, ' '), Color::none, background_color, {});

      // Use appropriate border type for cell separators or right border
      Which border_type = Which::right;
      line += borderformatter(border_type, cell, left, right, nullptr, nullptr, 1, stringformatter);
    }

    lines.push_back(line);
  }

  // padding lines on bottom
  if (cells.back()->format().borders.bottom.padding > 0) {
    std::string padline;
    for (size_t i = 0; i < cells.size(); i++) {
      auto cell = cells[i].get();
      auto &borders = cell->format().borders;
      auto left = i > 0 ? cells[i - 1].get() : nullptr;
      auto right = (i + 1 < cells.size()) ? cells[i + 1].get() : nullptr;
      size_t size = borders.left.padding + cell->width() + borders.right.padding;

      padline += borderformatter(Which::left, cell, left, right, nullptr, nullptr, 1, stringformatter);
      padline += stringformatter(std::string(size, ' '), Color::none, cell->background_color(), {});
    }
    padline += borderformatter(Which::right, cells.back().get(), nullptr, nullptr, nullptr, nullptr, 1, stringformatter);

    for (size_t i = 0; i < cells.back()->format().borders.bottom.padding; i++) {
      lines.push_back(padline);
    }
  }

  if (showbottom && cells.size() > 0 && cells.back()->format().borders.bottom.visiable) {
    std::string line;

    // Start with appropriate left corner type based on row position
    Which left_corner_type;
    if (is_middle_row) {
      // For middle rows, use middle_left (├) at the left edge
      left_corner_type = Which::middle_left;
    } else if (row_index == total_rows - 1) {
      // For the last row, use bottom_left at the left edge
      left_corner_type = Which::bottom_left;
    } else {
      // For the first row (if showing bottom border), use middle_left
      left_corner_type = Which::middle_left;
    }

    line += cornerformatter(left_corner_type, cells[0].get(), nullptr, nullptr, nullptr, nullptr, stringformatter);

    for (size_t i = 0; i < cells.size(); i++) {
      auto cell = cells[i].get();
      auto left = i > 0 ? cells[i - 1].get() : nullptr;
      auto right = (i + 1 < cells.size()) ? cells[i + 1].get() : nullptr;

      auto &borders = cell->format().borders;
      size_t size = borders.left.padding + cell->width() + borders.right.padding;

      // Use border formatter with the appropriate border type
      line += borderformatter(Which::bottom, cell, left, right, nullptr, nullptr, size, stringformatter);

      // Select appropriate corner type for the junction to the right of the current cell
      Which corner_type;
      if (i < cells.size() - 1) {
        // For internal junctions
        if (is_middle_row) {
          // Middle rows use cross (┼) for internal junctions
          corner_type = Which::cross;
        } else if (row_index == total_rows - 1) {
          // Last row uses bottom_middle (┴) for internal junctions
          corner_type = Which::bottom_middle;
        } else {
          // First or other non-last rows use cross for internal junctions if showing bottom
          corner_type = Which::cross;
        }
      } else {
        // For the right edge
        if (is_middle_row) {
          // Middle rows use middle_right (┤) at the right edge
          corner_type = Which::middle_right;
        } else if (row_index == total_rows - 1) {
          // Last row uses bottom_right at the right edge
          corner_type = Which::bottom_right;
        } else {
          // First or other non-last rows use middle_right at the right edge if showing bottom
          corner_type = Which::middle_right;
        }
      }

      // Ensure proper junction characters are used
      line += cornerformatter(corner_type, cell, nullptr, nullptr, nullptr, nullptr, stringformatter);
    }
    lines.push_back(line);
  }

  return lines;
}

// Column implementation
// Basic access methods
void Column::add(std::shared_ptr<Cell> cell)
{
  cells.push_back(cell);
}

BatchFormat Column::format()
{
  return BatchFormat(cells);
}

size_t Column::size()
{
  return cells.size();
}

Cell &Column::operator[](size_t index)
{
  return *cells[index];
}

const Cell &Column::operator[](size_t index) const
{
  return *cells[index];
}

// Format methods
BatchFormat Column::format(size_t from, size_t to)
{
  std::vector<std::shared_ptr<Cell>> selected_cells;
  for (size_t i = std::min(from, to); i <= std::max(from, to); i++) {
    selected_cells.push_back(cells[i]);
  }
  return BatchFormat(selected_cells);
}

BatchFormat Column::format(std::initializer_list<std::tuple<size_t, size_t>> ranges)
{
  std::vector<std::shared_ptr<Cell>> selected_cells;
  for (auto range : ranges) {
    for (size_t i = std::min(std::get<0>(range), std::get<1>(range)); i < std::max(std::get<0>(range), std::get<1>(range)); i++) {
      selected_cells.push_back(cells[i]);
    }
  }
  return BatchFormat(selected_cells);
}

// Iterator methods
Column::iterator Column::begin()
{
  return iterator(cells.begin());
}

Column::iterator Column::end()
{
  return iterator(cells.end());
}

Column::const_iterator Column::begin() const
{
  return const_iterator(cells.cbegin());
}

Column::const_iterator Column::end() const
{
  return const_iterator(cells.cend());
}
} // namespace tabulate

namespace tabulate::xterm
{
bool has_truecolor()
{
  const char *term = getenv("TERM");
  if (term == NULL) {
    term = "";
  };
  static const std::vector<std::string> terms_supported_truecolor = {"iterm", "linux", "xterm-truecolor", "xterm-256color"};

  return std::find(terms_supported_truecolor.begin(), terms_supported_truecolor.end(), term) != terms_supported_truecolor.end();
}

const bool supported_truecolor = has_truecolor();

std::string stringformatter(const std::string &str, TrueColor foreground_color, TrueColor background_color, const Styles &styles)
{
  std::string applied;

  bool have = !foreground_color.none() || !background_color.none() || styles.size() != 0;

  if (have) {
    auto rgb = [](TrueColor color) -> std::string {
      auto v = color.RGB();
      return std::to_string(std::get<0>(v)) + ":" + std::to_string(std::get<1>(v)) + ":" + std::to_string(std::get<2>(v));
    };

    auto style_code = [](Style style) -> unsigned int {
      // @RESERVED
      unsigned int i = to_underlying(style);
      switch (i) {
        case 0 ... 9: // 0 - 9
          return i;
        case 10 ... 18: // 21 - 29
          return i + 11;
        default:
          return 0; // none, reset
      }
    };

    if (!supported_truecolor) {
      applied += std::string("\033[");
      applied += std::to_string(to_underlying(TrueColor::most_similar(foreground_color.color)) + 30) + ";";
      applied += std::to_string(to_underlying(TrueColor::most_similar(background_color.color)) + 40) + ";";

      if (styles.size() > 0) {
        for (auto const &style : styles) {
          // style: 0 - 9, 21 - 29
          applied += std::to_string(style_code(style)) + ";";
        }
      }
      applied[applied.size() - 1] = 'm';
    } else {
      if (!foreground_color.none()) {
        // TrueColor: CSI 38 : 2 : r : g : b m
        applied += std::string("\033[38:2:") + rgb(foreground_color) + "m";
      }

      if (!background_color.none()) {
        // CSI 48 : 2 : r : g : b m
        applied += std::string("\033[48:2:") + rgb(background_color) + "m";
      }

      if (styles.size() > 0) {
        applied += "\033[";
        for (auto const &style : styles) {
          // style: 0 - 9, 21 - 29
          applied += std::to_string(style_code(style)) + ";";
        }
        applied[applied.size() - 1] = 'm';
      }
    }
  }

  applied += str;

  if (have) {
    applied += "\033[00m";
  }

  return applied;
}

std::string borderformatter(Which which, const Cell *self, const Cell *left, const Cell *right, const Cell *top, const Cell *bottom, size_t expected_size,
                            StringFormatter stringformatter)
{
#define TRY_GET(pattern, which, which_reverse)                                                                                                    \
  if (self->format().pattern.which.visiable) {                                                                                                    \
    auto it = self->format().pattern.which;                                                                                                       \
    return stringformatter(expand_to_size(it.content, expected_size, self->format().multi_bytes_character()), it.color, it.background_color, {}); \
  } else if (which && which->format().pattern.which_reverse.visiable) {                                                                           \
    auto it = which->format().pattern.which_reverse;                                                                                              \
    return stringformatter(expand_to_size(it.content, expected_size, self->format().multi_bytes_character()), it.color, it.background_color, {}); \
  }
  if (which == Which::top) {
    TRY_GET(borders, top, bottom);
  } else if (which == Which::bottom) {
    TRY_GET(borders, bottom, top);
  } else if (which == Which::left) {
    TRY_GET(borders, left, right);
  } else if (which == Which::right) {
    TRY_GET(borders, right, left);
  } else if (which == Which::cross) {
    // Cross junction with all four directions
    TRY_GET(borders, left, right); // Default to using left-right direction
  } else if (which == Which::bottom_middle) {
    // T-shape junction pointing north (┴)
    TRY_GET(borders, top, bottom);
  } else if (which == Which::top_middle) {
    // T-shape junction pointing south (┬)
    TRY_GET(borders, bottom, top);
  } else if (which == Which::middle_right) {
    // T-shape junction pointing west (┤)
    TRY_GET(borders, left, right);
  } else if (which == Which::middle_left) {
    // T-shape junction pointing east (├)
    TRY_GET(borders, right, left);
  }
#undef TRY_GET
  return "";
}

std::string cornerformatter(Which which, const Cell *self, const Cell *top_left, const Cell *top_right, const Cell *bottom_left, const Cell *bottom_right,
                            StringFormatter stringformatter)
{
#define TRY_GET(pattern, which, which_reverse)                             \
  if (self->format().pattern.which.visiable) {                             \
    auto it = self->format().pattern.which;                                \
    return stringformatter(it.content, it.color, it.background_color, {}); \
  } else if (which && which->format().pattern.which_reverse.visiable) {    \
    auto it = which->format().pattern.which_reverse;                       \
    return stringformatter(it.content, it.color, it.background_color, {}); \
  }

  // Junction corners - prioritize specialized corner members
  if (which == Which::cross) {
    // Cross junction with all four directions - use dedicated cross member
    if (self->format().corners.cross.visiable) {
      auto it = self->format().corners.cross;
      return stringformatter(it.content, it.color, it.background_color, {});
    }
  } else if (which == Which::bottom_middle) {
    // T-shape junction pointing north (┴) - use dedicated bottom_middle member
    if (self->format().corners.bottom_middle.visiable) {
      auto it = self->format().corners.bottom_middle;
      return stringformatter(it.content, it.color, it.background_color, {});
    }
  } else if (which == Which::top_middle) {
    // T-shape junction pointing south (┬) - use dedicated top_middle member
    if (self->format().corners.top_middle.visiable) {
      auto it = self->format().corners.top_middle;
      return stringformatter(it.content, it.color, it.background_color, {});
    }
  } else if (which == Which::middle_right) {
    // T-shape junction pointing west (┤) - use dedicated middle_right member
    if (self->format().corners.middle_right.visiable) {
      auto it = self->format().corners.middle_right;
      return stringformatter(it.content, it.color, it.background_color, {});
    }
  } else if (which == Which::middle_left) {
    // T-shape junction pointing east (├) - use dedicated middle_left member
    if (self->format().corners.middle_left.visiable) {
      auto it = self->format().corners.middle_left;
      return stringformatter(it.content, it.color, it.background_color, {});
    }
  }

  // Basic corners as fallback
  if (which == Which::top_left || which == Which::cross || which == Which::top_middle || which == Which::middle_left) {
    TRY_GET(corners, top_left, bottom_right);
  } else if (which == Which::top_right || which == Which::middle_right) {
    TRY_GET(corners, top_right, bottom_left);
  } else if (which == Which::bottom_left || which == Which::bottom_middle) {
    TRY_GET(corners, bottom_left, top_right);
  } else if (which == Which::bottom_right) {
    TRY_GET(corners, bottom_right, top_left);
  }

#undef TRY_GET

  // Default fallback characters when no specific junction is found
  if (which == Which::cross) {
    return stringformatter(symbols::cross, Color::none, Color::none, {});
  } else if (which == Which::bottom_middle) {
    return stringformatter(symbols::div_down, Color::none, Color::none, {});
  } else if (which == Which::top_middle) {
    return stringformatter(symbols::div_up, Color::none, Color::none, {});
  } else if (which == Which::middle_right) {
    return stringformatter(symbols::div_right, Color::none, Color::none, {});
  } else if (which == Which::middle_left) {
    return stringformatter(symbols::div_left, Color::none, Color::none, {});
  }

  return " ";
}
} // namespace tabulate::xterm

namespace tabulate
{
void Table::set_title(std::string title)
{
  this->title = std::move(title);
}

// Table class implementation
// Basic methods
BatchFormat Table::format()
{
  return BatchFormat(cells);
}

Row &Table::add()
{
  Row &row = __add_row();
  __on_add_auto_update();

  return row;
}

Row &Table::operator[](size_t index)
{
  if (index >= rows.size()) {
    size_t size = index + 1 - rows.size();
    for (size_t i = 0; i < size; i++) {
      add();
    }
  }
  return *rows[index];
}

size_t Table::size()
{
  return rows.size();
}

// Iterator methods
Table::iterator Table::begin()
{
  return iterator(rows.begin());
}

Table::iterator Table::end()
{
  return iterator(rows.end());
}

Table::const_iterator Table::begin() const
{
  return const_iterator(rows.cbegin());
}

Table::const_iterator Table::end() const
{
  return const_iterator(rows.cend());
}

// Column and layout methods
Column Table::column(size_t index)
{
  Column column;
  for (auto &row : rows) {
    if (row->size() <= index) {
      size_t size = index - row->size() + 1;
      for (size_t i = 0; i < size; i++) {
        row->add("");
      }
    }
    column.add(row->cell(index));
  }

  return column;
}

size_t Table::column_size() const
{
  size_t max_size = 0;
  for (auto const &row : rows) {
    max_size = std::max(max_size, row->size());
  }
  return max_size;
}

size_t Table::width() const
{
  return cached_width;
}

// Utility methods
int Table::merge(std::tuple<int, int> from, std::tuple<int, int> to)
{
  auto fx = std::get<0>(from), tx = std::get<0>(to);
  auto fy = std::get<1>(from), ty = std::get<1>(to);
  if (fx != tx && fy != ty) {
    merges.push_back(std::tuple<int, int, int, int>(fx, fy, tx, ty));
  }

  return 0;
}

// Output formatting methods
std::string Table::xterm(bool disable_color) const
{
  std::string exported;
  // add title
  if (!title.empty() && rows.size() > 0) {
    exported += std::string((__width() - title.size()) / 2, ' ') + title + NEWLINE;
  }

  auto stringformatter = [=](const std::string &str, TrueColor foreground_color, TrueColor background_color, const Styles &styles) -> std::string {
    if (disable_color) {
      return str;
    } else {
      return tabulate::xterm::stringformatter(str, foreground_color, background_color, styles);
    }
  };

  // Determine number of header rows (typically 1)
  size_t header_count = 1;
  size_t total_rows = rows.size();

  // add header
  if (rows.size() > 0) {
    const auto &header = *rows[0];
    // Pass row_index=0, header_count, and total_rows instead of boolean flags
    auto lines = header.dump(stringformatter, tabulate::xterm::borderformatter, tabulate::xterm::cornerformatter, 0, header_count, total_rows);
    for (auto line : lines) {
      exported += line + NEWLINE;
    }
  }

  // add table content
  for (size_t i = 1; i < rows.size(); i++) {
    auto const &row = *rows[i];
    // Pass row_index=i, header_count, and total_rows instead of is_middle flag
    auto lines = row.dump(stringformatter, tabulate::xterm::borderformatter, tabulate::xterm::cornerformatter, i, header_count, total_rows);
    for (auto line : lines) {
      exported += line + NEWLINE;
    }
  }
  if (exported.size() >= NEWLINE.size()) {
    exported.erase(exported.size() - NEWLINE.size(), NEWLINE.size()); // pop last NEWLINE
  }

  return exported;
}

std::string Table::xterm(size_t maxlines, bool keep_row_in_one_page) const
{
  std::string exported;
  if (!title.empty() && rows.size() > 0) {
    size_t size = width();
    if (size > title.size()) {
      exported = std::string((size - title.size()) / 2, ' ') + title + NEWLINE;
    } else {
      auto lines = wrap_lines(title, size, "", true);
      for (auto line : lines) {
        exported += line + NEWLINE;
      }
    }
  }

  // Determine number of header rows (typically 1)
  size_t header_count = 1;
  size_t total_rows = rows.size();

  // add header
  size_t hlines = 0;
  std::string header;
  if (rows.size() > 0) {
    const auto &_header = *rows[0];
    // Pass row_index=0, header_count, and total_rows
    auto lines =
        _header.dump(tabulate::xterm::stringformatter, tabulate::xterm::borderformatter, tabulate::xterm::cornerformatter, 0, header_count, total_rows);
    for (auto const &line : lines) {
      hlines++;
      header += line + NEWLINE;
    }
  }
  if (maxlines <= hlines) { // maxlines too small
    return header + "===== <Inappropriate Max Lines for PageBreak> ====";
  }

  exported += header;
  size_t nlines = hlines;
  for (size_t i = 1; i < rows.size(); i++) {
    auto const &row = *rows[i];
    // Pass row_index=i, header_count, and total_rows
    auto lines = row.dump(tabulate::xterm::stringformatter, tabulate::xterm::borderformatter, tabulate::xterm::cornerformatter, i, header_count, total_rows);

    if (keep_row_in_one_page) {
      size_t rowlines = lines.size();
      if (hlines + rowlines > maxlines) {
        return exported + "===== <Inappropriate Max Lines for PageBreak> ====";
      }
      if (nlines + rowlines > maxlines) {
        if (exported.size() >= NEWLINE.size()) {
          exported.erase(exported.size() - NEWLINE.size(), NEWLINE.size()); // pop last NEWLINE
        }
        exported += "\x0c" + header;
        nlines = hlines;
      }
      nlines += rowlines;
      for (auto const &line : lines) {
        exported += line + NEWLINE;
      }
    } else {
      for (auto line : lines) {
        if (nlines >= maxlines) {
          if (exported.size() >= NEWLINE.size()) {
            exported.erase(exported.size() - NEWLINE.size(), NEWLINE.size()); // pop last NEWLINE
          }
          exported += "\x0c" + header;
          nlines = hlines;
        }
        nlines++;
        exported += line;
      }
    }
  }

  return exported;
}

std::string Table::markdown() const
{
  std::string exported;

  auto format_cell = [](Cell &cell) {
    std::string applied;

    auto styles = cell.format().styles();
    auto foreground_color = cell.format().color();
    auto background_color = cell.format().background_color();
    bool have = !foreground_color.none() || !background_color.none() || styles.size() != 0;

    if (have) {
      applied += "<span style=\"";

      if (!foreground_color.none()) {
        // color: <color>
        applied += "color:" + to_string(foreground_color) + ";";
      }

      if (!background_color.none()) {
        // color: <color>
        applied += "background-color:" + to_string(background_color) + ";";
      }

      for (auto const &style : styles) {
        switch (style) {
          case Style::bold:
            applied += "font-weight:bold;";
            break;
          case Style::italic:
            applied += "font-style:italic;";
            break;
          // text-decoration: none|underline|overline|line-through|blink
          case Style::crossed:
            applied += "text-decoration:line-through;";
            break;
          case Style::underline:
            applied += "text-decoration:underline;";
            break;
          case Style::blink:
            applied += "text-decoration:blink;";
            break;
          default:
            // unsupported, do nothing
            break;
        }
      }
      applied += "\">";
    }

    applied += [&]() {
      return replace_all(cell.get(), NEWLINE, "<br>");
    }();

    if (have) {
      applied += "</span>";
    }

    return applied;
  };

  for (size_t i = 0; i < rows.size(); i++) {
    exported += "| ";
    for (auto cell : *rows[i]) {
      exported += format_cell(cell) + " | ";
    }
    exported += NEWLINE;

    if (i == 0) {
      // add alignentment
      std::string alignment = "|";
      for (auto const &cell : *rows[0]) {
        switch (cell.align()) {
          case Align::left:
            alignment += " :--";
            break;
          case Align::right:
            alignment += " --:";
            break;
          case Align::center:
            alignment += " :-:";
            break;
          default:
            alignment += " ---";
            break;
        }
        alignment += " |";
      }
      exported += alignment + NEWLINE;
    }
  }
  if (exported.size() >= NEWLINE.size()) {
    exported.erase(exported.size() - NEWLINE.size(), NEWLINE.size()); // pop last NEWLINE
  }

  return exported;
}

std::string Table::latex(size_t indentation) const
{
  std::string exported = "\\begin{table}[ht]" + NEWLINE;
  if (!title.empty()) {
    exported += "\\caption{" + title + "}" + NEWLINE;
    exported += "\\centering" + NEWLINE; // used for centering table
  }
  exported += "\\begin{tabular}";

  // add alignment header
  {
    exported += "{";
    for (auto &cell : *rows[0]) {
      if (cell.align() & Align::left) {
        exported += 'l';
      } else if (cell.align() & Align::hcenter) {
        exported += 'c';
      } else if (cell.align() & Align::right) {
        exported += 'r';
      }
    }
    exported += "}" + NEWLINE;
  }
  exported += "\\hline\\hline" + NEWLINE; // %inserts double horizontal lines

  // iterate content and put text into the table.
  for (size_t i = 0; i < rows.size(); i++) {
    auto &row = *rows[i];
    // apply row content indentation
    if (indentation != 0) {
      exported += std::string(indentation, ' ');
    }

    for (size_t j = 0; j < row.size(); j++) {
      auto cell = row[j];
      auto transfer = [](Cell &cell) {
        std::string tmp = replace_all(cell.get(), "#", "\\#");
        if (!(cell.format().background_color().none())) {
          tmp += "\\cellcolor[HTML]{" + to_string(cell.format().background_color()) + "} ";
        }
        return tmp;
      };
      exported += transfer(cell);
      exported += (j < row.size() - 1) ? " & " : " \\\\";
    }
    exported += NEWLINE;
    if (i == 0) {
      exported += "\\hline" + NEWLINE;
    }
  }
  exported += "\\hline" + NEWLINE;
  exported += "\\end{tabular}" + NEWLINE;
  exported += "\\end{table}";

  return exported;
}

// Private helper methods
Row &Table::__add_row()
{
  auto row = std::make_shared<Row>();
  rows.push_back(row);
  return *row;
}

void Table::__on_add_auto_update()
{
  // auto update width
  size_t headerwidth = 0;
  for (size_t i = 0; i < column_size(); i++) {
    Column col = column(i);

    size_t oldwidth = col.size() == 0 ? 0 : col[0].width();
    size_t newwidth = col[col.size() - 1].width();

    if (newwidth != oldwidth) {
      if (newwidth > oldwidth) {
        headerwidth += newwidth;
        col.format().width(newwidth);
      } else {
        headerwidth += oldwidth;
        col[col.size() - 1].format().width(oldwidth);
      }
    } else {
      headerwidth += oldwidth;
    }
  }
  cached_width = headerwidth;

  // append new cells
  Row &last_row = *rows[rows.size() - 1];
  for (size_t i = 0; i < last_row.size(); i++) {
    cells.push_back(last_row.cell(i));
  }
}

size_t Table::__width() const
{
  size_t size = 0;
  for (auto const &cell : *rows[0]) {
    auto &format = cell.format();
    if (format.borders.left.visiable) {
      size += display_width_of(format.borders.left.content, format.locale(), format.multi_bytes_character());
    }
    size += format.borders.left.padding + cell.width() + format.borders.right.padding;
    if (format.borders.right.visiable) {
      size += display_width_of(format.borders.right.content, format.locale(), format.multi_bytes_character());
    }
  }
  return size;
}
} // namespace tabulate

#undef BYTEn
