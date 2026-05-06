# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

```bash
make                  # Build main binary → build/bgp_simulator
make test             # Build and run all 8 test suites
make run_prefix       # Run simulator on prefix benchmark dataset
make run_many         # Run simulator on many-prefix benchmark dataset
make wasm             # Compile to WebAssembly via Emscripten
make clean            # Remove build artifacts
```

Run a single test binary directly after `make test`:
```bash
./build/test_bgp_policy
```

Usage:
```bash
./build/bgp_simulator <caida_file> <announcements.csv> <rov_asns.txt> <output.csv>
```

## Architecture

This is a BGP (Border Gateway Protocol) route propagation simulator. It models how routing announcements spread across Autonomous Systems (ASes) on the internet, using real CAIDA topology data.

### Pipeline (7 steps in `src/main.cpp`)
1. Parse CAIDA topology file → build AS graph
2. Detect cycles (skip invalid edges)
3. Assign propagation ranks via BFS (determines simulation order)
4. Seed initial announcements from CSV
5. Propagate UP (to providers) → ACROSS (to peers) → DOWN (to customers)
6. Write results to CSV

### Core Classes (`include/`)

**`as_graph.hpp`** — Central orchestrator. Owns all AS objects, builds the graph from CAIDA input, runs the full pipeline. Most complexity lives here.

**`as.hpp`** — Single AS node. Holds vectors of provider/peer/customer relationships, a propagation rank, and a RIB (Routing Information Base) mapping prefix → best Announcement.

**`announcement.hpp`** — A route: prefix, AS path, next-hop ASN, relation type (1-byte enum: CUSTOMER/PEER/PROVIDER), and ROV validity flag.

**`policy.hpp`** — Abstract `Policy` base with two implementations:
- `BGP` — Standard selection: prefer customer routes > peer > provider, then shorter AS path, then lower next-hop ASN.
- `ROV` — Extends BGP; filters out ROV-invalid announcements before selection.

### Key Design Decisions
- Relation stored as `enum class Relation : uint8_t` (not string) to avoid heap allocations in hot loops.
- BGP policy pointers are cached on each AS to avoid `dynamic_cast` during propagation.
- CAIDA file parsing uses `strtol` (not `istringstream`) for zero-allocation line parsing.
- Propagation iterates RIBs directly rather than copying to vectors.
- Output uses `'\n'` not `std::endl` to avoid flush overhead.

### Tests (`tests/`)
8 independent binaries, no external framework — assertions via stdout. Each tests one component:
`test_graph`, `test_announcement`, `test_bgp_policy`, `test_flatten_graph`, `test_seeding`, `test_propagation`, `test_output`, `test_rov`.

### WebAssembly (`src/wasm_main.cpp`, `web/`)
`wasm_main.cpp` wraps the same simulation logic for browser use. Pre-compiled outputs (`bgp_sim.js`, `bgp_sim.wasm`) live in `web/`. The web UI uses vis-network for graph visualization and a Web Worker for background simulation. Built-in presets (Small: 14 ASes, Medium: 99, Large: 250) live in `web/data/`.

### Input Format
CAIDA relationship file: each line is `<AS1>|<AS2>|<relation>` where `-1` = provider→customer, `0` = peer-to-peer. Documented in `data_format.md`.
