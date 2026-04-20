#pragma once

#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <cstdint>
#include <fstream>
#include <sstream>
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
    
    // Graph statistics ============================
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
        
        // Create new AS
        auto new_as = std::make_unique<AS>(asn);
        AS* as_ptr = new_as.get();
        ases_[asn] = std::move(new_as);
        
        return as_ptr;
    }

    void flattenGraph();
    
private:
    // Storage for all AS objects (ASN -> AS object)
    std::unordered_map<uint32_t, std::unique_ptr<AS>> ases_;
    
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
            
            // https://publicdata.caida.org/datasets/as-relationships/serial-2/#:~:text=The%20as%2Drel%20files%20contain%20p2p%20and%20p2c%20relationships.%20%20The%20format%20is%3A%0A%3Cprovider%2Das%3E%7C%3Ccustomer%2Das%3E%7C%2D1%0A%3Cpeer%2Das%3E%7C%3Cpeer%2Das%3E%7C0%7C%3Csource%3E

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