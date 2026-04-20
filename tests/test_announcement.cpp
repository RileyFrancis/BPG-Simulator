#include "announcement.hpp"
#include <iostream>
#include <cassert>

void test_announcement_creation() {
    std::cout << "\n=== Test: Announcement Creation ===" << std::endl;
    
    // Create an announcement as Google would (8.8.8.0/24 from AS 15169)
    Announcement google_ann(
        "8.8.8.0/24",           // prefix
        {15169},                // AS path (just Google initially)
        15169,                  // next hop (Google)
        "origin",               // received from (origin since it's the source)
        false                   // not ROV invalid
    );
    
    assert(google_ann.getPrefix() == "8.8.8.0/24");
    assert(google_ann.getASPath().size() == 1);
    assert(google_ann.getASPath()[0] == 15169);
    assert(google_ann.getNextHopASN() == 15169);
    assert(google_ann.getReceivedFrom() == "origin");
    assert(!google_ann.isROVInvalid());
    assert(google_ann.getASPathLength() == 1);
    assert(google_ann.getASPathString() == "15169");
    
    std::cout << "✓ Basic announcement creation works!" << std::endl;
    std::cout << "  Prefix: " << google_ann.getPrefix() << std::endl;
    std::cout << "  AS Path: " << google_ann.getASPathString() << std::endl;
    std::cout << "  Next Hop: " << google_ann.getNextHopASN() << std::endl;
    std::cout << "  Received From: " << google_ann.getReceivedFrom() << std::endl;
}

void test_announcement_prepend() {
    std::cout << "\n=== Test: AS Path Prepending ===" << std::endl;
    
    // Start with AS 777 announcing 1.2.0.0/16 (like bgpsimulator.com example)
    Announcement ann("1.2.0.0/16", {777}, 777, "origin", false);
    
    assert(ann.getASPathString() == "777");
    
    // AS 3 receives it and prepends itself
    ann.prependASPath(3);
    assert(ann.getASPath().size() == 2);
    assert(ann.getASPath()[0] == 3);     // Most recent is first
    assert(ann.getASPath()[1] == 777);   // Original is last
    assert(ann.getASPathString() == "3-777");
    
    // AS 4 receives it and prepends itself
    ann.prependASPath(4);
    assert(ann.getASPath().size() == 3);
    assert(ann.getASPathString() == "4-3-777");
    
    std::cout << "✓ AS path prepending works correctly!" << std::endl;
    std::cout << "  Path progression: 777 -> 3-777 -> 4-3-777" << std::endl;
}

void test_ipv6_prefix() {
    std::cout << "\n=== Test: IPv6 Prefix Support ===" << std::endl;
    
    // Test with IPv6 prefix (the class should handle any string)
    Announcement ipv6_ann(
        "2001:db8::/32",        // IPv6 prefix
        {64512},
        64512,
        "origin",
        false
    );
    
    assert(ipv6_ann.getPrefix() == "2001:db8::/32");
    
    std::cout << "✓ IPv6 prefix support works!" << std::endl;
    std::cout << "  IPv6 Prefix: " << ipv6_ann.getPrefix() << std::endl;
}

void test_rov_invalid_flag() {
    std::cout << "\n=== Test: ROV Invalid Flag ===" << std::endl;
    
    // Create an announcement with ROV invalid set
    Announcement hijack("1.2.0.0/16", {666}, 666, "origin", true);
    
    assert(hijack.isROVInvalid());
    
    // Create a valid announcement
    Announcement valid("1.2.0.0/16", {777}, 777, "origin", false);
    
    assert(!valid.isROVInvalid());
    
    std::cout << "✓ ROV invalid flag works correctly!" << std::endl;
}

void test_announcement_modification() {
    std::cout << "\n=== Test: Announcement Modification ===" << std::endl;
    
    // Create announcement
    Announcement ann;
    
    // Use setters to configure it
    ann.setPrefix("10.0.0.0/8");
    ann.setASPath({100});
    ann.setNextHopASN(100);
    ann.setReceivedFrom("origin");
    ann.setROVInvalid(false);
    
    assert(ann.getPrefix() == "10.0.0.0/8");
    assert(ann.getASPath().size() == 1);
    assert(ann.getNextHopASN() == 100);
    
    // Modify received_from and next_hop (simulating propagation)
    ann.setReceivedFrom("customer");
    ann.setNextHopASN(200);
    ann.prependASPath(200);
    
    assert(ann.getReceivedFrom() == "customer");
    assert(ann.getNextHopASN() == 200);
    assert(ann.getASPathString() == "200-100");
    
    std::cout << "✓ Announcement modification works!" << std::endl;
}

void test_bgpsimulator_example() {
    std::cout << "\n=== Test: bgpsimulator.com Example ===" << std::endl;
    
    // Recreate the example from bgpsimulator.com
    // AS 777 announces 1.2.0.0/16
    Announcement ann_at_777("1.2.0.0/16", {777}, 777, "origin", false);
    
    std::cout << "  At AS 777: " << ann_at_777.getASPathString() << std::endl;
    assert(ann_at_777.getASPathString() == "777");
    
    // AS 3 receives it from AS 777 (customer relationship)
    Announcement ann_at_3 = ann_at_777;  // Copy
    ann_at_3.prependASPath(3);
    ann_at_3.setNextHopASN(777);
    ann_at_3.setReceivedFrom("customer");
    
    std::cout << "  At AS 3: " << ann_at_3.getASPathString() << std::endl;
    assert(ann_at_3.getASPathString() == "3-777");
    
    // AS 4 receives it from AS 3 (customer relationship)
    Announcement ann_at_4 = ann_at_3;  // Copy
    ann_at_4.prependASPath(4);
    ann_at_4.setNextHopASN(3);
    ann_at_4.setReceivedFrom("customer");
    
    std::cout << "  At AS 4: " << ann_at_4.getASPathString() << std::endl;
    assert(ann_at_4.getASPathString() == "4-3-777");
    
    std::cout << "✓ bgpsimulator.com example recreated successfully!" << std::endl;
}

int main() {
    std::cout << "=== Testing Announcement Class (Section 3.1) ===" << std::endl;
    
    try {
        test_announcement_creation();
        test_announcement_prepend();
        test_ipv6_prefix();
        test_rov_invalid_flag();
        test_announcement_modification();
        test_bgpsimulator_example();
        
        std::cout << "\n✅ ALL ANNOUNCEMENT TESTS PASSED!" << std::endl;
        std::cout << "\nSection 3.1 Complete!" << std::endl;
        std::cout << "Next: Section 3.2 - Storing announcements (local RIB and received queue)" << std::endl;
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n❌ Test failed: " << e.what() << std::endl;
        return 1;
    }
}