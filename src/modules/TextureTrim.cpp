#include <Geode/Geode.hpp>

#include "../core/Config.hpp"
#include "Modules.hpp"

using namespace geode::prelude;

namespace {

// Al salir de un nivel, libera solo las texturas que nadie referencia
// (retainCount == 1, es decir, solo la caché). Se recargan solas si hacen falta.
class TextureTrim final : public opt::Module {
    bool m_hadPlayLayer = false;

public:
    char const* name() const override { return "TextureTrim"; }

    void onTick(float) override {
        bool hasPlay = PlayLayer::get() != nullptr;
        if (m_hadPlayLayer && !hasPlay && opt::flag("trim-textures")) {
            CCTextureCache::sharedTextureCache()->removeUnusedTextures();
        }
        m_hadPlayLayer = hasPlay;
    }
};

} // namespace

std::unique_ptr<opt::Module> opt::makeTextureTrim() {
    return std::make_unique<TextureTrim>();
}
