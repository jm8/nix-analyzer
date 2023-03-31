#include "document.h"

// https://github.com/llvm/llvm-project/blob/b8576086c78a5aebf056a8fc8cc716dfee40b72e/clang-tools-extra/clangd/SourceCode.cpp#L171
size_t findStartOfLine(std::string_view content, size_t line0indexed) {
    size_t startOfLine = 0;
    for (size_t i = 0; i < line0indexed; i++) {
        size_t nextNewLine = content.find('\n', startOfLine);
        if (nextNewLine == std::string_view::npos) {
            return 0;
        }
        startOfLine = nextNewLine + 1;
    }
    return startOfLine;
}

size_t positionToOffset(std::string_view content, Position p) {
    if (p.line < 0) {
        return std::string_view::npos;
    }
    if (p.character < 0) {
        return std::string_view::npos;
    }
    return findStartOfLine(content, p.line) + p.character;
}

void Document::applyContentChange(ContentChange contentChange) {
    if (!contentChange.range) {
        source = contentChange.text;
        return;
    }

    auto startIndex = positionToOffset(source, contentChange.range->start);
    auto endIndex = positionToOffset(source, contentChange.range->end);

    // todo: Add range length comparison to ensure the documents in sync
    // like clangd
    source.replace(startIndex, endIndex - startIndex, contentChange.text);
}