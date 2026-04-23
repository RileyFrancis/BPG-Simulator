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

        rank_structure_.clear();

        // Seed BFS from all leaf ASes (no customers) at rank 0
        std::unordered_set<AS*> current_rank_set;
        for (auto& pair : ases_) {
            AS* as = pair.second.get();
            if (!as->hasCustomers()) {
                as->setPropagationRank(0);
                current_rank_set.insert(as);
            }
        }

        // BFS upward: an AS's rank is the maximum rank of any of its customers + 1.
        // We keep re-raising ranks until no more changes occur.
        int current_rank = 0;
        while (!current_rank_set.empty()) {
            std::unordered_set<AS*> next_rank_set;
            int new_rank = current_rank + 1;
            for (AS* as : current_rank_set) {
                for (AS* provider : as->getProviders()) {
                    if (provider->getPropagationRank() < new_rank) {
                        provider->setPropagationRank(new_rank);
                        next_rank_set.insert(provider);
                    }
                }
            }
            if (next_rank_set.empty()) break;
            current_rank++;
            current_rank_set = std::move(next_rank_set);
        }

        // Single pass to build rank_structure_ from final rank assignments.
        // Previously this was done twice (once during BFS, then discarded and rebuilt).
        int actual_max_rank = -1;
        for (const auto& pair : ases_) {
            actual_max_rank = std::max(actual_max_rank, pair.second->getPropagationRank());
        }
        max_rank_ = actual_max_rank;
        rank_structure_.resize(max_rank_ + 1);
        for (auto& pair : ases_) {
            AS* as = pair.second.get();
            int rank = as->getPropagationRank();
            if (rank >= 0) rank_structure_[rank].push_back(as);
        }

        std::cout << "Graph flattened! Max rank: " << max_rank_ << std::endl;
        std::cout << "Total ranks: " << rank_structure_.size() << std::endl;

        int unranked = 0;
        for (const auto& pair : ases_) {
            if (pair.second->getPropagationRank() == -1) unranked++;
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

        for (size_t rank = 0; rank < rank_structure_.size(); ++rank) {
            for (AS* as : rank_structure_[rank]) {
                BGP* policy = as->getBGPPolicy();
                if (!policy) continue;

                for (const auto& [prefix, ann] : policy->getLocalRIB()) {
                    Announcement sent_ann = ann;
                    sent_ann.setNextHopASN(as->getASN());
                    sent_ann.setReceivedFrom(Relation::Customer);

                    for (AS* provider : as->getProviders()) {
                        BGP* provider_policy = provider->getBGPPolicy();
                        if (provider_policy) provider_policy->receiveAnnouncement(sent_ann);
                    }
                }
            }

            if (rank + 1 < rank_structure_.size()) {
                for (AS* as : rank_structure_[rank + 1]) {
                    BGP* policy = as->getBGPPolicy();
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

        // All ASes send simultaneously (prevents multi-hop peer routes)
        for (auto& pair : ases_) {
            AS* as = pair.second.get();
            BGP* policy = as->getBGPPolicy();
            if (!policy) continue;

            for (const auto& [prefix, ann] : policy->getLocalRIB()) {
                Announcement sent_ann = ann;
                sent_ann.setNextHopASN(as->getASN());
                sent_ann.setReceivedFrom(Relation::Peer);

                for (AS* peer : as->getPeers()) {
                    BGP* peer_policy = peer->getBGPPolicy();
                    if (peer_policy) peer_policy->receiveAnnouncement(sent_ann);
                }
            }
        }

        // All ASes process simultaneously
        for (auto& pair : ases_) {
            AS* as = pair.second.get();
            BGP* policy = as->getBGPPolicy();
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

        for (int rank = max_rank_; rank >= 0; --rank) {
            for (AS* as : rank_structure_[rank]) {
                BGP* policy = as->getBGPPolicy();
                if (!policy) continue;

                for (const auto& [prefix, ann] : policy->getLocalRIB()) {
                    Announcement sent_ann = ann;
                    sent_ann.setNextHopASN(as->getASN());
                    sent_ann.setReceivedFrom(Relation::Provider);

                    for (AS* customer : as->getCustomers()) {
                        BGP* customer_policy = customer->getBGPPolicy();
                        if (customer_policy) customer_policy->receiveAnnouncement(sent_ann);
                    }
                }
            }

            if (rank > 0) {
                for (AS* as : rank_structure_[rank - 1]) {
                    BGP* policy = as->getBGPPolicy();
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
        outfile << "asn,prefix,as_path\n";
        
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
                        << path_stream.str() << '\n';
                
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
        if (line.empty() || line[0] == '#') return true;

        // Manual char* parsing avoids constructing an istringstream per line,
        // which matters when processing 100k+ line topology files.
        const char* p = line.c_str();
        char* end;

        uint32_t asn1 = static_cast<uint32_t>(strtoul(p, &end, 10));
        if (*end != '|') return false;
        p = end + 1;

        uint32_t asn2 = static_cast<uint32_t>(strtoul(p, &end, 10));
        if (*end != '|') return false;
        p = end + 1;

        int relationship = static_cast<int>(strtol(p, &end, 10));
        if (*end != '\0' && *end != '|' && *end != '\r' && *end != '\n') return false;

        AS* as1 = getOrCreateAS(asn1);
        AS* as2 = getOrCreateAS(asn2);

        if (relationship == -1) {
            as1->addCustomer(as2);
            as2->addProvider(as1);
        } else if (relationship == 0) {
            as1->addPeer(as2);
            as2->addPeer(as1);
        } else {
            std::cerr << "Warning: Unknown relationship type " << relationship
                      << " for " << asn1 << "|" << asn2 << std::endl;
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