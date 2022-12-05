CFLAGS:=-O3 -Wall -std=c++20 $(shell pkg-config --cflags --libs nix-main bdw-gc nlohmann_json) -I$(boostInclude) -DNIX_VERSION=\"$(NIX_VERSION)\" -lnixutil -lnixstore -lnixexpr -lnixmain -lnixcmd -lgc
SOURCE:=src/nix-analyzer.cpp src/parser.cpp src/test.cpp

nix-analyzer: $(SOURCE)
	g++ $(CFLAGS) $(SOURCE) -o nix-analyzer