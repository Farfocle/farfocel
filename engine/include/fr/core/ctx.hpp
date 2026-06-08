/**
 * @file ctx.hpp
 * @author Kiju
 *
 * @brief Context is a mechanism of sharing common details about a module.
 */

#pragma once

#include "fr/core/alloc_typedefs.hpp"
#include "fr/core/macros.hpp"
#include "fr/core/typedefs.hpp"

namespace fr {

// -------------------------------------------------------- Forward Declarations
class Alloc;
class AllocTracer;
class TypeRegistry;
class Logger;

// ------------------------------------------------------------------ Ctx Struct
struct Ctx {
    const char *tag{"@noname"};
    Alloc *alloc{nullptr};
    AllocTracer *alloc_tracer{nullptr};
    OOMHandler oom_handler{nullptr};
    U8 oom_retries{2};
    TypeRegistry *type_registry{nullptr};
    Logger *logger;
};

// ------------------------------------------------------------- Global Pointers
namespace glob {
FR_API extern AllocTracer *core_alloc_tracer_ptr;
FR_API extern TypeRegistry *core_type_registry_ptr;
FR_API extern Alloc *core_heap_alloc_ptr;
FR_API extern Ctx *core_ctx_ptr;

FR_API extern thread_local Ctx *ambient_ctx_ptr;
FR_API extern Logger *logger_ptr;
} // namespace glob

// ------------------------------------------------------------------------- API

FR_API void init_core_ctx() noexcept;
FR_API void shutdown_core_ctx() noexcept;

FR_API const Ctx &get_ambient_ctx() noexcept;
FR_API Ctx &get_ambient_ctx_mut() noexcept;
FR_API void set_ambient_ctx(Ctx *ctx) noexcept;

/**
 * @brief RAII scope for switching ambient context.
 */
class CtxScope {
public:
    explicit CtxScope(Ctx *ctx) noexcept
        : m_prev(glob::ambient_ctx_ptr) {
        set_ambient_ctx(ctx);
    }

    ~CtxScope() noexcept {
        set_ambient_ctx(m_prev);
    }

    CtxScope(const CtxScope &) = delete;
    CtxScope &operator=(const CtxScope &) = delete;

private:
    Ctx *m_prev;
};

} // namespace fr
