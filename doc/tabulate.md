# Markdown

# tabulate

[![license](https://img.shields.io/badge/license-Apache-brightgreen.svg?style=flat)](https://github.com/vxfury/devkit/blob/master/LICENSE)
[![CI Status](https://github.com/vxfury/devkit/workflows/ci/badge.svg)](https://github.com/vxfury/devkit/actions)
[![codecov](https://codecov.io/gh/vxfury/devkit/branch/master/graph/badge.svg?token=BtMOpzpU1t)](https://codecov.io/gh/vxfury/devkit)
![GitHub release (latest by date)](https://img.shields.io/github/v/release/vxfury/devkit?color=red&label=release)
[![PRs Welcome](https://img.shields.io/badge/PRs-welcome-brightgreen.svg)](https://github.com/vxfury/devkit/pulls)

![logo](images/logo.jpg)

![summary](images/summary.png)
Source for the above image can be found [here](https://github.com/vxfury/devkit/blob/master/samples/summary.cc)

## Table of Contents

- [Markdown](#markdown)
- [tabulate](#tabulate)
  - [Table of Contents](#table-of-contents)
  - [Quick Start](#quick-start)
  - [Formatting Options](#formatting-options)
    - [Word Wrapping](#word-wrapping)
    - [Font Alignment](#font-alignment)
    - [Font Styles](#font-styles)
    - [Cell Colors](#cell-colors)
    - [Borders and Corners](#borders-and-corners)
    - [Range-based Iteration](#range-based-iteration)
    - [Nested Tables](#nested-tables)
    - [UTF-8 Support](#utf-8-support)
  - [Exporters](#exporters)
    - [Markdown](#markdown-1)
  - [Building Samples](#building-samples)
  - [Contributing](#contributing)
  - [License](#license)
  - [Colors](#colors)
  - [Styles](#styles)
  - [Appendix](#appendix)
    - [Colors Table](#colors-table)

## Quick Start

`tabulate` is a header-only library. Just add `include/` to your `include_directories` and you should be good to go. A single header file version is also available in `single_include/`.

**NOTE** Tabulate supports `>=C++11`. The rest of this README, however, assumes `C++17` support.

Create a `Table` object and call `Table.add_rows` to add rows to your table.

```cpp
#include "tabulate.h"

using namespace tabulate;

int main()
{
    Table universal_constants;

    universal_constants.add("Quantity", "Value");
    universal_constants.add("Characteristic impedance of vacuum", "376.730 313 461... Ω");
    universal_constants.add("Electric constant (permittivity of free space)", "8.854 187 817... × 10⁻¹²F·m⁻¹");
    universal_constants.add("Magnetic constant (permeability of free space)",
                            "4π × 10⁻⁷ N·A⁻² = 1.2566 370 614... × 10⁻⁶ N·A⁻²");
    universal_constants.add("Gravitational constant (Newtonian constant of gravitation)",
                            "6.6742(10) × 10⁻¹¹m³·kg⁻¹·s⁻²");
    universal_constants.add("Planck's constant", "6.626 0693(11) × 10⁻³⁴ J·s");
    universal_constants.add("Dirac's constant", "1.054 571 68(18) × 10⁻³⁴ J·s");
    universal_constants.add("Speed of light in vacuum", "299 792 458 m·s⁻¹");
```

You can format this table using `Table.format()` which returns a `Format` object. Using a fluent interface, format properties of the table, e.g., borders, font styles, colors etc.

```cpp
universal_constants.format()
        .styles(Style::bold)
        .border_top(" ")
        .border_bottom(" ")
        .border_left(" ")
        .border_right(" ")
        .corner(" ");
```

You can access rows in the table using `Table[row_index]`. This will return a `Row` object on which you can similarly call `Row.format()` to format properties of all the cells in that row.

Now, let's format the header of the table. The following code changes the font background of the header row to `red`, aligns the cell contents to `center` and applies a padding to the top and bottom of the row.

```cpp
    universal_constants[0]
        .format()
        .border_top_padding(1)
        .border_bottom_padding(1)
        .align(Align::center)
        .styles(Style::underline)
        .background_color(Color::red);
```

Calling `Table.column(index)` will return a `Column` object. Similar to rows, you can use `Column.format()` to format all the cells in that column.

Now, let's change the font color of the second column to yellow:

```cpp
    universal_constants.column(1).format().color(Color::yellow);
}
```

You can access cells by indexing twice from a table: From a row using `Table[row_index][col_index]` or from a column using `Table.column(col_index)[cell_index]`. Just like rows, columns, and tables, you can use `Cell.format()` to format individual cells

```cpp
    universal_constants[0][1].format().background_color(Color::blue).color(Color::white);

    std::cout << universal_constants.xterm() << std::endl;
}
```

![universal_constants](images/universal_constants.png)

## Formatting Options

### Word Wrapping

`tabulate` supports automatic word-wrapping when printing cells.

Although word-wrapping is automatic, there is a simple override. Automatic word-wrapping is used only if the cell contents do not have any embedded newline `\n` characters. So, you can embed newline characters in the cell contents and enforce the word-wrapping manually.

```cpp
#include "tabulate.h"
using namespace tabulate;

int main()
{
    Table table;

    table.add("This paragraph contains a veryveryveryveryveryverylong word. The long word will "
              "break and word wrap to the next line.",
              "This paragraph \nhas embedded '\\n' \ncharacters and\n will break\n exactly "
              "where\n you want it\n to\n break.");

    table[0][0].format().width(20);
    table[0][1].format().width(50);

    std::cout << table.xterm() << std::endl;
}
```

*  The above table has 1 row and 2 columns.
*  The first cell has automatic word-wrapping.
*  The second cell uses the embedded newline characters in the cell contents - even though the second column has plenty of space (50 characters width), it uses user-provided newline characters to break into new lines and enforce the cell style.
*  **NOTE**: Whether word-wrapping is automatic or not, `tabulate` performs a trim operation on each line of each cell to remove whitespace characters from either side of line.

![word_wrap](images/word_wrap.png)

**NOTE**: Both columns in the above table are left-aligned by default. This, however, can be easily changed.

### Font Alignment

`tabulate` supports three font alignment settings: `left`, `center`, and `right`. By default, all table content is left-aligned. To align cells, use `.format().align(alignment)`.

```cpp
#include "tabulate.h"
using namespace tabulate;

int main()
{
    tabulate::Table movies;

    movies.add("S/N", "Movie Name", "Director", "Estimated Budget", "Release Date");
    movies.add("tt1979376", "Toy Story 4", "Josh Cooley", 200000000, "21 June 2019");
    movies.add("tt3263904", "Sully", "Clint Eastwood", 60000000, "9 September 2016");
    movies.add("tt1535109", "Captain Phillips", "Paul Greengrass", 55000000, " 11 October 2013");

    // center align 'Director' column
    movies.column(2).format().align(Align::center);

    // right align 'Estimated Budget' column
    movies.column(3).format().align(Align::right);

    // right align 'Release Date' column
    movies.column(4).format().align(Align::right);

    // Color header cells
    for (size_t i = 0; i < movies.column_size(); ++i) {
        movies[0][i].format().color(Color::yellow).styles(Style::bold);
    }

    std::cout << movies.xterm() << std::endl;
    std::cout << "Markdown Table:\n" << movies.markdown() << std::endl;
}
```

![movies](images/movies.png)

### Font Styles

`tabulate` supports 8 font styles: `bold`, `dark`, `italic`, `underline`, `blink`, `reverse`, `concealed`, `crossed`. Depending on the terminal (or terminal settings), some of these might not work.

To apply a font style, simply call `.format().font_style({...})`. The `font_style` method takes a vector of font styles. This allows to apply multiple font styles to a cell, e.g., ***bold and italic***.

```cpp
#include "tabulate.h"

using namespace tabulate;

int main()
{
    Table table;
    table.add("Bold", "Italic", "Bold & Italic", "Blinking");
    table.add("Underline", "Crossed", "faint", "Bold, Italic & Underlined");
    table.add("Doubly Underline", "Invisable", "", "");

    table[0][0].format().styles(Style::bold);

    table[0][1].format().styles(Style::italic);

    table[0][2].format().styles(Style::bold, Style::italic);

    table[0][3].format().styles(Style::blink);

    table[1][0].format().styles(Style::underline);

    table[1][1].format().styles(Style::crossed);

    table[1][2].format().styles(Style::faint);

    table[1][3].format().styles(Style::bold, Style::italic, Style::underline);

    table[2][0].format().styles(Style::doubly_underline);
    table[2][1].format().styles(Style::invisible);

    std::cout << table.xterm() << std::endl;
    // std::cout << "Markdown Table:\n" << table.markdown() << std::endl;
}
```

![styles](images/styles.png)

**NOTE**: Font styles are applied to the entire cell. Unlike HTML, you cannot currently apply styles to specific words in a cell.

### Cell Colors

There are a number of methods in the `Format` object to color cells - foreground and background - for font, borders, corners, and column separators. `tabulate` supports 8 colors: `grey`, `red`, `green`, `yellow`, `blue`, `magenta`, `cyan`, and `white`, and support **True Color** if possiable. The look of these colors vary depending on your terminal.

For font, border, and corners, you can call `.format().<element>_color(value)` to set the foreground color and `.format().<element>_background_color(value)` to set the background color. Here's an example:

```cpp
#include "tabulate.h"

using namespace tabulate;

int main()
{
    Table colors;

    colors.add("Font Color is Red", "Font Color is Blue", "Font Color is Green");
    colors.add("Everything is Red", "Everything is Blue", "Everything is Green");
    colors.add("Font Background is Red", "Font Background is Blue", "Font Background is Green");

    colors[0][0].format().color(Color::red).styles(Style::bold);
    colors[0][1].format().color(Color::blue).styles(Style::bold);
    colors[0][2].format().color(Color::green).styles(Style::bold);

    colors[1][0]
        .format()
        .border_left_color(Color::red)
        .border_left_background_color(Color::red)
        .background_color(Color::red)
        .color(Color::red)
        .border_right_color(Color::blue)
        .border_right_background_color(Color::blue);

    colors[1][1]
        .format()
        .background_color(Color::blue)
        .color(Color::blue)
        .border_right_color(Color::green)
        .border_right_background_color(Color::green);

    colors[1][2]
        .format()
        .background_color(Color::green)
        .color(Color::green)
        .border_right_color(Color::green)
        .border_right_background_color(Color::green);

    colors[2][0].format().background_color(Color::red).styles(Style::bold);
    colors[2][1].format().background_color(Color::blue).styles(Style::bold);
    colors[2][2].format().background_color(Color::green).styles(Style::bold);

    std::cout << colors.xterm() << std::endl;
    // std::cout << "Markdown Table:\n" << colors.markdown() << std::endl;
}
```

![colors](images/colors.png)

### Borders and Corners

`tabulate` allows for fine control over borders and corners. For each border and corner, you can set the text, color, and background color.

**NOTE**: You can use `.corner(..)`, `.corner_color(..)`, and `.corner_background_color(..)` to set a common style for all corners. Similarly, you can use `.border(..)`, `.border_color(..)` and `.border_background_color(..)` to set a common style for all borders.

**NOTE**: Note the use of `.format().multi_bytes_character(true)`. Use this when you know your table has multi-byte characters. This is an opt-in because the calculation of column width when dealing with multi-byte characters is more involved and you don't want to pay the performance penalty unless you need it. Just like any other format setting, you can set this at the table-level, row-level, or on a per-cell basis.

Here's an example where each border and corner is individually styled:

```cpp
#include "tabulate.h"

using namespace tabulate;

int main()
{
    Table table;

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
}
```

![runic](images/runic.png)

### Range-based Iteration

Hand-picking and formatting cells using `operator[]` gets tedious very quickly. To ease this, `tabulate` supports range-based iteration on tables, rows, and columns. Quickly iterate over rows and columns to format cells.

```cpp
#include "tabulate.h"
using namespace tabulate;

int main()
{
    Table table;

    table.add("Company", "Contact", "Country");
    table.add("Alfreds Futterkiste", "Maria Anders", "Germany");
    table.add("Centro comercial Moctezuma", "Francisco Chang", "Mexico");
    table.add("Ernst Handel", "Roland Mendel", "Austria");
    table.add("Island Trading", "Helen Bennett", "UK");
    table.add("Laughing Bacchus Winecellars", "Yoshi Tannamuri", "Canada");
    table.add("Magazzini Alimentari Riuniti", "Giovanni Rovelli", "Italy");

    // Set width of cells in each column
    table.column(0).format().width(40);
    table.column(1).format().width(30);
    table.column(2).format().width(30);

    // Iterate over cells in the first row
    for (auto &cell : table[0]) { cell.format().styles(Style::underline).align(Align::center); }

    // Iterator over cells in the first column
    for (auto &cell : table.column(0)) {
        if (cell.get() != "Company") { cell.format().align(Align::right); }
    }

    // Iterate over rows in the table
    size_t index = 0;
    for (auto &row : table) {
        // row.format().styles(Style::bold);

        // Set blue background color for alternate rows
        if (index > 0 && index % 2 == 0) {
            for (auto &cell : row) { cell.format().background_color(Color::blue); }
        }
        index += 1;
    }

    std::cout << table.xterm() << std::endl;
    // std::cout << "Markdown Table:\n" << table.markdown() << std::endl;
}
```

![iterators](images/iterators.png)

### Nested Tables

`Table.add_row(...)` takes either a `std::string` or a `tabulate::Table`. This can be used to nest tables within tables. Here's an example program that prints a UML class diagram using `tabulate`. Note the use of font alignment, style, and width settings to generate a diagram that looks centered and great.

```cpp
#include "tabulate.h"
using namespace tabulate;

int main()
{
    Table class_diagram;

    // Animal class
    {
        Table animal;
        animal.add("Animal");
        animal[0].format().align(Align::center);

        // Animal properties nested table
        {
            Table animal_properties;
            animal_properties.add("+age: Int");
            animal_properties.add("+gender: String");
            animal_properties.format().width(20);
            animal_properties[0].format().hide_border_bottom();

            animal.add(animal_properties);
        }

        // Animal methods nested table
        {
            Table animal_methods;
            animal_methods.add("+isMammal()");
            animal_methods.add("+mate()");
            animal_methods.format().width(20);
            animal_methods[0].format().hide_border_bottom();

            animal.add(animal_methods);
        }
        animal[1].format().hide_border_bottom();

        class_diagram.add(animal);
    }

    // Add rows in the class diagram for the up-facing arrow
    // THanks to center alignment, these will align just fine
    class_diagram.add("▲");
    class_diagram[1][0].format().hide_border_bottom().multi_bytes_character(true);
    class_diagram.add("|");
    class_diagram[2].format().hide_border_bottom();
    class_diagram.add("|");
    class_diagram[3].format().hide_border_bottom();

    // Duck class
    {
        Table duck;
        duck.add("Duck");

        // Duck proeperties nested table
        {
            Table duck_properties;
            duck_properties.add("+beakColor: String = \"yellow\"");
            duck_properties.format().width(40);

            duck.add(duck_properties);
        }

        // Duck methods nested table
        {
            Table duck_methods;
            duck_methods.add("+swim()");
            duck_methods.add("+quack()");
            duck_methods.format().width(40);
            duck_methods[0].format().hide_border_bottom();

            duck.add(duck_methods);
        }

        duck[0].format().align(Align::center);
        duck[1].format().hide_border_bottom();

        class_diagram.add(duck);
    }

    // Global styling
    class_diagram.format().styles(Style::bold).align(Align::center).width(60).hide_border();

    std::cout << class_diagram.xterm() << std::endl;
}
```

![class_diagram](images/class_diagram.png)

### UTF-8 Support

In \*nix, `wcswidth` is used to compute the display width of multi-byte characters. Column alignment works well when your system supports the necessary locale, e.g., I've noticed on MacOS 10 there is no Arabic locale (searched with `locale -a`) and this ends up causing alignment issues when using Arabic text, e.g., `"ٲنَا بحِبَّك (Ana bahebak)"` in tables.

The following table prints the phrase `I love you` in different languages. Note the use of `.format().multi_bytes_character(true)` for the second column. Remember to do this when dealing with multi-byte characters.

```cpp
#include "tabulate.h"
using namespace tabulate;

int main()
{
    Table table;

    table.add("English", "I love you");
    table.add("French", "Je t’aime");
    table.add("Spanish", "Te amo");
    table.add("German", "Ich liebe Dich");
    table.add("Mandarin Chinese", "我爱你");
    table.add("Japanese", "愛してる");
    table.add("Korean", "사랑해 (Saranghae)");
    table.add("Greek", "Σ΄αγαπώ (Se agapo)");
    table.add("Italian", "Ti amo");
    table.add("Russian", "Я тебя люблю (Ya tebya liubliu)");
    table.add("Hebrew", "אני אוהב אותך (Ani ohev otakh)");

    // Column 1 is using mult-byte characters
    table.column(1).format().multi_bytes_character(true);
    table.format().corner("♥").styles(Style::bold).corner_color(Color::magenta).border_color(Color::magenta);

    std::cout << table.xterm() << std::endl;
}
```

![unicode](images/unicode.png)

You can explicitly set the locale for a cell using `.format().locale(value)`. Note that the locale string is system-specific. So, the following code might throw `std::runtime_error locale::facet::_S_create_c_locale name not valid` on your system.

```cpp
    // Set English-US locale for first column
    table.column(0).format().locale("en_US.UTF-8");
    table[0][1].format().locale("en_US.UTF-8");

    // Set locale for individual cells
    table[1][1].format().locale("fr_FR.UTF-8");  // French
    table[2][1].format().locale("es_ES.UTF-8");  // Spanish
    table[3][1].format().locale("de_DE.UTF-8");  // German
    table[4][1].format().locale("zh_CN.UTF-8");  // Chinese
    table[5][1].format().locale("ja_JP.UTF-8");  // Japanese
    table[6][1].format().locale("ko_KR.UTF-8");  // Korean
    table[7][1].format().locale("el_GR.UTF-8");  // Greek
    table[8][1].format().locale("it_IT.UTF-8");  // Italian
    table[9][1].format().locale("ru_RU.UTF-8");  // Russian
    table[10][1].format().locale("he_IL.UTF-8"); // Hebrew
```

## Exporters

### Markdown

Tables can be exported to GitHub-flavored markdown using a `table.markdown()` to generate a Markdown-formatted `std::string`.

```cpp
#include "tabulate.h"
using namespace tabulate;

int main()
{
    tabulate::Table movies;

    movies.add("S/N", "Movie Name", "Director", "Estimated Budget", "Release Date");
    movies.add("tt1979376", "Toy Story 4", "Josh Cooley", 200000000, "21 June 2019");
    movies.add("tt3263904", "Sully", "Clint Eastwood", 60000000, "9 September 2016");
    movies.add("tt1535109", "Captain Phillips", "Paul Greengrass", 55000000, " 11 October 2013");

    // center align 'Director' column
    movies.column(2).format().align(Align::center);

    // right align 'Estimated Budget' column
    movies.column(3).format().align(Align::right);

    // right align 'Release Date' column
    movies.column(4).format().align(Align::right);

    // Color header cells
    for (size_t i = 0; i < movies.column_size(); ++i) {
        movies[0][i].format().color(Color::yellow).styles(Style::bold);
    }

    std::cout << movies.xterm() << std::endl;
    std::cout << "Markdown Table:\n" << movies.markdown() << std::endl;
}
```

![markdown_export](images/markdown_export.png)

The above table renders in Markdown like below.

**NOTE**: Unlike `tabulate`, you cannot align individual cells in Markdown. Alignment is on a per-column basis. Markdown allows a second header row where such column-wise alignment can be specified. The `markdown()` uses the formatting of the header cells in the original `tabulate::Table` to decide how to align each column. As per the Markdown spec, columns are left-aligned by default. **True Color** supported for markdown exporter.

| <span style="color:#ffff00;">**S/N**</span> | <span style="color:#ffff00;">**Movie Name**</span> | <span style="color:#ffff00;">**Director**</span> | <span style="color:#ffff00;">**Estimated Budget**</span> | <span style="color:#ffff00;">**Release Date**</span> |
| :------------------------------------------ | :------------------------------------------------- | :----------------------------------------------: | -------------------------------------------------------: | ---------------------------------------------------: |
| tt1979376                                   | Toy Story 4                                        |                   Josh Cooley                    |                                                200000000 |                                         21 June 2019 |
| tt3263904                                   | Sully                                              |                  Clint Eastwood                  |                                                 60000000 |                                     9 September 2016 |
| tt1535109                                   | Captain Phillips                                   |                 Paul Greengrass                  |                                                 55000000 |                                      11 October 2013 |

## Building Samples

There are a number of samples in the `samples/` directory. You can build these samples by running the following commands.

```bash
cmake -B build
cmake --build build
```

## Contributing
Contributions are welcome, have a look at the [CONTRIBUTING.md](CONTRIBUTING.md) document for more information.

## License
The project is available under the [Apache License 2.0](http://www.apache.org/licenses/LICENSE-2.0) license.


## Colors

- xterm
  - VT100: `\033[35;47m[magenta on white]\033[m`
  - Control Sequence
    | Code                     |  Effect   | Note                                       |
    | :----------------------- | :-------: | :----------------------------------------- |
    | CSI n m                  |    SGR    | ANSI color code (Select Graphic Rendition) |
    | CSI 38 ; 5 ; n m         | 256COLOR  | Foreground 256 color code                  |
    | ESC 48 ; 5 ; n m         | 256COLOR  | Background 256 color code                  |
    | CSI 38 ; 2 ; r ; g ; b m | TRUECOLOR | Foreground 24 bit rgb color code           |
    | ESC 48 ; 2 ; r ; g ; b m | TRUECOLOR | Background 24 bit rgb color code           |

- markdown
  - `<span style="color:magenta;background-color:white;">[magenta on white]</span>`
  - `<span style="color:#FF00FF;background-color:#FFFFFF;">[magenta on white]</span>`

## Styles

- xterm
  - https://tintin.mudhalla.net/info/xterm
  - https://invisible-island.net/xterm/ctlseqs/ctlseqs.html

- markdown
  - **bold**
    - __bold__
    - <strong>bold</strong>
  - faint
  - _italic_
    - *italic*
    - <i>italic</i>
    - <em>italic</em>
    - <span style="font-style:italic;">italic</span>
  - <u>underline</u>
    - <span style="text-decoration:underline;">underline</span>
  - blink
  - inverse
  - invisible
  - ~~crossed~~
    - <span style="text-decoration:line-through">crossed</span>


## Appendix

### Colors Table

| Name                 |                          HEX                          |                RGB |
| :------------------- | :---------------------------------------------------: | -----------------: |
| AliceBlue            | <span style="background-color:#F0F8FF">#F0F8FF</span> | RGB(240, 248, 255) |
| AntiqueWhite         | <span style="background-color:#FAEBD7">#FAEBD7</span> | RGB(250, 235, 215) |
| Aqua                 | <span style="background-color:#00FFFF">#00FFFF</span> |   RGB(0, 255, 255) |
| Aquamarine           | <span style="background-color:#7FFFD4">#7FFFD4</span> | RGB(127, 255, 212) |
| Azure                | <span style="background-color:#F0FFFF">#F0FFFF</span> | RGB(240, 255, 255) |
| Beige                | <span style="background-color:#F5F5DC">#F5F5DC</span> | RGB(245, 245, 220) |
| Bisque               | <span style="background-color:#FFE4C4">#FFE4C4</span> | RGB(255, 228, 196) |
| Black                | <span style="background-color:#000000">#000000</span> |       RGB(0, 0, 0) |
| BlanchedAlmond       | <span style="background-color:#FFEBCD">#FFEBCD</span> | RGB(255, 235, 205) |
| Blue                 | <span style="background-color:#0000FF">#0000FF</span> |     RGB(0, 0, 255) |
| BlueViolet           | <span style="background-color:#8A2BE2">#8A2BE2</span> |  RGB(138, 43, 226) |
| Brown                | <span style="background-color:#A52A2A">#A52A2A</span> |   RGB(165, 42, 42) |
| BurlyWood            | <span style="background-color:#DEB887">#DEB887</span> | RGB(222, 184, 135) |
| CadetBlue            | <span style="background-color:#5F9EA0">#5F9EA0</span> |  RGB(95, 158, 160) |
| Chartreuse           | <span style="background-color:#7FFF00">#7FFF00</span> |   RGB(127, 255, 0) |
| Chocolate            | <span style="background-color:#D2691E">#D2691E</span> |  RGB(210, 105, 30) |
| Coral                | <span style="background-color:#FF7F50">#FF7F50</span> |  RGB(255, 127, 80) |
| CornflowerBlue       | <span style="background-color:#6495ED">#6495ED</span> | RGB(100, 149, 237) |
| Cornsilk             | <span style="background-color:#FFF8DC">#FFF8DC</span> | RGB(255, 248, 220) |
| Crimson              | <span style="background-color:#DC143C">#DC143C</span> |   RGB(220, 20, 60) |
| Cyan                 | <span style="background-color:#00FFFF">#00FFFF</span> |   RGB(0, 255, 255) |
| DarkBlue             | <span style="background-color:#00008B">#00008B</span> |     RGB(0, 0, 139) |
| DarkCyan             | <span style="background-color:#008B8B">#008B8B</span> |   RGB(0, 139, 139) |
| DarkGoldenRod        | <span style="background-color:#B8860B">#B8860B</span> |  RGB(184, 134, 11) |
| DarkGray             | <span style="background-color:#A9A9A9">#A9A9A9</span> | RGB(169, 169, 169) |
| DarkGreen            | <span style="background-color:#006400">#006400</span> |     RGB(0, 100, 0) |
| DarkKhaki            | <span style="background-color:#BDB76B">#BDB76B</span> | RGB(189, 183, 107) |
| DarkMagenta          | <span style="background-color:#8B008B">#8B008B</span> |   RGB(139, 0, 139) |
| DarkOliveGreen       | <span style="background-color:#556B2F">#556B2F</span> |   RGB(85, 107, 47) |
| Darkorange           | <span style="background-color:#FF8C00">#FF8C00</span> |   RGB(255, 140, 0) |
| DarkOrchid           | <span style="background-color:#9932CC">#9932CC</span> |  RGB(153, 50, 204) |
| DarkRed              | <span style="background-color:#8B0000">#8B0000</span> |     RGB(139, 0, 0) |
| DarkSalmon           | <span style="background-color:#E9967A">#E9967A</span> | RGB(233, 150, 122) |
| DarkSeaGreen         | <span style="background-color:#8FBC8F">#8FBC8F</span> | RGB(143, 188, 143) |
| DarkSlateBlue        | <span style="background-color:#483D8B">#483D8B</span> |   RGB(72, 61, 139) |
| DarkSlateGray        | <span style="background-color:#2F4F4F">#2F4F4F</span> |    RGB(47, 79, 79) |
| DarkTurquoise        | <span style="background-color:#00CED1">#00CED1</span> |   RGB(0, 206, 209) |
| DarkViolet           | <span style="background-color:#9400D3">#9400D3</span> |   RGB(148, 0, 211) |
| DeepPink             | <span style="background-color:#FF1493">#FF1493</span> |  RGB(255, 20, 147) |
| DeepSkyBlue          | <span style="background-color:#00BFFF">#00BFFF</span> |   RGB(0, 191, 255) |
| DimGray              | <span style="background-color:#696969">#696969</span> | RGB(105, 105, 105) |
| DodgerBlue           | <span style="background-color:#1E90FF">#1E90FF</span> |  RGB(30, 144, 255) |
| Feldspar             | <span style="background-color:#D19275">#D19275</span> | RGB(209, 146, 117) |
| FireBrick            | <span style="background-color:#B22222">#B22222</span> |   RGB(178, 34, 34) |
| FloralWhite          | <span style="background-color:#FFFAF0">#FFFAF0</span> | RGB(255, 250, 240) |
| ForestGreen          | <span style="background-color:#228B22">#228B22</span> |   RGB(34, 139, 34) |
| Fuchsia              | <span style="background-color:#FF00FF">#FF00FF</span> |   RGB(255, 0, 255) |
| Gainsboro            | <span style="background-color:#DCDCDC">#DCDCDC</span> | RGB(220, 220, 220) |
| GhostWhite           | <span style="background-color:#F8F8FF">#F8F8FF</span> | RGB(248, 248, 255) |
| Gold                 | <span style="background-color:#FFD700">#FFD700</span> |   RGB(255, 215, 0) |
| GoldenRod            | <span style="background-color:#DAA520">#DAA520</span> |  RGB(218, 165, 32) |
| Gray                 | <span style="background-color:#808080">#808080</span> | RGB(128, 128, 128) |
| Green                | <span style="background-color:#008000">#008000</span> |     RGB(0, 128, 0) |
| GreenYellow          | <span style="background-color:#ADFF2F">#ADFF2F</span> |  RGB(173, 255, 47) |
| HoneyDew             | <span style="background-color:#F0FFF0">#F0FFF0</span> | RGB(240, 255, 240) |
| HotPink              | <span style="background-color:#FF69B4">#FF69B4</span> | RGB(255, 105, 180) |
| IndianRed            | <span style="background-color:#CD5C5C">#CD5C5C</span> |   RGB(205, 92, 92) |
| Indigo               | <span style="background-color:#4B0082">#4B0082</span> |    RGB(75, 0, 130) |
| Ivory                | <span style="background-color:#FFFFF0">#FFFFF0</span> | RGB(255, 255, 240) |
| Khaki                | <span style="background-color:#F0E68C">#F0E68C</span> | RGB(240, 230, 140) |
| Lavender             | <span style="background-color:#E6E6FA">#E6E6FA</span> | RGB(230, 230, 250) |
| LavenderBlush        | <span style="background-color:#FFF0F5">#FFF0F5</span> | RGB(255, 240, 245) |
| LawnGreen            | <span style="background-color:#7CFC00">#7CFC00</span> |   RGB(124, 252, 0) |
| LemonChiffon         | <span style="background-color:#FFFACD">#FFFACD</span> | RGB(255, 250, 205) |
| LightBlue            | <span style="background-color:#ADD8E6">#ADD8E6</span> | RGB(173, 216, 230) |
| LightCoral           | <span style="background-color:#F08080">#F08080</span> | RGB(240, 128, 128) |
| LightCyan            | <span style="background-color:#E0FFFF">#E0FFFF</span> | RGB(224, 255, 255) |
| LightGoldenRodYellow | <span style="background-color:#FAFAD2">#FAFAD2</span> | RGB(250, 250, 210) |
| LightGrey            | <span style="background-color:#D3D3D3">#D3D3D3</span> | RGB(211, 211, 211) |
| LightGreen           | <span style="background-color:#90EE90">#90EE90</span> | RGB(144, 238, 144) |
| LightPink            | <span style="background-color:#FFB6C1">#FFB6C1</span> | RGB(255, 182, 193) |
| LightSalmon          | <span style="background-color:#FFA07A">#FFA07A</span> | RGB(255, 160, 122) |
| LightSeaGreen        | <span style="background-color:#20B2AA">#20B2AA</span> |  RGB(32, 178, 170) |
| LightSkyBlue         | <span style="background-color:#87CEFA">#87CEFA</span> | RGB(135, 206, 250) |
| LightSlateBlue       | <span style="background-color:#8470FF">#8470FF</span> | RGB(132, 112, 255) |
| LightSlateGray       | <span style="background-color:#778899">#778899</span> | RGB(119, 136, 153) |
| LightSteelBlue       | <span style="background-color:#B0C4DE">#B0C4DE</span> | RGB(176, 196, 222) |
| LightYellow          | <span style="background-color:#FFFFE0">#FFFFE0</span> | RGB(255, 255, 224) |
| Lime                 | <span style="background-color:#00FF00">#00FF00</span> |     RGB(0, 255, 0) |
| LimeGreen            | <span style="background-color:#32CD32">#32CD32</span> |   RGB(50, 205, 50) |
| Linen                | <span style="background-color:#FAF0E6">#FAF0E6</span> | RGB(250, 240, 230) |
| Magenta              | <span style="background-color:#FF00FF">#FF00FF</span> |   RGB(255, 0, 255) |
| Maroon               | <span style="background-color:#800000">#800000</span> |     RGB(128, 0, 0) |
| MediumAquaMarine     | <span style="background-color:#66CDAA">#66CDAA</span> | RGB(102, 205, 170) |
| MediumBlue           | <span style="background-color:#0000CD">#0000CD</span> |     RGB(0, 0, 205) |
| MediumOrchid         | <span style="background-color:#BA55D3">#BA55D3</span> |  RGB(186, 85, 211) |
| MediumPurple         | <span style="background-color:#9370D8">#9370D8</span> | RGB(147, 112, 216) |
| MediumSeaGreen       | <span style="background-color:#3CB371">#3CB371</span> |  RGB(60, 179, 113) |
| MediumSlateBlue      | <span style="background-color:#7B68EE">#7B68EE</span> | RGB(123, 104, 238) |
| MediumSpringGreen    | <span style="background-color:#00FA9A">#00FA9A</span> |   RGB(0, 250, 154) |
| MediumTurquoise      | <span style="background-color:#48D1CC">#48D1CC</span> |  RGB(72, 209, 204) |
| MediumVioletRed      | <span style="background-color:#C71585">#C71585</span> |  RGB(199, 21, 133) |
| MidnightBlue         | <span style="background-color:#191970">#191970</span> |   RGB(25, 25, 112) |
| MintCream            | <span style="background-color:#F5FFFA">#F5FFFA</span> | RGB(245, 255, 250) |
| MistyRose            | <span style="background-color:#FFE4E1">#FFE4E1</span> | RGB(255, 228, 225) |
| Moccasin             | <span style="background-color:#FFE4B5">#FFE4B5</span> | RGB(255, 228, 181) |
| NavajoWhite          | <span style="background-color:#FFDEAD">#FFDEAD</span> | RGB(255, 222, 173) |
| Navy                 | <span style="background-color:#000080">#000080</span> |     RGB(0, 0, 128) |
| OldLace              | <span style="background-color:#FDF5E6">#FDF5E6</span> | RGB(253, 245, 230) |
| Olive                | <span style="background-color:#808000">#808000</span> |   RGB(128, 128, 0) |
| OliveDrab            | <span style="background-color:#6B8E23">#6B8E23</span> |  RGB(107, 142, 35) |
| Orange               | <span style="background-color:#FFA500">#FFA500</span> |   RGB(255, 165, 0) |
| OrangeRed            | <span style="background-color:#FF4500">#FF4500</span> |    RGB(255, 69, 0) |
| Orchid               | <span style="background-color:#DA70D6">#DA70D6</span> | RGB(218, 112, 214) |
| PaleGoldenRod        | <span style="background-color:#EEE8AA">#EEE8AA</span> | RGB(238, 232, 170) |
| PaleGreen            | <span style="background-color:#98FB98">#98FB98</span> | RGB(152, 251, 152) |
| PaleTurquoise        | <span style="background-color:#AFEEEE">#AFEEEE</span> | RGB(175, 238, 238) |
| PaleVioletRed        | <span style="background-color:#D87093">#D87093</span> | RGB(216, 112, 147) |
| PapayaWhip           | <span style="background-color:#FFEFD5">#FFEFD5</span> | RGB(255, 239, 213) |
| PeachPuff            | <span style="background-color:#FFDAB9">#FFDAB9</span> | RGB(255, 218, 185) |
| Peru                 | <span style="background-color:#CD853F">#CD853F</span> |  RGB(205, 133, 63) |
| Pink                 | <span style="background-color:#FFC0CB">#FFC0CB</span> | RGB(255, 192, 203) |
| Plum                 | <span style="background-color:#DDA0DD">#DDA0DD</span> | RGB(221, 160, 221) |
| PowderBlue           | <span style="background-color:#B0E0E6">#B0E0E6</span> | RGB(176, 224, 230) |
| Purple               | <span style="background-color:#800080">#800080</span> |   RGB(128, 0, 128) |
| Red                  | <span style="background-color:#FF0000">#FF0000</span> |     RGB(255, 0, 0) |
| RosyBrown            | <span style="background-color:#BC8F8F">#BC8F8F</span> | RGB(188, 143, 143) |
| RoyalBlue            | <span style="background-color:#4169E1">#4169E1</span> |  RGB(65, 105, 225) |
| SaddleBrown          | <span style="background-color:#8B4513">#8B4513</span> |   RGB(139, 69, 19) |
| Salmon               | <span style="background-color:#FA8072">#FA8072</span> | RGB(250, 128, 114) |
| SandyBrown           | <span style="background-color:#F4A460">#F4A460</span> |  RGB(244, 164, 96) |
| SeaGreen             | <span style="background-color:#2E8B57">#2E8B57</span> |   RGB(46, 139, 87) |
| SeaShell             | <span style="background-color:#FFF5EE">#FFF5EE</span> | RGB(255, 245, 238) |
| Sienna               | <span style="background-color:#A0522D">#A0522D</span> |   RGB(160, 82, 45) |
| Silver               | <span style="background-color:#C0C0C0">#C0C0C0</span> | RGB(192, 192, 192) |
| SkyBlue              | <span style="background-color:#87CEEB">#87CEEB</span> | RGB(135, 206, 235) |
| SlateBlue            | <span style="background-color:#6A5ACD">#6A5ACD</span> |  RGB(106, 90, 205) |
| SlateGray            | <span style="background-color:#708090">#708090</span> | RGB(112, 128, 144) |
| Snow                 | <span style="background-color:#FFFAFA">#FFFAFA</span> | RGB(255, 250, 250) |
| SpringGreen          | <span style="background-color:#00FF7F">#00FF7F</span> |   RGB(0, 255, 127) |
| SteelBlue            | <span style="background-color:#4682B4">#4682B4</span> |  RGB(70, 130, 180) |
| Tan                  | <span style="background-color:#D2B48C">#D2B48C</span> | RGB(210, 180, 140) |
| Teal                 | <span style="background-color:#008080">#008080</span> |   RGB(0, 128, 128) |
| Thistle              | <span style="background-color:#D8BFD8">#D8BFD8</span> | RGB(216, 191, 216) |
| Tomato               | <span style="background-color:#FF6347">#FF6347</span> |   RGB(255, 99, 71) |
| Turquoise            | <span style="background-color:#40E0D0">#40E0D0</span> |  RGB(64, 224, 208) |
| Violet               | <span style="background-color:#EE82EE">#EE82EE</span> | RGB(238, 130, 238) |
| VioletRed            | <span style="background-color:#D02090">#D02090</span> |  RGB(208, 32, 144) |
| Wheat                | <span style="background-color:#F5DEB3">#F5DEB3</span> | RGB(245, 222, 179) |
| White                | <span style="background-color:#FFFFFF">#FFFFFF</span> | RGB(255, 255, 255) |
| WhiteSmoke           | <span style="background-color:#F5F5F5">#F5F5F5</span> | RGB(245, 245, 245) |
| Yellow               | <span style="background-color:#FFFF00">#FFFF00</span> |   RGB(255, 255, 0) |
| YellowGreen          | <span style="background-color:#9ACD32">#9ACD32</span> |  RGB(154, 205, 50) |


## Symbols
| Name                                        | Main  | Fallback |
| ------------------------------------------- | :---: | :------: |
| tick                                        |  `✔`  |   `√`    |
| info                                        |  `ℹ`  |   `i`    |
| warning                                     |  `⚠`  |   `‼`    |
| cross                                       |  `✘`  |   `×`    |
| square                                      |  `█`  |          |
| squareSmall                                 |  `◻`  |   `□`    |
| squareSmallFilled                           |  `◼`  |   `■`    |
| squareDarkShade                             |  `▓`  |          |
| squareMediumShade                           |  `▒`  |          |
| squareLightShade                            |  `░`  |          |
| squareTop                                   |  `▀`  |          |
| squareBottom                                |  `▄`  |          |
| squareLeft                                  |  `▌`  |          |
| squareRight                                 |  `▐`  |          |
| squareCenter                                |  `■`  |          |
| circle                                      |  `◯`  |  `( )`   |
| circleFilled                                |  `◉`  |  `(*)`   |
| circleDotted                                |  `◌`  |  `( )`   |
| circleDouble                                |  `◎`  |  `( )`   |
| circleCircle                                |  `ⓞ`  |  `(○)`   |
| circleCross                                 |  `ⓧ`  |  `(×)`   |
| circlePipe                                  |  `Ⓘ`  |  `(│)`   |
| circleQuestionMark                          | `?⃝ `  |  `(?)`   |
| radioOn                                     |  `◉`  |  `(*)`   |
| radioOff                                    |  `◯`  |  `( )`   |
| checkboxOn                                  |  `☒`  |  `[×]`   |
| checkboxOff                                 |  `☐`  |  `[ ]`   |
| checkboxCircleOn                            |  `ⓧ`  |  `(×)`   |
| checkboxCircleOff                           |  `Ⓘ`  |  `( )`   |
| questionMarkPrefix                          | `?⃝ `  |   `？`   |
| bullet                                      |  `●`  |          |
| dot                                         |  `․`  |          |
| ellipsis                                    |  `…`  |          |
| pointer                                     |  `❯`  |   `>`    |
| pointerSmall                                |  `›`  |   `›`    |
| triangleUp                                  |  `▲`  |          |
| triangleUpSmall                             |  `▴`  |          |
| triangleUpOutline                           |  `△`  |   `∆`    |
| triangleDown                                |  `▼`  |          |
| triangleDownSmall                           |  `▾`  |          |
| triangleLeft                                |  `◀`  |   `◄`    |
| triangleLeftSmall                           |  `◂`  |          |
| triangleRight                               |  `▶`  |   `►`    |
| triangleRightSmall                          |  `▸`  |          |
| lozenge                                     |  `◆`  |   `♦`    |
| lozengeOutline                              |  `◇`  |   `◊`    |
| home                                        |  `⌂`  |          |
| hamburger                                   |  `☰`  |   `≡`    |
| smiley                                      | `㋡`  |   `☺`    |
| mustache                                    |  `෴`  |  `┌─┐`   |
| heart                                       |  `♥`  |          |
| star                                        |  `★`  |   `✶`    |
| play                                        |  `▶`  |   `►`    |
| musicNote                                   |  `♪`  |          |
| musicNoteBeamed                             |  `♫`  |          |
| nodejs                                      |  `⬢`  |   `♦`    |
| arrowUp                                     |  `↑`  |          |
| arrowDown                                   |  `↓`  |          |
| arrowLeft                                   |  `←`  |          |
| arrowRight                                  |  `→`  |          |
| arrowLeftRight                              |  `↔`  |          |
| arrowUpDown                                 |  `↕`  |          |
| almostEqual                                 |  `≈`  |          |
| notEqual                                    |  `≠`  |          |
| lessOrEqual                                 |  `≤`  |          |
| greaterOrEqual                              |  `≥`  |          |
| identical                                   |  `≡`  |          |
| infinity                                    |  `∞`  |          |
| subscriptZero                               |  `₀`  |          |
| subscriptOne                                |  `₁`  |          |
| subscriptTwo                                |  `₂`  |          |
| subscriptThree                              |  `₃`  |          |
| subscriptFour                               |  `₄`  |          |
| subscriptFive                               |  `₅`  |          |
| subscriptSix                                |  `₆`  |          |
| subscriptSeven                              |  `₇`  |          |
| subscriptEight                              |  `₈`  |          |
| subscriptNine                               |  `₉`  |          |
| oneHalf                                     |  `½`  |          |
| oneThird                                    |  `⅓`  |          |
| oneQuarter                                  |  `¼`  |          |
| oneFifth                                    |  `⅕`  |          |
| oneSixth                                    |  `⅙`  |          |
| oneSeventh                                  |  `⅐`  |  `1/7`   |
| oneEighth                                   |  `⅛`  |          |
| oneNinth                                    |  `⅑`  |  `1/9`   |
| oneTenth                                    |  `⅒`  |  `1/10`  |
| twoThirds                                   |  `⅔`  |          |
| twoFifths                                   |  `⅖`  |          |
| threeQuarters                               |  `¾`  |          |
| threeFifths                                 |  `⅗`  |          |
| threeEighths                                |  `⅜`  |          |
| fourFifths                                  |  `⅘`  |          |
| fiveSixths                                  |  `⅚`  |          |
| fiveEighths                                 |  `⅝`  |          |
| sevenEighths                                |  `⅞`  |          |
| line                                        |  `─`  |          |
| lineBold                                    |  `━`  |          |
| lineDouble                                  |  `═`  |          |
| lineDashed0                                 |  `┄`  |          |
| lineDashed1                                 |  `┅`  |          |
| lineDashed2                                 |  `┈`  |          |
| lineDashed3                                 |  `┉`  |          |
| lineDashed4                                 |  `╌`  |          |
| lineDashed5                                 |  `╍`  |          |
| lineDashed6                                 |  `╴`  |          |
| lineDashed7                                 |  `╶`  |          |
| lineDashed8                                 |  `╸`  |          |
| lineDashed9                                 |  `╺`  |          |
| lineDashed10                                |  `╼`  |          |
| lineDashed11                                |  `╾`  |          |
| lineDashed12                                |  `−`  |          |
| lineDashed13                                |  `–`  |          |
| lineDashed14                                |  `‐`  |          |
| lineDashed15                                |  `⁃`  |          |
| lineVertical                                |  `│`  |          |
| lineVerticalBold                            |  `┃`  |          |
| lineVerticalDouble                          |  `║`  |          |
| lineVerticalDashed0                         |  `┆`  |          |
| lineVerticalDashed1                         |  `┇`  |          |
| lineVerticalDashed2                         |  `┊`  |          |
| lineVerticalDashed3                         |  `┋`  |          |
| lineVerticalDashed4                         |  `╎`  |          |
| lineVerticalDashed5                         |  `╏`  |          |
| lineVerticalDashed6                         |  `╵`  |          |
| lineVerticalDashed7                         |  `╷`  |          |
| lineVerticalDashed8                         |  `╹`  |          |
| lineVerticalDashed9                         |  `╻`  |          |
| lineVerticalDashed10                        |  `╽`  |          |
| lineVerticalDashed11                        |  `╿`  |          |
| lineDownLeft                                |  `┐`  |          |
| lineDownLeftArc                             |  `╮`  |          |
| lineDownBoldLeftBold                        |  `┓`  |          |
| lineDownBoldLeft                            |  `┒`  |          |
| lineDownLeftBold                            |  `┑`  |          |
| lineDownDoubleLeftDouble                    |  `╗`  |          |
| lineDownDoubleLeft                          |  `╖`  |          |
| lineDownLeftDouble                          |  `╕`  |          |
| lineDownRight                               |  `┌`  |          |
| lineDownRightArc                            |  `╭`  |          |
| lineDownBoldRightBold                       |  `┏`  |          |
| lineDownBoldRight                           |  `┎`  |          |
| lineDownRightBold                           |  `┍`  |          |
| lineDownDoubleRightDouble                   |  `╔`  |          |
| lineDownDoubleRight                         |  `╓`  |          |
| lineDownRightDouble                         |  `╒`  |          |
| lineUpLeft                                  |  `┘`  |          |
| lineUpLeftArc                               |  `╯`  |          |
| lineUpBoldLeftBold                          |  `┛`  |          |
| lineUpBoldLeft                              |  `┚`  |          |
| lineUpLeftBold                              |  `┙`  |          |
| lineUpDoubleLeftDouble                      |  `╝`  |          |
| lineUpDoubleLeft                            |  `╜`  |          |
| lineUpLeftDouble                            |  `╛`  |          |
| lineUpRight                                 |  `└`  |          |
| lineUpRightArc                              |  `╰`  |          |
| lineUpBoldRightBold                         |  `┗`  |          |
| lineUpBoldRight                             |  `┖`  |          |
| lineUpRightBold                             |  `┕`  |          |
| lineUpDoubleRightDouble                     |  `╚`  |          |
| lineUpDoubleRight                           |  `╙`  |          |
| lineUpRightDouble                           |  `╘`  |          |
| lineUpDownLeft                              |  `┤`  |          |
| lineUpBoldDownBoldLeftBold                  |  `┫`  |          |
| lineUpBoldDownBoldLeft                      |  `┨`  |          |
| lineUpDownLeftBold                          |  `┥`  |          |
| lineUpBoldDownLeftBold                      |  `┩`  |          |
| lineUpDownBoldLeftBold                      |  `┪`  |          |
| lineUpDownBoldLeft                          |  `┧`  |          |
| lineUpBoldDownLeft                          |  `┦`  |          |
| lineUpDoubleDownDoubleLeftDouble            |  `╣`  |          |
| lineUpDoubleDownDoubleLeft                  |  `╢`  |          |
| lineUpDownLeftDouble                        |  `╡`  |          |
| lineUpDownRight                             |  `├`  |          |
| lineUpBoldDownBoldRightBold                 |  `┣`  |          |
| lineUpBoldDownBoldRight                     |  `┠`  |          |
| lineUpDownRightBold                         |  `┝`  |          |
| lineUpBoldDownRightBold                     |  `┡`  |          |
| lineUpDownBoldRightBold                     |  `┢`  |          |
| lineUpDownBoldRight                         |  `┟`  |          |
| lineUpBoldDownRight                         |  `┞`  |          |
| lineUpDoubleDownDoubleRightDouble           |  `╠`  |          |
| lineUpDoubleDownDoubleRight                 |  `╟`  |          |
| lineUpDownRightDouble                       |  `╞`  |          |
| lineDownLeftRight                           |  `┬`  |          |
| lineDownBoldLeftBoldRightBold               |  `┳`  |          |
| lineDownLeftBoldRightBold                   |  `┯`  |          |
| lineDownBoldLeftRight                       |  `┰`  |          |
| lineDownBoldLeftBoldRight                   |  `┱`  |          |
| lineDownBoldLeftRightBold                   |  `┲`  |          |
| lineDownLeftRightBold                       |  `┮`  |          |
| lineDownLeftBoldRight                       |  `┭`  |          |
| lineDownDoubleLeftDoubleRightDouble         |  `╦`  |          |
| lineDownDoubleLeftRight                     |  `╥`  |          |
| lineDownLeftDoubleRightDouble               |  `╤`  |          |
| lineUpLeftRight                             |  `┴`  |          |
| lineUpBoldLeftBoldRightBold                 |  `┻`  |          |
| lineUpLeftBoldRightBold                     |  `┷`  |          |
| lineUpBoldLeftRight                         |  `┸`  |          |
| lineUpBoldLeftBoldRight                     |  `┹`  |          |
| lineUpBoldLeftRightBold                     |  `┺`  |          |
| lineUpLeftRightBold                         |  `┶`  |          |
| lineUpLeftBoldRight                         |  `┵`  |          |
| lineUpDoubleLeftDoubleRightDouble           |  `╩`  |          |
| lineUpDoubleLeftRight                       |  `╨`  |          |
| lineUpLeftDoubleRightDouble                 |  `╧`  |          |
| lineUpDownLeftRight                         |  `┼`  |          |
| lineUpBoldDownBoldLeftBoldRightBold         |  `╋`  |          |
| lineUpDownBoldLeftBoldRightBold             |  `╈`  |          |
| lineUpBoldDownLeftBoldRightBold             |  `╇`  |          |
| lineUpBoldDownBoldLeftRightBold             |  `╊`  |          |
| lineUpBoldDownBoldLeftBoldRight             |  `╉`  |          |
| lineUpBoldDownLeftRight                     |  `╀`  |          |
| lineUpDownBoldLeftRight                     |  `╁`  |          |
| lineUpDownLeftBoldRight                     |  `┽`  |          |
| lineUpDownLeftRightBold                     |  `┾`  |          |
| lineUpBoldDownBoldLeftRight                 |  `╂`  |          |
| lineUpDownLeftBoldRightBold                 |  `┿`  |          |
| lineUpBoldDownLeftBoldRight                 |  `╃`  |          |
| lineUpBoldDownLeftRightBold                 |  `╄`  |          |
| lineUpDownBoldLeftBoldRight                 |  `╅`  |          |
| lineUpDownBoldLeftRightBold                 |  `╆`  |          |
| lineUpDoubleDownDoubleLeftDoubleRightDouble |  `╬`  |          |
| lineUpDoubleDownDoubleLeftRight             |  `╫`  |          |
| lineUpDownLeftDoubleRightDouble             |  `╪`  |          |
| lineCross                                   |  `╳`  |          |
| lineBackslash                               |  `╲`  |          |
| lineSlash                                   |  `╱`  |          |
