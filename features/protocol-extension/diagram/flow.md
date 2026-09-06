# Open Protocol Extension — diagrams

See [../user_story.md](../user_story.md) and [../planning.md](../planning.md).

## Component layering (where new pieces sit)

```mermaid
flowchart TD
    subgraph engine["src/engine/ (unchanged interface)"]
        IEP["IEngineProtocol\n(pure abstract, untouched)"]
        GP["GomocupProtocol\n(implements IEngineProtocol)"]
        ICS["ICustomCommandSource\n(new, small, optional)"]
        PE["protocol_extension.{h,cpp}\n(new: .ptc loader + DSL interpreter)"]
        EC["EngineController"]
    end
    subgraph cmd["src/command/"]
        CD["CommandDispatcher"]
    end
    subgraph ui["src/ui/ (wired in main_window.cpp)"]
        Status["EngineStatusView\n+ new: small dynamic row container\nfor arbitrary-named set_status_field"]
        Banner["Inline banner\n(crash-banner pattern, pre-existing)"]
        Log["EngineLogModel\n(pre-existing)"]
    end

    GP -->|implements| IEP
    GP -->|implements, only when .ptc loaded| ICS
    GP -->|loads table from| PE
    EC -->|dynamic_cast to| ICS
    CD -->|sendCustomCommand| EC
    EC -->|signal_custom_action| Status
    EC -->|signal_custom_action| Banner
    EC -->|signal_custom_action| Log
```

Notes:
- `highlight_cell`/`clear_highlights` (a `BoardRenderer` sink) were considered and dropped from the
  v1 sink whitelist 2026-09-06 — no `src/ui/board_*` file is touched by this feature.
- `EngineStatusView` needs one small addition (a dynamic label-pair row keyed by `.ptc`-chosen field
  name) since its 6 existing stat fields are fixed members, not a lookup-by-name container — see
  user_story.md's sink whitelist note (2026-09-06).

## Sequence — engine developer's command end to end

```mermaid
sequenceDiagram
    participant Dev as Engine developer
    participant Cfg as EngineConfig (.ptc path)
    participant GP as GomocupProtocol
    participant CD as CommandDispatcher
    participant EC as EngineController
    participant E as Engine process
    participant UI as UI sinks

    Dev->>Cfg: writes yxAnalyzeOne.ptc
    Note over GP: startEngine() / reloadEngine()
    Cfg->>GP: load .ptc path
    GP->>GP: parse TOML, validate (Q3/Q5 limits), build command table
    GP->>CD: (via EC) register "yxAnalyzeOne" console command

    Note over CD: reviewer types the command in console
    CD->>EC: sendCustomCommand("yxAnalyzeOne", [7,7])
    EC->>GP: dynamic_cast<ICustomCommandSource>()->generateCustom(...)
    GP-->>EC: "YXANALYZEONE 7,7"
    EC->>E: sendLine("YXANALYZEONE 7,7")

    E-->>EC: signal_line_received("MOVEEVAL 7,7 0.93")
    EC->>GP: parseLine(line)
    GP->>GP: built-in patterns: no match
    GP->>GP: extension pattern matches -> typed fields {x:7,y:7,win:0.93}
    GP->>GP: interpret action DSL (if win > 0.9 ...)
    GP-->>EC: signal_custom_action(SetStatusField{"eval", 0.93})
    EC-->>UI: forwarded via main_window.cpp wiring
    UI->>UI: EngineStatusView shows the "eval" field, no board redraw involved
```

## Trust-boundary enforcement (load time vs. per-line)

```mermaid
flowchart LR
    A[".ptc file\n(semi-trusted: shared/downloaded)"] -->|load once| B["Schema + limit validation\n(Q5: depth, count, size caps)"]
    B -->|reject on violation| X["Refuse to load,\nsurface error to user"]
    B -->|valid| C["In-memory command table"]

    D["Engine stdout line\n(untrusted, PROTO-01 precedent)"] -->|per line| E["Pattern match + typed field extraction"]
    E -->|type/format mismatch| Y["Skip line,\nlog Debug — never UB"]
    E -->|matches| F["DSL interpreter\n(no loops, no recursion,\nonly declared fields,\nonly whitelisted sinks)"]
    C --> F
    F --> G["Sink call emitted"]
```
