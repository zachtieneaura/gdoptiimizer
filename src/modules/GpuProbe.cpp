#include <Geode/Geode.hpp>

#include <cstdio>
#include <string>

#include "../core/Config.hpp"
#include "Modules.hpp"

using namespace geode::prelude;

namespace {

std::string glString(GLenum name) {
    auto s = glGetString(name);
    return s ? std::string(reinterpret_cast<char const*>(s)) : std::string("(no disponible)");
}

// Informa de qué permite realmente la GPU/contexto OpenGL de esta máquina.
// SOLO LEE: no crea shaders ni recursos, no cambia estado de render.
// Sirve para saber si técnicas como compute shaders o draw indirect son
// siquiera posibles antes de diseñar nada sobre ellas.
class GpuProbe final : public opt::Module {
    bool m_done = false;

public:
    char const* name() const override { return "GpuProbe"; }

    void onTick(float) override {
        if (!opt::flag("gpu-probe-enabled")) {
            m_done = false;   // permite repetirlo apagando y encendiendo
            return;
        }
        if (m_done) return;
        m_done = true;

        std::string version  = glString(GL_VERSION);
        std::string renderer = glString(GL_RENDERER);
        std::string vendor   = glString(GL_VENDOR);
        std::string glsl     = glString(GL_SHADING_LANGUAGE_VERSION);
        std::string ext      = glString(GL_EXTENSIONS);

        int major = 0, minor = 0;
        std::sscanf(version.c_str(), "%d.%d", &major, &minor);

        bool gl43 = major > 4 || (major == 4 && minor >= 3);
        bool compute = gl43 || ext.find("GL_ARB_compute_shader") != std::string::npos;
        bool indirect = gl43 || ext.find("GL_ARB_multi_draw_indirect") != std::string::npos;
        bool instancing = major >= 3 || ext.find("GL_ARB_draw_instanced") != std::string::npos;

        // glGetString(GL_EXTENSIONS) puede dejar un error pendiente en contextos
        // modernos; lo limpiamos para no dejar rastro.
        glGetError();

        log::info("[GpuProbe] GPU: {} ({})", renderer, vendor);
        log::info("[GpuProbe] OpenGL {} | GLSL {}", version, glsl);
        log::info(
            "[GpuProbe] compute shaders: {} | draw indirect: {} | instancing: {}",
            compute ? "SI" : "NO", indirect ? "SI" : "NO", instancing ? "SI" : "NO"
        );
        if (ext == "(no disponible)") {
            log::info("[GpuProbe] la lista de extensiones no esta disponible en este contexto; "
                      "las banderas se deducen solo de la version");
        }
    }
};

} // namespace

std::unique_ptr<opt::Module> opt::makeGpuProbe() {
    return std::make_unique<GpuProbe>();
}
