# Geant4 gamma cross-section reference provider

This stage connects the backend-neutral gamma process-competition layer to
Geant4's initialized electromagnetic physics tables.

The reference provider reads macroscopic cross sections for the standard
gamma process names:

- `compt` — Compton scattering
- `phot` — photoelectric effect
- `conv` — gamma conversion / electron-positron pair production

The values are converted to the portable `mm^-1` contract and passed to the
same process-selection algorithm used by future portable/GPU providers.

This provider is a validation bridge, not the final GPU implementation.
Photoelectric and pair-production cross sections still need portable models
before process competition can run independently of Geant4.
