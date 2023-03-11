CFLAGS+=-Isrc
SOURCE+=src/parser/parser.cpp
SOURCE+=src/parser/tokenizer.cpp
SOURCE+=src/calculateenv/calculateenv.cpp
SOURCE+=src/schema/schema.cpp
SOURCE+=src/common/analysis.cpp
SOURCE+=src/common/position.cpp
OBJ = $(patsubst src/%.cpp,build/%.o,$(SOURCE))

all: nix-analyzer nix-analyzer-test

build/%.o: src/%.cpp
	mkdir -p `dirname $@`
	g++ $(CFLAGS) $< -c -o $@

nix-analyzer: ${OBJ} build/main.o
	g++ ${OBJ} build/main.o ${CFLAGS} -o nix-analyzer

nix-analyzer-test: ${OBJ} build/test.o
	g++ ${OBJ} build/test.o ${CFLAGS} -lgtest -o nix-analyzer-test

clean:
	rm -rf nix-analyzer nix-analyzer-test build