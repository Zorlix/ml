CC = clang++
CPPFLAGS = -Wall -std=c++17 -g -ggdb
DEBUGFLAGS = -Wall -fsanitize=address -fno-omit-frame-pointer -std=c++17 -g -ggdb
HEADERS = ml.h tensor.h
OBJECTS = train.cpp
TESTS = tests.cpp

all: test

ml: $(OBJECTS) $(HEADERS)
	$(CC) $(CPPFLAGS) $(OBJECTS) -o ml

debug: $(OBJECTS) $(HEADERS)
	$(CC) $(DEBUGFLAGS) $(OBJECTS) -o debug

test: $(TESTS) $(HEADERS)
	$(CC) $(CPPFLAGS) $(TESTS) -o test
