# WARNING - read before using `google-style-cpp` in another workspace

This skill was copied into the KnowledgeTree from an external GitHub repository (MIT, TestDino). It repackages Google's public style guide.
It may contain conventions that differ from the target workspace's formatter, linter or CLAUDE.md.

- Original location: https://github.com/testdino-hq/google-styleguides-skills/tree/8674a2a/cpp
- Fetched: 2026-09-20 (verbatim copy of the language folder only; the pack's top-level SKILL.md and the other 11 languages were not copied)
- Files with absolute-path-looking references: 0

Checklist before use:
1. The frontmatter name is 'cpp' (generic); the folder was renamed `google-style-cpp` to avoid clashes.
2. Precedence: the target workspace's own formatter config and CLAUDE.md win over this guide. In this tree, `rules/coding-standards/items/coding-style.md` adds a column-alignment pass run only on the user's `/align`, which the Google guide otherwise discourages.
3. Delete this warning file from the copy once the skill has been adapted.

## Known conflicts with knowledge notes

Reviewed 2026-09-26 (`docs/skill-knowledge-review.md`). Where this skill and a note disagree, follow the note
and tell the user why. The skill files themselves are kept verbatim.

- `pattern-singleton`: suggests singletons for mutable globals; dependency injection is the default.
