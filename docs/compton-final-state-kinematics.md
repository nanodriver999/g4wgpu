# Portable Compton final-state kinematics

The Klein-Nishina sampler produces scattered gamma energy, recoil-electron
kinetic energy, polar angle, and azimuth. This stage converts that sample into
lab-frame outgoing directions.

The portable kinematics layer:

- normalizes the incident gamma direction,
- constructs a stable orthonormal basis around it,
- rotates the sampled polar/azimuthal angles into a lab-frame gamma direction,
- obtains the recoil-electron momentum direction from momentum subtraction,
- checks energy and relativistic electron-momentum consistency.

This creates the kinematic object needed before a validated GPU Compton result
can be promoted into a Geant4 gamma track plus recoil-electron secondary.
It does not mutate Geant4 state.
