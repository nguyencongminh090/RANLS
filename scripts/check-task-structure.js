#!/usr/bin/env node
'use strict';

// Structural lint for TODO.md / instruction.md and their docs/todo,instruction/
// detail files. Complements check-tracking-sync.js (which only checks the ✅
// done-marker sync) by catching layout problems: duplicate CODEs, index lines
// pointing at a detail file that doesn't exist, malformed index lines, and
// detail files that aren't linked from any index (orphans). See
// .claude/rules/tracking-files.md.
//
// Usage: node scripts/check-task-structure.js
// Exit 0 + "OK" if clean, exit 1 + a report of every problem found otherwise.
//
// CODE may itself contain hyphens (e.g. "WALL-01"), which makes splitting a
// bare "<CODE>-<slug>.md" filename back into its parts ambiguous. To avoid
// that, every check below either (a) matches the filename via a backreference
// to the CODE already captured from the heading/link text, or (b) compares
// whole filenames instead of re-deriving CODE from a filename.

const fs = require('fs');
const path = require('path');

const ROOT = path.resolve(__dirname, '..');
const TODO_PATH = path.join(ROOT, 'TODO.md');
const INSTRUCTION_PATH = path.join(ROOT, 'instruction.md');
const TODO_DIR = path.join(ROOT, 'docs', 'todo');
const INSTRUCTION_DIR = path.join(ROOT, 'docs', 'instruction');

// A line that looks like a task index entry at all (used to flag malformed ones).
const BULLET_START_RE = /^-\s*(✅\s*)?\*\*([A-Za-z0-9-]+)\.\*\*/;
// A fully well-formed index line: - **CODE.** summary [...] — [detail](docs/todo/CODE-slug.md)
const TODO_LINE_RE = /^-\s*(✅\s*)?\*\*([A-Za-z0-9-]+)\.\*\*.*\(docs\/todo\/\2-([^)]+)\.md\)/;
// ## CODE — slug   (heading form used in instruction.md; CODE bounded by whitespace, not hyphen)
const INSTRUCTION_HEADING_RE = /^##\s+([A-Za-z0-9-]+)\s/;

function readLines(p) {
  if (!fs.existsSync(p)) return [];
  return fs.readFileSync(p, 'utf8').split('\n');
}

function listMdFiles(dir) {
  if (!fs.existsSync(dir)) return [];
  return fs.readdirSync(dir).filter((f) => f.endsWith('.md') && f !== 'README.md');
}

function checkTodo(problems) {
  const lines = readLines(TODO_PATH);
  const seen = new Map(); // code -> line number
  const linkedFiles = new Set(); // exact "CODE-slug.md" filenames referenced

  lines.forEach((line, i) => {
    const trimmed = line.trim();
    const bulletMatch = BULLET_START_RE.exec(trimmed);
    if (!bulletMatch) return;
    const lineNo = i + 1;
    const code = bulletMatch[2];

    const full = TODO_LINE_RE.exec(trimmed);
    if (!full) {
      problems.push(`TODO.md:${lineNo}: malformed index line for "${code}" — expected ` +
        `"... — [detail](docs/todo/${code}-<slug>.md)"`);
      return;
    }

    if (seen.has(code)) {
      problems.push(`TODO.md:${lineNo}: duplicate CODE "${code}" (also at line ${seen.get(code)})`);
    } else {
      seen.set(code, lineNo);
    }

    const filename = `${code}-${full[3]}.md`;
    linkedFiles.add(filename);
    if (!fs.existsSync(path.join(TODO_DIR, filename))) {
      problems.push(`TODO.md:${lineNo}: links to docs/todo/${filename}, which doesn't exist`);
    }
  });

  for (const f of listMdFiles(TODO_DIR)) {
    if (!linkedFiles.has(f)) {
      problems.push(`docs/todo/${f}: not linked from any TODO.md line (orphan)`);
    }
  }

  return seen; // codes filed in TODO.md, for cross-check against instruction.md
}

function checkInstruction(problems, todoCodes) {
  const lines = readLines(INSTRUCTION_PATH);
  const seen = new Map();

  lines.forEach((line, i) => {
    const m = INSTRUCTION_HEADING_RE.exec(line);
    if (!m) return;
    const code = m[1];
    const lineNo = i + 1;

    if (seen.has(code)) {
      problems.push(`instruction.md:${lineNo}: duplicate CODE "${code}" (also at line ${seen.get(code)})`);
    } else {
      seen.set(code, lineNo);
    }

    if (!todoCodes.has(code)) {
      problems.push(`instruction.md:${lineNo}: CODE "${code}" has no matching entry in TODO.md`);
    }

    const files = listMdFiles(INSTRUCTION_DIR);
    if (!files.some((f) => f.startsWith(code + '-'))) {
      problems.push(`instruction.md:${lineNo}: CODE "${code}" has no matching docs/instruction/${code}-*.md file`);
    }
  });

  for (const f of listMdFiles(INSTRUCTION_DIR)) {
    const matched = [...seen.keys()].some((code) => f.startsWith(code + '-'));
    if (!matched) {
      problems.push(`docs/instruction/${f}: not linked from any instruction.md heading (orphan)`);
    }
  }
}

function main() {
  const problems = [];
  const todoCodes = checkTodo(problems);
  checkInstruction(problems, todoCodes);

  if (problems.length === 0) {
    console.log('OK: TODO.md / instruction.md structure is consistent.');
    process.exit(0);
  }

  console.log(`Found ${problems.length} structural problem(s):`);
  for (const p of problems) console.log(`  ${p}`);
  process.exit(1);
}

main();
