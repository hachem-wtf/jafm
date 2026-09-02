CC := clang

WARNINGS := -Wall -Wextra        \
            -Wshadow -Wpedantic  \
            -Wconversion         \
            -Wstrict-prototypes  \
            -Wmissing-prototypes \
            -Wsign-conversion

CFLAGS := $(WARNINGS) \
          -O2        \
          -pthread   \
          -std=c17

LDFLAGS := -lz \
           -pthread

DEBUGFLAGS := $(WARNINGS) \
              -g         \
              -O0        \
              -pthread   \
              -std=c17   \
              -fno-omit-frame-pointer \
              -fsanitize=address,undefined

DEBUGLDFLAGS := -lz \
                -pthread \
                -fsanitize=address,undefined

CPPCHECKFLAGS := --error-exitcode=1              \
                 --project=compile_commands.json \
                 --enable=warning,style,performance,portability

TARGET := jafm
SRC := jafm.c

PREFIX ?= /usr/local
BINDIR := $(DESTDIR)$(PREFIX)/bin

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $@ $(LDFLAGS)

debug: $(SRC)
	$(CC) $(DEBUGFLAGS) $(SRC) -o $(TARGET) $(DEBUGLDFLAGS)

check:
	bear -- make clean debug
	cppcheck $(CPPCHECKFLAGS)

install: $(TARGET)
	install -d $(BINDIR)
	install -m 755 $(TARGET) $(BINDIR)/$(TARGET)

uninstall:
	rm -f $(BINDIR)/$(TARGET)

clean:
	rm -rf $(TARGET) $(TARGET).dSYM

.PHONY: all debug check install uninstall clean

