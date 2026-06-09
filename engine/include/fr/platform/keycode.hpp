/**
 * @file keycode.hpp
 * @author Tfoedy
 *
 * @brief keycodes for input handling.
 * * Values are mapped 1:1 with the SDL_Scancode  */

#pragma once

#include "fr/core/typedefs.hpp"

namespace fr {
enum class Key : U16 {
    Unknown = 0,

    A = 4,
    B = 5,
    C = 6,
    D = 7,
    E = 8,
    F = 9,
    G = 10,
    H = 11,
    I = 12,
    J = 13,
    K = 14,
    L = 15,
    M = 16,
    N = 17,
    O = 18,
    P = 19,
    Q = 20,
    R = 21,
    S = 22,
    T = 23,
    U = 24,
    V = 25,
    W = 26,
    X = 27,
    Y = 28,
    Z = 29,

    Num1 = 30,
    Num2 = 31,
    Num3 = 32,
    Num4 = 33,
    Num5 = 34,
    Num6 = 35,
    Num7 = 36,
    Num8 = 37,
    Num9 = 38,
    Num0 = 39,

    Return = 40,
    Escape = 41,
    Backspace = 42,
    Tab = 43,
    Space = 44,

    Right = 79,
    Left = 80,
    Down = 81,
    Up = 82,

    LCtrl = 224,
    LShift = 225,
    LAlt = 226,
    RCtrl = 228,
    RShift = 229,
    RAlt = 230,

    MaxKeys = 512
};

/**
 * @brief API-agnostic mouse button codes.
 */
enum class MouseButton : U8 { Left = 1, Middle = 2, Right = 3, MaxButtons = 8 };

} // namespace fr
