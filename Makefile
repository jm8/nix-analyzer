CFLAGS:=-Isrc -O3 -Wall -std=c++20 $(shell pkg-config --cflags --libs nix-main bdw-gc nlohmann_json) -I$(boostInclude) -DNIX_VERSION=\"$(NIX_VERSION)\" -lnixutil -lnixstore -lnixexpr -lnixmain -lnixcmd -lnixfetchers -lgc
SOURCE:=$(wildcard src/*.cpp) build/lexer-tab.cc build/parser-tab.cc

all: nix-analyzer

build/parser-tab.cc build/parser-tab.hh: src/parser.y
	mkdir -p build
	bison -v -o build/parser-tab.cc $< -d

build/lexer-tab.cc build/lexer-tab.hh: src/lexer.l
	mkdir -p build
	flex --outfile build/lexer-tab.cc --header-file=build/lexer-tab.hh $<

nix-analyzer: $(SOURCE)
	g++ $(CFLAGS) $(SOURCE) -o nix-analyzer

clean:
	rm -rf build
	rm nix-analyzer