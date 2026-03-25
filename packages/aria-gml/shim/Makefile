CC      ?= gcc
CFLAGS  := -O2 -Wall -Wextra -fPIC -shared
LDFLAGS := -lraylib -lm

TARGET  := libaria_gml_shim.so
SRC     := aria_gml_shim.c

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f $(TARGET)
