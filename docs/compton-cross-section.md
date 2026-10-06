# Compton cross section

This stage introduces the interaction-selection quantity needed before GPU
Klein-Nishina samples can become authoritative transport results.

## Scope

The portable function

`klein_nishina_cross_section_per_atom_barn(E, Z)`

implements the same numerical parameterization used by
`G4KleinNishinaCompton::ComputeCrossSectionPerAtom()` while remaining
independent of Geant4 classes and units.

Inputs are explicit:

- gamma energy in MeV
- atomic number Z

The return value is cross section per atom in barns.

## Validation

Two levels are used:

1. standalone numerical invariants over representative energies and elements,
2. direct integration regression against the installed Geant4
   `G4KleinNishinaCompton` implementation.

The Geant4 regression spans the low-energy correction region and the normal
Klein-Nishina parameterization for H, C, Al, Fe, and Pb.

## Why this comes before interaction selection

The current shadow pipeline always samples a Klein-Nishina final state for an
eligible gamma. Real transport first needs to decide whether Compton is the
selected interaction.

This cross-section primitive is the first building block toward:

```text
material composition
    ↓
per-element cross section
    ↓
macroscopic Compton cross section
    ↓
interaction distance / process competition
    ↓
Klein-Nishina final-state sampling
```

The GPU path remains shadow-only until process competition and material
selection are validated.
