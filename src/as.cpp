#include "as.hpp"
#include "announcement.hpp"
#include <algorithm>

/**
 * Constructor for AS
 * @param asn The Autonomous System Number (unique identifier)
 */
AS::AS(uint32_t asn) 
    : asn_(asn), 
      propagation_rank_(-1),
      policy_(nullptr) {
}

/**
 * Destructor
 */
AS::~AS() {
    // Smart pointers handle policy cleanup automatically
    // Raw pointers in vectors are managed by ASGraph, not individual AS objects
}

/**
 * Set the routing policy for this AS
 * @param policy Unique pointer to a Policy object (BGP or ROV)
 */
void AS::setPolicy(std::unique_ptr<Policy> policy) {
    policy_ = std::move(policy);
}

/**
 * Add a provider AS
 * @param provider Pointer to the provider AS
 */
void AS::addProvider(AS* provider) {
    if (provider && std::find(providers_.begin(), providers_.end(), provider) == providers_.end()) {
        providers_.push_back(provider);
    }
}

/**
 * Add a customer AS
 * @param customer Pointer to the customer AS
 */
void AS::addCustomer(AS* customer) {
    if (customer && std::find(customers_.begin(), customers_.end(), customer) == customers_.end()) {
        customers_.push_back(customer);
    }
}

/**
 * Add a peer AS
 * @param peer Pointer to the peer AS
 */
void AS::addPeer(AS* peer) {
    if (peer && std::find(peers_.begin(), peers_.end(), peer) == peers_.end()) {
        peers_.push_back(peer);
    }
}

/**
 * Check if the given AS is a provider of this AS
 * @param other The AS to check
 * @return true if other is a provider, false otherwise
 */
bool AS::isProvider(const AS* other) const {
    return std::find(providers_.begin(), providers_.end(), other) != providers_.end();
}

/**
 * Check if the given AS is a customer of this AS
 * @param other The AS to check
 * @return true if other is a customer, false otherwise
 */
bool AS::isCustomer(const AS* other) const {
    return std::find(customers_.begin(), customers_.end(), other) != customers_.end();
}

/**
 * Check if the given AS is a peer of this AS
 * @param other The AS to check
 * @return true if other is a peer, false otherwise
 */
bool AS::isPeer(const AS* other) const {
    return std::find(peers_.begin(), peers_.end(), other) != peers_.end();
}

/**
 * Send all announcements in local RIB to providers
 * Providers receive announcements with CUSTOMER relationship
 */
void AS::sendAnnouncementsToProviders() {
    if (!policy_) return;
    
    const auto& local_rib = policy_->getLocalRib();
    
    for (const auto& pair : local_rib) {
        const Announcement& ann = pair.second;
        
        for (AS* provider : providers_) {
            sendAnnouncementToNeighbor(provider, ann, Relationship::CUSTOMER);
        }
    }
}

/**
 * Send all announcements in local RIB to customers
 * Customers receive announcements with PROVIDER relationship
 */
void AS::sendAnnouncementsToCustomers() {
    if (!policy_) return;
    
    const auto& local_rib = policy_->getLocalRib();
    
    for (const auto& pair : local_rib) {
        const Announcement& ann = pair.second;
        
        for (AS* customer : customers_) {
            sendAnnouncementToNeighbor(customer, ann, Relationship::PROVIDER);
        }
    }
}

/**
 * Send announcements to peers
 * Only send announcements received from customers (export policy)
 * Peers receive announcements with PEER relationship
 */
void AS::sendAnnouncementsToPeers() {
    if (!policy_) return;
    
    const auto& local_rib = policy_->getLocalRib();
    
    for (const auto& pair : local_rib) {
        const Announcement& ann = pair.second;
        
        // Only send to peers if we learned it from a customer or it's our origin
        if (ann.getRecvRelationship() == Relationship::CUSTOMER ||
            ann.getRecvRelationship() == Relationship::ORIGIN) {
            
            for (AS* peer : peers_) {
                sendAnnouncementToNeighbor(peer, ann, Relationship::PEER);
            }
        }
    }
}

/**
 * Helper: Send a single announcement to a neighbor
 * Creates a modified copy with new next hop and relationship
 */
void AS::sendAnnouncementToNeighbor(AS* neighbor, const Announcement& ann, Relationship recv_rel) {
    if (!neighbor || !neighbor->getPolicy()) return;

    Announcement new_ann = ann;

    new_ann.setNextHopASN(asn_);
    new_ann.setRecvRelationship(recv_rel);

    neighbor->getPolicy()->receiveAnnouncement(new_ann);
}

/**
 * Process all announcements in the received queue
 * Updates local RIB based on BGP selection
 */
void AS::processReceivedAnnouncements() {
    if (!policy_) return;
    policy_->processAnnouncements(this);
}

/**
 * Clear the received queue after processing
 */
void AS::clearReceivedQueue() {
    if (!policy_) return;
    policy_->clearReceivedQueue();
}