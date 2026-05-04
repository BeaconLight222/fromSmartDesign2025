CC = gcc
CFLAGS = -Wall -Wextra -g
TARGET = main
SRC = main.c
TINYMAIX_SRC = $(wildcard ../src/TinyMaix/src/*.c)
MLX90640_SRC = $(wildcard ../src/mlx90640YoloProcessor/*.c)

all: $(TARGET)

$(TARGET): $(SRC) $(TINYMAIX_SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(TINYMAIX_SRC) $(MLX90640_SRC)

clean:
	rm -f $(TARGET)