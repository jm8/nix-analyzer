CFLAGS:=-O3 -Wall -std=c++20 $(shell pkg-config --cflags --libs nix-main bdw-gc nlohmann_json) -I$(boostInclude) -DNIX_VERSION=\"$(NIX_VERSION)\" -lnixutil -lnixstore -lnixexpr -lnixmain -lnixcmd -lgc

nix-analyzer: src/nix-analyzer.cpp
	g++ $(CFLAGS) src/nix-analyzer.cpp -o nix-analyzer