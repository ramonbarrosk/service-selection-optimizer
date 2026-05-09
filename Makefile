CXX = g++
CXXFLAGS = -std=c++17 -O3 -DNDEBUG -Wall -Wextra \
    -Isrc/basic -Isrc/enum -Isrc/instance -Isrc/util \
    -Isrc/validator -Isrc/search -Isrc/metaheuristic
DEPFLAGS = -MMD -MP
TARGET = build/service-selection-optimizer
DEPFILE = build/main.d

all: $(TARGET)

$(TARGET): src/main.cpp
	@mkdir -p build
	$(CXX) $(CXXFLAGS) $(DEPFLAGS) -MF $(DEPFILE) -o $@ $<

-include $(DEPFILE)

run: $(TARGET)
	@./$(TARGET)

clean:
	rm -rf build

.PHONY: all run clean
