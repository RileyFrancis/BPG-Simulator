#pragma once

#include "announcement.hpp"
#include <unordered_map>
#include <vector>
#include <string>

/**
 * Abstract Policy class
 * Base class for routing policies (BGP, ROV, etc.)
 */
class Policy {
public:
    virtual ~Policy() = default;
    
    // Seed an announcement (for origin ASes that own the prefix)
    virtual void seedAnnouncement(const Announcement& ann) = 0;
    
    // Receive an announcement from a neighbor
    virtual void receiveAnnouncement(const Announcement& ann) = 0;
    
    // Process all received announcements and update local RIB
    virtual void processReceivedAnnouncements() = 0;
    
    // Get all announcements to send to neighbors
    virtual std::vector<Announcement> getAnnouncementsToSend() const = 0;
    
    // Get the local RIB (for output/inspection)
    virtual const std::unordered_map<std::string, Announcement>& getLocalRIB() const = 0;
    
    // Clear the received queue (used after processing)
    virtual void clearReceivedQueue() = 0;
};

/**
 * BGP Policy Implementation
 * Stores announcements in a local RIB and processes them according to BGP rules
 */
class BGP : public Policy {
public:
    BGP() = default;
    virtual ~BGP() = default;
    
    /**
     * Seed an announcement (for origin ASes)
     * Directly adds to local RIB without going through received queue
     */
    void seedAnnouncement(const Announcement& ann) override {
        // Origin announcements go directly into local RIB
        local_rib_[ann.getPrefix()] = ann;
    }
    
    /**
     * Receive an announcement from a neighbor
     * Adds to received queue for later processing
     */
    void receiveAnnouncement(const Announcement& ann) override {
        // Add to the received queue for this prefix
        received_queue_[ann.getPrefix()].push_back(ann);
    }
    
    /**
     * Process all received announcements WITH prepending
     * For each prefix, select the best announcement and update local RIB
     * @param asn The ASN of the AS that owns this policy (for prepending to AS path)
     */
    void processReceivedAnnouncements(uint32_t asn) {
        // Process each prefix in the received queue
        for (auto& pair : received_queue_) {
            const std::string& prefix = pair.first;
            std::vector<Announcement>& announcements = pair.second;
            
            if (announcements.empty()) {
                continue;
            }
            
            // Find the best announcement for this prefix from received queue
            Announcement best = announcements[0];
            for (size_t i = 1; i < announcements.size(); ++i) {
                best = selectBest(best, announcements[i]);
            }
            
            // Check if we already have an announcement for this prefix
            auto rib_it = local_rib_.find(prefix);
            if (rib_it != local_rib_.end()) {
                // Compare with existing announcement
                Announcement existing = rib_it->second;
                Announcement new_candidate = best;
                
                // Prepend to the NEW candidate only
                new_candidate.prependASPath(asn);
                
                // Now select between existing and new
                best = selectBest(existing, new_candidate);
                
                // Store the winner (no additional prepending needed)
                local_rib_[prefix] = best;
            } else {
                // No existing announcement - prepend and store
                best.prependASPath(asn);
                local_rib_[prefix] = best;
            }
        }
    }
    
    /**
     * Process all received announcements WITHOUT prepending
     * This is the base interface version for backward compatibility
     */
    void processReceivedAnnouncements() override {
        // Process without prepending - used for testing
        for (auto& pair : received_queue_) {
            const std::string& prefix = pair.first;
            std::vector<Announcement>& announcements = pair.second;
            
            if (announcements.empty()) {
                continue;
            }
            
            // Find the best announcement for this prefix
            Announcement best = announcements[0];
            for (size_t i = 1; i < announcements.size(); ++i) {
                best = selectBest(best, announcements[i]);
            }
            
            // Check if we already have an announcement for this prefix
            auto rib_it = local_rib_.find(prefix);
            if (rib_it != local_rib_.end()) {
                // Compare with existing announcement
                best = selectBest(rib_it->second, best);
            }
            
            // Store without prepending
            local_rib_[prefix] = best;
        }
    }
    
    /**
     * Get all announcements to send to neighbors
     * Returns all announcements in the local RIB
     */
    std::vector<Announcement> getAnnouncementsToSend() const override {
        std::vector<Announcement> announcements;
        announcements.reserve(local_rib_.size());
        
        for (const auto& pair : local_rib_) {
            announcements.push_back(pair.second);
        }
        
        return announcements;
    }
    
    /**
     * Get the local RIB
     */
    const std::unordered_map<std::string, Announcement>& getLocalRIB() const override {
        return local_rib_;
    }
    
    /**
     * Clear the received queue
     * Called after processing announcements
     */
    void clearReceivedQueue() override {
        received_queue_.clear();
    }
    
protected:
    /**
     * Select the best announcement between two announcements
     *
     * BGP Selection Rules (in order):
     * 1. Prefer better relationship: origin > customer > peer > provider
     * 2. Prefer shorter AS path
     * 3. Prefer lower next hop ASN (tiebreaker)
     */
    virtual Announcement selectBest(const Announcement& a, const Announcement& b) {
        // Rule 1: Compare relationships — enum values encode preference directly
        int a_pref = static_cast<int>(a.getRelation());
        int b_pref = static_cast<int>(b.getRelation());

        if (a_pref > b_pref) {
            return a;
        } else if (b_pref > a_pref) {
            return b;
        }

        // Rule 2: Compare AS path lengths (shorter is better)
        if (a.getASPathLength() < b.getASPathLength()) {
            return a;
        } else if (b.getASPathLength() < a.getASPathLength()) {
            return b;
        }

        // Rule 3: Compare next hop ASN (lower is better)
        if (a.getNextHopASN() < b.getNextHopASN()) {
            return a;
        } else {
            return b;
        }
    }
    
    // Local RIB: prefix -> best announcement
    std::unordered_map<std::string, Announcement> local_rib_;

    // Received queue: prefix -> list of received announcements
    std::unordered_map<std::string, std::vector<Announcement>> received_queue_;
};

/**
 * ROV (Route Origin Validation) Policy
 * Extends BGP by dropping any announcement marked rov_invalid before queuing it.
 * All other BGP logic (selection, propagation) is inherited unchanged.
 */
class ROV : public BGP {
public:
    ROV() = default;
    ~ROV() override = default;

    void receiveAnnouncement(const Announcement& ann) override {
        if (ann.isROVInvalid()) {
            return;
        }
        BGP::receiveAnnouncement(ann);
    }
};