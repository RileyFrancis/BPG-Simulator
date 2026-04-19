#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <memory>
#include <cstdint>
#include "as.hpp"
#include "announcement.hpp"

/**
 * ASGraph - Manages the entire AS topology
 * Builds the graph from CAIDA data, detects cycles, and handles propagation
 */
class ASGraph {
public:
    ASGraph();
    ~ASGraph();
    
    // Build the graph from CAIDA topology file
    bool buildFromCAIDAFile(const std::string& filename);
    
    // Get an AS by its ASN (returns nullptr if not found)
    AS* getAS(uint32_t asn) const;
    
    // Get all ASes
    const std::unordered_map<uint32_t, std::unique_ptr<AS>>& getAllASes() const {
        return ases_;
    }
    
    // Check for cycles in provider-customer relationships
    bool hasCycles() const;
    
    // Graph statistics (useful for debugging)
    size_t getNumASes() const { return ases_.size(); }
    size_t getNumProviderCustomerEdges() const;
    size_t getNumPeerEdges() const;

    // Helper: Get or create an AS
    AS* getOrCreateAS(uint32_t asn);

    // Flatten the graph into propagation ranks
    void flattenGraph();

    // Get the flattened graph (vector of vectors, indexed by rank)
    const std::vector<std::vector<AS*>>& getPropagationRanks() const {
        return propagation_ranks_;
    }
    
    // Seed an announcement at a specific AS
    void seedAnnouncement(uint32_t asn, const Announcement& ann);
    
    // Propagate all announcements through the network
    void propagateAnnouncements();
    
    // Set policy for an AS (BGP or ROV)
    void setASPolicy(uint32_t asn, std::unique_ptr<Policy> policy);
    
    // Set policy for all ASes (default to BGP)
    void setAllASPolicies(std::unique_ptr<Policy> default_policy);
    
    // Load ROV ASes from file
    void loadROVASes(const std::string& filename);
    
    // Load announcements from CSV
    void loadAnnouncements(const std::string& filename);
    
    // Export local RIBs to CSV
    void exportToCSV(const std::string& filename) const;
    
private:
    // Storage for all AS objects (ASN -> AS object)
    std::unordered_map<uint32_t, std::unique_ptr<AS>> ases_;
    
    // Flattened graph for propagation
    std::vector<std::vector<AS*>> propagation_ranks_;
    
    // Helper: Parse a single line from CAIDA file
    bool parseCAIDALine(const std::string& line);
    
    // Helper: DFS for cycle detection
    bool hasCyclesDFS(uint32_t asn, 
                      std::unordered_map<uint32_t, int>& visited,
                      std::unordered_map<uint32_t, int>& recursion_stack) const;
    
    // Propagation helpers
    void propagateUp();      // Send to providers (rank 0 -> top)
    void propagateAcross();  // Send to peers (one hop)
    void propagateDown();    // Send to customers (top -> rank 0)
};