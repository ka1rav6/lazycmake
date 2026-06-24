#include "lazycmake/tui/color_helper.hpp"

#include <spdlog/spdlog.h>

#include <ftxui/dom/elements.hpp>

namespace lazycmake::tui {

ftxui::Color colorFromString(const std::string& colorStr) {
    if (colorStr.empty()) {
        return ftxui::Color::Default;
    }

    // Try parsing "#RRGGBB" hex format.
    if (colorStr.size() == 7 && colorStr[0] == '#') {
        auto hexToByte = [](char c) -> uint8_t {
            if (c >= '0' && c <= '9') return static_cast<uint8_t>(c - '0');
            if (c >= 'a' && c <= 'f') return static_cast<uint8_t>(10 + (c - 'a'));
            if (c >= 'A' && c <= 'F') return static_cast<uint8_t>(10 + (c - 'A'));
            return 0;
        };

        uint8_t r = static_cast<uint8_t>((hexToByte(colorStr[1]) << 4) | hexToByte(colorStr[2]));
        uint8_t g = static_cast<uint8_t>((hexToByte(colorStr[3]) << 4) | hexToByte(colorStr[4]));
        uint8_t b = static_cast<uint8_t>((hexToByte(colorStr[5]) << 4) | hexToByte(colorStr[6]));
        return ftxui::Color::RGB(r, g, b);
    }

    spdlog::trace("colorFromString: using named color '{}'", colorStr);
    return ftxui::Color::Default;
}

ftxui::Element withThemeBackground(ftxui::Element element,
                                    const std::string& bgColor) {
    return element | ftxui::bgcolor(colorFromString(bgColor));
}

ftxui::Element withThemeForeground(ftxui::Element element,
                                    const std::string& fgColor) {
    return element | ftxui::color(colorFromString(fgColor));
}

ftxui::Element withThemeBorder(ftxui::Element element,
                                const std::string& borderColor,
                                const std::string&) {
    return element | ftxui::border | ftxui::color(colorFromString(borderColor));
}

} // namespace lazycmake::tui
