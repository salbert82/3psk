#pragma once
/*
	3psk - 3-pole Phase Shift Keying
	
	Copyright (c) Edward Cree, 2012
	Licensed under the GNU GPL v3+
	
	strbuf: auto-reallocating string buffers
*/
#include <stdio.h>

char *fgetl(FILE *); // gets a line of string data; returns a malloc-like pointer
char *slurp(FILE *); // gets an entire file of string data; returns a malloc-like pointer
void init_char(char **buf, size_t *l, size_t *i); // initialises a string buffer in heap.  *buf becomes a malloc-like pointer
void append_char(char **buf, size_t *l, size_t *i, char c); // adds a character to a string buffer in heap (and realloc()s if needed)
void append_str(char **buf, size_t *l, size_t *i, const char *str); // adds a string to a string buffer in heap (and realloc()s if needed)
