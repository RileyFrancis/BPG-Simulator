#include "as_graph.hpp"
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    // Check command line arguments
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <caida_topology_file>" << std::endl;
        std::cerr << "Example: " << argv[0] << " bench/prefix/CAIDAASGraphCollector_2025.10.15.txt" << std::endl;
        return 1;
    }
    
    std::string caida_file = argv[1];
    
    std::cout << "=== BGP Simulator ===" << std::endl;
    std::cout << "Building AS Graph from CAIDA data..." << std::endl;
    std::cout << std::endl;
    
    // Create the AS graph
    ASGraph graph;
    
    // Build from CAIDA file
    if (!graph.buildFromCAIDAFile(caida_file)) {
        std::cerr << "Failed to build AS graph from file: " << caida_file << std::endl;
        return 1;
    }
    
    std::cout << std::endl;
    std::cout << "=== Graph Statistics ===" << std::endl;
    std::cout << "Total ASes: " << graph.getNumASes() << std::endl;
    std::cout << "Provider-Customer edges: " << graph.getNumProviderCustomerEdges() << std::endl;
    std::cout << "Peer-to-Peer edges: " << graph.getNumPeerEdges() << std::endl;
    std::cout << std::endl;
    
    std::cout << "=== AS Graph built successfully! ===" << std::endl;
    
    // TODO: Next steps will include:
    // - Reading announcements from CSV
    // - Reading ROV ASNs from CSV
    // - Flattening the graph (assigning propagation ranks)
    // - Seeding announcements
    // - Propagating announcements
    // - Writing output to ribs.csv
    
    return 0;
}