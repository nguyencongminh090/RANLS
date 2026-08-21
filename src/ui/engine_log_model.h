#pragma once

#include <cstddef>
#include <deque>
#include <string>

/// Direction/category of an engine log line. Mirrors the color-tag scheme in
/// bottom_panel.cpp (tagSend_ / tagRecvOutput_ / tagRecvMessage_ / tagRecvCoord_ /
/// tagRecvError_) without depending on any GTK type, so this header can be used
/// from a pure-logic unit test with no display server.
enum class LogTagKind {
    Send,
    RecvOutput,
    RecvMessage,
    RecvCoord,
    RecvError,
};

/// One logical engine-log line: a direction/category prefix (e.g. "[SEND]",
/// "[RECV]") plus the raw text.
struct EngineLogLine {
    std::string prefix;
    std::string text;
    LogTagKind  tag;
};

/// Bounded, GUI-toolkit-independent store for engine log lines.
///
/// Enforces a hard cap on retained lines: pushing past the cap silently drops
/// the oldest line(s). This is the single source of truth for "how many lines
/// are logically kept"; the GTK-facing view (BottomPanel) mirrors whatever
/// this model reports so the on-screen buffer and the model never disagree
/// about content or count.
class EngineLogModel {
public:
    explicit EngineLogModel(std::size_t maxLines = 5000) : maxLines_(maxLines) {}

    /// Appends a line, dropping oldest lines if over the cap.
    /// Returns how many lines were dropped as a result of this push (0 or 1
    /// in normal use, since one push can push the count over the cap by at
    /// most one; the loop is defensive against maxLines being lowered live).
    std::size_t push(EngineLogLine line)
    {
        lines_.push_back(std::move(line));
        std::size_t dropped = 0;
        while (lines_.size() > maxLines_) {
            lines_.pop_front();
            ++dropped;
        }
        return dropped;
    }

    void clear() { lines_.clear(); }

    std::size_t maxLines() const { return maxLines_; }
    void setMaxLines(std::size_t maxLines)
    {
        maxLines_ = maxLines;
        while (lines_.size() > maxLines_)
            lines_.pop_front();
    }

    std::size_t size() const { return lines_.size(); }
    bool empty() const { return lines_.empty(); }

    const std::deque<EngineLogLine> &lines() const { return lines_; }

private:
    std::deque<EngineLogLine> lines_;
    std::size_t                maxLines_;
};
