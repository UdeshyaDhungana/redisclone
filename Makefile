# Compiler and flags
CC = gcc
CFLAGS = -Wall -g

# Source files
SRC = parser.c main.c

# Object files (replace .c with .o for each source file)
OBJ = $(SRC:.c=.o)

# Output executable
TARGET = redis

# Default target
all: $(TARGET)

# Rule to build the executable by linking object files
$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJ)
	./$(TARGET)

# Rule to build object files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up object files and the executable
clean:
	rm -f $(OBJ) $(TARGET)

# Phony targets (not real files)
.PHONY: all clean

