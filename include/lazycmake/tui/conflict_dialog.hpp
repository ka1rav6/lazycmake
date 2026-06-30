#pragma once

#include <functional>
#include <string>

#include <ftxui/component/component.hpp>

#include "lazycmake/config/keymap_manager.hpp"
#include "lazycmake/config/theme_manager.hpp"
#include "lazycmake/core/generated_file_lock.hpp"

namespace lazycmake::tui {

class ConflictDialog {
public:
    ConflictDialog(config::KeymapManager& keymap,
                   config::ThemeManager& theme);

    ftxui::Component build();
    void show(const std::string& filePath,
              core::GeneratedFileLock::ConflictResult result);
    void hide();
    bool isVisible() const { return visible_; }

    void setOnResolved(std::function<void()> cb);

    core::GeneratedFileLock::ConflictAction chosenAction() const {
        return chosenAction_;
    }

private:
    config::KeymapManager& keymap_;
    config::ThemeManager& theme_;

    bool visible_ = false;
    std::string filePath_;
    core::GeneratedFileLock::ConflictResult result_;
    core::GeneratedFileLock::ConflictAction chosenAction_ =
        core::GeneratedFileLock::ConflictAction::Overwrite;
    std::function<void()> onConflictResolved_;
};

} // namespace lazycmake::tui
