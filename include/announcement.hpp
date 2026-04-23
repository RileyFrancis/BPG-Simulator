#pragma once

#include <string>
#include <vector>
#include <cstdint>

enum class Relation : uint8_t {
    Unknown  = 0,
    Provider = 1,
    Peer     = 2,
    Customer = 3,
    Origin   = 4
};

inline Relation relationFromString(const std::string& s) {
    if (s == "origin")   return Relation::Origin;
    if (s == "customer") return Relation::Customer;
    if (s == "peer")     return Relation::Peer;
    if (s == "provider") return Relation::Provider;
    return Relation::Unknown;
}

inline const char* relationToString(Relation r) {
    switch (r) {
        case Relation::Origin:   return "origin";
        case Relation::Customer: return "customer";
        case Relation::Peer:     return "peer";
        case Relation::Provider: return "provider";
        default:                 return "unknown";
    }
}

class Announcement {
public:
    // Primary constructor (enum-based, used in hot path)
    Announcement(const std::string& prefix,
                 const std::vector<uint32_t>& as_path,
                 uint32_t next_hop_asn,
                 Relation received_from,
                 bool rov_invalid = false)
        : prefix_(prefix),
          as_path_(as_path),
          next_hop_asn_(next_hop_asn),
          received_from_(received_from),
          rov_invalid_(rov_invalid) {}

    // String-based constructor for backward compatibility
    Announcement(const std::string& prefix,
                 const std::vector<uint32_t>& as_path,
                 uint32_t next_hop_asn,
                 const std::string& received_from,
                 bool rov_invalid = false)
        : prefix_(prefix),
          as_path_(as_path),
          next_hop_asn_(next_hop_asn),
          received_from_(relationFromString(received_from)),
          rov_invalid_(rov_invalid) {}

    // Default constructor
    Announcement() : next_hop_asn_(0), received_from_(Relation::Unknown), rov_invalid_(false) {}

    // Getters
    const std::string& getPrefix() const { return prefix_; }
    const std::vector<uint32_t>& getASPath() const { return as_path_; }
    uint32_t getNextHopASN() const { return next_hop_asn_; }
    Relation getRelation() const { return received_from_; }
    std::string getReceivedFrom() const { return relationToString(received_from_); }
    bool isROVInvalid() const { return rov_invalid_; }

    // Setters
    void setPrefix(const std::string& prefix) { prefix_ = prefix; }
    void setASPath(const std::vector<uint32_t>& as_path) { as_path_ = as_path; }
    void setNextHopASN(uint32_t asn) { next_hop_asn_ = asn; }
    void setReceivedFrom(Relation rel) { received_from_ = rel; }
    void setReceivedFrom(const std::string& rel) { received_from_ = relationFromString(rel); }
    void setROVInvalid(bool invalid) { rov_invalid_ = invalid; }

    void prependASPath(uint32_t asn) {
        as_path_.insert(as_path_.begin(), asn);
    }

    size_t getASPathLength() const {
        return as_path_.size();
    }

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
    std::string prefix_;
    std::vector<uint32_t> as_path_;
    uint32_t next_hop_asn_;
    Relation received_from_;   // 1-byte enum, not a heap-allocated string
    bool rov_invalid_;
};
