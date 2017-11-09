FILES = `find -name '*.c'`

all: compile run

debug:
	gcc $(FILES)  -Wall -pedantic -pthread -o out

compile:
	gcc $(FILES)   -pthread -o out

run:
	./out
