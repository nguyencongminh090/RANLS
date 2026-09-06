#pragma once

#include "i_engine_protocol.h"
#include "i_custom_command_source.h"
#include "protocol_extension.h"
#include <array>
#include <memory>

/// Implements the Gomocup and Yixin Extended protocols. PROTO-03: also acts as
/// the ICustomCommandSource once a `.ptc` extension table is loaded.
class GomocupProtocol : public IEngineProtocol, public ICustomCommandSource {
public:
    GomocupProtocol(int boardSize);
    ~GomocupProtocol() override = default;

    // ── PROTO-03: optional `.ptc` extension table ───────────────────────────
    /// Install (or clear, with nullptr) the loaded extension table. Load-once
    /// per engine start/reload — no hot reload (Q6).
    void setExtension(std::shared_ptr<protoext::ExtensionTable> table);
    bool hasExtension() const { return ext_ != nullptr; }

    std::vector<std::string> customCommandNames() const override;
    std::string customCommandGroup(const std::string &name) const override;
    std::vector<std::string> generateCustom(
        const std::string              &name,
        const std::vector<std::string> &args,
        const std::vector<Coord>       &path) override;

    std::vector<std::string> generateStart(int boardSize) override;
    std::vector<std::string> generateRule(GameRule rule) override;
    std::vector<std::string> generateConfig(const EngineConfig& cfg) override;
    std::vector<std::string> generateAnalyzeRequest(const std::vector<Coord>& path, int multiPV) override;
    std::vector<std::string> generateMoveRequest(const std::vector<Coord>& path) override;
    std::string generateStop() override;
    void clearAnalysisState() override;
    std::string generateQuit() override;
    std::vector<std::string> generateDatabaseQuery(const std::vector<Coord>& path) override;

    void parseLine(const std::string& line) override;

private:
    void parseRealtimePV(const std::string &data);
    void parseMessage(const std::string &msg);
    void parseInfo(const std::string &info);
    void resetCurrentPVState();
    void parseDatabase(const std::string &dbLine);
    void onPVDone();
    void tryExtensionReply(const std::string &line);

    std::shared_ptr<protoext::ExtensionTable> ext_;

    int boardSize_ = 15;

    // Accumulated analysis data for the current think.
    std::vector<PVLine> currentPVs_;
    EngineStatus        currentStatus_;

    // INFO PV state machine.
    int   currentPVIndex_  = 0;
    int   currentNumPV_    = 0;
    int   currentPvDepth_  = 0;
    int   currentPvSelDepth_ = 0;
    int64_t currentPvNodes_ = 0;
    double  currentPvWinrate_ = 0.5;
    int     currentPvMateStep_ = 0;
    std::string currentPvEvalText_;
    std::vector<Coord> currentBestLine_;
};
