#include "as_graph.hpp"
#include <iostream>
#include <cassert>
#include <fstream>

void test_simple_graph() {
    std::cout << "\n=== Test: Simple Manual Graph ===" << std::endl;
    
    ASGraph graph;
    
    // Manually create a simple graph
    AS* as1 = graph.getOrCreateAS(1);
    AS* as2 = graph.getOrCreateAS(2);
    AS* as3 = graph.getOrCreateAS(3);
    
    // AS1 is customer of AS2
    as2->addCustomer(as1);
    as1->addProvider(as2);
    
    // AS2 is customer of AS3
    as3->addCustomer(as2);
    as2->addProvider(as3);
    
    assert(graph.getNumASes() == 3);
    assert(as1->getProviders().size() == 1);
    assert(as2->getProviders().size() == 1);
    assert(as3->getCustomers().size() == 1);
    assert(as2->getCustomers().size() == 1);
    
    // Verify specific relationships
    assert(as1->isProvider(as2));
    assert(as2->isProvider(as3));
    assert(as2->isCustomer(as1));
    assert(as3->isCustomer(as2));
    
    // No cycles
    assert(!graph.hasCycles());
    
    std::cout << "✓ Simple graph test passed!" << std::endl;
}

void test_peer_relationships() {
    std::cout << "\n=== Test: Peer Relationships ===" << std::endl;
    
    ASGraph graph;
    
    AS* as1 = graph.getOrCreateAS(1);
    AS* as2 = graph.getOrCreateAS(2);
    AS* as3 = graph.getOrCreateAS(3);
    AS* as4 = graph.getOrCreateAS(4);
    
    // Create provider-customer chain
    as2->addCustomer(as1);
    as1->addProvider(as2);
    
    as3->addCustomer(as2);
    as2->addProvider(as3);
    
    // Add peer relationships
    as2->addPeer(as4);
    as4->addPeer(as2);
    
    as3->addPeer(as4);
    as4->addPeer(as3);
    
    assert(as2->getPeers().size() == 1);
    assert(as4->getPeers().size() == 2);
    assert(as2->isPeer(as4));
    assert(as4->isPeer(as2));
    assert(as3->isPeer(as4));
    
    // Peer cycles are okay, should not be detected
    assert(!graph.hasCycles());
    
    std::cout << "✓ Peer relationship test passed!" << std::endl;
}

void test_cycle_detection() {
    std::cout << "\n=== Test: Cycle Detection ===" << std::endl;
    
    ASGraph graph;
    
    AS* as1 = graph.getOrCreateAS(1);
    AS* as2 = graph.getOrCreateAS(2);
    AS* as3 = graph.getOrCreateAS(3);
    
    // Create a cycle in provider-customer relationships: 1 -> 2 -> 3 -> 1
    as2->addCustomer(as1);
    as1->addProvider(as2);
    
    as3->addCustomer(as2);
    as2->addProvider(as3);
    
    as1->addCustomer(as3);  // This creates the cycle!
    as3->addProvider(as1);
    
    assert(graph.hasCycles());
    
    std::cout << "✓ Cycle detection test passed!" << std::endl;
}

void test_duplicate_prevention() {
    std::cout << "\n=== Test: Duplicate Relationship Prevention ===" << std::endl;
    
    ASGraph graph;
    
    AS* as1 = graph.getOrCreateAS(1);
    AS* as2 = graph.getOrCreateAS(2);
    
    // Add the same relationship multiple times
    as2->addCustomer(as1);
    as2->addCustomer(as1);
    as2->addCustomer(as1);
    
    as1->addProvider(as2);
    as1->addProvider(as2);
    
    // Should only have one entry each
    assert(as2->getCustomers().size() == 1);
    assert(as1->getProviders().size() == 1);
    
    std::cout << "✓ Duplicate prevention test passed!" << std::endl;
}

void test_caida_parsing() {
    std::cout << "\n=== Test: CAIDA File Parsing ===" << std::endl;
    
    // Create a small test CAIDA file
    std::ofstream test_file("test_caida_temp.txt");
    test_file << "# Test CAIDA file\n";
    test_file << "# Format: provider|customer|-1 or peer|peer|0\n";
    test_file << "100|200|-1\n";  // 100 is provider, 200 is customer
    test_file << "100|300|-1\n";  // 100 is provider, 300 is customer
    test_file << "400|500|0\n";   // 400 and 500 are peers
    test_file << "\n";            // Empty line
    test_file << "# Comment line\n";
    test_file << "600|700|-1|source\n";  // With optional source column
    test_file.close();
    
    ASGraph graph;
    bool success = graph.buildFromCAIDAFile("test_caida_temp.txt");
    
    assert(success);
    assert(graph.getNumASes() == 7);  // 100, 200, 300, 400, 500, 600, 700
    assert(graph.getNumProviderCustomerEdges() == 3);
    assert(graph.getNumPeerEdges() == 1);
    
    // Verify specific relationships
    AS* as100 = graph.getAS(100);
    AS* as200 = graph.getAS(200);
    AS* as400 = graph.getAS(400);
    AS* as500 = graph.getAS(500);
    
    assert(as100 != nullptr);
    assert(as100->getCustomers().size() == 2);  // 200 and 300
    assert(as200->getProviders().size() == 1);  // 100
    assert(as400->getPeers().size() == 1);      // 500
    assert(as500->getPeers().size() == 1);      // 400
    
    // Clean up
    std::remove("test_caida_temp.txt");
    
    std::cout << "✓ CAIDA parsing test passed!" << std::endl;
}

void test_caida_file_optional() {
    std::cout << "\n=== Test: Read Real CAIDA File (Optional) ===" << std::endl;
    
    // Check if file exists first
    std::ifstream test_exists("bench/prefix/CAIDAASGraphCollector_2025.10.15.txt");
    if (!test_exists.good()) {
        std::cout << "⚠ Skipping - CAIDA file not found at bench/prefix/CAIDAASGraphCollector_2025.10.15.txt" << std::endl;
        return;
    }
    test_exists.close();
    
    ASGraph graph;
    bool success = graph.buildFromCAIDAFile("bench/prefix/CAIDAASGraphCollector_2025.10.15.txt");
    
    assert(success);
    assert(graph.getNumASes() > 0);
    assert(!graph.hasCycles());  // Real CAIDA data should not have cycles
    
    std::cout << "✓ Real CAIDA file test passed!" << std::endl;
    std::cout << "  ASes: " << graph.getNumASes() << std::endl;
    std::cout << "  Provider-Customer edges: " << graph.getNumProviderCustomerEdges() << std::endl;
    std::cout << "  Peer edges: " << graph.getNumPeerEdges() << std::endl;
}

void test_edge_cases() {
    std::cout << "\n=== Test: Edge Cases ===" << std::endl;
    
    ASGraph graph;
    
    // Test getting non-existent AS
    AS* nonexistent = graph.getAS(999999);
    assert(nonexistent == nullptr);
    
    // Test with empty graph
    assert(graph.getNumASes() == 0);
    assert(!graph.hasCycles());
    
    // Test single AS
    AS* single = graph.getOrCreateAS(1);
    assert(single != nullptr);
    assert(graph.getNumASes() == 1);
    assert(!single->hasProviders());
    assert(!single->hasCustomers());
    assert(!single->hasPeers());
    
    std::cout << "✓ Edge cases test passed!" << std::endl;
}

int main() {
    std::cout << "Running AS Graph Tests..." << std::endl;
    
    try {
        test_simple_graph();
        test_peer_relationships();
        test_cycle_detection();
        test_duplicate_prevention();
        test_caida_parsing();
        test_edge_cases();
        test_caida_file_optional();  // Run this last since it's optional
        
        std::cout << "\n✅ ALL TESTS PASSED!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n❌ Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "\n❌ Test failed with unknown exception!" << std::endl;
        return 1;
    }
}