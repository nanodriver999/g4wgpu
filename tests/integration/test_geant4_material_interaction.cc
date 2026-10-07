#include "g4wgpu/ComptonCrossSection.hh"
#include "g4wgpu/MaterialInteraction.hh"
#include "g4wgpu/geant4/MaterialConversion.hh"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>

#include "G4Material.hh"
#include "G4NistManager.hh"
#include "G4SystemOfUnits.hh"

namespace {

bool close_relative(
    const double a,
    const double b,
    const double tolerance = 2.0e-7) {
  const double scale =
      std::max(std::fabs(a), std::fabs(b));
  if (scale == 0.0) {
    return true;
  }
  return std::fabs(a - b) <= tolerance * scale;
}

double geant4_reference_sigma_per_mm(
    const G4Material& material,
    const double energy_mev) {
  const auto* elements = material.GetElementVector();
  const auto* densities =
      material.GetVecNbOfAtomsPerVolume();

  double sigma_internal = 0.0;
  for (std::size_t i = 0;
       i < material.GetNumberOfElements();
       ++i) {
    const double sigma_barn =
        g4wgpu::klein_nishina_cross_section_per_atom_barn(
            energy_mev,
            (*elements)[i]->GetZ());

    sigma_internal +=
        densities[i] *
        sigma_barn * CLHEP::barn;
  }

  return sigma_internal * CLHEP::mm;
}

}  // namespace

int main() {
  auto* nist = G4NistManager::Instance();

  const char* names[] = {
      "G4_WATER",
      "G4_Al",
      "G4_Fe",
  };

  constexpr double energies_mev[] = {
      0.02,
      0.1,
      1.0,
      10.0,
  };

  for (const char* name : names) {
    const G4Material* material =
        nist->FindOrBuildMaterial(name);

    if (material == nullptr) {
      std::cerr << "failed to construct material " << name << '\n';
      return EXIT_FAILURE;
    }

    const auto view =
        g4wgpu::make_material_view_from_geant4(*material);

    if (view.elements.size() !=
        material->GetNumberOfElements()) {
      std::cerr << "element count mismatch for " << name << '\n';
      return EXIT_FAILURE;
    }

    for (const double energy_mev : energies_mev) {
      const double portable =
          g4wgpu::compton_macroscopic_cross_section_per_mm(
              view, energy_mev);
      const double reference =
          geant4_reference_sigma_per_mm(
              *material, energy_mev);

      if (!close_relative(portable, reference)) {
        std::cerr
            << "material cross-section mismatch for "
            << name
            << " E=" << energy_mev
            << " MeV portable=" << portable
            << " 1/mm reference=" << reference
            << " 1/mm\n";
        return EXIT_FAILURE;
      }

      const double mfp =
          g4wgpu::compton_mean_free_path_mm(
              view, energy_mev);

      if (!(mfp > 0.0) ||
          !std::isfinite(mfp) ||
          !close_relative(mfp, 1.0 / reference)) {
        std::cerr
            << "mean free path mismatch for "
            << name
            << " E=" << energy_mev << " MeV\n";
        return EXIT_FAILURE;
      }
    }
  }

  return EXIT_SUCCESS;
}
