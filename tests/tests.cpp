#include "as_graph.hpp"
#include "announcement.hpp"
#include "policy.hpp"
#include <iostream>
#include <cassert>
#include <fstream>

// ============================================================================
// GRAPH TESTS
// ============================================================================

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
    
    // No cycles
    assert(!graph.hasCycles());
    
    std::cout << "✓ Simple graph test passed!" << std::endl;
}

void test_cycle_detection() {
    std::cout << "\n=== Test: Cycle Detection ===" << std::endl;
    
    ASGraph graph;
    
    AS* as1 = graph.getOrCreateAS(1);
    AS* as2 = graph.getOrCreateAS(2);
    AS* as3 = graph.getOrCreateAS(3);
    
    // Create a cycle: 1 -> 2 -> 3 -> 1
    as2->addCustomer(as1);
    as1->addProvider(as2);
    
    as3->addCustomer(as2);
    as2->addProvider(as3);
    
    as1->addCustomer(as3);  // This creates the cycle!
    as3->addProvider(as1);
    
    assert(graph.hasCycles());
    
    std::cout << "✓ Cycle detection test passed!" << std::endl;
}

void test_caida_file() {
    std::cout << "\n=== Test: Read CAIDA File ===" << std::endl;
    
    ASGraph graph;
    
    // Test with one of the provided files - try multiple locations
    std::vector<std::string> possible_files = {
        "bench/prefix/CAIDAASGraphCollector_2025.10.15.txt",
        "../bench/prefix/CAIDAASGraphCollector_2025.10.15.txt",
        "../../bench/prefix/CAIDAASGraphCollector_2025.10.15.txt"
    };
    
    bool success = false;
    for (const auto& file : possible_files) {
        success = graph.buildFromCAIDAFile(file);
        if (success) {
            std::cout << "  Using file: " << file << std::endl;
            break;
        }
    }
    
    if (!success) {
        std::cout << "⚠ Skipping test (CAIDA file not found in expected locations)" << std::endl;
        return;
    }
    
    assert(graph.getNumASes() > 0);
    
    std::cout << "✓ CAIDA file test passed!" << std::endl;
    std::cout << "  ASes: " << graph.getNumASes() << std::endl;
    std::cout << "  Provider-Customer edges: " << graph.getNumProviderCustomerEdges() << std::endl;
    std::cout << "  Peer edges: " << graph.getNumPeerEdges() << std::endl;
}

void test_graph_flattening() {
    std::cout << "\n=== Test: Graph Flattening ===" << std::endl;
    
    ASGraph graph;
    
    // Create a simple hierarchy:
    //   AS3 (provider)
    //    |
    //   AS2 (provider)
    //    |
    //   AS1 (customer, no customers of its own)
    
    AS* as1 = graph.getOrCreateAS(1);
    AS* as2 = graph.getOrCreateAS(2);
    AS* as3 = graph.getOrCreateAS(3);
    
    as2->addCustomer(as1);
    as1->addProvider(as2);
    
    as3->addCustomer(as2);
    as2->addProvider(as3);
    
    // Flatten
    graph.flattenGraph();
    
    // Check ranks
    assert(as1->getPropagationRank() == 0);  // No customers
    assert(as2->getPropagationRank() == 1);  // Provider of rank 0
    assert(as3->getPropagationRank() == 2);  // Provider of rank 1
    
    const auto& ranks = graph.getPropagationRanks();
    assert(ranks.size() == 3);
    assert(ranks[0].size() == 1);  // AS1
    assert(ranks[1].size() == 1);  // AS2
    assert(ranks[2].size() == 1);  // AS3
    
    std::cout << "✓ Graph flattening test passed!" << std::endl;
}

void test_flatten_caida() {
    std::cout << "\n=== Test: Flatten Real CAIDA Graph ===" << std::endl;
    
    ASGraph graph;
    
    std::vector<std::string> possible_files = {
        "bench/prefix/CAIDAASGraphCollector_2025.10.15.txt",
        "../bench/prefix/CAIDAASGraphCollector_2025.10.15.txt",
        "../../bench/prefix/CAIDAASGraphCollector_2025.10.15.txt"
    };
    
    bool success = false;
    for (const auto& file : possible_files) {
        success = graph.buildFromCAIDAFile(file);
        if (success) break;
    }
    
    if (!success) {
        std::cout << "⚠ Skipping test (CAIDA file not found)" << std::endl;
        return;
    }
    
    graph.flattenGraph();
    
    const auto& ranks = graph.getPropagationRanks();
    std::cout << "  Total propagation ranks: " << ranks.size() << std::endl;
    
    assert(ranks.size() > 0);
    
    std::cout << "✓ CAIDA flattening test passed!" << std::endl;
}

// ============================================================================
// ANNOUNCEMENT TESTS
// ============================================================================

void test_announcement_creation() {
    std::cout << "\n=== Test: Announcement Creation ===" << std::endl;
    
    // Test basic announcement
    Announcement ann1("1.2.0.0/16", 100, Relationship::ORIGIN, false);
    assert(ann1.getPrefix() == "1.2.0.0/16");
    assert(ann1.getNextHopASN() == 100);
    assert(ann1.getRecvRelationship() == Relationship::ORIGIN);
    assert(!ann1.isROVInvalid());
    assert(ann1.getASPath().empty());
    
    // Test with AS path
    std::vector<uint32_t> path = {100, 200, 300};
    Announcement ann2("2.3.0.0/16", path, 300, Relationship::CUSTOMER, true);
    assert(ann2.getASPath().size() == 3);
    assert(ann2.getASPath()[0] == 100);
    assert(ann2.isROVInvalid());
    
    std::cout << "✓ Announcement creation test passed!" << std::endl;
}

void test_bgp_selection() {
    std::cout << "\n=== Test: BGP Route Selection ===" << std::endl;
    
    // Test 1: Relationship preference (customer > peer > provider)
    Announcement customer("1.0.0.0/8", 100, Relationship::CUSTOMER, false);
    Announcement peer("1.0.0.0/8", 100, Relationship::PEER, false);
    Announcement provider("1.0.0.0/8", 100, Relationship::PROVIDER, false);
    
    assert(customer.isBetterThan(peer));
    assert(customer.isBetterThan(provider));
    assert(peer.isBetterThan(provider));
    
    // Test 2: Path length preference (same relationship)
    std::vector<uint32_t> short_path = {100, 200};
    std::vector<uint32_t> long_path = {100, 200, 300, 400};
    
    Announcement short_ann("2.0.0.0/8", short_path, 200, Relationship::CUSTOMER, false);
    Announcement long_ann("2.0.0.0/8", long_path, 400, Relationship::CUSTOMER, false);
    
    assert(short_ann.isBetterThan(long_ann));
    
    // Test 3: Next hop ASN tiebreaker (same relationship and path length)
    Announcement low_asn("3.0.0.0/8", short_path, 100, Relationship::CUSTOMER, false);
    Announcement high_asn("3.0.0.0/8", short_path, 200, Relationship::CUSTOMER, false);
    
    assert(low_asn.isBetterThan(high_asn));
    
    std::cout << "✓ BGP selection test passed!" << std::endl;
}

void test_as_path_manipulation() {
    std::cout << "\n=== Test: AS-Path Manipulation ===" << std::endl;
    
    std::vector<uint32_t> path = {100, 200};
    Announcement ann("1.0.0.0/8", path, 200, Relationship::CUSTOMER, false);
    
    // Prepend ASN
    ann.prependASPath(300);
    
    assert(ann.getASPath().size() == 3);
    assert(ann.getASPath()[0] == 300);
    assert(ann.getASPath()[1] == 100);
    assert(ann.getASPath()[2] == 200);
    
    // Check string format
    std::string path_str = ann.asPathToString();
    assert(path_str == "300_100_200");
    
    std::cout << "✓ AS-Path manipulation test passed!" << std::endl;
}

// ============================================================================
// POLICY TESTS
// ============================================================================

void test_bgp_policy() {
    std::cout << "\n=== Test: BGP Policy ===" << std::endl;
    
    ASGraph graph;
    AS* as1 = graph.getOrCreateAS(1);
    as1->setPolicy(std::make_unique<BGP>());
    
    // Seed an announcement
    Announcement ann("1.0.0.0/8", 1, Relationship::ORIGIN, false);
    as1->getPolicy()->seedAnnouncement(ann);
    
    // Check it's in the local RIB
    assert(as1->getPolicy()->hasAnnouncement("1.0.0.0/8"));
    
    const auto& rib = as1->getPolicy()->getLocalRib();
    assert(rib.size() == 1);
    assert(rib.at("1.0.0.0/8").getPrefix() == "1.0.0.0/8");
    
    std::cout << "✓ BGP policy test passed!" << std::endl;
}

void test_rov_policy() {
    std::cout << "\n=== Test: ROV Policy ===" << std::endl;
    
    ASGraph graph;
    AS* as1 = graph.getOrCreateAS(1);
    AS* as2 = graph.getOrCreateAS(2);  // ROV enabled
    
    as1->setPolicy(std::make_unique<BGP>());
    as2->setPolicy(std::make_unique<ROV>());
    
    // Create customer relationship
    as1->addCustomer(as2);
    as2->addProvider(as1);
    
    // Seed valid and invalid announcements at AS1
    Announcement valid("1.0.0.0/8", 1, Relationship::ORIGIN, false);
    Announcement invalid("2.0.0.0/8", 1, Relationship::ORIGIN, true);
    
    as1->getPolicy()->seedAnnouncement(valid);
    as1->getPolicy()->seedAnnouncement(invalid);
    
    // AS1 should have both
    assert(as1->getPolicy()->getLocalRib().size() == 2);
    
    // Send to AS2
    as1->sendAnnouncementsToCustomers();
    as2->processReceivedAnnouncements();
    
    // AS2 (ROV) should only accept the valid one
    assert(as2->getPolicy()->hasAnnouncement("1.0.0.0/8"));
    assert(!as2->getPolicy()->hasAnnouncement("2.0.0.0/8"));
    
    std::cout << "✓ ROV policy test passed!" << std::endl;
}

// ============================================================================
// PROPAGATION TESTS
// ============================================================================

void test_simple_propagation() {
    std::cout << "\n=== Test: Simple Propagation ===" << std::endl;
    
    ASGraph graph;
    
    // Create simple topology:
    //   AS1 (provider)
    //    |
    //   AS2 (customer)
    AS* as1 = graph.getOrCreateAS(1);
    AS* as2 = graph.getOrCreateAS(2);
    
    as1->addCustomer(as2);
    as2->addProvider(as1);
    
    // Set policies
    as1->setPolicy(std::make_unique<BGP>());
    as2->setPolicy(std::make_unique<BGP>());
    
    // Flatten and seed
    graph.flattenGraph();
    
    // Create origin announcement - AS2 is the origin
    std::vector<uint32_t> origin_path = {2};  // Origin has itself in path
    Announcement ann("1.0.0.0/8", origin_path, 2, Relationship::ORIGIN, false);
    graph.seedAnnouncement(2, ann);
    
    // Propagate
    graph.propagateAnnouncements();
    
    // AS1 should receive the announcement from customer AS2
    assert(as1->getPolicy()->hasAnnouncement("1.0.0.0/8"));
    
    const auto& as1_rib = as1->getPolicy()->getLocalRib();
    const Announcement& as1_ann = as1_rib.at("1.0.0.0/8");
    
    // AS1's path should be [1, 2] (AS1 prepends itself to the [2] it received)
    assert(as1_ann.getASPath().size() == 2);
    assert(as1_ann.getASPath()[0] == 1);
    assert(as1_ann.getASPath()[1] == 2);
    
    std::cout << "✓ Simple propagation test passed!" << std::endl;
}

void test_peer_propagation() {
    std::cout << "\n=== Test: Peer Propagation ===" << std::endl;
    
    ASGraph graph;
    
    // Create topology:
    //   AS1 --- AS2 (peers)
    //    |
    //   AS3 (customer of AS1)
    
    AS* as1 = graph.getOrCreateAS(1);
    AS* as2 = graph.getOrCreateAS(2);
    AS* as3 = graph.getOrCreateAS(3);
    
    as1->addPeer(as2);
    as2->addPeer(as1);
    
    as1->addCustomer(as3);
    as3->addProvider(as1);
    
    // Set policies
    as1->setPolicy(std::make_unique<BGP>());
    as2->setPolicy(std::make_unique<BGP>());
    as3->setPolicy(std::make_unique<BGP>());
    
    graph.flattenGraph();
    
    // AS3 announces
    Announcement ann("3.0.0.0/8", 3, Relationship::ORIGIN, false);
    graph.seedAnnouncement(3, ann);
    
    // Propagate
    graph.propagateAnnouncements();
    
    // AS1 should have it (from customer)
    assert(as1->getPolicy()->hasAnnouncement("3.0.0.0/8"));
    
    // AS2 should have it (from peer AS1, who learned from customer)
    assert(as2->getPolicy()->hasAnnouncement("3.0.0.0/8"));
    
    std::cout << "✓ Peer propagation test passed!" << std::endl;
}

void test_no_valley_routing() {
    std::cout << "\n=== Test: No Valley Routing ===" << std::endl;
    
    ASGraph graph;
    
    // Create topology:
    //   AS1 --- AS2 (peers)
    //           |
    //          AS3 (provider of AS2)
    
    AS* as1 = graph.getOrCreateAS(1);
    AS* as2 = graph.getOrCreateAS(2);
    AS* as3 = graph.getOrCreateAS(3);
    
    as1->addPeer(as2);
    as2->addPeer(as1);
    
    as3->addCustomer(as2);
    as2->addProvider(as3);
    
    // Set policies
    as1->setPolicy(std::make_unique<BGP>());
    as2->setPolicy(std::make_unique<BGP>());
    as3->setPolicy(std::make_unique<BGP>());
    
    graph.flattenGraph();
    
    // AS3 announces
    Announcement ann("3.0.0.0/8", 3, Relationship::ORIGIN, false);
    graph.seedAnnouncement(3, ann);
    
    // Propagate
    graph.propagateAnnouncements();
    
    // AS2 should have it (from provider)
    assert(as2->getPolicy()->hasAnnouncement("3.0.0.0/8"));
    
    // AS1 should NOT have it (AS2 should not export provider route to peer)
    assert(!as1->getPolicy()->hasAnnouncement("3.0.0.0/8"));
    
    std::cout << "✓ No valley routing test passed!" << std::endl;
}

// ============================================================================
// INTEGRATION TESTS
// ============================================================================

void test_full_simulation_small() {
    std::cout << "\n=== Test: Full Simulation (Small Dataset) ===" << std::endl;
    
    ASGraph graph;
    
    // Build from test topology
    assert(graph.buildFromCAIDAFile("test_topology.txt"));
    
    // Set policies
    for (const auto& pair : graph.getAllASes()) {
        graph.setASPolicy(pair.first, std::make_unique<BGP>());
    }
    
    // Flatten
    graph.flattenGraph();
    
    // Load announcements
    graph.loadAnnouncements("test_announcements.csv");
    
    // Propagate
    graph.propagateAnnouncements();
    
    // Export
    graph.exportToCSV("test_output_validation.csv");
    
    // Verify file exists and has content
    std::ifstream test_file("test_output_validation.csv");
    assert(test_file.good());
    
    std::string line;
    int line_count = 0;
    while (std::getline(test_file, line)) {
        line_count++;
    }
    
    assert(line_count > 1);  // Header + at least one data line
    
    std::cout << "  Generated " << (line_count - 1) << " output rows" << std::endl;
    std::cout << "✓ Full simulation test passed!" << std::endl;
}

void test_benchmark_dataset() {
    std::cout << "\n=== Test: Benchmark Dataset (Prefix) ===" << std::endl;
    
    ASGraph graph;
    
    // Try different paths
    std::vector<std::string> possible_paths = {
        "bench/prefix/",
        "../bench/prefix/",
        "../../bench/prefix/"
    };
    
    std::string base_path;
    for (const auto& path : possible_paths) {
        if (graph.buildFromCAIDAFile(path + "CAIDAASGraphCollector_2025.10.15.txt")) {
            base_path = path;
            break;
        }
    }
    
    if (base_path.empty()) {
        std::cout << "⚠ Skipping test (benchmark files not found)" << std::endl;
        return;
    }
    
    // Set default BGP policies
    for (const auto& pair : graph.getAllASes()) {
        graph.setASPolicy(pair.first, std::make_unique<BGP>());
    }
    
    // Load ROV ASes
    graph.loadROVASes(base_path + "rov_asns.csv");
    
    // Flatten
    graph.flattenGraph();
    
    // Load announcements
    graph.loadAnnouncements(base_path + "anns.csv");
    
    // Propagate
    std::cout << "  Propagating announcements..." << std::endl;
    graph.propagateAnnouncements();
    
    // Export
    graph.exportToCSV("test_bench_output.csv");
    
    std::cout << "✓ Benchmark dataset test passed!" << std::endl;
}

// ============================================================================
// MAIN
// ============================================================================

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "BGP Simulator Test Suite" << std::endl;
    std::cout << "========================================" << std::endl;
    
    try {
        // Graph tests
        test_simple_graph();
        test_cycle_detection();
        test_caida_file();
        test_graph_flattening();
        test_flatten_caida();
        
        // Announcement tests
        test_announcement_creation();
        test_bgp_selection();
        test_as_path_manipulation();
        
        // Policy tests
        test_bgp_policy();
        test_rov_policy();
        
        // Propagation tests
        test_simple_propagation();
        test_peer_propagation();
        test_no_valley_routing();
        
        // Integration tests
        test_full_simulation_small();
        test_benchmark_dataset();
        
        std::cout << "\n========================================" << std::endl;
        std::cout << "✓ ALL TESTS PASSED!" << std::endl;
        std::cout << "========================================" << std::endl;
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "\n❌ TEST FAILED: " << e.what() << std::endl;
        return 1;
    }
}