CXX ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wpedantic
CPPFLAGS ?= -Isrc
LDFLAGS ?=

BIN := build/studentmachine
OBJ_DIR := build/obj
SOURCES := $(shell find src -name '*.cpp')
OBJECTS := $(patsubst src/%.cpp,$(OBJ_DIR)/%.o,$(SOURCES))
DEPS := $(OBJECTS:.o=.d)

.PHONY: all clean debug run test

all: $(BIN)

$(BIN): $(OBJECTS)
	@mkdir -p $(dir $@)
	$(CXX) $(OBJECTS) -o $@ $(LDFLAGS)
	@echo "Built $(BIN)"

$(OBJ_DIR)/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c $< -o $@

debug: CXXFLAGS := -std=c++17 -O0 -g -Wall -Wextra -Wpedantic \
	-fsanitize=address,undefined -fno-omit-frame-pointer
debug: LDFLAGS := -fsanitize=address,undefined
debug: clean all

run: all
	./$(BIN)

test: all
	bash tests/smoke.sh

clean:
	rm -rf build/obj
	rm -f $(BIN)

-include $(DEPS)
