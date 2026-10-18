#pragma once
#include <Geode/Geode.hpp>

namespace opt {

// Acceso a los ajustes definidos en mod.json.
// Se usan desde los módulos en su onTick (4 veces/s), nunca por frame.
inline bool flag(char const* key) {
    return geode::Mod::get()->getSettingValue<bool>(key);
}

inline int64_t num(char const* key) {
    return geode::Mod::get()->getSettingValue<int64_t>(key);
}

} // namespace opt
