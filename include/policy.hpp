#pragma once

#include "announcement.hpp"
#include <unordered_map>
#include <vector>
#include <string>


class Policy {
public:
    virtual ~Policy() = default;
    virtual void seedAnnouncement(const Announcement& ann) = 0;
    virtual void receiveAnnouncement(const Announcement& ann) = 0;
    virtual void processReceivedAnnouncements() = 0;
    virtual std::vector<Announcement> getAnnouncementsToSend() const = 0;
    virtual const std::unordered_map<std::string, Announcement>& getLocalRIB() const = 0;
    virtual void clearReceivedQueue() = 0;
};


class BGP : public Policy {
public:
    BGP() = default;
    virtual ~BGP() = default;

    void seedAnnouncement(const Announcement& ann) override {
        // Origin announcements go directly into local RIB
        local_rib_[ann.getPrefix()] = ann;
    }

    void receiveAnnouncement(const Announcement& ann) override {
        // Add to the received queue for this prefix
        received_queue_[ann.getPrefix()].push_back(ann);
    }
    
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
    
    std::vector<Announcement> getAnnouncementsToSend() const override {
        std::vector<Announcement> announcements;
        announcements.reserve(local_rib_.size());
        
        for (const auto& pair : local_rib_) {
            announcements.push_back(pair.second);
        }
        
        return announcements;
    }

    const std::unordered_map<std::string, Announcement>& getLocalRIB() const override {
        return local_rib_;
    }
    
    void clearReceivedQueue() override {
        received_queue_.clear();
    }
    
protected:
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
    
    std::unordered_map<std::string, Announcement> local_rib_;
    std::unordered_map<std::string, std::vector<Announcement>> received_queue_;
};


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