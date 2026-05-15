#pragma once
// Real-layout I/O: enter a table position from your own game (the
// improvement loop the project is for). Dependency-free line format:
//   B <id> <C|S|T|E|O> <x> <z>     # one ball; type Cue/Solid/Stripe/Eight/Obj
// Lines starting with '#' or blank are ignored.
#include <string>

#include "engine/world.h"

namespace cue {

std::string saveLayout(const World& w);
World loadLayout(const std::string& text);

}  // namespace cue
