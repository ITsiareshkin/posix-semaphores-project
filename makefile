flags=-std=gnu99 -Wall -Wextra -Werror -pedantic

proj2: proj2.o
	gcc $(flags) proj2.o -pthread -o proj2

proj2.o: proj2.c
	gcc $(flags) -c proj2.c
