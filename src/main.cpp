#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <iostream>

struct AS {
    u_int32_t asn;
    std::unordered_set<u_int32_t> providers;
    std::unordered_set<u_int32_t> customers;
    std::unordered_set<u_int32_t> peers;
    
    AS(u_int32_t asn) : asn(asn) {}
};

class ASGraph {
private:
    std::unordered_map<u_int32_t, AS*> ases;
    
    // Get or create an AS node
    AS* getOrCreateAS(u_int32_t asn) {
        if (ases.find(asn) == ases.end()) {
            ases[asn] = new AS(asn);
        }
        return ases[asn];
    }
    
public:
    bool parseCAIDAFile(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Error opening file: " << filename << std::endl;
            return false;
        }
        
        std::string line;
        while (std::getline(file, line)) {
            // Skip comments and empty lines
            if (line.empty() || line[0] == '#') {
                continue;
            }
            
            // Parse the line: provider|customer|relationship|source
            std::istringstream iss(line);
            std::string provider_str, customer_str, rel_str, source;
            
            if (!std::getline(iss, provider_str, '|') ||
                !std::getline(iss, customer_str, '|') ||
                !std::getline(iss, rel_str, '|') ||
                !std::getline(iss, source, '|')) {
                std::cerr << "Malformed line: " << line << std::endl;
                continue;
            }
            
            u_int32_t provider_asn = std::stoul(provider_str);
            u_int32_t customer_asn = std::stoul(customer_str);
            int relationship = std::stoi(rel_str);
            
            // Get or create both ASes
            AS* provider = getOrCreateAS(provider_asn);
            AS* customer = getOrCreateAS(customer_asn);
            
            if (relationship == 0) {
                // Provider-to-customer relationship
                provider->customers.insert(customer_asn);
                customer->providers.insert(provider_asn);
            } 
            else if (relationship == -1) {
                // Peer-to-peer relationship (bidirectional)
                provider->peers.insert(customer_asn);
                customer->peers.insert(provider_asn);
            }
        }
        
        file.close();
        return true;
    }
    
    // Check for cycles in provider-customer relationships
    bool hasCycles() {
        // TODO: Implement cycle detection (DFS-based)
        // Return true if cycle found, false otherwise
        return false;
    }
    
    ~ASGraph() {
        for (auto& pair : ases) {
            delete pair.second;
        }
    }
};


int main(int argc, char* argv[]) {
    ASGraph graph;
    
    // Parse the CAIDA file
    if (!graph.parseCAIDAFile("bench/many/CAIDAASGraphCollector_2025.10.15.txt")) {
        return 1;
    }
    
    // Check for cycles
    if (graph.hasCycles()) {
        std::cerr << "Error: Cycle detected in AS relationships!" << std::endl;
        return 1;
    }
    
    std::cout << "Graph loaded successfully!" << std::endl;
    
    // Continue with announcement propagation...
    
    return 0;
}