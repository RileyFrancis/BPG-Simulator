#include "as_graph.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>

ASGraph::ASGraph() {
}

ASGraph::~ASGraph() {
    // unique_ptr handles cleanup automatically
}

/**
 * Get or create an AS by ASN
 * If the AS doesn't exist, create it and add to the map
 */
AS* ASGraph::getOrCreateAS(uint32_t asn) {
    auto it = ases_.find(asn);
    if (it != ases_.end()) {
        return it->second.get();
    }
    
    // Create new AS
    auto new_as = std::make_unique<AS>(asn);
    AS* as_ptr = new_as.get();
    ases_[asn] = std::move(new_as);
    
    return as_ptr;
}

/**
 * Get an existing AS (returns nullptr if not found)
 */
AS* ASGraph::getAS(uint32_t asn) const {
    auto it = ases_.find(asn);
    if (it != ases_.end()) {
        return it->second.get();
    }
    return nullptr;
}

/**
 * Parse a single line from the CAIDA file
 * Format: ASN1|ASN2|relationship
 * relationship: -1 = provider-customer, 0 = peer-peer
 */
bool ASGraph::parseCAIDALine(const std::string& line) {
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
    
    // There might be a 4th column (source) - ignore it as per instructions
    
    try {
        uint32_t asn1 = std::stoul(asn1_str);
        uint32_t asn2 = std::stoul(asn2_str);
        int relationship = std::stoi(rel_str);
        
        // Get or create both ASes
        AS* as1 = getOrCreateAS(asn1);
        AS* as2 = getOrCreateAS(asn2);
        
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

/**
 * Build the AS graph from a CAIDA topology file
 */
bool ASGraph::buildFromCAIDAFile(const std::string& filename) {
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

/**
 * Check if the graph has any cycles in provider-customer relationships
 * Uses DFS with a recursion stack to detect back edges
 */
bool ASGraph::hasCycles() const {
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
    
    return false;  // No cycles
}

/**
 * DFS helper for cycle detection
 * Only follows provider-customer edges (going UP the graph towards providers)
 */
bool ASGraph::hasCyclesDFS(uint32_t asn,
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

/**
 * Count provider-customer edges
 */
size_t ASGraph::getNumProviderCustomerEdges() const {
    size_t count = 0;
    for (const auto& pair : ases_) {
        count += pair.second->getCustomers().size();
    }
    return count;
}

/**
 * Count peer edges (divide by 2 since bidirectional)
 */
size_t ASGraph::getNumPeerEdges() const {
    size_t count = 0;
    for (const auto& pair : ases_) {
        count += pair.second->getPeers().size();
    }
    return count / 2;  // Each peer relationship counted twice
}

/**
 * Flatten the graph into propagation ranks
 * Rank 0 = ASes with no customers (edges)
 * Rank N = providers of rank N-1 ASes
 */
void ASGraph::flattenGraph() {
    std::cout << "Flattening graph into propagation ranks..." << std::endl;
    
    // Clear any existing ranks
    propagation_ranks_.clear();
    
    // Reset all propagation ranks to -1 (unassigned)
    for (auto& pair : ases_) {
        pair.second->setPropagationRank(-1);
    }
    
    // Find all ASes with no customers (rank 0)
    std::vector<AS*> rank_0;
    for (auto& pair : ases_) {
        if (!pair.second->hasCustomers()) {
            pair.second->setPropagationRank(0);
            rank_0.push_back(pair.second.get());
        }
    }
    
    propagation_ranks_.push_back(rank_0);
    std::cout << "Rank 0: " << rank_0.size() << " ASes" << std::endl;
    
    // Iteratively assign ranks going up the provider chain
    int current_rank = 0;
    while (true) {
        std::vector<AS*> next_rank;
        
        // For each AS at current rank, look at their providers
        for (AS* as : propagation_ranks_[current_rank]) {
            for (AS* provider : as->getProviders()) {
                // If provider hasn't been assigned a rank yet
                if (provider->getPropagationRank() == -1) {
                    provider->setPropagationRank(current_rank + 1);
                    next_rank.push_back(provider);
                }
            }
        }
        
        // Remove duplicates (an AS might be provider to multiple rank N ASes)
        std::sort(next_rank.begin(), next_rank.end());
        next_rank.erase(std::unique(next_rank.begin(), next_rank.end()), next_rank.end());
        
        if (next_rank.empty()) {
            break;  // No more ranks to assign
        }
        
        propagation_ranks_.push_back(next_rank);
        current_rank++;
        std::cout << "Rank " << current_rank << ": " << next_rank.size() << " ASes" << std::endl;
    }
    
    std::cout << "Graph flattening complete! Total ranks: " << propagation_ranks_.size() << std::endl;
    
    // Verify all ASes got a rank (except disconnected ones)
    int unranked = 0;
    for (const auto& pair : ases_) {
        if (pair.second->getPropagationRank() == -1) {
            unranked++;
        }
    }
    
    if (unranked > 0) {
        std::cout << "Warning: " << unranked << " ASes have no rank (disconnected from graph)" << std::endl;
    }
}

/**
 * Seed an announcement at a specific AS
 * If the AS doesn't exist, create it (origin ASes may not be in CAIDA data)
 */
void ASGraph::seedAnnouncement(uint32_t asn, const Announcement& ann) {
    // Get or create the AS (origin ASes might not be in CAIDA topology)
    AS* as = getOrCreateAS(asn);
    
    // Make sure it has a policy
    if (!as->getPolicy()) {
        as->setPolicy(std::make_unique<BGP>());
    }
    
    // Seed the announcement
    as->getPolicy()->seedAnnouncement(ann);
}

/**
 * Set policy for a specific AS
 */
void ASGraph::setASPolicy(uint32_t asn, std::unique_ptr<Policy> policy) {
    AS* as = getAS(asn);
    if (as) {
        as->setPolicy(std::move(policy));
    }
}

/**
 * Propagate announcements UP the graph (to providers)
 * Goes from rank 0 to highest rank
 */
void ASGraph::propagateUp() {
    // ✅ Process rank 0 first
    for (AS* as : propagation_ranks_[0]) {
        as->processReceivedAnnouncements();
        as->clearReceivedQueue();
    }

    for (size_t rank = 0; rank < propagation_ranks_.size(); ++rank) {
        // Send phase
        for (AS* as : propagation_ranks_[rank]) {
            as->sendAnnouncementsToProviders();
        }
        
        // Process next rank
        if (rank + 1 < propagation_ranks_.size()) {
            for (AS* as : propagation_ranks_[rank + 1]) {
                as->processReceivedAnnouncements();
                as->clearReceivedQueue();
            }
        }
    }
}

/**
 * Propagate announcements ACROSS the graph (to peers)
 * Only one hop - all ASes send, then all ASes process
 */
void ASGraph::propagateAcross() {
    // Send phase: ALL ASes send to peers simultaneously
    for (const auto& pair : ases_) {
        pair.second->sendAnnouncementsToPeers();
    }
    
    // Process phase: ALL ASes process what they received
    for (const auto& pair : ases_) {
        pair.second->processReceivedAnnouncements();
        pair.second->clearReceivedQueue();
    }
}

/**
 * Propagate announcements DOWN the graph (to customers)
 * Goes from highest rank to rank 0
 */
void ASGraph::propagateDown() {
    for (int rank = propagation_ranks_.size() - 1; rank >= 0; --rank) {
        // Send phase
        for (AS* as : propagation_ranks_[rank]) {
            as->sendAnnouncementsToCustomers();
        }

        // Process next rank down
        if (rank - 1 >= 0) {
            for (AS* as : propagation_ranks_[rank - 1]) {
                as->processReceivedAnnouncements();
                as->clearReceivedQueue();
            }
        }
    }
}

/**
 * Propagate all announcements through the entire network
 * Three phases: UP -> ACROSS -> DOWN
 */
void ASGraph::propagateAnnouncements() {
    std::cout << "Propagating announcements..." << std::endl;
    
    std::cout << "Phase 1: Propagating UP to providers..." << std::endl;
    propagateUp();
    
    std::cout << "Phase 2: Propagating ACROSS to peers..." << std::endl;
    propagateAcross();
    
    std::cout << "Phase 3: Propagating DOWN to customers..." << std::endl;
    propagateDown();
    
    std::cout << "Propagation complete!" << std::endl;
}

/**
 * Load ROV ASes from file
 * Format: one ASN per line
 */
void ASGraph::loadROVASes(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Warning: Could not open ROV file " << filename << std::endl;
        return;
    }
    
    std::string line;
    int count = 0;
    
    while (std::getline(file, line)) {
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') {
            continue;
        }
        
        try {
            uint32_t asn = std::stoul(line);
            AS* as = getAS(asn);
            
            if (as) {
                as->setPolicy(std::make_unique<ROV>());
                count++;
            }
        } catch (const std::exception& e) {
            std::cerr << "Error parsing ROV ASN: " << line << std::endl;
        }
    }
    
    file.close();
    std::cout << "Loaded " << count << " ROV ASes" << std::endl;
}

/**
 * Load announcements from CSV
 * Format: asn,prefix,rov_invalid
 */
void ASGraph::loadAnnouncements(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open announcements file " << filename << std::endl;
        return;
    }
    
    std::string line;
    int count = 0;
    
    // Skip header if present
    std::getline(file, line);
    
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        
        std::istringstream iss(line);
        std::string asn_str, prefix, rov_str;
        
        if (!std::getline(iss, asn_str, ',')) continue;
        if (!std::getline(iss, prefix, ',')) continue;
        if (!std::getline(iss, rov_str, ',')) continue;
        
        try {
            uint32_t asn = std::stoul(asn_str);
            bool rov_invalid = (rov_str == "1" || rov_str == "true" || rov_str == "True");
            
            // Create origin announcement
            std::vector<uint32_t> as_path = {asn};
            Announcement ann(prefix, as_path, asn, Relationship::ORIGIN, rov_invalid);
            
            seedAnnouncement(asn, ann);
            count++;
            
        } catch (const std::exception& e) {
            std::cerr << "Error parsing announcement: " << line << std::endl;
        }
    }
    
    file.close();
    std::cout << "Loaded " << count << " announcements" << std::endl;
    
    // Re-flatten graph in case new ASes were created during seeding
    std::cout << "Re-flattening graph to include origin ASes..." << std::endl;
    flattenGraph();
}

/**
 * Export all local RIBs to CSV
 * Format: asn,prefix,as_path
 */
void ASGraph::exportToCSV(const std::string& filename) const {
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open output file " << filename << std::endl;
        return;
    }
    
    // Write header
    file << "asn,prefix,as_path" << std::endl;
    
    // Write each AS's local RIB
    for (const auto& pair : ases_) {
        uint32_t asn = pair.first;
        const AS* as = pair.second.get();
        
        if (!as->getPolicy()) continue;
        
        const auto& local_rib = as->getPolicy()->getLocalRib();
        
        for (const auto& rib_entry : local_rib) {
            const std::string& prefix = rib_entry.first;
            const Announcement& ann = rib_entry.second;
            
            file << asn << "," << prefix << "," << ann.asPathToString() << std::endl;
        }
    }
    
    file.close();
    std::cout << "Exported local RIBs to " << filename << std::endl;
}