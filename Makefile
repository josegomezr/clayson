.PHONY: main

main: main.c
	$(CC) main.c -o main -Wall -Wextra -Wno-unknown-pragmas -Wno-missing-field-initializers -Wno-implicit-fallthrough -Wno-pedantic -std=c99
