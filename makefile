CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -g -Iinclude

# Directories
SRC_DIR = src
INCLUDE_DIR = include
TEST_DIR = tests
BUILD_DIR = build

# Header files (for dependency tracking)
HEADERS = $(INCLUDE_DIR)/as.hpp \
          $(INCLUDE_DIR)/as_graph.hpp \
          $(INCLUDE_DIR)/announcement.hpp \
          $(INCLUDE_DIR)/policy.hpp

# Create build directory if it doesn't exist
$(shell mkdir -p $(BUILD_DIR))

# Main program
bgp_simulator: $(SRC_DIR)/main.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $(BUILD_DIR)/bgp_simulator $(SRC_DIR)/main.cpp

# Test executables (header-only, no object files needed)
test_graph: $(TEST_DIR)/test_graph.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $(BUILD_DIR)/test_graph $(TEST_DIR)/test_graph.cpp

test_announcement: $(TEST_DIR)/test_announcement.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $(BUILD_DIR)/test_announcement $(TEST_DIR)/test_announcement.cpp

test_bgp_policy: $(TEST_DIR)/test_bgp_policy.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $(BUILD_DIR)/test_bgp_policy $(TEST_DIR)/test_bgp_policy.cpp

test_caida_policy: $(TEST_DIR)/test_caida_policy.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $(BUILD_DIR)/test_caida_policy $(TEST_DIR)/test_caida_policy.cpp

test_flatten_graph: $(TEST_DIR)/test_flatten_graph.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $(BUILD_DIR)/test_flatten_graph $(TEST_DIR)/test_flatten_graph.cpp

test_seeding: $(TEST_DIR)/test_seeding.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $(BUILD_DIR)/test_seeding $(TEST_DIR)/test_seeding.cpp

# Convenience target to run all tests
test: test_graph test_announcement test_bgp_policy test_flatten_graph test_seeding
	@echo "Running all tests..."
	@echo ""
	@echo "=== Graph Tests ==="
	./$(BUILD_DIR)/test_graph
	@echo ""
	@echo "=== Announcement Tests ==="
	./$(BUILD_DIR)/test_announcement
	@echo ""
	@echo "=== BGP Policy Tests ==="
	./$(BUILD_DIR)/test_bgp_policy
	@echo ""
	@echo "=== Graph Flattening Tests ==="
	./$(BUILD_DIR)/test_flatten_graph
	@echo ""
	@echo "=== Announcement Seeding Tests ==="
	./$(BUILD_DIR)/test_seeding

# Run main program with bench/prefix data
run_prefix: bgp_simulator
	./$(BUILD_DIR)/bgp_simulator bench/prefix/CAIDAASGraphCollector_2025.10.15.txt

# Run main program with bench/many data
run_many: bgp_simulator
	./$(BUILD_DIR)/bgp_simulator bench/many/CAIDAASGraphCollector_2025.10.15.txt

clean:
	rm -rf $(BUILD_DIR)/*

.PHONY: clean test pytest pytest-verbose test-python run_prefix run_many test_graph test_announcement test_bgp_policy test_caida_policy test_flatten_graph test_seeding