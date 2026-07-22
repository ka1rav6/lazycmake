#include "lazycmake/tui/panels/file_panel.hpp"

#include <algorithm>

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>

#include "lazycmake/tui/color_helper.hpp"

namespace lazycmake::tui {

namespace {

bool shouldSkip(const std::filesystem::path& p) {
    auto name = p.filename().string();
    return name.starts_with('.') || name == "build";
}

} // namespace

FilePanel::FilePanel(config::KeymapManager& keymap,
                     config::ThemeManager& theme)
    : keymap_(keymap), theme_(theme) {
    // Default dummy list until setProject is called.
    files_ = {};
}

ftxui::Component FilePanel::build() {
    auto self = this;
    auto renderer = ftxui::Renderer(std::function<ftxui::Element()>([self] {
        return self->render();
    }));

    auto wrapped = renderer | ftxui::CatchEvent([self](ftxui::Event event) {
        return self->onEvent(event);
    });

    return wrapped;
}

void FilePanel::setProject(const core::Project& project) {
    rootDir_ = project.rootDir;
    refreshFileList();
}

void FilePanel::refreshFileList() {
    files_.clear();
    if (rootDir_.empty() || !std::filesystem::exists(rootDir_)) return;

    try {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(rootDir_)) {
            if (entry.is_regular_file() && !shouldSkip(entry.path())) {
                files_.push_back(std::filesystem::relative(entry.path(), rootDir_));
            }
        }
    } catch (...) {
        // Ignore filesystem errors.
    }

    std::sort(files_.begin(), files_.end());
    if (selectedIndex_ >= static_cast<int>(files_.size())) {
        selectedIndex_ = files_.empty() ? 0 : static_cast<int>(files_.size()) - 1;
    }
}

bool FilePanel::onEvent(ftxui::Event event) {
    if (!focused_ || files_.empty()) return false;

    if (event == ftxui::Event::ArrowDown || event == ftxui::Event::Character('j')) {
        selectedIndex_ = std::min(selectedIndex_ + 1, static_cast<int>(files_.size()) - 1);
        return true;
    }
    if (event == ftxui::Event::ArrowUp || event == ftxui::Event::Character('k')) {
        selectedIndex_ = std::max(0, selectedIndex_ - 1);
        return true;
    }

    return false;
}

ftxui::Element FilePanel::render() {
    const auto& colors = theme_.activeColors();
    auto bg = focused_ ? colors.panelBackground : colors.listInactiveBackground;
    auto fg = focused_ ? colors.foreground : colors.listInactiveForeground;
    auto selBg = colors.listSelectionBackground;
    auto selFg = colors.listSelectionForeground;

    ftxui::Elements items;
    if (files_.empty()) {
        items.push_back(ftxui::text("  (no project loaded)") | ftxui::dim | ftxui::color(colorFromString(fg)));
    } else {
        for (size_t i = 0; i < files_.size(); ++i) {
            auto path = files_[i].string();
            auto prefix = path.find('/') != std::string::npos ? "  " : "";
            auto text = ftxui::text(prefix + path);
            if (i == static_cast<size_t>(selectedIndex_) && focused_) {
                items.push_back(text | ftxui::bgcolor(colorFromString(selBg))
                                       | ftxui::color(colorFromString(selFg)));
            } else {
                items.push_back(text | ftxui::color(colorFromString(fg)));
            }
        }
    }

    auto content = ftxui::vbox(std::move(items));
    auto title = focused_ ? ftxui::text(" Files ") | ftxui::bold
                          : ftxui::text(" Files ") | ftxui::dim;

    auto borderColor = focused_ ? colors.panelBorderFocused
                                : colors.panelBorderUnfocused;
    return ftxui::vbox({
               title,
               ftxui::separator(),
               content | ftxui::frame | ftxui::flex,
           }) |
           ftxui::border | ftxui::color(colorFromString(borderColor));
}

} // namespace lazycmake::tui
