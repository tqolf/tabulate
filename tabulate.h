/**
 * @file tabulate.h
 * @brief Header file for tabulate library
 *
 * This file contains the declarations for tabulate, a modern C++
 * library for creating, formatting, and displaying tabular data
 * in various output formats including terminal, Markdown, and LaTeX.
 *
 * The library provides a rich set of formatting options for text,
 * borders, colors, and alignment, along with internationalization
 * support for multi-byte characters.
 */

/**
 * Copyright 2022-2025 Kiran Nowak(kiran.nowak@gmail.com)
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

#include <string>
#include <vector>
#include <cctype>
#include <cassert>
#include <sstream>
#include <functional>
#include <clocale>
#include <wchar.h>
#include <math.h>
#include <iostream>
#include <locale>
#include <algorithm>
#include <regex>
#include <map>
#include <iomanip>
#include <array>

#if defined(__GNUC__)
#  pragma GCC diagnostic ignored "-Wswitch-enum"
#elif defined(__clang__)
#  pragma clang diagnostic ignored "-Wswitch-enum"
#endif

/**
 * @namespace tabulate
 * @brief A modern C++ tabular data representation library
 *
 * The tabulate namespace contains all the classes and functions
 * needed to create, format, and display tabular data in various forms.
 */
namespace tabulate
{
/**
 * @brief Helper function to convert enum to its underlying type
 * @tparam E The enum type
 * @param e The enum value
 * @return The underlying integral value of the enum
 */
template <typename E>
constexpr typename std::underlying_type<E>::type to_underlying(E e) noexcept
{
  return static_cast<typename std::underlying_type<E>::type>(e);
}

#define XCONCAT(a, b) __CONCAT(a, b)
#define RESERVED      XCONCAT(resevred_, __COUNTER__)

/**
 * @enum Align
 * @brief Specifies alignment options for cell content
 */
enum Align { none = 0, left = 1, hcenter = 2, right = 4, center = 18, top = 8, vcenter = 16, bottom = 32 };

/**
 * @enum Color
 * @brief Basic color options for text and backgrounds
 */
enum class Color { black, red, green, yellow, blue, magenta, cyan, white, RESERVED, none };

/**
 * @enum Which
 * @brief Specifies different positions within a table
 */
enum class Which {
  top,
  bottom,
  left,
  right,
  top_left,
  top_middle,
  top_right,
  bottom_left,
  bottom_middle,
  bottom_right,
  middle_left,
  cross,
  middle_right,
};

/**
 * @enum Style
 * @brief Text styling options for table content
 */
enum class Style {
  // 0 - 9
  none,      // default, VT100
  bold,      // bold, VT100
  faint,     // decreased intensity
  italic,    // italicized
  underline, // underlined
  blink,     // blink, VT100
  RESERVED,  // placeholder for underlying_type
  inverse,   // inverse, VT100
  invisible, // hidden
  crossed,   // crossed-out chracters

  // 21 - 29
  doubly_underline, // doubly underlined
  normal,           // neither bold nor faint
  not_italic,       // not italicized
  not_underline,    // not underlined
  steady,           // not blinking
  RESERVED,         // placeholder for underlying_type
  positive,         // not inverse
  visible,          // not hidden
  not_crossed,      // not crossed-out
};

/**
 * @struct TrueColor
 * @brief Represents RGB color values for text and background
 *
 * Provides more color options beyond the basic terminal colors
 * by supporting true RGB color values.
 */
struct TrueColor {
  /**
   * @brief Default constructor initializing to default/none color
   */
  TrueColor();

  /**
   * @brief Constructor with hex RGB value
   * @param hex The hexadecimal RGB value
   */
  TrueColor(int hex);

  /**
   * @brief Constructor from basic Color enum
   * @param color The basic color to convert to TrueColor
   */
  TrueColor(Color color);

  /**
   * @brief Checks if the color is the default/none color
   * @return true if the color is default, false otherwise
   */
  bool none() const
  {
    return hex == DEFAULT;
  }

  /**
   * @brief Gets the RGB components of the color
   * @return Tuple containing R, G, B components as unsigned chars
   */
  std::tuple<unsigned char, unsigned char, unsigned char> RGB() const;

  /**
   * @brief Merges two colors by averaging their RGB values
   * @param a First color
   * @param b Second color
   * @return A new color that is the average of the two input colors
   */
  static TrueColor merge(const TrueColor &a, const TrueColor &b);

  /**
   * @brief Calculates the similarity between two colors
   * @param a First color
   * @param b Second color
   * @return A value indicating the similarity (lower means more similar)
   */
  static double similarity(const TrueColor &a, const TrueColor &b);

  /**
   * @brief Finds the basic color most similar to a true color
   * @param a The color to match
   * @return The most similar basic Color from the Color enum
   */
  static Color most_similar(const TrueColor &a);

 public:
  int hex;
  Color color;

 private:
  static const int DEFAULT = 0xFF000000;
};

// Forward declaration of the Cell class
class Cell;

/** Type alias for a collection of styles */
using Styles = std::vector<Style>;

/**
 * @typedef StringFormatter
 * @brief Function that formats a string with color and style information
 */
using StringFormatter = std::function<std::string(const std::string &, TrueColor, TrueColor, const Styles &)>;

/**
 * @typedef BorderFormatter
 * @brief Function that formats borders in a table
 */
using BorderFormatter = std::function<std::string(Which which, const Cell *self, const Cell *left, const Cell *right, const Cell *top, const Cell *bottom,
                                                  size_t expect_size, StringFormatter stringformater)>;

/**
 * @typedef CornerFormatter
 * @brief Function that formats corners in a table
 */
using CornerFormatter = std::function<std::string(Which which, const Cell *self, const Cell *top_left, const Cell *top_right, const Cell *bottom_left,
                                                  const Cell *bottom_right, StringFormatter stringformater)>;
} // namespace tabulate

namespace tabulate
{
/**
 * @brief Converts any type to a string representation
 * @tparam T The type to convert
 * @param v The value to convert
 * @return String representation of the value
 */
template <typename T>
inline std::string to_string(const T &v)
{
  std::stringstream ss;
  ss << v;

  return ss.str();
}

/**
 * @brief Specialized conversion for boolean values
 * @param v The boolean value to convert
 * @return "true" or "false" string
 */
template <>
inline std::string to_string<bool>(const bool &v)
{
  return v ? "true" : "false";
}

/**
 * @brief Specialized conversion for C-style strings
 * @param v The C string to convert
 * @return The string value
 */
template <>
inline std::string to_string<const char *>(const char *const &v)
{
  return std::string(v);
}

/**
 * @brief Specialized conversion for std::string (identity operation)
 * @param v The string to "convert"
 * @return The same string
 */
template <>
inline std::string to_string<std::string>(const std::string &v)
{
  return v;
}

/**
 * @brief Specialized conversion for Color enum values
 * @param v The Color value to convert
 * @return String name of the color
 */
template <>
inline std::string to_string<Color>(const Color &v)
{
  switch (v) {
    case Color::black:
      return "black";
    case Color::red:
      return "red";
    case Color::green:
      return "green";
    case Color::yellow:
      return "yellow";
    case Color::blue:
      return "blue";
    case Color::magenta:
      return "magenta";
    case Color::cyan:
      return "cyan";
    case Color::white:
      return "white";
    case Color::none:
    default:
      return "(none)";
  }
}

/**
 * @brief Specialized conversion for TrueColor values
 * @param v The TrueColor value to convert
 * @return Hexadecimal string representation (e.g., "#FF0000")
 */
template <>
inline std::string to_string<TrueColor>(const TrueColor &v)
{
  std::stringstream ss;
  ss << "#" << std::setfill('0') << std::setw(6) << std::hex << v.hex;

  return std::string(ss.str());
}

/**
 * @brief Specialized conversion for Style enum values
 * @param v The Style value to convert
 * @return String name of the style
 */
template <>
inline std::string to_string<Style>(const Style &v)
{
  switch (v) {
    default:
      return "(none)";
    case Style::bold:
      return "bold";
    case Style::faint:
      return "faint";
    case Style::italic:
      return "italic";
    case Style::underline:
      return "underline";
    case Style::blink:
      return "blink";
    case Style::inverse:
      return "inverse";
    case Style::invisible:
      return "invisible";
    case Style::crossed:
      return "crossed";
    case Style::doubly_underline:
      return "doubly_underline";
  }
}
} // namespace tabulate

namespace tabulate
{
/**
 * @brief Newline character sequence for text formatting
 */
static const std::string NEWLINE = "\n";

/**
 * @brief Calculates the display width of a string considering locale
 * @param text The text to measure
 * @param locale The locale to use for width calculation
 * @param wchar_enabled Whether to consider wide characters
 * @return The display width of the text
 */
extern size_t display_width_of(const std::string &text, const std::string &locale, bool wchar_enabled);
} // namespace tabulate

namespace tabulate
{
/**
 * @brief Trims whitespace from the left end of a string
 * @param s The input string
 * @return The left-trimmed string
 */
extern std::string lstrip(const std::string &s);

/**
 * @brief Replaces all occurrences of a substring with another
 * @param str The input string
 * @param from The substring to replace
 * @param to The replacement substring
 * @return The resulting string after all replacements
 */
extern std::string replace_all(std::string str, const std::string &from, const std::string &to);

/**
 * @brief Splits a string into parts using multiple possible separators
 * @param input The input string
 * @param separators List of separator strings
 * @return Vector of string segments
 */
extern std::vector<std::string> explode_string(const std::string &input, const std::vector<std::string> &separators);

/**
 * @brief Wraps a string to fit within a specified width
 * @param str The input string
 * @param width The maximum width for each line
 * @param locale The locale to use for width calculation
 * @param multi_bytes_character Whether to consider multi-byte characters
 * @return Vector of wrapped lines
 */
extern std::vector<std::string> wrap_lines(const std::string &str, size_t width, const std::string &locale, bool multi_bytes_character);

/**
 * @brief Expands a string to fill a specified length
 * @param s The input string
 * @param len The target length
 * @param multi_bytes_character Whether to consider multi-byte characters
 * @return The expanded string
 */
extern std::string expand_to_size(const std::string &s, size_t len, bool multi_bytes_character = true);
} // namespace tabulate

/**
 * @namespace tabulate::symbols
 * @brief Collection of special characters and symbols for table rendering
 */
namespace tabulate::symbols
{
/** Horizontal line symbol */
const std::string hline = "─";
const std::string hline_light = "─";
const std::string hline_heavy = "━";
const std::string hline_double = "═";

/** Vertical line symbol */
const std::string vline = "│";
const std::string vline_light = "│";
const std::string vline_heavy = "┃";
const std::string vline_double = "║";

/** Dotted vertical line symbol */
const std::string dotted_vline = "╎"; // U+254E

/** Dotted horizontal line symbol */
const std::string dotted_hline = "╍"; // U+254D

/** Dashed vertical line symbol */
const std::string dashed_vline = "┆"; // U+2506

/** Dashed horizontal line symbol */
const std::string dashed_hline = "┄"; // U+2504

/** Left-up corner symbol */
const std::string left_up = "┌";
/** Right-up corner symbol */
const std::string right_up = "┐";
/** Left-down corner symbol */
const std::string left_down = "└";
/** Right-down corner symbol */
const std::string right_down = "┘";

/** Rounded left-up corner symbol */
const std::string round_left_up = "╭";
/** Rounded right-up corner symbol */
const std::string round_right_up = "╮";
/** Rounded left-down corner symbol */
const std::string round_left_down = "╰";
/** Rounded right-down corner symbol */
const std::string round_right_down = "╯";

/** Title left-down symbol */
const std::string title_left_down = "┘";
/** Title right-down symbol */
const std::string title_right_down = "└";
/** Title left symbol */
const std::string title_left = "┐";
/** Title right symbol */
const std::string title_right = "┌";

/** Cross symbol */
const std::string cross = "┼";

/** Right division symbol */
const std::string div_right = "┤";
/** Left division symbol */
const std::string div_left = "├";
/** Up division symbol */
const std::string div_up = "┬";
/** Down division symbol */
const std::string div_down = "┴";

/** Up arrow symbol */
const std::string arrow_up = "↑";
/** Down arrow symbol */
const std::string arrow_down = "↓";
/** Left arrow symbol */
const std::string arrow_left = "←";
/** Right arrow symbol */
const std::string arrow_right = "→";

/** Keyboard enter symbol */
const std::string keyboard_enter = "↵";

/** Meter/block symbol */
const std::string meter = "■";

/** Array of superscript digit symbols */
const std::array<std::string, 10> superscript = {"⁰", "¹", "²", "³", "⁴", "⁵", "⁶", "⁷", "⁸", "⁹"};

/**
 * @brief Gets a collection of graph symbols by name
 * @param name The name of the symbol set to retrieve
 * @return Vector of symbol strings
 */
const std::vector<std::string> &get_graph_symbols(const std::string &name);

// Loading
// Dots: ⠋ ⠙ ⠹ ⠸ ⠼ ⠴ ⠦ ⠧ ⠇ ⠏
// Circle: ◜ ◠ ◝ ◞ ◡ ◟

// structure
// Pipes: ┤ ┘ ┴ └ ├ ┌ ┬ ┐
// Line: ─ |

// fills
// fill: ▇
// square: ◼

// progress
// Vertical: ▁ ▃ ▄ ▅ ▆ ▇

// status
// ✔
// ✖

// indicators/arrorws
// ← ↖ ↑ ↗ → ↘ ↓ ↙
// ▹ ▸
// ● ◉ ◯
// ❯
// ›
} // namespace tabulate::symbols

namespace tabulate
{
class Cell;

/**
 * @struct Border
 * @brief Represents the formatting for a border element in a table
 *
 * Defines the visibility, padding, color, content, and background color
 * for table borders (top, bottom, left, right).
 */
struct Border {
  bool visiable;

  size_t padding;
  TrueColor color;
  std::string content;
  TrueColor background_color;

  enum class Style { solid, dotted, dashed, double_line, heavy };
  Style style;
  bool draw_outer; // Whether to draw this border when it's on the outside edge
};

/**
 * @struct Corner
 * @brief Represents the formatting for a corner element in a table
 *
 * Defines the visibility, color, content, and background color
 * for table corners (top-left, top-right, bottom-left, bottom-right).
 */
struct Corner {
  bool visiable;
  TrueColor color;
  std::string content;
  TrueColor background_color;

  enum class Style { normal, rounded, double_line, heavy };
  Style style;
  bool draw_outer; // Whether to draw this corner when it's on the outer edge
};

/**
 * @struct Format
 * @brief Core formatting class for table elements
 *
 * Provides comprehensive formatting options for table cells,
 * including alignment, colors, borders, corners, and internationalization.
 */
struct Format {
  /**
   * @brief Default constructor that initializes with standard formatting
   */
  Format();

  /**
   * @brief Gets the width of the cell
   * @return The cell width
   */
  inline size_t width() const
  {
    return cell.width;
  }

  /**
   * @brief Gets the height of the cell
   * @return The cell height
   */
  inline size_t height() const
  {
    return cell.height;
  }

  /**
   * @brief Sets the width of the cell
   * @param value The new width
   * @return Reference to this Format object for method chaining
   */
  inline Format &width(size_t value)
  {
    cell.width = value;
    return *this;
  }

  /**
   * @brief Gets the alignment of the cell
   * @return The cell alignment
   */
  inline Align align() const
  {
    return cell.align;
  }

  /**
   * @brief Sets the alignment of the cell
   * @param value The new alignment
   * @return Reference to this Format object for method chaining
   */
  inline Format &align(Align value)
  {
    cell.align = value;
    return *this;
  }

  /**
   * @brief Gets the text color of the cell
   * @return The cell text color
   */
  inline TrueColor color() const
  {
    return cell.color;
  }

  /**
   * @brief Sets the text color of the cell
   * @param value The new text color
   * @return Reference to this Format object for method chaining
   */
  inline Format &color(TrueColor value)
  {
    cell.color = value;
    return *this;
  }

  /**
   * @brief Gets the background color of the cell
   * @return The cell background color
   */
  inline TrueColor background_color() const
  {
    return cell.background_color;
  }

  /**
   * @brief Sets the background color of the cell
   * @param value The new background color
   * @return Reference to this Format object for method chaining
   */
  inline Format &background_color(TrueColor value)
  {
    cell.background_color = value;
    return *this;
  }

  /**
   * @brief Gets the text styles of the cell
   * @return The collection of cell styles
   */
  inline const Styles &styles() const
  {
    return cell.styles;
  }

  /**
   * @brief Adds a style to the cell
   * @param value The style to add
   * @return Reference to this Format object for method chaining
   */
  inline Format &styles(Style value)
  {
    cell.styles.push_back(value);
    return *this;
  }

  /**
   * @brief Adds multiple styles to the cell
   * @tparam Args Variadic template for multiple style arguments
   * @param style The first style to add
   * @param args The remaining styles to add
   * @return Reference to this Format object for method chaining
   */
  template <typename... Args>
  inline Format &styles(Style style, Args... args)
  {
    cell.styles.push_back(style);
    return styles(args...);
  }

  /* borders */
  /**
   * @brief Sets the content for all borders
   * @param value The content string
   * @return Reference to this Format object for method chaining
   */
  Format &border(const std::string &value);

  /**
   * @brief Sets the padding for all borders
   * @param value The padding size
   * @return Reference to this Format object for method chaining
   */
  Format &border_padding(size_t value);

  /**
   * @brief Sets the color for all borders
   * @param value The border color
   * @return Reference to this Format object for method chaining
   */
  Format &border_color(TrueColor value);

  /**
   * @brief Sets the background color for all borders
   * @param value The border background color
   * @return Reference to this Format object for method chaining
   */
  Format &border_background_color(TrueColor value);

  /**
   * @brief Sets the left border content
   * @param value The content string
   * @return Reference to this Format object for method chaining
   */
  Format &border_left(const std::string &value);

  /**
   * @brief Sets the left border color
   * @param value The border color
   * @return Reference to this Format object for method chaining
   */
  Format &border_left_color(TrueColor value);

  /**
   * @brief Sets the left border background color
   * @param value The border background color
   * @return Reference to this Format object for method chaining
   */
  Format &border_left_background_color(TrueColor value);

  /**
   * @brief Sets the left border padding
   * @param value The padding size
   * @return Reference to this Format object for method chaining
   */
  Format &border_left_padding(size_t value);

  /**
   * @brief Sets the right border content
   * @param value The content string
   * @return Reference to this Format object for method chaining
   */
  Format &border_right(const std::string &value);

  /**
   * @brief Sets the right border color
   * @param value The border color
   * @return Reference to this Format object for method chaining
   */
  Format &border_right_color(TrueColor value);

  /**
   * @brief Sets the right border background color
   * @param value The border background color
   * @return Reference to this Format object for method chaining
   */
  Format &border_right_background_color(TrueColor value);

  /**
   * @brief Sets the right border padding
   * @param value The padding size
   * @return Reference to this Format object for method chaining
   */
  Format &border_right_padding(size_t value);

  /**
   * @brief Sets the top border content
   * @param value The content string
   * @return Reference to this Format object for method chaining
   */
  Format &border_top(const std::string &value);

  /**
   * @brief Sets the top border color
   * @param value The border color
   * @return Reference to this Format object for method chaining
   */
  Format &border_top_color(TrueColor value);

  /**
   * @brief Sets the top border background color
   * @param value The border background color
   * @return Reference to this Format object for method chaining
   */
  Format &border_top_background_color(TrueColor value);

  /**
   * @brief Sets the top border padding
   * @param value The padding size
   * @return Reference to this Format object for method chaining
   */
  Format &border_top_padding(size_t value);

  /**
   * @brief Sets the bottom border content
   * @param value The content string
   * @return Reference to this Format object for method chaining
   */
  Format &border_bottom(const std::string &value);

  /**
   * @brief Sets the bottom border color
   * @param value The border color
   * @return Reference to this Format object for method chaining
   */
  Format &border_bottom_color(TrueColor value);

  /**
   * @brief Sets the bottom border background color
   * @param value The border background color
   * @return Reference to this Format object for method chaining
   */
  Format &border_bottom_background_color(TrueColor value);

  /**
   * @brief Sets the bottom border padding
   * @param value The padding size
   * @return Reference to this Format object for method chaining
   */
  Format &border_bottom_padding(size_t value);

  /**
   * @brief Shows all borders
   * @return Reference to this Format object for method chaining
   */
  Format &show_border();

  /**
   * @brief Hides all borders
   * @return Reference to this Format object for method chaining
   */
  Format &hide_border();

  /**
   * @brief Sets the style for all borders
   * @param style The border style to use
   * @return Reference to this Format object for method chaining
   */
  Format &border_style(Border::Style style);

  /**
   * @brief Sets the style for the left border
   * @param style The border style to use
   * @return Reference to this Format object for method chaining
   */
  Format &border_left_style(Border::Style style);

  /**
   * @brief Sets the style for the right border
   * @param style The border style to use
   * @return Reference to this Format object for method chaining
   */
  Format &border_right_style(Border::Style style);

  /**
   * @brief Sets the style for the top border
   * @param style The border style to use
   * @return Reference to this Format object for method chaining
   */
  Format &border_top_style(Border::Style style);

  /**
   * @brief Sets the style for the bottom border
   * @param style The border style to use
   * @return Reference to this Format object for method chaining
   */
  Format &border_bottom_style(Border::Style style);

  /**
   * @brief Sets outer border drawing for all borders
   * @param value Whether to draw the border when it's an outer border
   * @return Reference to this Format object for method chaining
   */
  Format &draw_outer_border(bool value);

  /**
   * @brief Sets outer border drawing for the left border
   * @param value Whether to draw the border when it's an outer border
   * @return Reference to this Format object for method chaining
   */
  Format &draw_outer_left_border(bool value);

  /**
   * @brief Sets outer border drawing for the right border
   * @param value Whether to draw the border when it's an outer border
   * @return Reference to this Format object for method chaining
   */
  Format &draw_outer_right_border(bool value);

  /**
   * @brief Sets outer border drawing for the top border
   * @param value Whether to draw the border when it's an outer border
   * @return Reference to this Format object for method chaining
   */
  Format &draw_outer_top_border(bool value);

  /**
   * @brief Sets outer border drawing for the bottom border
   * @param value Whether to draw the border when it's an outer border
   * @return Reference to this Format object for method chaining
   */
  Format &draw_outer_bottom_border(bool value);

  /**
   * @brief Shows the top border
   * @return Reference to this Format object for method chaining
   */
  Format &show_border_top();

  /**
   * @brief Hides the top border
   * @return Reference to this Format object for method chaining
   */
  Format &hide_border_top();

  /**
   * @brief Shows the bottom border
   * @return Reference to this Format object for method chaining
   */
  Format &show_border_bottom();

  /**
   * @brief Hides the bottom border
   * @return Reference to this Format object for method chaining
   */
  Format &hide_border_bottom();

  /**
   * @brief Shows the left border
   * @return Reference to this Format object for method chaining
   */
  Format &show_border_left();

  /**
   * @brief Hides the left border
   * @return Reference to this Format object for method chaining
   */
  Format &hide_border_left();

  /**
   * @brief Shows the right border
   * @return Reference to this Format object for method chaining
   */
  Format &show_border_right();

  /**
   * @brief Hides the right border
   * @return Reference to this Format object for method chaining
   */
  Format &hide_border_right();

  /* corners */
  /**
   * @brief Sets the content for all corners
   * @param value The corner content string
   * @return Reference to this Format object for method chaining
   */
  Format &corner(const std::string &value);

  /**
   * @brief Sets the color for all corners
   * @param value The corner color
   * @return Reference to this Format object for method chaining
   */
  Format &corner_color(TrueColor value);

  /**
   * @brief Sets the background color for all corners
   * @param value The corner background color
   * @return Reference to this Format object for method chaining
   */
  Format &corner_background_color(TrueColor value);

  /**
   * @brief Sets the top-left corner content
   * @param value The corner content string
   * @return Reference to this Format object for method chaining
   */
  Format &corner_top_left(const std::string &value);

  /**
   * @brief Sets the top-left corner color
   * @param value The corner color
   * @return Reference to this Format object for method chaining
   */
  Format &corner_top_left_color(TrueColor value);

  /**
   * @brief Sets the top-left corner background color
   * @param value The corner background color
   * @return Reference to this Format object for method chaining
   */
  Format &corner_top_left_background_color(TrueColor value);

  /**
   * @brief Sets the top-right corner content
   * @param value The corner content string
   * @return Reference to this Format object for method chaining
   */
  Format &corner_top_right(const std::string &value);

  /**
   * @brief Sets the top-right corner color
   * @param value The corner color
   * @return Reference to this Format object for method chaining
   */
  Format &corner_top_right_color(TrueColor value);

  /**
   * @brief Sets the top-right corner background color
   * @param value The corner background color
   * @return Reference to this Format object for method chaining
   */
  Format &corner_top_right_background_color(TrueColor value);

  /**
   * @brief Sets the bottom-left corner content
   * @param value The corner content string
   * @return Reference to this Format object for method chaining
   */
  Format &corner_bottom_left(const std::string &value);

  /**
   * @brief Sets the bottom-left corner color
   * @param value The corner color
   * @return Reference to this Format object for method chaining
   */
  Format &corner_bottom_left_color(TrueColor value);

  /**
   * @brief Sets the bottom-left corner background color
   * @param value The corner background color
   * @return Reference to this Format object for method chaining
   */
  Format &corner_bottom_left_background_color(TrueColor value);

  /**
   * @brief Sets the bottom-right corner content
   * @param value The corner content string
   * @return Reference to this Format object for method chaining
   */
  Format &corner_bottom_right(const std::string &value);

  /**
   * @brief Sets the bottom-right corner color
   * @param value The corner color
   * @return Reference to this Format object for method chaining
   */
  Format &corner_bottom_right_color(TrueColor value);

  /**
   * @brief Sets the bottom-right corner background color
   * @param value The corner background color
   * @return Reference to this Format object for method chaining
   */
  Format &corner_bottom_right_background_color(TrueColor value);

  /**
   * @brief Sets the style for all corners
   * @param style The corner style to use
   * @return Reference to this Format object for method chaining
   */
  Format &corner_style(Corner::Style style);

  /**
   * @brief Sets the style for the top-left corner
   * @param style The corner style to use
   * @return Reference to this Format object for method chaining
   */
  Format &corner_top_left_style(Corner::Style style);

  /**
   * @brief Sets the style for the top-right corner
   * @param style The corner style to use
   * @return Reference to this Format object for method chaining
   */
  Format &corner_top_right_style(Corner::Style style);

  /**
   * @brief Sets the style for the bottom-left corner
   * @param style The corner style to use
   * @return Reference to this Format object for method chaining
   */
  Format &corner_bottom_left_style(Corner::Style style);

  /**
   * @brief Sets the style for the bottom-right corner
   * @param style The corner style to use
   * @return Reference to this Format object for method chaining
   */
  Format &corner_bottom_right_style(Corner::Style style);

  /**
   * @brief Sets outer corner drawing for all corners
   * @param value Whether to draw the corner when it's an outer corner
   * @return Reference to this Format object for method chaining
   */
  Format &draw_outer_corner(bool value);

  /**
   * @brief Sets outer corner drawing for the top-left corner
   * @param value Whether to draw the corner when it's an outer corner
   * @return Reference to this Format object for method chaining
   */
  Format &draw_outer_top_left_corner(bool value);

  /**
   * @brief Sets outer corner drawing for the top-right corner
   * @param value Whether to draw the corner when it's an outer corner
   * @return Reference to this Format object for method chaining
   */
  Format &draw_outer_top_right_corner(bool value);

  /**
   * @brief Sets outer corner drawing for the bottom-left corner
   * @param value Whether to draw the corner when it's an outer corner
   * @return Reference to this Format object for method chaining
   */
  Format &draw_outer_bottom_left_corner(bool value);

  /**
   * @brief Sets outer corner drawing for the bottom-right corner
   * @param value Whether to draw the corner when it's an outer corner
   * @return Reference to this Format object for method chaining
   */
  Format &draw_outer_bottom_right_corner(bool value);

  /**
   * @brief Sets the content for the cross junction (all four directions)
   * @param value The junction content string
   * @return Reference to this Format object for method chaining
   */
  Format &corner_cross(const std::string &value);

  /**
   * @brief Sets the content for the tee-north junction (left, right, up)
   * @param value The junction content string
   * @return Reference to this Format object for method chaining
   */
  Format &corner_bottom_middle(const std::string &value);

  /**
   * @brief Sets the content for the tee-south junction (left, right, down)
   * @param value The junction content string
   * @return Reference to this Format object for method chaining
   */
  Format &corner_top_middle(const std::string &value);

  /**
   * @brief Sets the content for the tee-west junction (up, down, left)
   * @param value The junction content string
   * @return Reference to this Format object for method chaining
   */
  Format &corner_middle_right(const std::string &value);

  /**
   * @brief Sets the content for the tee-east junction (up, down, right)
   * @param value The junction content string
   * @return Reference to this Format object for method chaining
   */
  Format &corner_middle_left(const std::string &value);

  /**
   * @brief Sets the color for the cross junction
   * @param value The junction color
   * @return Reference to this Format object for method chaining
   */
  Format &corner_cross_color(TrueColor value);

  /**
   * @brief Sets the color for the tee-north junction
   * @param value The junction color
   * @return Reference to this Format object for method chaining
   */
  Format &corner_bottom_middle_color(TrueColor value);

  /**
   * @brief Sets the color for the tee-south junction
   * @param value The junction color
   * @return Reference to this Format object for method chaining
   */
  Format &corner_top_middle_color(TrueColor value);

  /**
   * @brief Sets the color for the tee-west junction
   * @param value The junction color
   * @return Reference to this Format object for method chaining
   */
  Format &corner_middle_right_color(TrueColor value);

  /**
   * @brief Sets the color for the tee-east junction
   * @param value The junction color
   * @return Reference to this Format object for method chaining
   */
  Format &corner_middle_left_color(TrueColor value);

  /**
   * @brief Sets the background color for the cross junction
   * @param value The junction background color
   * @return Reference to this Format object for method chaining
   */
  Format &corner_cross_background_color(TrueColor value);

  /**
   * @brief Sets the background color for the tee-north junction
   * @param value The junction background color
   * @return Reference to this Format object for method chaining
   */
  Format &corner_bottom_middle_background_color(TrueColor value);

  /**
   * @brief Sets the background color for the tee-south junction
   * @param value The junction background color
   * @return Reference to this Format object for method chaining
   */
  Format &corner_top_middle_background_color(TrueColor value);

  /**
   * @brief Sets the background color for the tee-west junction
   * @param value The junction background color
   * @return Reference to this Format object for method chaining
   */
  Format &corner_middle_right_background_color(TrueColor value);

  /**
   * @brief Sets the background color for the tee-east junction
   * @param value The junction background color
   * @return Reference to this Format object for method chaining
   */
  Format &corner_middle_left_background_color(TrueColor value);

  /* internationlization */
  /**
   * @brief Gets the locale for the cell
   * @return The cell locale string
   */
  const std::string &locale() const;

  /**
   * @brief Sets the locale for the cell
   * @param value The locale string
   * @return Reference to this Format object for method chaining
   */
  Format &locale(const std::string &value);

  /**
   * @brief Checks if multi-byte character support is enabled
   * @return true if multi-byte character support is enabled, false otherwise
   */
  bool multi_bytes_character() const;

  /**
   * @brief Enables or disables multi-byte character support
   * @param value true to enable, false to disable
   * @return Reference to this Format object for method chaining
   */
  Format &multi_bytes_character(bool value);

  /**
   * @struct CellFormat
   * @brief Cell-specific formatting options
   */
  struct {
    Align align;
    Styles styles;
    TrueColor color, background_color;

    size_t width, height;
  } cell;

  /**
   * @struct BorderFormat
   * @brief Container for all border formatting options
   */
  struct {
    Border left, right, top, bottom;
  } borders;

  /**
   * @struct CornerFormat
   * @brief Container for all corner formatting options
   */
  struct {
    Corner top_left, top_middle, top_right;
    Corner middle_left, cross, middle_right;
    Corner bottom_left, bottom_middle, bottom_right;
  } corners;

  /**
   * @struct ColumnSeparatorFormat
   * @brief Formatting for column separators
   */
  struct {
    std::string content;
    TrueColor color, background_color;
  } column_separator;

  /**
   * @struct InternationalizationFormat
   * @brief Internationalization options
   */
  struct {
    std::string locale;
    bool multi_bytes_character;
  } internationlization;
};

/**
 * @class BatchFormat
 * @brief Applies formatting to multiple cells at once
 *
 * This class allows consistent formatting to be applied to a collection
 * of cells, reducing repetitive code when formatting tables.
 */
class BatchFormat {
 public:
  /**
   * @brief Constructor that takes a collection of cells to format
   * @param cells The collection of cells to apply formatting to
   */
  BatchFormat(std::vector<std::shared_ptr<Cell>> cells);

  // Basic property methods
  /**
   * @brief Gets the number of cells in this batch
   * @return The cell count
   */
  size_t size();

  /**
   * @brief Sets the width for all cells in the batch
   * @param value The width to set
   * @return Reference to this BatchFormat for method chaining
   */
  BatchFormat &width(size_t value);

  /**
   * @brief Sets the alignment for all cells in the batch
   * @param value The alignment to set
   * @return Reference to this BatchFormat for method chaining
   */
  BatchFormat &align(Align value);

  /**
   * @brief Sets the text color for all cells in the batch
   * @param value The color to set
   * @return Reference to this BatchFormat for method chaining
   */
  BatchFormat &color(TrueColor value);

  /**
   * @brief Sets the background color for all cells in the batch
   * @param value The background color to set
   * @return Reference to this BatchFormat for method chaining
   */
  BatchFormat &background_color(TrueColor value);

  /**
   * @brief Adds a style to all cells in the batch
   * @param value The style to add
   * @return Reference to this BatchFormat for method chaining
   */
  BatchFormat &styles(Style value);

  /**
   * @brief Adds multiple styles to all cells in the batch
   * @tparam Args Variadic template for multiple style arguments
   * @param values The styles to add
   * @return Reference to this BatchFormat for method chaining
   */
  template <typename... Args>
  BatchFormat &styles(Args... values);

  // Border methods
  /**
   * @brief Sets the padding for all borders of all cells
   * @param value The padding size
   * @return Reference to this BatchFormat for method chaining
   */
  BatchFormat &border_padding(size_t value);

  /**
   * @brief Sets the left border padding for all cells
   * @param value The padding size
   * @return Reference to this BatchFormat for method chaining
   */
  BatchFormat &border_left_padding(size_t value);

  /**
   * @brief Sets the right border padding for all cells
   * @param value The padding size
   * @return Reference to this BatchFormat for method chaining
   */
  BatchFormat &border_right_padding(size_t value);

  /**
   * @brief Sets the top border padding for all cells
   * @param value The padding size
   * @return Reference to this BatchFormat for method chaining
   */
  BatchFormat &border_top_padding(size_t value);

  /**
   * @brief Sets the bottom border padding for all cells
   * @param value The padding size
   * @return Reference to this BatchFormat for method chaining
   */
  BatchFormat &border_bottom_padding(size_t value);

  /**
   * @brief Sets the content for all borders of all cells
   * @param value The border content string
   * @return Reference to this BatchFormat for method chaining
   */
  BatchFormat &border(const std::string &value);

  /**
   * @brief Sets the color for all borders of all cells
   * @param value The border color
   * @return Reference to this BatchFormat for method chaining
   */
  BatchFormat &border_color(TrueColor value);

  /**
   * @brief Sets the background color for all borders of all cells
   * @param value The border background color
   * @return Reference to this BatchFormat for method chaining
   */
  BatchFormat &border_background_color(TrueColor value);

  /**
   * @brief Sets the left border content for all cells
   * @param value The border content string
   * @return Reference to this BatchFormat for method chaining
   */
  BatchFormat &border_left(const std::string &value);

  /**
   * @brief Sets the left border color for all cells
   * @param value The border color
   * @return Reference to this BatchFormat for method chaining
   */
  BatchFormat &border_left_color(TrueColor value);

  /**
   * @brief Sets the left border background color for all cells
   * @param value The border background color
   * @return Reference to this BatchFormat for method chaining
   */
  BatchFormat &border_left_background_color(TrueColor value);

  /**
   * @brief Sets the right border content for all cells
   * @param value The border content string
   * @return Reference to this BatchFormat for method chaining
   */
  BatchFormat &border_right(const std::string &value);

  /**
   * @brief Sets the right border color for all cells
   * @param value The border color
   * @return Reference to this BatchFormat for method chaining
   */
  BatchFormat &border_right_color(TrueColor value);

  /**
   * @brief Sets the right border background color for all cells
   * @param value The border background color
   * @return Reference to this BatchFormat for method chaining
   */
  BatchFormat &border_right_background_color(TrueColor value);

  /**
   * @brief Sets the top border content for all cells
   * @param value The border content string
   * @return Reference to this BatchFormat for method chaining
   */
  BatchFormat &border_top(const std::string &value);

  /**
   * @brief Sets the top border color for all cells
   * @param value The border color
   * @return Reference to this BatchFormat for method chaining
   */
  BatchFormat &border_top_color(TrueColor value);

  /**
   * @brief Sets the top border background color for all cells
   * @param value The border background color
   * @return Reference to this BatchFormat for method chaining
   */
  BatchFormat &border_top_background_color(TrueColor value);

  /**
   * @brief Sets the bottom border content for all cells
   * @param value The border content string
   * @return Reference to this BatchFormat for method chaining
   */
  BatchFormat &border_bottom(const std::string &value);

  /**
   * @brief Sets the bottom border color for all cells
   * @param value The border color
   * @return Reference to this BatchFormat for method chaining
   */
  BatchFormat &border_bottom_color(TrueColor value);

  /**
   * @brief Sets the bottom border background color for all cells
   * @param value The border background color
   * @return Reference to this BatchFormat for method chaining
   */
  BatchFormat &border_bottom_background_color(TrueColor value);

  // Border visibility methods
  /**
   * @brief Shows all borders for all cells
   * @return Reference to this BatchFormat for method chaining
   */
  BatchFormat &show_border();

  /**
   * @brief Hides all borders for all cells
   * @return Reference to this BatchFormat for method chaining
   */
  BatchFormat &hide_border();

  /**
   * @brief Shows the top border for all cells
   * @return Reference to this BatchFormat for method chaining
   */
  BatchFormat &show_border_top();

  /**
   * @brief Hides the top border for all cells
   * @return Reference to this BatchFormat for method chaining
   */
  BatchFormat &hide_border_top();

  /**
   * @brief Shows the bottom border for all cells
   * @return Reference to this BatchFormat for method chaining
   */
  BatchFormat &show_border_bottom();

  /**
   * @brief Hides the bottom border for all cells
   * @return Reference to this BatchFormat for method chaining
   */
  BatchFormat &hide_border_bottom();

  /**
   * @brief Shows the left border for all cells
   * @return Reference to this BatchFormat for method chaining
   */
  BatchFormat &show_border_left();

  /**
   * @brief Hides the left border for all cells
   * @return Reference to this BatchFormat for method chaining
   */
  BatchFormat &hide_border_left();

  /**
   * @brief Shows the right border for all cells
   * @return Reference to this BatchFormat for method chaining
   */
  BatchFormat &show_border_right();

  /**
   * @brief Hides the right border for all cells
   * @return Reference to this BatchFormat for method chaining
   */
  BatchFormat &hide_border_right();

  // Border style methods
  /**
   * @brief Sets the style for all borders of all cells
   * @param style The border style to use
   * @return Reference to this BatchFormat for method chaining
   */
  BatchFormat &border_style(Border::Style style);

  /**
   * @brief Sets the style for the left border of all cells
   * @param style The border style to use
   * @return Reference to this BatchFormat for method chaining
   */
  BatchFormat &border_left_style(Border::Style style);

  /**
   * @brief Sets the style for the right border of all cells
   * @param style The border style to use
   * @return Reference to this BatchFormat for method chaining
   */
  BatchFormat &border_right_style(Border::Style style);

  /**
   * @brief Sets the style for the top border of all cells
   * @param style The border style to use
   * @return Reference to this BatchFormat for method chaining
   */
  BatchFormat &border_top_style(Border::Style style);

  /**
   * @brief Sets the style for the bottom border of all cells
   * @param style The border style to use
   * @return Reference to this BatchFormat for method chaining
   */
  BatchFormat &border_bottom_style(Border::Style style);

  /**
   * @brief Sets outer border drawing for all borders of all cells
   * @param value Whether to draw the border when it's an outer border
   * @return Reference to this BatchFormat for method chaining
   */
  BatchFormat &draw_outer_border(bool value);

  /**
   * @brief Sets outer border drawing for the left border of all cells
   * @param value Whether to draw the border when it's an outer border
   * @return Reference to this BatchFormat for method chaining
   */
  BatchFormat &draw_outer_left_border(bool value);

  /**
   * @brief Sets outer border drawing for the right border of all cells
   * @param value Whether to draw the border when it's an outer border
   * @return Reference to this BatchFormat for method chaining
   */
  BatchFormat &draw_outer_right_border(bool value);

  /**
   * @brief Sets outer border drawing for the top border of all cells
   * @param value Whether to draw the border when it's an outer border
   * @return Reference to this BatchFormat for method chaining
   */
  BatchFormat &draw_outer_top_border(bool value);

  /**
   * @brief Sets outer border drawing for the bottom border of all cells
   * @param value Whether to draw the border when it's an outer border
   * @return Reference to this BatchFormat for method chaining
   */
  BatchFormat &draw_outer_bottom_border(bool value);

  // Corner methods
  /**
   * @brief Sets the content for all corners of all cells
   * @param value The corner content string
   * @return Reference to this BatchFormat for method chaining
   */
  BatchFormat &corner(const std::string &value);

  /**
   * @brief Sets the color for all corners of all cells
   * @param value The corner color
   * @return Reference to this BatchFormat for method chaining
   */
  BatchFormat &corner_color(TrueColor value);

  /**
   * @brief Sets the background color for all corners of all cells
   * @param value The corner background color
   * @return Reference to this BatchFormat for method chaining
   */
  BatchFormat &corner_background_color(TrueColor value);

  /**
   * @brief Sets the top-left corner content for all cells
   * @param value The corner content string
   * @return Reference to this BatchFormat for method chaining
   */
  BatchFormat &corner_top_left(const std::string &value);

  /**
   * @brief Sets the top-left corner color for all cells
   * @param value The corner color
   * @return Reference to this BatchFormat for method chaining
   */
  BatchFormat &corner_top_left_color(TrueColor value);

  /**
   * @brief Sets the top-left corner background color for all cells
   * @param value The corner background color
   * @return Reference to this BatchFormat for method chaining
   */
  BatchFormat &corner_top_left_background_color(TrueColor value);

  /**
   * @brief Sets the top-right corner content for all cells
   * @param value The corner content string
   * @return Reference to this BatchFormat for method chaining
   */
  BatchFormat &corner_top_right(const std::string &value);

  /**
   * @brief Sets the top-right corner color for all cells
   * @param value The corner color
   * @return Reference to this BatchFormat for method chaining
   */
  BatchFormat &corner_top_right_color(TrueColor value);

  /**
   * @brief Sets the top-right corner background color for all cells
   * @param value The corner background color
   * @return Reference to this BatchFormat for method chaining
   */
  BatchFormat &corner_top_right_background_color(TrueColor value);

  /**
   * @brief Sets the bottom-left corner content for all cells
   * @param value The corner content string
   * @return Reference to this BatchFormat for method chaining
   */
  BatchFormat &corner_bottom_left(const std::string &value);

  /**
   * @brief Sets the bottom-left corner color for all cells
   * @param value The corner color
   * @return Reference to this BatchFormat for method chaining
   */
  BatchFormat &corner_bottom_left_color(TrueColor value);

  /**
   * @brief Sets the bottom-left corner background color for all cells
   * @param value The corner background color
   * @return Reference to this BatchFormat for method chaining
   */
  BatchFormat &corner_bottom_left_background_color(TrueColor value);

  /**
   * @brief Sets the bottom-right corner content for all cells
   * @param value The corner content string
   * @return Reference to this BatchFormat for method chaining
   */
  BatchFormat &corner_bottom_right(const std::string &value);

  /**
   * @brief Sets the bottom-right corner color for all cells
   * @param value The corner color
   * @return Reference to this BatchFormat for method chaining
   */
  BatchFormat &corner_bottom_right_color(TrueColor value);

  /**
   * @brief Sets the bottom-right corner background color for all cells
   * @param value The corner background color
   * @return Reference to this BatchFormat for method chaining
   */
  BatchFormat &corner_bottom_right_background_color(TrueColor value);

  /**
   * @brief Sets the style for all corners
   * @param style The corner style to use
   * @return Reference to this BatchFormat for method chaining
   */
  BatchFormat &corner_style(Corner::Style style);

  /**
   * @brief Sets the style for the top-left corner
   * @param style The corner style to use
   * @return Reference to this BatchFormat for method chaining
   */
  BatchFormat &corner_top_left_style(Corner::Style style);

  /**
   * @brief Sets the style for the top-right corner
   * @param style The corner style to use
   * @return Reference to this BatchFormat for method chaining
   */
  BatchFormat &corner_top_right_style(Corner::Style style);

  /**
   * @brief Sets the style for the bottom-left corner
   * @param style The corner style to use
   * @return Reference to this BatchFormat for method chaining
   */
  BatchFormat &corner_bottom_left_style(Corner::Style style);

  /**
   * @brief Sets the style for the bottom-right corner
   * @param style The corner style to use
   * @return Reference to this BatchFormat for method chaining
   */
  BatchFormat &corner_bottom_right_style(Corner::Style style);

  /**
   * @brief Sets outer corner drawing for all corners
   * @param value Whether to draw the corner when it's an outer corner
   * @return Reference to this BatchFormat for method chaining
   */
  BatchFormat &draw_outer_corner(bool value);

  /**
   * @brief Sets outer corner drawing for the top-left corner
   * @param value Whether to draw the corner when it's an outer corner
   * @return Reference to this BatchFormat for method chaining
   */
  BatchFormat &draw_outer_top_left_corner(bool value);

  /**
   * @brief Sets outer corner drawing for the top-right corner
   * @param value Whether to draw the corner when it's an outer corner
   * @return Reference to this BatchFormat for method chaining
   */
  BatchFormat &draw_outer_top_right_corner(bool value);

  /**
   * @brief Sets outer corner drawing for the bottom-left corner
   * @param value Whether to draw the corner when it's an outer corner
   * @return Reference to this BatchFormat for method chaining
   */
  BatchFormat &draw_outer_bottom_left_corner(bool value);

  /**
   * @brief Sets outer corner drawing for the bottom-right corner
   * @param value Whether to draw the corner when it's an outer corner
   * @return Reference to this BatchFormat for method chaining
   */
  BatchFormat &draw_outer_bottom_right_corner(bool value);

  // Internationalization methods
  /**
   * @brief Sets the locale for all cells
   * @param value The locale string
   * @return Reference to this BatchFormat for method chaining
   */
  BatchFormat &locale(const std::string &value);

  /**
   * @brief Enables or disables multi-byte character support for all cells
   * @param value true to enable, false to disable
   * @return Reference to this BatchFormat for method chaining
   */
  BatchFormat &multi_bytes_character(bool value);

 private:
  std::vector<std::shared_ptr<Cell>> cells;
};

/**
 * @class Cell
 * @brief Represents a single cell in a table
 *
 * Contains the content and formatting information for a table cell.
 */
class Cell {
 public:
  /**
   * @brief Constructor that creates a cell with the given content
   * @param content The cell content string
   */
  Cell(const std::string &content);

  /**
   * @brief Gets the content of the cell
   * @return The cell content string
   */
  const std::string &get() const;

  /**
   * @brief Sets the content of the cell
   * @param content The new content string
   */
  void set(const std::string &content);

  /**
   * @brief Sets the content of the cell using a value of any type
   * @tparam T The type of value to set
   * @param value The value to convert to string and set as content
   */
  template <typename T>
  void set(const T value)
  {
    content_ = to_string(value);
  }

  /**
   * @brief Calculates the display size of the cell content
   * @return The display size
   */
  size_t size();

  /**
   * @brief Gets the format object for the cell
   * @return Reference to the cell's Format object
   */
  Format &format();

  /**
   * @brief Gets the format object for the cell (const version)
   * @return Const reference to the cell's Format object
   */
  const Format &format() const;

  /**
   * @brief Gets the display width of the cell
   * @return The display width
   */
  size_t width() const;

  /**
   * @brief Gets the alignment of the cell
   * @return The cell alignment
   */
  Align align() const;

  /**
   * @brief Gets the text color of the cell
   * @return The cell text color
   */
  TrueColor color() const;

  /**
   * @brief Gets the background color of the cell
   * @return The cell background color
   */
  TrueColor background_color() const;

  /**
   * @brief Gets the text styles of the cell
   * @return The collection of cell styles
   */
  Styles styles() const;

 private:
  Format m_format;
  std::string content_;
};

template <typename... Args>
BatchFormat &BatchFormat::styles(Args... values)
{
  for (auto &cell : cells) {
    cell->format().styles(values...);
  }
  return *this;
}

/**
 * @class PtrIterator
 * @brief Iterator for containers of shared pointer elements
 *
 * Provides iterator functionality for containers of shared pointers,
 * dereferencing the shared pointer to access the underlying object.
 *
 * @tparam Container The container type (e.g., std::vector)
 * @tparam Value The type of object pointed to by the shared pointer
 */
template <template <typename, typename> class Container, typename Value>
class PtrIterator {
  using iterator = typename Container<std::shared_ptr<Value>, std::allocator<std::shared_ptr<Value>>>::iterator;

 public:
  /**
   * @brief Constructs an iterator from a container iterator
   * @param ptr The container iterator
   */
  explicit PtrIterator(iterator ptr) : ptr(ptr) {}

  /**
   * @brief Prefix increment operator
   * @return Reference to this iterator after incrementing
   */
  PtrIterator operator++()
  {
    ++ptr;
    return *this;
  }

  /**
   * @brief Inequality comparison operator
   * @param other Another iterator to compare with
   * @return true if iterators are not equal, false otherwise
   */
  bool operator!=(const PtrIterator &other) const
  {
    return ptr != other.ptr;
  }

  /**
   * @brief Dereference operator
   * @return Reference to the underlying object
   */
  Value &operator*()
  {
    return **ptr;
  }

 private:
  iterator ptr;
};

/**
 * @class PtrConstIterator
 * @brief Const iterator for containers of shared pointer elements
 *
 * Provides const iterator functionality for containers of shared pointers,
 * dereferencing the shared pointer to access the underlying object.
 *
 * @tparam Container The container type (e.g., std::vector)
 * @tparam Value The type of object pointed to by the shared pointer
 */
template <template <typename, typename> class Container, typename Value>
class PtrConstIterator {
 public:
  using iterator = typename Container<std::shared_ptr<Value>, std::allocator<std::shared_ptr<Value>>>::const_iterator;

  /**
   * @brief Constructs a const iterator from a container const iterator
   * @param ptr The container const iterator
   */
  explicit PtrConstIterator(iterator ptr) : ptr(ptr) {}

  /**
   * @brief Prefix increment operator
   * @return Reference to this iterator after incrementing
   */
  PtrConstIterator operator++()
  {
    ++ptr;
    return *this;
  }

  /**
   * @brief Inequality comparison operator
   * @param other Another iterator to compare with
   * @return true if iterators are not equal, false otherwise
   */
  bool operator!=(const PtrConstIterator &other) const
  {
    return ptr != other.ptr;
  }

  /**
   * @brief Dereference operator
   * @return Const reference to the underlying object
   */
  const Value &operator*()
  {
    return **ptr;
  }

 private:
  iterator ptr;
};

/**
 * @class Row
 * @brief Represents a row of cells in a table
 *
 * Manages a collection of cells that form a row, providing methods
 * to add, access, and format the cells.
 */
class Row {
 public:
  /**
   * @brief Default constructor that creates an empty row
   */
  Row() {}

  /**
   * @brief Constructor that creates a row with multiple values
   * @tparam Args Variadic template for multiple cell values
   * @param args The values to create cells from
   */
  template <typename... Args>
  Row(Args... args)
  {
    add(args...);
  }

  /**
   * @brief Adds a single value as a new cell
   * @tparam T The type of the value to add
   * @param v The value to add
   */
  template <typename T>
  void add(const T v)
  {
    cells.push_back(std::shared_ptr<Cell>(new Cell(to_string(v))));
  }

  /**
   * @brief Adds multiple values as new cells
   * @tparam T The type of the first value
   * @tparam Args Variadic template for additional values
   * @param v The first value to add
   * @param args Additional values to add
   */
  template <typename T, typename... Args>
  void add(T v, Args... args)
  {
    add(v);
    add(args...);
  }

  /**
   * @brief Array access operator to get a cell by index
   * @param index The index of the cell to retrieve
   * @return Reference to the cell at the specified index
   */
  Cell &operator[](size_t index);

  /**
   * @brief Const array access operator to get a cell by index
   * @param index The index of the cell to retrieve
   * @return Const reference to the cell at the specified index
   */
  const Cell &operator[](size_t index) const;

  /**
   * @brief Gets a shared pointer to a cell by index
   * @param index The index of the cell to retrieve
   * @return Shared pointer to the cell at the specified index
   */
  std::shared_ptr<Cell> &cell(size_t index);

  /**
   * @brief Creates a batch formatter for a range of cells
   * @param from The starting index (inclusive)
   * @param to The ending index (inclusive)
   * @return BatchFormat object for the specified range
   */
  BatchFormat format(size_t from, size_t to);

  /**
   * @brief Creates a batch formatter for multiple ranges of cells
   * @param ranges List of (start, end) index tuples
   * @return BatchFormat object for the specified ranges
   */
  BatchFormat format(std::initializer_list<std::tuple<size_t, size_t>> ranges);

  /**
   * @brief Gets the number of cells in the row
   * @return The cell count
   */
  size_t size() const;

  /* iterator */
  /**
   * @typedef iterator
   * @brief Iterator type for Row cells
   */
  using iterator = PtrIterator<std::vector, Cell>;

  /**
   * @typedef const_iterator
   * @brief Const iterator type for Row cells
   */
  using const_iterator = PtrConstIterator<std::vector, Cell>;

  /**
   * @brief Gets an iterator to the beginning of the row
   * @return Iterator pointing to the first cell
   */
  iterator begin();

  /**
   * @brief Gets an iterator to the end of the row
   * @return Iterator pointing past the last cell
   */
  iterator end();

  /**
   * @brief Gets a const iterator to the beginning of the row
   * @return Const iterator pointing to the first cell
   */
  const_iterator begin() const;

  /**
   * @brief Gets a const iterator to the end of the row
   * @return Const iterator pointing past the last cell
   */
  const_iterator end() const;

  /**
   * @brief Creates a batch formatter for all cells in the row
   * @return BatchFormat object for the entire row
   */
  BatchFormat format();

  /**
   * @brief Converts the row to a vector of formatted strings
   * @param stringformatter Function to format strings with color and style
   * @param borderformatter Function to format borders
   * @param cornerformatter Function to format corners
   * @param row_index The index of this row in the table (0-based)
   * @param header_count Number of header rows in the table
   * @param total_rows Total number of rows in the table
   * @return Vector of formatted strings representing the row
   */
  std::vector<std::string> dump(StringFormatter stringformatter, BorderFormatter borderformatter, CornerFormatter cornerformatter, size_t row_index,
                                size_t header_count, size_t total_rows) const;

 private:
  std::vector<std::shared_ptr<Cell>> cells;
};

/**
 * @class Column
 * @brief Represents a column of cells in a table
 *
 * Manages a collection of cells that form a column, providing methods
 * to add, access, and format the cells.
 */
class Column {
 public:
  /**
   * @brief Default constructor that creates an empty column
   */
  Column() {}

  /**
   * @brief Adds a cell to the column
   * @param cell Shared pointer to the cell to add
   */
  void add(std::shared_ptr<Cell> cell);

  /**
   * @brief Creates a batch formatter for all cells in the column
   * @return BatchFormat object for the entire column
   */
  BatchFormat format();

  /**
   * @brief Gets the number of cells in the column
   * @return The cell count
   */
  size_t size();

  /**
   * @brief Array access operator to get a cell by index
   * @param index The index of the cell to retrieve
   * @return Reference to the cell at the specified index
   */
  Cell &operator[](size_t index);

  /**
   * @brief Const array access operator to get a cell by index
   * @param index The index of the cell to retrieve
   * @return Const reference to the cell at the specified index
   */
  const Cell &operator[](size_t index) const;

  /**
   * @brief Creates a batch formatter for a range of cells
   * @param from The starting index (inclusive)
   * @param to The ending index (inclusive)
   * @return BatchFormat object for the specified range
   */
  BatchFormat format(size_t from, size_t to);

  /**
   * @brief Creates a batch formatter for multiple ranges of cells
   * @param ranges List of (start, end) index tuples
   * @return BatchFormat object for the specified ranges
   */
  BatchFormat format(std::initializer_list<std::tuple<size_t, size_t>> ranges);

  /* iterator */
  /**
   * @typedef iterator
   * @brief Iterator type for Column cells
   */
  using iterator = PtrIterator<std::vector, Cell>;

  /**
   * @typedef const_iterator
   * @brief Const iterator type for Column cells
   */
  using const_iterator = PtrConstIterator<std::vector, Cell>;

  /**
   * @brief Gets an iterator to the beginning of the column
   * @return Iterator pointing to the first cell
   */
  iterator begin();

  /**
   * @brief Gets an iterator to the end of the column
   * @return Iterator pointing past the last cell
   */
  iterator end();

  /**
   * @brief Gets a const iterator to the beginning of the column
   * @return Const iterator pointing to the first cell
   */
  const_iterator begin() const;

  /**
   * @brief Gets a const iterator to the end of the column
   * @return Const iterator pointing past the last cell
   */
  const_iterator end() const;

 private:
  std::vector<std::shared_ptr<Cell>> cells;
};
} // namespace tabulate

namespace tabulate
{
// https://invisible-island.net/xterm/ctlseqs/ctlseqs.html
namespace xterm
{
/**
 * @brief Checks if the terminal supports true color
 * @return true if true color is supported, false otherwise
 */
bool has_truecolor();

/**
 * @brief Formats a string with color and style for xterm
 * @param str The string to format
 * @param foreground_color The text color
 * @param background_color The background color
 * @param styles The text styles to apply
 * @return The formatted string with ANSI escape sequences
 */
std::string stringformatter(const std::string &str, TrueColor foreground_color, TrueColor background_color, const Styles &styles);

/**
 * @brief Formats a border in a table for xterm
 * @param which Which border to format
 * @param self The cell whose border is being formatted
 * @param left The cell to the left (or nullptr)
 * @param right The cell to the right (or nullptr)
 * @param top The cell above (or nullptr)
 * @param bottom The cell below (or nullptr)
 * @param expected_size The expected size of the border
 * @param stringformatter Function to format strings with color and style
 * @return The formatted border string
 */
std::string borderformatter(Which which, const Cell *self, const Cell *left, const Cell *right, const Cell *top, const Cell *bottom, size_t expected_size,
                            StringFormatter stringformatter);

/**
 * @brief Formats a corner in a table for xterm
 * @param which Which corner to format
 * @param self The cell whose corner is being formatted
 * @param top_left The top-left cell (or nullptr)
 * @param top_right The top-right cell (or nullptr)
 * @param bottom_left The bottom-left cell (or nullptr)
 * @param bottom_right The bottom-right cell (or nullptr)
 * @param stringformatter Function to format strings with color and style
 * @return The formatted corner string
 */
std::string cornerformatter(Which which, const Cell *self, const Cell *top_left, const Cell *top_right, const Cell *bottom_left, const Cell *bottom_right,
                            StringFormatter stringformatter);
} // namespace xterm
} // namespace tabulate

namespace tabulate
{
/**
 * @class Table
 * @brief Main class for creating and managing tables
 *
 * Provides comprehensive functionality for building, formatting,
 * and rendering tables in various output formats.
 */
class Table : public std::enable_shared_from_this<Table> {
 public:
  /**
   * @brief Default constructor that creates an empty table
   */
  Table() = default;

  /**
   * @brief Constructor that creates a table with a single row of values
   * @tparam Args Variadic template for row values
   * @param args The values to add to the first row
   */
  template <typename... Args>
  Table(Args... args)
  {
    add(args...);
  }

  /**
   * @brief Sets the title of the table
   * @param title The title string
   */
  void set_title(std::string title);

  /**
   * @brief Creates a batch formatter for all cells in the table
   * @return BatchFormat object for the entire table
   */
  BatchFormat format();

  /**
   * @brief Adds an empty row to the table
   * @return Reference to the new row
   */
  Row &add();

  /**
   * @brief Adds a row with a single value
   * @tparam T The type of the value
   * @param arg The value to add
   * @return Reference to the new row
   */
  template <typename T>
  Row &add(T arg)
  {
    Row &row = __add_row();
    row.add(arg);
    __on_add_auto_update();

    return row;
  }

  /**
   * @brief Adds a row with multiple values
   * @tparam Args Variadic template for row values
   * @param args The values to add to the row
   * @return Reference to the new row
   */
  template <typename... Args>
  Row &add(Args... args)
  {
    Row &row = __add_row();
    row.add(args...);
    __on_add_auto_update();

    return row;
  }

  /**
   * @brief Adds a row with values from a container
   * @tparam Value The type of values in the container
   * @tparam Container The container type
   * @tparam Allocator The allocator type for the container
   * @param multiple The container of values to add
   * @return Reference to the new row
   */
  template <typename Value, template <typename, typename> class Container, typename Allocator = std::allocator<Value>>
  Row &add_multiple(const Container<Value, Allocator> &multiple)
  {
    Row &row = __add_row();
    for (auto const &value : multiple) {
      row.add(value);
    }
    __on_add_auto_update();

    return row;
  }

  /**
   * @brief Array access operator to get a row by index
   * @param index The index of the row to retrieve
   * @return Reference to the row at the specified index
   */
  Row &operator[](size_t index);

  /**
   * @brief Gets the number of rows in the table
   * @return The row count
   */
  size_t size();

  /* iterator */
  /**
   * @typedef iterator
   * @brief Iterator type for Table rows
   */
  using iterator = PtrIterator<std::vector, Row>;

  /**
   * @typedef const_iterator
   * @brief Const iterator type for Table rows
   */
  using const_iterator = PtrConstIterator<std::vector, Row>;

  /**
   * @brief Gets an iterator to the beginning of the table
   * @return Iterator pointing to the first row
   */
  iterator begin();

  /**
   * @brief Gets an iterator to the end of the table
   * @return Iterator pointing past the last row
   */
  iterator end();

  /**
   * @brief Gets a const iterator to the beginning of the table
   * @return Const iterator pointing to the first row
   */
  const_iterator begin() const;

  /**
   * @brief Gets a const iterator to the end of the table
   * @return Const iterator pointing past the last row
   */
  const_iterator end() const;

  /**
   * @brief Gets a column from the table by index
   * @param index The index of the column to retrieve
   * @return The column at the specified index
   */
  Column column(size_t index);

  /**
   * @brief Gets the number of columns in the table
   * @return The column count
   */
  size_t column_size() const;

  /**
   * @brief Gets the display width of the table
   * @return The table width
   */
  size_t width() const;

  /**
   * @brief Merges cells in the table
   * @param from The starting cell coordinates (x, y)
   * @param to The ending cell coordinates (x, y)
   * @return Result code (0 for success)
   */
  int merge(std::tuple<int, int> from, std::tuple<int, int> to);

  /**
   * @brief Renders the table in xterm format
   * @param disable_color Whether to disable color in the output
   * @return String representation of the table
   */
  std::string xterm(bool disable_color = false) const;

  /**
   * @brief Renders the table in xterm format with page breaks
   * @param maxlines Maximum number of lines per page
   * @param keep_row_in_one_page Whether to keep rows together on the same page
   * @return String representation of the table with page breaks
   */
  std::string xterm(size_t maxlines, bool keep_row_in_one_page = true) const;

  /**
   * @brief Renders the table in Markdown format
   * @return Markdown representation of the table
   */
  std::string markdown() const;

  /**
   * @brief Renders the table in LaTeX format
   * @param indentation Number of spaces to indent content lines
   * @return LaTeX representation of the table
   */
  std::string latex(size_t indentation = 0) const;

 private:
  std::string title;
  std::vector<std::shared_ptr<Row>> rows;
  std::vector<std::shared_ptr<Cell>> cells; // for batch format
  std::vector<std::tuple<int, int, int, int>> merges;

  size_t cached_width;

  /**
   * @brief Helper method to add a new row
   * @return Reference to the new row
   */
  Row &__add_row();

  /**
   * @brief Helper method to update internal state after adding a row
   */
  void __on_add_auto_update();

  /**
   * @brief Helper method to calculate the display width of the table
   * @return The calculated width
   */
  size_t __width() const;
};

/**
 * @brief Specialized conversion of Row to string
 * @param v The Row to convert
 * @return String representation of the row
 */
template <>
inline std::string to_string<Row>(const Row &v)
{
  std::string ret;
  auto lines = v.dump(tabulate::xterm::stringformatter, tabulate::xterm::borderformatter, tabulate::xterm::cornerformatter, 0, 0, 1);
  for (auto &line : lines) {
    ret += line + NEWLINE;
  }
  return ret;
}

/**
 * @brief Specialized conversion of Table to string
 * @param v The Table to convert
 * @return String representation of the table in xterm format
 */
template <>
inline std::string to_string<Table>(const Table &v)
{
  return v.xterm();
}
} // namespace tabulate
