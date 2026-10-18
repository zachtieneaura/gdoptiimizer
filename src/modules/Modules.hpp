#pragma once
#include "../core/Module.hpp"
#include <memory>

namespace opt {

std::unique_ptr<Module> makeFpsLimiter();
std::unique_ptr<Module> makeTextureTrim();
std::unique_ptr<Module> makeFrameProfiler();

} // namespace opt
