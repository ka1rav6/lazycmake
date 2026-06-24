#pragma once

#include <string>

#include <ftxui/dom/elements.hpp>

namespace lazycmake::tui {

// Convert a color string (in "#RRGGBB" or named format) to an FTXUI Color.
// Falls back to ftxui::Color::Default if parsing fails.
ftxui::Color colorFromString(const std::string& colorStr);

// Apply the theme's background color to an element.
ftxui::Element withThemeBackground(ftxui::Element element,
                                    const std::string& bgColor);

// Apply the theme's foreground color to text.
ftxui::Element withThemeForeground(ftxui::Element element,
                                    const std::string& fgColor);

// Apply a border with the theme's panel border color.
ftxui::Element withThemeBorder(ftxui::Element element,
                                const std::string& borderColor,
                                const std::string& title = "");

} // namespace lazycmake::tui
