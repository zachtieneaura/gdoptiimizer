#include <Geode/Geode.hpp>
#include <Geode/modify/CCDirector.hpp>
#include <Geode/modify/CCSpriteBatchNode.hpp>

#include <chrono>
#include <cstdint>

#include "../core/Config.hpp"
#include "Modules.hpp"

using namespace geode::prelude;

// ---------------------------------------------------------------------------
//  Profiler de render. SOLO MIDE: no cambia nada de lo que se dibuja.
//  Sirve para decidir con datos dónde merece la pena optimizar (batching,
//  culling...) antes de escribir hooks más invasivos.
// ---------------------------------------------------------------------------

namespace {

bool g_profile = false;           // lo actualiza el módulo en cada tick
uint64_t g_frames = 0;
uint64_t g_batches = 0;           // llamadas a CCSpriteBatchNode::draw
uint64_t g_quads = 0;             // quads enviados en esos batches
uint64_t g_frameNs = 0;           // tiempo total de drawScene (incluye vsync)
uint64_t g_worstNs = 0;           // peor frame de la ventana

void resetCounters() {
    g_frames = g_batches = g_quads = g_frameNs = g_worstNs = 0;
}

class FrameProfiler final : public opt::Module {
    float m_acc = 0.f;

public:
    char const* name() const override { return "FrameProfiler"; }

    void onTick(float dt) override {
        bool enabled = opt::flag("profiler-enabled");
        if (enabled != g_profile) {
            g_profile = enabled;
            resetCounters();
            m_acc = 0.f;
        }
        if (!g_profile) return;

        m_acc += dt;
        if (m_acc < static_cast<float>(opt::num("profiler-interval"))) return;

        if (g_frames > 0) {
            double frames = static_cast<double>(g_frames);
            log::info(
                "[Profiler] FPS {:.0f} | batches/frame {:.1f} | quads/frame {:.0f} | "
                "frame medio {:.2f} ms | peor frame {:.2f} ms (incluye vsync)",
                frames / m_acc,
                static_cast<double>(g_batches) / frames,
                static_cast<double>(g_quads) / frames,
                static_cast<double>(g_frameNs) / frames / 1e6,
                static_cast<double>(g_worstNs) / 1e6
            );
        }
        resetCounters();
        m_acc = 0.f;
    }
};

} // namespace

std::unique_ptr<opt::Module> opt::makeFrameProfiler() {
    return std::make_unique<FrameProfiler>();
}

// Hooks mínimos: con el profiler apagado solo cuestan una comparación.

class $modify(ProfDirector, CCDirector) {
    void drawScene() {
        if (!g_profile) {
            CCDirector::drawScene();
            return;
        }
        auto t0 = std::chrono::steady_clock::now();
        CCDirector::drawScene();
        auto ns = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - t0
            ).count()
        );
        g_frames++;
        g_frameNs += ns;
        if (ns > g_worstNs) g_worstNs = ns;
    }
};

class $modify(ProfBatchNode, CCSpriteBatchNode) {
    void draw() {
        if (g_profile) {
            g_batches++;
            if (auto atlas = this->getTextureAtlas()) {
                g_quads += atlas->getTotalQuads();
            }
        }
        CCSpriteBatchNode::draw();
    }
};
