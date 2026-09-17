"use strict";

const $ = (sel, root) => (root || document).querySelector(sel);
const hostEl = $("#server-host");
const rootEl = $("#server-root");
const groupsEl = $("#groups");
const filtersEl = $("#filters");
const statusEl = $("#status-text");

const modalBackdrop = $("#modal-backdrop");
const modalTitle = $("#modal-title");
const modalBody = $("#modal-body");

const stateDot = {
  running: "running",
  starting: "starting",
  stopped: "stopped",
  failed: "failed",
  detached: "running",
};
const stateLabel = {
  running: "运行中",
  starting: "启动中",
  stopped: "已停止",
  failed: "启动失败",
  detached: "已启动",
};

let allTools = [];
let selection = "全部";

async function api(path, opts) {
  const res = await fetch(path, opts);
  if (!res.ok) throw new Error(path + " -> " + res.status);
  return res.json();
}

async function refresh() {
  try {
    const d = await api("/api/tools");
    // Ignore pure port/state ping-pong from probes.
    allTools = d;
    statusEl.textContent = "已更新 " + new Date().toLocaleTimeString();
    render();
    return true;
  } catch (e) {
    statusEl.textContent = "连接失败 " + new Date().toLocaleTimeString();
    return false;
  }
}

function categories(tools) {
  const seen = [];
  for (const t of tools) if (!seen.includes(t.category)) seen.push(t.category);
  return seen;
}

function renderFilters() {
  filtersEl.innerHTML = "";
  ["全部", ...categories(allTools)].forEach((c) => {
    const b = document.createElement("button");
    b.className = "chip" + (c === selection ? " active" : "");
    b.textContent = c;
    b.onclick = () => {
      selection = c;
      renderFilters();
      render();
    };
    filtersEl.appendChild(b);
  });
}

function render() {
  renderFilters();
  const filtered = selection === "全部"
    ? allTools
    : allTools.filter((t) => t.category === selection);
  const grouped = {};
  for (const t of filtered) (grouped[t.category] = grouped[t.category] || []).push(t);

  groupsEl.innerHTML = "";
  if (!filtered.length) {
    groupsEl.innerHTML = '<div class="notice">该分类下暂无工具</div>';
    return;
  }
  for (const cat of Object.keys(grouped)) {
    const group = document.createElement("section");
    group.className = "group";
    const h = document.createElement("h2");
    h.textContent = cat;
    group.appendChild(h);
    const cards = document.createElement("div");
    cards.className = "cards";
    for (const t of grouped[cat]) cards.appendChild(card(t));
    group.appendChild(cards);
    groupsEl.appendChild(group);
  }
}

function card(t) {
  const el = document.createElement("div");
  el.className = "card";

  const head = document.createElement("div");
  head.className = "card-head";
  const title = document.createElement("span");
  title.className = "card-title";
  title.textContent = t.name;
  const badge = document.createElement("span");
  badge.className = "type-badge";
  badge.textContent = t.typeLabel;
  head.append(title, badge);

  const summary = document.createElement("div");
  summary.className = "card-summary";
  summary.textContent = t.summary;

  const body = document.createElement("div");
  body.className = "card-body";

  const status = document.createElement("span");
  status.className = "status";
  const dot = document.createElement("span");
  dot.className = "dot " + (stateDot[t.state] || "stopped");
  const sLabel = document.createElement("span");
  sLabel.textContent = stateLabel[t.state] || t.state;
  status.append(dot, sLabel);
  if (t.portOpen && t.state === "running") {
    const ph = document.createElement("span");
    ph.className = "port-hint";
    ph.textContent = " · 端口 " + t.port + " 已就绪";
    status.appendChild(ph);
  } else if (t.port > 0 && t.state === "running") {
    const ph = document.createElement("span");
    ph.className = "port-hint";
    ph.textContent = " · 端口 " + t.port;
    status.appendChild(ph);
  }
  body.appendChild(status);

  if (t.error) {
    const e = document.createElement("div");
    e.className = "err";
    e.textContent = t.error;
    body.appendChild(e);
  }

  const actions = document.createElement("div");
  actions.className = "actions";

  const btn = actionButton(t);
  if (btn) actions.appendChild(btn);

  const webUrl = webUrlOf(t);
  if (webUrl) {
    const open = document.createElement("button");
    open.textContent = "打开界面";
    open.onclick = () => window.open(webUrl, "_blank");
    actions.appendChild(open);
  }
  if (t.docs) {
    const docs = document.createElement("button");
    docs.textContent = "文档";
    docs.onclick = () => openDocs(t);
    actions.appendChild(docs);
  }
  if (t.type === "service") {
    const logs = document.createElement("button");
    logs.textContent = "日志";
    logs.onclick = () => openLogs(t);
    actions.appendChild(logs);
  }

  body.appendChild(actions);
  el.append(head, summary, body);
  return el;
}

function webUrlOf(t) {
  if (t.port <= 0) return "";
  if (t.type !== "service" && t.type !== "webapp") return "";
  return "http://" + location.hostname + ":" + t.port + (t.webPath || "/");
}

function actionButton(t) {
  if (t.type === "document") return null;
  const managed = t.type === "service" && t.managed;
  const running = t.state === "running" || t.state === "starting" || t.state === "detached";
  const b = document.createElement("button");
  b.className = running && managed ? "danger" : "primary";
  b.textContent = running ? (managed ? "停止" : "已启动") : "启动";
  if (running && !managed) { b.disabled = true; return b; }
  b.onclick = async () => {
    b.disabled = true;
    try {
      await api("/api/tools/" + t.id + "/" + (running ? "stop" : "start"),
        { method: "POST" });
    } catch (_) {}
    refresh();
  };
  return b;
}

function openModal(title, node) {
  modalTitle.textContent = title;
  modalBody.innerHTML = "";
  modalBody.appendChild(node);
  modalBackdrop.hidden = false;
}

async function openDocs(t) {
  const box = document.createElement("div");
  box.innerHTML = "<p class='muted'>载入文档…</p>";
  openModal(t.name + " — 文档", box);
  try {
    const res = await fetch(t.docs);
    if (!res.ok) throw new Error("status " + res.status);
    const md = await res.text();
    const holder = document.createElement("div");
    holder.className = "docs";
    holder.innerHTML = mdRender(md);
    modalBody.replaceChildren(holder);
  } catch (_) {
    box.innerHTML = "<p class='err'>无法加载文档</p>";
  }
}

async function openLogs(t) {
  const pre = document.createElement("pre");
  pre.textContent = "载入日志…";
  openModal(t.name + " — 日志", pre);
  try {
    const d = await api("/api/tools/" + t.id + "/logs");
    pre.textContent = d.log || "(暂无输出)";
  } catch (_) {
    pre.textContent = "(无法获取日志)";
  }
}

function mdEscape(s) {
  return s.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;");
}

function mdInline(s) {
  return s
    .replace(/`([^`]+)`/g, "<code>$1</code>")
    .replace(/\*\*([^*]+)\*\*/g, "<strong>$1</strong>")
    .replace(/\*([^*]+)\*/g, "<em>$1</em>")
    .replace(/\[([^\]]+)\]\(([^)]+)\)/g,
      '<a href="$2" target="_blank" rel="noopener">$1</a>');
}

function mdRender(src) {
  const lines = src.replace(/\r\n/g, "\n").split("\n");
  let html = "";
  let inCode = false;
  let code = [];
  let para = [];
  let inList = false;

  const flushPara = () => {
    if (para.length) {
      html += "<p>" + mdInline(para.join(" ")) + "</p>";
      para = [];
    }
  };
  const flushList = () => {
    if (inList) { html = html.replace(/<li>([\s\S]*)$/, "<li>$1</ul>"); html += "</ul>"; inList = false; }
  };

  for (const raw of lines) {
    const line = raw.replace(/\s+$/, "");
    if (inCode) {
      if (line.startsWith("```")) {
        html += "<pre><code>" + mdEscape(code.join("\n")).trim() + "</code></pre>";
        code = [];
        inCode = false;
      } else {
        code.push(line);
      }
      continue;
    }
    if (line.startsWith("```")) { flushPara(); flushList(); inCode = true; continue; }
    if (/^#{1,4}\s/.test(line)) {
      flushPara(); flushList();
      const level = line.match(/^#+/)[0].length;
      html += "<h" + level + ">" + mdInline(line.replace(/^#+\s*/, "")) + "</h" + level + ">";
      continue;
    }
    if (/^>\s?/.test(line)) {
      flushPara(); flushList();
      html += "<blockquote>" + mdInline(line.replace(/^>\s?/, "")) + "</blockquote>";
      continue;
    }
    if (/^[-*]\s/.test(line)) {
      flushPara();
      if (!inList) { html += "<ul>"; inList = true; }
      html += "<li>" + mdInline(line.replace(/^[-*]\s/, "")) + "</li>";
      continue;
    }
    if (/^\d+\.\s/.test(line)) {
      flushPara();
      if (!inList) { html += "<ol>"; inList = true; }
      html += "<li>" + mdInline(line.replace(/^\d+\.\s/, "")) + "</li>";
      continue;
    }
    if (/^\s*$/.test(line)) {
      flushPara();
      continue;
    }
    para.push(line);
  }
  flushPara();
  if (inCode && code.length) {
    html += "<pre><code>" + mdEscape(code.join("\n")) + "</code></pre>";
  }
  return html;
}

// Initial load + polling.
(async function init() {
  try {
    const ok = await refresh();
    const host = location.hostname;
    hostEl.textContent = host + (location.port ? ":" + location.port : "");
  } catch (_) {}
  try {
    const st = await api("/api/status");
    rootEl.textContent = st.root;
  } catch (_) {}
  setInterval(refresh, 2000);
})();

modalBackdrop.addEventListener("click", (e) => {
  if (e.target === modalBackdrop) modalBackdrop.hidden = true;
});
$("#modal-close").addEventListener("click", () => (modalBackdrop.hidden = true));