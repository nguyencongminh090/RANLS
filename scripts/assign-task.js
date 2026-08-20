#!/usr/bin/env node
'use strict';

// Moves one TODO.md item from Backlog to Active (sprint commitment) and stamps
// it with an owner/model tag. See CLAUDE.md "Sprint cadence" and AGENTS.md
// "Model tiers by task shape" for what belongs in --model.
//
// Usage:
//   node scripts/assign-task.js <CODE> [--model="<tier>"] [--owner="<name>"]
//
// Re-running on an already-Active CODE updates its tags in place rather than
// erroring, so it also works as "reassign".

const fs = require('fs');
const path = require('path');

const ROOT = path.resolve(__dirname, '..');
const TODO_PATH = path.join(ROOT, 'TODO.md');

const HEADING_RE = /^##\s+(.+)$/;
const TODO_LINE_RE = /^-\s*(✅\s*)?\*\*([A-Za-z0-9-]+)\.\*\*/;
const BACKLOG_PLACEHOLDER_RE = /^_\(empty — file new items here/;
const ACTIVE_PLACEHOLDER_RE = /^_\(nothing committed to a sprint yet/;

function parseArgs(argv) {
  const code = argv[2];
  if (!code || code.startsWith('--')) {
    console.error('Usage: node scripts/assign-task.js <CODE> [--model="<tier>"] [--owner="<name>"]');
    process.exit(2);
  }
  let model = null;
  let owner = null;
  for (const arg of argv.slice(3)) {
    const m = /^--model=(.*)$/.exec(arg);
    const o = /^--owner=(.*)$/.exec(arg);
    if (m) model = m[1];
    else if (o) owner = o[1];
    else {
      console.error(`Unrecognized argument: ${arg}`);
      process.exit(2);
    }
  }
  return { code, model, owner };
}

// Returns { start, end } line-index range (end exclusive) of the section
// whose "## <name>" heading matches headingName, not counting the heading
// line itself.
function findSection(lines, headingName) {
  let start = -1;
  for (let i = 0; i < lines.length; i++) {
    const m = HEADING_RE.exec(lines[i]);
    if (m && m[1].trim() === headingName) {
      start = i + 1;
      break;
    }
  }
  if (start === -1) return null;
  let end = lines.length;
  for (let i = start; i < lines.length; i++) {
    if (HEADING_RE.test(lines[i])) {
      end = i;
      break;
    }
  }
  return { start, end };
}

function stampLine(line, model, owner) {
  const idx = line.indexOf(' — [detail](');
  if (idx === -1) {
    console.error(`Could not parse index line (missing " — [detail](...)" suffix):\n  ${line}`);
    process.exit(1);
  }
  let left = line.slice(0, idx);
  const right = line.slice(idx);

  // Strip any existing tags so re-running updates them instead of stacking.
  left = left.replace(/\s*\[Model:[^\]]*\]/g, '').replace(/\s*\[Owner:[^\]]*\]/g, '');

  if (model) left += ` [Model: ${model}]`;
  if (owner) left += ` [Owner: ${owner}]`;

  return left + right;
}

function main() {
  const { code, model, owner } = parseArgs(process.argv);

  if (!fs.existsSync(TODO_PATH)) {
    console.error('TODO.md not found.');
    process.exit(1);
  }
  const lines = fs.readFileSync(TODO_PATH, 'utf8').split('\n');

  const activeSection = findSection(lines, 'Active');
  const backlogSection = findSection(lines, 'Backlog');
  if (!activeSection || !backlogSection) {
    console.error('Could not find "## Active" and "## Backlog" sections in TODO.md.');
    process.exit(1);
  }

  // Already Active? Update tags in place instead of erroring.
  for (let i = activeSection.start; i < activeSection.end; i++) {
    const m = TODO_LINE_RE.exec(lines[i]);
    if (m && m[2] === code) {
      lines[i] = stampLine(lines[i], model, owner);
      fs.writeFileSync(TODO_PATH, lines.join('\n'));
      console.log(`Updated ${code} in place (already Active):\n  ${lines[i].trim()}`);
      return;
    }
  }

  // Find it in Backlog.
  let backlogLineIdx = -1;
  for (let i = backlogSection.start; i < backlogSection.end; i++) {
    const m = TODO_LINE_RE.exec(lines[i]);
    if (m && m[2] === code) {
      backlogLineIdx = i;
      break;
    }
  }
  if (backlogLineIdx === -1) {
    console.error(`CODE "${code}" not found in TODO.md's Backlog or Active section.`);
    process.exit(1);
  }

  const stamped = stampLine(lines[backlogLineIdx], model, owner);

  // Remove from Backlog.
  lines.splice(backlogLineIdx, 1);
  // Re-locate Active section (line numbers shifted if Backlog was above it —
  // it isn't in this file's current layout, but stay correct either way).
  const active2 = findSection(lines, 'Active');

  // Drop the Active placeholder line if present, insert the stamped line at
  // the end of the Active section's content.
  let insertAt = active2.end;
  for (let i = active2.start; i < active2.end; i++) {
    if (ACTIVE_PLACEHOLDER_RE.test(lines[i].trim())) {
      lines.splice(i, 1);
      insertAt = findSection(lines, 'Active').end;
      break;
    }
  }
  // Insert before the trailing blank line that precedes the next heading, if any.
  while (insertAt > active2.start && lines[insertAt - 1].trim() === '') insertAt--;
  lines.splice(insertAt, 0, stamped);

  fs.writeFileSync(TODO_PATH, lines.join('\n'));

  console.log(`Assigned ${code}: moved Backlog → Active.`);
  console.log(`  ${stamped.trim()}`);
  console.log('Remember to add it to docs/sprint/current.md\'s committed-items table.');
}

main();
