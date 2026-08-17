CXX = g++
CXXFLAGS = -std=c++17 -O3 -DNDEBUG -Wall -Wextra \
    -Isrc/basic -Isrc/enum -Isrc/instance -Isrc/util \
    -Isrc/validator -Isrc/search -Isrc/metaheuristic

# Proposta 1: construtivo best-fit. Separado da oscilacao para permitir
# ablacoes justas como BEST_FIT=1 OSC=0 GLS=1.
BEST_FIT ?= 1
ifneq ($(BEST_FIT),0)
CXXFLAGS += -DENABLE_BEST_FIT
endif

# Proposta 2: busca por oscilacao estrategica. Para desativa-la sem remover o
# best-fit: make BEST_FIT=1 OSC=0
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
GLS_FINAL_MIN_GAP ?= 0.01
GLS_CAPACITY_WEIGHT ?= 0.0
GLS_REGRET_WEIGHT ?= 0.0
GLS_EJECTION_CHAIN ?= 0
GLS_PARTIAL_RECONSTRUCTION ?= 0
GLS_RECONSTRUCTION_SIZE ?= 3
GLS_RECONSTRUCTION_PERIOD ?= 5
PATH_RELINKING ?= 0
PATH_RELINKING_POOL ?= 5
PATH_RELINKING_MIN_GAP ?= 0.05

CXXFLAGS += -DENABLE_GLS \
            -DGLS_ALPHA=$(GLS_ALPHA) \
            -DGLS_ROUNDS=$(GLS_ROUNDS) \
            -DGLS_MAX_ROUNDS=$(GLS_MAX_ROUNDS) \
            -DGLS_FINAL_MIN_GAP=$(GLS_FINAL_MIN_GAP) \
            -DGLS_CAPACITY_WEIGHT=$(GLS_CAPACITY_WEIGHT) \
            -DGLS_REGRET_WEIGHT=$(GLS_REGRET_WEIGHT)
ifneq ($(GLS_ADAPTIVE),0)
CXXFLAGS += -DENABLE_ADAPTIVE_GLS_ROUNDS
endif
ifneq ($(GLS_PERSISTENT),0)
CXXFLAGS += -DENABLE_PERSISTENT_GLS
endif
ifneq ($(GLS_GFLS),0)
CXXFLAGS += -DENABLE_GUIDED_FAST_LOCAL_SEARCH
endif
ifneq ($(GLS_EJECTION_CHAIN),0)
CXXFLAGS += -DENABLE_GLS_EJECTION_CHAIN
endif
ifneq ($(GLS_PARTIAL_RECONSTRUCTION),0)
CXXFLAGS += -DENABLE_GLS_PARTIAL_RECONSTRUCTION \
            -DGLS_RECONSTRUCTION_SIZE=$(GLS_RECONSTRUCTION_SIZE) \
            -DGLS_RECONSTRUCTION_PERIOD=$(GLS_RECONSTRUCTION_PERIOD)
endif
ifneq ($(PATH_RELINKING),0)
CXXFLAGS += -DENABLE_PATH_RELINKING \
            -DPATH_RELINKING_POOL=$(PATH_RELINKING_POOL) \
            -DPATH_RELINKING_MIN_GAP=$(PATH_RELINKING_MIN_GAP)
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

# Variante experimental validada em data/gls_structural_reform_report.md.
# Ela preserva a funcao objetivo do GLS e muda apenas a utilidade usada para
# escolher quais features recebem penalidade.
build-gls-structural:
	$(MAKE) -B GLS=1 GLS_ALPHA=1.0 GLS_CAPACITY_WEIGHT=2.0 GLS_REGRET_WEIGHT=0.5 all

run-gls-structural:
	$(MAKE) -B GLS=1 GLS_ALPHA=1.0 GLS_CAPACITY_WEIGHT=2.0 GLS_REGRET_WEIGHT=0.5 all
	@./$(TARGET)

# GLS estrutural com cadeia de duas realocacoes A->B e B->C.
build-gls-ejection-chain:
	$(MAKE) -B GLS=1 GLS_ALPHA=1.0 GLS_CAPACITY_WEIGHT=2.0 GLS_REGRET_WEIGHT=0.5 GLS_EJECTION_CHAIN=1 all

run-gls-ejection-chain:
	$(MAKE) -B GLS=1 GLS_ALPHA=1.0 GLS_CAPACITY_WEIGHT=2.0 GLS_REGRET_WEIGHT=0.5 GLS_EJECTION_CHAIN=1 all
	@./$(TARGET)

# GLS estrutural com destroy-and-repair exato de um pequeno grupo de tarefas.
build-gls-partial-reconstruction:
	$(MAKE) -B GLS=1 GLS_ALPHA=1.0 GLS_CAPACITY_WEIGHT=2.0 GLS_REGRET_WEIGHT=0.5 GLS_PARTIAL_RECONSTRUCTION=1 all

run-gls-partial-reconstruction:
	$(MAKE) -B GLS=1 GLS_ALPHA=1.0 GLS_CAPACITY_WEIGHT=2.0 GLS_REGRET_WEIGHT=0.5 GLS_PARTIAL_RECONSTRUCTION=1 all
	@./$(TARGET)

# GLS estrutural com conjunto elite e path relinking apos estagnacao.
build-gls-path-relinking:
	$(MAKE) -B GLS=1 GLS_ALPHA=1.0 GLS_CAPACITY_WEIGHT=2.0 GLS_REGRET_WEIGHT=0.5 PATH_RELINKING=1 all

run-gls-path-relinking:
	$(MAKE) -B GLS=1 GLS_ALPHA=1.0 GLS_CAPACITY_WEIGHT=2.0 GLS_REGRET_WEIGHT=0.5 PATH_RELINKING=1 all
	@./$(TARGET)

clean:
	rm -rf build

.PHONY: all run build-gls-structural run-gls-structural \
	build-gls-ejection-chain run-gls-ejection-chain \
	build-gls-partial-reconstruction run-gls-partial-reconstruction \
	build-gls-path-relinking run-gls-path-relinking clean
