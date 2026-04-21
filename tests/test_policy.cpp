#include "policy.hpp"
#include "announcement.hpp"
#include <iostream>
#include <cassert>
#include <memory>

void test_bgp_creation() {
    std::cout << "\n=== Test: BGP Policy Creation ===" << std::endl;
    
    auto bgp = std::make_unique<BGP>();
    
    assert(bgp != nullptr);
    assert(bgp->getLocalRIB().empty());
    
    std::cout << "✓ BGP policy created successfully" << std::endl;
}

void test_seed_announcement() {
    std::cout << "\n=== Test: Seed Announcement ===" << std::endl;
    
    BGP bgp;
    
    // Create an announcement from AS 777 (origin)
    Announcement ann("1.2.0.0/16", {777}, 777, "origin", false);
    
    // Seed it (origin announcements go directly to local RIB)
    bgp.seedAnnouncement(ann);
    
    // Verify it's in the local RIB
    const auto& rib = bgp.getLocalRIB();
    assert(rib.size() == 1);
    assert(rib.count("1.2.0.0/16") == 1);
    assert(rib.at("1.2.0.0/16").getASPathString() == "777");
    
    std::cout << "✓ Announcement seeded into local RIB" << std::endl;
    std::cout << "  Prefix: " << rib.at("1.2.0.0/16").getPrefix() << std::endl;
    std::cout << "  AS Path: " << rib.at("1.2.0.0/16").getASPathString() << std::endl;
}

void test_receive_and_process() {
    std::cout << "\n=== Test: Receive and Process Announcements ===" << std::endl;
    
    BGP bgp;
    
    // Receive an announcement
    Announcement ann("8.8.8.0/24", {15169}, 15169, "customer", false);
    bgp.receiveAnnouncement(ann);
    
    // Local RIB should still be empty (not processed yet)
    assert(bgp.getLocalRIB().empty());
    
    // Process received announcements
    bgp.processReceivedAnnouncements();
    
    // Now it should be in local RIB
    const auto& rib = bgp.getLocalRIB();
    assert(rib.size() == 1);
    assert(rib.count("8.8.8.0/24") == 1);
    assert(rib.at("8.8.8.0/24").getASPathString() == "15169");
    
    std::cout << "✓ Announcement received, processed, and stored in RIB" << std::endl;
}

void test_multiple_announcements_same_prefix() {
    std::cout << "\n=== Test: Multiple Announcements for Same Prefix ===" << std::endl;
    
    BGP bgp;
    
    // Receive two announcements for the same prefix
    // One from customer, one from provider
    Announcement from_customer("1.2.0.0/16", {100}, 100, "customer", false);
    Announcement from_provider("1.2.0.0/16", {200}, 200, "provider", false);
    
    bgp.receiveAnnouncement(from_customer);
    bgp.receiveAnnouncement(from_provider);
    
    // Process
    bgp.processReceivedAnnouncements();
    
    // Should prefer customer over provider
    const auto& rib = bgp.getLocalRIB();
    assert(rib.size() == 1);
    assert(rib.at("1.2.0.0/16").getNextHopASN() == 100);  // Customer
    assert(rib.at("1.2.0.0/16").getReceivedFrom() == "customer");
    
    std::cout << "✓ Correctly selected customer announcement over provider" << std::endl;
    std::cout << "  Selected: AS " << rib.at("1.2.0.0/16").getNextHopASN() 
              << " (customer)" << std::endl;
}

void test_relationship_preference() {
    std::cout << "\n=== Test: Relationship Preference Order ===" << std::endl;
    
    BGP bgp;
    
    // Test all relationship preferences: origin > customer > peer > provider
    Announcement origin("1.0.0.0/8", {1}, 1, "origin", false);
    Announcement customer("1.0.0.0/8", {2}, 2, "customer", false);
    Announcement peer("1.0.0.0/8", {3}, 3, "peer", false);
    Announcement provider("1.0.0.0/8", {4}, 4, "provider", false);
    
    // Receive in random order
    bgp.receiveAnnouncement(provider);
    bgp.receiveAnnouncement(peer);
    bgp.receiveAnnouncement(origin);
    bgp.receiveAnnouncement(customer);
    
    bgp.processReceivedAnnouncements();
    
    // Should select origin (highest preference)
    const auto& rib = bgp.getLocalRIB();
    assert(rib.at("1.0.0.0/8").getReceivedFrom() == "origin");
    
    std::cout << "✓ Correctly prefers: origin > customer > peer > provider" << std::endl;
}

void test_path_length_tiebreaker() {
    std::cout << "\n=== Test: AS Path Length Tiebreaker ===" << std::endl;
    
    BGP bgp;
    
    // Same relationship (customer), different path lengths
    Announcement short_path("2.0.0.0/8", {100}, 100, "customer", false);
    Announcement long_path("2.0.0.0/8", {200, 300, 400}, 200, "customer", false);
    
    bgp.receiveAnnouncement(long_path);
    bgp.receiveAnnouncement(short_path);
    
    bgp.processReceivedAnnouncements();
    
    // Should prefer shorter path
    const auto& rib = bgp.getLocalRIB();
    assert(rib.at("2.0.0.0/8").getASPathLength() == 1);
    assert(rib.at("2.0.0.0/8").getNextHopASN() == 100);
    
    std::cout << "✓ Correctly prefers shorter AS path" << std::endl;
    std::cout << "  Selected path length: " << rib.at("2.0.0.0/8").getASPathLength() << std::endl;
}

void test_next_hop_tiebreaker() {
    std::cout << "\n=== Test: Next Hop ASN Tiebreaker ===" << std::endl;
    
    BGP bgp;
    
    // Same relationship, same path length, different next hop
    Announcement high_asn("3.0.0.0/8", {500}, 500, "customer", false);
    Announcement low_asn("3.0.0.0/8", {100}, 100, "customer", false);
    
    bgp.receiveAnnouncement(high_asn);
    bgp.receiveAnnouncement(low_asn);
    
    bgp.processReceivedAnnouncements();
    
    // Should prefer lower ASN
    const auto& rib = bgp.getLocalRIB();
    assert(rib.at("3.0.0.0/8").getNextHopASN() == 100);
    
    std::cout << "✓ Correctly prefers lower next hop ASN" << std::endl;
    std::cout << "  Selected ASN: " << rib.at("3.0.0.0/8").getNextHopASN() << std::endl;
}

void test_clear_received_queue() {
    std::cout << "\n=== Test: Clear Received Queue ===" << std::endl;
    
    BGP bgp;
    
    // Receive some announcements
    Announcement ann1("4.0.0.0/8", {100}, 100, "customer", false);
    Announcement ann2("5.0.0.0/8", {200}, 200, "customer", false);
    
    bgp.receiveAnnouncement(ann1);
    bgp.receiveAnnouncement(ann2);
    
    // Process
    bgp.processReceivedAnnouncements();
    
    // Local RIB should have 2 entries
    assert(bgp.getLocalRIB().size() == 2);
    
    // Clear received queue
    bgp.clearReceivedQueue();
    
    // Local RIB should still have entries (clearing queue doesn't affect RIB)
    assert(bgp.getLocalRIB().size() == 2);
    
    std::cout << "✓ Received queue cleared, local RIB preserved" << std::endl;
}

void test_get_announcements_to_send() {
    std::cout << "\n=== Test: Get Announcements to Send ===" << std::endl;
    
    BGP bgp;
    
    // Seed some announcements
    bgp.seedAnnouncement(Announcement("1.0.0.0/8", {100}, 100, "origin", false));
    bgp.seedAnnouncement(Announcement("2.0.0.0/8", {200}, 200, "origin", false));
    bgp.seedAnnouncement(Announcement("3.0.0.0/8", {300}, 300, "origin", false));
    
    // Get announcements to send
    auto announcements = bgp.getAnnouncementsToSend();
    
    assert(announcements.size() == 3);
    
    std::cout << "✓ Retrieved all announcements from local RIB" << std::endl;
    std::cout << "  Count: " << announcements.size() << std::endl;
}

void test_update_existing_announcement() {
    std::cout << "\n=== Test: Update Existing Announcement ===" << std::endl;
    
    BGP bgp;
    
    // Seed an announcement from provider
    Announcement initial("6.0.0.0/8", {100}, 100, "provider", false);
    bgp.seedAnnouncement(initial);
    
    assert(bgp.getLocalRIB().at("6.0.0.0/8").getReceivedFrom() == "provider");
    
    // Receive a better announcement (from customer)
    Announcement better("6.0.0.0/8", {200}, 200, "customer", false);
    bgp.receiveAnnouncement(better);
    bgp.processReceivedAnnouncements();
    
    // Should have updated to the better announcement
    const auto& rib = bgp.getLocalRIB();
    assert(rib.at("6.0.0.0/8").getReceivedFrom() == "customer");
    assert(rib.at("6.0.0.0/8").getNextHopASN() == 200);
    
    std::cout << "✓ Existing announcement updated with better option" << std::endl;
}

void test_bgpsimulator_example() {
    std::cout << "\n=== Test: bgpsimulator.com Example Scenario ===" << std::endl;
    
    // Simulate AS 4 receiving announcements for 1.2.0.0/16 from two customers
    BGP as4_bgp;
    
    // From AS 666 (customer) - path length 1
    Announcement from_666("1.2.0.0/16", {666}, 666, "customer", false);
    
    // From AS 3 (customer) - path length 2 (3-777)
    Announcement from_3("1.2.0.0/16", {3, 777}, 3, "customer", false);
    
    as4_bgp.receiveAnnouncement(from_666);
    as4_bgp.receiveAnnouncement(from_3);
    as4_bgp.processReceivedAnnouncements();
    
    // Should prefer AS 666 (shorter path, same relationship)
    const auto& rib = as4_bgp.getLocalRIB();
    assert(rib.at("1.2.0.0/16").getNextHopASN() == 666);
    assert(rib.at("1.2.0.0/16").getASPathLength() == 1);
    
    std::cout << "✓ AS 4 correctly selects AS 666 (shorter path)" << std::endl;
    std::cout << "  Selected path: " << rib.at("1.2.0.0/16").getASPathString() << std::endl;
}

int main() {
    std::cout << "=== Testing BGP Policy Class (Section 3.2) ===" << std::endl;
    
    try {
        test_bgp_creation();
        test_seed_announcement();
        test_receive_and_process();
        test_multiple_announcements_same_prefix();
        test_relationship_preference();
        test_path_length_tiebreaker();
        test_next_hop_tiebreaker();
        test_clear_received_queue();
        test_get_announcements_to_send();
        test_update_existing_announcement();
        test_bgpsimulator_example();
        
        std::cout << "\n✅ ALL BGP POLICY TESTS PASSED!" << std::endl;
        std::cout << "\nSection 3.2 Complete!" << std::endl;
        std::cout << "Next: Section 3.3 - Flattening the graph and propagation" << std::endl;
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n❌ Test failed: " << e.what() << std::endl;
        return 1;
    }
}