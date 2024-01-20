#include "documents/documents.h"
#include "input-accessor.hh"
#include "parser/parser.h"

Range Document::tokenRangeToRange(TokenRange tokenRange) {
    return {
        tokens[tokenRange.start].range.start, tokens[tokenRange.end].range.end};
}
