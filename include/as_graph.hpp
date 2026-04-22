#pragma once

#include <iostream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <memory>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <queue>
#include "as.hpp"   

/**
 * ASGraph - Manages the entire AS topology
 * Builds the graph from CAIDA data, detects cycles, and handles propagation
 */
class ASGraph {
public:
    ASGraph() = default;
    ~ASGraph() = default;
    
    // Build the graph from CAIDA topology file
    bool buildFromCAIDAFile(const std::string& filename) {
        std::ifstream file(filename);
    
        if (!file.is_open()) {
            std::cerr << "Error: Could not open file " << filename << std::endl;
            return false;
        }
        
        std::string line;
        size_t line_num = 0;
        size_t errors = 0;
        
        std::cout << "Reading CAIDA topology file: " << filename << std::endl;
        
        while (std::getline(file, line)) {
            line_num++;
            
            if (!parseCAIDALine(line)) {
                errors++;
                if (errors > 10) {
                    std::cerr << "Too many errors, aborting." << std::endl;
                    return false;
                }
            }
            
            // Progress indicator for large files
            if (line_num % 100000 == 0) {
                std::cout << "Processed " << line_num << " lines, "
                        << ases_.size() << " ASes so far..." << std::endl;
            }
        }
        
        file.close();
        
        std::cout << "Graph building complete!" << std::endl;
        std::cout << "Total ASes: " << ases_.size() << std::endl;
        std::cout << "Total lines processed: " << line_num << std::endl;
        
        // Check for cycles
        if (hasCycles()) {
            std::cerr << "ERROR: Graph contains provider-customer cycles!" << std::endl;
            return false;
        }
        
        std::cout << "No cycles detected - graph is valid!" << std::endl;
        
        return true;
    }
    
    // Get an AS by its ASN (returns nullptr if not found)
    AS* getAS(uint32_t asn) const {
        auto it = ases_.find(asn);
        if (it != ases_.end()) {
            return it->second.get();
        }
        return nullptr;
    }
    
    // Get all ASes
    const std::unordered_map<uint32_t, std::unique_ptr<AS>>& getAllASes() const {
        return ases_;
    }
    
    // Check for cycles in provider-customer relationships
    bool hasCycles() const {
        // visited: 0 = unvisited, 1 = visiting, 2 = visited
        std::unordered_map<uint32_t, int> visited;
        std::unordered_map<uint32_t, int> recursion_stack;
        
        // Initialize all as unvisited
        for (const auto& pair : ases_) {
            visited[pair.first] = 0;
            recursion_stack[pair.first] = 0;
        }
        
        // Check each AS
        for (const auto& pair : ases_) {
            uint32_t asn = pair.first;
            
            if (visited[asn] == 0) {  // Unvisited
                if (hasCyclesDFS(asn, visited, recursion_stack)) {
                    return true;  // Cycle found
                }
            }
        }
        
        return false;
    }
    
    // Graph statistics
    size_t getNumASes() const { 
        return ases_.size(); 
    }

    size_t getNumProviderCustomerEdges() const {
        size_t count = 0;
        for (const auto& pair : ases_) {
            count += pair.second->getCustomers().size();
        }
        return count;
    }

    size_t getNumPeerEdges() const {
        size_t count = 0;
        for (const auto& pair : ases_) {
            count += pair.second->getPeers().size();
        }
        return count / 2;  // Each peer relationship counted twice
    }

    // Helper: Get or create an AS
    AS* getOrCreateAS(uint32_t asn) {
        auto it = ases_.find(asn);
        if (it != ases_.end()) {
            return it->second.get();
        }

        auto new_as = std::make_unique<AS>(asn);
        AS* as_ptr = new_as.get();
        ases_[asn] = std::move(new_as);

        return as_ptr;
    }

    // Build graph from an in-memory string (same format as CAIDA file)
    bool buildFromCAIDAString(const std::string& content) {
        std::istringstream stream(content);
        std::string line;
        size_t errors = 0;
        while (std::getline(stream, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (!parseCAIDALine(line)) {
                if (++errors > 10) return false;
            }
        }
        if (hasCycles()) return false;
        return true;
    }

    // Seed announcements from an in-memory CSV string
    bool seedFromCSVString(const std::string& content) {
        std::istringstream stream(content);
        std::string line;
        std::getline(stream, line);  // skip header
        size_t count = 0;
        while (std::getline(stream, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty()) continue;
            std::istringstream iss(line);
            std::string asn_str, prefix, rov_str;
            if (!std::getline(iss, asn_str, ',') ||
                !std::getline(iss, prefix, ',') ||
                !std::getline(iss, rov_str)) continue;
            if (!rov_str.empty() && rov_str.back() == '\r') rov_str.pop_back();
            try {
                uint32_t asn = std::stoul(asn_str);
                bool rov_invalid = (rov_str == "True" || rov_str == "true" || rov_str == "1");
                AS* as = getAS(asn);
                if (!as) continue;
                Policy* policy = as->getPolicy();
                if (!policy) continue;
                policy->seedAnnouncement(Announcement(prefix, {asn}, asn, "origin", rov_invalid));
                count++;
            } catch (...) {}
        }
        return count > 0;
    }

    /**
     * Assign BGP or ROV policies to every AS in the graph.
     * Must be called after buildFromCAIDAFile and before seeding/propagation.
     * ASNs in rov_asns get ROV policy; all others get BGP.
     */
    void initializePolicies(const std::unordered_set<uint32_t>& rov_asns = {}) {
        for (auto& pair : ases_) {
            uint32_t asn = pair.first;
            AS* as = pair.second.get();
            if (rov_asns.count(asn)) {
                as->setPolicy(std::make_unique<ROV>());
            } else {
                as->setPolicy(std::make_unique<BGP>());
            }
        }
    }

    /**
     * Seed announcements from a CSV file.
     * Expected columns: seed_asn,prefix,rov_invalid
     * Must be called after initializePolicies.
     */
    bool seedFromCSV(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Error: Could not open announcements file: " << filename << std::endl;
            return false;
        }

        std::string line;
        std::getline(file, line);  // skip header

        size_t count = 0;
        while (std::getline(file, line)) {
            if (line.empty()) continue;

            std::istringstream iss(line);
            std::string asn_str, prefix, rov_str;

            if (!std::getline(iss, asn_str, ',') ||
                !std::getline(iss, prefix, ',') ||
                !std::getline(iss, rov_str)) {
                std::cerr << "Warning: malformed announcement line: " << line << std::endl;
                continue;
            }

            // Strip trailing \r from Windows line endings
            if (!rov_str.empty() && rov_str.back() == '\r') rov_str.pop_back();

            try {
                uint32_t asn = std::stoul(asn_str);
                bool rov_invalid = (rov_str == "True" || rov_str == "true" || rov_str == "1");

                AS* as = getAS(asn);
                if (!as) {
                    std::cerr << "Warning: seed ASN " << asn << " not in graph, skipping" << std::endl;
                    continue;
                }
                Policy* policy = as->getPolicy();
                if (!policy) {
                    std::cerr << "Warning: AS " << asn << " has no policy, skipping" << std::endl;
                    continue;
                }
                policy->seedAnnouncement(Announcement(prefix, {asn}, asn, "origin", rov_invalid));
                count++;
            } catch (const std::exception& e) {
                std::cerr << "Warning: failed to parse line: " << line
                          << " (" << e.what() << ")" << std::endl;
            }
        }

        file.close();
        std::cout << "Seeded " << count << " announcements" << std::endl;
        return true;
    }

    /**
     * Flatten the graph by assigning propagation ranks
     * 
     * Algorithm from Section 3.3:
     * 1. Find all ASes with no customers (rank 0 - the "edge" ASes)
     * 2. For each AS at rank 0, assign all their providers rank 1
     * 3. For each AS at rank 1, assign all their providers rank 2
     * 4. Continue until all ASes have ranks (reaching ASes with no providers)
     * 
     * This creates a hierarchical structure where:
     * - Rank 0: ASes with no customers (edges)
     * - Higher ranks: Move up the provider chain
     */
    void flattenGraph() {
        std::cout << "\nFlattening graph (assigning propagation ranks)..." << std::endl;
        
        // Clear existing rank structure
        rank_structure_.clear();
        
        // Step 1: Find all ASes with no customers and assign rank 0
        std::unordered_set<AS*> current_rank_set;
        
        for (auto& pair : ases_) {
            AS* as = pair.second.get();
            if (!as->hasCustomers()) {
                as->setPropagationRank(0);
                current_rank_set.insert(as);
            }
        }
        
        // Add rank 0 ASes to structure
        std::vector<AS*> rank_0_ases(current_rank_set.begin(), current_rank_set.end());
        rank_structure_.push_back(rank_0_ases);
        
        std::cout << "  Rank 0: " << rank_0_ases.size() << " ASes (no customers)" << std::endl;
        
        // Step 2: Propagate ranks upward through provider chain
        int current_rank = 0;
        
        while (!current_rank_set.empty()) {
            std::unordered_set<AS*> next_rank_set;
            
            // For each AS at current rank, check all their providers
            for (AS* as : current_rank_set) {
                for (AS* provider : as->getProviders()) {
                    // Assign rank if:
                    // 1. Not yet ranked, OR
                    // 2. Current rank would be higher than existing rank
                    int new_rank = current_rank + 1;
                    if (provider->getPropagationRank() == -1 || 
                        provider->getPropagationRank() < new_rank) {
                        
                        provider->setPropagationRank(new_rank);
                        next_rank_set.insert(provider);
                    }
                }
            }
            
            // Move to next rank
            if (!next_rank_set.empty()) {
                current_rank++;
                std::vector<AS*> rank_ases(next_rank_set.begin(), next_rank_set.end());
                rank_structure_.push_back(rank_ases);
                
                std::cout << "  Rank " << current_rank << ": " 
                         << rank_ases.size() << " ASes" << std::endl;
                
                current_rank_set = next_rank_set;
            } else {
                break;
            }
        }
        
        max_rank_ = current_rank;
        
        // Now rebuild rank_structure_ based on final ranks
        // (since ASes may have changed ranks during propagation)
        rank_structure_.clear();
        
        // Find max rank
        int actual_max_rank = -1;
        for (const auto& pair : ases_) {
            if (pair.second->getPropagationRank() > actual_max_rank) {
                actual_max_rank = pair.second->getPropagationRank();
            }
        }
        
        max_rank_ = actual_max_rank;
        
        // Resize rank structure
        rank_structure_.resize(max_rank_ + 1);
        
        // Populate rank structure with final ranks
        for (auto& pair : ases_) {
            AS* as = pair.second.get();
            int rank = as->getPropagationRank();
            if (rank >= 0) {
                rank_structure_[rank].push_back(as);
            }
        }
        
        std::cout << "Graph flattened! Max rank: " << max_rank_ << std::endl;
        std::cout << "Total ranks: " << rank_structure_.size() << std::endl;
        
        // Verify all ASes have ranks
        int unranked = 0;
        for (const auto& pair : ases_) {
            if (pair.second->getPropagationRank() == -1) {
                unranked++;
            }
        }
        
        if (unranked > 0) {
            std::cerr << "Warning: " << unranked << " ASes have no rank!" << std::endl;
        }
    }
    
    /**
     * Get all ASes at a specific propagation rank
     */
    const std::vector<AS*>& getASesAtRank(int rank) const {
        static const std::vector<AS*> empty;
        if (rank < 0 || rank >= static_cast<int>(rank_structure_.size())) {
            return empty;
        }
        return rank_structure_[rank];
    }
    
    /**
     * Get the maximum propagation rank
     */
    int getMaxPropagationRank() const {
        return max_rank_;
    }
    
    /**
     * Get the number of ranks
     */
    size_t getNumRanks() const {
        return rank_structure_.size();
    }
    
    /**
     * Propagate announcements UP the provider chain (Section 3.5)
     * Go from rank 0 up to max rank
     */
    void propagateUp() {
        std::cout << "\n=== Propagating UP (customers -> providers) ===" << std::endl;
        
        // Start at rank 0 and go up
        for (size_t rank = 0; rank < rank_structure_.size(); ++rank) {
            // Step 1: All ASes at this rank send to their providers
            for (AS* as : rank_structure_[rank]) {
                BGP* policy = dynamic_cast<BGP*>(as->getPolicy());
                if (!policy) continue;
                
                // Get all announcements from local RIB
                auto announcements = policy->getAnnouncementsToSend();
                
                for (const auto& ann : announcements) {
                    // Create new announcement for sending
                    Announcement sent_ann = ann;
                    sent_ann.setNextHopASN(as->getASN());
                    sent_ann.setReceivedFrom("customer");  // Provider receives from customer
                    
                    // Send to all providers
                    for (AS* provider : as->getProviders()) {
                        BGP* provider_policy = dynamic_cast<BGP*>(provider->getPolicy());
                        if (provider_policy) {
                            provider_policy->receiveAnnouncement(sent_ann);
                        }
                    }
                }
            }
            
            // Step 2: Process at next rank (if not at top)
            if (rank + 1 < rank_structure_.size()) {
                for (AS* as : rank_structure_[rank + 1]) {
                    BGP* policy = dynamic_cast<BGP*>(as->getPolicy());
                    if (policy) {
                        policy->processReceivedAnnouncements(as->getASN());
                        policy->clearReceivedQueue();
                    }
                }
            }
        }
        
        std::cout << "  Propagated through " << rank_structure_.size() << " ranks" << std::endl;
    }
    
    /**
     * Propagate announcements ACROSS to peers (Section 3.5)
     * ONE HOP ONLY - valley-free routing
     */
    void propagateAcross() {
        std::cout << "\n=== Propagating ACROSS (peers -> peers, one hop only) ===" << std::endl;
        
        // Step 1: ALL ASes send to peers (all at once)
        for (auto& pair : ases_) {
            AS* as = pair.second.get();
            BGP* policy = dynamic_cast<BGP*>(as->getPolicy());
            if (!policy) continue;
            
            auto announcements = policy->getAnnouncementsToSend();
            
            for (const auto& ann : announcements) {
                Announcement sent_ann = ann;
                sent_ann.setNextHopASN(as->getASN());
                sent_ann.setReceivedFrom("peer");
                
                // Send to all peers
                for (AS* peer : as->getPeers()) {
                    BGP* peer_policy = dynamic_cast<BGP*>(peer->getPolicy());
                    if (peer_policy) {
                        peer_policy->receiveAnnouncement(sent_ann);
                    }
                }
            }
        }
        
        // Step 2: ALL ASes process (all at once - prevents multi-hop)
        for (auto& pair : ases_) {
            AS* as = pair.second.get();
            BGP* policy = dynamic_cast<BGP*>(as->getPolicy());
            if (policy) {
                policy->processReceivedAnnouncements(as->getASN());
                policy->clearReceivedQueue();
            }
        }
        
        std::cout << "  Propagated across peers" << std::endl;
    }
    
    /**
     * Propagate announcements DOWN to customers (Section 3.5)
     * Go from max rank down to rank 0
     */
    void propagateDown() {
        std::cout << "\n=== Propagating DOWN (providers -> customers) ===" << std::endl;
        
        // Start at max rank and go down to 0
        for (int rank = max_rank_; rank >= 0; --rank) {
            // Step 1: All ASes at this rank send to their customers
            for (AS* as : rank_structure_[rank]) {
                BGP* policy = dynamic_cast<BGP*>(as->getPolicy());
                if (!policy) continue;
                
                auto announcements = policy->getAnnouncementsToSend();
                
                for (const auto& ann : announcements) {
                    Announcement sent_ann = ann;
                    sent_ann.setNextHopASN(as->getASN());
                    sent_ann.setReceivedFrom("provider");  // Customer receives from provider
                    
                    // Send to all customers
                    for (AS* customer : as->getCustomers()) {
                        BGP* customer_policy = dynamic_cast<BGP*>(customer->getPolicy());
                        if (customer_policy) {
                            customer_policy->receiveAnnouncement(sent_ann);
                        }
                    }
                }
            }
            
            // Step 2: Process at lower rank (if not at bottom)
            if (rank > 0) {
                for (AS* as : rank_structure_[rank - 1]) {
                    BGP* policy = dynamic_cast<BGP*>(as->getPolicy());
                    if (policy) {
                        policy->processReceivedAnnouncements(as->getASN());
                        policy->clearReceivedQueue();
                    }
                }
            }
        }
        
        std::cout << "  Propagated through " << rank_structure_.size() << " ranks" << std::endl;
    }
    
    /**
     * Full propagation: UP -> ACROSS -> DOWN (Sections 3.5 & 3.6)
     * This is the complete BGP announcement propagation
     */
    void propagateAnnouncements() {
        std::cout << "\n========================================" << std::endl;
        std::cout << "Starting BGP Announcement Propagation" << std::endl;
        std::cout << "========================================" << std::endl;
        
        propagateUp();
        propagateAcross();
        propagateDown();
        
        std::cout << "\n========================================" << std::endl;
        std::cout << "Propagation Complete!" << std::endl;
        std::cout << "========================================" << std::endl;
    }
    
    /**
     * Output the routing tables to a CSV file (Section 3.7)
     * Format: asn,prefix,as_path
     * 
     * This dumps the local RIB of every AS to a CSV file that Cloudflare
     * can use to optimize their routing decisions.
     * 
     * @param filename Output CSV filename
     * @return true if successful, false otherwise
     */
    bool outputToCSV(const std::string& filename) {
        std::ofstream outfile(filename);
        
        if (!outfile.is_open()) {
            std::cerr << "Error: Could not open output file " << filename << std::endl;
            return false;
        }
        
        // Write CSV header
        outfile << "asn,prefix,as_path" << std::endl;
        
        // Iterate through all ASes
        size_t total_announcements = 0;
        for (const auto& pair : ases_) {
            AS* as = pair.second.get();
            uint32_t asn = as->getASN();
            
            // Get the policy (should be BGP)
            BGP* policy = dynamic_cast<BGP*>(as->getPolicy());
            if (!policy) {
                continue;  // Skip ASes without BGP policy
            }
            
            // Get the local RIB
            const auto& local_rib = policy->getLocalRIB();
            
            // Write each announcement in the RIB
            for (const auto& rib_entry : local_rib) {
                const std::string& prefix = rib_entry.first;
                const Announcement& ann = rib_entry.second;
                
                // Format: asn,prefix,as_path
                const std::vector<uint32_t>& path = ann.getASPath();

                // Build Python tuple format: "(a, b, c)" or "(a,)" for single element
                std::ostringstream path_stream;
                path_stream << "\"(";

                for (size_t i = 0; i < path.size(); ++i) {
                    path_stream << path[i];
                    if (i != path.size() - 1) {
                        path_stream << ", ";
                    }
                }

                if (path.size() == 1) {
                    path_stream << ",";
                }

                path_stream << ")\"";

                outfile << asn << ","
                        << prefix << ","
                        << path_stream.str() << std::endl;
                
                total_announcements++;
            }
        }
        
        outfile.close();
        
        std::cout << "\nOutput written to " << filename << std::endl;
        std::cout << "Total announcements: " << total_announcements << std::endl;
        
        return true;
    }
    
private:
    // Storage for all AS objects (ASN -> AS object)
    std::unordered_map<uint32_t, std::unique_ptr<AS>> ases_;
    
    // Flattened graph structure: rank -> vector of ASes at that rank
    std::vector<std::vector<AS*>> rank_structure_;
    
    // Maximum rank in the graph
    int max_rank_ = -1;
    
    // Helper: Parse a single line from CAIDA file
    bool parseCAIDALine(const std::string& line) {
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') {
            return true;
        }
        
        std::istringstream iss(line);
        std::string asn1_str, asn2_str, rel_str;
        
        // Parse the three fields separated by |
        if (!std::getline(iss, asn1_str, '|')) return false;
        if (!std::getline(iss, asn2_str, '|')) return false;
        if (!std::getline(iss, rel_str, '|')) return false;
        
        // Ignore the 4th column (source) if it exists
        
        try {
            uint32_t asn1 = std::stoul(asn1_str);
            uint32_t asn2 = std::stoul(asn2_str);
            int relationship = std::stoi(rel_str);
            
            // Get or create both ASes
            AS* as1 = getOrCreateAS(asn1);
            AS* as2 = getOrCreateAS(asn2);
            
            // CAIDA format: https://publicdata.caida.org/datasets/as-relationships/
            // <provider-as>|<customer-as>|-1
            // <peer-as>|<peer-as>|0|<source>

            if (relationship == -1) {
                // Provider-customer relationship
                // asn1 is the PROVIDER, asn2 is the CUSTOMER
                as1->addCustomer(as2);
                as2->addProvider(as1);
                
            } else if (relationship == 0) {
                // Peer-to-peer relationship (bidirectional)
                as1->addPeer(as2);
                as2->addPeer(as1);
                
            } else {
                std::cerr << "Warning: Unknown relationship type " << relationship 
                        << " for " << asn1 << "|" << asn2 << std::endl;
                return false;
            }
            
        } catch (const std::exception& e) {
            std::cerr << "Error parsing line: " << line << std::endl;
            std::cerr << "Exception: " << e.what() << std::endl;
            return false;
        }
        
        return true;
    }
    
    // Helper: DFS for cycle detection
    bool hasCyclesDFS(uint32_t asn, 
                      std::unordered_map<uint32_t, int>& visited,
                      std::unordered_map<uint32_t, int>& recursion_stack) const {
        AS* current_as = getAS(asn);
        if (!current_as) return false;
        
        // Mark as visiting
        visited[asn] = 1;
        recursion_stack[asn] = 1;
        
        // Check all providers (going UP the provider chain)
        for (AS* provider : current_as->getProviders()) {
            uint32_t provider_asn = provider->getASN();
            
            if (visited[provider_asn] == 0) {
                // Unvisited - recurse
                if (hasCyclesDFS(provider_asn, visited, recursion_stack)) {
                    return true;
                }
            } else if (recursion_stack[provider_asn] == 1) {
                // Back edge found - cycle detected!
                std::cerr << "Cycle detected involving AS " << asn 
                        << " -> " << provider_asn << std::endl;
                return true;
            }
        }
        
        // Mark as visited (done)
        visited[asn] = 2;
        recursion_stack[asn] = 0;
        
        return false;
    }
};