# Console AutoComplete + AutoCorrect — diagrams

Back to [../user_story.md](../user_story.md) · [../planning.md](../planning.md).

## Keystroke → suggestion (Feature A)

```mermaid
sequenceDiagram
    actor U as User
    participant E as Gtk::Entry (commandEntry_)
    participant KC as EventControllerKey
    participant M as CommandCompleter (pure, src/ui or src/command)
    participant R as CommandDispatcher registry
    participant P as Suggestion popover

    U->>E: types "!an"
    E->>M: notify::text  ("!an")
    M->>R: registeredNames()  (built-ins + .ptc)
    R-->>M: ["analyze","about","play",...]
    M->>M: filter prefix "an" → ["analyze"]
    M->>P: show(["analyze"], usage hint)
    U->>KC: Tab
    KC->>M: completeToken()
    M->>M: longest-common-prefix → "analyze"
    M->>E: set_text("!analyze "), caret to end
    M->>P: show(usage "analyze [depth]")
    U->>KC: Esc
    KC->>P: hide()  (entry text unchanged, R5/A5)
```

## Submit → autocorrect (Feature B)

```mermaid
flowchart TD
    A[User presses Enter] --> B{trimmed input}
    B -->|"empty"| Z[ignore]
    B -->|"starts with ! or ！"| C[extract first token]
    B -->|"no bang"| D{first token == registered name?}
    D -->|yes| E[B1: rewrite to '!token …']
    D -->|no| P[pass through unchanged<br/>→ existing raw-line rules]
    C --> F{token matches a name exactly, any case?}
    F -->|"exact, wrong case"| G[B2: normalise to registered spelling]
    F -->|"exact match"| H[no change]
    F -->|"no exact match"| I{edit-distance ≤ 2 to one name?}
    I -->|yes| J[B4: echo 'did you mean !name ?'<br/>do NOT auto-run]
    I -->|no| K[pass through → 'Unknown internal command']
    E --> L[show corrected text in entry<br/>B5: visible + undoable]
    G --> L
    L --> M[dispatch corrected line to executeLine]
    H --> M
    M --> N[log as SEND, push to commandHistory_]
```

## Where the new piece sits (component)

```mermaid
classDiagram
    class BottomPanel {
        Gtk::Entry commandEntry_
        vector~string~ commandHistory_
        int historyIdx_
        +signal_command_sent
    }
    class CommandCompleter {
        <<pure, no GTK>>
        +matches(prefix, names) vector~string~
        +longestCommonPrefix(matches) string
        +normalizeCase(token, names) optional~string~
        +didYouMean(token, names) optional~string~
    }
    class SuggestionPopover {
        <<src/ui>>
        +show(items, hint)
        +hide()
        +selected() optional~string~
    }
    class CommandDispatcher {
        +registeredNames() vector~string~   («new, read-only»)
        +executeLine(line)
    }
    BottomPanel --> CommandCompleter : owns
    BottomPanel --> SuggestionPopover : owns
    BottomPanel ..> CommandDispatcher : registeredNames() on popup open
    CommandCompleter ..> CommandDispatcher : candidate set (via BottomPanel)
```
