# Board marks — diagrams

See [../user_story.md](../user_story.md) and [../planning.md](../planning.md).

## Data flow (unchanged layering: ui → model)

```mermaid
flowchart LR
    GS[GameState] --> BVM["BoardViewModel::update()<br/>resolves render-ready marks"]
    AO[AnalysisOverlay] --> GS
    DB["DatabaseEntry map"] --> GS
    VT[VariationTree] --> GS
    BVM --> BR["BoardRenderer<br/>(reads only)"]
    BVM --> BV["BoardView<br/>tooltip lookup"]
```

## Today: one mark per cell vs. proposed: channels that stack

```mermaid
flowchart TB
    subgraph Today["Today (single winner per cell)"]
        T1["tag > lost > best > examined > examining"] --> T2["Best usually lost behind tag"]
        T3["DB diamond drawn first"] --> T4["Hidden under engine tag"]
    end
    subgraph Proposed["Proposed (layers by channel)"]
        P1["Engine disc (heat, centre)"]
        P2["Best ring (flag, over disc)"]
        P3["DB outlined diamond / corner badge"]
        P4["Variant ring+dot (+count)"]
        P5["Lost X, forbidden X (shape-distinct)"]
    end
```

## Z-order (proposed, bottom → top)

```mermaid
flowchart LR
    a[grid] --> b[stones+numbers] --> c[last move] --> d[variant] --> e[database] --> f[engine disc/tag] --> g[best ring] --> h[lost/forbidden] --> i[PV ghost] --> j["hover (empty cells only)"]
```
