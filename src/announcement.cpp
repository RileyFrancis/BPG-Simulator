#include "announcement.hpp"
#include <sstream>

/**
 * Constructor - creates an announcement with an empty AS path
 */
Announcement::Announcement(const std::string& prefix,
                           uint32_t next_hop_asn,
                           Relationship recv_relationship,
                           bool rov_invalid)
    : prefix_(prefix),
      as_path_(),
      next_hop_asn_(next_hop_asn),
      recv_relationship_(recv_relationship),
      rov_invalid_(rov_invalid) {
}

/**
 * Constructor - creates an announcement with a given AS path
 */
Announcement::Announcement(const std::string& prefix,
                           const std::vector<uint32_t>& as_path,
                           uint32_t next_hop_asn,
                           Relationship recv_relationship,
                           bool rov_invalid)
    : prefix_(prefix),
      as_path_(as_path),
      next_hop_asn_(next_hop_asn),
      recv_relationship_(recv_relationship),
      rov_invalid_(rov_invalid) {
}

/**
 * Prepend an ASN to the AS path (used when announcement propagates)
 */
void Announcement::prependASPath(uint32_t asn) {
    as_path_.insert(as_path_.begin(), asn);
}

/**
 * Convert AS path to string format for output
 * Format: "(1,)" for single element, "(1, 2, 3)" for multiple elements
 */
std::string Announcement::asPathToString() const {
    if (as_path_.empty()) {
        return "\"()\"";
    }
    
    std::ostringstream oss;
    oss << "\"(";
    for (size_t i = 0; i < as_path_.size(); ++i) {
        if (i > 0) {
            oss << ", ";
        }
        oss << as_path_[i];
    }
    
    // Add trailing comma for single-element paths
    if (as_path_.size() == 1) {
        oss << ",";
    }
    
    oss << ")\"";
    return oss.str();
}

/**
 * BGP route selection algorithm
 * Returns true if this announcement is better than the other
 * 
 * Selection criteria (in order):
 * 1. Best relationship (Origin > Customer > Peer > Provider)
 * 2. Shortest AS path
 * 3. Lowest next hop ASN (tiebreaker)
 */
bool Announcement::isBetterThan(const Announcement& other) const {
    // 1. Compare relationships
    if (recv_relationship_ != other.recv_relationship_) {
        return recv_relationship_ < other.recv_relationship_;  // Lower enum value = better
    }
    
    // 2. Compare AS path lengths (shorter is better)
    if (as_path_.size() != other.as_path_.size()) {
        return as_path_.size() < other.as_path_.size();
    }
    
    // 3. Tiebreaker: lowest next hop ASN
    return next_hop_asn_ < other.next_hop_asn_;
}