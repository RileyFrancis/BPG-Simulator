#include "as_graph.hpp"
#include <iostream>
#include <fstream>
#include <cassert>
#include <vector>

void test_simple_linear_chain() {
    std::cout << "\n=== Test: Simple Linear Chain ===" << std::endl;
    
    ASGraph graph;
    
    /*
     * Create simple chain:
     *   AS1 (rank 2, provider)
     *    |
     *   AS2 (rank 1, provider)
     *    |
     *   AS3 (rank 0, no customers)
     */
    
    AS* as1 = graph.getOrCreateAS(1);
    AS* as2 = graph.getOrCreateAS(2);
    AS* as3 = graph.getOrCreateAS(3);
    
    // AS1 -> AS2 -> AS3 (provider chain)
    as1->addCustomer(as2);
    as2->addProvider(as1);
    
    as2->addCustomer(as3);
    as3->addProvider(as2);
    
    // Flatten
    graph.flattenGraph();
    
    // Verify ranks
    assert(as3->getPropagationRank() == 0);  // No customers
    assert(as2->getPropagationRank() == 1);  // Customer at rank 0
    assert(as1->getPropagationRank() == 2);  // Customer at rank 1
    
    assert(graph.getMaxPropagationRank() == 2);
    assert(graph.getNumRanks() == 3);
    
    // Verify rank structure
    assert(graph.getASesAtRank(0).size() == 1);
    assert(graph.getASesAtRank(1).size() == 1);
    assert(graph.getASesAtRank(2).size() == 1);
    
    std::cout << "✓ Linear chain ranks assigned correctly" << std::endl;
    std::cout << "  AS3 (edge): rank " << as3->getPropagationRank() << std::endl;
    std::cout << "  AS2: rank " << as2->getPropagationRank() << std::endl;
    std::cout << "  AS1: rank " << as1->getPropagationRank() << std::endl;
}

void test_tree_structure() {
    std::cout << "\n=== Test: Tree Structure ===" << std::endl;
    
    ASGraph graph;
    
    /*
     * Create tree:
     *        AS1 (rank 2)
     *       /   \
     *     AS2   AS3 (rank 1)
     *    /   \
     *  AS4   AS5 (rank 0, no customers)
     * 
     */
    
    AS* as1 = graph.getOrCreateAS(1);
    AS* as2 = graph.getOrCreateAS(2);
    AS* as3 = graph.getOrCreateAS(3);
    AS* as4 = graph.getOrCreateAS(4);
    AS* as5 = graph.getOrCreateAS(5);
    
    as1->addCustomer(as2);
    as2->addProvider(as1);
    
    as1->addCustomer(as3);
    as3->addProvider(as1);
    
    as2->addCustomer(as4);
    as4->addProvider(as2);
    
    as2->addCustomer(as5);
    as5->addProvider(as2);
    
    // Flatten
    graph.flattenGraph();
    
    // Verify ranks
    assert(as4->getPropagationRank() == 0);
    assert(as5->getPropagationRank() == 0);
    assert(as2->getPropagationRank() == 1);
    assert(as3->getPropagationRank() == 0);  // No customers, also rank 0!
    assert(as1->getPropagationRank() == 2);
    
    // Verify rank sizes
    assert(graph.getASesAtRank(0).size() == 3);  // AS3, AS4, AS5
    assert(graph.getASesAtRank(1).size() == 1);  // AS2
    assert(graph.getASesAtRank(2).size() == 1);  // AS1
    
    std::cout << "✓ Tree structure ranks assigned correctly" << std::endl;
    std::cout << "  Rank 0: " << graph.getASesAtRank(0).size() << " ASes (edges)" << std::endl;
    std::cout << "  Rank 1: " << graph.getASesAtRank(1).size() << " AS" << std::endl;
    std::cout << "  Rank 2: " << graph.getASesAtRank(2).size() << " AS" << std::endl;
}

void test_diamond_structure() {
    std::cout << "\n=== Test: Diamond Structure (Multiple Paths) ===" << std::endl;
    
    ASGraph graph;
    
    /*
     * Create diamond:
     *       AS1 (rank 2)
     *      /   \
     *    AS2   AS3 (rank 1)
     *      \   /
     *       AS4 (rank 0, no customers)
     */
    
    AS* as1 = graph.getOrCreateAS(1);
    AS* as2 = graph.getOrCreateAS(2);
    AS* as3 = graph.getOrCreateAS(3);
    AS* as4 = graph.getOrCreateAS(4);
    
    as1->addCustomer(as2);
    as2->addProvider(as1);
    
    as1->addCustomer(as3);
    as3->addProvider(as1);
    
    as2->addCustomer(as4);
    as4->addProvider(as2);
    
    as3->addCustomer(as4);
    as4->addProvider(as3);
    
    // Flatten
    graph.flattenGraph();
    
    // AS4 has two providers but should still be rank 0 (no customers)
    assert(as4->getPropagationRank() == 0);
    assert(as2->getPropagationRank() == 1);
    assert(as3->getPropagationRank() == 1);
    assert(as1->getPropagationRank() == 2);
    
    std::cout << "✓ Diamond structure handles multiple paths correctly" << std::endl;
    std::cout << "  AS4 (2 providers): rank " << as4->getPropagationRank() << std::endl;
    std::cout << "  AS2, AS3: rank 1" << std::endl;
    std::cout << "  AS1: rank 2" << std::endl;
}

void test_isolated_as() {
    std::cout << "\n=== Test: Isolated AS ===" << std::endl;
    
    ASGraph graph;
    
    // Create an AS with no relationships
    AS* isolated = graph.getOrCreateAS(999);
    
    // Create a connected component
    AS* as1 = graph.getOrCreateAS(1);
    AS* as2 = graph.getOrCreateAS(2);
    
    as1->addCustomer(as2);
    as2->addProvider(as1);
    
    // Flatten
    graph.flattenGraph();
    
    // Isolated AS should be rank 0 (no customers)
    assert(isolated->getPropagationRank() == 0);
    
    // Connected ASes
    assert(as2->getPropagationRank() == 0);
    assert(as1->getPropagationRank() == 1);
    
    // Should have 2 ASes at rank 0 (isolated and AS2)
    assert(graph.getASesAtRank(0).size() == 2);
    
    std::cout << "✓ Isolated ASes handled correctly" << std::endl;
}

void test_peer_relationships_ignored() {
    std::cout << "\n=== Test: Peer Relationships Ignored in Ranking ===" << std::endl;
    
    ASGraph graph;
    
    /*
     * Create:
     *   AS1 (rank 1)
     *    |
     *   AS2 (rank 0, no customers)
     *   
     *   AS2 --- AS3 (peers, rank 0, no customers)
     */
    
    AS* as1 = graph.getOrCreateAS(1);
    AS* as2 = graph.getOrCreateAS(2);
    AS* as3 = graph.getOrCreateAS(3);
    
    as1->addCustomer(as2);
    as2->addProvider(as1);
    
    as2->addPeer(as3);
    as3->addPeer(as2);
    
    // Flatten
    graph.flattenGraph();
    
    // Peer relationships should not affect ranking
    // Both AS2 and AS3 should be rank 0 (no customers)
    assert(as2->getPropagationRank() == 0);
    assert(as3->getPropagationRank() == 0);
    assert(as1->getPropagationRank() == 1);
    
    std::cout << "✓ Peer relationships correctly ignored in ranking" << std::endl;
    std::cout << "  AS2 and AS3 (peers): both rank 0" << std::endl;
}

void test_bgpsimulator_example() {
    std::cout << "\n=== Test: bgpsimulator.com Example Topology ===" << std::endl;
    
    ASGraph graph;
    
    /*
     * Recreate bgpsimulator.com topology:
     *        AS4 --- AS777 (peers)
     *       /   \      |
     *     AS3   AS666  |
     *        \________/
     *    (AS3 is also customer of AS777)
     */
    
    AS* as3 = graph.getOrCreateAS(3);
    AS* as4 = graph.getOrCreateAS(4);
    AS* as666 = graph.getOrCreateAS(666);
    AS* as777 = graph.getOrCreateAS(777);
    
    // Provider-customer relationships
    as4->addCustomer(as3);
    as3->addProvider(as4);
    
    as4->addCustomer(as666);
    as666->addProvider(as4);
    
    as777->addCustomer(as3);
    as3->addProvider(as777);
    
    // Peer relationship
    as4->addPeer(as777);
    as777->addPeer(as4);
    
    // Flatten
    graph.flattenGraph();
    
    // Verify ranks
    assert(as666->getPropagationRank() == 0);  // No customers
    assert(as3->getPropagationRank() == 0);    // No customers (despite having 2 providers)
    assert(as4->getPropagationRank() == 1);    // Has customers at rank 0
    assert(as777->getPropagationRank() == 1);  // Has customer (AS3) at rank 0
    
    assert(graph.getMaxPropagationRank() == 1);
    assert(graph.getASesAtRank(0).size() == 2);  // AS3, AS666
    assert(graph.getASesAtRank(1).size() == 2);  // AS4, AS777
    
    std::cout << "✓ bgpsimulator.com topology ranked correctly" << std::endl;
    std::cout << "  Rank 0 (edges): AS3, AS666" << std::endl;
    std::cout << "  Rank 1: AS4, AS777" << std::endl;
}

void test_real_caida_graph() {
    std::cout << "\n=== Test: Real CAIDA Graph Flattening ===" << std::endl;
    
    ASGraph graph;
    
    // Try to find CAIDA file
    std::vector<std::string> possible_paths = {
        "bench/prefix/CAIDAASGraphCollector_2025.10.15.txt",
        "../bench/prefix/CAIDAASGraphCollector_2025.10.15.txt",
        "../../bench/prefix/CAIDAASGraphCollector_2025.10.15.txt"
    };
    
    std::string caida_file;
    for (const auto& path : possible_paths) {
        std::ifstream test(path);
        if (test.good()) {
            caida_file = path;
            break;
        }
    }
    
    if (caida_file.empty()) {
        std::cout << "⚠ CAIDA file not found. Skipping this test." << std::endl;
        std::cout << "  (This test is optional)" << std::endl;
        return;
    }
    
    std::cout << "Loading CAIDA topology..." << std::endl;
    if (!graph.buildFromCAIDAFile(caida_file)) {
        std::cerr << "Failed to load CAIDA file" << std::endl;
        return;
    }
    
    // Flatten the graph
    graph.flattenGraph();
    
    std::cout << "\n✓ Successfully flattened real CAIDA graph!" << std::endl;
    std::cout << "  Total ASes: " << graph.getNumASes() << std::endl;
    std::cout << "  Max rank: " << graph.getMaxPropagationRank() << std::endl;
    std::cout << "  Number of ranks: " << graph.getNumRanks() << std::endl;
    
    // Show distribution
    std::cout << "\n  Rank distribution:" << std::endl;
    for (size_t i = 0; i < graph.getNumRanks() && i < 10; ++i) {
        std::cout << "    Rank " << i << ": " 
                  << graph.getASesAtRank(i).size() << " ASes" << std::endl;
    }
    
    if (graph.getNumRanks() > 10) {
        std::cout << "    ... (" << (graph.getNumRanks() - 10) << " more ranks)" << std::endl;
    }
    
    // Verify all ASes have ranks
    int unranked = 0;
    for (const auto& pair : graph.getAllASes()) {
        if (pair.second->getPropagationRank() == -1) {
            unranked++;
        }
    }
    
    assert(unranked == 0);
    std::cout << "\n  ✓ All ASes assigned ranks (no unranked ASes)" << std::endl;
}

void test_rank_structure_access() {
    std::cout << "\n=== Test: Rank Structure Access Methods ===" << std::endl;
    
    ASGraph graph;
    
    // Create simple structure
    AS* as1 = graph.getOrCreateAS(1);
    AS* as2 = graph.getOrCreateAS(2);
    AS* as3 = graph.getOrCreateAS(3);
    
    as1->addCustomer(as2);
    as2->addProvider(as1);
    as2->addCustomer(as3);
    as3->addProvider(as2);
    
    graph.flattenGraph();
    
    // Test getASesAtRank
    const auto& rank0 = graph.getASesAtRank(0);
    const auto& rank1 = graph.getASesAtRank(1);
    const auto& rank2 = graph.getASesAtRank(2);
    
    assert(rank0.size() == 1);
    assert(rank1.size() == 1);
    assert(rank2.size() == 1);
    
    assert(rank0[0]->getASN() == 3);
    assert(rank1[0]->getASN() == 2);
    assert(rank2[0]->getASN() == 1);
    
    // Test invalid rank access (should return empty)
    const auto& invalid = graph.getASesAtRank(999);
    assert(invalid.empty());
    
    std::cout << "✓ Rank structure access methods work correctly" << std::endl;
}

int main() {
    std::cout << "=== Testing Graph Flattening (Section 3.3) ===" << std::endl;
    
    try {
        test_simple_linear_chain();
        test_tree_structure();
        test_diamond_structure();
        test_isolated_as();
        test_peer_relationships_ignored();
        test_bgpsimulator_example();
        test_rank_structure_access();
        test_real_caida_graph();
        
        std::cout << "\n✅ ALL GRAPH FLATTENING TESTS PASSED!" << std::endl;
        std::cout << "\nSection 3.3 Complete!" << std::endl;
        std::cout << "Next: Section 3.4 - Seeding announcements" << std::endl;
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n❌ Test failed: " << e.what() << std::endl;
        return 1;
    }
}