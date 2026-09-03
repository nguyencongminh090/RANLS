#!/usr/bin/env node
'use strict';

// Antigravity Stop-hook adapter for YixinBoard tracking-sync discipline.
// Conforms to Antigravity Lifecycle Hooks specification (agy-customizations).
//
// Reads Stop event context on stdin:
//   { "executionNum": 1, "terminationReason": "model_stop", ... }
//
// Runs project script: ../scripts/check-tracking-sync.js (diff mode)
//
// Outputs JSON to stdout:
//   Clean: {} (allows stop)
//   Drift: { "decision": "continue", "reason": "<explanation>" } (blocks stop)

const fs = require('fs');
const path = require('path');
const { execSync } = require('child_process');

function readStdin() {
  if (process.stdin.isTTY) return null;
  try {
    const raw = fs.readFileSync(0, 'utf8');
    return raw ? JSON.parse(raw) : null;
  } catch (e) {
    return null;
  }
}

function main() {
  const input = readStdin();

  // Prevent infinite loop if already blocked once this turn
  if (input && typeof input.executionNum === 'number' && input.executionNum > 1) {
    process.stdout.write('{}');
    process.exit(0);
  }

  const scriptPath = path.resolve(__dirname, '..', '..', 'scripts', 'check-tracking-sync.js');
  if (!fs.existsSync(scriptPath)) {
    process.stdout.write('{}');
    process.exit(0);
  }

  try {
    // Run diff mode check against git HEAD
    execSync(`node "${scriptPath}"`, {
      encoding: 'utf8',
      stdio: ['ignore', 'pipe', 'pipe'],
    });
    // Exit code 0 means in sync (no newly marked ✅ without detail completion marker)
    process.stdout.write('{}');
    process.exit(0);
  } catch (err) {
    const stdout = (err.stdout || '').trim();
    const stderr = (err.stderr || '').trim();
    const details = stdout || stderr || 'Newly marked ✅ item in TODO.md missing detail-file marker';

    const response = {
      decision: 'continue',
      reason:
        `Tracking-sync check blocked stop: TODO.md marks item(s) as newly done, but matching ` +
        `detail file in docs/todo/ has no completion marker (per .agents/rules/tracking-files.md ` +
        `"Index/detail sync" rule). ${details}. Please add a "**Status:** ✅ <DONE|FIXED|CLOSED|VERIFIED>" ` +
        `line (with summary/test notes) to each detail file before finishing, or remove the ✅ from TODO.md.`,
    };

    process.stdout.write(JSON.stringify(response));
    process.exit(0);
  }
}

main();
