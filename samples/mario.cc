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
  Table mario;
  size_t rows = 16;
  for (size_t i = 0; i < rows; ++i) {
    mario.add("█", "█", "█", "█", "█", "█", "█", "█", "█", "█", "█", "█", "█", "█", "█", "█", "█", "█", "█", "█", "█", "█", "█", "█", "█", "█", "█", "█", "█",
              "█");
  }
  mario.format().color(Color::white).border("").corner("").border_padding(0).hide_border();

  // Row 0
  for (size_t i = 7; i < 20; ++i) {
    mario[0][i].format().color(Color::red);
  }
  // Row 1
  for (size_t i = 5; i < 26; ++i) {
    mario[1][i].format().color(Color::red);
  }
  // Row 2
  for (size_t i = 5; i < 13; ++i) {
    mario[2][i].format().color(Color::green).styles(Style::faint);
  }
  for (size_t i = 13; i < 18; ++i) {
    mario[2][i].format().color(Color::yellow);
  }
  for (size_t i = 18; i < 20; ++i) {
    mario[2][i].format().color(Color::green).styles(Style::faint);
  }
  for (size_t i = 20; i < 22; ++i) {
    mario[2][i].format().color(Color::yellow);
  }
  // Row 3
  for (size_t i = 3; i < 7; ++i) {
    mario[3][i].format().color(Color::green).styles(Style::faint);
  }
  for (size_t i = 7; i < 9; ++i) {
    mario[3][i].format().color(Color::yellow);
  }
  for (size_t i = 9; i < 11; ++i) {
    mario[3][i].format().color(Color::green).styles(Style::faint);
  }
  for (size_t i = 11; i < 18; ++i) {
    mario[3][i].format().color(Color::yellow);
  }
  for (size_t i = 18; i < 20; ++i) {
    mario[3][i].format().color(Color::green).styles(Style::faint);
  }
  for (size_t i = 20; i < 26; ++i) {
    mario[3][i].format().color(Color::yellow);
  }
  // Row 4
  for (size_t i = 3; i < 7; ++i) {
    mario[4][i].format().color(Color::green).styles(Style::faint);
  }
  for (size_t i = 7; i < 9; ++i) {
    mario[4][i].format().color(Color::yellow);
  }
  for (size_t i = 9; i < 13; ++i) {
    mario[4][i].format().color(Color::green).styles(Style::faint);
  }
  for (size_t i = 13; i < 20; ++i) {
    mario[4][i].format().color(Color::yellow);
  }
  for (size_t i = 20; i < 22; ++i) {
    mario[4][i].format().color(Color::green).styles(Style::faint);
  }
  for (size_t i = 22; i < 28; ++i) {
    mario[4][i].format().color(Color::yellow);
  }
  // Row 5
  for (size_t i = 3; i < 9; ++i) {
    mario[5][i].format().color(Color::green).styles(Style::faint);
  }
  for (size_t i = 9; i < 18; ++i) {
    mario[5][i].format().color(Color::yellow);
  }
  for (size_t i = 18; i < 26; ++i) {
    mario[5][i].format().color(Color::green).styles(Style::faint);
  }
  // Row 6
  for (size_t i = 7; i < 24; ++i) {
    mario[6][i].format().color(Color::yellow);
  }
  // Row 7
  for (size_t i = 5; i < 11; ++i) {
    mario[7][i].format().color(Color::green).styles(Style::faint);
  }
  for (size_t i = 11; i < 13; ++i) {
    mario[7][i].format().color(Color::red);
  }
  for (size_t i = 13; i < 20; ++i) {
    mario[7][i].format().color(Color::green).styles(Style::faint);
  }
  // Row 8
  for (size_t i = 3; i < 11; ++i) {
    mario[8][i].format().color(Color::green).styles(Style::faint);
  }
  for (size_t i = 11; i < 13; ++i) {
    mario[8][i].format().color(Color::red);
  }
  for (size_t i = 13; i < 18; ++i) {
    mario[8][i].format().color(Color::green).styles(Style::faint);
  }
  for (size_t i = 18; i < 20; ++i) {
    mario[8][i].format().color(Color::red);
  }
  for (size_t i = 20; i < 26; ++i) {
    mario[8][i].format().color(Color::green).styles(Style::faint);
  }
  // Row 9
  for (size_t i = 1; i < 11; ++i) {
    mario[9][i].format().color(Color::green).styles(Style::faint);
  }
  for (size_t i = 11; i < 20; ++i) {
    mario[9][i].format().color(Color::red);
  }
  for (size_t i = 20; i < 29; ++i) {
    mario[9][i].format().color(Color::green).styles(Style::faint);
  }
  // Row 10
  for (size_t i = 1; i < 7; ++i) {
    mario[10][i].format().color(Color::yellow);
  }
  for (size_t i = 7; i < 9; ++i) {
    mario[10][i].format().color(Color::green).styles(Style::faint);
  }
  for (size_t i = 9; i < 11; ++i) {
    mario[10][i].format().color(Color::red);
  }
  for (size_t i = 11; i < 13; ++i) {
    mario[10][i].format().color(Color::yellow);
  }
  for (size_t i = 13; i < 18; ++i) {
    mario[10][i].format().color(Color::red);
  }
  for (size_t i = 18; i < 20; ++i) {
    mario[10][i].format().color(Color::yellow);
  }
  for (size_t i = 20; i < 22; ++i) {
    mario[10][i].format().color(Color::red);
  }
  for (size_t i = 22; i < 24; ++i) {
    mario[10][i].format().color(Color::green).styles(Style::faint);
  }
  for (size_t i = 24; i < 29; ++i) {
    mario[10][i].format().color(Color::yellow);
  }
  // Row 11
  for (size_t i = 1; i < 9; ++i) {
    mario[11][i].format().color(Color::yellow);
  }
  for (size_t i = 9; i < 22; ++i) {
    mario[11][i].format().color(Color::red);
  }
  for (size_t i = 22; i < 29; ++i) {
    mario[11][i].format().color(Color::yellow);
  }
  // Row 12
  for (size_t i = 1; i < 7; ++i) {
    mario[12][i].format().color(Color::yellow);
  }
  for (size_t i = 24; i < 29; ++i) {
    mario[12][i].format().color(Color::yellow);
  }
  for (size_t i = 7; i < 24; ++i) {
    mario[12][i].format().color(Color::red);
  }
  // Row 13
  for (size_t i = 5; i < 14; ++i) {
    mario[13][i].format().color(Color::red);
  }
  for (size_t i = 16; i < 24; ++i) {
    mario[13][i].format().color(Color::red);
  }
  // Row 14
  for (size_t i = 3; i < 12; ++i) {
    mario[14][i].format().color(Color::green).styles(Style::faint);
  }
  for (size_t i = 18; i < 26; ++i) {
    mario[14][i].format().color(Color::green).styles(Style::faint);
  }
  // Row 15
  for (size_t i = 1; i < 12; ++i) {
    mario[15][i].format().color(Color::green).styles(Style::faint);
  }
  for (size_t i = 18; i < 29; ++i) {
    mario[15][i].format().color(Color::green).styles(Style::faint);
  }
  mario.format().multi_bytes_character(true);

  std::cout << mario.xterm() << "\n";
}
