#include "as_graph.hpp"
#include "policy.hpp"
#include "announcement.hpp"
#include <iostream>
#include <cassert>
#include <memory>

void test_propagate_up_simple() {
    std::cout << "\n=== Test: Propagate UP (Simple Chain) ===" << std::endl;
    
    ASGraph graph;
    
    /*
     * AS1 (rank 2)
     *  |
     * AS2 (rank 1)
     *  |
     * AS3 (rank 0) - announces 1.2.0.0/16
     */
    
    AS* as1 = graph.getOrCreateAS(1);
    AS* as2 = graph.getOrCreateAS(2);
    AS* as3 = graph.getOrCreateAS(3);
    
    as1->addCustomer(as2);
    as2->addProvider(as1);
    as2->addCustomer(as3);
    as3->addProvider(as2);
    
    // Give all ASes BGP policies
    as1->setPolicy(std::make_unique<BGP>());
    as2->setPolicy(std::make_unique<BGP>());
    as3->setPolicy(std::make_unique<BGP>());
    
    // Flatten graph
    graph.flattenGraph();
    
    // Seed announcement at AS3
    BGP* policy3 = dynamic_cast<BGP*>(as3->getPolicy());
    policy3->seedAnnouncement(Announcement("1.2.0.0/16", {3}, 3, "origin", false));
    
    // Propagate UP
    graph.propagateUp();
    
    // Verify AS2 received it
    BGP* policy2 = dynamic_cast<BGP*>(as2->getPolicy());
    const auto& rib2 = policy2->getLocalRIB();
    
    std::cout << "DEBUG: AS2 RIB size: " << rib2.size() << std::endl;
    if (rib2.count("1.2.0.0/16") > 0) {
        std::cout << "DEBUG: AS2 has prefix 1.2.0.0/16" << std::endl;
        std::cout << "DEBUG: AS2 path: " << rib2.at("1.2.0.0/16").getASPathString() << std::endl;
        std::cout << "DEBUG: AS2 received_from: " << rib2.at("1.2.0.0/16").getReceivedFrom() << std::endl;
    }
    
    assert(rib2.size() == 1);
    assert(rib2.count("1.2.0.0/16") == 1);
    assert(rib2.at("1.2.0.0/16").getASPathString() == "2-3");
    assert(rib2.at("1.2.0.0/16").getReceivedFrom() == "customer");
    
    // Verify AS1 received it
    BGP* policy1 = dynamic_cast<BGP*>(as1->getPolicy());
    const auto& rib1 = policy1->getLocalRIB();
    assert(rib1.size() == 1);
    assert(rib1.at("1.2.0.0/16").getASPathString() == "1-2-3");
    assert(rib1.at("1.2.0.0/16").getReceivedFrom() == "customer");
    
    std::cout << "✓ Announcements propagated UP correctly!" << std::endl;
    std::cout << "  AS3: " << policy3->getLocalRIB().at("1.2.0.0/16").getASPathString() << std::endl;
    std::cout << "  AS2: " << rib2.at("1.2.0.0/16").getASPathString() << std::endl;
    std::cout << "  AS1: " << rib1.at("1.2.0.0/16").getASPathString() << std::endl;
}

void test_propagate_across_simple() {
    std::cout << "\n=== Test: Propagate ACROSS (Peers) ===" << std::endl;
    
    ASGraph graph;
    
    /*
     * AS1 --- AS2 (peers)
     * AS1 announces 10.0.0.0/8
     */
    
    AS* as1 = graph.getOrCreateAS(1);
    AS* as2 = graph.getOrCreateAS(2);
    
    as1->addPeer(as2);
    as2->addPeer(as1);
    
    as1->setPolicy(std::make_unique<BGP>());
    as2->setPolicy(std::make_unique<BGP>());
    
    graph.flattenGraph();
    
    // Seed at AS1
    BGP* policy1 = dynamic_cast<BGP*>(as1->getPolicy());
    policy1->seedAnnouncement(Announcement("10.0.0.0/8", {1}, 1, "origin", false));
    
    // Propagate ACROSS
    graph.propagateAcross();
    
    // Verify AS2 received it
    BGP* policy2 = dynamic_cast<BGP*>(as2->getPolicy());
    const auto& rib2 = policy2->getLocalRIB();
    assert(rib2.size() == 1);
    assert(rib2.at("10.0.0.0/8").getASPathString() == "2-1");
    assert(rib2.at("10.0.0.0/8").getReceivedFrom() == "peer");
    
    std::cout << "✓ Announcements propagated ACROSS correctly!" << std::endl;
    std::cout << "  AS1: " << policy1->getLocalRIB().at("10.0.0.0/8").getASPathString() << std::endl;
    std::cout << "  AS2: " << rib2.at("10.0.0.0/8").getASPathString() << std::endl;
}

void test_propagate_down_simple() {
    std::cout << "\n=== Test: Propagate DOWN (Providers to Customers) ===" << std::endl;
    
    ASGraph graph;
    
    /*
     * AS1 (rank 2) - announces 192.0.2.0/24
     *  |
     * AS2 (rank 1)
     *  |
     * AS3 (rank 0)
     */
    
    AS* as1 = graph.getOrCreateAS(1);
    AS* as2 = graph.getOrCreateAS(2);
    AS* as3 = graph.getOrCreateAS(3);
    
    as1->addCustomer(as2);
    as2->addProvider(as1);
    as2->addCustomer(as3);
    as3->addProvider(as2);
    
    as1->setPolicy(std::make_unique<BGP>());
    as2->setPolicy(std::make_unique<BGP>());
    as3->setPolicy(std::make_unique<BGP>());
    
    graph.flattenGraph();
    
    // Seed at AS1
    BGP* policy1 = dynamic_cast<BGP*>(as1->getPolicy());
    policy1->seedAnnouncement(Announcement("192.0.2.0/24", {1}, 1, "origin", false));
    
    // Propagate DOWN
    graph.propagateDown();
    
    // Verify AS2 received it
    BGP* policy2 = dynamic_cast<BGP*>(as2->getPolicy());
    const auto& rib2 = policy2->getLocalRIB();
    assert(rib2.size() == 1);
    assert(rib2.at("192.0.2.0/24").getASPathString() == "2-1");
    assert(rib2.at("192.0.2.0/24").getReceivedFrom() == "provider");
    
    // Verify AS3 received it
    BGP* policy3 = dynamic_cast<BGP*>(as3->getPolicy());
    const auto& rib3 = policy3->getLocalRIB();
    assert(rib3.size() == 1);
    assert(rib3.at("192.0.2.0/24").getASPathString() == "3-2-1");
    assert(rib3.at("192.0.2.0/24").getReceivedFrom() == "provider");
    
    std::cout << "✓ Announcements propagated DOWN correctly!" << std::endl;
    std::cout << "  AS1: " << policy1->getLocalRIB().at("192.0.2.0/24").getASPathString() << std::endl;
    std::cout << "  AS2: " << rib2.at("192.0.2.0/24").getASPathString() << std::endl;
    std::cout << "  AS3: " << rib3.at("192.0.2.0/24").getASPathString() << std::endl;
}

void test_full_propagation_bgpsimulator() {
    std::cout << "\n=== Test: Full Propagation (bgpsimulator.com Example) ===" << std::endl;
    
    ASGraph graph;
    
    /*
     *        AS4 --- AS777 (peers)
     *       /   \      |
     *     AS3   AS666  |
     *        \________/
     *    (AS3 is also customer of AS777)
     * 
     * AS777 announces 1.2.0.0/16
     * AS666 announces 1.2.0.0/16 (hijack!)
     */
    
    AS* as3 = graph.getOrCreateAS(3);
    AS* as4 = graph.getOrCreateAS(4);
    AS* as666 = graph.getOrCreateAS(666);
    AS* as777 = graph.getOrCreateAS(777);
    
    // Setup relationships
    as4->addCustomer(as3);
    as3->addProvider(as4);
    as4->addCustomer(as666);
    as666->addProvider(as4);
    as777->addCustomer(as3);
    as3->addProvider(as777);
    as4->addPeer(as777);
    as777->addPeer(as4);
    
    // Give all BGP policies
    as3->setPolicy(std::make_unique<BGP>());
    as4->setPolicy(std::make_unique<BGP>());
    as666->setPolicy(std::make_unique<BGP>());
    as777->setPolicy(std::make_unique<BGP>());
    
    graph.flattenGraph();
    
    std::cout << "\nDEBUG: Rank assignments:" << std::endl;
    std::cout << "  AS3 rank: " << as3->getPropagationRank() << std::endl;
    std::cout << "  AS4 rank: " << as4->getPropagationRank() << std::endl;
    std::cout << "  AS666 rank: " << as666->getPropagationRank() << std::endl;
    std::cout << "  AS777 rank: " << as777->getPropagationRank() << std::endl;
    
    // Seed announcements
    BGP* policy777 = dynamic_cast<BGP*>(as777->getPolicy());
    BGP* policy666 = dynamic_cast<BGP*>(as666->getPolicy());
    
    policy777->seedAnnouncement(Announcement("1.2.0.0/16", {777}, 777, "origin", false));
    policy666->seedAnnouncement(Announcement("1.2.0.0/16", {666}, 666, "origin", false));
    
    // Full propagation
    graph.propagateAnnouncements();
    
    // Verify results
    BGP* policy3 = dynamic_cast<BGP*>(as3->getPolicy());
    BGP* policy4 = dynamic_cast<BGP*>(as4->getPolicy());
    
    const auto& rib3 = policy3->getLocalRIB();
    const auto& rib4 = policy4->getLocalRIB();
    
    std::cout << "\nDEBUG: AS3 RIB size: " << rib3.size() << std::endl;
    if (rib3.count("1.2.0.0/16") > 0) {
        std::cout << "DEBUG: AS3 path: " << rib3.at("1.2.0.0/16").getASPathString() << std::endl;
        std::cout << "DEBUG: AS3 received_from: " << rib3.at("1.2.0.0/16").getReceivedFrom() << std::endl;
    } else {
        std::cout << "DEBUG: AS3 does NOT have prefix 1.2.0.0/16!" << std::endl;
    }
    
    std::cout << "DEBUG: AS4 RIB size: " << rib4.size() << std::endl;
    if (rib4.count("1.2.0.0/16") > 0) {
        std::cout << "DEBUG: AS4 path: " << rib4.at("1.2.0.0/16").getASPathString() << std::endl;
        std::cout << "DEBUG: AS4 received_from: " << rib4.at("1.2.0.0/16").getReceivedFrom() << std::endl;
    }
    
    // AS3 should prefer AS777 (customer > provider from AS4)
    assert(rib3.at("1.2.0.0/16").getASPathString() == "3-777");
    
    // AS4 should prefer AS666 (both customers, but AS666 has shorter path)
    assert(rib4.at("1.2.0.0/16").getASPathString() == "4-666");
    
    std::cout << "✓ bgpsimulator.com example matches!" << std::endl;
    std::cout << "  AS3 chose: " << rib3.at("1.2.0.0/16").getASPathString() 
              << " (customer > provider)" << std::endl;
    std::cout << "  AS4 chose: " << rib4.at("1.2.0.0/16").getASPathString() 
              << " (shorter path)" << std::endl;
}

void test_conflict_relationship_preference() {
    std::cout << "\n=== Test: Conflict Resolution - Relationship Preference ===" << std::endl;
    
    ASGraph graph;
    
    /*
     *     AS1
     *    / | \
     *  AS2 AS3 AS4 (AS2=customer, AS3=peer, AS4=provider to AS1)
     *  
     * All announce same prefix
     */
    
    AS* as1 = graph.getOrCreateAS(1);
    AS* as2 = graph.getOrCreateAS(2);
    AS* as3 = graph.getOrCreateAS(3);
    AS* as4 = graph.getOrCreateAS(4);
    
    // AS2 is customer of AS1
    as1->addCustomer(as2);
    as2->addProvider(as1);
    
    // AS3 is peer of AS1
    as1->addPeer(as3);
    as3->addPeer(as1);
    
    // AS1 is customer of AS4 (so AS4 is provider)
    as4->addCustomer(as1);
    as1->addProvider(as4);
    
    as1->setPolicy(std::make_unique<BGP>());
    as2->setPolicy(std::make_unique<BGP>());
    as3->setPolicy(std::make_unique<BGP>());
    as4->setPolicy(std::make_unique<BGP>());
    
    graph.flattenGraph();
    
    // Seed same prefix from all
    BGP* policy2 = dynamic_cast<BGP*>(as2->getPolicy());
    BGP* policy3 = dynamic_cast<BGP*>(as3->getPolicy());
    BGP* policy4 = dynamic_cast<BGP*>(as4->getPolicy());
    
    policy2->seedAnnouncement(Announcement("100.0.0.0/8", {2}, 2, "origin", false));
    policy3->seedAnnouncement(Announcement("100.0.0.0/8", {3}, 3, "origin", false));
    policy4->seedAnnouncement(Announcement("100.0.0.0/8", {4}, 4, "origin", false));
    
    // Propagate
    graph.propagateAnnouncements();
    
    // AS1 should prefer customer (AS2) > peer (AS3) > provider (AS4)
    BGP* policy1 = dynamic_cast<BGP*>(as1->getPolicy());
    const auto& rib1 = policy1->getLocalRIB();
    
    assert(rib1.at("100.0.0.0/8").getASPathString() == "1-2");
    assert(rib1.at("100.0.0.0/8").getReceivedFrom() == "customer");
    
    std::cout << "✓ Relationship preference working correctly!" << std::endl;
    std::cout << "  AS1 chose customer (AS2) over peer and provider" << std::endl;
}

void test_conflict_path_length() {
    std::cout << "\n=== Test: Conflict Resolution - Path Length ===" << std::endl;
    
    ASGraph graph;
    
    /*
     *      AS1
     *     /   \
     *   AS2   AS3
     *    |     |
     *   AS4   AS5
     *    |
     *   AS6
     * 
     * AS6 and AS5 both announce same prefix
     * AS1 receives via AS2->AS4->AS6 (length 3) and AS3->AS5 (length 2)
     * Both are customers, should prefer shorter path
     */
    
    AS* as1 = graph.getOrCreateAS(1);
    AS* as2 = graph.getOrCreateAS(2);
    AS* as3 = graph.getOrCreateAS(3);
    AS* as4 = graph.getOrCreateAS(4);
    AS* as5 = graph.getOrCreateAS(5);
    AS* as6 = graph.getOrCreateAS(6);
    
    as1->addCustomer(as2);
    as2->addProvider(as1);
    as1->addCustomer(as3);
    as3->addProvider(as1);
    as2->addCustomer(as4);
    as4->addProvider(as2);
    as3->addCustomer(as5);
    as5->addProvider(as3);
    as4->addCustomer(as6);
    as6->addProvider(as4);
    
    for (auto asn : {1, 2, 3, 4, 5, 6}) {
        graph.getAS(asn)->setPolicy(std::make_unique<BGP>());
    }
    
    graph.flattenGraph();
    
    // Seed announcements
    dynamic_cast<BGP*>(as5->getPolicy())->seedAnnouncement(
        Announcement("200.0.0.0/8", {5}, 5, "origin", false));
    dynamic_cast<BGP*>(as6->getPolicy())->seedAnnouncement(
        Announcement("200.0.0.0/8", {6}, 6, "origin", false));
    
    graph.propagateAnnouncements();
    
    // AS1 should prefer shorter path through AS3->AS5
    const auto& rib1 = dynamic_cast<BGP*>(as1->getPolicy())->getLocalRIB();
    
    // Path via AS3: 1-3-5 (length 3)
    // Path via AS2: 1-2-4-6 (length 4)
    assert(rib1.at("200.0.0.0/8").getASPathLength() == 3);
    
    std::cout << "✓ Path length preference working correctly!" << std::endl;
    std::cout << "  AS1 chose shorter path: " << rib1.at("200.0.0.0/8").getASPathString() << std::endl;
}

void test_conflict_next_hop_tiebreaker() {
    std::cout << "\n=== Test: Conflict Resolution - Next Hop Tiebreaker ===" << std::endl;
    
    ASGraph graph;
    
    /*
     *       AS1
     *      /   \
     *   AS100  AS50  (both customers, same path length)
     * 
     * Should prefer AS50 (lower ASN)
     */
    
    AS* as1 = graph.getOrCreateAS(1);
    AS* as50 = graph.getOrCreateAS(50);
    AS* as100 = graph.getOrCreateAS(100);
    
    as1->addCustomer(as50);
    as50->addProvider(as1);
    as1->addCustomer(as100);
    as100->addProvider(as1);
    
    as1->setPolicy(std::make_unique<BGP>());
    as50->setPolicy(std::make_unique<BGP>());
    as100->setPolicy(std::make_unique<BGP>());
    
    graph.flattenGraph();
    
    // Both announce same prefix
    dynamic_cast<BGP*>(as50->getPolicy())->seedAnnouncement(
        Announcement("150.0.0.0/8", {50}, 50, "origin", false));
    dynamic_cast<BGP*>(as100->getPolicy())->seedAnnouncement(
        Announcement("150.0.0.0/8", {100}, 100, "origin", false));
    
    graph.propagateAnnouncements();
    
    // AS1 should prefer lower next hop ASN (50 < 100)
    const auto& rib1 = dynamic_cast<BGP*>(as1->getPolicy())->getLocalRIB();
    assert(rib1.at("150.0.0.0/8").getNextHopASN() == 50);
    
    std::cout << "✓ Next hop ASN tiebreaker working correctly!" << std::endl;
    std::cout << "  AS1 chose AS50 (50 < 100)" << std::endl;
}

int main() {
    std::cout << "=== Testing Announcement Propagation (Sections 3.5 & 3.6) ===" << std::endl;
    
    try {
        // Basic propagation tests
        test_propagate_up_simple();
        test_propagate_across_simple();
        test_propagate_down_simple();
        
        // Full propagation test
        test_full_propagation_bgpsimulator();
        
        // Conflict resolution tests
        test_conflict_relationship_preference();
        test_conflict_path_length();
        test_conflict_next_hop_tiebreaker();
        
        std::cout << "\n✅ ALL PROPAGATION TESTS PASSED!" << std::endl;
        std::cout << "\nSections 3.5 & 3.6 Complete!" << std::endl;
        std::cout << "Next: Section 3.7 - Output and Section 4 - ROV" << std::endl;
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n❌ Test failed: " << e.what() << std::endl;
        return 1;
    }
}