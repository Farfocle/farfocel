/**
 * @file math.hpp
 * @author Kiju
 *
 * @brief Utility helpers.
 */

#pragma once

#include <bit>

#include "fr/core/typedefs.hpp"
#include "glm/ext/matrix_float2x2.hpp"
#include "glm/ext/matrix_float3x3.hpp"
#include "glm/ext/matrix_float4x4.hpp"
#include "glm/ext/quaternion_float.hpp"
#include "glm/ext/vector_float2.hpp"
#include "glm/ext/vector_float3.hpp"
#include "glm/ext/vector_float4.hpp"
#include "glm/ext/vector_int2.hpp"
#include "glm/ext/vector_int3.hpp"
#include "glm/ext/vector_int4.hpp"
#include "glm/ext/vector_uint2.hpp"
#include "glm/ext/vector_uint3.hpp"
#include "glm/ext/vector_uint4.hpp"

namespace fr::math {

// karol: added min and max functions
template <typename T>
constexpr const T &min(const T &a, const T &b) noexcept {
    return (b < a) ? b : a;
}

template <typename T>
constexpr const T &max(const T &a, const T &b) noexcept {
    return (a < b) ? b : a;
}
/**
 * @brief Check whether @p n is a power of two.
 * @param n Value to test.
 * @return True if @p n is a power of two, false otherwise.
 */
inline bool is_pow2(USize n) noexcept {
    return n && ((n & (n - 1)) == 0);
}

/**
 * @brief Round @p n up to the next power of two.
 * @param n Value to round.
 * @return Smallest power of two greater than or equal to @p n.
 * @note Returns 1 when @p n is 0.
 */
inline USize round_up_pow2(USize n) noexcept {
    if (n == 0) {
        return 1;
    }

    if (is_pow2(n)) {
        return n;
    }

    return USize{1} << std::bit_width(n - 1);
}

/**
 * @brief Round @p n up to the next multiple of @p multiple.
 * @param n Value to round.
 * @param multiple Multiple to round up to.
 * @return Smallest multiple of @p multiple greater than or equal to @p n.
 */
inline USize round_up_to_multiple_of(USize n, USize multiple) noexcept {
    return (n + multiple - 1) & ~(multiple - 1);
}
} // namespace fr::math

namespace fr {

// --------------------------------------------------------------- Float Vectors
using Vec2 = glm::vec2;
using Vec3 = glm::vec3;
using Vec4 = glm::vec4;

// ----------------------------------------------------------------- Int Vectors
using IVec2 = glm::ivec2;
using IVec3 = glm::ivec3;
using IVec4 = glm::ivec4;

// ---------------------------------------------------------------- Uint Vectors
using UVec2 = glm::uvec2;
using UVec3 = glm::uvec3;
using UVec4 = glm::uvec4;

// -------------------------------------------------------------------- Matrices
using Mat2 = glm::mat2;
using Mat3 = glm::mat3;
using Mat4 = glm::mat4;

// ------------------------------------------------------------------ Quaternion
using Quat = glm::quat;

// ------------------------------------------------------------- Shape Protocols

template <typename A>
void shape(A &archive, Vec2 &v) {
    archive.prop("x", v.x);
    archive.prop("y", v.y);
}
template <typename A>
void shape(A &archive, Vec3 &v) {
    archive.prop("x", v.x);
    archive.prop("y", v.y);
    archive.prop("z", v.z);
}
template <typename A>
void shape(A &archive, Vec4 &v) {
    archive.prop("x", v.x);
    archive.prop("y", v.y);
    archive.prop("z", v.z);
    archive.prop("w", v.w);
}

template <typename A>
void shape(A &archive, IVec2 &v) {
    archive.prop("x", v.x);
    archive.prop("y", v.y);
}
template <typename A>
void shape(A &archive, IVec3 &v) {
    archive.prop("x", v.x);
    archive.prop("y", v.y);
    archive.prop("z", v.z);
}
template <typename A>
void shape(A &archive, IVec4 &v) {
    archive.prop("x", v.x);
    archive.prop("y", v.y);
    archive.prop("z", v.z);
    archive.prop("w", v.w);
}

template <typename A>
void shape(A &archive, UVec2 &v) {
    archive.prop("x", v.x);
    archive.prop("y", v.y);
}
template <typename A>
void shape(A &archive, UVec3 &v) {
    archive.prop("x", v.x);
    archive.prop("y", v.y);
    archive.prop("z", v.z);
}
template <typename A>
void shape(A &archive, UVec4 &v) {
    archive.prop("x", v.x);
    archive.prop("y", v.y);
    archive.prop("z", v.z);
    archive.prop("w", v.w);
}

// Matrices are serialized column-by-column (glm is column-major).
template <typename A>
void shape(A &archive, Mat2 &m) {
    archive.prop("c0", m[0]);
    archive.prop("c1", m[1]);
}
template <typename A>
void shape(A &archive, Mat3 &m) {
    archive.prop("c0", m[0]);
    archive.prop("c1", m[1]);
    archive.prop("c2", m[2]);
}
template <typename A>
void shape(A &archive, Mat4 &m) {
    archive.prop("c0", m[0]);
    archive.prop("c1", m[1]);
    archive.prop("c2", m[2]);
    archive.prop("c3", m[3]);
}

// Quaternion stored as (x, y, z, w) — the standard interop order.
template <typename A>
void shape(A &archive, Quat &q) {
    archive.prop("x", q.x);
    archive.prop("y", q.y);
    archive.prop("z", q.z);
    archive.prop("w", q.w);
}

} // namespace fr
