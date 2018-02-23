FLAGS=-std=gnu99 -Wall -Wextra -Wfloat-equal -Wundef -Wcast-align -Wwrite-strings -Wlogical-op -Wmissing-declarations -Wredundant-decls -Wshadow -g
LIBS=-lncurses -lm
SRC=ascii_game.c george_graphics.c coord.c items.c enemies.c
DST=ascii_game

all:
	gcc $(FLAGS) $(SRC) -o $(DST) $(LIBS) 


clean:
	rm *.exe

rebuild: clean all