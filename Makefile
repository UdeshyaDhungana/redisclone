# Compiler and flags
CC = gcc
CFLAGS = -Wall -g

# Source files
SRC = $(wildcard app/*.c)

# Object files (replace .c with .o for each source file)
OBJ = $(SRC:.c=.o)

# Output executable
TARGET = redis

# Default target
master: $(TARGET)
	./redis --dir /home/udeshya --dbfilename empty.rdb

slave: $(TARGET)
	./redis --dir /home/udeshya --replicaof "localhost 6379" --port 10000

# Rule to build the executable by linking object files
$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJ)
	rm -f $(OBJ)

# Rule to build object files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up object files and the executable
clean:
	rm -f $(OBJ) $(TARGET)


# Phony targets (not real files)
.PHONY: server clean

