CFLAGS:=-Isrc -Ibuild -O3 -Wall $(shell pkg-config --cflags --libs bdw-gc nlohmann_json nix-main) -I$(boostInclude) -L$(boostLib) -I$(lspcpp)/include -lnixutil -lnixstore -lnixexpr -lnixmain -lnixcmd -lnixfetchers -lgc -lboost_filesystem
SOURCE:=src/nix-analyzer.cpp
OBJ:=$(patsubst src/%.cpp,build/%.o,$(SOURCE))

all: nix-analyzer-test nix-analyzer

build/%.o: src/%.cpp
	mkdir -p build
	g++ $(CFLAGS) $< -c -o $@

nix-analyzer-test: $(OBJ) build/test.o
	g++ $(CFLAGS) $(OBJ) build/test.o -o nix-analyzer-test

nix-analyzer: $(OBJ) build/main.o
	g++ $(CFLAGS) $(OBJ) build/main.o $(lspcpp)/lib/liblspcpp.a $(lspcpp)/lib/libnetwork-uri.a -o nix-analyzer

nix/config.h:
	cd nix && ./bootstrap.sh && ./configure --prefix=$$PWD/build

clean:
	rm -rf build
	rm nix-analyzer-test
	$(MAKE) -C nix clean