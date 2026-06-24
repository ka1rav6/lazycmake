#include "lazycmake/fswatch/file_watcher.hpp"

#include <algorithm>
#include <chrono>
#include <thread>

#include <spdlog/spdlog.h>

namespace lazycmake::fswatch {

FileClassifier::FileClassifier() {
    relevantExtensions_ = {".cpp", ".cxx", ".cc", ".c", ".hpp", ".h", ".hxx",
                           ".cmake", ".txt", ".md", ".json", ".toml", ".yaml"};
    excludeDirs_ = {"build", ".git", ".lazycmake", "__pycache__", ".cache"};
}

bool FileClassifier::isRelevant(const std::filesystem::path& path) const {
    if (isExcluded(path)) return false;
    auto ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return std::find(relevantExtensions_.begin(), relevantExtensions_.end(), ext)
           != relevantExtensions_.end();
}

void FileClassifier::addPattern(const std::string& pattern) {
    relevantExtensions_.push_back(pattern);
}

void FileClassifier::setExcludeDirs(const std::vector<std::string>& dirs) {
    excludeDirs_ = dirs;
}

bool FileClassifier::isExcluded(const std::filesystem::path& path) const {
    for (const auto& dir : excludeDirs_) {
        auto pathStr = path.generic_string();
        if (pathStr.find('/' + dir + '/') != std::string::npos ||
            pathStr.find(dir + '/') == 0 ||
            pathStr == dir) {
            return true;
        }
    }
    return false;
}

FileWatcher::FileWatcher(std::filesystem::path watchDir)
    : watchDir_(std::move(watchDir)) {
    classifier_ = std::make_unique<FileClassifier>();
}

FileWatcher::~FileWatcher() {
    stop();
}

void FileWatcher::start(Callback callback) {
    if (running_) return;
    running_ = true;
    pollThread_ = std::thread([this, callback = std::move(callback)]() {
        auto previous = scanDirectory();
        while (running_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(debounceMs_));
            detectChanges(previous, callback);
        }
    });
}

void FileWatcher::stop() {
    running_ = false;
    if (pollThread_.joinable()) {
        pollThread_.join();
    }
}

bool FileWatcher::isRunning() const {
    return running_;
}

void FileWatcher::setDebounceMs(int ms) {
    debounceMs_ = ms;
}

void FileWatcher::setClassifier(std::unique_ptr<FileClassifier> classifier) {
    classifier_ = std::move(classifier);
}

void FileWatcher::detectChanges(
    std::map<std::filesystem::path, std::filesystem::file_time_type>& previous,
    Callback callback) {
    auto current = scanDirectory();

    for (const auto& [path, time] : current) {
        auto prevIt = previous.find(path);
        if (prevIt == previous.end()) {
            callback({FileChangeKind::Added, path});
        } else if (prevIt->second != time) {
            callback({FileChangeKind::Modified, path});
        }
    }

    for (const auto& [path, time] : previous) {
        if (current.find(path) == current.end()) {
            callback({FileChangeKind::Removed, path});
        }
    }

    previous = std::move(current);
}

std::map<std::filesystem::path, std::filesystem::file_time_type>
    FileWatcher::scanDirectory() const {
    std::map<std::filesystem::path, std::filesystem::file_time_type> result;

    if (!std::filesystem::exists(watchDir_)) return result;

    try {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(watchDir_)) {
            if (!entry.is_regular_file()) continue;
            if (classifier_ && !classifier_->isRelevant(entry.path())) continue;
            result[entry.path()] = entry.last_write_time();
        }
    } catch (const std::filesystem::filesystem_error& e) {
        spdlog::warn("FileWatcher: scan error: {}", e.what());
    }

    return result;
}

} // namespace lazycmake::fswatch
