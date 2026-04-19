#pragma once

#include <unordered_map>
#include <vector>
#include <memory>
#include "announcement.hpp"

// Forward declaration
class AS;

/**
 * Abstract base class for routing policies
 */
class Policy {
public:
    virtual ~Policy() = default;
    
    // Process incoming announcements and update local RIB
    virtual void processAnnouncements(AS* owner) = 0;
    
    // Receive an announcement (add to received queue)
    virtual void receiveAnnouncement(const Announcement& ann) = 0;
    
    // Get the local RIB (best announcements)
    virtual const std::unordered_map<std::string, Announcement>& getLocalRib() const = 0;
    
    // Check if we have an announcement for a prefix
    virtual bool hasAnnouncement(const std::string& prefix) const = 0;
    
    // Clear the received queue after processing
    virtual void clearReceivedQueue() = 0;
    
    // Seed an announcement (for originating announcements)
    virtual void seedAnnouncement(const Announcement& ann) = 0;
};

/**
 * BGP Policy - standard BGP route selection
 */
class BGP : public Policy {
public:
    BGP() = default;
    virtual ~BGP() = default;
    
    // Process announcements from received queue
    void processAnnouncements(AS* owner) override;
    
    // Receive an announcement
    void receiveAnnouncement(const Announcement& ann) override;
    
    // Get local RIB
    const std::unordered_map<std::string, Announcement>& getLocalRib() const override {
        return local_rib_;
    }
    
    // Check if announcement exists for prefix
    bool hasAnnouncement(const std::string& prefix) const override;
    
    // Clear received queue
    void clearReceivedQueue() override {
        received_queue_.clear();
    }
    
    // Seed an announcement directly into local RIB
    void seedAnnouncement(const Announcement& ann) override;
    
protected:
    // Local RIB: one best announcement per prefix
    std::unordered_map<std::string, Announcement> local_rib_;
    
    // Received queue: all announcements received for each prefix (not yet processed)
    std::unordered_map<std::string, std::vector<Announcement>> received_queue_;
    
    // Helper: Process a single announcement (can be overridden by ROV)
    virtual bool shouldAcceptAnnouncement(const Announcement& ann) const;
};

/**
 * ROV Policy - BGP with Route Origin Validation
 * Drops announcements marked as ROV invalid
 */
class ROV : public BGP {
public:
    ROV() = default;
    ~ROV() override = default;
    
protected:
    // Override to reject ROV invalid announcements
    bool shouldAcceptAnnouncement(const Announcement& ann) const override;
};