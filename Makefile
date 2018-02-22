TARGETS=ascii_game

LIBS=-lncurses -lm
FLAGS=-std=gnu99 -Wall -Werror -g

all: $(TARGETS)

ascii_game: ascii_game.c george_graphics.c items.c enemies.c
	gcc $(FLAGS) ascii_game.c george_graphics.c items.c enemies.c -o ascii_game $(LIBS) 


clean:
	for f in $(TARGETS); do \
		if [ -f $$f ]; then rm $$f; fi; \
		if [ -f $$f.exe ]; then rm $$f.exe; fi; \
	done

rebuild: clean all