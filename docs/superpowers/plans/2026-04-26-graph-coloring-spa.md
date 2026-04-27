# Graph Coloring SPA Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a standalone `index.html` SPA that visualizes graph coloring backtracking with Degree Descending, MRV, and Forward Checking, with replayable trace states.

**Architecture:** Keep the shipped app in a single `index.html`, but develop the pure solver/parser logic in one small testable module first so TDD is practical. Once the core behavior is verified, wire the tested logic into the standalone page UI, renderer, and playback controller.

**Tech Stack:** HTML5, CSS3, Vanilla JavaScript, `vis-network` CDN, Node.js built-in `node:test` for verification

---

### Task 1: Create the testable core and lock parser behavior

**Files:**
- Create: `graph-coloring-core.mjs`
- Create: `tests/graph-coloring-core.test.mjs`

- [ ] **Step 1: Write the failing parser tests**

```js
import test from 'node:test';
import assert from 'node:assert/strict';
import { parseEdgeList } from '../graph-coloring-core.mjs';

test('parseEdgeList normalizes whitespace, removes duplicates, and filters self loops', () => {
  const parsed = parseEdgeList('0-1, 1-2, 2-2, 1-0, 2-3');
  assert.deepEqual(parsed.nodes, [0, 1, 2, 3]);
  assert.deepEqual(parsed.edges, [
    [0, 1],
    [1, 2],
    [2, 3],
  ]);
});

test('parseEdgeList rejects malformed tokens', () => {
  assert.throws(() => parseEdgeList('0-1, nope'), /边格式错误/);
});
```

- [ ] **Step 2: Run test to verify it fails**

Run: `node --test tests/graph-coloring-core.test.mjs`
Expected: FAIL with missing export or missing module errors for `parseEdgeList`

- [ ] **Step 3: Write minimal parser implementation**

```js
export function parseEdgeList(input) {
  const text = input.trim();
  if (!text) {
    throw new Error('图为空');
  }
  const seen = new Set();
  const edges = [];
  const nodes = new Set();

  for (const rawToken of text.split(',')) {
    const token = rawToken.trim();
    const match = token.match(/^(\d+)\s*-\s*(\d+)$/);
    if (!match) {
      throw new Error(`边格式错误: ${token}`);
    }
    const a = Number(match[1]);
    const b = Number(match[2]);
    if (a === b) continue;
    const [u, v] = a < b ? [a, b] : [b, a];
    const key = `${u}-${v}`;
    if (seen.has(key)) continue;
    seen.add(key);
    nodes.add(u);
    nodes.add(v);
    edges.push([u, v]);
  }

  if (!edges.length) {
    throw new Error('图为空');
  }

  return {
    nodes: [...nodes].sort((x, y) => x - y),
    edges,
  };
}
```

- [ ] **Step 4: Run test to verify it passes**

Run: `node --test tests/graph-coloring-core.test.mjs`
Expected: PASS for both parser tests

### Task 2: Add solver trace behavior under TDD

**Files:**
- Modify: `graph-coloring-core.mjs`
- Modify: `tests/graph-coloring-core.test.mjs`

- [ ] **Step 1: Write the failing solver tests**

```js
import { solveGraphColoring } from '../graph-coloring-core.mjs';

test('solveGraphColoring returns a successful trace for a simple path', () => {
  const trace = solveGraphColoring([0, 1, 2], [[0, 1], [1, 2]], 2, {
    staticDegree: false,
    mrv: false,
    forwardChecking: false,
  });

  assert.equal(trace.at(-1).actionType, 'SOLUTION');
  assert.deepEqual(trace.at(-1).assignments, { 0: 0, 1: 1, 2: 0 });
});

test('solveGraphColoring records forward checking failure on a triangle with two colors', () => {
  const trace = solveGraphColoring([0, 1, 2], [[0, 1], [1, 2], [0, 2]], 2, {
    staticDegree: false,
    mrv: true,
    forwardChecking: true,
  });

  assert.equal(trace.some((step) => step.actionType === 'FORWARD_CHECK_FAILED'), true);
  assert.equal(trace.at(-1).actionType, 'FAILURE');
});
```

- [ ] **Step 2: Run test to verify it fails**

Run: `node --test tests/graph-coloring-core.test.mjs`
Expected: FAIL because `solveGraphColoring` is not defined yet

- [ ] **Step 3: Write minimal solver implementation**

```js
export function solveGraphColoring(nodes, edges, numColors, strategies) {
  // Build adjacency, domains, deterministic ordering, and a trace recorder.
  // Emit SELECT_NODE, TRY_COLOR, COLORING, REJECT_COLOR, FORWARD_CHECK,
  // FORWARD_CHECK_FAILED, BACKTRACKING, SOLUTION, and FAILURE states.
}
```

Implementation notes:
- Use `0..numColors-1` color indexes internally
- Use deterministic ordering for ties
- When `mrv` is enabled, use current domain size first and static degree second
- Record every state as a deep copy so trace playback is stable

- [ ] **Step 4: Run test to verify it passes**

Run: `node --test tests/graph-coloring-core.test.mjs`
Expected: PASS for parser and solver tests

### Task 3: Add graph factories and safety checks

**Files:**
- Modify: `graph-coloring-core.mjs`
- Modify: `tests/graph-coloring-core.test.mjs`

- [ ] **Step 1: Write the failing factory tests**

```js
import { createPresetGraphs } from '../graph-coloring-core.mjs';

test('createPresetGraphs exposes success and failure presets', () => {
  const presets = createPresetGraphs();
  assert.ok(presets.success.nodes.length >= 4);
  assert.ok(presets.failure.nodes.length >= 3);
});
```

- [ ] **Step 2: Run test to verify it fails**

Run: `node --test tests/graph-coloring-core.test.mjs`
Expected: FAIL because `createPresetGraphs` is missing

- [ ] **Step 3: Write minimal factory implementation**

```js
export function createPresetGraphs() {
  return {
    success: { /* deterministic connected graph */ },
    failure: { /* deterministic graph that fails under default low color count */ },
  };
}
```

Also add:
- `createRandomGraph`
- `createGridGraph`
- `createCircularGraph`
- a state-count guard inside `solveGraphColoring` that throws `状态过多`

- [ ] **Step 4: Run test to verify it passes**

Run: `node --test tests/graph-coloring-core.test.mjs`
Expected: PASS for all core tests

### Task 4: Replace the page shell with the standalone SPA

**Files:**
- Modify: `index.html`

- [ ] **Step 1: Write the failing UI shell check**

Use a content check before editing:

Run: `rg -n "图着色工作台|Graph Coloring" index.html`
Expected: no matches in the current file

- [ ] **Step 2: Rewrite the page with embedded CSS and JS**

Include:
- three-column layout
- left control panel
- center graph canvas
- right analysis panel
- inline CSS variables and responsive styles
- inline JavaScript bootstrapping the app
- `vis-network` CDN script

- [ ] **Step 3: Run a static sanity check**

Run: `rg -n "vis-network|domain|Play|Forward Checking" index.html`
Expected: matches for all major UI features

### Task 5: Wire replay, renderer, and validation behavior

**Files:**
- Modify: `index.html`

- [ ] **Step 1: Write the failing behavior checks**

Use content checks to confirm missing handlers before wiring:

Run: `rg -n "stepBackward|applyState|runSolver|parseEdgeList" index.html`
Expected: partial or missing matches before the full integration is added

- [ ] **Step 2: Implement the UI controller**

Add:
- graph type switching
- preset buttons
- custom edge parsing
- color slider with palette preview
- strategy toggles
- play/pause/next/back/speed controls
- trace invalidation on config changes
- metrics, log, domains, and ranking updates

- [ ] **Step 3: Run the automated tests and one static page check**

Run:
- `node --test tests/graph-coloring-core.test.mjs`
- `node -e "const fs=require('fs'); const html=fs.readFileSync('index.html','utf8'); ['Play','Pause','Forward Checking','MRV','Domain Empty -> Backtrack'].forEach((x)=>{ if(!html.includes(x)) throw new Error('missing '+x);}); console.log('html-ok')"`

Expected:
- all tests pass
- `html-ok`

### Task 6: Final verification

**Files:**
- Verify only: `index.html`, `graph-coloring-core.mjs`, `tests/graph-coloring-core.test.mjs`

- [ ] **Step 1: Re-run the full verification commands**

Run:
- `node --test tests/graph-coloring-core.test.mjs`
- `node -e "const fs=require('fs'); const html=fs.readFileSync('index.html','utf8'); ['vis-network','FORWARD_CHECK_FAILED','Backtracks','Nodes Explored'].forEach((x)=>{ if(!html.includes(x)) throw new Error('missing '+x);}); console.log('html-keywords-ok')"`

Expected:
- test suite passes
- `html-keywords-ok`

- [ ] **Step 2: Manually review against the spec**

Checklist:
- standalone `index.html`
- fixed-layout graph playback
- success preset and failure preset
- random / grid / circular / custom graph input
- color slider and preview
- strategy combinations
- step-based replay with back button
- metrics, logs, domains, ranking
- clear error and unsat handling
