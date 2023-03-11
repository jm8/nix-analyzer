CFLAGS+=-Isrc
SOURCE_FILES=$(shell find src -type f -name '*.cpp')

nix-analyzer: ${SOURCE_FILES}
	g++ ${SOURCE_FILES} ${CFLAGS} -o nix-analyzer

clean:
	rm -f nix-analyzer