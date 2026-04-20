#pragma once

#include <string>
#include <vector>
#include <cstdint>

/**
 * Represents a BGP announcement
 * 
 * From the project description:
 * - A prefix (such as 8.8.8.0/24)
 * - An AS-Path (at the start, just the originating ASN)
 * - The next hop ASN (where the announcement just came from)
 * - The received from relationship (customer, peer, provider, or origin)
 */
class Announcement {
public:
    // Constructor
    Announcement(const std::string& prefix,
                 const std::vector<uint32_t>& as_path,
                 uint32_t next_hop_asn,
                 const std::string& received_from,
                 bool rov_invalid = false)
        : prefix_(prefix),
          as_path_(as_path),
          next_hop_asn_(next_hop_asn),
          received_from_(received_from),
          rov_invalid_(rov_invalid) {}
    
    // Default constructor
    Announcement() 
        : next_hop_asn_(0),
          rov_invalid_(false) {}
    
    // Getters
    const std::string& getPrefix() const { return prefix_; }
    const std::vector<uint32_t>& getASPath() const { return as_path_; }
    uint32_t getNextHopASN() const { return next_hop_asn_; }
    const std::string& getReceivedFrom() const { return received_from_; }
    bool isROVInvalid() const { return rov_invalid_; }
    
    // Setters
    void setPrefix(const std::string& prefix) { prefix_ = prefix; }
    void setASPath(const std::vector<uint32_t>& as_path) { as_path_ = as_path; }
    void setNextHopASN(uint32_t asn) { next_hop_asn_ = asn; }
    void setReceivedFrom(const std::string& rel) { received_from_ = rel; }
    void setROVInvalid(bool invalid) { rov_invalid_ = invalid; }
    
    // Utility: Prepend an ASN to the AS path
    void prependASPath(uint32_t asn) {
        as_path_.insert(as_path_.begin(), asn);
    }
    
    // Utility: Get AS path length
    size_t getASPathLength() const {
        return as_path_.size();
    }
    
    // Utility: Get AS path as string (e.g., "3-777")
    std::string getASPathString() const {
        if (as_path_.empty()) {
            return "";
        }
        
        std::string result = std::to_string(as_path_[0]);
        for (size_t i = 1; i < as_path_.size(); ++i) {
            result += "-" + std::to_string(as_path_[i]);
        }
        return result;
    }
    
private:
    std::string prefix_;                    // IP prefix (e.g., "8.8.8.0/24" or "2001:db8::/32")
    std::vector<uint32_t> as_path_;         // AS path (prepended as announcement travels)
    uint32_t next_hop_asn_;                 // Immediate neighbor who sent this
    std::string received_from_;             // "origin", "customer", "peer", or "provider"
    bool rov_invalid_;                      // True if announcement is ROV invalid
};