# Geant4 shadow Compton final-state materialization

The Geant4 shadow pipeline now converts accepted Klein-Nishina samples into
portable lab-frame Compton final states.

For each buffered gamma:

1. portable gamma process competition selects the winning process,
2. only Compton-selected tracks enter the Klein-Nishina sampler,
3. the sampled energy and angles are rotated from the incident-track frame
   into lab-frame outgoing gamma and recoil-electron directions,
4. the resulting final state is stored in an optional slot aligned with the
   original batch index.

Non-Compton tracks keep an empty final-state slot.

The integration tests verify:

- process gating remains intact,
- outgoing directions are unit vectors,
- scattered-gamma plus recoil-electron kinetic energy conserves the incident
  gamma energy within the CPU/WebGPU precision contract.

This remains shadow-only. The generated final state is not yet applied to
`G4Track` and no Geant4 secondary electron is created. A later promotion
stage can use these validated records to implement authoritative Compton
transport behind an explicit feature gate.
