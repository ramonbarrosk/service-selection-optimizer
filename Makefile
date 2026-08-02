CXX = g++
CXXFLAGS = -std=c++17 -O3 -DNDEBUG -Wall -Wextra \
    -Isrc/basic -Isrc/enum -Isrc/instance -Isrc/util \
    -Isrc/validator -Isrc/search -Isrc/metaheuristic

# Híbrido proposta (1)+(2): construtivo best-fit + busca por oscilação estratégica.
# Ligado por padrão (comprovado pelo A/B: gap médio 36.6% -> 17.2% nas difíceis).
# Para voltar ao baseline original:  make OSC=0
OSC ?= 1
ifneq ($(OSC),0)
CXXFLAGS += -DENABLE_OSCILLATION
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
