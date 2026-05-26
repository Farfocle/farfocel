/**
 * @file command_pool.hpp
 * @author Jakub Kijek
 *
 * @brief Command pool for storing commands. Lazy world modifictions. This implementation is stupid
 * af but eh.
 */

#include "fr/core/alloc.hpp"
#include "fr/core/ctx.hpp"
#include "fr/core/dynamic_array.hpp"
#include "fr/core/inline_any.hpp"
#include "fr/core/typedefs.hpp"
#include "fr/core/typeidx.hpp"
#include "fr/data/part.hpp"
#include "fr/data/thing.hpp"
#include "fr/data/typeidx.hpp"

namespace fr {

enum class CmdKind : U8 { InsertPart, DestroyPart, MutatePart };

struct DestroyPartCmd {
    Thing thing{};
    TypeIdx tidx{};
};

template <typename T>
struct InsertPartCmd {
    Thing thing{};
    T part{};
};

template <typename T>
struct MutatePartCmd {
    Thing thing{};
    T prev_part{};
    T next_part{};
};

namespace impl {

class CmdPool {
public:
    using AnyCmdArray = InlineAny<sizeof(DynamicArray<Byte>), alignof(DynamicArray<Byte>)>;

    CmdPool() noexcept
        : CmdPool(get_ambient_ctx().alloc) {
    }

    explicit CmdPool(Alloc *alloc) noexcept
        : m_alloc(alloc) {
        m_destroy_part_array = DynamicArray<DestroyPartCmd>::with_alloc(alloc);
    }

    void flush() noexcept {
        m_destroy_part_array.clear();

        for (USize i = 0; i < MAX_PARTS; ++i) {
            m_insert_part_pool[i].clear();
            m_mutate_part_pool[i].clear();
        }
    }

private:
    template <typename T>
    void do_create_insert_part_pool(TypeIdx tidx) noexcept {
        m_insert_part_pool[tidx].emplace<DynamicArray<InsertPartCmd<T>>>(m_alloc);
    }

    bool do_check_insert_part_pool(TypeIdx tidx) const noexcept {
        return m_insert_part_pool[tidx].is_nil();
    }

    template <typename T>
    void do_create_mutate_part_pool(TypeIdx tidx) noexcept {
        m_mutate_part_pool[tidx].emplace<DynamicArray<MutatePartCmd<T>>>(m_alloc);
    }

    bool do_check_mutate_part_pool(TypeIdx tidx) const noexcept {
        return m_mutate_part_pool[tidx].is_nil();
    }

    template <typename T>
    TypeIdx do_gen_tidx() const noexcept {
        TypeIdx tidx = DataTypeIdxGen::gen<T>();
        FR_ASSERT(tidx < MAX_PARTS, "type index exceeds MAX_PARTS");
        return tidx;
    };

    Alloc *m_alloc{nullptr};
    DynamicArray<DestroyPartCmd> m_destroy_part_array{};
    Array<AnyCmdArray, MAX_PARTS> m_insert_part_pool{};
    Array<AnyCmdArray, MAX_PARTS> m_mutate_part_pool{};
};
} // namespace impl
} // namespace fr
