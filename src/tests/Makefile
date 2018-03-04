CFLAGS=-std=gnu99 -Wall -g
LIBS=-lncurses -lm
SRC=tests.c ../ascii_game.c ../george_graphics.c ../coord.c ../items.c ../enemies.c
DST=tests

all: tests

tests: $(SRC)
	gcc $(CFLAGS) $(SRC) -o $(DST) $(LIBS)

clean:
	rm *.o
	rm *.exe
