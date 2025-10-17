CXX = gcc
CXXFLAGS = 
SRC_DIR = src
BIN_DIR = bin

CHAT_SRC = $(SRC_DIR)/personas-chat.c
CHAT_BIN = $(BIN_DIR)/personas-chat

TARGETS = $(CHAT_BIN)

all: $(TARGETS)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(CHAT_BIN): $(CHAT_SRC) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $< -o $@

run: $(CHAT_BIN)
	@$(CHAT_BIN)

clean:
	rm -rf $(BIN_DIR)

.PHONY: all clean run