#include "ui/palette_catalog.h"

#include "ui/settings_registry.h"

namespace palette_catalog {

const std::array<ActionMeta, 22> kActions = {{
    {"act.new-game", "New Game", "Ván mới",
     "start;restart;reset;fresh;clear board", "chơi lại;bắt đầu;làm mới;xóa bàn"},
    {"act.load-game", "Load Game", "Mở ván cờ",
     "open;file;import;read;rdb", "tải;mở file;nhập;đọc"},
    {"act.save-game", "Save Game", "Lưu ván cờ",
     "write;export;store;file", "ghi;xuất;lưu file"},
    {"act.quit", "Quit", "Thoát",
     "exit;close;leave", "đóng;thoát chương trình"},
    {"act.board-size", "Board Size", "Kích thước bàn cờ",
     "resize;dimensions;grid;15x15;19x19", "cỡ bàn;đổi cỡ;lưới"},
    {"act.rule-freestyle", "Rule: Freestyle Gomoku", "Rule: Freestyle Gomoku",
     "gomoku;free;rules;overline", "tự do;luật tự do;luật chơi;cờ caro;năm quân"},
    {"act.rule-standard", "Rule: Standard Gomoku", "Rule: Standard Gomoku",
     "exactly five;gomoku;rules", "tiêu chuẩn;luật tiêu chuẩn;luật chuẩn;đúng năm"},
    {"act.rule-renju", "Rule: Free Renju", "Rule: Free Renju",
     "renju;forbidden;black restrictions", "renju;luật renju;cấm tay;luật cấm;quân đen"},
    {"act.settings", "Settings", "Cài đặt",
     "preferences;options;configure;config", "tùy chọn;thiết lập;cấu hình"},
    {"act.about", "About", "Giới thiệu",
     "version;info;credits", "phiên bản;thông tin"},
    {"act.analyze", "Start Analysis", "Bắt đầu phân tích",
     "analyze;search;think;engine", "chạy engine;tính nước;suy nghĩ"},
    {"act.stop", "Stop Analysis", "Dừng phân tích",
     "stop;halt;cancel;interrupt", "ngừng;hủy;dừng engine"},
    {"act.analyze-mode", "Analyze Mode", "Chế độ phân tích",
     "continuous;auto analyze;live", "liên tục;tự động phân tích"},
    {"act.engine-plays-black", "Engine plays Black", "Máy chơi quân đen",
     "computer;ai;opponent;play black", "máy đánh;đấu với máy"},
    {"act.engine-plays-white", "Engine plays White", "Máy chơi quân trắng",
     "computer;ai;opponent;play white", "máy đánh;đấu với máy"},
    {"act.engine-plays-off", "Engine plays Off", "Tắt máy đánh",
     "disable computer;two players;manual", "hai người;tự đánh"},
    {"act.view-search-overlay", "Search Overlay", "Hiện lớp gợi ý nước",
     "candidates;markers;engine marks;board overlay", "gợi ý nước;đánh dấu;lớp phủ"},
    {"act.view-search-winrate", "Search Winrate Tags", "Hiện tỉ lệ thắng",
     "percent;win probability;tags;heatmap", "xác suất thắng;phần trăm;nhãn"},
    {"act.nav-first", "First Move", "Về nước đầu",
     "start;beginning;rewind;home", "đầu ván;quay về đầu"},
    {"act.nav-undo", "Undo", "Hoàn tác",
     "back;previous move;take back", "lùi;quay lại;đi lại"},
    {"act.nav-redo", "Redo", "Làm lại",
     "forward;next move;replay", "tiến;nước kế"},
    {"act.nav-last", "Last Move", "Đến nước cuối",
     "end;final;latest", "cuối ván;nước mới nhất"},
}};

const std::array<CommandMeta, 19> kCommands = {{
    {"help", "Xem trợ giúp",
     "commands;list;manual", "danh sách lệnh;hướng dẫn"},
    {"about", "Hỏi thông tin engine",
     "engine info;version", "phiên bản engine"},
    {"start", "Ván mới với cỡ bàn",
     "size;restart", "đổi cỡ;chơi lại"},
    {"rule", "Đặt luật chơi",
     "freestyle;standard;renju", "luật;cờ caro"},
    {"new", "Chơi lại ván này",
     "reset;keep size", "làm mới;giữ cỡ bàn"},
    {"undo", "Lùi một nước",
     "back;take back", "hoàn tác;đi lại"},
    {"redo", "Tiến một nước",
     "forward;replay", "làm lại"},
    {"loadpos", "Nạp thế cờ từ chuỗi nước",
     "moves;paste;position", "dán nước đi;thế cờ"},
    {"pos", "Nhập thế cờ",
     "moves;setup", "đặt thế;nhập nước"},
    {"getpos", "In thế cờ hiện tại",
     "dump;coordinates;export", "xuất tọa độ;thế cờ"},
    {"analyze", "Bắt đầu phân tích",
     "search;think", "chạy engine;suy nghĩ"},
    {"yxanalz", "Chỉ phân tích các nước chọn",
     "allow-list;candidates;root moves", "nước chỉ định;giới hạn nước"},
    {"stop", "Dừng phân tích",
     "halt;cancel", "ngừng;hủy"},
    {"play", "Nạp nước rồi phân tích",
     "moves;analyze", "nạp nước đi"},
    {"engine", "Quản lý engine: chạy, dừng, nạp lại",
     "restart engine;process", "khởi động lại engine"},
    {"send", "Gửi dòng lệnh thô tới engine",
     "raw;protocol;gomocup", "giao thức;lệnh thô"},
    {"info", "Sửa cấu hình engine",
     "set key value;config", "đặt tham số;cấu hình"},
    {"clear", "Xóa nhật ký console",
     "empty;wipe log", "làm sạch;xóa log"},
    {"db", "Lệnh cơ sở dữ liệu",
     "query;load;save;label;comment", "truy vấn;nhãn;ghi chú"},
}};

namespace {
const CommandMeta *commandMeta(const std::string &name)
{
    for (const auto &m : kCommands)
        if (m.name == name)
            return &m;
    return nullptr;
}
}  // namespace

std::vector<palette_search::Entry> buildEntries(const std::vector<CommandSpec> &commands)
{
    std::vector<palette_search::Entry> out;
    for (const auto &a : kActions)
        out.push_back({std::string(a.id), "act", std::string(a.titleEn), std::string(a.titleVi),
                       std::string(a.keywordsEn), std::string(a.keywordsVi), "Action", true});
    for (const auto &s : settings_registry::kSettings)
        out.push_back({std::string(s.id), "set", std::string(s.titleEn), std::string(s.titleVi),
                       std::string(s.keywordsEn), std::string(s.keywordsVi), "Setting", true});
    for (const auto &c : commands) {
        const CommandMeta *m = commandMeta(c.name);
        const std::string  bang = "!" + c.name + " \xE2\x80\x94 ";  // "!name — "
        palette_search::Entry e;
        e.id         = "cmd." + c.name;
        e.kind       = "cmd";
        e.titleEn    = bang + c.summary;
        e.titleVi    = m ? bang + std::string(m->titleVi) : e.titleEn;
        e.keywordsEn = (m ? std::string(m->keywordsEn) + ";" : std::string()) + c.group;
        e.keywordsVi = m ? std::string(m->keywordsVi) : std::string();
        e.group      = "Command";
        out.push_back(std::move(e));
    }
    return out;
}

bool commandTakesArguments(const CommandSpec &spec)
{
    return spec.usage.find('<') != std::string::npos || spec.usage.find('[') != std::string::npos;
}

}  // namespace palette_catalog
