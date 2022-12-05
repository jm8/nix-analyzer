#include "parser.h"
#include "nixexpr.hh"
#include "test.h"
#include <cctype>
#include <filesystem>
#include <fstream>
#include <glob.h>
#include <initializer_list>
#include <iostream>
#include <sstream>
#include <string_view>
#include <unordered_map>
#include <vector>

using namespace std;
using namespace nix;

enum class TokenType {
    IF,
    THEN,
    ELSE,
    ASSERT,
    WITH,
    LET,
    IN,
    REC,
    INHERIT,
    OR_KW,

    ID,

    EQ,
    NEQ,
    LEQ,
    GEQ,
    AND,
    OR,
    IMPL,
    UPDATE,
    CONCAT,
};

unordered_map<string_view, TokenType> keywords{
    {"if", TokenType::IF},           {"then", TokenType::THEN},
    {"else", TokenType::ELSE},       {"assert", TokenType::ASSERT},
    {"with", TokenType::WITH},       {"let", TokenType::LET},
    {"in", TokenType::IN},           {"rec", TokenType::REC},
    {"inherit", TokenType::INHERIT}, {"or", TokenType::OR_KW},
};

map<string_view, TokenType> symbols{
    {"==", TokenType::EQ},     {"!=", TokenType::NEQ},
    {"<=", TokenType::LEQ},    {">=", TokenType::GEQ},
    {"&&", TokenType::AND},    {"||", TokenType::OR},
    {"->", TokenType::IMPL},   {"//", TokenType::UPDATE},
    {"++", TokenType::CONCAT},
};

struct Token {
    string_view source;
    Pos pos;
    TokenType type;
};

class Lexer {
  private:
    Token token;
    size_t index;
    Pos pos;
    string_view source;

  public:
    Lexer(string_view source) : index(0), pos(), source(source) {
        advanceToken();
    }

    Token peek() {
        return token;
    }

    Token consume() {
        Token res = peek();
        advanceToken();
        return res;
    }

  private:
    char peekChar() {
        return source[index];
    }

    char advanceChar() {
        char curr = peekChar();
        index++;
        if (curr == '\n') {
            pos.line++;
            pos.column = 1;
        } else {
            pos.column++;
        }
        return curr;
    }

    void advanceToken() {
        skipWhitespace();
        token.pos = pos;
        // Symbol
        for (auto [symbol, type] : symbols) {
            string_view substring = source.substr(index, symbol.length());
            if (substring == symbol) {
                token.type = type;
                token.source = substring;
                for (size_t i = 0; i < symbol.length(); i++)
                    advanceChar();
                return;
            }
        }
        // ID or keyword
        if (isIdStartChar(peekChar())) {
            size_t start = index;
            advanceChar();
            while (isIdChar(peekChar())) {
                advanceChar();
            }
            size_t end = index;
            token.source = string_view(source.begin() + start, end - start);
            auto it = keywords.find(token.source);
            if (it != keywords.end()) {
                token.type = it->second;
            } else {
                token.type = TokenType::ID;
            }
            return;
        }
    }

    void skipWhitespace() {
        while (isspace(peekChar()))
            advanceChar();
    }

    bool isIdStartChar(char c) {
        return isalpha(c) || c == '_';
    }

    bool isIdChar(char c) {
        return isalpha(c) || isdigit(c) || c == '_' || c == '-' || c == '\'';
    }
};

struct Parser {
    vector<ParseDiagnostic> &diagnostics;
    Token token;
};

Expr *parse(string_view s, vector<ParseDiagnostic> &diagnostics) {
    return nullptr;
};

initializer_list<pair<string_view, vector<TokenType>>> lexTests{
    {"if", {TokenType::IF}},
    {">=", {TokenType::GEQ}},
    {"if x == y", {TokenType::IF, TokenType::ID, TokenType::EQ, TokenType::ID}},
};

bool runLexTest(string_view source, vector<TokenType> expectedTypes) {
    Lexer lexer(source);
    bool good = true;
    for (auto expectedType : expectedTypes) {
        if (lexer.consume().type != expectedType) {
            good = false;
        }
    }
    if (good) {
        cout << "PASS ";
    } else {
        cout << "FAIL ";
    }
    cout << source << "\n";
    return good;
}

bool runLexTests() {
    int successes = 0;
    int total = 0;

    for (auto [source, expectedTypes] : lexTests) {
        successes += runLexTest(source, expectedTypes);
        total++;
    }
    cout << successes << " / " << total << endl;
    return successes == total;
}

bool runParseTests() {
    int successes = 0;
    int total = 0;
    for (const auto &entry : filesystem::directory_iterator("test")) {
        ifstream f(entry.path().c_str());
        stringstream ss;
        ss << f.rdbuf();
        vector<ParseDiagnostic> diagnostics;
        Expr *e = parse(ss.str(), diagnostics);
        bool good = diagnostics.empty() && e != nullptr;
        if (good) {
            cout << "PASS ";
            successes++;
        } else {
            cout << "FAIL ";
        }
        total++;
        cout << entry.path().c_str() << "\n";
    }
    cout << successes << " / " << total << endl;
    return successes == total;
}