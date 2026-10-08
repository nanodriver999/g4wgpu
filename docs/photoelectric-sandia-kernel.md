# Portable photoelectric Sandia kernel

Geant4's standard `G4PhotoElectricEffect` uses
`G4PEEffectFluoModel`. Its macroscopic photoelectric cross section is
evaluated from four Sandia coefficients as a polynomial in inverse photon
energy.

This stage separates that small numerical kernel from Geant4:

`sigma(E) = a0/E + a1/E^2 + a2/E^3 + a3/E^4`

where the portable contract uses:

- photon energy in MeV,
- macroscopic cross section in mm^-1.

The Geant4 adapter selects the appropriate material/energy Sandia segment and
normalizes its dimensional coefficients into that contract.

The Sandia data lookup itself remains a host-side preprocessing concern for
now. The polynomial evaluation is backend-neutral and suitable for a later
WGSL implementation.
