"use strict";

const get = id => document.getElementById(id);
const data = JSON.parse(get("data").textContent);
const byId = new Map(data.nodes.map(node => [node.id, node]));
const classes = data.nodes.filter(node => node.kind === "class");
const projects = data.nodes.filter(node => node.kind === "project");
const libraries = data.nodes.filter(node => node.kind === "library");
const projectEdges = data.edges.filter(edge => edge.kind === "reference" || edge.kind === "external");
const classEdges = data.edges.filter(edge => edge.kind === "inherits" || edge.kind === "field" || edge.kind === "call");
const types = {
  reference: { name: "프로젝트 참조", color: "#a8c6e2" },
  external: { name: "외부 라이브러리", color: "#f4bd7a" },
  inherits: { name: "상속", color: "#72c5ff" },
  field: { name: "멤버 타입", color: "#60dbbd" },
  call: { name: "정적 호출", color: "#d3b1fc" },
  aggregate: { name: "필터 간 연결", color: "#acd1f3" },
  boundary: { name: "Interface 경계", color: "#62efb1" }
};
const classKinds = ["inherits", "field", "call"];
const namespaceOf = node => node.namespace ?? "";
const namespaceTitle = name => name || "(전역)";
const groups = groupByNamespace(classes);
const namespacePalette = ["#68cbff", "#7de0ba", "#f4be72", "#c6a8ff", "#f49bbb", "#a8da74", "#77dfde", "#eda890", "#a4b9fc"];
const namespaceColors = new Map([...groups.keys()].sort().map((name, index) =>
  [name, namespacePalette[index % namespacePalette.length]]));
const hasStudioFilters = classes.some(node => node.visualStudioFilter);
const projectOf = node => node?.visualStudioFilter?.project ?? node?.project ?? "";
const projectGroups = groupByNamespace(classes, projectOf);
const projectColors = distinctColors(projectGroups.keys());
const filterGraph = hasStudioFilters ? createFilterGroups() : { nodes: [], memberToGroup: new Map() };
const interfaceInheritance = new Set(classEdges.filter(edge => {
  const from = projectOf(byId.get(edge.from)), to = projectOf(byId.get(edge.to));
  return edge.kind === "inherits" && from && to && from !== to && isInterface(byId.get(edge.to));
}));
const state = {
  view: classes.length || !projects.length ? "class" : "project",
  namespace: [...groups].sort((a, b) => b[1].length - a[1].length)[0]?.[0] ?? "",
  external: false, hidden: new Set(), kind: "", selected: "", query: "", direction: "", focused: false, arrangement: "namespace", cardViews: new Set(["project"]),
  hiddenProjects: new Set(), hiddenFilters: new Set(), interfaceOnly: false, filterInterfaceOnly: false
};
const card = { width: 219, height: 37, stepX: 255, stepY: 62 };
const fullLayout = classLayout(classes);
const graph = get("graph");
const world = svg("g", { id: "world" });
let visibleNodes = [];
let visibleEdges = [];
let scopeNodes = [];
let scopeEdges = [];
let layout = fullLayout;
let layoutKey = "";
const movedNodes = new Map();
let drawnPanels = [];
let bounds = null;
let nodeElements = new Map();
let edgeElements = [];
let routes = new Map();
let pickedEdge = null;
let evidenceRows = new Map();
let camera = { x: 0, y: 0, scale: 1 };
let overviewCamera = null;
let viewport = { width: 0, height: 0 };
let edgesAtReadableZoom = false;
let drag = null;
let suppressClick = false;

init();

function init() {
  get("origin").textContent = data.directory + " · " + data.configuration;
  get("method-projects").hidden = !projects.length;
  get("method-filters").hidden = !hasStudioFilters;
  get("method-external").hidden = !libraries.length;
  const missed = data.sources - data.parsed - data.parsedWithErrors;
  get("coverage").textContent = "분석 성공 " + data.parsed + "/" + data.sources + " · 오류 포함 "
    + data.parsedWithErrors + " · 구문 분석 결과 없음 " + missed
    + (data.excludedTests.length ? " · 감지한 테스트 제외 " + data.excludedTests.length : "");
  get("coverage").classList.toggle("issue", data.failures.length > 0 || data.sources === 0);
  graph.append(markers(), world);
  const namespaceChoices = [...groups.keys()].sort((a, b) => a.localeCompare(b))
    .map(name => [name, namespaceTitle(name)]);
  setOptions(get("namespace-select"), namespaceChoices);
  get("namespace-select").value = state.namespace;
  document.querySelector('[data-tab="group"]').hidden = !hasStudioFilters;
  get("layout-mode").querySelector('[value="project"]').disabled = !projects.length;
  showIssues();
  bindControls();
  changeView(state.view);
  new ResizeObserver(resizeViewport).observe(graph);
}

function bindControls() {
  for (const tab of document.querySelectorAll("nav button"))
    tab.addEventListener("click", () => changeView(tab.dataset.tab));
  get("search").addEventListener("input", event => {
    state.query = event.target.value.trim().toLowerCase();
    updateSearch();
    updateHighlights();
  });
  get("nodes").addEventListener("change", event => choose(event.target.value, true));
  get("kind").addEventListener("change", event => {
    state.kind = event.target.value;
    renderFilteredMap();
  });
  for (const button of document.querySelectorAll("[data-presentation]")) {
    button.addEventListener("click", () => {
      const cards = button.dataset.presentation === "cards";
      if (cards === state.cardViews.has(state.view)) return;
      if (cards) state.cardViews.add(state.view);
      else state.cardViews.delete(state.view);
      clearFocus();
      refreshGroupChecks();
      renderMap(true);
    });
  }
  get("layout-mode").addEventListener("change", event => {
    state.arrangement = event.target.value;
    refreshGroupChecks();
    renderMap(true);
  });
  get("interfaces").addEventListener("change", event => {
    if (state.view === "group") state.filterInterfaceOnly = event.target.checked;
    else {
      state.interfaceOnly = event.target.checked;
      if (state.interfaceOnly) state.kind = get("kind").value = "";
    }
    renderMap(true);
  });
  get("namespace-select").addEventListener("change", event => {
    state.namespace = event.target.value;
    state.selected = "";
    clearFocus();
    renderMap(true);
  });
  get("external").addEventListener("change", event => {
    state.external = event.target.checked;
    renderFilteredMap();
  });
  get("show-all").addEventListener("click", () => toggleAllGroups(true));
  get("hide-all").addEventListener("click", () => toggleAllGroups(false));
  get("fit").addEventListener("click", () => fit());
  get("locate").addEventListener("click", locate);
  get("toggle-filters").addEventListener("click", () => togglePanel("filters"));
  get("toggle-inspector").addEventListener("click", () => togglePanel("inspector"));
  get("focus").addEventListener("click", toggleFocus);
  get("direction").addEventListener("change", event => {
    state.direction = event.target.value;
    pickedEdge = null;
    if (state.focused) renderMap(true);
    else {
      updateHighlights();
      showEvidence();
    }
  });
  get("reset-layout").addEventListener("click", () => {
    movedNodes.delete(layoutKey);
    renderMap(true);
  });
  get("zoom-in").addEventListener("click", () => zoom(1.32, graph.clientWidth / 2, graph.clientHeight / 2));
  get("zoom-out").addEventListener("click", () => zoom(1 / 1.32, graph.clientWidth / 2, graph.clientHeight / 2));
  graph.addEventListener("wheel", event => {
    event.preventDefault();
    if (drag?.moved) return;
    const rect = graph.getBoundingClientRect();
    const multiplier = Math.exp(-Math.max(-180, Math.min(180, event.deltaY)) * .0025);
    zoom(multiplier, event.clientX - rect.left, event.clientY - rect.top);
  }, { passive: false });
  graph.addEventListener("pointerdown", startDrag);
  graph.addEventListener("pointermove", moveDrag);
  graph.addEventListener("pointerup", endDrag);
  graph.addEventListener("pointercancel", endDrag);
  graph.addEventListener("lostpointercapture", endDrag);
  graph.addEventListener("pointerleave", event => {
    if (!drag?.moved) endDrag(event);
  });
  graph.addEventListener("click", event => {
    if (suppressClick) event.stopImmediatePropagation();
  }, true);
}

function togglePanel(id) {
  const panel = get(id);
  panel.hidden = !panel.hidden;
  get("toggle-" + id).setAttribute("aria-pressed", !panel.hidden);
}

function toggleFocus() {
  if (!state.selected) return;
  if (state.focused) {
    const previousCamera = overviewCamera;
    clearFocus();
    if (previousCamera?.width === graph.clientWidth && previousCamera.height === graph.clientHeight)
      camera = previousCamera.camera;
    else {
      renderMap(true);
      return;
    }
    renderMap(false);
    updateCamera();
  } else {
    overviewCamera = { camera: { ...camera }, width: graph.clientWidth, height: graph.clientHeight };
    state.focused = true;
    renderMap(true);
  }
}

function clearFocus() {
  state.focused = false;
  overviewCamera = null;
}

function changeView(view) {
  const previous = byId.get(state.selected);
  if (view === "namespace" && previous?.kind === "class")
    state.namespace = namespaceOf(previous);
  state.view = view;
  state.selected = state.query = state.kind = state.direction = "";
  pickedEdge = null;
  clearFocus();
  get("direction").value = "";
  get("search").value = "";
  for (const tab of document.querySelectorAll("nav button"))
    tab.setAttribute("aria-pressed", tab.dataset.tab === view);
  get("explorer").hidden = view === "diagnostics";
  get("panels").hidden = view === "diagnostics";
  get("diagnostics").hidden = view !== "diagnostics";
  if (view === "diagnostics") return;

  get("namespace-controls").hidden = view !== "namespace";
  get("namespace-select").value = state.namespace;
  get("namespace-select").disabled = groups.size === 0;
  get("relation-control").hidden = view === "project";
  setOptions(get("kind"), [["", "모든 관계"], ...classKinds.map(kind => [kind, types[kind].name])]);
  get("diagram-title").textContent = {
    class: "전체 클래스 맵", namespace: "namespace별 클래스 맵", project: "프로젝트 의존성", group: "Visual Studio 필터 지도"
  }[view];
  refreshGroupChecks();
  renderMap(true);
}

function renderLegend() {
  const grouped = state.view === "group";
  const legend = state.view === "project" ? (libraries.length ? ["reference", "external"] : ["reference"]) : grouped ? ["aggregate", "boundary"]
    : projectClassView() && hasStudioFilters ? [...classKinds, "boundary"] : classKinds;
  get("legend").replaceChildren(...legend.map(kind => {
    const item = document.createElement("span");
    const line = document.createElement("i");
    line.style.borderColor = types[kind].color;
    line.className = kind;
    const label = kind !== "boundary" ? types[kind].name
      : grouped ? "Interface 참조 포함" : "프로젝트 간 Interface 상속";
    item.append(line, label);
    return item;
  }), document.createTextNode(grouped ? "숫자는 연결된 클래스 쌍 수 · 선을 눌러 근거 보기"
    : "화살표는 의존 대상 방향 · 노드를 끌어 이동 · 선을 눌러 근거 보기"));
}

function relationshipView() { return !state.cardViews.has(state.view); }
function projectClassView() { return state.view === "class" && !relationshipView() && state.arrangement === "project"; }

function hiddenGroups() {
  return state.view === "group" ? state.hiddenFilters : projectClassView() ? state.hiddenProjects : state.hidden;
}

function refreshGroupChecks() {
  get("groups").hidden = state.view !== "class" && state.view !== "group";
  if (get("groups").hidden) return;

  get("group-heading").textContent = state.view === "group" ? "표시할 필터"
    : projectClassView() ? "표시할 프로젝트" : "표시할 namespace";
  const choices = state.view === "group" ? filterGraph.nodes.map(node => [node.id, node.name, node.count, node.color])
    : projectClassView() ? [...projectGroups].map(([id, members]) => [id, projectName(id), members.length, projectColors.get(id)])
    : [...groups].map(([name, members]) => [name, namespaceTitle(name), members.length, namespaceColors.get(name)]);
  choices.sort((a, b) => a[1].localeCompare(b[1]));
  const list = get("group-list");
  list.replaceChildren();
  for (const [name, title, count, color] of choices) {
    const label = document.createElement("label");
    const checkbox = document.createElement("input");
    const text = document.createElement("span");
    const dot = document.createElement("span");
    dot.className = "namespace-dot";
    dot.style.backgroundColor = color;
    label.className = "check";
    checkbox.type = "checkbox";
    checkbox.value = name;
    checkbox.checked = !hiddenGroups().has(name);
    text.textContent = title + " (" + count + ")";
    label.append(checkbox, dot, text);
    list.append(label);
    checkbox.addEventListener("change", () => {
      if (checkbox.checked) hiddenGroups().delete(name);
      else hiddenGroups().add(name);
      renderFilteredMap();
    });
  }
}

function toggleAllGroups(visible) {
  const hidden = hiddenGroups();
  hidden.clear();
  for (const checkbox of get("group-list").querySelectorAll("input")) {
    checkbox.checked = visible;
    if (!visible) hidden.add(checkbox.value);
  }
  renderFilteredMap();
}

function renderFilteredMap() {
  const changesPlacement = state.view === "group" || relationshipView() || projectClassView();
  if (changesPlacement) overviewCamera = null;
  renderMap(changesPlacement || bounds === null);
}

function projectName(id) { return byId.get(id)?.name ?? "(프로젝트 미지정)"; }

function distinctColors(ids) {
  const sorted = [...ids].sort();
  return new Map(sorted.map((id, index) => [id, "hsl(" + Math.round(index * 137.508 % 360) + " 76% 69%)"]));
}

function isInterface(node) {
  return node?.visualStudioFilter?.name.split(/[\\/]/).some(part => part.toLowerCase() === "interface") ?? false;
}

function createFilterGroups() {
  const memberToGroup = new Map();
  const groups = new Map();
  for (const node of classes) {
    const project = projectOf(node);
    const filter = node.visualStudioFilter?.name ?? "(필터 미지정)";
    const id = "filter:" + JSON.stringify([project, filter]);
    if (!groups.has(id)) groups.set(id, {
      id, name: projectName(project) + " / " + filter, project, projectLabel: projectName(project), filterName: filter, kind: "group", count: 0
    });
    groups.get(id).count++;
    memberToGroup.set(node.id, id);
  }
  const colors = distinctColors(groups.keys());
  const nodes = [...groups.values()].map(group => ({ ...group, color: colors.get(group.id) }));
  for (const node of nodes) byId.set(node.id, node);
  return { nodes, memberToGroup };
}

function aggregateFilters(edges) {
  const groups = filterGraph.nodes.filter(node => !state.hiddenFilters.has(node.id));
  const visible = new Set(groups.map(node => node.id));
  const links = new Map();
  for (const edge of edges) {
    const from = filterGraph.memberToGroup.get(edge.from);
    const to = filterGraph.memberToGroup.get(edge.to);
    if (from === to || !visible.has(from) || !visible.has(to)) continue;
    const boundary = isInterface(byId.get(edge.to));
    if (state.filterInterfaceOnly && !boundary) continue;
    const key = JSON.stringify([from, to]);
    if (!links.has(key)) links.set(key, { from, to, kind: "aggregate", relations: [], pairs: new Set(), interfaces: 0 });
    const link = links.get(key);
    link.relations.push(edge);
    link.pairs.add(JSON.stringify([edge.from, edge.to]));
    if (boundary) link.interfaces++;
  }

  const groupEdges = [...links.values()].map(({ pairs, ...edge }) => ({ ...edge, pairCount: pairs.size }));
  const connected = new Set(groupEdges.flatMap(edge => [edge.from, edge.to]));
  return { nodes: state.filterInterfaceOnly ? groups.filter(node => connected.has(node.id)) : groups, edges: groupEdges };
}

function renderMap(resetCamera) {
  for (const button of document.querySelectorAll("[data-presentation]"))
    button.setAttribute("aria-pressed", (button.dataset.presentation === "relationships") === relationshipView());
  get("layout-control").hidden = state.view !== "class" || relationshipView();
  const inProjects = projectClassView();
  const showInterfaces = state.view === "group" || (inProjects && hasStudioFilters);
  get("interface-control").hidden = !showInterfaces;
  get("interface-label").textContent = state.view === "group" ? "Interface 연결만"
    : "프로젝트 간 Interface 상속만";
  get("interfaces").checked = state.view === "group" ? state.filterInterfaceOnly : state.interfaceOnly;
  get("kind").disabled = inProjects && state.interfaceOnly;
  renderLegend();
  if (state.view === "project") {
    scopeNodes = [...projects, ...libraries];
    scopeEdges = projectEdges;
  } else if (state.view === "group") {
    const result = aggregateFilters(state.kind ? classEdges.filter(edge => edge.kind === state.kind) : classEdges);
    scopeNodes = result.nodes;
    scopeEdges = result.edges;
  } else {
    const matchingEdges = inProjects && state.interfaceOnly ? classEdges.filter(edge => interfaceInheritance.has(edge))
      : state.kind ? classEdges.filter(edge => edge.kind === state.kind) : classEdges;
    const internal = state.view === "class"
      ? classes.filter(node => inProjects ? !state.hiddenProjects.has(projectOf(node)) : !state.hidden.has(namespaceOf(node)))
      : groups.get(state.namespace) ?? [];
    const ids = new Set(internal.map(node => node.id));
    if (state.view === "namespace" && state.external) {
      // Take one hop from the selected namespace, without following external-to-external edges.
      scopeEdges = matchingEdges.filter(edge => ids.has(edge.from) || ids.has(edge.to));
      const external = new Set(scopeEdges.flatMap(edge => [edge.from, edge.to]).filter(id => !ids.has(id)));
      scopeNodes = [...internal, ...[...external].map(id => byId.get(id))];
    } else {
      scopeEdges = matchingEdges.filter(edge => ids.has(edge.from) && ids.has(edge.to));
      scopeNodes = internal;
      if (inProjects && state.interfaceOnly) {
        const connected = new Set(scopeEdges.flatMap(edge => [edge.from, edge.to]));
        scopeNodes = scopeNodes.filter(node => connected.has(node.id));
      }
    }
  }
  if (state.selected && !scopeNodes.some(node => node.id === state.selected)) {
    state.selected = state.direction = "";
    get("direction").value = "";
    clearFocus();
  }

  visibleNodes = scopeNodes;
  visibleEdges = scopeEdges;
  let autoLayout;
  const mapKey = state.view === "namespace" ? ["namespace", state.namespace] : [state.view];
  mapKey.push(relationshipView() ? "relationships" : "cards");
  if (state.view === "group") mapKey.push(state.filterInterfaceOnly);
  if (inProjects) mapKey.push("project", state.interfaceOnly);
  if (state.focused) {
    visibleEdges = selectedEdges();
    const nearby = new Set([state.selected, ...visibleEdges.flatMap(edge => [edge.from, edge.to])]);
    visibleNodes = scopeNodes.filter(node => nearby.has(node.id));
    autoLayout = focusLayout();
    mapKey.push(state.selected, state.direction);
  } else if (relationshipView()) {
    const spacing = state.view === "group" ? 1.65 : state.view === "project" ? 1.35 : 1;
    autoLayout = createRelationshipLayout(scopeNodes, scopeEdges, state.view === "namespace" ? state.namespace : null, spacing);
  } else if (inProjects) {
    autoLayout = state.interfaceOnly ? projectBoundaryLayout(scopeNodes, scopeEdges) : classLayout(scopeNodes, null, projectOf);
  } else if (state.view === "project") {
    autoLayout = projectLayout();
  } else if (state.view === "group") {
    autoLayout = classLayout(scopeNodes, null, projectOf);
  } else {
    autoLayout = state.view === "class" ? fullLayout : classLayout(scopeNodes, state.namespace);
  }

  get("layout-mode").disabled = state.focused;
  layoutKey = JSON.stringify(mapKey);
  layout = { panels: autoLayout.panels, positions: new Map(autoLayout.positions) };
  for (const [id, offset] of movedNodes.get(layoutKey) ?? []) {
    const position = layout.positions.get(id);
    if (position)
      layout.positions.set(id, { ...position, x: position.x + offset.x, y: position.y + offset.y });
  }
  const ids = new Set(visibleNodes.map(node => node.id));
  drawnPanels = layout.panels.filter(panel => panel.nodes.some(node => ids.has(node.id)));
  updateBounds();
  if (pickedEdge && !visibleEdges.includes(pickedEdge)) pickedEdge = null;
  const total = state.view === "group" ? filterGraph.nodes.length : classes.length;
  get("map-count").textContent = state.focused
    ? "선택 주변 " + visibleNodes.length + "개 · 연결 " + visibleEdges.length
    : state.view === "namespace"
    ? namespaceTitle(state.namespace) + " · 내부 " + (groups.get(state.namespace)?.length ?? 0)
      + "개 · 외부 " + (visibleNodes.length - (groups.get(state.namespace)?.length ?? 0)) + "개 · 연결 " + visibleEdges.length
    : state.view === "group" ? "필터 " + visibleNodes.length + "/" + total + "개 · 방향 연결 " + visibleEdges.length
    : state.view === "project" ? "프로젝트 " + projects.length + "개 · 외부 라이브러리 " + libraries.length + "개 · 연결 " + visibleEdges.length
    : "표시 " + visibleNodes.length + "/" + total + "개 · 연결 " + visibleEdges.length
      + (inProjects && hasStudioFilters ? " · Interface 상속 " + visibleEdges.filter(edge => interfaceInheritance.has(edge)).length : "");
  get("empty").hidden = visibleNodes.length > 0;
  get("empty").textContent = state.view === "project" && projects.length === 0
    ? "프로젝트 참조 지도는 Visual Studio 프로젝트 파일에서 생성합니다."
    : "현재 표시할 항목이 없습니다.";
  drawGraph(drawnPanels);
  updateSearch();
  updateHighlights();
  showEvidence();
  if (resetCamera) fit(!relationshipView() && state.view === "class" && !state.focused ? .75 : 0);
}

function selectedEdges() {
  return scopeEdges.filter(edge =>
    (state.direction !== "incoming" && edge.from === state.selected)
    || (state.direction !== "outgoing" && edge.to === state.selected));
}

function updateBounds() {
  bounds = measure([...drawnPanels, ...visibleNodes.map(node => layout.positions.get(node.id))]);
  get("reset-layout").disabled = !movedNodes.get(layoutKey)?.size;
}

function focusLayout() {
  const outgoing = new Set(visibleEdges.filter(edge => edge.from === state.selected).map(edge => edge.to));
  const incoming = new Set(visibleEdges.filter(edge => edge.to === state.selected && !outgoing.has(edge.from)).map(edge => edge.from));
  const ordered = ids => visibleNodes.filter(node => ids.has(node.id));
  const lanes = [
    { title: "들어오는 연결", nodes: ordered(incoming) },
    { title: "선택 항목", nodes: [byId.get(state.selected)], center: true },
    { title: "나가는 연결", nodes: ordered(outgoing) }
  ].filter(lane => lane.nodes.length > 0);
  const width = 232, height = 52, stepX = 250, stepY = 68;
  const panels = lanes.map(lane => {
    const columns = Math.min(3, Math.ceil(lane.nodes.length / 11));
    return { ...lane, columns, width: width + (columns - 1) * stepX + 32,
      height: height + (Math.ceil(lane.nodes.length / columns) - 1) * stepY + 61 };
  });
  const mapHeight = Math.max(...panels.map(panel => panel.height));
  const positions = new Map();
  let x = 0;
  for (const panel of panels) {
    const y = (mapHeight - panel.height) / 2;
    Object.assign(panel, { x, y });
    panel.nodes.forEach((node, index) => positions.set(node.id, {
      x: x + 16 + index % panel.columns * stepX, y: y + 45 + Math.floor(index / panel.columns) * stepY,
      width, height, external: state.view === "namespace" && node.kind === "class" && namespaceOf(node) !== state.namespace
    }));
    x += panel.width + 88;
  }
  return { positions, panels };
}

function groupByNamespace(nodes, getGroup = namespaceOf) {
  const result = new Map();
  for (const node of nodes) {
    const name = getGroup(node);
    if (!result.has(name)) result.set(name, []);
    result.get(name).push(node);
  }
  return result;
}

function classLayout(nodes, focus = null, groupBy = namespaceOf) {
  const groupedByProject = groupBy === projectOf;
  const groups = [...groupByNamespace(nodes, groupBy)].sort((a, b) =>
    Number(b[0] === focus) - Number(a[0] === focus) || b[1].length - a[1].length || a[0].localeCompare(b[0]));
  const panels = groups.map(([name, entries]) => {
    const columns = Math.max(1, Math.ceil(Math.sqrt(entries.length * .33)));
    const rows = Math.ceil(entries.length / columns);
    return {
      name, nodes: entries, columns, external: focus !== null && name !== focus,
      title: groupedByProject ? projectName(name) : undefined,
      color: groupedByProject ? projectColors.get(name) : undefined,
      width: 32 + card.width + (columns - 1) * card.stepX,
      height: 64 + card.height + (rows - 1) * card.stepY
    };
  });
  const area = panels.reduce((total, panel) => total + panel.width * panel.height, 0);
  const limit = Math.max(...panels.map(panel => panel.width), Math.sqrt(area) * 1.48);
  const positions = new Map();
  let x = 0, y = 0, height = 0;
  for (const panel of panels) {
    if (x > 0 && x + panel.width > limit) {
      y += height + 40;
      x = height = 0;
    }
    Object.assign(panel, { x, y });
    panel.nodes.forEach((node, index) => {
      positions.set(node.id, {
        x: x + 16 + index % panel.columns * card.stepX,
        y: y + 48 + Math.floor(index / panel.columns) * card.stepY,
        width: card.width, height: card.height, external: panel.external
      });
    });
    height = Math.max(height, panel.height);
    x += panel.width + 32;
  }
  return { positions, panels };
}

function projectBoundaryLayout(nodes, edges) {
  const layout = classLayout(nodes, null, projectOf);
  const targets = new Set(edges.map(edge => projectOf(byId.get(edge.to))));
  const inputs = layout.panels.filter(panel => !targets.has(panel.name));
  const outputs = layout.panels.filter(panel => targets.has(panel.name));
  if (!inputs.length || !outputs.length) return layout;

  const columnHeight = panels => panels.reduce((height, panel) => height + panel.height + 26, -26);
  const height = Math.max(columnHeight(inputs), columnHeight(outputs));
  for (const [column, panels] of [inputs, outputs].entries()) {
    const x = column === 0 ? 0 : Math.max(...inputs.map(panel => panel.width)) + 112;
    let y = (height - columnHeight(panels)) / 2;
    for (const panel of panels) {
      const dx = x - panel.x, dy = y - panel.y;
      Object.assign(panel, { x, y });
      for (const node of panel.nodes) {
        const position = layout.positions.get(node.id);
        position.x += dx;
        position.y += dy;
      }
      y += panel.height + 26;
    }
  }
  return layout;
}

function projectLayout() {
  const remaining = new Set(projects.map(node => node.id));
  const tiers = [];
  while (remaining.size > 0) {
    const roots = [...remaining].filter(id => !projectEdges.some(edge => edge.to === id && remaining.has(edge.from)));
    const tier = roots.length ? roots : [...remaining];
    for (const id of tier) remaining.delete(id);
    tiers.push(tier);
  }
  const height = Math.max(1, libraries.length, ...tiers.map(tier => tier.length)) * card.stepY;
  const positions = new Map();
  const gap = libraries.length ? 82 : 105;
  tiers.forEach((tier, column) => tier.forEach((id, row) => {
    positions.set(id, {
      x: column * (card.width + gap), y: (height - tier.length * card.stepY) / 2 + row * card.stepY,
      width: card.width, height: card.height
    });
  }));
  const panels = [];
  if (libraries.length) {
    const x = tiers.length * (card.width + gap);
    const y = (height - libraries.length * card.stepY) / 2;
    libraries.forEach((library, row) => positions.set(library.id, {
      x, y: y + row * card.stepY, width: card.width, height: card.height
    }));
    panels.push({ x: x - 14, y: y - 42, width: card.width + 28, height: libraries.length * card.stepY + 27,
      title: "외부 라이브러리", color: types.external.color, nodes: libraries });
  }
  return { positions, panels };
}

function measure(rectangles) {
  if (!rectangles.length) return null;
  const left = Math.min(...rectangles.map(rect => rect.x));
  const top = Math.min(...rectangles.map(rect => rect.y));
  const right = Math.max(...rectangles.map(rect => rect.x + rect.width));
  const bottom = Math.max(...rectangles.map(rect => rect.y + rect.height));
  return { left, top, width: right - left, height: bottom - top };
}

function svg(tag, attributes = {}, text = "") {
  const element = document.createElementNS("http://www.w3.org/2000/svg", tag);
  for (const [key, value] of Object.entries(attributes)) element.setAttribute(key, value);
  if (text) element.textContent = text;
  return element;
}

function markers() {
  const defs = svg("defs");
  for (const [kind, type] of Object.entries(types)) {
    const marker = svg("marker", { id: "arrow-" + kind, viewBox: "0 0 10 10", refX: 9, refY: 5,
      markerWidth: 11, markerHeight: 11, markerUnits: "userSpaceOnUse", orient: "auto" });
    marker.append(svg("path", { d: "M 0 0 L 10 5 L 0 10 Z", fill: type.color }));
    defs.append(marker);
  }
  return defs;
}

function interfaceBoundary(edge) {
  return edge.kind === "aggregate" ? edge.interfaces > 0 : projectClassView() && interfaceInheritance.has(edge);
}

function drawGraph(panels) {
  world.replaceChildren();
  nodeElements = new Map();
  edgeElements = [];
  routes = new Map();
  const background = svg("g", { "aria-hidden": "true" });
  for (const panel of panels) {
    const title = panel.title ?? namespaceTitle(panel.name);
    const group = svg("g", panel.name === undefined ? {} : { "data-namespace": panel.name });
    if (panel.color) {
      group.classList.add("project-group");
      group.style.setProperty("--group-color", panel.color);
    }
    group.append(svg("title", {}, title), svg("rect", {
      class: "group-box" + (panel.external ? " external" : "") + (panel.center ? " center" : ""),
      x: panel.x, y: panel.y, width: panel.width, height: panel.height, rx: 9
    }), svg("text", { class: "group-name", x: panel.x + 16, y: panel.y + 34 },
      shorten(title, Math.floor((panel.width - 28) / 13))));
    background.append(group);
  }
  const lines = svg("g", { "aria-hidden": "true" });
  const offsets = new Map();
  const directions = new Set(visibleEdges.map(edge => JSON.stringify([edge.from, edge.to])));
  const labels = svg("g", { "aria-hidden": "true" });
  for (const edge of visibleEdges) {
    const key = JSON.stringify([edge.from, edge.to].sort());
    const order = offsets.get(key) ?? 0;
    offsets.set(key, order + 1);
    const aggregate = edge.kind === "aggregate";
    const reciprocal = aggregate && directions.has(JSON.stringify([edge.to, edge.from]));
    const offset = reciprocal ? (edge.from < edge.to ? 1 : -1) * 17
      : (order % 2 ? -1 : 1) * Math.ceil(order / 2) * 5;
    const boundary = interfaceBoundary(edge);
    const style = boundary ? "boundary" : edge.kind;
    const path = svg("path", { class: "link " + edge.kind + (boundary ? " interface-boundary" : ""),
      "data-from": edge.from, "data-to": edge.to, stroke: types[style].color,
      "marker-end": "url(#arrow-" + style + ")" });
    const hit = svg("path", { class: "edge-hit" });
    const description = aggregate ? edge.pairCount + " 클래스 쌍" : boundary ? "Interface 상속" : types[edge.kind].name;
    hit.append(svg("title", {}, byId.get(edge.from).name + " → " + byId.get(edge.to).name + " · " + description));
    hit.addEventListener("click", () => inspectEdge(edge));
    const label = aggregate ? svg("text", { class: "edge-count", "text-anchor": "middle" }, String(edge.pairCount)) : null;
    if (label) labels.append(label);
    const route = { edge, path, hit, offset, label };
    edgeElements.push(route);
    for (const id of [edge.from, edge.to]) {
      if (!routes.has(id)) routes.set(id, []);
      routes.get(id).push(route);
    }
    drawRoute(route);
    lines.append(path, hit);
  }
  const nodes = svg("g");
  for (const node of visibleNodes) {
    const pos = layout.positions.get(node.id);
    const appearance = "node" + (pos.external ? " external" : "") + (pos.radial ? " radial" : "")
      + (pos.hub ? " hub" : "") + (projectClassView() ? " project-member" : "")
      + (node.kind === "group" ? " filter-node" : "") + (node.kind === "library" ? " external-library" : "")
      + (node.kind === "project" ? " project-node" : "");
    const group = svg("g", { class: appearance, role: "button", tabindex: 0,
      "data-node-id": node.id, "aria-label": node.kind === "library" ? node.name + " 외부 라이브러리" : node.name,
      transform: nodeTransform(pos) });
    const prefix = namespaceOf(node) + "::";
    const label = node.name.startsWith(prefix) ? node.name.slice(prefix.length) : node.name;
    group.append(svg("title", {}, node.name));
    if (node.color || projectClassView())
      group.style.setProperty("--node-color", node.color ?? projectColors.get(projectOf(node)));
    if (pos.radial) {
      const radius = pos.width / 2;
      group.style.setProperty("--node-color", node.color ?? (node.kind === "class" ? namespaceColors.get(namespaceOf(node)) : types.reference.color));
      group.append(svg("circle", { class: "box", cx: radius, cy: radius, r: radius }),
        svg("text", { class: "node-label", x: radius, y: pos.height + 16, "text-anchor": "middle" },
          shorten(node.filterName ?? label, 25)));
      if (node.kind === "group")
        group.append(svg("text", { class: "filter-project", x: radius, y: pos.height + 29, "text-anchor": "middle" },
          shorten(node.projectLabel, 25)));
    } else {
      group.append(svg("rect", { class: "box", width: pos.width, height: pos.height, rx: 6 }),
        svg("text", { x: 10, y: state.focused ? 21 : 24 }, shorten(node.filterName ?? label, state.focused ? 28 : 26)));
      if (state.focused && node.kind === "class")
        group.append(svg("text", { class: "subtitle", x: 10, y: 40 }, shorten(namespaceTitle(namespaceOf(node)), 32)));
    }
    group.addEventListener("click", () => choose(node.id, false));
    group.addEventListener("dblclick", () => {
      if (!state.focused && state.selected === node.id) toggleFocus();
    });
    group.addEventListener("keydown", event => {
      if (event.key !== "Enter" && event.key !== " ") return;
      event.preventDefault();
      choose(node.id, true);
    });
    nodeElements.set(node.id, group);
    nodes.append(group);
  }
  world.append(background, lines, labels, nodes);
  for (const route of edgeElements) positionRouteLabel(route);
}

function nodeTransform(position) {
  return "translate(" + position.x + " " + position.y + ")";
}

function drawRoute(route) {
  const shape = edgePath(route.edge, route.offset);
  route.path.setAttribute("d", shape);
  route.hit.setAttribute("d", shape);
  if (route.path.isConnected) positionRouteLabel(route);
}

function positionRouteLabel({ path, label }) {
  if (!label) return;
  const point = path.getPointAtLength(path.getTotalLength() * .5);
  label.setAttribute("x", point.x);
  label.setAttribute("y", point.y - 3);
}

function edgePath(edge, offset) {
  const a = layout.positions.get(edge.from);
  const b = layout.positions.get(edge.to);
  if (a.radial && b.radial) {
    const startX = a.x + a.width / 2, startY = a.y + a.height / 2;
    const endX = b.x + b.width / 2, endY = b.y + b.height / 2;
    const dx = endX - startX, dy = endY - startY;
    const length = Math.hypot(dx, dy);
    const ux = length ? dx / length : 1, uy = length ? dy / length : 0;
    const fromX = startX + ux * a.width / 2, fromY = startY + uy * a.height / 2;
    const toX = endX - ux * b.width / 2, toY = endY - uy * b.height / 2;
    const bend = offset * 2 * (edge.from < edge.to ? 1 : -1);
    const cx = (fromX + toX) / 2 - uy * bend, cy = (fromY + toY) / 2 + ux * bend;
    return "M " + fromX + " " + fromY + " Q " + cx + " " + cy + ", " + toX + " " + toY;
  }
  const ay = a.y + a.height / 2;
  const by = b.y + b.height / 2;
  const forward = a.x + a.width / 2 < b.x + b.width / 2;
  const room = forward ? b.x - (a.x + a.width) : a.x - (b.x + b.width);
  if (room < 14) {
    const x1 = a.x + a.width, x2 = b.x + b.width;
    const curve = Math.max(x1, x2) + 23 + Math.abs(offset);
    const y1 = ay + offset - 2, y2 = by + offset + 2;
    return "M " + x1 + " " + y1 + " C " + curve + " " + y1 + ", " + curve + " " + y2 + ", " + x2 + " " + y2;
  }
  const x1 = forward ? a.x + a.width : a.x, x2 = forward ? b.x : b.x + b.width;
  const bend = (x2 - x1) * .5;
  const y1 = ay + offset, y2 = by + offset;
  return "M " + x1 + " " + y1 + " C " + (x1 + bend) + " " + y1 + ", " + (x2 - bend) + " " + y2 + ", " + x2 + " " + y2;
}

function shorten(text, limit) {
  return text.length > limit ? text.slice(0, limit - 1) + "…" : text;
}

function setOptions(element, options) {
  element.replaceChildren(...options.map(([id, name]) => new Option(name, id)));
}

function matches(node) {
  return !state.query || (node.name + " " + (node.location?.file ?? "")).toLowerCase().includes(state.query);
}

function updateSearch() {
  const found = scopeNodes.filter(matches);
  setOptions(get("nodes"), found.map(node => [node.id, node.name]));
  get("nodes").selectedIndex = -1;
  if (found.some(node => node.id === state.selected)) get("nodes").value = state.selected;
  get("results").textContent = (state.query ? "검색 결과 " : "목록 ") + found.length + "개";
}

function choose(id, move) {
  if (!id || !scopeNodes.some(node => node.id === id)) return;
  const refocus = state.focused && state.selected !== id;
  state.selected = id;
  pickedEdge = null;
  if (refocus) {
    renderMap(true);
    return;
  }
  updateSearch();
  updateHighlights();
  showEvidence();
  if (move) locate();
}

function inspectEdge(edge) {
  if (!selectedEdges().includes(edge)) {
    state.selected = edge.from;
    state.direction = "";
    get("direction").value = "";
  }
  pickedEdge = edge;
  updateSearch();
  updateHighlights();
  showEvidence();
  if (!get("inspector").hidden) evidenceRows.get(edge)?.scrollIntoView({ block: "nearest" });
}

function updateHighlights() {
  graph.classList.toggle("has-selection", !!state.selected);
  const selected = new Set(selectedEdges());
  const connected = new Set();
  for (const { edge, path, label } of edgeElements) {
    const incident = selected.has(edge);
    if (incident) {
      connected.add(edge.from);
      connected.add(edge.to);
    }
    path.classList.toggle("connected", incident);
    path.classList.toggle("unfocused", !!state.selected && (!incident || !!pickedEdge && pickedEdge !== edge));
    path.classList.toggle("picked", pickedEdge === edge);
    label?.classList.toggle("unfocused", !!state.selected && (!incident || !!pickedEdge && pickedEdge !== edge));
  }
  for (const [id, node] of nodeElements) {
    node.classList.toggle("active", id === state.selected);
    node.classList.toggle("connected", connected.has(id));
    node.classList.toggle("dimmed", !!state.selected && id !== state.selected && !connected.has(id) && !state.focused);
    node.classList.toggle("matched", !!state.query && matches(byId.get(id)));
    node.setAttribute("aria-pressed", id === state.selected);
  }
  updateEdgeTargets();
  get("locate").disabled = !state.selected;
  get("direction").disabled = !state.selected;
  get("focus").disabled = !state.selected;
  get("focus").setAttribute("aria-pressed", state.focused);
}

function updateEdgeTargets() {
  for (const { path, hit } of edgeElements)
    hit.classList.toggle("available", state.focused || path.classList.contains("connected") || (!state.selected && edgesAtReadableZoom));
}

function startDrag(event) {
  if (!event.isPrimary || event.button !== 0 || drag) return;
  const node = event.target.closest(".node");
  const id = node?.dataset.nodeId;
  suppressClick = false;
  drag = {
    pointer: event.pointerId, x: event.clientX, y: event.clientY, moved: false, id,
    origin: id ? layout.positions.get(id) : { x: camera.x, y: camera.y },
    offset: movedNodes.get(layoutKey)?.get(id) ?? { x: 0, y: 0 }
  };
}

function moveDrag(event) {
  if (drag?.pointer !== event.pointerId) return;
  const dx = event.clientX - drag.x, dy = event.clientY - drag.y;
  if (!drag.moved) {
    if (Math.hypot(dx, dy) < 4) return;
    graph.setPointerCapture(event.pointerId);
    graph.classList.add("dragging");
    drag.moved = suppressClick = true;
  }
  if (!drag.id) {
    camera.x = drag.origin.x + dx;
    camera.y = drag.origin.y + dy;
    updateCamera();
    return;
  }

  const x = dx / camera.scale, y = dy / camera.scale;
  const position = { ...drag.origin, x: drag.origin.x + x, y: drag.origin.y + y };
  layout.positions.set(drag.id, position);
  nodeElements.get(drag.id).setAttribute("transform", nodeTransform(position));
  for (const route of routes.get(drag.id) ?? []) drawRoute(route);
  if (!movedNodes.has(layoutKey)) movedNodes.set(layoutKey, new Map());
  const offsets = movedNodes.get(layoutKey);
  const offset = { x: drag.offset.x + x, y: drag.offset.y + y };
  if (Math.hypot(offset.x, offset.y) < 1) offsets.delete(drag.id);
  else offsets.set(drag.id, offset);
}

function endDrag(event) {
  if (drag?.pointer !== event.pointerId) return;
  if (drag.id && drag.moved) updateBounds();
  drag = null;
  graph.classList.remove("dragging");
  setTimeout(() => { suppressClick = false; }, 0);
}

function resizeViewport() {
  const width = graph.clientWidth, height = graph.clientHeight;
  if (!width || !height) return;
  if (viewport.width && viewport.height) {
    camera.x += (width - viewport.width) / 2;
    camera.y += (height - viewport.height) / 2;
  }
  updateCamera();
}

function updateCamera() {
  const width = graph.clientWidth, height = graph.clientHeight;
  if (!width || !height) return;
  viewport = { width, height };
  graph.setAttribute("viewBox", "0 0 " + width + " " + height);
  world.setAttribute("transform", "translate(" + camera.x + " " + camera.y + ") scale(" + camera.scale + ")");
  graph.style.setProperty("--group-title-size", Math.min(39, Math.max(19, 9 / camera.scale)) + "px");
  graph.style.setProperty("--relationship-label-size", Math.min(34, Math.max(14, 11 / camera.scale)) + "px");
  graph.classList.toggle("compact-labels", camera.scale < .63);
  get("zoom-level").textContent = Math.round(camera.scale * 100) + "%";
  const readable = camera.scale >= .45;
  if (readable !== edgesAtReadableZoom) {
    edgesAtReadableZoom = readable;
    updateEdgeTargets();
  }
}

function fit(minimumScale = 0) {
  if (!bounds || !graph.clientWidth || !graph.clientHeight) return;
  const w = graph.clientWidth, h = graph.clientHeight;
  const margin = state.view === "group" ? 56 : 32;
  const fittedScale = Math.min(state.view === "group" ? 1.6 : 1.25, (w - margin) / bounds.width, (h - margin) / bounds.height);
  const scale = Math.max(minimumScale, fittedScale);
  camera = {
    x: Math.max(margin / 2, (w - bounds.width * scale) / 2) - bounds.left * scale,
    y: Math.max(margin / 2, (h - bounds.height * scale) / 2) - bounds.top * scale,
    scale
  };
  updateCamera();
}

function zoom(multiplier, x, y) {
  if (!bounds) return;
  const next = Math.max(.015, Math.min(2.8, camera.scale * multiplier));
  const ratio = next / camera.scale;
  camera = { x: x - (x - camera.x) * ratio, y: y - (y - camera.y) * ratio, scale: next };
  updateCamera();
}

function locate() {
  const position = layout.positions.get(state.selected);
  if (!position) return;
  const scale = Math.max(.9, camera.scale);
  camera = {
    x: graph.clientWidth / 2 - (position.x + position.width / 2) * scale,
    y: graph.clientHeight / 2 - (position.y + position.height / 2) * scale,
    scale
  };
  updateCamera();
}

function showEvidence() {
  const node = state.selected ? byId.get(state.selected) : null;
  get("details-title").textContent = node?.name ?? "연결 근거";
  get("location").textContent = node?.kind === "group" ? "클래스 " + node.count + "개"
    : node?.kind === "library" ? "외부 라이브러리 · " + node.location.file
    : node?.location ? formatLocation(node.location) : "";
  const edges = node ? selectedEdges() : state.view === "project" ? scopeEdges : [];
  const container = get("details");
  const scrollTop = container.scrollTop;
  container.replaceChildren();
  evidenceRows = new Map();
  if (!edges.length) {
    const hint = document.createElement("p");
    hint.className = "muted";
    hint.textContent = node ? "현재 필터에 해당하는 연결이 없습니다." : "지도나 목록에서 항목을 선택하면 연결과 코드 위치가 표시됩니다.";
    container.append(hint);
    return;
  }
  if (!node) {
    for (const edge of edges) container.append(evidenceRow(edge, null));
    return;
  }
  for (const [title, direction] of [["들어오는 연결", "incoming"], ["나가는 연결", "outgoing"]]) {
    const group = edges.filter(edge => (direction === "incoming" ? edge.to : edge.from) === node.id);
    if (!group.length) continue;
    const heading = document.createElement("h3");
    heading.className = "direction-title";
    heading.textContent = title + " " + group.length + "개";
    container.append(heading);
    for (const edge of group) container.append(evidenceRow(edge, node.id));
  }
  if (pickedEdge) container.scrollTop = scrollTop;
}

function evidenceRow(edge, selected) {
  const row = document.createElement("details");
  const summary = document.createElement("summary");
  const kind = document.createElement("span");
  const neighbor = selected === edge.from ? edge.to : edge.from;
  row.className = "entry";
  row.open = pickedEdge === edge;
  kind.className = "relation-type";
  kind.textContent = edge.kind === "aggregate"
    ? edge.pairCount + "쌍 · " + edge.relations.length + "관계" + (edge.interfaces ? " · Interface " + edge.interfaces : "")
    : interfaceBoundary(edge) ? "Interface 상속" : types[edge.kind].name;
  summary.append(kind, selected ? byId.get(neighbor).name : byId.get(edge.from).name + " → " + byId.get(edge.to).name);
  summary.addEventListener("click", () => {
    if (!row.open) pickedEdge = edge;
    else if (pickedEdge === edge) pickedEdge = null;
    updateHighlights();
  });
  const entries = edge.kind === "aggregate" ? groupEvidence(edge, row) : evidenceLocations(edge.evidence);
  const navigate = document.createElement("button");
  navigate.type = "button";
  navigate.className = "inspect-node";
  navigate.dataset.target = neighbor;
  navigate.textContent = "이 항목으로 이동";
  navigate.addEventListener("click", () => { choose(neighbor, true); get("details-title").focus(); });
  row.append(summary, entries, navigate);
  evidenceRows.set(edge, row);
  return row;
}

function groupEvidence(edge, row) {
  const container = document.createElement("div");
  container.className = "group-sources";
  const materialize = () => {
    if (!row.open || container.childElementCount) return;
    for (const relation of edge.relations) {
      const detail = document.createElement("details");
      const summary = document.createElement("summary");
      const navigate = document.createElement("button");
      detail.className = "entry";
      summary.textContent = types[relation.kind].name + " · " + byId.get(relation.from).name + " → " + byId.get(relation.to).name;
      navigate.type = "button";
      navigate.className = "inspect-node";
      navigate.textContent = "대상 클래스로 이동";
      navigate.addEventListener("click", () => {
        const target = byId.get(relation.to);
        state.hidden.delete(namespaceOf(target));
        state.hiddenProjects.delete(projectOf(target));
        state.interfaceOnly = false;
        changeView("class");
        choose(target.id, true);
        get("details-title").focus();
      });
      detail.append(summary, evidenceLocations(relation.evidence), navigate);
      container.append(detail);
    }
  };
  row.addEventListener("toggle", materialize);
  materialize();
  return container;
}

function evidenceLocations(evidenceList) {
  const entries = document.createElement("ul");
  for (const evidence of evidenceList) {
    const item = document.createElement("li");
    const location = document.createElement("code");
    location.textContent = formatLocation(evidence.location);
    item.append(location, " · " + evidence.description);
    entries.append(item);
  }
  return entries;
}

function formatLocation(location) {
  return location.file + ":" + location.line + (location.column ? ":" + location.column : "");
}

function showIssues() {
  const issues = get("issues");
  const info = document.createElement("p");
  info.className = "muted";
  info.textContent = "호출 관계는 Clang이 소스에서 해석한 정적 대상입니다. 오류가 있어도 Clang이 복구한 선언은 표시될 수 있습니다.";
  issues.append(info);
  const rows = [];
  if (data.excludedTests.length)
    rows.push(["감지한 테스트 제외", data.excludedTests.join("\n")]);
  for (const [kind, items] of [["오류", data.failures], ["참고", data.notes]])
    for (const issue of items) rows.push([kind + " · " + issue.file, issue.message]);
  for (const [label, text] of rows) {
    const row = document.createElement("details"), title = document.createElement("summary"), pre = document.createElement("pre");
    row.className = "entry";
    title.textContent = label;
    pre.textContent = text;
    row.append(title, pre);
    issues.append(row);
  }
}
