#include <Geode/Geode.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <cmath>

#ifdef GEODE_IS_WINDOWS
#include <Windows.h>
#endif

using namespace geode::prelude;

// ===========================================================================
//  GD Optimizer
//  Regla: nada de lo que hace este mod cambia lo que se dibuja. Solo recorta
//  trabajo que el juego hace sin necesidad (frames de más en pausa/menús/
//  segundo plano, memoria sin referencias) y ofrece diagnóstico de stutters.
// ===========================================================================

// ---- helpers de ajustes ---------------------------------------------------

static bool flag(char const* key) {
    return Mod::get()->getSettingValue<bool>(key);
}

static int64_t num(char const* key) {
    return Mod::get()->getSettingValue<int64_t>(key);
}

static double intervalFor(int64_t fps) {
    if (fps < 1) fps = 1;
    return 1.0 / static_cast<double>(fps);
}

// ---- estado del juego -----------------------------------------------------

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

static bool loadingScreenOpen() {
    // La pantalla de carga de GD carga un paso por frame: limitar los FPS
    // ahí alargaría la carga, así que nunca se limita.
    auto scene = CCDirector::get()->getRunningScene();
    if (!scene) return false;
    return scene->getChildByType<LoadingLayer>(0) != nullptr;
}

static bool pauseMenuOpen() {
    auto scene = CCDirector::get()->getRunningScene();
    if (!scene) return false;
    return scene->getChildByType<PauseLayer>(0) != nullptr;
}

static bool insideLevelOrEditor() {
    return PlayLayer::get() != nullptr || LevelEditorLayer::get() != nullptr;
}

// ---- valores en caché para el hook por-frame ------------------------------

static bool   g_logSpikes = false;
static double g_spikeSec  = 0.05;

// ---- vigilante: se ejecuta 4 veces por segundo, no por frame --------------

class OptWatcher : public CCObject {
protected:
    double m_orig = 0.0;   // intervalo original del juego
    double m_last = 0.0;   // último intervalo que puse yo
    bool m_applied = false;
    bool m_hadPlayLayer = false;

    // 0 = no limitar. Si no, el intervalo (segundos/frame) más lento pedido.
    double wantedInterval() {
        if (loadingScreenOpen()) return 0.0;

        double want = 0.0;
        auto consider = [&](double v) { if (v > want) want = v; };

        if (flag("pause-limit-enabled") && pauseMenuOpen()) {
            consider(intervalFor(num("pause-fps")));
        }
        if (flag("menu-limit-enabled") && !insideLevelOrEditor()) {
            consider(intervalFor(num("menu-fps")));
        }
        if (flag("bg-limit-enabled") && !windowHasFocus()) {
            consider(intervalFor(num("bg-fps")));
        }
        return want;
    }

public:
    void tick(float) {
        // refrescar la caché que lee el hook por-frame
        g_logSpikes = flag("log-spikes");
        g_spikeSec = static_cast<double>(num("spike-ms")) / 1000.0;

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

        // al salir de un nivel, liberar solo texturas que nadie referencia
        bool hasPlay = PlayLayer::get() != nullptr;
        if (m_hadPlayLayer && !hasPlay && flag("trim-textures")) {
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

// ---- diagnóstico de picos de frame ----------------------------------------

class $modify(OptPlayLayer, PlayLayer) {
    void update(float dt) {
        PlayLayer::update(dt);

        if (g_logSpikes && dt > g_spikeSec) {
            // como mucho un aviso cada ~0.25 s para no llenar el log
            static double sinceLog = 1.0;
            sinceLog += dt;
            if (sinceLog > 0.25) {
                sinceLog = 0.0;
                log::warn("Pico de frame: {:.1f} ms", dt * 1000.0f);
            }
        }
    }
};
