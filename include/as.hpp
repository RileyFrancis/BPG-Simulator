#pragma once

#include <cstdint>
#include <vector>
#include <memory>
#include "policy.hpp"

// Forward declaration
class Policy;

/**
 * Autonomous System (AS) class
 * Represents a node in the internet graph with providers, customers, and peers
 */
class AS {
public:
    // Constructor
    explicit AS(uint32_t asn);
    
    // Destructor
    ~AS();
    
    // Getters
    uint32_t getASN() const { return asn_; }
    int getPropagationRank() const { return propagation_rank_; }
    const std::vector<AS*>& getProviders() const { return providers_; }
    const std::vector<AS*>& getCustomers() const { return customers_; }
    const std::vector<AS*>& getPeers() const { return peers_; }
    Policy* getPolicy() const { return policy_.get(); }
    
    // Setters
    void setPropagationRank(int rank) { propagation_rank_ = rank; }
    void setPolicy(std::unique_ptr<Policy> policy);
    
    // Relationship management
    void addProvider(AS* provider);
    void addCustomer(AS* customer);
    void addPeer(AS* peer);
    
    // Check if AS has relationships
    bool hasProviders() const { return !providers_.empty(); }
    bool hasCustomers() const { return !customers_.empty(); }
    bool hasPeers() const { return !peers_.empty(); }
    
    // Utility methods
    bool isProvider(const AS* other) const;
    bool isCustomer(const AS* other) const;
    bool isPeer(const AS* other) const;
    
    // Announcement propagation methods
    void sendAnnouncementsToProviders();
    void sendAnnouncementsToCustomers();
    void sendAnnouncementsToPeers();
    void processReceivedAnnouncements();
    void clearReceivedQueue();
    
private:
    uint32_t asn_;                              // Autonomous System Number (unique ID)
    int propagation_rank_;                      // Rank for graph flattening (-1 if unset)
    
    std::vector<AS*> providers_;                // List of provider ASes
    std::vector<AS*> customers_;                // List of customer ASes
    std::vector<AS*> peers_;                    // List of peer ASes
    
    std::unique_ptr<Policy> policy_;            // Routing policy (BGP or ROV)
    
    // Helper: Send announcement to a neighbor with appropriate relationship
    void sendAnnouncementToNeighbor(AS* neighbor, const Announcement& ann, Relationship recv_rel);
};