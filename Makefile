CC := clang

CFLAGS := -Wall \
          -Wextra \
          -Wpedantic \
          -Wconversion \
          -Wsign-conversion \
          -Wshadow \
          -Wstrict-prototypes \
          -Wmissing-prototypes \
          -g \
          -fno-omit-frame-pointer \
          -fsanitize=address,undefined \
          -std=c17 \
          -Isrc

LDFLAGS := -fsanitize=address,undefined \
           -lz

CPPCHECKFLAGS := --project=compile_commands.json \
                 --enable=warning,style,performance,portability \
                 --error-exitcode=1

TARGET := bin/jafm

SRC := $(shell find src -name '*.c')
OBJ := $(SRC:src/%.c=bin/%.o)

all: $(TARGET)

$(TARGET): $(OBJ)
	@mkdir -p $(@D)
	$(CC) $^ -o $@ $(LDFLAGS)

bin/%.o: src/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

check:
	bear -- make clean all
	cppcheck $(CPPCHECKFLAGS)

clean:
	rm -rf bin

.PHONY: all check clean
