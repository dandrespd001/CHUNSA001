# MiniMax package — Sprint 1.6 resource lifecycle tests

## Objective

Strengthen the existing deterministic economy test without changing runtime
code. Extend `tests/unit/test_economy.cpp` with focused tests for the pure
`economy.hpp` state machine.

## Allowed implementation

- Edit only `tests/unit/test_economy.cpp`.
- Reuse the existing `CHECK` harness and public APIs from `economy.hpp`.
- Keep the existing end-to-end scenario and its assertions intact.

## Required cases

1. An exhausted assigned deposit makes an empty citizen return to `SEEK` and
   allows deterministic retargeting to the nearest non-exhausted deposit on
   the next step.
2. Harvesting the last amount from a deposit never emits more than
   `remaining` or the citizen carry capacity.
3. Equal-distance deposits select the lower array index.
4. A citizen with carried resources returns and emits one exact dropoff delta,
   then has zero carry and returns to `SEEK`.
5. Invalid assigned-deposit indices never read out of bounds and are replaced
   deterministically.

## Invariants

- Integer/fixed-point logic only.
- No heap allocation in the new focused cases.
- No changes to production code, CMake, schemas, save/replay, or data.
- No network, process, filesystem, environment, dynamic loading, or new
  dependencies.
- Assertions must check exact state/delta values, not merely lack of crashes.

## Acceptance

The existing `economy` executable compiles and passes:

```text
cmake --build build-gcc -j2 --target chunsa_test_economy
ctest --test-dir build-gcc --output-on-failure -R '^economy$'
```
