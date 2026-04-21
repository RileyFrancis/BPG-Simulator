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
     * Process all received announcements
     * For each prefix, select the best announcement and update local RIB
     */
    void processReceivedAnnouncements() override {
        // Process each prefix in the received queue
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
            
            // Store the best announcement in local RIB
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
        // Rule 1: Compare relationships
        int a_pref = getRelationshipPreference(a.getReceivedFrom());
        int b_pref = getRelationshipPreference(b.getReceivedFrom());
        
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
    
    /**
     * Get numerical preference for relationship type
     * Higher number = more preferred
     */
    int getRelationshipPreference(const std::string& relationship) const {
        if (relationship == "origin") return 4;      // Most preferred
        if (relationship == "customer") return 3;
        if (relationship == "peer") return 2;
        if (relationship == "provider") return 1;    // Least preferred
        return 0;  // Unknown
    }
    
    // Local RIB: prefix -> best announcement
    std::unordered_map<std::string, Announcement> local_rib_;
    
    // Received queue: prefix -> list of received announcements
    std::unordered_map<std::string, std::vector<Announcement>> received_queue_;
};