# Extract MainWindow orchestration — diagrams

See [../user_story.md](../user_story.md) and [../planning.md](../planning.md).

## Dependencies today vs. proposed

```mermaid
flowchart LR
    subgraph Today
        MW1["MainWindow (GTK)<br/>widgets + signal wiring +<br/>analyze/auto-move rules +<br/>file orchestration"] --> EC1[EngineController]
        MW1 --> GS1[GameState]
        MW1 --> RDB1["rdb::archive*"]
        EC1 --> GS1
    end
    subgraph Proposed
        MW2["MainWindow (GTK)<br/>widgets + signal wiring +<br/>idle scheduling + dialogs"] --> AC["AnalysisCoordinator<br/>(GTK-free decisions)"]
        MW2 --> GF["GameFileService<br/>(GTK-free)"]
        AC --> EC2[EngineController]
        AC --> GS2[GameState]
        GF --> RDB2["rdb::archive*"]
        GF --> GS2
        EC2 --> GS2
    end
```

Arrows point from dependant to dependency; nothing in `AnalysisCoordinator` / `GameFileService` includes
gtk/gtkmm.

## Analyze-restart decision, split across the seam

```mermaid
sequenceDiagram
    participant GS as GameState
    participant MW as MainWindow (GTK)
    participant AC as AnalysisCoordinator
    participant EC as EngineController

    GS->>MW: signal_board_changed (xN in a burst)
    MW->>AC: onBoardChanged()
    AC->>AC: scheduled_? coalesce, latch force
    AC->>MW: postIdle(cb)  (injected scheduler)
    Note over MW: Glib::signal_idle().connect_once(cb)<br/>— tests inject a manual queue instead
    MW-->>AC: cb() runs once, on the settled position
    AC->>AC: analyzeMode? engine running? Idle? converged && !force?
    AC->>EC: stopAnalysis()
    AC->>EC: analyze()
```

The coalescing flags, the guard order and the force-latch live in the coordinator; only "run this later"
stays in the widget class.
