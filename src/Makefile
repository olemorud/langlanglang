
CC = gcc
CFLAGS = -Wall -Wextra -g -O0

lang : parser.c tokenizer.c error.c file_stream.c | tokenizer.h error.h common.h file_stream.h
	$(CC) $(CFLAGS) -o $@ $^
