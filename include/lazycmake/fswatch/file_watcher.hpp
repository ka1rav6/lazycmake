#pragma once

#include <filesystem>
#include <functional>
#include <map>
#include <string>
#include <thread>
#include <vector>

namespace lazycmake::fswatch {

enum class FileChangeKind { Added, Removed, Modified };

struct FileChange {
    FileChangeKind kind;
    std::filesystem::path path;
};

class FileClassifier {
public:
    FileClassifier();
    bool isRelevant(const std::filesystem::path& path) const;
    void addPattern(const std::string& pattern);
    void setExcludeDirs(const std::vector<std::string>& dirs);
    bool isExcluded(const std::filesystem::path& path) const;

private:
    std::vector<std::string> relevantExtensions_;
    std::vector<std::string> excludeDirs_;
};

class FileWatcher {
public:
    using Callback = std::function<void(const FileChange&)>;

    explicit FileWatcher(std::filesystem::path watchDir);
    ~FileWatcher();

    void start(Callback callback);
    void stop();
    bool isRunning() const;

    void setDebounceMs(int ms);
    void setClassifier(std::unique_ptr<FileClassifier> classifier);

private:
    void pollLoop();
    void detectChanges(std::map<std::filesystem::path, std::filesystem::file_time_type>& previous,
                       Callback callback);
    std::map<std::filesystem::path, std::filesystem::file_time_type>
        scanDirectory() const;

    std::filesystem::path watchDir_;
    std::unique_ptr<FileClassifier> classifier_;
    std::thread pollThread_;
    bool running_ = false;
    int debounceMs_ = 250;
};

} // namespace lazycmake::fswatch
