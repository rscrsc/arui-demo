CC=cc
CFLAGS=-O2 -Wall -Wextra -Iinclude
OUT_DIR=out
TARGET=$(OUT_DIR)/app

all: $(TARGET)

$(TARGET): src/main.c | $(OUT_DIR)
	$(CC) $(CFLAGS) -o $(TARGET) src/main.c -lm

$(OUT_DIR):
	mkdir -p $(OUT_DIR)

run: $(TARGET)
	websocketd --port=8080 --staticdir=ui $(TARGET)

clean:
	rm -rf $(OUT_DIR) plot.bmp

.PHONY: all run clean
