#include "lazycmake/core/generated_file_lock.hpp"

#include <fstream>
#include <iomanip>
#include <sstream>

#include <spdlog/spdlog.h>

#if LAZYCMAKE_HAVE_OPENSSL
#include <openssl/sha.h>
#else
#include <functional>
#endif

namespace lazycmake::core {

GeneratedFileLock::GeneratedFileLock(std::filesystem::path lockPath,
                                     std::filesystem::path rootDir)
    : lockPath_(std::move(lockPath))
    , rootDir_(rootDir.empty() ? lockPath_.parent_path() : std::move(rootDir)) {}

std::string GeneratedFileLock::toRelative(const std::filesystem::path& path) const {
    return std::filesystem::relative(path, rootDir_).generic_string();
}

bool GeneratedFileLock::load() {
    if (!std::filesystem::exists(lockPath_)) return false;
    try {
        std::ifstream file(lockPath_);
        auto json = nlohmann::json::parse(file);
        if (json.contains("files")) {
            entries_.clear();
            for (const auto& entry : json["files"]) {
                FileEntry fe;
                fe.relativePath = entry["path"];
                fe.sha256 = entry["sha256"];
                fe.userOwned = entry.value("userOwned", false);
                entries_.push_back(fe);
            }
        }
        spdlog::debug("GeneratedFileLock: loaded {} entries from {}", entries_.size(), lockPath_.string());
        return true;
    } catch (const std::exception& e) {
        spdlog::warn("GeneratedFileLock: failed to load: {}", e.what());
        return false;
    }
}

bool GeneratedFileLock::save() const {
    try {
        auto parent = lockPath_.parent_path();
        if (!std::filesystem::exists(parent)) {
            std::filesystem::create_directories(parent);
        }
        nlohmann::json json;
        json["version"] = "1.0";
        auto& files = json["files"] = nlohmann::json::array();
        for (const auto& entry : entries_) {
            files.push_back({
                {"path", entry.relativePath},
                {"sha256", entry.sha256},
                {"userOwned", entry.userOwned},
            });
        }
        std::ofstream file(lockPath_);
        file << json.dump(2);
        return true;
    } catch (const std::exception& e) {
        spdlog::error("GeneratedFileLock: failed to save: {}", e.what());
        return false;
    }
}

std::string GeneratedFileLock::computeSha256(const std::string& content) {
#if LAZYCMAKE_HAVE_OPENSSL
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(content.data()),
           content.size(), hash);
    std::stringstream ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
    }
    return ss.str();
#else
    std::hash<std::string> hasher;
    auto h = hasher(content);
    std::stringstream ss;
    ss << std::hex << h;
    return ss.str();
#endif
}

bool GeneratedFileLock::hasChanged(const std::filesystem::path& filePath) const {
    auto rel = toRelative(filePath);
    for (const auto& entry : entries_) {
        if (entry.relativePath == rel) {
            if (!std::filesystem::exists(filePath)) return true;
            std::ifstream file(filePath);
            std::stringstream buffer;
            buffer << file.rdbuf();
            auto currentHash = computeSha256(buffer.str());
            return currentHash != entry.sha256;
        }
    }
    return false;
}

void GeneratedFileLock::recordHash(const std::filesystem::path& filePath,
                                    const std::string& content) {
    auto rel = toRelative(filePath);
    auto hash = computeSha256(content);
    auto* existing = findEntry(filePath);
    if (existing) {
        existing->sha256 = hash;
        existing->userOwned = false;
    } else {
        entries_.push_back(FileEntry{rel, hash, false});
    }
}

void GeneratedFileLock::markUserOwned(const std::filesystem::path& filePath) {
    auto* entry = findEntry(filePath);
    if (entry) entry->userOwned = true;
}

bool GeneratedFileLock::isUserOwned(const std::filesystem::path& filePath) const {
    auto* entry = findEntry(filePath);
    return entry && entry->userOwned;
}

void GeneratedFileLock::remove(const std::filesystem::path& filePath) {
    auto rel = toRelative(filePath);
    entries_.erase(
        std::remove_if(entries_.begin(), entries_.end(),
                       [&](const FileEntry& e) { return e.relativePath == rel; }),
        entries_.end());
}

GeneratedFileLock::ConflictResult GeneratedFileLock::checkConflict(
    const std::filesystem::path& filePath, const std::string& /*newContent*/) const {
    ConflictResult cr;
    cr.filePath = toRelative(filePath);
    cr.hasConflict = false;
    cr.chosenAction = ConflictAction::Overwrite;

    if (isUserOwned(filePath)) {
        cr.hasConflict = true;
        cr.chosenAction = ConflictAction::KeepMine;
        return cr;
    }

    if (!std::filesystem::exists(filePath)) return cr;

    std::ifstream file(filePath);
    std::stringstream buffer;
    buffer << file.rdbuf();
    auto diskHash = computeSha256(buffer.str());

    auto* entry = findEntry(filePath);
    if (!entry) {
        cr.hasConflict = true;
        cr.chosenAction = ConflictAction::ViewDiff;
        return cr;
    }

    if (diskHash != entry->sha256) {
        cr.hasConflict = true;
    }

    return cr;
}

FileEntry* GeneratedFileLock::findEntry(const std::filesystem::path& filePath) {
    auto rel = toRelative(filePath);
    for (auto& entry : entries_) {
        if (entry.relativePath == rel) return &entry;
    }
    return nullptr;
}

const FileEntry* GeneratedFileLock::findEntry(const std::filesystem::path& filePath) const {
    auto rel = toRelative(filePath);
    for (const auto& entry : entries_) {
        if (entry.relativePath == rel) return &entry;
    }
    return nullptr;
}

} // namespace lazycmake::core
