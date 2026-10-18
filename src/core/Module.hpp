#pragma once
#include <memory>
#include <vector>

namespace opt {

// Cada optimización es un módulo independiente con su interruptor en mod.json.
// Para añadir uno nuevo:
//   1. Crea src/modules/MiModulo.cpp con una clase que herede de Module.
//   2. Declara su fábrica en modules/Modules.hpp.
//   3. Regístralo en registerAllModules() (main.cpp).
//   4. Añade sus ajustes en mod.json.
class Module {
public:
    virtual ~Module() = default;
    virtual char const* name() const = 0;

    // Se llama unas 4 veces por segundo. NUNCA hagas trabajo pesado aquí
    // ni pongas lógica por-frame: para eso usa un hook mínimo.
    virtual void onTick(float dt) = 0;
};

class Registry {
public:
    static Registry& get() {
        static Registry instance;
        return instance;
    }

    void add(std::unique_ptr<Module> module) {
        m_modules.push_back(std::move(module));
    }

    void tickAll(float dt) {
        for (auto& module : m_modules) {
            module->onTick(dt);
        }
    }

private:
    std::vector<std::unique_ptr<Module>> m_modules;
};

} // namespace opt
