#include <nix/nixexpr.hh>
#include <string>
#include <vector>
#include "common/analysis.h"

Analysis parse(nix::EvalState& state,
               std::string source,
               nix::Path path,
               nix::Path basePath,
               nix::Pos targetPos);