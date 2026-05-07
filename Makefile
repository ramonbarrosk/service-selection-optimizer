CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra \
    -Isrc/basic -Isrc/enum -Isrc/instance -Isrc/util \
    -Isrc/validator -Isrc/search -Isrc/metaheuristic
TARGET = build/service-selection-optimizer

all: $(TARGET)

$(TARGET): src/main.cpp
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -o $@ $<

run: $(TARGET)
	@./$(TARGET)

clean:
	rm -rf build

.PHONY: all run clean
