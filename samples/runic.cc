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

#include "tabulate.h"
using namespace tabulate;

int main()
{
  Table table;

  /*
      This is a story of a bear and
      a wolf, who wandered the
      realms nine to fulfill a promise
      to one before; they walk the
      twilight path, destined to
      discower the truth
      that is to come.
  */
  table.add("ᛏᚺᛁᛊ ᛁᛊ ᚨ ᛊᛏᛟᚱy ᛟᚠᚨ ᛒᛖᚨᚱ ᚨᚾᛞ\n"
            "ᚨ ᚹᛟᛚᚠ, ᚹᚺᛟ ᚹᚨᚾᛞᛖᚱᛖᛞ ᛏᚺᛖ\n"
            "ᚱᛖᚨᛚᛗᛊ ᚾᛁᚾᛖ ᛏᛟ ᚠᚢᛚᚠᛁᛚᛚ ᚨ ᛈᚱᛟᛗᛁᛊᛖ\n"
            "ᛏᛟ ᛟᚾᛖ ᛒᛖᚠᛟᚱᛖ; ᛏᚺᛖy ᚹᚨᛚᚲ ᛏᚺᛖ\n"
            "ᛏᚹᛁᛚᛁᚷᚺᛏ ᛈᚨᛏᚺ, ᛞᛖᛊᛏᛁᚾᛖᛞ ᛏᛟ\n"
            "ᛞᛁᛊcᛟᚹᛖᚱ ᛏᚺᛖ ᛏᚱᚢᛏᚺ\nᛏᚺᚨᛏ ᛁᛊ ᛏᛟ cᛟᛗᛖ.");

  table[0][0]
      .format()
      .multi_bytes_character(true)
      // Font styling
      .styles(Style::bold, Style::faint)
      .align(Align::center)
      .color(Color::red)
      .background_color(Color::yellow)
      // Corners
      .corner_top_left("ᛰ")
      .corner_top_right("ᛯ")
      .corner_bottom_left("ᛮ")
      .corner_bottom_right("ᛸ")
      .corner_top_left_color(Color::cyan)
      .corner_top_right_color(Color::yellow)
      .corner_bottom_left_color(Color::green)
      .corner_bottom_right_color(Color::red)
      // Borders
      .border_top("ᛜ")
      .border_bottom("ᛜ")
      .border_left("ᚿ")
      .border_right("ᛆ")
      .border_left_color(Color::yellow)
      .border_right_color(Color::green)
      .border_top_color(Color::cyan)
      .border_bottom_color(Color::red);

  std::cout << table.xterm() << std::endl;
  // std::cout << "Markdown Table:\n" << table.markdown() << std::endl;
}
