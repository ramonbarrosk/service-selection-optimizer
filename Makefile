CXX = g++
CXXFLAGS = -std=c++17 -O3 -DNDEBUG -Wall -Wextra \
    -Isrc/basic -Isrc/enum -Isrc/instance -Isrc/util \
    -Isrc/validator -Isrc/search

DEPFLAGS = -MMD -MP
TARGET = build/service-selection-optimizer
GLS_EXPERIMENT_TARGET = build/compare-gls-improvements
GLS_EXPERIMENT_SOURCE = experiments/06_gls_melhorias/compare_gls_improvements.cpp

GLS_RESULTS = experiments/06_gls_melhorias/results
GLS_CHARTS = experiments/06_gls_melhorias/charts
GLS_SCRIPTS = experiments/06_gls_melhorias/scripts

all: $(TARGET)

$(TARGET): src/main.cpp
	@mkdir -p build
	$(CXX) $(CXXFLAGS) $(DEPFLAGS) -MF build/main.d -o $@ $<

$(GLS_EXPERIMENT_TARGET): $(GLS_EXPERIMENT_SOURCE)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) $(DEPFLAGS) -MF build/compare-gls-improvements.d -o $@ $<

-include build/main.d build/compare-gls-improvements.d

run: $(TARGET)
	@./$(TARGET)

clean:
	rm -rf build

# Experimentos da GLS (fase 06). Os experimentos das fases anteriores usam
# código arquivado na tag archive/pre-cleanup; veja experiments/README.md.
experiment-gls-improvements: $(GLS_EXPERIMENT_TARGET)

run-gls-improvements: $(GLS_EXPERIMENT_TARGET)
	SSO_EXPERIMENT_OUTPUT=$(GLS_RESULTS)/gls_improvements_difficult5.csv \
	./$(GLS_EXPERIMENT_TARGET)
	python3 $(GLS_SCRIPTS)/plot_gls_improvements.py

run-gls-time-curve: $(GLS_EXPERIMENT_TARGET)
	SSO_FACTORIAL=1 SSO_REPETITIONS=1 SSO_VARIANTS=i1_s1_l0_r1 \
	SSO_TIME_CURVE=0.55,2,5,10,30 \
	SSO_EXPERIMENT_OUTPUT=$(GLS_RESULTS)/gls_time_curve_best_factorial_difficult5.csv \
	./$(GLS_EXPERIMENT_TARGET)
	python3 $(GLS_SCRIPTS)/plot_gls_time_curve.py \
	--csv $(GLS_RESULTS)/gls_time_curve_best_factorial_difficult5.csv \
	--variant i1_s1_l0_r1 \
	--output-dir $(GLS_CHARTS)/gls_time_curve_best_factorial \
	--summary $(GLS_RESULTS)/gls_time_curve_best_factorial_difficult5_summary.md

run-gls-factorial: $(GLS_EXPERIMENT_TARGET)
	SSO_FACTORIAL=1 SSO_REPETITIONS=3 SSO_TIME_SECONDS=0.55 \
	SSO_EXPERIMENT_OUTPUT=$(GLS_RESULTS)/gls_factorial_difficult5.csv \
	./$(GLS_EXPERIMENT_TARGET)
	python3 $(GLS_SCRIPTS)/plot_gls_factorial.py

run-gls-promoted-all: $(GLS_EXPERIMENT_TARGET)
	SSO_FACTORIAL=1 SSO_INSTANCE_SET=all SSO_REPETITIONS=3 \
	SSO_VARIANTS=i1_s1_l0_r1 SSO_TIME_SECONDS=0.55 \
	SSO_EXPERIMENT_OUTPUT=$(GLS_RESULTS)/gls_promoted_all94.csv \
	./$(GLS_EXPERIMENT_TARGET)
	python3 $(GLS_SCRIPTS)/plot_gls_all_instances.py

run-gls-promoted-all-30s: $(GLS_EXPERIMENT_TARGET)
	SSO_FACTORIAL=1 SSO_INSTANCE_SET=all SSO_REPETITIONS=3 \
	SSO_VARIANTS=i1_s1_l0_r1 SSO_TIME_SECONDS=30 \
	SSO_EXPERIMENT_OUTPUT=$(GLS_RESULTS)/gls_promoted_all94_30s.csv \
	./$(GLS_EXPERIMENT_TARGET)
	python3 $(GLS_SCRIPTS)/plot_gls_all_instances.py \
	--csv $(GLS_RESULTS)/gls_promoted_all94_30s.csv \
	--output-dir $(GLS_CHARTS)/gls_promoted_all94_30s \
	--summary $(GLS_RESULTS)/gls_promoted_all94_30s_summary.md

run-gls-remaining-time: $(GLS_EXPERIMENT_TARGET)
	SSO_FACTORIAL=1 SSO_INSTANCES=11,28,100,128,147 SSO_REPETITIONS=1 \
	SSO_VARIANTS=i1_s1_l0_r1 SSO_TIME_CURVE=2,5,10,30 \
	SSO_EXPERIMENT_OUTPUT=$(GLS_RESULTS)/gls_time_curve_remaining5_stage1.csv \
	./$(GLS_EXPERIMENT_TARGET)
	SSO_FACTORIAL=1 SSO_INSTANCES=100 SSO_REPETITIONS=1 \
	SSO_VARIANTS=i1_s1_l0_r1 SSO_TIME_CURVE=60,120 \
	SSO_EXPERIMENT_OUTPUT=$(GLS_RESULTS)/gls_time_curve_instance100_stage2.csv \
	./$(GLS_EXPERIMENT_TARGET)
	python3 $(GLS_SCRIPTS)/plot_gls_remaining_time.py

run-gls-ablation:
	bash experiments/08_ablacao_gls/scripts/run_gls_ablation.sh

.PHONY: all run clean experiment-gls-improvements run-gls-improvements \
	run-gls-time-curve run-gls-factorial run-gls-promoted-all \
	run-gls-promoted-all-30s run-gls-remaining-time run-gls-ablation
