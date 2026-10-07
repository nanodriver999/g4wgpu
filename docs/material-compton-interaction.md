# Material-aware Compton interaction selection

This stage lifts the validated per-atom Compton cross section into a
backend-neutral material transport quantity.

For a material with element number densities n_i and per-atom cross sections
sigma_i, the macroscopic cross section is

Sigma = sum_i n_i sigma_i

with Sigma expressed in mm^-1.

The mean free path is 1/Sigma, and the shadow interaction distance is sampled
as -log(U)/Sigma using the stable counter-based RNG.

This remains a Compton-only shadow calculation. It does not yet compete with
photoelectric, pair production, or other gamma processes and does not mutate
authoritative Geant4 transport state.
