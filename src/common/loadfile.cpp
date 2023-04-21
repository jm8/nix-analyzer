#include "loadfile.h"
#include "common/logging.h"

nix::Value* loadFile(nix::EvalState& state, std::string path) {
    auto v = state.allocValue();
    try {
        state.evalFile("/home/josh/dev/nix-analyzer/src/" + path, *v);
    } catch (nix::Error& e) {
        REPORT_ERROR(e);
        v->mkNull();
    }
    return v;
}
