/**
 * @file imgui_archive.hpp
 * @author Kiju
 *
 * @brief ImGui archive implementing the shape protocol.
 *
 * @note Special display overrides (bypass the type's own shape method):
 *  - `Thing`                - single read-only line.
 *  - `Vec2`, `Vec3`, `Vec4` - inline X/Y/Z/W labeled DragFloat components on one row.
 *  - `DynamicArray<T>`      - flat expandable list of auto-indexed items
 *  - `Slice<T>`             - same as `DynamicArray`
 */

#pragma once

#include <cstdio>
#include <imgui.h>
#include <type_traits>

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

// ======================================================== Type detection helpers

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

// ================================================================ ImGuiWriterArchive

/**
 * @brief Archive that renders the shape tree as ImGui widgets.
 *
 * Labels appear on the LEFT of every widget row. Interactive widgets fill the remaining width.
 * Shapeable objects and collections open as collapsible TreeNodes.
 */
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

        // --------------------------------------------------------------- label
        char idx_label[16];
        const char *label;
        bool pushed_id = false;

        if (name.is_empty() && !m_list_indices.is_empty()) {
            const int idx = static_cast<int>(m_list_indices.back()++);
            ImGui::PushID(idx);
            pushed_id = true;
            snprintf(idx_label, sizeof(idx_label), "[%d]", idx);
            label = idx_label;
        } else {
            label = name.is_empty() ? "##anon" : name.data();
        }

        const bool do_push_id = !pushed_id && label[0] != '#';
        if (do_push_id) {
            ImGui::PushID(label);
        }

        // ------------------------------------------------------- Render Widget
        if constexpr (std::is_same_v<RawT, bool>) {
            do_text_label(label);
            ImGui::Checkbox("##v", &value);

        } else if constexpr (std::is_same_v<RawT, Byte>) {
            do_text_label(label);
            auto v = static_cast<U8>(value);

            if (ImGui::InputScalar("##v", ImGuiDataType_U8, &v)) {
                value = static_cast<Byte>(v);
            }
        } else if constexpr (std::is_same_v<RawT, U8>) {
            do_text_label(label);
            ImGui::InputScalar("##v", ImGuiDataType_U8, &value);
        } else if constexpr (std::is_same_v<RawT, U16>) {
            do_text_label(label);
            ImGui::InputScalar("##v", ImGuiDataType_U16, &value);
        } else if constexpr (std::is_same_v<RawT, U32>) {
            do_text_label(label);
            ImGui::InputScalar("##v", ImGuiDataType_U32, &value);
        } else if constexpr (std::is_same_v<RawT, U64>) {
            do_text_label(label);
            ImGui::InputScalar("##v", ImGuiDataType_U64, &value);
        } else if constexpr (std::is_same_v<RawT, S8>) {
            do_text_label(label);
            ImGui::InputScalar("##v", ImGuiDataType_S8, &value);
        } else if constexpr (std::is_same_v<RawT, S16>) {
            do_text_label(label);
            ImGui::InputScalar("##v", ImGuiDataType_S16, &value);
        } else if constexpr (std::is_same_v<RawT, S32>) {
            do_text_label(label);
            ImGui::InputScalar("##v", ImGuiDataType_S32, &value);
        } else if constexpr (std::is_same_v<RawT, S64>) {
            do_text_label(label);
            ImGui::InputScalar("##v", ImGuiDataType_S64, &value);
        } else if constexpr (std::is_same_v<RawT, F32>) {
            do_text_label(label);
            ImGui::DragFloat("##v", &value, 0.1f);
        } else if constexpr (std::is_same_v<RawT, F64>) {
            do_text_label(label);
            ImGui::InputDouble("##v", &value);

        } else if constexpr (std::is_same_v<RawT, String>) {
            do_text_label_inline(label);
            const StringView sv = value.view();
            ImGui::TextUnformatted(sv.data(), sv.data() + sv.size());
        } else if constexpr (std::is_same_v<RawT, StringView>) {
            do_text_label_inline(label);
            ImGui::TextUnformatted(value.data(), value.data() + value.size());
        } else if constexpr (std::is_same_v<RawT, const char *> || std::is_same_v<RawT, char *>) {
            do_text_label_inline(label);
            ImGui::TextUnformatted(value ? value : "(null)");

        } else if constexpr (std::is_same_v<RawT, Thing>) {
            do_text_label_inline(label);

            if (value.is_nil()) {
                ImGui::TextDisabled("nil");
            } else {
                ImGui::Text("#%u  gen:%u", value.idx(), value.gen());
            }

        } else if constexpr (std::is_same_v<RawT, Vec2>) {
            do_vec_label(label);
            do_vec_component("X", "##x", &value.x, false);
            ImGui::SameLine();
            do_vec_component("Y", "##y", &value.y, true);

        } else if constexpr (std::is_same_v<RawT, Vec3>) {
            do_vec_label(label);
            do_vec_component("X", "##x", &value.x, false);
            ImGui::SameLine();
            do_vec_component("Y", "##y", &value.y, false);
            ImGui::SameLine();
            do_vec_component("Z", "##z", &value.z, true);

        } else if constexpr (std::is_same_v<RawT, Vec4>) {
            do_vec_label(label);

            do_vec_component("X", "##x", &value.x, false);
            ImGui::SameLine();
            do_vec_component("Y", "##y", &value.y, false);
            ImGui::SameLine();
            do_vec_component("Z", "##z", &value.z, false);
            ImGui::SameLine();
            do_vec_component("W", "##w", &value.w, true);

        } else if constexpr (is_dynamic_array_v<RawT> || is_slice_v<RawT>) {
            // Flat list - no size/capacity metadata, only the items.
            if (ImGui::TreeNode(label)) {
                m_list_indices.push_back(0);

                for (auto &item : value) {
                    prop("", item);
                }

                m_list_indices.pop_back();
                ImGui::TreePop();
            }

        } else if constexpr (IsShape<ImGuiWriterArchive, V>) {
            if (ImGui::TreeNode(label)) {
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

        if (ImGui::TreeNode(label)) {
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
            const int idx = static_cast<int>(m_list_indices.back()++);
            ImGui::PushID(idx);
            pushed_id = true;
            snprintf(idx_label, sizeof(idx_label), "[%d]", idx);
            label = idx_label;
        } else {
            label = name.is_empty() ? "##dict" : name.data();
        }

        if (ImGui::TreeNode(label)) {
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

    static void do_text_label(const char *label) {
        if (label[0] != '#') {
            ImGui::AlignTextToFramePadding();
            ImGui::Text("%s", label);
            ImGui::SameLine();
        }

        ImGui::SetNextItemWidth(-FLT_MIN);
    }

    static void do_text_label_inline(const char *label) {
        if (label[0] == '#') {
            return;
        }

        ImGui::AlignTextToFramePadding();
        ImGui::Text("%s", label);
        ImGui::SameLine();
    }

    static void do_vec_label(const char *label) {
        if (label[0] == '#') {
            return;
        }

        ImGui::AlignTextToFramePadding();
        ImGui::Text("%s", label);
        ImGui::SameLine();
    }

    static void do_vec_component(const char *comp_label, const char *id, F32 *v, bool last) {
        ImGui::AlignTextToFramePadding();
        ImGui::Text("%s", comp_label);
        ImGui::SameLine();

        if (last) {
            ImGui::SetNextItemWidth(-FLT_MIN);
        } else {
            const F32 avail = ImGui::GetContentRegionAvail().x;
            const F32 letter_w = ImGui::CalcTextSize("X").x + ImGui::GetStyle().ItemSpacing.x;

            ImGui::SetNextItemWidth((avail - letter_w) * 0.48f);
        }

        ImGui::DragFloat(id, v, 0.1f);
    }
};

} // namespace fr
