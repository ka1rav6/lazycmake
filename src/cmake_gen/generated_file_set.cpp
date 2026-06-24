#include "lazycmake/cmake_gen/generated_file_set.hpp"

namespace lazycmake::cmake_gen {

void GeneratedFileSet::addFile(std::filesystem::path relativePath, std::string content) {
    // Normalize to forward slashes for cross-platform consistency.
    // On Windows, std::filesystem::path might use backslashes; CMake
    // always expects forward slashes in its file paths. Using
    // make_preferred() followed by generic_string() would be ideal but
    // we keep paths in native format internally and convert at render.
    // Since we're storing keys as std::filesystem::path, comparison
    // is platform-normalized automatically by the path class.
    files_[std::move(relativePath)] = std::move(content);
}

bool GeneratedFileSet::contains(const std::filesystem::path& relativePath) const {
    return files_.contains(relativePath);
}

const std::string* GeneratedFileSet::find(const std::filesystem::path& relativePath) const {
    auto it = files_.find(relativePath);
    if (it != files_.end()) {
        return &it->second;
    }
    return nullptr;
}

std::size_t GeneratedFileSet::size() const {
    return files_.size();
}

const std::map<std::filesystem::path, std::string>& GeneratedFileSet::files() const {
    return files_;
}

void GeneratedFileSet::clear() {
    files_.clear();
}

bool GeneratedFileSet::empty() const {
    return files_.empty();
}

} // namespace lazycmake::cmake_gen
