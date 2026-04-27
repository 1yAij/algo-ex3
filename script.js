const graphData = {
  nodes: {
    A: {
      label: "A",
      color: "#bfdbfe",
      mapPath: "M32 32 L164 22 L190 118 L128 184 L36 148 Z",
      mapLabel: { x: 98, y: 102 },
      graphPosition: { x: 138, y: 84 }
    },
    B: {
      label: "B",
      color: "#bbf7d0",
      mapPath: "M164 22 L318 46 L296 150 L190 118 Z",
      mapLabel: { x: 236, y: 82 },
      graphPosition: { x: 258, y: 100 }
    },
    C: {
      label: "C",
      color: "#fde68a",
      mapPath: "M36 148 L128 184 L154 314 L42 298 L18 216 Z",
      mapLabel: { x: 84, y: 230 },
      graphPosition: { x: 104, y: 228 }
    },
    D: {
      label: "D",
      color: "#fecdd3",
      mapPath: "M128 184 L190 118 L296 150 L286 282 L154 314 Z",
      mapLabel: { x: 210, y: 224 },
      graphPosition: { x: 228, y: 206 }
    },
    E: {
      label: "E",
      color: "#ddd6fe",
      mapPath: "M296 150 L352 96 L384 194 L342 312 L286 282 Z",
      mapLabel: { x: 328, y: 220 },
      graphPosition: { x: 340, y: 220 }
    }
  },
  edges: [
    ["A", "B"],
    ["A", "C"],
    ["A", "D"],
    ["B", "D"],
    ["B", "E"],
    ["C", "D"],
    ["D", "E"]
  ]
};

const demoGraphData = {
  nodes: {
    0: { x: 112, y: 102 },
    1: { x: 232, y: 88 },
    2: { x: 170, y: 196 },
    3: { x: 302, y: 194 },
    4: { x: 382, y: 104 },
    5: { x: 430, y: 240 }
  },
  edges: [
    [0, 1],
    [0, 2],
    [1, 2],
    [1, 3],
    [2, 3],
    [3, 4],
    [4, 5]
  ],
  searchOrder: [0, 1, 4, 3, 2, 5],
  colors: {
    1: "#bfdbfe",
    2: "#bbf7d0",
    3: "#fde68a",
    4: "#ddd6fe"
  }
};

const demoState = {
  events: [],
  eventIndex: 0,
  timer: null,
  assignments: {},
  isRunning: false
};

const svgNamespace = "http://www.w3.org/2000/svg";

document.addEventListener("DOMContentLoaded", () => {
  renderMap();
  renderGraph();
  renderLegend();
  setupInteractions();
  renderDemoGraph();
  setupDemoControls();
  resetDemo();
});

// Render the simplified map using the same region data as the graph model.
function renderMap() {
  const container = document.getElementById("mapView");
  const svg = createSvg("0 0 400 340", "mapSvg");

  Object.entries(graphData.nodes).forEach(([id, node]) => {
    const path = createSvgElement("path", {
      d: node.mapPath,
      fill: node.color,
      class: "map-region",
      "data-node": id
    });

    const label = createSvgElement("text", {
      x: node.mapLabel.x,
      y: node.mapLabel.y,
      class: "map-label"
    });
    label.textContent = node.label;

    svg.append(path, label);
  });

  container.appendChild(svg);
}

// Render the graph coloring abstraction from the adjacency list.
function renderGraph() {
  const container = document.getElementById("graphView");
  const svg = createSvg("0 0 420 340", "graphSvg");

  graphData.edges.forEach(([source, target]) => {
    const sourceNode = graphData.nodes[source];
    const targetNode = graphData.nodes[target];

    const edge = createSvgElement("line", {
      x1: sourceNode.graphPosition.x,
      y1: sourceNode.graphPosition.y,
      x2: targetNode.graphPosition.x,
      y2: targetNode.graphPosition.y,
      class: "graph-edge",
      "data-edge": `${source}-${target}`,
      "data-source": source,
      "data-target": target
    });

    svg.appendChild(edge);
  });

  Object.entries(graphData.nodes).forEach(([id, node]) => {
    const group = createSvgElement("g", {
      class: "graph-node-group",
      "data-node": id
    });

    const circle = createSvgElement("circle", {
      cx: node.graphPosition.x,
      cy: node.graphPosition.y,
      r: 31,
      fill: node.color,
      class: "graph-node",
      "data-node": id
    });

    const label = createSvgElement("text", {
      x: node.graphPosition.x,
      y: node.graphPosition.y,
      class: "graph-label"
    });
    label.textContent = node.label;

    group.append(circle, label);
    svg.appendChild(group);
  });

  container.appendChild(svg);
}

// Build a compact color legend for report screenshots and quick reading.
function renderLegend() {
  const legend = document.getElementById("legend");

  Object.entries(graphData.nodes).forEach(([id, node]) => {
    const item = document.createElement("div");
    item.className = "legend-item";
    item.dataset.node = id;

    const swatch = document.createElement("span");
    swatch.className = "legend-swatch";
    swatch.style.backgroundColor = node.color;

    const text = document.createElement("span");
    text.textContent = `区域 ${id}`;

    item.append(swatch, text);
    legend.appendChild(item);
  });
}

// Connect map regions, graph nodes, and graph edges through shared node IDs.
function setupInteractions() {
  document.querySelectorAll(".map-region, .graph-node, .legend-item").forEach((element) => {
    element.addEventListener("mouseenter", () => highlightNodes([element.dataset.node]));
    element.addEventListener("mouseleave", clearHighlights);
  });

  document.querySelectorAll(".graph-edge").forEach((edge) => {
    edge.addEventListener("mouseenter", () => {
      highlightNodes([edge.dataset.source, edge.dataset.target], edge.dataset.edge);
    });
    edge.addEventListener("mouseleave", clearHighlights);
  });
}

function highlightNodes(nodeIds, activeEdge = null) {
  const activeSet = new Set(nodeIds);

  document.querySelectorAll(".map-region, .graph-node, .legend-item").forEach((element) => {
    const isActive = activeSet.has(element.dataset.node);
    element.classList.toggle("is-highlighted", isActive);
    element.classList.toggle("is-dimmed", !isActive);
  });

  document.querySelectorAll(".graph-edge").forEach((edge) => {
    const connectsActiveNodes = activeSet.has(edge.dataset.source) && activeSet.has(edge.dataset.target);
    const isActive = activeEdge ? edge.dataset.edge === activeEdge : connectsActiveNodes;
    edge.classList.toggle("is-highlighted", isActive);
    edge.classList.toggle("is-dimmed", !isActive);
  });
}

function clearHighlights() {
  document.querySelectorAll(".is-highlighted, .is-dimmed").forEach((element) => {
    element.classList.remove("is-highlighted", "is-dimmed");
  });
}

// Precompute the recursive search as a linear event list for deterministic playback.
function generateBacktrackingEvents(colorCount) {
  const events = [];
  const assignment = {};
  const stats = {
    visited: 0,
    backtracks: 0,
    checks: 0
  };

  function snapshot(extra = {}) {
    return {
      stats: { ...stats },
      assignment: { ...assignment },
      ...extra
    };
  }

  function solve(depth) {
    if (depth === demoGraphData.searchOrder.length) {
      events.push(snapshot({
        type: "success",
        vertex: "-",
        color: "-",
        depth,
        message: "所有顶点均已成功着色"
      }));
      return true;
    }

    const vertex = demoGraphData.searchOrder[depth];

    for (let color = 1; color <= colorCount; color += 1) {
      stats.visited += 1;
      events.push(snapshot({
        type: "try",
        vertex,
        color,
        depth,
        message: `尝试为顶点 ${vertex} 分配颜色 ${color}`
      }));

      const result = isSafe(vertex, color, assignment, stats);

      if (result.safe) {
        assignment[vertex] = color;
        events.push(snapshot({
          type: "assign",
          vertex,
          color,
          depth,
          message: `将颜色 ${color} 分配给顶点 ${vertex}`
        }));

        if (solve(depth + 1)) {
          return true;
        }

        delete assignment[vertex];
        stats.backtracks += 1;
        events.push(snapshot({
          type: "backtrack",
          vertex,
          color,
          depth,
          message: `从顶点 ${vertex} 回溯`
        }));
      } else {
        events.push(snapshot({
          type: "conflict",
          vertex,
          color,
          conflictWith: result.conflictWith,
          edge: result.edge,
          depth,
          message: `与顶点 ${result.conflictWith} 冲突，剪枝`
        }));
      }
    }

    return false;
  }

  const solved = solve(0);

  if (!solved) {
    events.push(snapshot({
      type: "failed",
      vertex: "-",
      color: "-",
      depth: 0,
      message: "当前颜色数下不存在合法着色方案"
    }));
  }

  return events;
}

// Check whether a candidate color is legal against already assigned neighbors.
function isSafe(vertex, color, assignment, stats) {
  for (const [source, target] of demoGraphData.edges) {
    const neighbor = source === vertex ? target : target === vertex ? source : null;

    if (neighbor === null || assignment[neighbor] === undefined) {
      continue;
    }

    stats.checks += 1;

    if (assignment[neighbor] === color) {
      return {
        safe: false,
        conflictWith: neighbor,
        edge: edgeId(vertex, neighbor)
      };
    }
  }

  return { safe: true };
}

// Draw the fixed 6-vertex graph used by the backtracking demonstration.
function renderDemoGraph() {
  const container = document.getElementById("demoGraphView");
  const svg = createSvg("0 0 540 330", "demoGraphSvg");

  demoGraphData.edges.forEach(([source, target]) => {
    const sourceNode = demoGraphData.nodes[source];
    const targetNode = demoGraphData.nodes[target];
    const edge = createSvgElement("line", {
      x1: sourceNode.x,
      y1: sourceNode.y,
      x2: targetNode.x,
      y2: targetNode.y,
      class: "demo-edge",
      "data-demo-edge": edgeId(source, target),
      "data-source": source,
      "data-target": target
    });
    svg.appendChild(edge);
  });

  Object.entries(demoGraphData.nodes).forEach(([id, node]) => {
    const group = createSvgElement("g", {
      "data-demo-node-group": id
    });

    const circle = createSvgElement("circle", {
      cx: node.x,
      cy: node.y,
      r: 31,
      fill: "#ffffff",
      class: "demo-node",
      "data-demo-node": id
    });

    const label = createSvgElement("text", {
      x: node.x,
      y: node.y,
      class: "demo-label"
    });
    label.textContent = id;

    group.append(circle, label);
    svg.appendChild(group);
  });

  container.appendChild(svg);
}

function setupDemoControls() {
  document.getElementById("startBtn").addEventListener("click", startAnimation);
  document.getElementById("pauseBtn").addEventListener("click", pauseAnimation);
  document.getElementById("stepBtn").addEventListener("click", stepAnimation);
  document.getElementById("resetBtn").addEventListener("click", resetDemo);
  document.getElementById("colorCountSelector").addEventListener("change", resetDemo);
  document.getElementById("speedSlider").addEventListener("input", () => {
    if (demoState.isRunning) {
      pauseAnimation();
      startAnimation();
    }
  });
}

// Apply one precomputed algorithm event to the SVG, statistics panel, and log.
function applyEvent(event) {
  clearDemoHighlights();
  demoState.assignments = { ...event.assignment };
  paintDemoAssignments();

  if (event.type === "try") {
    markCurrentNode(event.vertex);
  }

  if (event.type === "assign") {
    markCurrentNode(event.vertex);
  }

  if (event.type === "conflict") {
    markConflict(event.vertex, event.conflictWith, event.edge);
  }

  if (event.type === "backtrack") {
    markBacktrack(event.vertex);
  }

  updateStats(event);
  appendLog(event.message);
  setDemoStatus(event.type);
}

function startAnimation() {
  if (demoState.isRunning || demoState.eventIndex >= demoState.events.length) {
    return;
  }

  demoState.isRunning = true;
  setDemoStatus("running");
  updateDemoButtons();
  demoState.timer = window.setInterval(stepAnimation, getAnimationDelay());
}

function pauseAnimation() {
  if (demoState.timer) {
    window.clearInterval(demoState.timer);
    demoState.timer = null;
  }

  demoState.isRunning = false;
  updateDemoButtons();
}

function stepAnimation() {
  if (demoState.eventIndex >= demoState.events.length) {
    pauseAnimation();
    return;
  }

  const event = demoState.events[demoState.eventIndex];
  applyEvent(event);
  demoState.eventIndex += 1;
  document.getElementById("eventCounter").textContent = `${demoState.eventIndex} / ${demoState.events.length} 个事件`;

  if (demoState.eventIndex >= demoState.events.length) {
    pauseAnimation();
  }
}

function resetDemo() {
  pauseAnimation();
  const colorCount = Number(document.getElementById("colorCountSelector").value);
  demoState.events = generateBacktrackingEvents(colorCount);
  demoState.eventIndex = 0;
  demoState.assignments = {};

  clearDemoHighlights();
  paintDemoAssignments();
  clearEventLog();
  updateStats({
    type: "ready",
    vertex: "-",
    color: "-",
    depth: 0,
    stats: { visited: 0, backtracks: 0, checks: 0 }
  });
  setDemoStatus("ready");
  document.getElementById("eventCounter").textContent = `0 / ${demoState.events.length} 个事件`;
  updateDemoButtons();
}

function paintDemoAssignments() {
  document.querySelectorAll(".demo-node").forEach((node) => {
    const vertex = node.dataset.demoNode;
    const color = demoState.assignments[vertex];
    node.setAttribute("fill", color ? demoGraphData.colors[color] : "#ffffff");
  });
}

function markCurrentNode(vertex) {
  const node = document.querySelector(`[data-demo-node="${vertex}"]`);
  if (node) {
    node.classList.add("is-current");
  }
}

function markConflict(vertex, conflictWith, edge) {
  [vertex, conflictWith].forEach((id) => {
    const node = document.querySelector(`[data-demo-node="${id}"]`);
    if (node) {
      node.classList.add("is-conflict");
    }
  });

  const edgeElement = document.querySelector(`[data-demo-edge="${edge}"]`);
  if (edgeElement) {
    edgeElement.classList.add("is-conflict");
  }
}

function markBacktrack(vertex) {
  const node = document.querySelector(`[data-demo-node="${vertex}"]`);
  if (node) {
    node.classList.add("is-backtrack");
  }
}

function clearDemoHighlights() {
  document.querySelectorAll(".demo-node").forEach((node) => {
    node.classList.remove("is-current", "is-conflict", "is-backtrack");
  });
  document.querySelectorAll(".demo-edge").forEach((edge) => {
    edge.classList.remove("is-conflict", "is-active-edge");
  });
}

function updateStats(event) {
  document.getElementById("statVertex").textContent = event.vertex;
  document.getElementById("statColor").innerHTML = formatColorStat(event.color);
  document.getElementById("statVisited").textContent = event.stats.visited;
  document.getElementById("statBacktracks").textContent = event.stats.backtracks;
  document.getElementById("statChecks").textContent = event.stats.checks;
  document.getElementById("statDepth").textContent = event.depth;
  document.getElementById("statStatus").textContent = statusText(event.type);
}

function appendLog(message) {
  const eventLog = document.getElementById("eventLog");
  eventLog.querySelectorAll(".latest").forEach((item) => item.classList.remove("latest"));

  const item = document.createElement("li");
  item.className = "latest";
  item.textContent = message;
  eventLog.appendChild(item);
  eventLog.scrollTop = eventLog.scrollHeight;
}

function clearEventLog() {
  document.getElementById("eventLog").innerHTML = "";
}

function setDemoStatus(status) {
  const badge = document.getElementById("demoStatusBadge");
  badge.textContent = statusText(status);
  badge.className = `status-badge status-${statusClass(status)}`;
}

function updateDemoButtons() {
  const isComplete = demoState.eventIndex >= demoState.events.length;
  document.getElementById("startBtn").disabled = demoState.isRunning || isComplete;
  document.getElementById("pauseBtn").disabled = !demoState.isRunning;
  document.getElementById("stepBtn").disabled = demoState.isRunning || isComplete;
}

function getAnimationDelay() {
  const sliderValue = Number(document.getElementById("speedSlider").value);
  return 1650 - sliderValue;
}

function formatColorStat(color) {
  if (color === "-" || color === undefined) {
    return "-";
  }

  return `<span class="color-chip" style="background:${demoGraphData.colors[color]}"></span>${color}`;
}

function statusText(status) {
  const labels = {
    ready: "就绪",
    try: "运行中",
    assign: "合法",
    conflict: "冲突",
    backtrack: "回溯",
    success: "成功",
    failed: "失败",
    running: "运行中"
  };

  return labels[status] || "运行中";
}

function statusClass(status) {
  const classes = {
    ready: "ready",
    try: "running",
    assign: "valid",
    conflict: "conflict",
    backtrack: "backtrack",
    success: "success",
    failed: "failed",
    running: "running"
  };

  return classes[status] || "running";
}

function edgeId(source, target) {
  return [Number(source), Number(target)].sort((a, b) => a - b).join("-");
}

function createSvg(viewBox, id) {
  const svg = createSvgElement("svg", {
    id,
    viewBox,
    role: "img",
    "aria-hidden": "false"
  });
  return svg;
}

function createSvgElement(tagName, attributes) {
  const element = document.createElementNS(svgNamespace, tagName);

  Object.entries(attributes).forEach(([key, value]) => {
    element.setAttribute(key, value);
  });

  return element;
}
