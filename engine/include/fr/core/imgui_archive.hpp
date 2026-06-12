/**
 * @file imgui_archive.hpp
 * @author Kiju
 *
 * @brief ImGui archive implementing the shape protocol.
 *
 * @details ImGuiWriterArchive traverses the shape tree and renders ImGui widgets for each
 * property. Primitive types map to appropriate input widgets; nested shapeable types are rendered
 * inside collapsible TreeNodes. Unnamed items inside list contexts are auto-indexed.
 */

#pragma once

#include <cstdio>
#include <imgui.h>
#include <type_traits>

#include "fr/core/dynamic_array.hpp"
#include "fr/core/macros.hpp"
#include "fr/core/shape.hpp"
#include "fr/core/string.hpp"
#include "fr/core/string_view.hpp"
#include "fr/core/typedefs.hpp"

namespace fr {

/**
 * @brief Archive that renders the shape tree as ImGui widgets.
 *
 * Implements the Archive concept (ArchiveAction::Write). Each call to @c prop renders an
 * appropriate ImGui widget; @c list and @c dict open collapsible TreeNodes. Primitive edits
 * propagate back to the object through the reference passed to @c prop.
 *
 * @note All property names passed via @c FR_PROP / @c archive.prop must be null-terminated string
 * literals (guaranteed by the macro expansion).
 *
 * Example:
 * @code
 * ImGuiWriterArchive ar;
 * ar.prop("transform", my_transform);
 * @endcode
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

    /**
     * @brief Render a named property as an ImGui widget.
     *
     * Primitive types map to the most appropriate input widget. Types implementing the shape
     * protocol are rendered recursively inside a TreeNode. String types are displayed read-only
     * via @c LabelText.
     *
     * @param name Property name (used as widget label). Empty names are auto-indexed when inside
     *             a list context.
     * @param value Reference to the value — edits propagate back to the caller's object.
     */
    template <typename V>
    void prop(StringView name, V &value) {
        using RawT = std::remove_cvref_t<V>;

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
            label = name.is_empty() ? "##item" : name.data();
        }

        if constexpr (std::is_same_v<RawT, bool>) {
            ImGui::Checkbox(label, &value);
        } else if constexpr (std::is_same_v<RawT, Byte>) {
            auto v = static_cast<U8>(value);
            if (ImGui::InputScalar(label, ImGuiDataType_U8, &v)) {
                value = static_cast<Byte>(v);
            }
        } else if constexpr (std::is_same_v<RawT, U8>) {
            ImGui::InputScalar(label, ImGuiDataType_U8, &value);
        } else if constexpr (std::is_same_v<RawT, U16>) {
            ImGui::InputScalar(label, ImGuiDataType_U16, &value);
        } else if constexpr (std::is_same_v<RawT, U32>) {
            ImGui::InputScalar(label, ImGuiDataType_U32, &value);
        } else if constexpr (std::is_same_v<RawT, U64>) {
            ImGui::InputScalar(label, ImGuiDataType_U64, &value);
        } else if constexpr (std::is_same_v<RawT, S8>) {
            ImGui::InputScalar(label, ImGuiDataType_S8, &value);
        } else if constexpr (std::is_same_v<RawT, S16>) {
            ImGui::InputScalar(label, ImGuiDataType_S16, &value);
        } else if constexpr (std::is_same_v<RawT, S32>) {
            ImGui::InputScalar(label, ImGuiDataType_S32, &value);
        } else if constexpr (std::is_same_v<RawT, S64>) {
            ImGui::InputScalar(label, ImGuiDataType_S64, &value);
        } else if constexpr (std::is_same_v<RawT, F32>) {
            ImGui::DragFloat(label, &value);
        } else if constexpr (std::is_same_v<RawT, F64>) {
            ImGui::InputDouble(label, &value);
        } else if constexpr (std::is_same_v<RawT, String>) {
            const StringView sv = value.view();
            ImGui::LabelText(label, "%.*s", static_cast<int>(sv.size()), sv.data());
        } else if constexpr (std::is_same_v<RawT, StringView>) {
            ImGui::LabelText(label, "%.*s", static_cast<int>(value.size()), value.data());
        } else if constexpr (std::is_same_v<RawT, const char *> || std::is_same_v<RawT, char *>) {
            ImGui::LabelText(label, "%s", value ? value : "(null)");
        } else if constexpr (IsShape<ImGuiWriterArchive, V>) {
            if (ImGui::TreeNode(label)) {
                call_shape<ImGuiWriterArchive, V>(*this, value);
                ImGui::TreePop();
            }
        } else {
            FR_STATIC_ASSERT(false, "Object does not implement the shape protocol");
        }

        if (pushed_id) {
            ImGui::PopID();
        }
    }

    /**
     * @brief Render a named list as a collapsible TreeNode, then invoke @p fn inside it.
     *
     * Inside @p fn, unnamed @c prop calls are auto-indexed starting from 0.
     *
     * @param name Label for the list TreeNode.
     * @param fn   Callback that populates the list entries via this archive.
     */
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

    /**
     * @brief Render a named dict (object) as a collapsible TreeNode, then invoke @p fn inside it.
     *
     * Empty names are auto-indexed when inside a list context, producing labels like `[0]`, `[1]`.
     *
     * @param name Label for the dict TreeNode (may be empty when inside a list).
     * @param fn   Callback that populates the dict entries via this archive.
     */
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

    /// @brief Always returns 0 — writers do not expose list sizes.
    USize current_list_size() const {
        return 0;
    }

private:
    /// @brief Per-active-list item counter stack for auto-indexing unnamed props/dicts.
    DynamicArray<USize> m_list_indices{};
};

} // namespace fr
