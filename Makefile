CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra
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
