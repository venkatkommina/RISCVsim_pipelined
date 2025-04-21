# Makefile for CS204_Phase3 simulator

# Compiler
CC = g++

# Compiler flags
CFLAGS = -c -Wall

# Target executable
TARGET = ../bin/myRISCVSim

# Object files
OBJECTS = main.o pipeline.o

# Default target
all: $(TARGET)

# Link object files to create executable
$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(TARGET)

# Compile source files to object files
main.o: main.cpp
	$(CC) $(CFLAGS) main.cpp

pipeline.o: pipeline.cpp
	$(CC) $(CFLAGS) pipeline.cpp

# Clean up
clean:
	rm -f *.o $(TARGET)

.PHONY: all clean