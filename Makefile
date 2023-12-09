# Makefile for the FAT32 file system project

# Compiler settings
CC = gcc
CFLAGS = -Wall -g

# Target executable name
TARGET = filesys

# Source files
SOURCES = filesys.c
OBJECTS = $(SOURCES:.c=.o)

# Build the project
all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $<

# Clean up
clean:
	rm -f $(TARGET) $(OBJECTS)

# Run the program with fat32.img
run: $(TARGET)
	./$(TARGET) fat32.img
