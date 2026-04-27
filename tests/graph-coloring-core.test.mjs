import test from 'node:test';
import assert from 'node:assert/strict';

import {
  parseEdgeList,
  solveGraphColoring,
  createPresetGraphs,
} from '../graph-coloring-core.mjs';

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

test('createPresetGraphs exposes success and failure presets', () => {
  const presets = createPresetGraphs();

  assert.ok(presets.success.nodes.length >= 4);
  assert.ok(presets.failure.nodes.length >= 3);
});
