CC ?= clang
CFLAGS := -O3 -ffast-math

BIN := doc-converter
SRC := src/doc-converter.c

# Optional: zlib for DOCX/ODT/EPUB support
HAVE_ZLIB := $(shell pkg-config --exists zlib 2>/dev/null && echo 1)
ifeq ($(HAVE_ZLIB),1)
  CFLAGS += -DHAVE_ZLIB
  LDFLAGS += -lz
else
  $(info NOTE: zlib not found; doc-converter compiled without DOCX/ODT/EPUB support)
endif

# Optional: Pango/Cairo for embedded Unicode PDF fonts
HAVE_PANGOCAIRO := $(shell pkg-config --exists pangocairo 2>/dev/null && echo 1)
ifeq ($(HAVE_PANGOCAIRO),1)
  CFLAGS += -DHAVE_PANGOCAIRO $(shell pkg-config --cflags pangocairo)
  LDFLAGS += $(shell pkg-config --libs pangocairo)
else
  $(info NOTE: pangocairo not found; PDF output falls back to the basic built-in renderer)
endif

all: $(BIN)

$(BIN): $(SRC) $(wildcard src/lib/*.h) $(wildcard src/lookup/*.h)
	$(CC) $(CFLAGS) $(SRC) -o $@ $(LDFLAGS)
	strip $@

install: $(BIN)
	install -d ~/.local/bin
	install -m 755 $(BIN) ~/.local/bin/

clean:
	rm -f $(BIN)

.PHONY: all install clean
