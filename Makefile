# ==============================================================================
# LogiSAT: Digital Logic Circuit Equivalence & Fault Verifier
# Makefile - Cross-Platform (Windows MinGW / Linux / macOS)
# Includes Official Google Test (gTest) Automation
# ==============================================================================

CC       := gcc
CXX      := g++
CFLAGS   := -Wall -Wextra -pedantic -std=c99 -O2
CXXFLAGS := -Wall -Wextra -std=gnu++11 -O2 -DGTEST_HAS_PTHREAD=0
INCLUDES := -Iinclude

SRC_DIR   := src
INC_DIR   := include
TEST_DIR  := test
BUILD_DIR := build
BIN_DIR   := bin
OUT_DIR   := output
GTEST_DIR := external/googletest/googletest
GTEST_LIB := external/googletest/libgtest.a

# Core C library sources
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

TEST_SRC  := $(TEST_DIR)/test_runner.cpp
TEST_OBJ  := $(BUILD_DIR)/test_runner.o

# Platform specifics
ifeq ($(OS),Windows_NT)
    TARGET_EXT := .exe
    RM_CMD     := del /Q /F
    DIR_GUARD   = if not exist "$(@D)" mkdir "$(@D)"
else
    TARGET_EXT :=
    RM_CMD     := rm -f
    DIR_GUARD   = mkdir -p $(@D)
endif

TARGET      := $(BIN_DIR)/logisat$(TARGET_EXT)
TEST_TARGET := $(BIN_DIR)/test_runner$(TARGET_EXT)

.PHONY: all clean clean-output clean-all test help benchmark

# Default rule
all: $(TARGET)

# Compile C sources
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@$(DIR_GUARD)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Build official Google Test static library if missing
$(GTEST_LIB):
	@echo [BUILDING] Official Google Test archive...
	$(CXX) $(CXXFLAGS) -I$(GTEST_DIR)/include -I$(GTEST_DIR) -c $(GTEST_DIR)/src/gtest-all.cc -o external/googletest/gtest-all.o
	$(CXX) $(CXXFLAGS) -I$(GTEST_DIR)/include -I$(GTEST_DIR) -c $(GTEST_DIR)/src/gtest_main.cc -o external/googletest/gtest_main.o
	ar rcs $(GTEST_LIB) external/googletest/gtest-all.o external/googletest/gtest_main.o
	@echo [OK] Built Google Test library: $(GTEST_LIB)

# Compile C++ Google Test runner
$(BUILD_DIR)/test_runner.o: $(TEST_SRC)
	@$(DIR_GUARD)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -I$(GTEST_DIR)/include -c $< -o $@

# Executable target
$(TARGET): $(CORE_OBJS) $(MAIN_OBJ)
	@$(DIR_GUARD)
	$(CC) $(CFLAGS) $^ -o $@
	@echo [OK] Built executable: $@

# Google Test executable target
$(TEST_TARGET): $(CORE_OBJS) $(TEST_OBJ) $(GTEST_LIB)
	@$(DIR_GUARD)
	$(CXX) $(CXXFLAGS) $^ -o $@
	@echo [OK] Built Google Test runner: $@

# Run Google Test suite
test: $(TEST_TARGET)
	@echo [RUNNING] Executing Official Google Test (gTest) Suite...
	@$(TEST_TARGET)

# Benchmark / Demonstration run
benchmark: $(TARGET)
	@if not exist "$(OUT_DIR)" mkdir "$(OUT_DIR)"
	@echo [DEMO 1] Explaining Full Adder circuit structure and Tseytin transformation...
	@$(TARGET) --explain examples/full_adder_a.net
	@echo [DEMO 2] Combinational Equivalence: Golden vs Synthesized Full Adder...
	@$(TARGET) --equiv examples/full_adder_a.net examples/full_adder_b.net --output $(OUT_DIR)/full_adder_cec.rep
	@echo [DEMO 3] Combinational Equivalence: Standard MUX vs All-NAND MUX...
	@$(TARGET) --equiv examples/mux2to1_a.net examples/mux2to1_b.net --output $(OUT_DIR)/mux_cec.rep
	@echo [DEMO 4] Combinational Equivalence: Buggy Circuit with Counterexample...
	@$(TARGET) --equiv examples/full_adder_a.net examples/full_adder_buggy.net
	@echo [DEMO 5] ATPG Fault Pattern Generation: Stuck-at-0 on 'sum'...
	@$(TARGET) --fault examples/full_adder_a.net sum 0 --output $(OUT_DIR)/fault_sum_sa0.rep
	@echo [DEMO 6] Direct DIMACS CNF SAT Benchmark...
	@$(TARGET) --dimacs benchmarks/simple_sat.cnf --output $(OUT_DIR)/benchmark_sat.rep

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
	@echo  make test          : Build and execute Official Google Test suite
	@echo  make benchmark     : Run demonstration benchmarks and emit reports
	@echo  make clean         : Remove compiled objects and binaries
	@echo  make clean-output  : Delete generated verification and report files
	@echo  make clean-all     : Clean all binaries, object files, and outputs
	@echo ----------------------------------------------------------------------
