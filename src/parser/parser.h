#pragma once
#include "na_config.h"
#include <nix/nixexpr.hh>
#include <memory>
#include <string>
#include <vector>
#include "documents/documents.h"
#include "position/position.h"

Document parse(
    nix::EvalState& state,
    nix::SourcePath path,
    nix::SourcePath basePath,
    std::string_view source
);