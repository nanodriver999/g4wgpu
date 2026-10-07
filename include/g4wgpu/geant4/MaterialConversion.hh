#pragma once

#include "g4wgpu/MaterialInteraction.hh"

class G4Material;

namespace g4wgpu {

MaterialView make_material_view_from_geant4(
    const G4Material& material);

}  // namespace g4wgpu
