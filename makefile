FILES = `find -name '*.c'`

all: compile run

compile:
	gcc $(FILES) -Wall -pedantic -pthread -o out

run:
	./out
