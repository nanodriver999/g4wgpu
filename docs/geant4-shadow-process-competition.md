# Geant4 shadow process competition

The Geant4 tracking adapter now records an observational gamma-process
competition sample for every buffered gamma when shadow physics is enabled.

For each track, the adapter:

1. reads the configured Geant4 material,
2. queries Geant4 macroscopic cross sections for Compton, photoelectric, and
   pair production,
3. samples the total interaction distance and winning process through the
   backend-neutral competition algorithm,
4. keeps the existing Klein-Nishina shadow sample for validation,
5. leaves authoritative Geant4 transport unchanged.

The competition RNG uses a copy of the track's stable RNG address, so this
validation path does not modify the RNG stream passed to the existing
Klein-Nishina shadow backend.

This is still a reference/shadow stage. Portable photoelectric and
pair-production cross-section providers are required before the competition
can run independently of Geant4.
