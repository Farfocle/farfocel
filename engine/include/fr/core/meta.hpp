/**
 * @file meta.hpp
 * @author Kiju
 *
 * @brief Meta serves a purpose of a reflection-like system metadata for dynamically managed and
 * type-erased objects.
 */

#pragma once

#include "fr/core/typedefs.hpp"
#include "fr/core/typeidx.hpp"

namespace fr {
enum class MetaDispatchAction : U8 { DefaultConstruct, Copy, Move, Destroy };

class Meta {
private:
    TypeIdx tidx{0};
    USize size{0};
    USize alignment{0};
    bool (*call_dispatch)(MetaDispatchAction, Byte *, Byte *){nullptr};
bool()

    public : static Meta nil() noexcept {
        return Meta{};
    }

    bool is_nil() const noexcept {
        return call_dispatch == nullptr;
    }
};
} // namespace fr
