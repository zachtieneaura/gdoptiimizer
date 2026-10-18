#include <Geode/Geode.hpp>
#include <cmath>

#include "../core/Config.hpp"
#include "Modules.hpp"

#ifdef GEODE_IS_WINDOWS
#include <Windows.h>
#endif

using namespace geode::prelude;

namespace {

double intervalFor(int64_t fps) {
    if (fps < 1) fps = 1;
    return 1.0 / static_cast<double>(fps);
}

bool windowHasFocus() {
#ifdef GEODE_IS_WINDOWS
    HWND fg = GetForegroundWindow();
    if (!fg) return false;
    DWORD pid = 0;
    GetWindowThreadProcessId(fg, &pid);
    return pid == GetCurrentProcessId();
#else
    return true;
#endif
}

// La pantalla de carga carga un paso por frame: limitar FPS ahí la alargaría.
bool loadingScreenOpen() {
    auto scene = CCDirector::get()->getRunningScene();
    return scene && scene->getChildByType<LoadingLayer>(0) != nullptr;
}

bool pauseMenuOpen() {
    auto scene = CCDirector::get()->getRunningScene();
    return scene && scene->getChildByType<PauseLayer>(0) != nullptr;
}

bool insideLevelOrEditor() {
    return PlayLayer::get() != nullptr || LevelEditorLayer::get() != nullptr;
}

class FpsLimiter final : public opt::Module {
    double m_orig = 0.0;   // intervalo original del juego
    double m_last = 0.0;   // último intervalo que puse yo
    bool m_applied = false;

    // 0 = no limitar; si no, el intervalo más lento pedido (segundos/frame).
    double wantedInterval() {
        if (!opt::flag("fps-limiter-enabled")) return 0.0;
        if (loadingScreenOpen()) return 0.0;

        double want = 0.0;
        auto consider = [&](double v) { if (v > want) want = v; };

        if (opt::flag("pause-limit-enabled") && pauseMenuOpen()) {
            consider(intervalFor(opt::num("pause-fps")));
        }
        if (opt::flag("menu-limit-enabled") && !insideLevelOrEditor()) {
            consider(intervalFor(opt::num("menu-fps")));
        }
        if (opt::flag("bg-limit-enabled") && !windowHasFocus()) {
            consider(intervalFor(opt::num("bg-fps")));
        }
        return want;
    }

public:
    char const* name() const override { return "FpsLimiter"; }

    void onTick(float) override {
        auto dir = CCDirector::get();
        double cur = dir->getAnimationInterval();
        double want = this->wantedInterval();

        if (want > 0.0) {
            if (!m_applied) {
                m_orig = cur;
                m_applied = true;
            }
            else if (std::fabs(cur - m_last) > 1e-6) {
                // otro mod o el juego cambió el FPS: respetamos ese valor
                m_orig = cur;
            }
            // nunca subimos los FPS por encima del valor original
            double target = want > m_orig ? want : m_orig;
            if (std::fabs(target - cur) > 1e-6) {
                dir->setAnimationInterval(target);
            }
            m_last = target;
        }
        else if (m_applied) {
            if (std::fabs(cur - m_last) < 1e-6) {
                dir->setAnimationInterval(m_orig);
            }
            m_applied = false;
        }
    }
};

} // namespace

std::unique_ptr<opt::Module> opt::makeFpsLimiter() {
    return std::make_unique<FpsLimiter>();
}
