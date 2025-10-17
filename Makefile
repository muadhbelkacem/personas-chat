# Makefile

CXX = gcc
CXXFLAGS = 
SRC_DIR = src
BIN_DIR = bin

CHAT_SRC = $(SRC_DIR)/personas-chat.c

CHAT_BIN = $(BIN_DIR)/personas-chat

TARGETS = $(CHAT_BIN) 

all: $(TARGETS)

# Ensure bin directory exists
$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# Build personas-chat
$(CHAT_BIN): $(CHAT_SRC) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $< -o $@

clean:
	rm -rf $(BIN_DIR)

.PHONY: all clean
