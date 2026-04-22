#include <emscripten/emscripten.h>
#include "as_graph.hpp"
#include <algorithm>
#include <sstream>
#include <cstring>

// Persistent result buffer — avoids manual memory management from JS
static std::string g_result;

extern "C" {

EMSCRIPTEN_KEEPALIVE
const char* run_bgp_simulation(
    const char* caida_str,
    const char* anns_str,
    const char* rov_str,     // newline-separated ASNs, may be empty
    uint32_t    target_asn
) {
    try {
        ASGraph graph;

        if (!graph.buildFromCAIDAString(std::string(caida_str))) {
            g_result = R"({"error":"Failed to build AS graph — check topology format"})";
            return g_result.c_str();
        }

        // Parse ROV ASNs (one per line)
        std::unordered_set<uint32_t> rov_asns;
        {
            std::istringstream iss(rov_str);
            std::string line;
            while (std::getline(iss, line)) {
                if (!line.empty() && line.back() == '\r') line.pop_back();
                if (!line.empty()) {
                    try { rov_asns.insert(std::stoul(line)); } catch (...) {}
                }
            }
        }

        graph.initializePolicies(rov_asns);
        graph.flattenGraph();

        if (!graph.seedFromCSVString(std::string(anns_str))) {
            g_result = R"({"error":"Failed to seed announcements — check CSV format"})";
            return g_result.c_str();
        }

        graph.propagateAnnouncements();

        AS* as = graph.getAS(target_asn);
        if (!as) {
            g_result = R"({"error":"Target ASN not found in the topology"})";
            return g_result.c_str();
        }

        BGP* policy = dynamic_cast<BGP*>(as->getPolicy());
        if (!policy) {
            g_result = R"({"error":"No routing policy for target AS"})";
            return g_result.c_str();
        }

        // Collect and sort entries for deterministic output
        const auto& rib = policy->getLocalRIB();
        std::vector<std::pair<std::string, const Announcement*>> entries;
        entries.reserve(rib.size());
        for (const auto& e : rib) entries.push_back({e.first, &e.second});
        std::sort(entries.begin(), entries.end(),
            [](const auto& a, const auto& b) { return a.first < b.first; });

        std::ostringstream json;
        json << "[";
        for (size_t i = 0; i < entries.size(); ++i) {
            if (i) json << ",";
            const auto& path = entries[i].second->getASPath();
            json << "{\"prefix\":\"" << entries[i].first << "\","
                 << "\"as_path\":[";
            for (size_t j = 0; j < path.size(); ++j) {
                if (j) json << ",";
                json << path[j];
            }
            json << "],\"rov_invalid\":"
                 << (entries[i].second->isROVInvalid() ? "true" : "false")
                 << "}";
        }
        json << "]";

        g_result = json.str();
        return g_result.c_str();

    } catch (const std::exception& e) {
        g_result = std::string(R"({"error":")") + e.what() + "\"}";
        return g_result.c_str();
    } catch (...) {
        g_result = R"({"error":"Unknown error during simulation"})";
        return g_result.c_str();
    }
}

} // extern "C"
