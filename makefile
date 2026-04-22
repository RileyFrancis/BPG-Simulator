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

test_propagation: $(TEST_DIR)/test_propagation.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $(BUILD_DIR)/test_propagation $(TEST_DIR)/test_propagation.cpp

test_output: $(TEST_DIR)/test_output.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $(BUILD_DIR)/test_output $(TEST_DIR)/test_output.cpp

test_rov: $(TEST_DIR)/test_rov.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $(BUILD_DIR)/test_rov $(TEST_DIR)/test_rov.cpp

# Convenience target to run all tests
test: test_graph test_announcement test_bgp_policy test_flatten_graph test_seeding test_propagation test_output test_rov
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
	@echo ""
	@echo "=== Announcement Propagation Tests ==="
	./$(BUILD_DIR)/test_propagation
	@echo ""
	@echo "=== CSV Output Tests ==="
	./$(BUILD_DIR)/test_output
	@echo ""
	@echo "=== ROV Tests ==="
	./$(BUILD_DIR)/test_rov

# Run main program with bench/prefix data
run_prefix: bgp_simulator
	./$(BUILD_DIR)/bgp_simulator \
		bench/prefix/CAIDAASGraphCollector_2025.10.15.txt \
		bench/prefix/anns.csv \
		bench/prefix/rov_asns.csv \
		output/ribs_prefix.csv

# Run main program with bench/many data
run_many: bgp_simulator
	./$(BUILD_DIR)/bgp_simulator \
		bench/many/CAIDAASGraphCollector_2025.10.15.txt \
		bench/many/anns.csv \
		bench/many/rov_asns.csv \
		output/ribs_many.csv

# WebAssembly build — requires Emscripten (run: source ~/emsdk/emsdk_env.sh)
EMCC = $(HOME)/emsdk/upstream/emscripten/emcc

wasm: $(SRC_DIR)/wasm_main.cpp $(HEADERS)
	mkdir -p web
	$(EMCC) -std=c++17 -O2 \
		-Iinclude \
		-s WASM=1 \
		-s EXPORTED_RUNTIME_METHODS='["ccall"]' \
		-s EXPORTED_FUNCTIONS='["_run_bgp_simulation"]' \
		-s ALLOW_MEMORY_GROWTH=1 \
		-s MODULARIZE=1 \
		-s EXPORT_NAME='createBGPModule' \
		-s ENVIRONMENT='web,worker' \
		-o web/bgp_sim.js \
		$(SRC_DIR)/wasm_main.cpp
	@echo "WASM build complete — files written to web/"

clean:
	rm -rf $(BUILD_DIR)/*
	rm -rf output/*

.PHONY: clean test pytest pytest-verbose test-python run_prefix run_many wasm \
        test_graph test_announcement test_bgp_policy test_caida_policy \
        test_flatten_graph test_seeding test_propagation test_output test_rov