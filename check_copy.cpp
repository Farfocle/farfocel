#include "fr/data/thing_pool.hpp"
#include <utility>

int main() {
    fr::impl::ThingPool p1;
    fr::impl::ThingPool p2 = std::move(p1);
    return 0;
}
