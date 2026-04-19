#include <iostream>
#include <string>
#include <memory>
#include "as_graph.hpp"
#include "policy.hpp"

int main(int argc, char* argv[]) {
    // Parse command line arguments
    if (argc < 4) {
        std::cerr << "Usage: " << argv[0] 
                  << " <caida_file> <announcements_csv> <output_csv> [rov_asns_file]" 
                  << std::endl;
        return 1;
    }
    
    std::string caida_file = argv[1];
    std::string announcements_file = argv[2];
    std::string output_file = argv[3];
    std::string rov_file = (argc >= 5) ? argv[4] : "";
    
    std::cout << "========================================" << std::endl;
    std::cout << "BGP Simulator" << std::endl;
    std::cout << "========================================" << std::endl;
    
    // Step 1: Build the AS graph from CAIDA data
    ASGraph graph;
    
    std::cout << "\n[Step 1] Building AS Graph..." << std::endl;
    if (!graph.buildFromCAIDAFile(caida_file)) {
        std::cerr << "Failed to build AS graph" << std::endl;
        return 1;
    }
    
    // Step 2: Flatten the graph for propagation
    std::cout << "\n[Step 2] Flattening graph..." << std::endl;
    graph.flattenGraph();
    
    // Step 3: Set default BGP policy for all ASes
    std::cout << "\n[Step 3] Setting default BGP policies..." << std::endl;
    for (const auto& pair : graph.getAllASes()) {
        uint32_t asn = pair.first;
        graph.setASPolicy(asn, std::make_unique<BGP>());
    }
    std::cout << "Set BGP policy for " << graph.getNumASes() << " ASes" << std::endl;
    
    // Step 4: Load ROV ASes (if file provided)
    if (!rov_file.empty()) {
        std::cout << "\n[Step 4] Loading ROV ASes..." << std::endl;
        graph.loadROVASes(rov_file);
    } else {
        std::cout << "\n[Step 4] No ROV file provided, skipping..." << std::endl;
    }
    
    // Step 5: Load announcements
    std::cout << "\n[Step 5] Loading announcements..." << std::endl;
    graph.loadAnnouncements(announcements_file);
    
    // Step 6: Propagate announcements through the network
    std::cout << "\n[Step 6] Propagating announcements..." << std::endl;
    graph.propagateAnnouncements();
    
    // Step 7: Export results to CSV
    std::cout << "\n[Step 7] Exporting results..." << std::endl;
    graph.exportToCSV(output_file);
    
    std::cout << "\n========================================" << std::endl;
    std::cout << "Simulation complete!" << std::endl;
    std::cout << "Results written to: " << output_file << std::endl;
    std::cout << "========================================" << std::endl;
    
    return 0;
}