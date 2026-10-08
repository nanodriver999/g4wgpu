# Process-gated Compton shadow sampling

The Geant4 shadow path now samples a Klein-Nishina final state only when gamma
process competition selects Compton scattering.

Process competition consumes its RNG draws first. The updated counter address
is then forwarded to the Compton sampler, so process selection and final-state
sampling do not reuse the same random numbers.

For tracks where photoelectric absorption or pair production wins, the
corresponding Klein-Nishina shadow slot remains unaccepted and no Compton
sampling is dispatched.

This is still observational only. Geant4 CPU transport remains authoritative.
A later stage can add final-state samplers for the other selected processes and
then promote validated GPU results into track/secondary state updates.
