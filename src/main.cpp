#include <Geode/Geode.hpp>
#include <Geode/modify/MenuLayer.hpp>

#include "core/Module.hpp"
#include "modules/Modules.hpp"

using namespace geode::prelude;

// ---------------------------------------------------------------------------
//  GD Optimizer by Zachbtw
//  Arquitectura: un núcleo mínimo que reparte un "tick" (4 veces/s) a una
//  lista de módulos. Cada módulo tiene su interruptor en los ajustes y no
//  toca lo que se dibuja ni la lógica del juego.
// ---------------------------------------------------------------------------

static void registerAllModules() {
    auto& reg = opt::Registry::get();
    reg.add(opt::makeFpsLimiter());
    reg.add(opt::makeTextureTrim());
    reg.add(opt::makeFrameProfiler());
    reg.add(opt::makeGpuProbe());
}

class OptCore : public CCObject {
public:
    void tick(float dt) {
        opt::Registry::get().tickAll(dt);
    }
};

class $modify(CoreMenuLayer, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) return false;

        static bool started = false;
        if (!started) {
            started = true;
            registerAllModules();

            auto core = new OptCore();
            CCDirector::get()->getScheduler()->scheduleSelector(
                schedule_selector(OptCore::tick), core, 0.25f, false
            );
        }
        return true;
    }
};
