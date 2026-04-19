# Highlights on submission for Part 1 - 3 Passes:

* The blocks-level outputs of Liveness (bb and bb2 switch places) and Reaching definitions (%13 is not at the end) are out of order.
	- Explanation: The blocks are at the same hierarchy/level in the CFG, so their change in position is understandable.
* The elements within the IN, OUT, GEN, KILL, USE and DEF sets are unordered but the set as a whole is valid.
	- The elemental order is based on data structures and varies with implementation.
* Hence, the output and the ground truth for the Available Expressions, Liveness and Reaching Definitions match perfectly.

# Assignment 2 Starter Code

This starter package contains:
- `unifiedpass.cpp`: LLVM plugin starter with
  - reusable fixed-point dataflow engine skeleton
  - set-print helper utilities
  - a partially wired Available Expressions template (with TODO transfer logic)
  - a map-based Constant Propagation starter using a 3-point lattice
    (`TOP`, `Const`, `NAC`) with TODO extension points
  - pass registration for
  - `available`
  - `liveness`
  - `reaching`
  - `constantprop`
- `Makefile`: build + run targets
- `tests/`: 4 provided test inputs (`*.bc`)

## Build

```bash
make
```

This builds `build/unifiedpass.so`.

## Run all tests

```bash
make tests
```

This generates:
- `build/tests/*-m2r.ll` (disassembled inputs)
- `build/tests/*-opt.ll` (outputs after running each pass)

## Run one pass manually

```bash
opt -bugpoint-enable-legacy-pm=1 \
  -load-pass-plugin=build/unifiedpass.so \
  -passes='available' tests/available-test-m2r.bc -o /tmp/out.bc
```

Replace `available` with one of: `liveness`, `reaching`, `constantprop`.


