#include "lazycmake/cmake_gen/cmake_file.hpp"

namespace lazycmake::cmake_gen {

void CMakeFile::addStatement(CMakeStatement stmt) {
    elements_.push_back(Element{
        .type = ElementType::Statement,
        .comment = {},
        .statement = std::move(stmt),
    });
}

void CMakeFile::addComment(const std::string& text) {
    elements_.push_back(Element{
        .type = ElementType::Comment,
        .comment = text,
        .statement = {},
    });
}

void CMakeFile::addBlankLine() {
    elements_.push_back(Element{
        .type = ElementType::BlankLine,
        .comment = {},
        .statement = {},
    });
}

const std::vector<CMakeFile::Element>& CMakeFile::elements() const {
    return elements_;
}

std::size_t CMakeFile::size() const {
    return elements_.size();
}

bool CMakeFile::empty() const {
    return elements_.empty();
}

void CMakeFile::clear() {
    elements_.clear();
}

} // namespace lazycmake::cmake_gen
