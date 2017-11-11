FILES = `find -name '*.c'`

all: compile run

debug:
	gcc $(FILES)  -Wall -pedantic -pthread -o out

compile:
	gcc $(FILES)   -pthread -o homework2

run:
	# Run ./homework2 <filename> to run the program
	# Once the program is started you will be prompted to select either SPF or least laxity first
	# The simulation appears to be stuck at the very end but it is not, after hitting enter the program will exit
