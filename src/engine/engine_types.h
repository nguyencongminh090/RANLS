#pragma once

// Protocol-only types. Analysis domain data (PVLine, EngineStatus, DatabaseEntry,
// AnalysisOverlay*) moved to model/analysis_types.h (ARCH-01); re-included here
// (engine -> model is the allowed direction) so engine-side includes keep working.
#include "model/analysis_types.h"

/// Category of an engine output message.
enum class EngineMessageType {
    Output,
    Coord,
    Message,
    Error,
    Debug
};
