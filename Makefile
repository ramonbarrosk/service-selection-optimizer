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
EXPERIMENT_TARGET = build/compare-ils-gls
SOFT_SLA_EXPERIMENT_TARGET = build/compare-gls-soft-sla

all: $(TARGET)

$(TARGET): src/main.cpp
	@mkdir -p build
	$(CXX) $(CXXFLAGS) $(DEPFLAGS) -MF $(DEPFILE) -o $@ $<

-include $(DEPFILE)

run: $(TARGET)
	@./$(TARGET)

clean:
	rm -rf build

experiment-ils-gls: experiments/compare_ils_gls.cpp
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -DENABLE_GLS -o $(EXPERIMENT_TARGET) $<

experiment-gls-soft-sla: experiments/compare_gls_soft_sla.cpp
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -DENABLE_GLS -o $(SOFT_SLA_EXPERIMENT_TARGET) $<

summary-gls-soft-sla:
	python3 scripts/summarize_gls_soft_sla_experiment.py

images-ils-gls:
	python3 scripts/plot_ils_gls_experiment.py

image-gls-soft-sla-difficult:
	python3 scripts/plot_gls_soft_sla_difficult.py

.PHONY: all run clean experiment-ils-gls experiment-gls-soft-sla \
	summary-gls-soft-sla images-ils-gls image-gls-soft-sla-difficult
