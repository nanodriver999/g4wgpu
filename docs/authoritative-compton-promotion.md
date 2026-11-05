# Feature-gated authoritative Compton promotion

The custom Geant4 tracking manager can now optionally promote a validated
shadow Compton final state into Geant4 transport state.

The feature is disabled by default through
`TrackingPolicy::enable_authoritative_compton`.

Promotion is allowed only when:

1. gamma process competition selects Compton,
2. the sampled interaction distance is strictly before the next geometry
   boundary,
3. the Klein-Nishina sample is accepted,
4. a validated lab-frame Compton final state exists.

When promoted, the gamma is advanced to the sampled interaction point,
its time, track length, energy, and direction are updated, and a recoil
electron track is created at the same space-time point. The gamma keeps its
track identity and both outgoing particles are returned to the Geant4 stack.

All non-Compton interactions, geometry-limited steps, and disabled-policy
cases continue through the existing Geant4 CPU fallback.

This is deliberately an opt-in incremental transport path. Photoelectric and
pair-production final-state promotion remain future work.
