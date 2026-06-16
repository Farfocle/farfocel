/**
 * @file imgui_archive.hpp
 * @author Kiju
 *
 * @brief ImGui archive implementing the shape protocol.
 */

#pragma once

#include <cstdio>
#include <imgui.h>
#include <type_traits>

#include <glm/gtc/quaternion.hpp>

#include "fr/core/dynamic_array.hpp"
#include "fr/core/macros.hpp"
#include "fr/core/math.hpp"
#include "fr/core/shape.hpp"
#include "fr/core/slice.hpp"
#include "fr/core/string.hpp"
#include "fr/core/string_view.hpp"
#include "fr/core/typedefs.hpp"
#include "fr/data/thing.hpp"

namespace fr {

// ====================================================== Type Detection Helpers

namespace impl {

template <typename T>
struct IsDynamicArrayImpl : std::false_type {};
template <typename T>
struct IsDynamicArrayImpl<DynamicArray<T>> : std::true_type {};

template <typename T>
struct IsSliceImpl : std::false_type {};
template <typename T>
struct IsSliceImpl<Slice<T>> : std::true_type {};

} // namespace impl

template <typename T>
inline constexpr bool is_dynamic_array_v = impl::IsDynamicArrayImpl<std::remove_cvref_t<T>>::value;

template <typename T>
inline constexpr bool is_slice_v = impl::IsSliceImpl<std::remove_cvref_t<T>>::value;

// ========================================================== ImGuiWriterArchive

/// @brief Archive that renders the shape tree as ImGui widgets.
class ImGuiWriterArchive {
public:
    struct Options {};

    static constexpr ArchiveAction action = ArchiveAction::Write;

    ImGuiWriterArchive() = default;
    ImGuiWriterArchive(const ImGuiWriterArchive &) = delete;
    ImGuiWriterArchive &operator=(const ImGuiWriterArchive &) = delete;
    ImGuiWriterArchive(ImGuiWriterArchive &&) = delete;
    ImGuiWriterArchive &operator=(ImGuiWriterArchive &&) = delete;

    template <typename V>
    void prop(StringView name, V &value) {
        using RawT = std::remove_cvref_t<V>;

        char idx_label[16];
        const char *label;
        bool pushed_id = false;

        if (name.is_empty() && !m_list_indices.is_empty()) {
            const S32 idx = static_cast<S32>(m_list_indices.back()++);
            ImGui::PushID(idx);
            pushed_id = true;

            std::snprintf(idx_label, sizeof(idx_label), "[%d]", idx);
            label = idx_label;
        } else {
            label = name.is_empty() ? "##anon" : name.data();
        }

        const bool do_push_id = !pushed_id && label[0] != '#';
        if (do_push_id) {
            ImGui::PushID(label);
        }

        if constexpr (std::is_same_v<RawT, bool>) {
            do_label(label);
            ImGui::Checkbox("##v", &value);

        } else if constexpr (std::is_same_v<RawT, Byte>) {
            do_label(label);
            U8 v = static_cast<U8>(value);

            if (ImGui::InputScalar("##v", ImGuiDataType_U8, &v)) {
                value = static_cast<Byte>(v);
            }
        } else if constexpr (std::is_same_v<RawT, U8>) {
            do_label(label);
            ImGui::InputScalar("##v", ImGuiDataType_U8, &value);
        } else if constexpr (std::is_same_v<RawT, U16>) {
            do_label(label);
            ImGui::InputScalar("##v", ImGuiDataType_U16, &value);
        } else if constexpr (std::is_same_v<RawT, U32>) {
            do_label(label);
            ImGui::InputScalar("##v", ImGuiDataType_U32, &value);
        } else if constexpr (std::is_same_v<RawT, U64>) {
            do_label(label);
            ImGui::InputScalar("##v", ImGuiDataType_U64, &value);
        } else if constexpr (std::is_same_v<RawT, S8>) {
            do_label(label);
            ImGui::InputScalar("##v", ImGuiDataType_S8, &value);
        } else if constexpr (std::is_same_v<RawT, S16>) {
            do_label(label);
            ImGui::InputScalar("##v", ImGuiDataType_S16, &value);
        } else if constexpr (std::is_same_v<RawT, S32>) {
            do_label(label);
            ImGui::InputScalar("##v", ImGuiDataType_S32, &value);
        } else if constexpr (std::is_same_v<RawT, S64>) {
            do_label(label);
            ImGui::InputScalar("##v", ImGuiDataType_S64, &value);
        } else if constexpr (std::is_same_v<RawT, F32>) {
            do_label(label);
            ImGui::DragFloat("##v", &value, 0.1f);
        } else if constexpr (std::is_same_v<RawT, F64>) {
            do_label(label);
            ImGui::InputDouble("##v", &value);

        } else if constexpr (std::is_same_v<RawT, String>) {
            do_inline_label(label);

            const StringView sv = value.view();
            ImGui::TextUnformatted(sv.data(), sv.data() + sv.size());
        } else if constexpr (std::is_same_v<RawT, StringView>) {
            do_inline_label(label);
            ImGui::TextUnformatted(value.data(), value.data() + value.size());
        } else if constexpr (std::is_same_v<RawT, const char *> || std::is_same_v<RawT, char *>) {
            do_inline_label(label);
            ImGui::TextUnformatted(value ? value : "(null)");

        } else if constexpr (std::is_same_v<RawT, Thing>) {
            do_inline_label(label);

            if (value.is_nil()) {
                ImGui::TextDisabled("nil");
            } else {
                ImGui::Text("#%u  gen:%u", value.idx(), value.gen());
            }

        } else if constexpr (std::is_same_v<RawT, Vec2>) {
            do_label(label);
            ImGui::DragFloat2("##v", &value.x, 0.1f);

        } else if constexpr (std::is_same_v<RawT, Vec3>) {
            do_label(label);
            ImGui::DragFloat3("##v", &value.x, 0.1f);

        } else if constexpr (std::is_same_v<RawT, Vec4>) {
            do_label(label);
            ImGui::DragFloat4("##v", &value.x, 0.1f);

        } else if constexpr (std::is_same_v<RawT, Quat>) {
            do_label(label);
            Vec3 euler = glm::degrees(glm::eulerAngles(value));
            if (ImGui::DragFloat3("##v", &euler.x, 0.5f)) {
                value = Quat(glm::radians(euler));
            }

        } else if constexpr (std::is_same_v<RawT, Mat4>) {
            if (do_tree(label)) {
                for (int row = 0; row < 4; ++row) {
                    ImGui::PushID(row);
                    char row_label[8];
                    std::snprintf(row_label, sizeof(row_label), "[%d]", row);

                    F32 vals[4] = {value[0][row], value[1][row], value[2][row], value[3][row]};
                    do_label(row_label);
                    ImGui::SetNextItemWidth(-FLT_MIN);

                    if (ImGui::DragFloat4("##r", vals, 0.01f)) {
                        value[0][row] = vals[0];
                        value[1][row] = vals[1];
                        value[2][row] = vals[2];
                        value[3][row] = vals[3];
                    }

                    ImGui::PopID();
                }
                ImGui::TreePop();
            }

        } else if constexpr (is_dynamic_array_v<RawT> || is_slice_v<RawT>) {
            if (do_tree(label)) {
                m_list_indices.push_back(0);

                for (auto &item : value) {
                    prop("", item);
                }

                m_list_indices.pop_back();
                ImGui::TreePop();
            }

        } else if constexpr (IsShape<ImGuiWriterArchive, V>) {
            if (do_tree(label)) {
                call_shape<ImGuiWriterArchive, V>(*this, value);
                ImGui::TreePop();
            }
        } else {
            FR_STATIC_ASSERT(false, "object does not implement the shape protocol");
        }

        if (pushed_id || do_push_id) {
            ImGui::PopID();
        }
    }

    template <typename Fn>
    void list(StringView name, Fn &&fn) {
        const char *label = name.is_empty() ? "##list" : name.data();
        if (do_tree(label)) {
            m_list_indices.push_back(0);
            std::forward<Fn>(fn)(*this);

            m_list_indices.pop_back();
            ImGui::TreePop();
        }
    }

    template <typename Fn>
    void dict(StringView name, Fn &&fn) {
        char idx_label[16];
        const char *label;
        bool pushed_id = false;

        if (name.is_empty() && !m_list_indices.is_empty()) {
            const S32 idx = static_cast<S32>(m_list_indices.back()++);
            ImGui::PushID(idx);
            pushed_id = true;

            std::snprintf(idx_label, sizeof(idx_label), "[%d]", idx);
            label = idx_label;
        } else {
            label = name.is_empty() ? "##dict" : name.data();
        }

        if (do_tree(label)) {
            std::forward<Fn>(fn)(*this);
            ImGui::TreePop();
        }

        if (pushed_id) {
            ImGui::PopID();
        }
    }

    /// @brief Always returns 0 - writers do not expose list sizes.
    USize current_list_size() const {
        return 0;
    }

private:
    DynamicArray<USize> m_list_indices{};

    /// @brief Displays `label` then sets up the next item to fill the remaining width.
    static void do_label(const char *label) {
        if (label[0] != '#') {
            ImGui::AlignTextToFramePadding();
            ImGui::Text("%s", label);
            ImGui::SameLine();
        }

        ImGui::SetNextItemWidth(-FLT_MIN);
    }

    /// @brief Displays `label` inline (no width setup - used for text-only values).
    static void do_inline_label(const char *label) {
        if (label[0] == '#') {
            return;
        }
        ImGui::AlignTextToFramePadding();
        ImGui::Text("%s", label);
        ImGui::SameLine();
    }

    /// @brief Opens a collapsible tree node. Returns true if it should be expanded.
    static bool do_tree(const char *label) {
        return ImGui::TreeNode(label[0] == '#' ? "##node" : label);
    }
};

} // namespace fr
