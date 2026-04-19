CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O3 -march=native -Iinclude
DEBUGFLAGS = -std=c++17 -Wall -Wextra -g -O0 -Iinclude

# Directories
SRC_DIR = src
INC_DIR = include
TEST_DIR = tests
BUILD_DIR = build

# Targets
TARGET = $(BUILD_DIR)/bgp_simulator
TEST_TARGET = $(BUILD_DIR)/test_suite

# Source files
SOURCES = $(SRC_DIR)/as.cpp $(SRC_DIR)/as_graph.cpp $(SRC_DIR)/announcement.cpp $(SRC_DIR)/policy.cpp
MAIN_SOURCES = $(SRC_DIR)/main.cpp $(SOURCES)
TEST_SOURCES = $(TEST_DIR)/tests.cpp $(SOURCES)

# Object files
MAIN_OBJECTS = $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(MAIN_SOURCES))
TEST_OBJECTS = $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/src_%.o,$(SOURCES)) \
               $(BUILD_DIR)/tests.o

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Default target
all: $(BUILD_DIR) $(TARGET)

# Build everything
everything: $(BUILD_DIR) $(TARGET) $(TEST_TARGET)

# Debug build
debug: CXXFLAGS = $(DEBUGFLAGS)
debug: clean all

# Link main simulator
$(TARGET): $(MAIN_OBJECTS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(MAIN_OBJECTS)

# Link test suite
$(TEST_TARGET): $(TEST_OBJECTS)
	$(CXX) $(CXXFLAGS) -o $(TEST_TARGET) $(TEST_OBJECTS)

# Compile source files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Compile source files for tests (different naming to avoid conflicts)
$(BUILD_DIR)/src_%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Compile test files
$(BUILD_DIR)/tests.o: $(TEST_DIR)/tests.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Run test suite
test: $(TEST_TARGET)
	@echo "Running test suite..."
	@$(TEST_TARGET)

# Run quick validation
quicktest: $(TARGET)
	@echo "Running quick validation..."
	@$(TARGET) test_topology.txt test_announcements.csv test_output.csv

# Run all benchmark tests
benchmark: $(TARGET)
	@echo "Running benchmark tests..."
	@for test in many prefix subprefix; do \
		echo ""; \
		echo "========================================"; \
		echo "Testing: $$test"; \
		echo "========================================"; \
		time $(TARGET) bench/$$test/CAIDAASGraphCollector_2025.10.15.txt \
			bench/$$test/anns.csv \
			output_$$test.csv \
			bench/$$test/rov_asns.csv; \
		echo ""; \
		echo "Comparing output..."; \
		bash bench/compare_output.sh output_$$test.csv bench/$$test/ribs.csv && \
			echo "✓ $$test PASSED" || echo "✗ $$test FAILED"; \
	done

# Clean
clean:
	rm -rf $(BUILD_DIR)
	rm -f test_output.csv test_output_validation.csv test_bench_output.csv
	rm -f output_*.csv

.PHONY: all everything debug clean test quicktest benchmark
