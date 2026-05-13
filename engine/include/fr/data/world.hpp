/**
 * @file world.hpp
 * @author Kiju
 *
 * @brief World is the top-level container for all game data.
 */

#pragma once

#include "fr/core/alloc.hpp"
#include "fr/core/array.hpp"
#include "fr/core/bitset.hpp"
#include "fr/core/ctx.hpp"
#include "fr/core/inline_any.hpp"
#include "fr/core/macros.hpp"
#include "fr/core/typeidx.hpp"
#include "fr/data/part.hpp"
#include "fr/data/part_pool.hpp"
#include "fr/data/thing.hpp"
#include "fr/data/thing_pool.hpp"
#include "fr/data/typeidx.hpp"

namespace fr {
class World {
public:
    using AnyPartPool = InlineAny<sizeof(impl::PartPool<Byte>), alignof(impl::PartPool<Byte>)>;
    using PartPools = Array<AnyPartPool, max_parts>;

    World() noexcept
        : m_alloc(get_ambient_ctx().alloc) {
    }

    World(Alloc *alloc) noexcept
        : m_alloc(alloc) {
    }

    World(const World &) = delete;
    World &operator=(const World &) = delete;

    ~World() noexcept = default;

    /**
     * @brief Returns the allocator for this world.
     */
    [[nodiscard]] const Alloc *alloc() const noexcept {
        return m_alloc;
    }

    /**
     * @brief Returns the thing pool for this world.
     * @note The thing pool is owned and managed by the world, most of the time
     * you should not need to access it directly.
     */
    [[nodiscard]] const impl::ThingPool &thing_pool() const noexcept {
        return m_thing_pool;
    }

    /**
     * @brief Returns the part pools for this world.
     * @note The part pools are owned and managed by the world, most of the time
     * you should not need to access them directly.
     */
    [[nodiscard]] const PartPools &part_pools() const noexcept {
        return m_part_pools;
    }

    /**
     * @brief Hands out a thing from the game world.
     * @note The thing returned has no parts attached to it - except for the base parts.
     */
    [[nodiscard]] Thing handout_thing() noexcept {
        return m_thing_pool.handout();
    }

    /**
     * @brief Destroys a thing if it is valid.
     * @return If the thing was destroyed returns true, if not (the thing does not exist or is not
     * valid) returns false.
     */
    bool destroy_thing(Thing thing) noexcept {
        return m_thing_pool.destroy(thing);
    }

    /**
     * @brief Checks if a thing is valid.
     * @return Returns true if the thing is valid, false otherwise.
     */
    bool check_thing(Thing thing) const noexcept {
        return m_thing_pool.check(thing);
    }

    template <typename T>
    bool check_part(Thing thing) const noexcept {
        FR_ASSERT(check_thing(thing), "thing is not valid");

        TypeIdx tidx = impl::DataTypeIdxGen::gen<T>();
    }

    /**
     * @brief Emplaces a part of type T on the given thing, constructing it in-place with the given
     * arguments.
     * @return A reference to the attached part.
     */
    template <typename T, typename... Args>
    T &emplace_part(Thing thing, Args &&...args) noexcept {
        FR_ASSERT(check_thing(thing), "thing is not valid");
        // @todo Return a stub if the thing does not exist.

        TypeIdx tidx = impl::DataTypeIdxGen::template gen<T>();
        FR_ASSERT(tidx < max_parts, "part type exceeds max parts; increase max_parts");

        if (!m_part_pools_alive.check_bit(tidx)) {
            m_part_pools_alive.one_bit(tidx);
            m_part_pools[tidx] = AnyPartPool(impl::PartPool<T>(m_alloc));
        }

        auto pool = do_part_pool_by_tidx<T>(tidx);
        return pool.template emplace<T, Args...>(std::forward<Args>(args)...);
    }

    /**
     * @brief Attaches a part of type T to the given thing, copying the part from the given value.
     * @return A reference to the attached part.
     */
    template <typename T>
    T &attach_part(Thing thing, const T &part) noexcept {
        return emplace_part<T>(thing, std::forward<T>(part));
    }

    /**
     * @brief Attaches a part of type T to the given thing, moving the part from the given value.
     * @return A reference to the attached part.
     */
    template <typename T>
    T &attach_part(Thing thing, T &&part) noexcept {
        return emplace_part<T>(thing, std::forward<T>(part));
    }

    /**
     * @brief Detaches the part of type T from the given thing.
     */
    template <typename T>
    void detach_part(Thing thing) noexcept {
        FR_ASSERT(thing != Thing::nil(), "detach_part: thing is nil");
        auto pool = do_part_pool_by_tidx<T>(tidx_of<T>());
        pool.template detach<T>(thing);
    }

private:
    template <typename T>
    impl::PartPool<T> do_part_pool_by_tidx(TypeIdx tidx) noexcept {
        FR_ASSERT(tidx < max_parts, "part type exceeds max parts; increase max_parts");
        return m_part_pools[tidx].cast<impl::PartPool<T>>();
    }

    impl::ThingPool m_thing_pool{};

    PartPools m_part_pools{PartPools::from_repeated(AnyPartPool::nil())};
    Bitset<max_parts> m_part_pools_alive{Bitset<max_parts>::with_zeros()};

    Alloc *m_alloc{get_ambient_ctx().alloc};
};
} // namespace fr
