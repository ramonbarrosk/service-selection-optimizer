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
GLS_IMPROVEMENTS_TARGET = build/compare-gls-improvements

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

experiment-gls-improvements: experiments/compare_gls_improvements.cpp
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -DENABLE_GLS -o $(GLS_IMPROVEMENTS_TARGET) $<

run-gls-improvements: experiment-gls-improvements
	./$(GLS_IMPROVEMENTS_TARGET)
	python3 scripts/plot_gls_improvements.py

summary-gls-soft-sla:
	python3 scripts/summarize_gls_soft_sla_experiment.py

images-ils-gls:
	python3 scripts/plot_ils_gls_experiment.py

image-gls-soft-sla-difficult:
	python3 scripts/plot_gls_soft_sla_difficult.py

images-gls-improvements:
	python3 scripts/plot_gls_improvements.py

run-gls-time-curve: experiment-gls-improvements
	SSO_FACTORIAL=1 SSO_REPETITIONS=1 SSO_VARIANTS=i1_s1_l0_r1 \
	SSO_TIME_CURVE=0.55,2,5,10,30 \
	SSO_EXPERIMENT_OUTPUT=data/experiments/gls_time_curve_best_factorial_difficult5.csv \
	./$(GLS_IMPROVEMENTS_TARGET)
	python3 scripts/plot_gls_time_curve.py \
	--csv data/experiments/gls_time_curve_best_factorial_difficult5.csv \
	--variant i1_s1_l0_r1 \
	--output-dir data/charts_gls_time_curve_best_factorial \
	--summary data/experiments/gls_time_curve_best_factorial_difficult5_summary.md

run-gls-factorial: experiment-gls-improvements
	SSO_FACTORIAL=1 SSO_REPETITIONS=3 SSO_TIME_SECONDS=0.55 \
	SSO_EXPERIMENT_OUTPUT=data/experiments/gls_factorial_difficult5.csv \
	./$(GLS_IMPROVEMENTS_TARGET)
	python3 scripts/plot_gls_factorial.py

run-gls-promoted-all: experiment-gls-improvements
	SSO_FACTORIAL=1 SSO_INSTANCE_SET=all SSO_REPETITIONS=3 \
	SSO_VARIANTS=i1_s1_l0_r1 SSO_TIME_SECONDS=0.55 \
	SSO_EXPERIMENT_OUTPUT=data/experiments/gls_promoted_all94.csv \
	./$(GLS_IMPROVEMENTS_TARGET)
	python3 scripts/plot_gls_all_instances.py

run-gls-promoted-all-30s: experiment-gls-improvements
	SSO_FACTORIAL=1 SSO_INSTANCE_SET=all SSO_REPETITIONS=3 \
	SSO_VARIANTS=i1_s1_l0_r1 SSO_TIME_SECONDS=30 \
	SSO_EXPERIMENT_OUTPUT=data/experiments/gls_promoted_all94_30s.csv \
	./$(GLS_IMPROVEMENTS_TARGET)
	python3 scripts/plot_gls_all_instances.py \
	--csv data/experiments/gls_promoted_all94_30s.csv \
	--output-dir data/charts_gls_promoted_all94_30s \
	--summary data/experiments/gls_promoted_all94_30s_summary.md

run-gls-remaining-time: experiment-gls-improvements
	SSO_FACTORIAL=1 SSO_INSTANCES=11,28,100,128,147 SSO_REPETITIONS=1 \
	SSO_VARIANTS=i1_s1_l0_r1 SSO_TIME_CURVE=2,5,10,30 \
	SSO_EXPERIMENT_OUTPUT=data/experiments/gls_time_curve_remaining5_stage1.csv \
	./$(GLS_IMPROVEMENTS_TARGET)
	SSO_FACTORIAL=1 SSO_INSTANCES=100 SSO_REPETITIONS=1 \
	SSO_VARIANTS=i1_s1_l0_r1 SSO_TIME_CURVE=60,120 \
	SSO_EXPERIMENT_OUTPUT=data/experiments/gls_time_curve_instance100_stage2.csv \
	./$(GLS_IMPROVEMENTS_TARGET)
	python3 scripts/plot_gls_remaining_time.py

.PHONY: all run clean experiment-ils-gls experiment-gls-soft-sla \
	experiment-gls-improvements run-gls-improvements summary-gls-soft-sla \
	images-ils-gls image-gls-soft-sla-difficult images-gls-improvements \
	run-gls-time-curve run-gls-factorial run-gls-promoted-all \
	run-gls-promoted-all-30s run-gls-remaining-time
