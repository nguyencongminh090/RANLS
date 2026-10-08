#pragma once

// PAL-03: declarative table of every control in the Settings dialog. Single
// source for (a) the label the dialog shows, (b) the bilingual title/keywords
// the Ctrl+K command palette (PAL-01/02) searches, and (c) which notebook tab
// "open this setting" jumps to. No GTK types, so the table is unit-testable
// without a display server (tests/test_pal03_settings_registry.cpp).
//
// Adding a setting to SettingsDialog without a row here is caught at
// construction time (addRow looks the id up) and by the widget test.

#include "i18n/i18n.h"

#include <array>
#include <cstddef>
#include <string_view>

namespace settings_registry {

struct SettingEntry {
    std::string_view id;          ///< Palette id, e.g. "set.threads" (matches tests/data/palette/entries.tsv).
    int              tab;         ///< Notebook page index: 0 Engine, 1 Time, 2 Search, 3 UI, 4 Hotkeys.
    std::string_view label;       ///< Text shown in the dialog (unchanged from before PAL-03).
    std::string_view titleEn;     ///< Palette title (English) = catalog key, ctx "palette".
    // (I18N-04: the Vietnamese title lives in vi.tsv, ctx "palette".)
    std::string_view keywordsEn;  ///< ';'-separated search keywords.
    std::string_view keywordsVi;
};

inline constexpr std::array<std::string_view, 5> kTabs = {"Engine", "Time", "Search", "UI", "Hotkeys"};

inline constexpr std::array kSettings = std::to_array<SettingEntry>({
    {"set.engine-path", 0, "Engine Path", i18n::trcNoop("palette", "Engine Path"),
     "executable;binary;rapfi;yixin;exe", "file chạy;chương trình;đặt engine"},
    {"set.ptc", 0, "Protocol Extension (.ptc)", i18n::trcNoop("palette", "Protocol Extension (.ptc)"),
     "extension;plugin;commands", "tiện ích;lệnh mở rộng"},
    {"set.threads", 0, "Threads", i18n::trcNoop("palette", "Threads"),
     "cpu;cores;parallel;processors", "nhân;đa luồng;bộ xử lý"},
    {"set.hash", 0, "Hash Size (MB)", i18n::trcNoop("palette", "Hash Size (MB)"),
     "memory;transposition table;ram", "bộ nhớ;bảng chuyển vị"},
    {"set.timeout-turn", 1, "Timeout / Turn (ms)", i18n::trcNoop("palette", "Timeout / Turn (ms)"),
     "time limit;move time;seconds", "giới hạn;giây;suy nghĩ"},
    {"set.timeout-match", 1, "Timeout / Match (ms)", i18n::trcNoop("palette", "Timeout / Match (ms)"),
     "clock;game time;total", "đồng hồ;cả ván"},
    {"set.increment", 1, "Increment (ms)", i18n::trcNoop("palette", "Increment (ms)"),
     "fischer;bonus time", "thời gian cộng;thêm giờ"},
    {"set.max-depth", 2, "Max Depth", i18n::trcNoop("palette", "Max Depth"),
     "plies;search depth;deeper", "độ sâu;số tầng"},
    {"set.max-nodes", 2, "Max Nodes", i18n::trcNoop("palette", "Max Nodes"),
     "node budget;limit search", "giới hạn nút"},
    {"set.multipv", 2, "Multi PV", i18n::trcNoop("palette", "Multi PV"),
     "principal variations;lines;candidates", "số biến;nhiều đường"},
    {"set.analysis-detail", 2, "Analysis Detail", i18n::trcNoop("palette", "Analysis Detail"),
     "show detail;verbosity;info", "chi tiết;mức độ"},
    {"set.theme", 3, "Theme", i18n::trcNoop("palette", "Theme"),
     "dark;light;colours;appearance", "tối;sáng;màu sắc;chủ đề"},
    {"set.language", 3, "Language", i18n::trcNoop("palette", "Language"),
     "locale;interface language;english;vietnamese;translation", "ngôn ngữ giao diện;tiếng việt;tiếng anh;dịch"},
    {"set.wingraph-mode", 3, "WinGraph Mode", i18n::trcNoop("palette", "WinGraph Mode"),
     "graph;chart;winrate graph;plot", "đồ thị;biểu đồ"},
    {"set.move-numbers", 3, "Show Move Numbers", i18n::trcNoop("palette", "Show Move Numbers"),
     "ordinal;stone numbers;digits", "số nước;đánh số"},
    {"set.coordinates", 3, "Show Coordinates", i18n::trcNoop("palette", "Show Coordinates"),
     "labels;letters;rows;columns", "nhãn;hàng cột;chữ cái"},
    {"set.hotkey-analyze", 4, "Analyze", i18n::trcNoop("palette", "Hotkey: Analyze"),
     "shortcut;keybinding;key", "bàn phím;tổ hợp phím"},
    {"set.hotkey-stop", 4, "Stop", i18n::trcNoop("palette", "Hotkey: Stop"),
     "shortcut;keybinding;key", "bàn phím;tổ hợp phím"},
    {"set.hotkey-undo", 4, "Undo", i18n::trcNoop("palette", "Hotkey: Undo"),
     "shortcut;keybinding;key", "bàn phím;tổ hợp phím"},
    {"set.hotkey-redo", 4, "Redo", i18n::trcNoop("palette", "Hotkey: Redo"),
     "shortcut;keybinding;key", "bàn phím;tổ hợp phím"},
    {"set.hotkey-newgame", 4, "New Game", i18n::trcNoop("palette", "Hotkey: New Game"),
     "shortcut;keybinding;key", "bàn phím;tổ hợp phím"},
});

/// Entry for `id`, or nullptr.
inline const SettingEntry *find(std::string_view id)
{
    for (const auto &e : kSettings)
        if (e.id == id)
            return &e;
    return nullptr;
}

}  // namespace settings_registry
