CFLAGS+=-g -Isrc
SOURCE_FILES=src/parser/parser.cpp src/calculateenv/calculateenv.cpp src/main.cpp

nix-analyzer: ${SOURCE_FILES}
	g++ ${CFLAGS} ${SOURCE_FILES} -o nix-analyzer

clean:
	rm -f nix-analyzer