# PR 8 — First flattened Box navigation CPU reference

This is an intentionally small first step toward `docs/implementation-plan.md`
PR 8 (batched navigation). The reference scene contains one origin-centered
axis-aligned Box world and zero or more **unrotated, translated, non-touching
Box daughters**. Each daughter has a distinct nonzero integer ID. Only
one level of containment is supported; neither nested placements nor general
transforms/material changes are represented.

`flat_box_next_boundary_mm` computes the first boundary along an
unbounded ray in world millimeters. If the ray begins inside a daughter,
the result is that daughter's local `DistanceToOut`. Otherwise, candidate
distances are the world's `DistanceToOut` and each daughter's translated
`DistanceToIn`. The closest positive or zero distance wins; exact ties favor
world exit. Non-unit directions are accepted and normalized by the existing
CPU Box primitive.

Preconditions are deliberately strict: daughters must fit entirely inside
the world, must be disjoint with a positive gap, and must have unique
identifiers. Ray origins must be inside/on the world and directions finite
and nonzero. Scene validation runs **for every query**. There is no
performance claim.

The CPU test checks nearest daughter entries, daughter exits, world exit,
direction scaling and invalid scenes. A Geant4 integration test constructs
the same world and two translated daughter volumes and compares
`geant4_boundary_distance_mm` from a private Geant4 `G4Navigator`
against the independent CPU result for nondegenerate rays. Surface
coincidences, overlap tolerance, nesting, rotations, GPU batch dispatch,
and batch-size benchmarks are explicitly deferred.

**Safety:** This is standalone reference code, not a replacement for
Geant4 navigation; CPU fallback, particle tracking, RNG and physics remain
unchanged. A future PR can flatten descriptors into contiguous GPU buffers,
compare next boundaries from actual GPU batch execution, and measure
net runtime including transfer and dispatch.
