#pragma once

#include <cstdint>
#include <vector>
#include <memory>
#include <algorithm>
#include "policy.hpp"

class Policy;

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
    BGP* getBGPPolicy() const { return bgp_policy_; }
    
    // Setters
    void setPropagationRank(int rank) { propagation_rank_ = rank; }
    void setPolicy(std::unique_ptr<Policy> policy) {
        // Cache raw BGP* once at init so propagation avoids dynamic_cast in hot loops
        bgp_policy_ = dynamic_cast<BGP*>(policy.get());
        policy_ = std::move(policy);
    }
    
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
    uint32_t asn_;
    int propagation_rank_;
    
    std::vector<AS*> providers_;
    std::vector<AS*> customers_;
    std::vector<AS*> peers_;
    
    std::unique_ptr<Policy> policy_;
    BGP* bgp_policy_ = nullptr;
};