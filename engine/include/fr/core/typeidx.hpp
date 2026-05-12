/**
 * @file typeidx.hpp
 * @author Kiju
 *
 * @brief TypeIdx is a mechanism for generating monotonic type indexes unique per translation unit.
 */

#include "fr/core/typedefs.hpp"

namespace fr {
using TypeIdx = U32;

/**
 * @brief Generates monotonic type indexes unique per translation unit. Indexes are U32.
 * @warning Not thread safe.
 */
class TypeIdxGen {
public:
    /**
     * @brief Generates TypeIdx.
     * @return Unique monotonic type idx.
     * @warning Not thread safe.
     */
    template <typename T>
    static TypeIdx gen() {
        static TypeIdx idx = gen_next();
        return idx;
    }

private:
    static TypeIdx gen_next() {
        static TypeIdx counter = 0;
        return counter++;
    }
};
} // namespace fr
