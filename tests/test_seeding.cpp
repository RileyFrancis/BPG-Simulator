#include "as_graph.hpp"
#include "policy.hpp"
#include "announcement.hpp"
#include <iostream>
#include <fstream>
#include <cassert>
#include <memory>
#include <vector>

void test_simple_seeding() {
    std::cout << "\n=== Test: Simple Announcement Seeding ===" << std::endl;
    
    ASGraph graph;
    
    // Create an AS
    AS* as1 = graph.getOrCreateAS(1);
    
    // Give it a BGP policy
    as1->setPolicy(std::make_unique<BGP>());
    
    // Create an announcement (1.2.0.0/16 from AS 1)
    Announcement ann("1.2.0.0/16", {1}, 1, "origin", false);
    
    // Seed the announcement
    BGP* policy = dynamic_cast<BGP*>(as1->getPolicy());
    assert(policy != nullptr);
    
    policy->seedAnnouncement(ann);
    
    // Verify it's in the local RIB
    const auto& rib = policy->getLocalRIB();
    assert(rib.size() == 1);
    assert(rib.count("1.2.0.0/16") == 1);
    assert(rib.at("1.2.0.0/16").getASPathString() == "1");
    assert(rib.at("1.2.0.0/16").getReceivedFrom() == "origin");
    assert(rib.at("1.2.0.0/16").getNextHopASN() == 1);
    
    std::cout << "✓ Announcement seeded successfully!" << std::endl;
    std::cout << "  AS: " << as1->getASN() << std::endl;
    std::cout << "  Prefix: " << ann.getPrefix() << std::endl;
    std::cout << "  AS Path: " << ann.getASPathString() << std::endl;
    std::cout << "  Local RIB size: " << rib.size() << std::endl;
}

void test_multiple_announcements_different_prefixes() {
    std::cout << "\n=== Test: Multiple Announcements (Different Prefixes) ===" << std::endl;
    
    ASGraph graph;
    AS* as15169 = graph.getOrCreateAS(15169);  // Google
    
    as15169->setPolicy(std::make_unique<BGP>());
    BGP* policy = dynamic_cast<BGP*>(as15169->getPolicy());
    
    // Seed multiple announcements
    policy->seedAnnouncement(Announcement("8.8.8.0/24", {15169}, 15169, "origin", false));
    policy->seedAnnouncement(Announcement("8.8.4.0/24", {15169}, 15169, "origin", false));
    policy->seedAnnouncement(Announcement("2001:4860::/32", {15169}, 15169, "origin", false));
    
    const auto& rib = policy->getLocalRIB();
    assert(rib.size() == 3);
    assert(rib.count("8.8.8.0/24") == 1);
    assert(rib.count("8.8.4.0/24") == 1);
    assert(rib.count("2001:4860::/32") == 1);
    
    std::cout << "✓ Multiple announcements seeded successfully!" << std::endl;
    std::cout << "  Total prefixes in RIB: " << rib.size() << std::endl;
}

void test_seeding_multiple_ases() {
    std::cout << "\n=== Test: Seeding Multiple ASes ===" << std::endl;
    
    ASGraph graph;
    
    // Create several ASes and seed announcements
    struct SeedInfo {
        uint32_t asn;
        std::string prefix;
    };
    
    std::vector<SeedInfo> seeds = {
        {1, "1.0.0.0/8"},
        {777, "1.2.0.0/16"},
        {666, "1.2.0.0/16"},  // Same prefix as AS 777 (hijack scenario)
        {15169, "8.8.8.0/24"},
        {13335, "1.1.1.0/24"}
    };
    
    for (const auto& seed : seeds) {
        AS* as = graph.getOrCreateAS(seed.asn);
        as->setPolicy(std::make_unique<BGP>());
        
        Announcement ann(seed.prefix, {seed.asn}, seed.asn, "origin", false);
        BGP* policy = dynamic_cast<BGP*>(as->getPolicy());
        policy->seedAnnouncement(ann);
        
        // Verify
        const auto& rib = policy->getLocalRIB();
        assert(rib.size() == 1);
        assert(rib.count(seed.prefix) == 1);
    }
    
    std::cout << "✓ Multiple ASes seeded successfully!" << std::endl;
    std::cout << "  Seeded " << seeds.size() << " ASes" << std::endl;
}

void test_origin_relationship_priority() {
    std::cout << "\n=== Test: Origin Relationship Always Wins ===" << std::endl;
    
    ASGraph graph;
    AS* as1 = graph.getOrCreateAS(1);
    as1->setPolicy(std::make_unique<BGP>());
    BGP* policy = dynamic_cast<BGP*>(as1->getPolicy());
    
    // Seed an origin announcement
    Announcement origin_ann("10.0.0.0/8", {1}, 1, "origin", false);
    policy->seedAnnouncement(origin_ann);
    
    // Try to receive a "better" announcement from customer
    Announcement customer_ann("10.0.0.0/8", {2}, 2, "customer", false);
    policy->receiveAnnouncement(customer_ann);
    policy->processReceivedAnnouncements();
    
    // Origin should still win (origin > customer > peer > provider)
    const auto& rib = policy->getLocalRIB();
    assert(rib.at("10.0.0.0/8").getReceivedFrom() == "origin");
    assert(rib.at("10.0.0.0/8").getNextHopASN() == 1);
    
    std::cout << "✓ Origin relationship correctly prioritized!" << std::endl;
    std::cout << "  Origin beats customer (as expected)" << std::endl;
}

void test_seeding_with_graph_topology() {
    std::cout << "\n=== Test: Seeding with Real Graph Topology ===" << std::endl;
    
    ASGraph graph;
    
    // Create bgpsimulator.com topology
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
    
    // Give each AS a BGP policy
    as3->setPolicy(std::make_unique<BGP>());
    as4->setPolicy(std::make_unique<BGP>());
    as666->setPolicy(std::make_unique<BGP>());
    as777->setPolicy(std::make_unique<BGP>());
    
    // Seed announcements at AS 777 and AS 666
    BGP* policy777 = dynamic_cast<BGP*>(as777->getPolicy());
    BGP* policy666 = dynamic_cast<BGP*>(as666->getPolicy());
    
    policy777->seedAnnouncement(Announcement("1.2.0.0/16", {777}, 777, "origin", false));
    policy666->seedAnnouncement(Announcement("1.2.0.0/16", {666}, 666, "origin", false));
    
    // Verify both have the announcement in their RIB
    assert(policy777->getLocalRIB().size() == 1);
    assert(policy666->getLocalRIB().size() == 1);
    assert(policy777->getLocalRIB().at("1.2.0.0/16").getASPathString() == "777");
    assert(policy666->getLocalRIB().at("1.2.0.0/16").getASPathString() == "666");
    
    std::cout << "✓ Seeded bgpsimulator.com topology!" << std::endl;
    std::cout << "  AS 777: " << policy777->getLocalRIB().at("1.2.0.0/16").getASPathString() << std::endl;
    std::cout << "  AS 666: " << policy666->getLocalRIB().at("1.2.0.0/16").getASPathString() << std::endl;
}

void test_rov_invalid_seeding() {
    std::cout << "\n=== Test: Seeding ROV Invalid Announcements ===" << std::endl;
    
    ASGraph graph;
    AS* as666 = graph.getOrCreateAS(666);
    as666->setPolicy(std::make_unique<BGP>());
    BGP* policy = dynamic_cast<BGP*>(as666->getPolicy());
    
    // Seed an announcement with ROV invalid flag set
    Announcement hijack("1.2.0.0/16", {666}, 666, "origin", true);
    policy->seedAnnouncement(hijack);
    
    // Should still be in local RIB (origin AS can announce whatever it wants)
    const auto& rib = policy->getLocalRIB();
    assert(rib.size() == 1);
    assert(rib.at("1.2.0.0/16").isROVInvalid());
    
    std::cout << "✓ ROV invalid announcement seeded successfully!" << std::endl;
    std::cout << "  (Will be filtered by ROV-enabled ASes during propagation)" << std::endl;
}

void test_seeding_ipv4_and_ipv6() {
    std::cout << "\n=== Test: Seeding IPv4 and IPv6 Prefixes ===" << std::endl;
    
    ASGraph graph;
    AS* as1 = graph.getOrCreateAS(1);
    as1->setPolicy(std::make_unique<BGP>());
    BGP* policy = dynamic_cast<BGP*>(as1->getPolicy());
    
    // Seed both IPv4 and IPv6
    policy->seedAnnouncement(Announcement("192.0.2.0/24", {1}, 1, "origin", false));
    policy->seedAnnouncement(Announcement("2001:db8::/32", {1}, 1, "origin", false));
    policy->seedAnnouncement(Announcement("10.0.0.0/8", {1}, 1, "origin", false));
    policy->seedAnnouncement(Announcement("2001:4860:4860::/48", {1}, 1, "origin", false));
    
    const auto& rib = policy->getLocalRIB();
    assert(rib.size() == 4);
    assert(rib.count("192.0.2.0/24") == 1);
    assert(rib.count("2001:db8::/32") == 1);
    
    std::cout << "✓ Both IPv4 and IPv6 prefixes seeded successfully!" << std::endl;
    std::cout << "  Total prefixes: " << rib.size() << std::endl;
}

void test_seeding_real_caida_graph() {
    std::cout << "\n=== Test: Seeding on Real CAIDA Graph ===" << std::endl;
    
    ASGraph graph;
    
    // Try to load CAIDA file
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
        return;
    }
    
    std::cout << "Loading CAIDA topology..." << std::endl;
    if (!graph.buildFromCAIDAFile(caida_file)) {
        std::cerr << "Failed to load CAIDA file" << std::endl;
        return;
    }
    
    // Pick some real ASes and seed announcements
    struct RealAS {
        uint32_t asn;
        std::string name;
        std::string prefix;
    };
    
    std::vector<RealAS> real_ases = {
        {15169, "Google", "8.8.8.0/24"},
        {13335, "Cloudflare", "1.1.1.0/24"},
        {174, "Cogent", "38.0.0.0/8"},
        {3356, "Level3", "4.0.0.0/8"}
    };
    
    int seeded = 0;
    for (const auto& real_as : real_ases) {
        AS* as = graph.getAS(real_as.asn);
        if (as) {
            as->setPolicy(std::make_unique<BGP>());
            BGP* policy = dynamic_cast<BGP*>(as->getPolicy());
            
            Announcement ann(real_as.prefix, {real_as.asn}, real_as.asn, "origin", false);
            policy->seedAnnouncement(ann);
            
            // Verify
            const auto& rib = policy->getLocalRIB();
            assert(rib.size() == 1);
            assert(rib.count(real_as.prefix) == 1);
            
            std::cout << "  ✓ Seeded AS " << real_as.asn << " (" << real_as.name 
                      << "): " << real_as.prefix << std::endl;
            seeded++;
        }
    }
    
    std::cout << "✓ Seeded " << seeded << " real ASes on CAIDA graph!" << std::endl;
}

void test_get_announcements_after_seeding() {
    std::cout << "\n=== Test: Get Announcements After Seeding ===" << std::endl;
    
    ASGraph graph;
    AS* as1 = graph.getOrCreateAS(1);
    as1->setPolicy(std::make_unique<BGP>());
    BGP* policy = dynamic_cast<BGP*>(as1->getPolicy());
    
    // Seed multiple announcements
    policy->seedAnnouncement(Announcement("1.0.0.0/8", {1}, 1, "origin", false));
    policy->seedAnnouncement(Announcement("2.0.0.0/8", {1}, 1, "origin", false));
    policy->seedAnnouncement(Announcement("3.0.0.0/8", {1}, 1, "origin", false));
    
    // Get announcements to send
    auto announcements = policy->getAnnouncementsToSend();
    
    assert(announcements.size() == 3);
    
    // All should be origin announcements
    for (const auto& ann : announcements) {
        assert(ann.getReceivedFrom() == "origin");
        assert(ann.getASPathLength() == 1);
        assert(ann.getNextHopASN() == 1);
    }
    
    std::cout << "✓ Successfully retrieved " << announcements.size() 
              << " announcements for sending!" << std::endl;
}

int main() {
    std::cout << "=== Testing Announcement Seeding (Section 3.4) ===" << std::endl;
    
    try {
        test_simple_seeding();
        test_multiple_announcements_different_prefixes();
        test_seeding_multiple_ases();
        test_origin_relationship_priority();
        test_seeding_with_graph_topology();
        test_rov_invalid_seeding();
        test_seeding_ipv4_and_ipv6();
        test_get_announcements_after_seeding();
        test_seeding_real_caida_graph();
        
        std::cout << "\n✅ ALL SEEDING TESTS PASSED!" << std::endl;
        std::cout << "\nSection 3.4 Complete!" << std::endl;
        std::cout << "Next: Section 3.5 - Propagating announcements (UP, ACROSS, DOWN)" << std::endl;
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n❌ Test failed: " << e.what() << std::endl;
        return 1;
    }
}