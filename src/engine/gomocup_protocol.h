#pragma once

#include "i_engine_protocol.h"
#include <array>

/// Implements the Gomocup and Yixin Extended protocols.
class GomocupProtocol : public IEngineProtocol {
public:
    GomocupProtocol(int boardSize);
    ~GomocupProtocol() override = default;

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
