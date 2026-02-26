#!/usr/bin/env python3
"""
ShaderLabDX12 – Code Complexity Report Generator
=================================================
Analyzes C/C++ source files for cyclomatic complexity and generates:
  • report_data.json  – machine-readable snapshot (embed or load externally)
  • report.html       – self-contained static HTML report with embedded data

Usage
-----
# Generate a fresh report from source
python tools/generate_complexity_report.py --src src --out reports

# Compare two snapshots and generate a delta report
python tools/generate_complexity_report.py --compare baseline.json current.json --out reports
"""

from __future__ import annotations

import argparse
import json
import math
import os
import re
import sys
from datetime import datetime, timezone
from pathlib import Path
from typing import Optional

# ---------------------------------------------------------------------------
# Cyclomatic complexity – simple regex-based C++ analyser
# ---------------------------------------------------------------------------
# Count: function definitions + each branch point (if/else if/for/while/
# switch-case/catch/&&/||/?:).  Each function starts at complexity 1.
# ---------------------------------------------------------------------------

_FUNCTION_RE = re.compile(
    r"""
    (?:[\w:<>*&~\s]+?)\s+          # return type (loose)
    ([\w:~<>]+)\s*                 # function name (captured)
    \((?:[^()]|\([^()]*\))*\)\s*   # parameter list
    (?:const|override|noexcept|\s)* # optional qualifiers
    \{                             # opening brace
    """,
    re.VERBOSE | re.MULTILINE,
)

_BRANCH_RE = re.compile(
    r"""
    \b(?:if|else\s+if|for|while|do|case|catch)\b  # keywords
    | [?]                                          # ternary
    | &&                                           # logical-and
    | \|\|                                         # logical-or
    """,
    re.VERBOSE,
)

_SINGLE_LINE_COMMENT = re.compile(r"//.*?$", re.MULTILINE)
_MULTI_LINE_COMMENT = re.compile(r"/\*.*?\*/", re.DOTALL)
_STRING_LITERAL = re.compile(r'"(?:[^"\\]|\\.)*"')


def _strip_comments_and_strings(src: str) -> str:
    src = _MULTI_LINE_COMMENT.sub(" ", src)
    src = _SINGLE_LINE_COMMENT.sub(" ", src)
    src = _STRING_LITERAL.sub('""', src)
    return src


def _extract_function_body(src: str, open_brace_pos: int) -> tuple[str, int]:
    """Return (body, end_pos) given position of the opening '{' in src."""
    depth = 0
    i = open_brace_pos
    for i, ch in enumerate(src[open_brace_pos:], open_brace_pos):
        if ch == "{":
            depth += 1
        elif ch == "}":
            depth -= 1
            if depth == 0:
                return src[open_brace_pos : i + 1], i + 1
    return src[open_brace_pos:], len(src)


def analyse_file(path: Path) -> dict:
    """Return a file-level complexity dict."""
    try:
        text = path.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return {"path": str(path), "lines": 0, "functions": [], "error": "unreadable"}

    lines = text.count("\n") + 1
    stripped = _strip_comments_and_strings(text)
    functions = []

    for m in _FUNCTION_RE.finditer(stripped):
        name = m.group(1)
        # Skip obvious non-functions (macros, control flow keywords, etc.)
        if name in {
            "if", "for", "while", "switch", "catch", "return", "else",
            "do", "case", "namespace", "class", "struct", "enum",
        }:
            continue
        open_brace = m.end() - 1  # position of '{'
        body, end_pos = _extract_function_body(stripped, open_brace)
        branch_count = len(_BRANCH_RE.findall(body))
        complexity = 1 + branch_count
        # Approximate line number from original text
        line_number = text[: m.start()].count("\n") + 1
        # Approximate function length
        func_lines = text[m.start() : end_pos].count("\n") + 1
        functions.append(
            {
                "name": name,
                "line": line_number,
                "complexity": complexity,
                "lines": func_lines,
            }
        )

    return {
        "path": str(path),
        "lines": lines,
        "functions": functions,
    }


def collect_sources(src_dir: Path, extensions: tuple[str, ...] = (".cpp", ".h", ".hpp", ".cxx", ".cc")) -> list[Path]:
    files = []
    for ext in extensions:
        files.extend(src_dir.rglob(f"*{ext}"))
    return sorted(files)


# ---------------------------------------------------------------------------
# Metrics aggregation
# ---------------------------------------------------------------------------

_THRESHOLD_MEDIUM = 5
_THRESHOLD_HIGH = 10
_THRESHOLD_CRITICAL = 20


def _classify(complexity: int) -> str:
    if complexity >= _THRESHOLD_CRITICAL:
        return "critical"
    if complexity >= _THRESHOLD_HIGH:
        return "high"
    if complexity >= _THRESHOLD_MEDIUM:
        return "medium"
    return "low"


def build_report_data(files_data: list[dict], project: str = "ShaderLabDX12") -> dict:
    all_funcs = [f for fd in files_data for f in fd.get("functions", [])]
    complexities = [f["complexity"] for f in all_funcs] or [0]

    avg = sum(complexities) / len(complexities)
    mx = max(complexities)

    outliers = sorted(
        [
            {**func, "file": fd["path"]}
            for fd in files_data
            for func in fd.get("functions", [])
            if func["complexity"] >= _THRESHOLD_HIGH
        ],
        key=lambda x: x["complexity"],
        reverse=True,
    )

    return {
        "generated_at": datetime.now(timezone.utc).isoformat(),
        "project": project,
        "summary": {
            "total_files": len(files_data),
            "total_functions": len(all_funcs),
            "total_lines": sum(fd.get("lines", 0) for fd in files_data),
            "avg_complexity": round(avg, 2),
            "max_complexity": mx,
            "complexity_distribution": {
                "low": sum(1 for c in complexities if c < _THRESHOLD_MEDIUM),
                "medium": sum(1 for c in complexities if _THRESHOLD_MEDIUM <= c < _THRESHOLD_HIGH),
                "high": sum(1 for c in complexities if _THRESHOLD_HIGH <= c < _THRESHOLD_CRITICAL),
                "critical": sum(1 for c in complexities if c >= _THRESHOLD_CRITICAL),
            },
        },
        "files": [
            {
                **fd,
                "max_complexity": max((f["complexity"] for f in fd["functions"]), default=0),
                "avg_complexity": round(
                    sum(f["complexity"] for f in fd["functions"]) / max(len(fd["functions"]), 1), 2
                ),
                "functions": sorted(fd["functions"], key=lambda f: f["complexity"], reverse=True),
            }
            for fd in sorted(files_data, key=lambda x: max((f["complexity"] for f in x.get("functions", [])), default=0), reverse=True)
        ],
        "outliers": outliers,
    }


# ---------------------------------------------------------------------------
# Delta comparison
# ---------------------------------------------------------------------------

def compute_delta(baseline: dict, current: dict) -> dict:
    """Return a delta dict comparing two report_data snapshots."""
    bs = baseline["summary"]
    cs = current["summary"]

    # Index functions by (file, name) for diffing
    def func_index(report: dict) -> dict[tuple[str, str], dict]:
        idx: dict[tuple[str, str], dict] = {}
        for fd in report["files"]:
            for func in fd["functions"]:
                idx[(fd["path"], func["name"])] = {**func, "file": fd["path"]}
        return idx

    b_idx = func_index(baseline)
    c_idx = func_index(current)

    improved, worsened, new_outliers, fixed_outliers = [], [], [], []
    all_keys = set(b_idx) | set(c_idx)

    for key in all_keys:
        b_func = b_idx.get(key)
        c_func = c_idx.get(key)
        if b_func and c_func:
            delta_c = c_func["complexity"] - b_func["complexity"]
            if delta_c > 0:
                worsened.append({**c_func, "delta_complexity": delta_c})
            elif delta_c < 0:
                improved.append({**c_func, "delta_complexity": delta_c})
            # Detect new or fixed outliers
            if b_func["complexity"] < _THRESHOLD_HIGH <= c_func["complexity"]:
                new_outliers.append({**c_func, "delta_complexity": delta_c})
            if c_func["complexity"] < _THRESHOLD_HIGH <= b_func["complexity"]:
                fixed_outliers.append({**c_func, "delta_complexity": delta_c, "was_complexity": b_func["complexity"]})
        elif c_func and not b_func:
            if c_func["complexity"] >= _THRESHOLD_HIGH:
                new_outliers.append({**c_func, "delta_complexity": c_func["complexity"]})
        elif b_func and not c_func:
            if b_func["complexity"] >= _THRESHOLD_HIGH:
                fixed_outliers.append({**b_func, "delta_complexity": -b_func["complexity"], "was_complexity": b_func["complexity"]})

    return {
        "baseline_date": baseline.get("generated_at"),
        "current_date": current.get("generated_at"),
        "summary_delta": {
            "total_files": cs["total_files"] - bs["total_files"],
            "total_functions": cs["total_functions"] - bs["total_functions"],
            "total_lines": cs["total_lines"] - bs["total_lines"],
            "avg_complexity": round(cs["avg_complexity"] - bs["avg_complexity"], 2),
            "max_complexity": cs["max_complexity"] - bs["max_complexity"],
            "distribution_delta": {
                k: cs["complexity_distribution"][k] - bs["complexity_distribution"][k]
                for k in bs["complexity_distribution"]
            },
        },
        "improved": sorted(improved, key=lambda x: x["delta_complexity"]),
        "worsened": sorted(worsened, key=lambda x: x["delta_complexity"], reverse=True),
        "new_outliers": new_outliers,
        "fixed_outliers": fixed_outliers,
    }


# ---------------------------------------------------------------------------
# HTML generation
# ---------------------------------------------------------------------------

_HTML_TEMPLATE = """\
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>{title}</title>
<style>
/* ── reset & base ── */
*, *::before, *::after {{ box-sizing: border-box; margin: 0; padding: 0; }}
body {{
  font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
  background: #0d1117; color: #c9d1d9; line-height: 1.6;
}}
a {{ color: #58a6ff; text-decoration: none; }}
a:hover {{ text-decoration: underline; }}
h1 {{ font-size: 1.7rem; font-weight: 600; }}
h2 {{ font-size: 1.25rem; font-weight: 600; margin-bottom: .5rem; }}
h3 {{ font-size: 1rem; font-weight: 600; }}
code {{ font-family: 'SFMono-Regular', Consolas, monospace; font-size: .85em; }}

/* ── layout ── */
.wrapper {{ max-width: 1280px; margin: 0 auto; padding: 1.5rem 1rem; }}
header {{ border-bottom: 1px solid #30363d; padding-bottom: 1rem; margin-bottom: 1.5rem; display: flex; align-items: center; gap: 1rem; flex-wrap: wrap; }}
header .meta {{ font-size: .8rem; color: #8b949e; margin-top: .25rem; }}

/* ── cards ── */
.card {{ background: #161b22; border: 1px solid #30363d; border-radius: 6px; padding: 1rem 1.25rem; }}
.grid-4 {{ display: grid; grid-template-columns: repeat(auto-fill, minmax(180px, 1fr)); gap: 1rem; margin-bottom: 1.5rem; }}
.stat-card {{ text-align: center; }}
.stat-card .value {{ font-size: 2rem; font-weight: 700; }}
.stat-card .label {{ font-size: .8rem; color: #8b949e; }}
.stat-card .sub {{ font-size: .75rem; margin-top: .2rem; }}

/* ── severity colours ── */
.low    {{ color: #3fb950; }}
.medium {{ color: #d29922; }}
.high   {{ color: #f78166; }}
.critical {{ color: #ff5f57; font-weight: 700; }}

/* ── badges ── */
.badge {{
  display: inline-block; font-size: .7rem; font-weight: 600;
  padding: .1rem .45rem; border-radius: 12px; vertical-align: middle;
}}
.badge-low      {{ background: #1f4a2a; color: #3fb950; }}
.badge-medium   {{ background: #3d2f00; color: #d29922; }}
.badge-high     {{ background: #4a1f1f; color: #f78166; }}
.badge-critical {{ background: #5c1a1a; color: #ff5f57; }}

.delta-pos {{ color: #f78166; }}
.delta-neg {{ color: #3fb950; }}
.delta-zero {{ color: #8b949e; }}

/* ── tabs ── */
.tabs {{ margin-bottom: 1.5rem; }}
.tab-bar {{
  display: flex; gap: .5rem; border-bottom: 2px solid #30363d;
  margin-bottom: 1.5rem; overflow-x: auto;
}}
.tab-btn {{
  background: none; border: none; color: #8b949e;
  padding: .6rem 1rem; cursor: pointer; font-size: .9rem; white-space: nowrap;
  border-bottom: 2px solid transparent; margin-bottom: -2px;
  transition: color .15s, border-color .15s;
}}
.tab-btn:hover {{ color: #c9d1d9; }}
.tab-btn.active {{ color: #58a6ff; border-bottom-color: #58a6ff; }}
.tab-panel {{ display: none; }}
.tab-panel.active {{ display: block; }}

/* ── accordion ── */
.accordion {{ border: 1px solid #30363d; border-radius: 6px; overflow: hidden; margin-bottom: .75rem; }}
.accordion-header {{
  width: 100%; background: #161b22; border: none; color: #c9d1d9;
  padding: .7rem 1rem; text-align: left; cursor: pointer;
  display: flex; justify-content: space-between; align-items: center;
  gap: .5rem; font-size: .9rem;
}}
.accordion-header:hover {{ background: #1c2128; }}
.accordion-header .chevron {{ transition: transform .2s; font-size: .75rem; color: #8b949e; }}
.accordion-header.open .chevron {{ transform: rotate(180deg); }}
.accordion-body {{ display: none; padding: 0 1rem 1rem; background: #0d1117; }}
.accordion-body.open {{ display: block; }}

/* ── heatmap ── */
.heatmap-grid {{
  display: grid;
  grid-template-columns: repeat(auto-fill, minmax(130px, 1fr));
  gap: 4px; margin-bottom: 1.5rem;
}}
.heatmap-cell {{
  border-radius: 4px; padding: .4rem .5rem; font-size: .7rem;
  overflow: hidden; text-overflow: ellipsis; white-space: nowrap;
  cursor: default; transition: transform .1s;
  border: 1px solid rgba(255,255,255,.05);
}}
.heatmap-cell:hover {{ transform: scale(1.05); z-index: 1; position: relative; overflow: visible; white-space: normal; word-break: break-all; }}
.hm-0 {{ background: #1c3a2a; color: #3fb950; }}
.hm-1 {{ background: #2e3a0e; color: #7fd14e; }}
.hm-2 {{ background: #3d2f00; color: #d29922; }}
.hm-3 {{ background: #4a1f1f; color: #f78166; }}
.hm-4 {{ background: #5c1a1a; color: #ff5f57; }}

/* ── tables ── */
table {{ width: 100%; border-collapse: collapse; font-size: .88rem; }}
thead th {{ background: #161b22; text-align: left; padding: .5rem .75rem; border-bottom: 2px solid #30363d; color: #8b949e; font-weight: 600; white-space: nowrap; }}
tbody td {{ padding: .45rem .75rem; border-bottom: 1px solid #21262d; }}
tbody tr:last-child td {{ border-bottom: none; }}
tbody tr:hover td {{ background: #161b22; }}
.sortable {{ cursor: pointer; user-select: none; }}
.sortable::after {{ content: ' ↕'; font-size: .7em; opacity: .5; }}

/* ── bar ── */
.bar-track {{ background: #21262d; border-radius: 4px; height: 6px; min-width: 60px; }}
.bar-fill {{ height: 6px; border-radius: 4px; }}

/* ── search ── */
.search-box {{
  width: 100%; max-width: 400px; background: #0d1117; border: 1px solid #30363d;
  border-radius: 6px; padding: .45rem .75rem; color: #c9d1d9; font-size: .9rem; margin-bottom: 1rem;
}}
.search-box:focus {{ outline: none; border-color: #58a6ff; }}

/* ── distribution bar ── */
.dist-bar {{ display: flex; height: 14px; border-radius: 4px; overflow: hidden; margin-top: .5rem; }}
.dist-bar span {{ height: 100%; transition: width .3s; }}

/* ── legend ── */
.legend {{ display: flex; gap: .75rem; flex-wrap: wrap; margin-bottom: 1rem; font-size: .8rem; }}
.legend-item {{ display: flex; align-items: center; gap: .3rem; }}
.legend-dot {{ width: 10px; height: 10px; border-radius: 2px; }}

/* ── delta section ── */
.delta-summary-grid {{
  display: grid; grid-template-columns: repeat(auto-fill, minmax(160px, 1fr)); gap: 1rem; margin-bottom: 1.5rem;
}}
.delta-card {{
  background: #161b22; border: 1px solid #30363d; border-radius: 6px; padding: .75rem 1rem; text-align: center;
}}
.delta-card .d-value {{ font-size: 1.6rem; font-weight: 700; }}
.delta-card .d-label {{ font-size: .75rem; color: #8b949e; }}

/* ── footer ── */
footer {{ margin-top: 2rem; padding-top: 1rem; border-top: 1px solid #30363d; font-size: .78rem; color: #8b949e; text-align: center; }}
</style>
</head>
<body>
<div class="wrapper">
  <header>
    <div>
      <h1>📊 {title}</h1>
      <div class="meta">Generated: {generated_at} &nbsp;|&nbsp; Source: <code>{project}</code></div>
    </div>
  </header>
  <!-- ── embedded data ── -->
  <script id="report-data" type="application/json">{report_data_json}</script>
  <div id="app"></div>
</div>

<script>
// ── helpers ─────────────────────────────────────────────────────────────────
const $ = (sel, ctx = document) => ctx.querySelector(sel);
const $$ = (sel, ctx = document) => [...ctx.querySelectorAll(sel)];
const el = (tag, attrs = {{}}, ...children) => {{
  const e = document.createElement(tag);
  Object.entries(attrs).forEach(([k, v]) => {{
    if (k === 'className') e.className = v;
    else if (k.startsWith('on')) e.addEventListener(k.slice(2).toLowerCase(), v);
    else e.setAttribute(k, v);
  }});
  children.flat(Infinity).forEach(c => {{
    if (c == null) return;
    if (typeof c === 'string' || typeof c === 'number') e.appendChild(document.createTextNode(String(c)));
    else if (c instanceof Node) e.appendChild(c);
  }});
  return e;
}};

function getBadgeClass(c) {{
  if (c >= 20) return 'badge-critical';
  if (c >= 10) return 'badge-high';
  if (c >= 5)  return 'badge-medium';
  return 'badge-low';
}}
function getSeverityClass(c) {{
  if (c >= 20) return 'critical';
  if (c >= 10) return 'high';
  if (c >= 5)  return 'medium';
  return 'low';
}}
function getHeatmapClass(c) {{
  if (c >= 20) return 'hm-4';
  if (c >= 10) return 'hm-3';
  if (c >= 5)  return 'hm-2';
  if (c >= 3)  return 'hm-1';
  return 'hm-0';
}}
function getDeltaClass(d) {{
  if (d > 0) return 'delta-pos';
  if (d < 0) return 'delta-neg';
  return 'delta-zero';
}}
function fmt(n, prec = 1) {{ return Number(n).toFixed(prec); }}
function relPath(p) {{ return p.replace(/\\\\/g, '/').replace(/^.*?src[/\\\\]/, 'src/'); }}

// ── tab machinery ────────────────────────────────────────────────────────────
function buildTabs(tabs) {{
  const bar = el('div', {{className: 'tab-bar'}});
  const panels = [];
  tabs.forEach((t, i) => {{
    const btn = el('button', {{className: 'tab-btn' + (i === 0 ? ' active' : ''), onClick: () => activate(i)}}, t.label);
    bar.appendChild(btn);
    const panel = el('div', {{className: 'tab-panel' + (i === 0 ? ' active' : '')}});
    if (typeof t.content === 'function') t.content(panel);
    else panel.innerHTML = t.content;
    panels.push(panel);
  }});
  function activate(idx) {{
    $$('.tab-btn', bar).forEach((b, i) => b.classList.toggle('active', i === idx));
    panels.forEach((p, i) => p.classList.toggle('active', i === idx));
  }}
  const wrap = el('div', {{className: 'tabs'}});
  wrap.appendChild(bar);
  panels.forEach(p => wrap.appendChild(p));
  return wrap;
}}

// ── accordion ───────────────────────────────────────────────────────────────
function buildAccordion(title, buildBody, defaultOpen = false) {{
  const acc = el('div', {{className: 'accordion'}});
  const hdr = el('button', {{className: 'accordion-header' + (defaultOpen ? ' open' : '')}});
  hdr.innerHTML = title + '<span class="chevron">▼</span>';
  const body = el('div', {{className: 'accordion-body' + (defaultOpen ? ' open' : '')}});
  buildBody(body);
  hdr.addEventListener('click', () => {{
    hdr.classList.toggle('open');
    body.classList.toggle('open');
  }});
  acc.appendChild(hdr);
  acc.appendChild(body);
  return acc;
}}

// ── sortable table ───────────────────────────────────────────────────────────
function buildTable(headers, rows, opts = {{}}) {{
  const wrap = el('div', {{style: 'overflow-x:auto'}});
  const tbl = el('table');
  const thead = el('thead');
  const hr = el('tr');
  headers.forEach((h, hi) => {{
    const th = el('th', {{className: 'sortable'}}, String(h.label ?? (typeof h === 'string' ? h : '')));
    th.addEventListener('click', () => {{
      const key = h.key || hi;
      rows.sort((a, b) => {{
        const av = a[key] ?? a[hi] ?? '', bv = b[key] ?? b[hi] ?? '';
        return typeof av === 'number' ? (th._asc ? av - bv : bv - av) : String(av).localeCompare(String(bv));
      }});
      th._asc = !th._asc;
      populateBody();
    }});
    hr.appendChild(th);
  }});
  thead.appendChild(hr);
  tbl.appendChild(thead);
  const tbody = el('tbody');
  tbl.appendChild(tbody);
  wrap.appendChild(tbl);

  function populateBody() {{
    tbody.innerHTML = '';
    rows.forEach(row => {{
      const tr = el('tr');
      const cells = opts.renderRow ? opts.renderRow(row, el) : Object.values(row);
      cells.forEach(cell => {{
        if (cell instanceof HTMLElement) {{ tr.appendChild(el('td', {{}}, cell)); }}
        else {{ tr.appendChild(el('td', {{}}, String(cell ?? ''))); }}
      }});
      tbody.appendChild(tr);
    }});
  }}
  populateBody();
  return wrap;
}}

// ── bar helper ───────────────────────────────────────────────────────────────
function complexityBar(value, max, cls) {{
  const pct = max > 0 ? Math.round((value / max) * 100) : 0;
  const track = el('div', {{className: 'bar-track'}});
  const fill = el('div', {{className: 'bar-fill ' + cls, style: `width:${{pct}}%; background:currentColor`}});
  track.appendChild(fill);
  return track;
}}

// ── main render ──────────────────────────────────────────────────────────────
const data = JSON.parse(document.getElementById('report-data').textContent);
const delta = data.delta || null;
const report = delta ? data.current : data;
const baseline = delta ? data.baseline : null;
const app = document.getElementById('app');

// ────────────────────────────────────────────────────────────────────────────
// Summary cards
// ────────────────────────────────────────────────────────────────────────────
function renderSummaryCards(s, container) {{
  const dist = s.complexity_distribution;
  const total = s.total_functions || 1;
  const cards = [
    {{v: s.total_files, l: 'Files Analysed', cls: ''}},
    {{v: s.total_functions, l: 'Functions', cls: ''}},
    {{v: fmt(s.avg_complexity), l: 'Avg Complexity', cls: getSeverityClass(s.avg_complexity)}},
    {{v: s.max_complexity, l: 'Max Complexity', cls: getSeverityClass(s.max_complexity)}},
    {{v: dist.critical, l: 'Critical (≥20)', cls: 'critical'}},
    {{v: dist.high, l: 'High (≥10)', cls: 'high'}},
  ];
  const grid = el('div', {{className: 'grid-4'}});
  cards.forEach(c => {{
    const card = el('div', {{className: 'card stat-card'}});
    card.appendChild(el('div', {{className: 'value ' + c.cls}}, String(c.v)));
    card.appendChild(el('div', {{className: 'label'}}, c.l));
    grid.appendChild(card);
  }});
  container.appendChild(grid);

  // distribution bar
  const card = el('div', {{className: 'card'}});
  card.appendChild(el('h3', {{}}, 'Complexity Distribution'));
  const distBar = el('div', {{className: 'dist-bar', style: 'margin-top:.5rem'}});
  const colors = {{low:'#3fb950', medium:'#d29922', high:'#f78166', critical:'#ff5f57'}};
  ['low','medium','high','critical'].forEach(k => {{
    const pct = Math.round((dist[k] / total) * 100);
    const span = el('span', {{
      title: `${{k}}: ${{dist[k]}} (${{pct}}%)`,
      style: `width:${{pct}}%; background:${{colors[k]}};`
    }});
    distBar.appendChild(span);
  }});
  card.appendChild(distBar);
  const legend = el('div', {{className: 'legend', style: 'margin-top:.5rem'}});
  ['low','medium','high','critical'].forEach(k => {{
    const item = el('div', {{className: 'legend-item'}});
    item.appendChild(el('div', {{className: 'legend-dot', style: `background:${{colors[k]}}`}}));
    item.appendChild(document.createTextNode(`${{k.charAt(0).toUpperCase()+k.slice(1)}}: ${{dist[k]}}`));
    legend.appendChild(item);
  }});
  card.appendChild(legend);
  container.appendChild(card);
}}

// ────────────────────────────────────────────────────────────────────────────
// Heatmap
// ────────────────────────────────────────────────────────────────────────────
function renderHeatmap(files, container) {{
  container.appendChild(el('h2', {{}}, '🌡 Complexity Heatmap'));
  const legend = el('div', {{className: 'legend', style: 'margin-bottom:.75rem'}});
  [['hm-0','Low (<3)'],['hm-1','Moderate (3-4)'],['hm-2','Medium (5-9)'],['hm-3','High (10-19)'],['hm-4','Critical (≥20)']].forEach(([cls, lbl]) => {{
    const item = el('div', {{className: 'legend-item'}});
    item.appendChild(el('div', {{className: 'legend-dot heatmap-cell '+cls, style:'width:16px;height:16px;padding:0;display:inline-block'}}));
    item.appendChild(document.createTextNode(lbl));
    legend.appendChild(item);
  }});
  container.appendChild(legend);

  const grid = el('div', {{className: 'heatmap-grid'}});
  files.forEach(f => {{
    const maxC = f.max_complexity || 0;
    const name = relPath(f.path).split('/').pop();
    const cell = el('div', {{
      className: 'heatmap-cell ' + getHeatmapClass(maxC),
      title: `${{relPath(f.path)}}\\nMax: ${{maxC}} | Avg: ${{f.avg_complexity}} | Funcs: ${{f.functions.length}}`
    }});
    cell.textContent = name;
    grid.appendChild(cell);
  }});
  container.appendChild(grid);
}}

// ────────────────────────────────────────────────────────────────────────────
// Files tab
// ────────────────────────────────────────────────────────────────────────────
function renderFilesTab(files, container) {{
  const search = el('input', {{className: 'search-box', type: 'text', placeholder: 'Filter files…'}});
  container.appendChild(search);
  const listWrap = el('div');
  container.appendChild(listWrap);

  function render(filtered) {{
    listWrap.innerHTML = '';
    filtered.forEach((f, i) => {{
      const maxC = f.max_complexity || 0;
      const hdrHtml = `
        <span><code>${{relPath(f.path)}}</code></span>
        <span style="display:flex;gap:.75rem;align-items:center;flex-shrink:0">
          <span class="badge ${{getBadgeClass(maxC)}}">${{maxC}}</span>
          <span style="color:#8b949e;font-size:.8rem">${{f.functions.length}} fn | ${{f.lines}} lines</span>
        </span>`;
      const acc = buildAccordion(hdrHtml, body => {{
        if (!f.functions.length) {{
          body.appendChild(el('p', {{style:'color:#8b949e;padding:.5rem 0'}}, 'No functions found.'));
          return;
        }}
        const rows = f.functions.map(fn => ({{
          name: fn.name, complexity: fn.complexity, line: fn.line, lines: fn.lines
        }}));
        const maxFnC = Math.max(...rows.map(r => r.complexity));
        body.appendChild(buildTable(
          [
            {{label:'Function', key:'name'}},
            {{label:'Complexity', key:'complexity'}},
            {{label:'', key:'bar'}},
            {{label:'Line', key:'line'}},
            {{label:'Func Lines', key:'lines'}},
          ],
          rows,
          {{renderRow: (row, el) => [
            el('code', {{}}, row.name),
            el('span', {{className: getSeverityClass(row.complexity)}}, String(row.complexity)),
            complexityBar(row.complexity, maxFnC, getSeverityClass(row.complexity)),
            String(row.line),
            String(row.lines),
          ]}}
        ));
      }}, i === 0 && maxC >= 10);
      listWrap.appendChild(acc);
    }});
  }}

  search.addEventListener('input', () => {{
    const q = search.value.toLowerCase();
    render(files.filter(f => relPath(f.path).toLowerCase().includes(q)));
  }});
  render(files);
}}

// ────────────────────────────────────────────────────────────────────────────
// Functions tab
// ────────────────────────────────────────────────────────────────────────────
function renderFunctionsTab(files, container) {{
  const allFuncs = files.flatMap(f => f.functions.map(fn => ({{
    file: relPath(f.path), name: fn.name,
    complexity: fn.complexity, line: fn.line, lines: fn.lines
  }})));
  const maxC = Math.max(...allFuncs.map(f => f.complexity), 1);

  const search = el('input', {{className: 'search-box', type: 'text', placeholder: 'Filter functions…'}});
  container.appendChild(search);
  const wrap = el('div');
  container.appendChild(wrap);

  function render(funcs) {{
    wrap.innerHTML = '';
    wrap.appendChild(buildTable(
      [
        {{label:'Function', key:'name'}},
        {{label:'File', key:'file'}},
        {{label:'Complexity', key:'complexity'}},
        {{label:'', key:'bar'}},
        {{label:'Line', key:'line'}},
        {{label:'Func Lines', key:'lines'}},
      ],
      funcs,
      {{renderRow: (row, el) => [
        el('code', {{}}, row.name),
        el('span', {{style:'font-size:.8rem;color:#8b949e'}}, row.file),
        el('span', {{className: 'badge ' + getBadgeClass(row.complexity)}}, String(row.complexity)),
        complexityBar(row.complexity, maxC, getSeverityClass(row.complexity)),
        String(row.line),
        String(row.lines),
      ]}}
    ));
  }}

  search.addEventListener('input', () => {{
    const q = search.value.toLowerCase();
    render(allFuncs.filter(f => f.name.toLowerCase().includes(q) || f.file.toLowerCase().includes(q)));
  }});
  render(allFuncs);
}}

// ────────────────────────────────────────────────────────────────────────────
// Outliers tab
// ────────────────────────────────────────────────────────────────────────────
function renderOutliersTab(outliers, container) {{
  if (!outliers.length) {{
    container.appendChild(el('p', {{style:'color:#3fb950;padding:.5rem 0'}}, '✅ No outliers found (all functions have complexity < 10).'));
    return;
  }}
  container.appendChild(el('p', {{style:'color:#8b949e;margin-bottom:.75rem'}},
    `${{outliers.length}} function(s) with cyclomatic complexity ≥ 10. ` +
    'Consider refactoring these to reduce risk and improve testability.'));

  const maxC = Math.max(...outliers.map(o => o.complexity), 1);
  container.appendChild(buildTable(
    [
      {{label:'Severity', key:'complexity'}},
      {{label:'Function', key:'name'}},
      {{label:'File', key:'file'}},
      {{label:'Complexity', key:'complexity'}},
      {{label:'', key:'bar'}},
      {{label:'Line', key:'line'}},
    ],
    outliers,
    {{renderRow: (row, el) => [
      el('span', {{className: 'badge ' + getBadgeClass(row.complexity)}}, row.complexity >= 20 ? 'Critical' : 'High'),
      el('code', {{}}, row.name),
      el('span', {{style:'font-size:.8rem;color:#8b949e'}}, relPath(row.file)),
      el('span', {{className: getSeverityClass(row.complexity)}}, String(row.complexity)),
      complexityBar(row.complexity, maxC, getSeverityClass(row.complexity)),
      String(row.line),
    ]}}
  ));
}}

// ────────────────────────────────────────────────────────────────────────────
// Delta tab
// ────────────────────────────────────────────────────────────────────────────
function renderDeltaTab(delta, container) {{
  const sd = delta.summary_delta;

  // Summary cards
  const grid = el('div', {{className: 'delta-summary-grid'}});
  [
    {{v: sd.avg_complexity, l: 'Avg Complexity Δ', prec: 2}},
    {{v: sd.max_complexity, l: 'Max Complexity Δ', prec: 0}},
    {{v: sd.total_functions, l: 'Functions Δ', prec: 0}},
    {{v: sd.total_lines, l: 'Lines Δ', prec: 0}},
    {{v: delta.new_outliers.length, l: 'New Outliers', prec: 0, forceNeg: true}},
    {{v: delta.fixed_outliers.length, l: 'Fixed Outliers', prec: 0, forcePos: true}},
  ].forEach(c => {{
    const isPos = c.forcePos ? c.v > 0 : (c.forceNeg ? false : c.v > 0);
    const isNeg = c.forceNeg ? c.v > 0 : (c.forcePos ? false : c.v < 0);
    const cls = isPos ? 'delta-pos' : (isNeg ? 'delta-neg' : 'delta-zero');
    const sign = c.v > 0 ? '+' : '';
    const card = el('div', {{className: 'delta-card'}});
    card.appendChild(el('div', {{className: 'd-value ' + cls}}, sign + fmt(c.v, c.prec)));
    card.appendChild(el('div', {{className: 'd-label'}}, c.l));
    grid.appendChild(card);
  }});
  container.appendChild(grid);

  // Dates
  container.appendChild(el('p', {{style:'color:#8b949e;font-size:.8rem;margin-bottom:1rem'}},
    `Baseline: ${{delta.baseline_date || 'unknown'}}  →  Current: ${{delta.current_date || 'unknown'}}`));

  // New outliers
  const newOut = el('div', {{style:'margin-bottom:1.5rem'}});
  newOut.appendChild(el('h2', {{}}, `🔴 New Outliers (${{delta.new_outliers.length}})`));
  if (!delta.new_outliers.length) {{
    newOut.appendChild(el('p', {{style:'color:#3fb950'}}, '✅ No new outliers introduced.'));
  }} else {{
    newOut.appendChild(buildTable(
      [{{label:'Function',key:'name'}},{{label:'File',key:'file'}},{{label:'Complexity',key:'complexity'}},{{label:'Delta',key:'delta_complexity'}}],
      delta.new_outliers,
      {{renderRow: (row, el) => [
        el('code',{{}},row.name),
        el('span',{{style:'font-size:.8rem;color:#8b949e'}},relPath(row.file)),
        el('span',{{className:'badge '+getBadgeClass(row.complexity)}},String(row.complexity)),
        el('span',{{className:'delta-pos'}},'+'+row.delta_complexity),
      ]}}
    ));
  }}
  container.appendChild(newOut);

  // Fixed outliers
  const fixedOut = el('div', {{style:'margin-bottom:1.5rem'}});
  fixedOut.appendChild(el('h2', {{}}, `🟢 Fixed Outliers (${{delta.fixed_outliers.length}})`));
  if (!delta.fixed_outliers.length) {{
    fixedOut.appendChild(el('p', {{style:'color:#8b949e'}}, 'No previously-flagged outliers were resolved.'));
  }} else {{
    fixedOut.appendChild(buildTable(
      [{{label:'Function',key:'name'}},{{label:'File',key:'file'}},{{label:'Was',key:'was_complexity'}},{{label:'Now',key:'complexity'}},{{label:'Delta',key:'delta_complexity'}}],
      delta.fixed_outliers,
      {{renderRow: (row, el) => [
        el('code',{{}},row.name),
        el('span',{{style:'font-size:.8rem;color:#8b949e'}},relPath(row.file)),
        el('span',{{className:'high'}},String(row.was_complexity)),
        el('span',{{className:'low'}},String(row.complexity)),
        el('span',{{className:'delta-neg'}},String(row.delta_complexity)),
      ]}}
    ));
  }}
  container.appendChild(fixedOut);

  // Worsened
  const worsenedDiv = el('div', {{style:'margin-bottom:1.5rem'}});
  worsenedDiv.appendChild(el('h2', {{}}, `📈 Most Worsened (${{Math.min(delta.worsened.length,20)}} of ${{delta.worsened.length}})`));
  if (!delta.worsened.length) {{
    worsenedDiv.appendChild(el('p',{{style:'color:#3fb950'}},'✅ No functions became more complex.'));
  }} else {{
    worsenedDiv.appendChild(buildTable(
      [{{label:'Function',key:'name'}},{{label:'File',key:'file'}},{{label:'Complexity',key:'complexity'}},{{label:'Delta',key:'delta_complexity'}}],
      delta.worsened.slice(0,20),
      {{renderRow: (row, el) => [
        el('code',{{}},row.name),
        el('span',{{style:'font-size:.8rem;color:#8b949e'}},relPath(row.file)),
        el('span',{{className:'badge '+getBadgeClass(row.complexity)}},String(row.complexity)),
        el('span',{{className:'delta-pos'}},'+'+row.delta_complexity),
      ]}}
    ));
  }}
  container.appendChild(worsenedDiv);

  // Improved
  const improvedDiv = el('div');
  improvedDiv.appendChild(el('h2', {{}}, `📉 Most Improved (${{Math.min(delta.improved.length,20)}} of ${{delta.improved.length}})`));
  if (!delta.improved.length) {{
    improvedDiv.appendChild(el('p',{{style:'color:#8b949e'}},'No functions improved in complexity.'));
  }} else {{
    improvedDiv.appendChild(buildTable(
      [{{label:'Function',key:'name'}},{{label:'File',key:'file'}},{{label:'Complexity',key:'complexity'}},{{label:'Delta',key:'delta_complexity'}}],
      delta.improved.slice(0,20),
      {{renderRow: (row, el) => [
        el('code',{{}},row.name),
        el('span',{{style:'font-size:.8rem;color:#8b949e'}},relPath(row.file)),
        el('span',{{className:'badge '+getBadgeClass(row.complexity)}},String(row.complexity)),
        el('span',{{className:'delta-neg'}},String(row.delta_complexity)),
      ]}}
    ));
  }}
  container.appendChild(improvedDiv);
}}

// ────────────────────────────────────────────────────────────────────────────
// Wire up
// ────────────────────────────────────────────────────────────────────────────
(function main() {{
  const s = report.summary;
  const files = report.files;
  const outliers = report.outliers;

  const tabs = [
    {{
      label: '📋 Summary',
      content: panel => {{
        renderSummaryCards(s, panel);
        renderHeatmap(files, panel);
      }}
    }},
    {{
      label: `📁 Files (${{files.length}})`,
      content: panel => renderFilesTab(files, panel)
    }},
    {{
      label: `ƒ Functions (${{(report.summary.total_functions || 0)}})`,
      content: panel => renderFunctionsTab(files, panel)
    }},
    {{
      label: `⚠ Outliers (${{outliers.length}})`,
      content: panel => renderOutliersTab(outliers, panel)
    }},
  ];

  if (delta) {{
    tabs.push({{
      label: `Δ Delta`,
      content: panel => renderDeltaTab(delta, panel)
    }});
  }}

  app.appendChild(buildTabs(tabs));
}})();
</script>

<footer>
  ShaderLabDX12 Complexity Report &nbsp;|&nbsp; Generated {generated_at}
</footer>
</body>
</html>
"""


def render_html(report_data: dict, title: str = "Code Complexity Report") -> str:
    generated_at = report_data.get("generated_at", "")
    project = report_data.get("project", "")
    return _HTML_TEMPLATE.format(
        title=title,
        generated_at=generated_at,
        project=project,
        report_data_json=json.dumps(report_data, separators=(",", ":")),
    )


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def main(argv: Optional[list[str]] = None) -> int:
    parser = argparse.ArgumentParser(
        description="Generate a complexity report for C/C++ source files.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    parser.add_argument(
        "--src", "-s",
        type=Path,
        default=Path("src"),
        help="Source root directory (default: src)",
    )
    parser.add_argument(
        "--out", "-o",
        type=Path,
        default=Path("reports"),
        help="Output directory (default: reports)",
    )
    parser.add_argument(
        "--project", "-p",
        default="ShaderLabDX12",
        help="Project name embedded in the report",
    )
    parser.add_argument(
        "--compare",
        nargs=2,
        metavar=("BASELINE_JSON", "CURRENT_JSON"),
        help="Compare two report_data.json snapshots and generate a delta report",
    )
    parser.add_argument(
        "--title",
        default=None,
        help="Override the HTML report title",
    )
    args = parser.parse_args(argv)

    out_dir: Path = args.out
    out_dir.mkdir(parents=True, exist_ok=True)

    if args.compare:
        baseline_path, current_path = Path(args.compare[0]), Path(args.compare[1])
        print(f"Loading baseline: {baseline_path}")
        baseline = json.loads(baseline_path.read_text(encoding="utf-8"))
        print(f"Loading current:  {current_path}")
        current = json.loads(current_path.read_text(encoding="utf-8"))
        delta = compute_delta(baseline, current)
        combined = {
            "generated_at": datetime.now(timezone.utc).isoformat(),
            "project": current.get("project", args.project),
            "baseline": baseline,
            "current": current,
            "delta": delta,
        }
        title = args.title or f"{combined['project']} – Delta Report"
        json_path = out_dir / "report_delta.json"
        html_path = out_dir / "report_delta.html"
        json_path.write_text(json.dumps(combined, indent=2, ensure_ascii=False), encoding="utf-8")
        print(f"Delta JSON: {json_path}")
        html_path.write_text(render_html(combined, title), encoding="utf-8")
        print(f"Delta HTML: {html_path}")
        _print_delta_summary(delta)
    else:
        src_dir: Path = args.src
        if not src_dir.exists():
            print(f"ERROR: source directory not found: {src_dir}", file=sys.stderr)
            return 1

        print(f"Analysing {src_dir} …")
        sources = collect_sources(src_dir)
        print(f"Found {len(sources)} source file(s).")
        files_data = [analyse_file(f) for f in sources]
        report_data = build_report_data(files_data, args.project)
        title = args.title or f"{args.project} – Complexity Report"

        json_path = out_dir / "report_data.json"
        html_path = out_dir / "report.html"
        json_path.write_text(json.dumps(report_data, indent=2, ensure_ascii=False), encoding="utf-8")
        print(f"Data JSON : {json_path}")
        html_path.write_text(render_html(report_data, title), encoding="utf-8")
        print(f"Report    : {html_path}")
        _print_summary(report_data["summary"])

    return 0


def _print_summary(s: dict) -> None:
    print()
    print("─" * 40)
    print(f"  Files      : {s['total_files']}")
    print(f"  Functions  : {s['total_functions']}")
    print(f"  Avg CC     : {s['avg_complexity']}")
    print(f"  Max CC     : {s['max_complexity']}")
    dist = s["complexity_distribution"]
    print(f"  Low        : {dist['low']}")
    print(f"  Medium     : {dist['medium']}")
    print(f"  High       : {dist['high']}")
    print(f"  Critical   : {dist['critical']}")
    print("─" * 40)


def _print_delta_summary(d: dict) -> None:
    sd = d["summary_delta"]
    print()
    print("─" * 40)
    print(f"  Avg CC Δ   : {sd['avg_complexity']:+.2f}")
    print(f"  Max CC Δ   : {sd['max_complexity']:+d}")
    print(f"  Functions Δ: {sd['total_functions']:+d}")
    print(f"  New outliers : {len(d['new_outliers'])}")
    print(f"  Fixed outliers: {len(d['fixed_outliers'])}")
    print(f"  Improved   : {len(d['improved'])}")
    print(f"  Worsened   : {len(d['worsened'])}")
    print("─" * 40)


if __name__ == "__main__":
    sys.exit(main())
