#include "engine/layout.h"

#include <sstream>

#include "core/constants.h"

namespace cue {
namespace {

char typeChar(BallType t) {
    switch (t) {
        case BallType::Cue:    return 'C';
        case BallType::Solid:  return 'S';
        case BallType::Stripe: return 'T';
        case BallType::Eight:  return 'E';
        case BallType::Object: return 'O';
    }
    return 'O';
}
BallType charType(char c) {
    switch (c) {
        case 'C': return BallType::Cue;
        case 'S': return BallType::Solid;
        case 'T': return BallType::Stripe;
        case 'E': return BallType::Eight;
        default:  return BallType::Object;
    }
}

}  // namespace

std::string saveLayout(const World& w) {
    std::ostringstream os;
    os << "# cuesearch layout: B id type x z\n";
    for (const Ball& b : w.balls) {
        if (b.pocketed) continue;
        os << "B " << b.id << ' ' << typeChar(b.type) << ' ' << b.r.x << ' '
           << b.r.z << '\n';
    }
    return os.str();
}

World loadLayout(const std::string& text) {
    World w;
    std::istringstream is(text);
    std::string line;
    while (std::getline(is, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream ls(line);
        std::string tag;
        ls >> tag;
        if (tag != "B") continue;
        Ball b;
        char tc = 'O';
        double x = 0.0, z = 0.0;
        ls >> b.id >> tc >> x >> z;
        b.type = charType(tc);
        b.r = {x, k::R, z};
        w.balls.push_back(b);
    }
    return w;
}

}  // namespace cue
