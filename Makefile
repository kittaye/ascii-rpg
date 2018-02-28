CFLAGS=-std=gnu99 -Wall -Wextra -Wfloat-equal -Wundef -Wcast-align -Wwrite-strings -Wlogical-op -Wmissing-declarations -Wredundant-decls -Wshadow -g
LIBS=-lncurses -lm
SRC=src/ascii_game.c src/george_graphics.c src/coord.c src/items.c src/enemies.c
DST=ascii_game

all: ascii_game

ascii_game: $(SRC)
	gcc $(CFLAGS) $(SRC) -o $(DST) $(LIBS)

clean:
	rm *.o
	rm *.exe
