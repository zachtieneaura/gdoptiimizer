#include <Geode/Geode.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <cmath>

#ifdef GEODE_IS_WINDOWS
#include <Windows.h>
#endif

using namespace geode::prelude;

// ---------------------------------------------------------------------------
// Ninguna de estas optimizaciones toca la calidad gráfica: solo reducen
// trabajo innecesario (pausa, ventana sin foco, memoria sin uso).
// ---------------------------------------------------------------------------

static bool windowHasFocus() {
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

static bool pauseMenuOpen() {
    auto scene = CCDirector::get()->getRunningScene();
    if (!scene) return false;
    return scene->getChildByType<PauseLayer>(0) != nullptr;
}

class OptWatcher : public CCObject {
protected:
    double m_orig = 0.0;      // intervalo original del juego
    double m_last = 0.0;      // último intervalo que puse yo
    bool m_applied = false;
    bool m_hadPlayLayer = false;

    // devuelve 0 si no hay que limitar nada; si no, el intervalo deseado
    double wantedInterval() {
        auto mod = Mod::get();
        double want = 0.0;

        if (mod->getSettingValue<bool>("pause-limit-enabled") && pauseMenuOpen()) {
            auto fps = mod->getSettingValue<int64_t>("pause-fps");
            if (fps < 1) fps = 1;
            want = 1.0 / static_cast<double>(fps);
        }
        if (mod->getSettingValue<bool>("bg-limit-enabled") && !windowHasFocus()) {
            auto fps = mod->getSettingValue<int64_t>("bg-fps");
            if (fps < 1) fps = 1;
            double bg = 1.0 / static_cast<double>(fps);
            if (bg > want) want = bg; // intervalo mayor = menos FPS
        }
        return want;
    }

public:
    void tick(float) {
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

        // al salir de un nivel, liberar solo texturas que nadie usa
        bool hasPlay = PlayLayer::get() != nullptr;
        if (m_hadPlayLayer && !hasPlay && Mod::get()->getSettingValue<bool>("trim-textures")) {
            CCTextureCache::sharedTextureCache()->removeUnusedTextures();
        }
        m_hadPlayLayer = hasPlay;
    }
};

class $modify(OptMenuLayer, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) return false;

        static bool started = false;
        if (!started) {
            started = true;
            auto watcher = new OptWatcher();
            CCDirector::get()->getScheduler()->scheduleSelector(
                schedule_selector(OptWatcher::tick), watcher, 0.25f, false
            );
        }
        return true;
    }
};
