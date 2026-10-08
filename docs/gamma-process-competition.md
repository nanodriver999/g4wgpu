# Gamma process competition

This stage introduces a backend-neutral process-selection layer for gamma
transport.

Given macroscopic cross sections for:

- Compton scattering
- photoelectric absorption
- pair production

the selector:

1. sums the cross sections,
2. samples the distance to the next interaction from the total macroscopic
   cross section,
3. selects the winning process proportional to its contribution.

This isolates process competition from process-specific cross-section models.
PR #9/#10 provide the Compton side. Photoelectric and pair-production
cross-section providers can be attached later without changing the selection
algorithm.

The implementation remains shadow-only and does not mutate Geant4 transport
state.
