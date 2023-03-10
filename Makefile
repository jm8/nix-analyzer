CFLAGS+=-g -Isrc
SOURCE_FILES=src/calculateenv/calculateenv.cpp src/common/analysis.cpp src/common/allocvalue.cpp src/parser/parser.cpp src/schema/schema.cpp src/main.cpp


nix-analyzer: ${SOURCE_FILES}
	g++ ${SOURCE_FILES} ${CFLAGS} -o nix-analyzer

clean:
	rm -f nix-analyzer