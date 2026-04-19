#include "policy.hpp"
#include "as.hpp"
#include <iostream>

/**
 * BGP::receiveAnnouncement
 * Add announcement to the received queue (not yet processed)
 */
void BGP::receiveAnnouncement(const Announcement& ann) {
    received_queue_[ann.getPrefix()].push_back(ann);
}

/**
 * BGP::processAnnouncements
 * Process all announcements in received queue and update local RIB
 * 
 * For each prefix:
 * 1. Check if we should accept each announcement
 * 2. Select the best announcement using BGP selection criteria
 * 3. Update local RIB if better than current
 * 4. Prepend our ASN to the path for storage
 */
void BGP::processAnnouncements(AS* owner) {
    if (!owner) return;
    
    uint32_t our_asn = owner->getASN();
    
    // Process each prefix in the received queue
    for (const auto& pair : received_queue_) {
        const std::string& prefix = pair.first;
        const std::vector<Announcement>& received_anns = pair.second;
        
        // Find the best announcement among all received for this prefix
        const Announcement* best_received = nullptr;
        
        for (const Announcement& ann : received_anns) {
            // Check if we should accept this announcement
            if (!shouldAcceptAnnouncement(ann)) {
                continue;  // Skip invalid announcements
            }
            
            // Check for loop prevention (our ASN already in path)
            const auto& as_path = ann.getASPath();
            bool loop_detected = false;
            for (uint32_t asn : as_path) {
                if (asn == our_asn) {
                    loop_detected = true;
                    break;
                }
            }
            
            if (loop_detected) {
                continue;  // Skip announcements with loops
            }
            
            // Compare with current best received
            if (!best_received || ann.isBetterThan(*best_received)) {
                best_received = &ann;
            }
        }
        
        // If we found a valid announcement
        if (best_received) {
            // Create a copy and prepend our ASN first
            Announcement new_ann = *best_received;
            new_ann.prependASPath(our_asn);
            
            // Check if it's better than what's in our local RIB
            auto rib_it = local_rib_.find(prefix);
            
            if (rib_it == local_rib_.end() || new_ann.isBetterThan(rib_it->second)) {
                // Store in local RIB (use insert_or_assign to avoid default constructor requirement)
                local_rib_.insert_or_assign(prefix, new_ann);
            }
        }
    }
}

/**
 * BGP::hasAnnouncement
 * Check if we have an announcement for a given prefix
 */
bool BGP::hasAnnouncement(const std::string& prefix) const {
    return local_rib_.find(prefix) != local_rib_.end();
}

/**
 * BGP::seedAnnouncement
 * Directly insert an announcement into local RIB (for origin announcements)
 */
void BGP::seedAnnouncement(const Announcement& ann) {
    local_rib_.insert_or_assign(ann.getPrefix(), ann);
}

/**
 * BGP::shouldAcceptAnnouncement
 * Check if announcement should be accepted (can be overridden by ROV)
 */
bool BGP::shouldAcceptAnnouncement(const Announcement& ann) const {
    // Standard BGP accepts all announcements
    return true;
}

/**
 * ROV::shouldAcceptAnnouncement
 * ROV drops announcements marked as invalid
 */
bool ROV::shouldAcceptAnnouncement(const Announcement& ann) const {
    // ROV rejects invalid announcements
    return !ann.isROVInvalid();
}