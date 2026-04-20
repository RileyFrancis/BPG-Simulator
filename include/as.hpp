#pragma once

#include <cstdint>
#include <vector>
#include <memory>
#include <algorithm>
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
    explicit AS(uint32_t asn) : asn_(asn), propagation_rank_(-1), policy_(nullptr) {}
    
    // Destructor
    ~AS() = default;
    
    // Getters
    uint32_t getASN() const { return asn_; }
    int getPropagationRank() const { return propagation_rank_; }
    const std::vector<AS*>& getProviders() const { return providers_; }
    const std::vector<AS*>& getCustomers() const { return customers_; }
    const std::vector<AS*>& getPeers() const { return peers_; }
    Policy* getPolicy() const { return policy_.get(); }
    
    // Setters
    void setPropagationRank(int rank) { propagation_rank_ = rank; }
    void setPolicy(std::unique_ptr<Policy> policy) { policy_ = std::move(policy); }
    
    // Relationship management
    void addProvider(AS* provider) {
        if (provider && std::find(providers_.begin(), providers_.end(), provider) == providers_.end()) {
            providers_.push_back(provider);
        }
    }

    void addCustomer(AS* customer) {
        if (customer && std::find(customers_.begin(), customers_.end(), customer) == customers_.end()) {
            customers_.push_back(customer);
        }
    }

    void addPeer(AS* peer) {
        if (peer && std::find(peers_.begin(), peers_.end(), peer) == peers_.end()) {
            peers_.push_back(peer);
        }
    }
    
    // Check if AS has relationships
    bool hasProviders() const { return !providers_.empty(); }
    bool hasCustomers() const { return !customers_.empty(); }
    bool hasPeers() const { return !peers_.empty(); }
    
    // Utility methods
    bool isProvider(const AS* other) const {
        return std::find(providers_.begin(), providers_.end(), other) != providers_.end();
    }

    bool isCustomer(const AS* other) const {
        return std::find(customers_.begin(), customers_.end(), other) != customers_.end();
    }

    bool isPeer(const AS* other) const {
        return std::find(peers_.begin(), peers_.end(), other) != peers_.end();
    }
    
private:
    uint32_t asn_;                              // Autonomous System Number (unique ID)
    int propagation_rank_;                      // Rank for graph flattening (-1 if unset)
    
    std::vector<AS*> providers_;                // List of provider ASes
    std::vector<AS*> customers_;                // List of customer ASes
    std::vector<AS*> peers_;                    // List of peer ASes
    
    std::unique_ptr<Policy> policy_;            // Routing policy (BGP or ROV)
};