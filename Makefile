CFLAGS:=-Isrc -Ibuild -O3 -Wall
CFLAGS+=$(shell pkg-config --cflags --libs bdw-gc nlohmann_json nix-main) -lnixutil -lnixstore -lnixexpr -lnixmain -lnixcmd -lnixfetchers -lgc
CFLAGS+=-I$(boostInclude) -L$(boostLib) -lboost_filesystem
CFLAGS+=-I$(lspcpp)/include
HEADERS:=src/nix-analyzer.h src/logger.h src/mkderivation-schema.h
SOURCE:=src/nix-analyzer.cpp src/logger.cpp
OBJ:=$(patsubst src/%.cpp,build/%.o,$(SOURCE))

all: nix-analyzer-test nix-analyzer

src/mkderivation-schema.h: src/gen_mkderivation_schema.py
	python3 $< $(nixpkgs) > $@

build/%.o: src/%.cpp $(HEADERS)
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
	rm -f nix-analyzer nix-analyzer-test
	rm -f src/mkderivation-schema.h
	$(MAKE) -C nix clean