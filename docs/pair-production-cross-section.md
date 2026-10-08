# Portable pair-production cross section

Geant4's standard `G4GammaConversion` uses
`G4PairProductionRelModel`. Below the model's high-energy numerical
integration regime, Geant4 evaluates the total atomic pair-production cross
section using the Urban/Hubbell parameterization.

This stage ports that parameterization into the backend-neutral core.

The portable contract provides:

- per-atom cross section in barns,
- material macroscopic cross section in mm^-1,
- exact kinematic threshold handling at 2 electron rest masses,
- the same quadratic correction used by Geant4 below 1.5 MeV.

This makes the pair-production input to gamma process competition portable for
the parameterized energy range. Very-high-energy numerical integration and
LPM suppression remain future work.
