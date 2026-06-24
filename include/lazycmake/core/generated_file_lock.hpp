#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace lazycmake::core {

struct FileEntry {
    std::string relativePath;
    std::string sha256;
    bool userOwned = false;
};

class GeneratedFileLock {
public:
    GeneratedFileLock(std::filesystem::path lockPath,
                      std::filesystem::path rootDir = {});

    bool load();
    bool save() const;

    bool hasChanged(const std::filesystem::path& filePath) const;
    void recordHash(const std::filesystem::path& filePath, const std::string& content);
    void markUserOwned(const std::filesystem::path& filePath);
    bool isUserOwned(const std::filesystem::path& filePath) const;
    void remove(const std::filesystem::path& filePath);

    enum class ConflictAction { Overwrite, KeepMine, ViewDiff };
    struct ConflictResult {
        std::string filePath;
        bool hasConflict;
        ConflictAction chosenAction;
    };

    ConflictResult checkConflict(const std::filesystem::path& filePath,
                                  const std::string& newContent) const;

    std::vector<FileEntry> entries() const { return entries_; }

private:
    std::filesystem::path lockPath_;
    std::filesystem::path rootDir_;
    std::vector<FileEntry> entries_;

    static std::string computeSha256(const std::string& content);
    std::string toRelative(const std::filesystem::path& path) const;
    FileEntry* findEntry(const std::filesystem::path& filePath);
    const FileEntry* findEntry(const std::filesystem::path& filePath) const;
};

} // namespace lazycmake::core
