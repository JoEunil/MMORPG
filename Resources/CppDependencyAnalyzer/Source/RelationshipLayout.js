"use strict";

// Relationship placement uses one undirected connection per pair. The report keeps
// each original direction and relationship type when it draws the edges.
function createRelationshipLayout(nodes, edges, namespace = null, spacing = 1) {
  const records = new Map(nodes.map(node => [node.id, { node, neighbors: new Set() }]));
  for (const { from, to } of edges) {
    if (from === to || !records.has(from) || !records.has(to)) continue;
    records.get(from).neighbors.add(to);
    records.get(to).neighbors.add(from);
  }

  const visited = new Set();
  const components = [];
  const isolated = [];
  for (const [id, record] of records) {
    if (visited.has(id)) continue;
    visited.add(id);
    if (record.neighbors.size === 0) {
      isolated.push(record);
      continue;
    }
    const queue = [id];
    for (const current of queue) {
      for (const other of records.get(current).neighbors) {
        if (visited.has(other)) continue;
        visited.add(other);
        queue.push(other);
      }
    }
    components.push(queue.map(member => records.get(member)));
  }

  components.sort((a, b) => b.length - a.length);
  const blocks = components.map(placeComponent);
  if (isolated.length) {
    const columns = Math.max(1, Math.ceil(Math.sqrt(isolated.length * 1.8)));
    const field = isolated.map((record, index) => ({
      ...record, x: (index % columns) * 55, y: Math.floor(index / columns) * 47
    }));
    const isolatedTitles = { class: "연결 없는 클래스", group: "연결 없는 필터", project: "연결 없는 프로젝트" };
    blocks.push(makeBlock(field, isolatedTitles[nodes[0].kind], 46));
  }

  const area = blocks.reduce((sum, block) => sum + (block.width + 40) * (block.height + 40), 0);
  const shelfWidth = Math.max(Math.sqrt(area) * 1.43, ...blocks.map(block => block.width));
  const panels = [];
  const positions = new Map();
  let x = 0, y = 0, shelfHeight = 0;
  for (const block of blocks) {
    if (x && x + block.width > shelfWidth) {
      y += shelfHeight + 42;
      x = shelfHeight = 0;
    }
    if (block.title)
      panels.push({ x: x * spacing, y: y * spacing, width: block.width * spacing, height: block.height * spacing,
        title: block.title, nodes: block.points.map(point => point.node) });
    for (const point of block.points) {
      const degree = point.neighbors.size;
      const radius = nodeRadius(degree);
      positions.set(point.node.id, {
        x: (x + point.x) * spacing - radius, y: (y + point.y) * spacing - radius,
        width: radius * 2, height: radius * 2, radial: true, degree, hub: point.hub,
        external: namespace !== null && (point.node.namespace ?? "") !== namespace
      });
    }
    shelfHeight = Math.max(shelfHeight, block.height);
    x += block.width + 42;
  }
  return { positions, panels };
}

function placeComponent(members) {
  const leaves = members.filter(record => record.neighbors.size === 1);
  const core = members.filter(record => record.neighbors.size > 1)
    .sort((a, b) => b.neighbors.size - a.neighbors.size || a.node.name.localeCompare(b.node.name));
  if (!core.length) {
    return makeBlock(members.map((record, index) => ({ ...record, x: index * 105, y: 0 })));
  }

  const spiralAngle = Math.PI * (3 - Math.sqrt(5));
  const points = core.map((record, index) => {
    const distance = 47 * Math.sqrt(index);
    return {
      ...record, x: Math.cos(index * spiralAngle) * distance, y: Math.sin(index * spiralAngle) * distance,
      dx: 0, dy: 0, hub: index === 0 && members.length >= 5
    };
  });
  const coreIndex = new Map(points.map((point, index) => [point.node.id, index]));
  const springs = [];
  points.forEach((point, index) => {
    for (const id of point.neighbors) {
      const other = coreIndex.get(id);
      if (other !== undefined && other > index) springs.push([point, points[other]]);
    }
  });
  relaxCore(points, springs);
  separateCoreNodes(points);
  const grouped = members[0].node.kind === "group";
  const spacing = grouped ? 1.48 : points.length >= 40 ? 1.18 : 1;
  for (const point of points) { point.x *= spacing; point.y *= spacing; }

  if (leaves.length) {
    const turn = 2 * Math.PI;
    const spacing = turn / leaves.length;
    const radius = Math.max(Math.max(...points.map(point => Math.hypot(point.x, point.y))) + (grouped ? 75 : 110),
      leaves.length * 39 / turn);
    const attachments = leaves.map(record => {
      const parent = points[coreIndex.get(record.neighbors.values().next().value)];
      const angle = parent.x || parent.y ? (Math.atan2(parent.y, parent.x) + turn) % turn : null;
      return { record, angle };
    }).sort((a, b) => (a.angle ?? 0) - (b.angle ?? 0) || a.record.node.name.localeCompare(b.record.node.name));

    // Keep leaves ordered around their parent directions, with space between them.
    let phaseX = 0, phaseY = 0;
    attachments.forEach(({ angle }, index) => {
      if (angle === null) return;
      phaseX += Math.cos(angle - index * spacing);
      phaseY += Math.sin(angle - index * spacing);
    });
    const rotation = Math.atan2(phaseY, phaseX);
    attachments.forEach(({ record }, index) => {
      const angle = rotation + index * spacing;
      const distance = radius + (index % 3) * 7;
      points.push({ ...record, x: Math.cos(angle) * distance, y: Math.sin(angle) * distance });
    });
  }
  return makeBlock(points);
}

function relaxCore(points, springs) {
  // Bound pair checks in larger graphs while keeping the seed and every iteration deterministic.
  const stride = Math.max(1, Math.ceil(points.length * points.length / 180000));
  const iterations = 72;
  for (let frame = 0; frame < iterations; frame++) {
    for (const p of points) {
      const centerPull = .039 + Math.min(p.neighbors.size, 22) * .004;
      p.dx = -p.x * centerPull;
      p.dy = -p.y * centerPull;
    }
    for (let i = 0; i < points.length; i++) {
      const a = points[i];
      for (let j = i + 1 + (frame % stride); j < points.length; j += stride) {
        const b = points[j];
        const x = b.x - a.x, y = b.y - a.y;
        const distance = Math.max(1, Math.hypot(x, y));
        const clearance = nodeRadius(a.neighbors.size) + nodeRadius(b.neighbors.size) + 24;
        const force = (15200 / (distance * distance + 180) + Math.max(0, clearance - distance) * .48) / distance;
        a.dx -= x * force; a.dy -= y * force;
        b.dx += x * force; b.dy += y * force;
      }
    }
    for (const [a, b] of springs) {
      const x = b.x - a.x, y = b.y - a.y;
      const distance = Math.max(1, Math.hypot(x, y));
      const force = (distance - 104) * .041 / distance;
      a.dx += x * force; a.dy += y * force;
      b.dx -= x * force; b.dy -= y * force;
    }
    const cooling = 1 - frame / (iterations * 1.2);
    for (let i = 1; i < points.length; i++) {
      const p = points[i];
      const scale = Math.min(cooling, 23 / Math.max(1, Math.hypot(p.dx, p.dy)));
      p.x += p.dx * scale;
      p.y += p.dy * scale;
    }
  }
}

function separateCoreNodes(points) {
  const cellSize = 64;
  for (let pass = 0; pass < 30; pass++) {
    const cells = new Map();
    let moved = false;
    for (const point of points) {
      const column = Math.floor(point.x / cellSize);
      const row = Math.floor(point.y / cellSize);
      for (let x = column - 1; x <= column + 1; x++) {
        for (let y = row - 1; y <= row + 1; y++) {
          for (const other of cells.get(x + "," + y) ?? []) {
            const dx = point.x - other.x, dy = point.y - other.y;
            const length = Math.hypot(dx, dy);
            const gap = nodeRadius(point.neighbors.size) + nodeRadius(other.neighbors.size) + 9 - length;
            if (gap <= .25) continue;
            const ux = length > .001 ? dx / length : 1, uy = length > .001 ? dy / length : 0;
            const share = other === points[0] ? 1 : .5;
            point.x += ux * gap * share; point.y += uy * gap * share;
            other.x -= ux * gap * (1 - share); other.y -= uy * gap * (1 - share);
            moved = true;
          }
        }
      }
      const key = Math.floor(point.x / cellSize) + "," + Math.floor(point.y / cellSize);
      if (!cells.has(key)) cells.set(key, []);
      cells.get(key).push(point);
    }
    if (!moved) break;
  }
}

function nodeRadius(degree) {
  return Math.min(24, 8 + 3 * Math.sqrt(degree));
}

function makeBlock(points, title, topPadding = 25) {
  const left = Math.min(...points.map(point => point.x - nodeRadius(point.neighbors.size)));
  const right = Math.max(...points.map(point => point.x + nodeRadius(point.neighbors.size)));
  const top = Math.min(...points.map(point => point.y - nodeRadius(point.neighbors.size)));
  const bottom = Math.max(...points.map(point => point.y + nodeRadius(point.neighbors.size)));
  return {
    points: points.map(point => ({ ...point, x: point.x + 24 - left, y: point.y + topPadding - top })),
    width: Math.max(title ? 235 : 0, right - left + 48), height: bottom - top + topPadding + 30, title
  };
}
