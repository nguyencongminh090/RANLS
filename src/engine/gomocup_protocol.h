#pragma once

#include "i_engine_protocol.h"
#include "i_custom_command_source.h"
#include "protocol_extension.h"
#include <array>
#include <map>
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
    std::vector<std::string> generateAnalyzeMovesRequest(const std::vector<Coord>& path,
                                                         const std::vector<Coord>& moves) override;
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
    bool parseMessageEval(int pvIdx, const std::string &eval, double &winrate, int &mateStep,
                          std::string &evalText) const;
    void parseDatabase(const std::string &dbLine);
    void onPVDone();

    // ── PROTO-09: one round model shared by the INFO and MESSAGE streams ────
    // A "round" is one pass of the engine over its PV list (one depth
    // iteration in alpha-beta, one periodic root-move dump in MCTS). Both
    // streams report the same rounds, interleaved, so neither may truncate the
    // PV list on its own: rows and board tags are stamped with the round that
    // last refreshed them and only dropped once a whole round went by without
    // them (or as soon as INFO NUMPV says the round is complete).
    enum class PvSource { Info = 0, Message = 1 };
    enum class MsgKind { Ranked, DepthSummary, Bestline };
    /// Note that `src` reports PV `idx`; the same source repeating an index
    /// means the previous round is over, so a new one starts.
    void beginReport(PvSource src, int idx);
    /// Grow currentPVs_ (and its round stamps) so row `idx` exists.
    void ensureRow(int idx);
    /// Drop trailing PV rows and board tags last refreshed before `minRound`.
    void pruneStale(int minRound);
    /// Write `pv`'s winrate/mate tag onto its first move's cell.
    void stampTag(const PVLine &pv);
    /// Commit a PV parsed from a MESSAGE line (never overwrites a row the INFO
    /// stream already wrote in this round — INFO carries the engine's own
    /// winrate, per-move node count and full-precision line).
    void commitMessagePV(int idx, PVLine pv, MsgKind kind);
    /// Learn the engine's cp -> winrate sigmoid scale from one INFO pair.
    void learnCpScale(const std::string &evalText, double winrate);
    void tryExtensionReply(const std::string &line);

    /// PROTO-06: push the current accumulated overlay out on
    /// signal_analysis_overlay. Called per mutating REALTIME line and from
    /// onPVDone(); the downstream (GameState) coalesces to the RT-01 tick.
    void emitOverlay();

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
    bool        currentPvHasInfoWinrate_ = false;   ///< INFO WINRATE seen for this PV block

    /// Engine-reported winrate per PV index, with the INFO EVAL text it belongs to.
    struct InfoWinrate {
        bool        valid = false;
        double      winrate = 0.5;
        std::string evalText;
    };
    std::vector<InfoWinrate> infoWinrates_;
    std::vector<Coord> currentBestLine_;

    // PROTO-09 round bookkeeping (see beginReport()).
    int              roundId_     = 0;   ///< current round, 0 = none yet
    int              rankedRound_ = 0;   ///< round whose PV #1 a ranked MESSAGE line reported
    int              infoNumPV_   = 0;   ///< last INFO NUMPV (0 = stream gives no count)
    std::vector<int> pvRound_;           ///< per PV row: round that last refreshed it
    std::vector<int> pvInfoRound_;       ///< per PV row: round the INFO stream last wrote it
    std::array<std::vector<int>, 2> reportRound_;   ///< per source, per PV index
    std::map<Coord, int> tagRound_;      ///< per tagged cell: round that last stamped it
    /// cp -> winrate scale for MESSAGE evals with no INFO WINRATE of their own.
    /// Engine/config constant, so it deliberately survives clearAnalysisState().
    double cpScale_ = 200.0;

    // PROTO-06: accumulated live per-cell search overlay for the current think.
    // Fed by parseMessage()'s REALTIME branch + onPVDone(); cleared by
    // clearAnalysisState() (analysis start / position change).
    AnalysisOverlay overlay_;
};
