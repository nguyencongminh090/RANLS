# Diagrams — versioning + changelog

See [../user_story.md](../user_story.md) and [../planning.md](../planning.md).

## Release cut at sprint close (sequence)

```mermaid
sequenceDiagram
    actor M as Maintainer
    participant S as docs/sprint/archive/
    participant F as docs/fix-log.md
    participant C as CHANGELOG.md
    participant G as git / GitHub

    M->>S: close sprint N (existing step)
    M->>F: review rows since last v0.x tag
    M->>C: move [Unreleased] items into ## [0.(N).0] - <date>
    Note over M,C: phrase as user impact, group Added/Changed/Fixed
    M->>C: add fresh empty [Unreleased] at top
    M->>G: commit "Release v0.N.0", tag v0.N.0, push --tags
    opt Releases enabled (future)
        G-->>G: GitHub Release from tag, body = changelog section, attach build
    end
```

## Version string — one source, many readers

```mermaid
flowchart TD
    CM["CMakeLists.txt<br/>project(rapfi-gui VERSION 0.N.0)"] -->|configure_file| H["version.h (generated)<br/>APP_VERSION = \"0.N.0\""]
    H --> A["Help → About dialog<br/>set_version(APP_VERSION)"]
    H --> CLI["--version handler<br/>prints APP_VERSION, exits before GTK init"]
    CM -.matches.-> TAG["git tag v0.N.0"]
    TAG -.matches.-> CH["CHANGELOG.md ## [0.N.0]"]
```

## Backfill (one-time)

```mermaid
flowchart LR
    AR["docs/sprint/archive/<br/>sprint-1 … sprint-6"] --> R{reconstruct<br/>user-facing lines}
    FL["docs/fix-log.md rows"] --> R
    R --> V0["CHANGELOG.md<br/>## [0.1.0] initial … up to current"]
    V0 --> T["tag the current commit v0.(current)"]
```
