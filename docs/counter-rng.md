# Counter-based RNG prototype

This module establishes a deterministic CPU/WGSL random-addressing contract for
batched transport work.

## Address model

Each track carries:

- `rng_stream_lo`
- `rng_stream_hi`
- `rng_counter_lo`
- `rng_counter_hi`

Compaction preserves all four values. Therefore moving a track between CPU
queues and compact GPU batches does not implicitly change its random stream.

A sample is addressed by:

```text
(stream_lo, stream_hi, counter_lo, counter_hi, lane)
```

and does not depend on thread scheduling or batch position.

## Cross-backend reproducibility

The CPU implementation uses only wrapping 32-bit integer operations.
`shaders/rng/counter_rng.wgsl` mirrors the same operations.

`uniform_f32` uses the upper 24 bits of the generated integer so that the
result is representable by portable WGSL `f32`.

A fixed reference vector is included in unit tests to detect accidental changes
to the sequence.

## Scientific-quality warning

The current generator is a **prototype addressing/reproducibility generator**.
It has not yet been qualified as the production Monte Carlo RNG for Geant4
physics.

Before it is used for physics sampling, the project must either:

1. replace it with a well-studied counter-based generator suitable for
   scientific Monte Carlo, or
2. qualify the selected generator with reproducibility and statistical testing
   appropriate for the target physics workloads.

At minimum, later validation should include standard PRNG batteries plus
application-level Geant4 observables.

The important architectural decision in this PR is the stable
stream/counter addressing model, not endorsement of this hash as the final
physics RNG.
