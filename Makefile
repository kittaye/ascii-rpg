FLAGS=-std=gnu99 -Wall -Werror -g
LIBS=-lncurses -lm
SRC=ascii_game.c george_graphics.c coord.c items.c enemies.c
DST=ascii_game

all:
	gcc $(FLAGS) $(SRC) -o $(DST) $(LIBS) 


clean:
	rm *.exe

rebuild: clean all