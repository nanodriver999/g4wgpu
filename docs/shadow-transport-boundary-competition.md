# Shadow interaction vs geometry-boundary competition

The shadow transport path now compares the sampled physics interaction
distance with the distance to the next Geant4 geometry boundary.

This is required before any sampled final state can become authoritative:
applying a Compton final state at the current track position would otherwise
move the interaction to the wrong location.

For each buffered gamma, the shadow path records:

- sampled physics interaction distance,
- next geometry-boundary distance,
- the winning transport limiter,
- the resulting step distance.

Exact ties are conservatively assigned to geometry. The current PR remains
observational; Geant4 CPU tracking still owns the actual step and track state.
