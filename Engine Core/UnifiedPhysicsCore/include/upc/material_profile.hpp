#pragma once
// Central appearance metadata for UPC materials. Physics voxels remain small:
// each Voxel stores material_id only; color/shading are looked up once per
// material from this registry. Render/physics separation is intentional.
// C++17 header-only, no Unreal dependencies.
#include "upc/core.hpp"
#include <array>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>

namespace upc {

struct MaterialColor {
    float red=0.7f, green=0.7f, blue=0.7f;
};

struct MaterialAppearance {
    MaterialColor base_color{};
    float metallic=0.0f;
    float roughness=0.6f;
    float opacity=1.0f;         // 1 opaque; 0 transparent
    MaterialColor emissive_color{0.0f,0.0f,0.0f};
    float emissive_strength=0.0f;
};

// One authoritative appearance for every material_id, not for every voxel.
// The material ID and physical density/phase remain in MaterialLibrary.
struct MaterialProfile {
    std::uint32_t material_id=0;
    std::string name;
    MaterialAppearance appearance{};
};

class MaterialProfileLibrary {
    const MaterialLibrary& physics_;
    std::unordered_map<std::uint32_t,MaterialProfile> profiles_;
    static void validate_color(const MaterialColor& color) {
        for(float c:{color.red,color.green,color.blue})
            if(!std::isfinite(c) || c<0 || c>1)
                throw std::invalid_argument("Material RGB channels must be finite in [0,1]");
    }
    static void validate(const MaterialProfile& p) {
        if(!p.material_id || p.name.empty())
            throw std::invalid_argument("Material profile needs ID and name");
        validate_color(p.appearance.base_color);
        validate_color(p.appearance.emissive_color);
        for(float c:{p.appearance.metallic,p.appearance.roughness,p.appearance.opacity})
            if(!std::isfinite(c) || c<0 || c>1)
                throw std::invalid_argument("Material shading parameters must be in [0,1]");
        if(!std::isfinite(p.appearance.emissive_strength) ||
           p.appearance.emissive_strength<0)
            throw std::invalid_argument("Invalid material emission strength");
    }
public:
    explicit MaterialProfileLibrary(const MaterialLibrary& physics):physics_(physics){}

    // Register or update once for every material; all voxels using that ID
    // automatically share the new appearance on the next render lookup.
    void set(MaterialProfile profile) {
        validate(profile);
        physics_.at(profile.material_id); // reject unknown physics material ID
        profiles_.insert_or_assign(profile.material_id,std::move(profile));
    }

    const MaterialProfile& at(std::uint32_t material_id) const {
        const auto it=profiles_.find(material_id);
        if(it==profiles_.end())
            throw std::out_of_range("No visual profile for this material ID");
        return it->second;
    }

    bool contains(std::uint32_t material_id)const{
        return profiles_.find(material_id)!=profiles_.end();
    }
    std::size_t size()const{return profiles_.size();}
};

// Reusable visual presets; IDs are supplied by MaterialLibrary clients.
// These are artistic defaults, NOT physically derived spectral optics.
inline MaterialProfile make_water_profile(std::uint32_t material_id) {
    MaterialProfile p;
    p.material_id=material_id;
    p.name="Wasser";
    p.appearance.base_color={0.06f,0.35f,0.82f};
    p.appearance.metallic=0;
    p.appearance.roughness=0.08f;
    p.appearance.opacity=0.45f;
    return p;
}
inline MaterialProfile make_ice_profile(std::uint32_t material_id) {
    MaterialProfile p;
    p.material_id=material_id;
    p.name="Eis";
    p.appearance.base_color={0.64f,0.87f,0.96f};
    p.appearance.roughness=0.22f;
    p.appearance.opacity=0.78f;
    return p;
}
inline MaterialProfile make_sand_profile(std::uint32_t material_id) {
    MaterialProfile p;
    p.material_id=material_id;
    p.name="Sand";
    p.appearance.base_color={0.74f,0.61f,0.38f};
    p.appearance.roughness=0.94f;
    return p;
}
inline MaterialProfile make_metal_profile(std::uint32_t material_id) {
    MaterialProfile p;
    p.material_id=material_id;
    p.name="Metall";
    p.appearance.base_color={0.52f,0.56f,0.60f};
    p.appearance.metallic=1.0f;
    p.appearance.roughness=0.27f;
    return p;
}

} // namespace upc
