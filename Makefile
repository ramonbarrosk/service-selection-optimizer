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

# Fast Local Search (Voudouris/Tsang sub-neighborhood activation-bits) para o
# hot path FIRST_IMPROVEMENT de MOVE/SWAP. Ligado por padrao (comprovado pelo
# A/B de 94 instancias: gap medio 4.04% -> 4.10%, 46->47/94 otimos, tempo-ate-
# -melhor medio 2.22s -> 1.92s, -13.5%; ver data/report_cpp_fls_off.txt e
# data/report_cpp_fls_on.txt). Para voltar ao comportamento antigo: make FLS=0
FLS ?= 1
ifneq ($(FLS),0)
CXXFLAGS += -DENABLE_FLS
endif

# Guided Local Search over task->service assignment features. It is invoked
# after ILS stagnation and as a final intensification step. GLS remains opt-in
# to preserve the historical executable; its promoted settings are the default
# whenever it is enabled with `make GLS=1`.
GLS ?= 0
ifneq ($(GLS),0)
GLS_ALPHA ?= 0.3
GLS_ROUNDS ?= 30
GLS_MAX_ROUNDS ?= 120
GLS_ADAPTIVE ?= 1
GLS_PERSISTENT ?= 1
GLS_GFLS ?= 1

CXXFLAGS += -DENABLE_GLS \
            -DGLS_ALPHA=$(GLS_ALPHA) \
            -DGLS_ROUNDS=$(GLS_ROUNDS) \
            -DGLS_MAX_ROUNDS=$(GLS_MAX_ROUNDS)
ifneq ($(GLS_ADAPTIVE),0)
CXXFLAGS += -DENABLE_ADAPTIVE_GLS_ROUNDS
endif
ifneq ($(GLS_PERSISTENT),0)
CXXFLAGS += -DENABLE_PERSISTENT_GLS
endif
ifneq ($(GLS_GFLS),0)
CXXFLAGS += -DENABLE_GUIDED_FAST_LOCAL_SEARCH
endif
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
