#include "g4wgpu/geant4/MaterialConversion.hh"

#include <cstdint>
#include <stdexcept>

#include "G4Element.hh"
#include "G4Material.hh"
#include "G4SystemOfUnits.hh"

namespace g4wgpu {

MaterialView make_material_view_from_geant4(
    const G4Material& material) {
  const auto* elements = material.GetElementVector();
  const auto* number_density =
      material.GetVecNbOfAtomsPerVolume();

  if (elements == nullptr || number_density == nullptr) {
    throw std::runtime_error(
        "Geant4 material has no element composition");
  }

  MaterialView view;
  view.material_id =
      static_cast<std::uint32_t>(material.GetIndex());
  view.elements.reserve(material.GetNumberOfElements());

  for (std::size_t i = 0;
       i < material.GetNumberOfElements();
       ++i) {
    const auto* element = (*elements)[i];
    if (element == nullptr) {
      throw std::runtime_error(
          "Geant4 material contains a null element");
    }

    // Geant4 number density is stored in internal inverse-volume units.
    // Convert explicitly to atoms/mm^3 for the portable contract.
    const double density_per_mm3 =
        number_density[i] * CLHEP::mm3;

    view.elements.push_back(MaterialElement{
        element->GetZ(),
        density_per_mm3});
  }

  view.validate();
  return view;
}

}  // namespace g4wgpu
