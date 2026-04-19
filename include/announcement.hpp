#pragma once

#include <cstdint>
#include <vector>
#include <string>

/**
 * Enum representing where an announcement was received from
 * Used for BGP route selection (origin beats customer beats peer beats provider)
 */
enum class Relationship {
    ORIGIN,      // Announcement originated from this AS (best)
    CUSTOMER,    // Received from a customer
    PEER,        // Received from a peer
    PROVIDER     // Received from a provider (worst)
};

/**
 * Announcement class - represents a BGP announcement
 * Contains prefix, AS-Path, next hop, relationship, and ROV validity
 */
class Announcement {
public:
    // Constructors
    Announcement(const std::string& prefix, 
                 uint32_t next_hop_asn,
                 Relationship recv_relationship,
                 bool rov_invalid = false);
    
    Announcement(const std::string& prefix,
                 const std::vector<uint32_t>& as_path,
                 uint32_t next_hop_asn,
                 Relationship recv_relationship,
                 bool rov_invalid = false);
    
    // Copy constructor
    Announcement(const Announcement& other) = default;
    
    // Getters
    const std::string& getPrefix() const { return prefix_; }
    const std::vector<uint32_t>& getASPath() const { return as_path_; }
    uint32_t getNextHopASN() const { return next_hop_asn_; }
    Relationship getRecvRelationship() const { return recv_relationship_; }
    bool isROVInvalid() const { return rov_invalid_; }
    
    // Setters (for creating modified copies during propagation)
    void prependASPath(uint32_t asn);
    void setNextHopASN(uint32_t asn) { next_hop_asn_ = asn; }
    void setRecvRelationship(Relationship rel) { recv_relationship_ = rel; }
    
    // Utility
    std::string asPathToString() const;
    
    // BGP route selection comparison
    // Returns true if this announcement is better than other
    bool isBetterThan(const Announcement& other) const;
    
private:
    std::string prefix_;                    // e.g., "1.2.0.0/16"
    std::vector<uint32_t> as_path_;         // Path the announcement has taken
    uint32_t next_hop_asn_;                 // Where it came from
    Relationship recv_relationship_;         // Customer/Peer/Provider/Origin
    bool rov_invalid_;                      // ROV validation status
};