#!/usr/bin/env node
'use strict';

// Information-retrieval search over docs/audit/ (BM25 ranking, no dependencies).
// Usage:
//   node scripts/audit-search.js "<natural-language query>" [-k N] [--all-status] [--scope audit|all]
//   node scripts/audit-search.js --hook        # reads a Claude Code hook JSON on stdin (see rules file)
//
// Why: audit entries are only useful if they are found when relevant work starts. This ranks
// entries against a free-text need (tokenised, stop-worded, lightly stemmed, BM25) instead of
// relying on exact grep keywords.
//
// Chunking: a plain docs/audit/<date>-<slug>.md is one chunk; a monthly digest
// docs/audit/<YYYY-MM>-log.md is split into one chunk per `## ` section.
// Status: `**Status:** Superseded by <ref>` or any entry's `**Supersedes:** <ref>[, <ref>]` marks
// the referenced entry Superseded (append-only: old files are never edited). A ref is a file
// basename (no .md) or a substring of a digest section heading. Superseded hits are hidden unless
// --all-status is given.
// --scope all also indexes docs/todo, docs/fix-log and docs/notes.

const fs = require('fs');
const path = require('path');

const ROOT = path.resolve(__dirname, '..');
const K1 = 1.5, B = 0.75, TITLE_BOOST = 3;

const STOP = new Set(('a an and are as at be but by for from has have how i in is it its of on or ' +
  'that the this to was we what when where which who why will with you do does did not no if so ' +
  'than then there these they their them can could should would may might about into over under ' +
  'nao la cua va cac nhung mot cho voi khi de duoc trong den tu').split(' '));

function stem(w) {
  if (w.length > 5 && w.endsWith('ing')) return w.slice(0, -3);
  if (w.length > 4 && w.endsWith('ed')) return w.slice(0, -2);
  if (w.length > 4 && w.endsWith('es')) return w.slice(0, -2);
  if (w.length > 3 && w.endsWith('s') && !w.endsWith('ss')) return w.slice(0, -1);
  return w;
}

function tokenize(text) {
  const t = text.toLowerCase().normalize('NFD').replace(/[̀-ͯ]/g, '').replace(/đ/g, 'd');
  const out = [];
  for (const raw of t.match(/[a-z0-9]+(?:[-_.][a-z0-9]+)*/g) || []) {
    const parts = raw.split(/[-_.]/);
    const cand = parts.length > 1 ? [raw, ...parts] : parts; // keep CODEs like anlz-05 whole too
    for (const c of cand) if (c.length > 1 && !STOP.has(c)) out.push(stem(c));
  }
  return out;
}

function listMd(dir) {
  const abs = path.join(ROOT, dir);
  if (!fs.existsSync(abs)) return [];
  return fs.readdirSync(abs).filter(f => f.endsWith('.md') && f !== 'README.md')
    .map(f => path.join(dir, f));
}

function chunksOf(rel, kind) {
  const text = fs.readFileSync(path.join(ROOT, rel), 'utf8');
  const base = path.basename(rel, '.md');
  const isDigest = kind === 'audit' && /^\d{4}-\d{2}-log$/.test(base);
  if (!isDigest) {
    const title = (text.match(/^#\s+(.+)$/m) || [, base])[1];
    return [{ id: base, file: rel, title, text, kind }];
  }
  const out = [];
  const parts = text.split(/^(?=## )/m).filter(s => s.startsWith('## '));
  for (const p of parts) {
    const title = p.split('\n')[0].replace(/^##\s+/, '');
    out.push({ id: `${base}#${title}`, file: rel, title, text: p, kind });
  }
  return out;
}

function loadCorpus(scope) {
  const sources = [['docs/audit', 'audit']];
  if (scope === 'all') sources.push(['docs/todo', 'todo'], ['docs/fix-log', 'fix-log'], ['docs/notes', 'notes']);
  const chunks = [];
  for (const [dir, kind] of sources)
    for (const f of listMd(dir)) chunks.push(...chunksOf(f, kind));
  return chunks;
}

function markSuperseded(chunks) {
  const refs = [];
  for (const c of chunks) {
    const sup = c.text.match(/^\*\*Supersedes:\*\*\s*(.+)$/m);
    if (sup) for (const r of sup[1].split(',')) refs.push(r.trim().replace(/\.md$/, '').replace(/`/g, ''));
    const st = c.text.match(/^\*\*Status:\*\*\s*Superseded\b/mi);
    c.superseded = !!st;
  }
  for (const c of chunks)
    for (const r of refs) if (r && (c.id === r || c.id.startsWith(r + '#') || c.title.includes(r))) c.superseded = true;
}

function rank(chunks, query) {
  const docs = chunks.map(c => {
    const toks = [...tokenize(c.text), ...Array(TITLE_BOOST - 1).fill(0).flatMap(() => tokenize(c.title))];
    const tf = new Map();
    for (const t of toks) tf.set(t, (tf.get(t) || 0) + 1);
    return { c, tf, len: toks.length };
  });
  const N = docs.length, avg = docs.reduce((s, d) => s + d.len, 0) / Math.max(N, 1);
  const df = new Map();
  for (const d of docs) for (const t of d.tf.keys()) df.set(t, (df.get(t) || 0) + 1);
  const q = [...new Set(tokenize(query))];
  const scored = docs.map(d => {
    let s = 0;
    for (const t of q) {
      const f = d.tf.get(t) || 0;
      if (!f) continue;
      const idf = Math.log(1 + (N - df.get(t) + 0.5) / (df.get(t) + 0.5));
      s += idf * (f * (K1 + 1)) / (f + K1 * (1 - B + B * d.len / avg));
    }
    return { c: d.c, score: s };
  });
  return scored.filter(x => x.score > 0).sort((a, b) => b.score - a.score);
}

function snippet(c, query) {
  const q = new Set(tokenize(query));
  let best = '', bestN = -1;
  for (const line of c.text.split('\n')) {
    const n = tokenize(line).filter(t => q.has(t)).length;
    if (n > bestN && line.trim() && !line.startsWith('#')) { bestN = n; best = line.trim(); }
  }
  return best.length > 160 ? best.slice(0, 157) + '...' : best;
}

function search(query, { k = 5, allStatus = false, scope = 'audit' } = {}) {
  const chunks = loadCorpus(scope);
  markSuperseded(chunks);
  const hits = rank(chunks, query).filter(h => allStatus || !h.c.superseded);
  return hits.slice(0, k).map(h => ({ ...h, snippet: snippet(h.c, query) }));
}

function format(hits) {
  return hits.map((h, i) => `${i + 1}. [${h.score.toFixed(2)}] ${h.c.kind}: ${h.c.title}` +
    `${h.c.superseded ? '  (SUPERSEDED)' : ''}\n   ${h.c.file}\n   > ${h.snippet}`).join('\n');
}

function main() {
  const argv = process.argv.slice(2);
  if (argv.includes('--hook')) {
    // UserPromptSubmit hook: stdin JSON {prompt}; print top audit hits as extra context.
    let raw = '';
    process.stdin.on('data', d => raw += d).on('end', () => {
      let prompt = '';
      try { prompt = JSON.parse(raw).prompt || ''; } catch { prompt = raw; }
      const hits = search(prompt, { k: 3 }).filter(h => h.score >= 4);
      if (hits.length) console.log('Relevant audit entries (BM25, active only):\n' + format(hits));
    });
    return;
  }
  const opts = { k: 5, allStatus: false, scope: 'audit' };
  const words = [];
  for (let i = 0; i < argv.length; i++) {
    if (argv[i] === '-k') opts.k = parseInt(argv[++i], 10);
    else if (argv[i] === '--all-status') opts.allStatus = true;
    else if (argv[i] === '--scope') opts.scope = argv[++i];
    else words.push(argv[i]);
  }
  if (!words.length) { console.error('usage: audit-search.js "<query>" [-k N] [--all-status] [--scope audit|all]'); process.exit(2); }
  const hits = search(words.join(' '), opts);
  console.log(hits.length ? format(hits) : 'No matching audit entries.');
}

if (require.main === module) main();
module.exports = { tokenize, search };
