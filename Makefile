CXX = g++
CXXFLAGS = -std=c++17 -O3 -DNDEBUG -Wall -Wextra \
    -Isrc/basic -Isrc/enum -Isrc/instance -Isrc/util \
    -Isrc/validator -Isrc/search -Isrc/metaheuristic

# O best-fit, a oscilação estratégica e a Fast Local Search (FLS) fazem parte
# permanente do algoritmo. A Guided Local Search (GLS), já acompanhada da GFLS,
# das rodadas adaptativas e das penalidades persistentes, é o único recurso opcional.
GLS ?= 0
ifneq ($(GLS),0)
CXXFLAGS += -DENABLE_GLS
endif

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
