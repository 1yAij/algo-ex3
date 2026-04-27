const DEFAULT_MAX_STATES = 4000;

export function parseEdgeList(input) {
  const text = String(input ?? '').trim();
  if (!text) {
    throw new Error('图为空');
  }

  const seen = new Set();
  const nodes = new Set();
  const edges = [];

  for (const rawToken of text.split(',')) {
    const token = rawToken.trim();
    const match = token.match(/^(\d+)\s*-\s*(\d+)$/);
    if (!match) {
      throw new Error(`边格式错误: ${token}`);
    }

    const a = Number(match[1]);
    const b = Number(match[2]);
    if (a === b) {
      continue;
    }

    const [u, v] = a < b ? [a, b] : [b, a];
    const key = `${u}-${v}`;
    if (seen.has(key)) {
      continue;
    }

    seen.add(key);
    nodes.add(u);
    nodes.add(v);
    edges.push([u, v]);
  }

  if (!edges.length) {
    throw new Error('图为空');
  }

  return {
    nodes: [...nodes].sort((left, right) => left - right),
    edges,
  };
}

export function createPresetGraphs() {
  return {
    success: {
      name: 'success',
      nodes: [0, 1, 2, 3, 4],
      edges: [
        [0, 1],
        [0, 2],
        [1, 2],
        [1, 3],
        [2, 4],
        [3, 4],
      ],
      suggestedColors: 3,
    },
    failure: {
      name: 'failure',
      nodes: [0, 1, 2, 3],
      edges: [
        [0, 1],
        [1, 2],
        [0, 2],
        [0, 3],
        [1, 3],
      ],
      suggestedColors: 2,
    },
  };
}

export function createRandomGraph(nodeCount = 10, edgeProbability = 0.28) {
  const safeNodeCount = clampInteger(nodeCount, 4, 30);
  const probability = Math.max(0.08, Math.min(0.7, edgeProbability));
  const nodes = Array.from({ length: safeNodeCount }, (_, index) => index);
  const edges = [];

  for (let source = 0; source < safeNodeCount; source += 1) {
    for (let target = source + 1; target < safeNodeCount; target += 1) {
      if (Math.random() < probability) {
        edges.push([source, target]);
      }
    }
  }

  if (!edges.length) {
    edges.push([0, 1]);
  }

  return {
    name: 'random',
    nodes,
    edges,
    suggestedColors: 4,
  };
}

export function createGridGraph(rows = 3, cols = 4) {
  const safeRows = clampInteger(rows, 2, 5);
  const safeCols = clampInteger(cols, 2, 6);
  const nodes = [];
  const edges = [];

  const indexAt = (row, col) => row * safeCols + col;

  for (let row = 0; row < safeRows; row += 1) {
    for (let col = 0; col < safeCols; col += 1) {
      nodes.push(indexAt(row, col));
      if (col + 1 < safeCols) {
        edges.push([indexAt(row, col), indexAt(row, col + 1)]);
      }
      if (row + 1 < safeRows) {
        edges.push([indexAt(row, col), indexAt(row + 1, col)]);
      }
    }
  }

  return {
    name: 'grid',
    nodes,
    edges,
    suggestedColors: 3,
  };
}

export function createCircularGraph(nodeCount = 10) {
  const safeNodeCount = clampInteger(nodeCount, 4, 24);
  const nodes = Array.from({ length: safeNodeCount }, (_, index) => index);
  const edges = [];

  for (let index = 0; index < safeNodeCount; index += 1) {
    edges.push([index, (index + 1) % safeNodeCount]);
    if (index + 2 < safeNodeCount) {
      edges.push([index, (index + 2) % safeNodeCount]);
    }
  }

  return {
    name: 'circular',
    nodes,
    edges: dedupeEdges(edges),
    suggestedColors: safeNodeCount % 2 === 0 ? 3 : 4,
  };
}

export function solveGraphColoring(nodes, edges, numColors, strategies = {}) {
  const normalizedNodes = [...nodes].sort((left, right) => left - right);
  const normalizedEdges = dedupeEdges(edges);
  const adjacency = buildAdjacency(normalizedNodes, normalizedEdges);
  const degreeMap = buildDegreeMap(adjacency);
  const maxStates = strategies.maxStates ?? DEFAULT_MAX_STATES;
  const staticOrder = [...normalizedNodes].sort((left, right) => (
    degreeMap.get(right) - degreeMap.get(left) || left - right
  ));

  const assignments = Object.fromEntries(normalizedNodes.map((node) => [node, null]));
  const domains = Object.fromEntries(normalizedNodes.map((node) => [
    node,
    Array.from({ length: numColors }, (_, index) => index),
  ]));

  const trace = [];
  let visitedCount = 0;
  let backtrackCount = 0;

  const pushState = ({
    actionType,
    focusedNode = null,
    candidateColor = null,
    message = '',
    prunedNodes = [],
    failedNode = null,
    depth = 0,
  }) => {
    if (trace.length >= maxStates) {
      throw new Error('状态过多');
    }
    trace.push({
      stepIndex: trace.length,
      actionType,
      focusedNode,
      candidateColor,
      assignments: cloneAssignments(assignments),
      domains: cloneDomains(domains),
      nodeOrder: getNodeOrder(assignments, domains, staticOrder, degreeMap, strategies),
      visitedCount,
      backtrackCount,
      message,
      prunedNodes: [...prunedNodes],
      failedNode,
      depth,
    });
  };

  const search = (depth) => {
    const nextNode = pickNextNode(assignments, domains, staticOrder, degreeMap, strategies);
    if (nextNode === null) {
      pushState({
        actionType: 'SOLUTION',
        depth,
        message: '所有节点均已成功着色',
      });
      return true;
    }

    pushState({
      actionType: 'SELECT_NODE',
      focusedNode: nextNode,
      depth,
      message: `选择节点 ${nextNode}`,
    });

    const candidateColors = [...domains[nextNode]].sort((left, right) => left - right);
    visitedCount += 1;

    for (const color of candidateColors) {
      pushState({
        actionType: 'TRY_COLOR',
        focusedNode: nextNode,
        candidateColor: color,
        depth,
        message: `尝试将颜色 ${color + 1} 分配给节点 ${nextNode}`,
      });

      if (!isColorLegal(nextNode, color, assignments, adjacency)) {
        pushState({
          actionType: 'REJECT_COLOR',
          focusedNode: nextNode,
          candidateColor: color,
          depth,
          message: `颜色 ${color + 1} 与相邻节点冲突`,
        });
        continue;
      }

      assignments[nextNode] = color;
      const previousDomain = domains[nextNode];
      domains[nextNode] = [color];

      pushState({
        actionType: 'COLORING',
        focusedNode: nextNode,
        candidateColor: color,
        depth,
        message: `节点 ${nextNode} 着色为 ${color + 1}`,
      });

      let snapshot = null;
      let failedNode = null;
      let prunedNodes = [];

      if (strategies.forwardChecking) {
        snapshot = cloneDomains(domains);
        ({ failedNode, prunedNodes } = applyForwardChecking(nextNode, color, assignments, domains, adjacency));

        pushState({
          actionType: failedNode === null ? 'FORWARD_CHECK' : 'FORWARD_CHECK_FAILED',
          focusedNode: nextNode,
          candidateColor: color,
          depth,
          prunedNodes,
          failedNode,
          message: failedNode === null
            ? `前向检查更新了 ${prunedNodes.length} 个邻居`
            : `节点 ${failedNode} 的 domain 为空，触发回溯`,
        });
      }

      if (failedNode === null && search(depth + 1)) {
        return true;
      }

      if (snapshot) {
        restoreDomains(domains, snapshot);
      }
      assignments[nextNode] = null;
      domains[nextNode] = previousDomain.filter((candidate) => candidate !== undefined);
      backtrackCount += 1;

      pushState({
        actionType: 'BACKTRACKING',
        focusedNode: nextNode,
        candidateColor: color,
        depth,
        message: `撤销节点 ${nextNode} 的颜色 ${color + 1}`,
      });
    }

    return false;
  };

  const solved = search(0);

  if (!solved) {
    pushState({
      actionType: 'FAILURE',
      depth: 0,
      message: '当前颜色数量下无解',
    });
  }

  return trace;
}

function clampInteger(value, min, max) {
  return Math.max(min, Math.min(max, Math.round(value)));
}

function dedupeEdges(edges) {
  const seen = new Set();
  const normalized = [];

  for (const [rawSource, rawTarget] of edges) {
    if (rawSource === rawTarget) {
      continue;
    }
    const source = Math.min(rawSource, rawTarget);
    const target = Math.max(rawSource, rawTarget);
    const key = `${source}-${target}`;
    if (seen.has(key)) {
      continue;
    }
    seen.add(key);
    normalized.push([source, target]);
  }

  return normalized;
}

function buildAdjacency(nodes, edges) {
  const adjacency = new Map(nodes.map((node) => [node, new Set()]));

  for (const [source, target] of edges) {
    adjacency.get(source)?.add(target);
    adjacency.get(target)?.add(source);
  }

  return adjacency;
}

function buildDegreeMap(adjacency) {
  return new Map([...adjacency.entries()].map(([node, neighbors]) => [node, neighbors.size]));
}

function pickNextNode(assignments, domains, staticOrder, degreeMap, strategies) {
  const uncolored = staticOrder.filter((node) => assignments[node] === null);
  if (!uncolored.length) {
    return null;
  }

  if (!strategies.mrv) {
    return strategies.staticDegree ? uncolored[0] : Object.keys(assignments)
      .map(Number)
      .filter((node) => assignments[node] === null)
      .sort((left, right) => left - right)[0];
  }

  return [...uncolored].sort((left, right) => (
    domains[left].length - domains[right].length ||
    (strategies.staticDegree ? degreeMap.get(right) - degreeMap.get(left) : 0) ||
    left - right
  ))[0];
}

function getNodeOrder(assignments, domains, staticOrder, degreeMap, strategies) {
  const base = staticOrder.filter((node) => assignments[node] === null);
  if (!strategies.mrv) {
    return base;
  }

  return [...base].sort((left, right) => (
    domains[left].length - domains[right].length ||
    (strategies.staticDegree ? degreeMap.get(right) - degreeMap.get(left) : 0) ||
    left - right
  ));
}

function isColorLegal(node, color, assignments, adjacency) {
  for (const neighbor of adjacency.get(node) ?? []) {
    if (assignments[neighbor] === color) {
      return false;
    }
  }
  return true;
}

function applyForwardChecking(node, color, assignments, domains, adjacency) {
  const prunedNodes = [];
  let failedNode = null;

  for (const neighbor of adjacency.get(node) ?? []) {
    if (assignments[neighbor] !== null) {
      continue;
    }

    const nextDomain = domains[neighbor].filter((candidate) => candidate !== color);
    if (nextDomain.length !== domains[neighbor].length) {
      prunedNodes.push(neighbor);
      domains[neighbor] = nextDomain;
    }
    if (!domains[neighbor].length) {
      failedNode = neighbor;
      break;
    }
  }

  return { failedNode, prunedNodes };
}

function restoreDomains(targetDomains, sourceDomains) {
  for (const [node, domain] of Object.entries(sourceDomains)) {
    targetDomains[node] = [...domain];
  }
}

function cloneAssignments(assignments) {
  return Object.fromEntries(Object.entries(assignments));
}

function cloneDomains(domains) {
  return Object.fromEntries(
    Object.entries(domains).map(([node, domain]) => [node, [...domain]])
  );
}
