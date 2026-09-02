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
          -std=c17

LDFLAGS := -fsanitize=address,undefined \
           -lz

CPPCHECKFLAGS := --project=compile_commands.json \
                 --enable=warning,style,performance,portability \
                 --error-exitcode=1

TARGET := jafm
SRC := jafm.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $@ $(LDFLAGS)

check:
	bear -- make clean all
	cppcheck $(CPPCHECKFLAGS)

clean:
	rm -rf $(TARGET) $(TARGET).dSYM

.PHONY: all check clean
