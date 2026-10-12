# ==============================================================================
# LogiSAT: Digital Logic Circuit Equivalence & Fault Verifier
# Makefile - Cross-Platform (Windows MinGW / Linux / macOS)
# ==============================================================================

CC       := gcc
CFLAGS   := -Wall -Wextra -pedantic -std=c99 -O2
INCLUDES := -Iinclude

SRC_DIR   := src
INC_DIR   := include
TEST_DIR  := test
BUILD_DIR := build
BIN_DIR   := bin
OUT_DIR   := output

# Core library source files (excluding main.c)
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

# Platform specifics
ifeq ($(OS),Windows_NT)
    TARGET_EXT := .exe
    RM_CMD     := del /Q /F
    FIX_PATH    = $(subst /,\,$1)
    DIR_GUARD   = if not exist "$(@D)" mkdir "$(@D)"
else
    TARGET_EXT :=
    RM_CMD     := rm -f
    FIX_PATH    = $1
    DIR_GUARD   = mkdir -p $(@D)
endif

TARGET      := $(BIN_DIR)/logisat$(TARGET_EXT)
TEST_TARGET := $(BIN_DIR)/test_runner$(TARGET_EXT)

.PHONY: all clean clean-output clean-all test help benchmark

# Default rule
all: $(TARGET)

# Object files compilation with automatic directory creation
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@$(DIR_GUARD)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD_DIR)/test_runner.o: $(TEST_DIR)/test_runner.c
	@$(DIR_GUARD)
	$(CC) $(CFLAGS) $(INCLUDES) -I$(TEST_DIR) -c $< -o $@

# Executable target
$(TARGET): $(CORE_OBJS) $(MAIN_OBJ)
	@$(DIR_GUARD)
	$(CC) $(CFLAGS) $^ -o $@
	@echo [OK] Built executable: $@

# Test executable target
$(TEST_TARGET): $(CORE_OBJS) $(TEST_OBJ)
	@$(DIR_GUARD)
	$(CC) $(CFLAGS) $^ -o $@
	@echo [OK] Built test runner: $@

# Run test suite
test: $(TEST_TARGET)
	@echo [RUNNING] Executing LogiSAT Unit Test Suite...
	@$(TEST_TARGET)

# Benchmark / Demonstration run
benchmark: $(TARGET)
	@if not exist "$(OUT_DIR)" mkdir "$(OUT_DIR)"
	@echo [DEMO] Running sample benchmark verification...
	@$(TARGET) --dimacs benchmarks/simple_sat.cnf --output $(OUT_DIR)/benchmark_sat.rep
	@$(TARGET) --equiv examples/full_adder_a.net examples/full_adder_b.net --output $(OUT_DIR)/full_adder_cec.rep
	@$(TARGET) --fault examples/full_adder_a.net sum 0 --output $(OUT_DIR)/fault_sum_sa0.rep

# Clean build artifacts
clean:
	@echo Cleaning build artifacts...
	-@cmd /c "del /q /f $(BUILD_DIR)\*.o 2>nul || exit 0"
	-@cmd /c "del /q /f $(TARGET) 2>nul || exit 0"
	-@cmd /c "del /q /f $(TEST_TARGET) 2>nul || exit 0"

# Clean generated reports and output files
clean-output:
	@echo Cleaning generated report files...
	-@cmd /c "del /q /f $(OUT_DIR)\*.* 2>nul || exit 0"
	-@cmd /c "del /q /f *.rep 2>nul || exit 0"
	-@cmd /c "del /q /f *.out 2>nul || exit 0"

# Clean all (build + outputs)
clean-all: clean clean-output
	@echo [OK] Cleaned all build and output artifacts.

help:
	@echo ----------------------------------------------------------------------
	@echo  LogiSAT Build Automation Helper
	@echo ----------------------------------------------------------------------
	@echo  make               : Build the main LogiSAT binary
	@echo  make test          : Build and execute automated Google Test unit tests
	@echo  make benchmark     : Run demonstration benchmarks and emit reports
	@echo  make clean         : Remove compiled objects and binaries
	@echo  make clean-output  : Delete generated verification and report files
	@echo  make clean-all     : Clean all binaries, object files, and outputs
	@echo ----------------------------------------------------------------------
