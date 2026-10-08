#include "ui/palette_catalog.h"

#include "ui/settings_registry.h"

namespace palette_catalog {

const std::array<ActionMeta, 22> kActions = {{
    {"act.new-game", i18n::trcNoop("palette", "New Game"),
     "start;restart;reset;fresh;clear board", "chơi lại;bắt đầu;làm mới;xóa bàn"},
    {"act.load-game", i18n::trcNoop("palette", "Load Game"),
     "open;file;import;read;rdb", "tải;mở file;nhập;đọc"},
    {"act.save-game", i18n::trcNoop("palette", "Save Game"),
     "write;export;store;file", "ghi;xuất;lưu file"},
    {"act.quit", i18n::trcNoop("palette", "Quit"),
     "exit;close;leave", "đóng;thoát chương trình"},
    {"act.board-size", i18n::trcNoop("palette", "Board Size"),
     "resize;dimensions;grid;15x15;19x19", "cỡ bàn;đổi cỡ;lưới"},
    {"act.rule-freestyle", i18n::trcNoop("palette", "Rule: Freestyle Gomoku"),
     "gomoku;free;rules;overline", "tự do;luật tự do;luật chơi;cờ caro;năm quân"},
    {"act.rule-standard", i18n::trcNoop("palette", "Rule: Standard Gomoku"),
     "exactly five;gomoku;rules", "tiêu chuẩn;luật tiêu chuẩn;luật chuẩn;đúng năm"},
    {"act.rule-renju", i18n::trcNoop("palette", "Rule: Free Renju"),
     "renju;forbidden;black restrictions", "renju;luật renju;cấm tay;luật cấm;quân đen"},
    {"act.settings", i18n::trcNoop("palette", "Settings"),
     "preferences;options;configure;config", "tùy chọn;thiết lập;cấu hình"},
    {"act.about", i18n::trcNoop("palette", "About"),
     "version;info;credits", "phiên bản;thông tin"},
    {"act.analyze", i18n::trcNoop("palette", "Start Analysis"),
     "analyze;search;think;engine", "chạy engine;tính nước;suy nghĩ"},
    {"act.stop", i18n::trcNoop("palette", "Stop Analysis"),
     "stop;halt;cancel;interrupt", "ngừng;hủy;dừng engine"},
    {"act.analyze-mode", i18n::trcNoop("palette", "Analyze Mode"),
     "continuous;auto analyze;live", "liên tục;tự động phân tích"},
    {"act.engine-plays-black", i18n::trcNoop("palette", "Engine plays Black"),
     "computer;ai;opponent;play black", "máy đánh;đấu với máy"},
    {"act.engine-plays-white", i18n::trcNoop("palette", "Engine plays White"),
     "computer;ai;opponent;play white", "máy đánh;đấu với máy"},
    {"act.engine-plays-off", i18n::trcNoop("palette", "Engine plays Off"),
     "disable computer;two players;manual", "hai người;tự đánh"},
    {"act.view-search-overlay", i18n::trcNoop("palette", "Search Overlay"),
     "candidates;markers;engine marks;board overlay", "gợi ý nước;đánh dấu;lớp phủ"},
    {"act.view-search-winrate", i18n::trcNoop("palette", "Search Winrate Tags"),
     "percent;win probability;tags;heatmap", "xác suất thắng;phần trăm;nhãn"},
    {"act.nav-first", i18n::trcNoop("palette", "First Move"),
     "start;beginning;rewind;home", "đầu ván;quay về đầu"},
    {"act.nav-undo", i18n::trcNoop("palette", "Undo"),
     "back;previous move;take back", "lùi;quay lại;đi lại"},
    {"act.nav-redo", i18n::trcNoop("palette", "Redo"),
     "forward;next move;replay", "tiến;nước kế"},
    {"act.nav-last", i18n::trcNoop("palette", "Last Move"),
     "end;final;latest", "cuối ván;nước mới nhất"},
}};

const std::array<CommandMeta, 19> kCommands = {{
    {i18n::trcNoop("palette-cmd", "help"),
     "commands;list;manual", "danh sách lệnh;hướng dẫn"},
    {i18n::trcNoop("palette-cmd", "about"),
     "engine info;version", "phiên bản engine"},
    {i18n::trcNoop("palette-cmd", "start"),
     "size;restart", "đổi cỡ;chơi lại"},
    {i18n::trcNoop("palette-cmd", "rule"),
     "freestyle;standard;renju", "luật;cờ caro"},
    {i18n::trcNoop("palette-cmd", "new"),
     "reset;keep size", "làm mới;giữ cỡ bàn"},
    {i18n::trcNoop("palette-cmd", "undo"),
     "back;take back", "hoàn tác;đi lại"},
    {i18n::trcNoop("palette-cmd", "redo"),
     "forward;replay", "làm lại"},
    {i18n::trcNoop("palette-cmd", "loadpos"),
     "moves;paste;position", "dán nước đi;thế cờ"},
    {i18n::trcNoop("palette-cmd", "pos"),
     "moves;setup", "đặt thế;nhập nước"},
    {i18n::trcNoop("palette-cmd", "getpos"),
     "dump;coordinates;export", "xuất tọa độ;thế cờ"},
    {i18n::trcNoop("palette-cmd", "analyze"),
     "search;think", "chạy engine;suy nghĩ"},
    {i18n::trcNoop("palette-cmd", "yxanalz"),
     "allow-list;candidates;root moves", "nước chỉ định;giới hạn nước"},
    {i18n::trcNoop("palette-cmd", "stop"),
     "halt;cancel", "ngừng;hủy"},
    {i18n::trcNoop("palette-cmd", "play"),
     "moves;analyze", "nạp nước đi"},
    {i18n::trcNoop("palette-cmd", "engine"),
     "restart engine;process", "khởi động lại engine"},
    {i18n::trcNoop("palette-cmd", "send"),
     "raw;protocol;gomocup", "giao thức;lệnh thô"},
    {i18n::trcNoop("palette-cmd", "info"),
     "set key value;config", "đặt tham số;cấu hình"},
    {i18n::trcNoop("palette-cmd", "clear"),
     "empty;wipe log", "làm sạch;xóa log"},
    {i18n::trcNoop("palette-cmd", "db"),
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

namespace {
constexpr std::string_view kViLang = "vi";
const char                *kDash   = " \xE2\x80\x94 ";  // " — "

std::string actionOrSettingVi(std::string_view titleEn)
{
    return i18n::trIn(kViLang, "palette", titleEn);
}
}  // namespace

std::vector<palette_search::Entry> buildEntries(const std::vector<CommandSpec> &commands)
{
    // titleVi is search data read from the Vietnamese catalog (vi.tsv) regardless of
    // the UI language, so ranking input is identical under every UI language.
    std::vector<palette_search::Entry> out;
    for (const auto &a : kActions)
        out.push_back({std::string(a.id), "act", std::string(a.titleEn), actionOrSettingVi(a.titleEn),
                       std::string(a.keywordsEn), std::string(a.keywordsVi), "Action", true});
    for (const auto &s : settings_registry::kSettings)
        out.push_back({std::string(s.id), "set", std::string(s.titleEn), actionOrSettingVi(s.titleEn),
                       std::string(s.keywordsEn), std::string(s.keywordsVi), "Setting", true});
    for (const auto &c : commands) {
        const CommandMeta *m = commandMeta(c.name);
        const std::string  bang = "!" + c.name + kDash;  // "!name — "
        palette_search::Entry e;
        e.id         = "cmd." + c.name;
        e.kind       = "cmd";
        e.titleEn    = bang + c.summary;
        const auto vi = m ? i18n::lookupIn(kViLang, "palette-cmd|" + c.name) : std::nullopt;
        e.titleVi    = vi ? bang + *vi : e.titleEn;
        e.keywordsEn = (m ? std::string(m->keywordsEn) + ";" : std::string()) + c.group;
        e.keywordsVi = m ? std::string(m->keywordsVi) : std::string();
        e.group      = "Command";
        out.push_back(std::move(e));
    }
    return out;
}

std::string displayTitle(const palette_search::Entry &e, std::string_view lang)
{
    if (e.kind == "cmd" && e.id.size() > 4) {
        const std::string name = e.id.substr(4);
        const std::string bang = "!" + name + kDash;
        if (e.titleEn.compare(0, bang.size(), bang) == 0)
            if (auto t = i18n::lookupIn(lang, "palette-cmd|" + name))
                return bang + *t;
        return e.titleEn;
    }
    return i18n::trIn(lang, "palette", e.titleEn);
}

std::string displayTitle(const palette_search::Entry &e)
{
    return displayTitle(e, i18n::currentLanguage());
}

bool commandTakesArguments(const CommandSpec &spec)
{
    return spec.usage.find('<') != std::string::npos || spec.usage.find('[') != std::string::npos;
}

}  // namespace palette_catalog
