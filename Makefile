# Contrapunctus Universalis
# Une seule cible : le noyau. gcc -std=c11, aucune dependance externe.

CC      ?= gcc
CFLAGS  ?= -std=c11 -O2 -Wall -Wextra
LDLIBS   = -lm

SRC = src/main.c src/voices.c src/synth.c src/wav.c
HDR = src/cu.h

cu-render: $(SRC) $(HDR)
	$(CC) $(CFLAGS) -o $@ $(SRC) $(LDLIBS)

clean:
	rm -f cu-render

.PHONY: clean
