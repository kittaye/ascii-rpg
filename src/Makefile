CFLAGS=-std=gnu99 -Wall -Wextra -Wfloat-equal -Wundef -Wcast-align -Wwrite-strings -Wlogical-op -Wmissing-declarations -Wredundant-decls -Wshadow -g
LIBS=-lncurses -lm
SRC=main.c ascii_game.c george_graphics.c coord.c items.c enemies.c tiles.c
DST=ascii_game

all: ascii_game

ascii_game: $(SRC)
	gcc $(CFLAGS) $(SRC) -o $(DST) $(LIBS)

clean:
	rm *.o
	rm *.exe
