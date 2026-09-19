#include "core/Version.h"

#include <iostream>
#include <stdexcept>

int main() {
    if (openemperor::version::display != "0.1.0-alpha.1")
        throw std::runtime_error("unexpected alpha display version");
    if (openemperor::version::project != "0.1.0")
        throw std::runtime_error("CMake project version must remain numeric");
    if (openemperor::version::revision.empty() || openemperor::version::build_type.empty())
        throw std::runtime_error("build provenance is incomplete");
    std::cout << openemperor::version::display << " (" << openemperor::version::revision
              << (openemperor::version::dirty ? ", dirty" : ", clean") << ")\n";
}
