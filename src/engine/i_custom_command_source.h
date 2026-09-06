#pragma once

// PROTO-03: the custom-command SEND path.
//
// Hard constraint (features/protocol-extension/user_story.md): this is a
// SEPARATE, minimal interface — NOT a method bolted onto IEngineProtocol.
// GomocupProtocol implements it; EngineController only treats a protocol as a
// custom-command source once a `.ptc` table has actually been loaded (see
// EngineController::loadExtensionTable). A `.ptc`-less run never registers an
// extension command and never calls anything here, so its behaviour stays
// byte-identical to before this feature existed.

#include "model/board_state.h" // Coord
#include "protocol_extension.h"

#include <sigc++/sigc++.h>
#include <string>
#include <vector>

class ICustomCommandSource {
public:
    virtual ~ICustomCommandSource() = default;

    /// Names of every loaded extension command (empty when no `.ptc` is loaded).
    virtual std::vector<std::string> customCommandNames() const = 0;

    /// The declared help group of one extension command (Q9 — informational
    /// classification only; never wired to any EngineState transition).
    virtual std::string customCommandGroup(const std::string &name) const = 0;

    /// Build the wire line(s) for `name` given console args and the live game
    /// path (index 0 = first move). The implementation derives each item's
    /// x/y/color (color alternates by index, same formula as
    /// GomocupProtocol::generateAnalyzeRequest). `path` is read-only — nothing
    /// on this path mutates game state. Returns an empty vector on any error
    /// (which is also surfaced via signal_log).
    virtual std::vector<std::string> generateCustom(
        const std::string              &name,
        const std::vector<std::string> &args,
        const std::vector<Coord>       &path) = 0;

    /// Emitted when a matched `on_reply` action fires a whitelisted sink.
    sigc::signal<void(const protoext::SinkAction &)> signal_custom_action;
};
