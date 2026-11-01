# Makefile - Build automation for LogiSAT (Linux)

CC       := gcc
CFLAGS   := -Wall -Wextra -pedantic -std=c99 -O2
INCLUDES := -Iinclude

SRC_DIR   := src
INC_DIR   := include
TEST_DIR  := test
BUILD_DIR := build
BIN_DIR   := bin
OUT_DIR   := output

CORE_SRCS := $(SRC_DIR)/cnf.c \
             $(SRC_DIR)/dimacs.c \
             $(SRC_DIR)/netlist.c \
             $(SRC_DIR)/parser.c \
             $(SRC_DIR)/tseytin.c \
             $(SRC_DIR)/solver.c \
             $(SRC_DIR)/verifier.c \
             $(SRC_DIR)/reporter.c

CORE_OBJS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(CORE_SRCS))

MAIN_SRC  := $(SRC_DIR)/main.c
MAIN_OBJ  := $(BUILD_DIR)/main.o

TEST_SRC  := $(TEST_DIR)/test_runner.c
TEST_OBJ  := $(BUILD_DIR)/test_runner.o

TARGET      := $(BIN_DIR)/logisat
TEST_TARGET := $(BIN_DIR)/test_runner

.PHONY: all clean clean-output clean-all test benchmark help

all: $(TARGET)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD_DIR)/test_runner.o: $(TEST_SRC)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -I$(TEST_DIR) -c $< -o $@

$(TARGET): $(CORE_OBJS) $(MAIN_OBJ)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $^ -o $@
	@echo "Built: $@"

$(TEST_TARGET): $(CORE_OBJS) $(TEST_OBJ)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $^ -o $@
	@echo "Built: $@"

test: $(TEST_TARGET)
	@./$(TEST_TARGET)

benchmark: $(TARGET)
	@mkdir -p $(OUT_DIR)
	@./$(TARGET) --explain examples/full_adder_a.net
	@./$(TARGET) --equiv examples/full_adder_a.net examples/full_adder_b.net --output $(OUT_DIR)/full_adder_cec.rep
	@./$(TARGET) --equiv examples/mux2to1_a.net examples/mux2to1_b.net --output $(OUT_DIR)/mux_cec.rep
	@./$(TARGET) --equiv examples/full_adder_a.net examples/full_adder_buggy.net
	@./$(TARGET) --fault examples/full_adder_a.net sum 0 --output $(OUT_DIR)/fault_sum_sa0.rep
	@./$(TARGET) --dimacs benchmarks/simple_sat.cnf --output $(OUT_DIR)/benchmark_sat.rep

clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)

clean-output:
	rm -rf $(OUT_DIR) *.rep *.out tmp_test*

clean-all: clean clean-output

help:
	@echo "Usage:"
	@echo "  make            Build logisat executable"
	@echo "  make test       Build and run test suite"
	@echo "  make benchmark  Run verification benchmark suite"
	@echo "  make clean      Remove build objects and binaries"
	@echo "  make clean-all  Remove all binaries, objects, and output reports"
