# BGP Simulator

## The Simulator

This is a BGP (Border Gateway Protocol) simulator written in C++. BGP is the protocol that routers on the internet use to figure out how to get traffic from one place to another — every major network (called an Autonomous System, or AS) runs it to share routing information with its neighbors.

The simulator takes a real-world internet topology from CAIDA (a dataset of AS relationships) and a set of route announcements, then simulates how those routes would spread across the network. Each AS can run either plain BGP or ROV (Route Origin Validation), which is a security extension that lets an AS reject routes it knows are invalid. The route selection follows real BGP preference rules: a route learned from a customer is preferred over one from a peer, which is preferred over one from a provider, with AS path length and next-hop ASN as tiebreakers.

Propagation works in three phases — routes travel up to providers first, then across to peers, then down to customers — which reflects how the real internet works. At the end, you get a routing table (RIB) for every AS showing which route it picked for each prefix and how it got there.

To run it from the command line:

```
./build/bgp_simulator <caida_file> <announcements.csv> <rov_asns.txt> <output.csv>
```

### Algorithms and Complexity

The main stages of the simulator and their time complexity:

| Stage | Algorithm | Complexity |
|---|---|---|
| Graph construction | Adjacency list build | O(V + E) |
| Cycle detection | DFS | O(V + E) |
| Rank assignment (flatten) | BFS from leaves upward | O(V + E) |
| Route propagation (each phase) | Per-AS RIB broadcast | O(A × P̄ × D̄) |
| Route selection | Best-of-candidates scan | O(D̄) per prefix |

Where V = ASes, E = relationships, A = ASes with routes, P̄ = average prefixes per AS, D̄ = average neighbor degree. Propagation dominates for large topologies.

### Optimizations

**Cached policy pointer.** Each AS now caches a raw `BGP*` alongside its owning `unique_ptr<Policy>` at initialization time. The propagation loop previously called `dynamic_cast` on every AS at every rank — which walks the RTTI hierarchy at runtime — and was replaced with a direct pointer read.

**Relation enum.** The `received_from` field on each announcement was stored as a heap-allocated `std::string` (`"customer"`, `"peer"`, etc.). It is now a 1-byte enum. This removes a heap allocation every time an announcement is copied during propagation, and the BGP route selection comparison (`origin > customer > peer > provider`) became a direct integer comparison instead of a chain of string comparisons.

**Direct RIB iteration.** The propagation code used to call `getAnnouncementsToSend()`, which constructed a fresh `std::vector<Announcement>` by copying every entry out of the routing table. The propagation loop then copied each entry again into a `sent_ann`. The vector copy was eliminated by iterating the RIB map directly by const reference.

**Manual CAIDA line parsing.** The topology file parser previously constructed a `std::istringstream` for every line to split on `|`. For a full CAIDA file this means over 100,000 stream objects allocated and destroyed just to do string splitting. It was replaced with `strtoul`/`strtol` directly on the raw character pointer, which does the same work with zero allocations.

**Single-pass rank structure build.** The graph flattening previously ran a BFS to assign ranks (building `rank_structure_` along the way), then discarded `rank_structure_` entirely and rebuilt it from scratch because ranks can be bumped upward during BFS. Now the BFS only tracks which ASes to visit next, and `rank_structure_` is built once at the end from the final rank assignments.

**Buffered CSV output.** The output writer used `std::endl` after every row, which flushes the I/O buffer to disk each time. For large simulations with millions of routing table entries this caused millions of unnecessary flushes. Replaced with `'\n'`, which lets the OS buffer fill and flush naturally.

## The Website

The website is a browser-based front-end for the same simulator, with the C++ core compiled to WebAssembly so everything runs locally — no server involved. It has a dark GitHub-inspired look and is laid out as a two-panel interface: inputs on the left, results on the right, and a live interactive graph below.

There are three built-in presets (Small at 14 ASes, Medium at 99, and Large at 250) that pre-fill all the inputs so you can get started immediately. You can also upload your own CAIDA topology file, type in announcements manually row by row or load them from a CSV, and specify which ASes should run ROV. Once you enter a target ASN and hit Run, the simulator runs in a background worker thread and populates a routing table showing every prefix the target AS learned, its AS path, where it came from, and whether any routes were flagged ROV-invalid.

The AS graph visualization uses vis-network with live physics and rank-based node sizing, so the tier-1 provider ASes float to the top and smaller customer ASes settle at the bottom naturally. Nodes on active routing paths are highlighted, and you can click any node in the graph to seed an announcement directly from it. Flow particles travel along the edges to show the direction of route propagation.
