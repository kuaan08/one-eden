// SPDX-License-Identifier: GPL-3.0-or-later
// Cross-platform test for the Xbox UWP updater's version parser.
// Compile: c++ -std=c++20 tools/xbox/tests/update-check.cpp -o update-check
#include <cassert>
#include "../../../src/eden_uwp/update_checker.h"
int main() {
    using namespace EdenXbox::Updates;
    Version v{};
    assert(ParseVersion("0.2.88.36", v) && v == (Version{0,2,88,36}));
    assert(ParseTag("xbox-one-dev-0.2.88.37", v) && v == (Version{0,2,88,37}));
    assert(!ParseTag("xbox-one-dev-0.2.88.37-malware", v));
    assert(!ParseTag("other-0.2.88.37", v));
    assert(!ParseVersion("0.2.88.65536", v));
    assert(!ParseVersion("0.2.88", v));
    assert(!ParseVersion("0.2.88.1.2", v));
    assert(!ParseVersion("0.2.88.-1", v));
    assert(!ParseVersion("0.2.88.xyz", v));
    assert(Version({0,2,88,37}) > Version({0,2,88,36}));
    assert(!(Version({0,2,88,36}) > Version({0,2,88,36})));
    assert(Version({0,2,88,37}) > Version({0,2,87,100}));
}
