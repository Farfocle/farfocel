/**
 * @file cmd.cpp
 * @author Kiju
 *
 * @brief CmdBatch relation-commit implementations.
 * @details `commit_attach_child_all` and `commit_detach_child_all` live here instead of in the
 * header because they need a complete `World`, and `world.hpp` includes `cmd.hpp` - putting them
 * here breaks the cycle.
 */

#include "fr/data/cmd.hpp"
#include "fr/data/world.hpp"

namespace fr {

DetachChildCmd AttachChildCmd::inverse() const noexcept {
    return {parent, child};
}

AttachChildCmd DetachChildCmd::inverse() const noexcept {
    return {parent, child};
}

void CmdBatch::commit_attach_child_all(World *world) noexcept {
    for (const impl::RawCmd &cmd : m_cmds) {
        if (cmd.kind != CmdKind::AttachChild) {
            continue;
        }

        world->attach_child_now(cmd.attach_child.parent, cmd.attach_child.child);
    }
}

void CmdBatch::commit_detach_child_all(World *world) noexcept {
    for (const impl::RawCmd &cmd : m_cmds) {
        if (cmd.kind != CmdKind::DetachChild) {
            continue;
        }

        world->detach_child_now(cmd.detach_child.parent, cmd.detach_child.child);
    }
}

} // namespace fr
