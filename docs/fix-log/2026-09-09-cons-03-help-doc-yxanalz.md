# CONS-03 — help-doc-yxanalz-protocol-extension (2026-09-09)

## Summary

Updated `src/command/command_dispatcher.cpp` to improve help text for the `!yxAnalz` command, densifying the `CommandSpec` summary to convey all required context in one line:
- YXANALZ is a protocol extension requiring a compatible engine
- runs a real interruptible search
- places no stone (analysis-only output)
- takes alphabetic moves only (h3, h2, h9 format)

**Help line (as displayed):**
```
  !yxAnalz <moveText...> — Analyze listed root moves (YXANALZ protocol extension—requires compatible engine); interruptible; no stone; alphabetic moves
```

**Previous version:**
```
  !yxAnalz <moveText...> — Analyze only the listed root moves (YXANALZ), e.g. !yxAnalz h3 h2 h9
```

## Action

Modified `/src/command/command_dispatcher.cpp` line 367:
- Changed `CommandSpec` `summary` field only; `usage` unchanged (remains `"!yxAnalz <moveText...>"`)
- No behavior changes; command logic untouched
- One commit: `7c613e6` "CONS-03: denser help text for !yxAnalz—convey protocol-extension dependency, interruptibility, no stone, alphabetic moves"

## Verification

1. **Build:** `./build.sh` clean, no new warnings ✓
2. **Tests:** `ctest` from `build_cmd/` — all 4/4 suites pass ✓
   - `test_proto07_yxanalz_console.cpp` assertions intact (usage string unchanged) ✓
3. **Help text eyeball:** Confirms all four requirements (a-d) appear in one line ✓

## Notes

- This is a documentation-only change; no regression test added (help text is static).
- The existing test `test_proto07_yxanalz_console.cpp` already scans `!help` output for `"!yxAnalz"` as part of test case "PROTO-07: !yxAnalz is registered in the analysis group and shows in !help".
