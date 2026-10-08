#pragma once

// PAL-03: declarative table of every control in the Settings dialog. Single
// source for (a) the label the dialog shows, (b) the bilingual title/keywords
// the Ctrl+K command palette (PAL-01/02) searches, and (c) which notebook tab
// "open this setting" jumps to. No GTK types, so the table is unit-testable
// without a display server (tests/test_pal03_settings_registry.cpp).
//
// Adding a setting to SettingsDialog without a row here is caught at
// construction time (addRow looks the id up) and by the widget test.

#include <array>
#include <cstddef>
#include <string_view>

namespace settings_registry {

struct SettingEntry {
    std::string_view id;          ///< Palette id, e.g. "set.threads" (matches tests/data/palette/entries.tsv).
    int              tab;         ///< Notebook page index: 0 Engine, 1 Time, 2 Search, 3 UI, 4 Hotkeys.
    std::string_view label;       ///< Text shown in the dialog (unchanged from before PAL-03).
    std::string_view titleEn;     ///< Palette title (English).
    std::string_view titleVi;     ///< Palette title (Vietnamese).
    std::string_view keywordsEn;  ///< ';'-separated search keywords.
    std::string_view keywordsVi;
};

inline constexpr std::array<std::string_view, 5> kTabs = {"Engine", "Time", "Search", "UI", "Hotkeys"};

inline constexpr std::array kSettings = std::to_array<SettingEntry>({
    {"set.engine-path", 0, "Engine Path", "Engine Path", "Đường dẫn engine",
     "executable;binary;rapfi;yixin;exe", "file chạy;chương trình;đặt engine"},
    {"set.ptc", 0, "Protocol Extension (.ptc)", "Protocol Extension (.ptc)", "Mở rộng giao thức (.ptc)",
     "extension;plugin;commands", "tiện ích;lệnh mở rộng"},
    {"set.threads", 0, "Threads", "Threads", "Số luồng",
     "cpu;cores;parallel;processors", "nhân;đa luồng;bộ xử lý"},
    {"set.hash", 0, "Hash Size (MB)", "Hash Size (MB)", "Dung lượng hash (MB)",
     "memory;transposition table;ram", "bộ nhớ;bảng chuyển vị"},
    {"set.timeout-turn", 1, "Timeout / Turn (ms)", "Timeout / Turn (ms)", "Thời gian mỗi nước (ms)",
     "time limit;move time;seconds", "giới hạn;giây;suy nghĩ"},
    {"set.timeout-match", 1, "Timeout / Match (ms)", "Timeout / Match (ms)", "Tổng thời gian ván (ms)",
     "clock;game time;total", "đồng hồ;cả ván"},
    {"set.increment", 1, "Increment (ms)", "Increment (ms)", "Cộng thêm giờ (ms)",
     "fischer;bonus time", "thời gian cộng;thêm giờ"},
    {"set.max-depth", 2, "Max Depth", "Max Depth", "Độ sâu tối đa",
     "plies;search depth;deeper", "độ sâu;số tầng"},
    {"set.max-nodes", 2, "Max Nodes", "Max Nodes", "Số nút tối đa",
     "node budget;limit search", "giới hạn nút"},
    {"set.multipv", 2, "Multi PV", "Multi PV", "Nhiều biến (MultiPV)",
     "principal variations;lines;candidates", "số biến;nhiều đường"},
    {"set.analysis-detail", 2, "Analysis Detail", "Analysis Detail", "Mức chi tiết phân tích",
     "show detail;verbosity;info", "chi tiết;mức độ"},
    {"set.theme", 3, "Theme", "Theme", "Giao diện",
     "dark;light;colours;appearance", "tối;sáng;màu sắc;chủ đề"},
    {"set.wingraph-mode", 3, "WinGraph Mode", "WinGraph Mode", "Chế độ biểu đồ thắng",
     "graph;chart;winrate graph;plot", "đồ thị;biểu đồ"},
    {"set.move-numbers", 3, "Show Move Numbers", "Show Move Numbers", "Hiện số thứ tự nước",
     "ordinal;stone numbers;digits", "số nước;đánh số"},
    {"set.coordinates", 3, "Show Coordinates", "Show Coordinates", "Hiện tọa độ",
     "labels;letters;rows;columns", "nhãn;hàng cột;chữ cái"},
    {"set.hotkey-analyze", 4, "Analyze", "Hotkey: Analyze", "Phím tắt: Phân tích",
     "shortcut;keybinding;key", "bàn phím;tổ hợp phím"},
    {"set.hotkey-stop", 4, "Stop", "Hotkey: Stop", "Phím tắt: Dừng",
     "shortcut;keybinding;key", "bàn phím;tổ hợp phím"},
    {"set.hotkey-undo", 4, "Undo", "Hotkey: Undo", "Phím tắt: Hoàn tác",
     "shortcut;keybinding;key", "bàn phím;tổ hợp phím"},
    {"set.hotkey-redo", 4, "Redo", "Hotkey: Redo", "Phím tắt: Làm lại",
     "shortcut;keybinding;key", "bàn phím;tổ hợp phím"},
    {"set.hotkey-newgame", 4, "New Game", "Hotkey: New Game", "Phím tắt: Ván mới",
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
