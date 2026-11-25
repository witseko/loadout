#pragma once

#include <string_view>

#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Woverlength-strings"
#endif

/// Embedded zero-dependency Web UI assets served by `loadout ui`.
namespace loadout::ui {

inline constexpr std::string_view INDEX_HTML = R"LOADOUT_HTML(<!doctype html>
<html lang="en" data-theme="dark">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <meta name="color-scheme" content="dark light">
  <title>loadout - Skill Patchbay</title>
  <link rel="icon" type="image/svg+xml" href="data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 32 32'%3E%3Crect width='32' height='32' rx='7' fill='%230B0D10' stroke='%232C323D' stroke-width='2'/%3E%3Ccircle cx='10' cy='11' r='3' fill='%233DDC97'/%3E%3Ccircle cx='22' cy='21' r='3' fill='%233DDC97'/%3E%3Cpath d='M13 11h3a3 3 0 0 1 3 3v4a3 3 0 0 0 3 3' fill='none' stroke='%233DDC97' stroke-width='2.2' stroke-linecap='round'/%3E%3Ccircle cx='22' cy='11' r='2.2' fill='%23C8D46A'/%3E%3Ccircle cx='10' cy='21' r='2.2' fill='%232C323D'/%3E%3C/svg%3E">
  <link rel="stylesheet" href="/app.css">
</head>
<body>
  <loadout-switchboard mode="standalone"></loadout-switchboard>
  <script src="/switchboard.js" defer></script>
</body>
</html>
)LOADOUT_HTML";

inline constexpr std::string_view APP_CSS = R"LOADOUT_CSS(/* loadout: Dark-first precision patchbay stylesheet (ported from prototype/index.html) */
:root {
  --loadout-bg: #0B0D10;
  --loadout-surface: #12151B;
  --loadout-surface-2: #171B22;
  --loadout-surface-3: #1D222A;
  --loadout-border: #20252E;
  --loadout-border-2: #2C323D;
  --loadout-text: #E7E9EC;
  --loadout-dim: #98A1B0;
  --loadout-muted: #5D6676;
  --loadout-emerald: #3DDC97;
  --loadout-emerald-dim: rgba(61, 220, 151, .14);
  --loadout-amber: #F5B544;
  --loadout-amber-dim: rgba(245, 181, 68, .14);
  --loadout-rose: #F2647A;
  --loadout-rose-dim: rgba(242, 100, 122, .14);
  --loadout-sky: #6CB6FF;
  --loadout-sky-dim: rgba(108, 182, 255, .14);
  --loadout-accent: #E0DDBD;
  --loadout-accent-dim: rgba(224, 221, 189, .16);
  --loadout-brand-o: #C8D46A;
  --loadout-on-accent: #0B0D10;
  --loadout-shadow: 0 24px 64px rgba(0, 0, 0, .55), 0 0 0 1px var(--loadout-border-2);
  --loadout-font-mono: "JetBrains Mono", "SF Mono", ui-monospace, Menlo, Consolas, monospace;
  --loadout-font-sans: Inter, ui-sans-serif, system-ui, -apple-system, "Segoe UI", sans-serif;
  color-scheme: dark;
}

:root[data-theme="light"],
loadout-switchboard[theme="light"],
loadout-switchboard[data-theme="light"] {
  --loadout-bg: #F2F2EE;
  --loadout-surface: #FFFFFF;
  --loadout-surface-2: #F7F7F4;
  --loadout-surface-3: #EDEDE8;
  --loadout-border: #E1E1DA;
  --loadout-border-2: #CFCFC7;
  --loadout-text: #14161A;
  --loadout-dim: #596070;
  --loadout-muted: #8C929E;
  --loadout-emerald: #0C9466;
  --loadout-emerald-dim: rgba(12, 148, 102, .12);
  --loadout-amber: #B06F0E;
  --loadout-amber-dim: rgba(176, 111, 14, .12);
  --loadout-rose: #CF3553;
  --loadout-rose-dim: rgba(207, 53, 83, .10);
  --loadout-sky: #2273C9;
  --loadout-sky-dim: rgba(34, 115, 201, .10);
  --loadout-accent: #6F6A3A;
  --loadout-accent-dim: rgba(111, 106, 58, .14);
  --loadout-brand-o: #6E7D12;
  --loadout-on-accent: #FFFFFF;
  --loadout-shadow: 0 24px 64px rgba(20, 22, 26, .18), 0 0 0 1px var(--loadout-border-2);
  color-scheme: light;
}

html, body {
  margin: 0;
  height: 100%;
  background: var(--loadout-bg);
  color: var(--loadout-text);
  font: 13px/1.45 var(--loadout-font-sans);
  -webkit-font-smoothing: antialiased;
}

loadout-switchboard {
  --bg: var(--loadout-bg);
  --surface: var(--loadout-surface);
  --surface-2: var(--loadout-surface-2);
  --surface-3: var(--loadout-surface-3);
  --border: var(--loadout-border);
  --border-2: var(--loadout-border-2);
  --text: var(--loadout-text);
  --dim: var(--loadout-dim);
  --faint: var(--loadout-muted);
  --emerald: var(--loadout-emerald);
  --emerald-dim: var(--loadout-emerald-dim);
  --amber: var(--loadout-amber);
  --amber-dim: var(--loadout-amber-dim);
  --rose: var(--loadout-rose);
  --rose-dim: var(--loadout-rose-dim);
  --sky: var(--loadout-sky);
  --sky-dim: var(--loadout-sky-dim);
  --accent: var(--loadout-accent);
  --accent-dim: var(--loadout-accent-dim);
  --brand-o: var(--loadout-brand-o);
  --on-accent: var(--loadout-on-accent);
  --shadow: var(--loadout-shadow);
  --mono: var(--loadout-font-mono);
  --sans: var(--loadout-font-sans);
  display: flex;
  flex-direction: column;
  height: 100vh;
  width: 100%;
  min-height: 360px;
  background: var(--bg);
  color: var(--text);
  font: 13px/1.45 var(--sans);
  -webkit-font-smoothing: antialiased;
  position: relative;
  overflow: hidden;
  box-sizing: border-box;
}

loadout-switchboard[mode="embedded"] {
  height: 100%;
  min-height: 320px;
}

loadout-switchboard *, loadout-switchboard *::before, loadout-switchboard *::after {
  box-sizing: border-box;
}

loadout-switchboard button,
loadout-switchboard input,
loadout-switchboard select,
loadout-switchboard textarea {
  font: inherit;
  color: inherit;
}

loadout-switchboard button { cursor: pointer; }
loadout-switchboard :focus-visible { outline: 2px solid var(--accent); outline-offset: 1px; }

loadout-switchboard .mono { font-family: var(--mono); font-size: 12px; font-variant-numeric: tabular-nums; }
loadout-switchboard .dim { color: var(--dim); }
loadout-switchboard .faint { color: var(--faint); }
loadout-switchboard .em { color: var(--emerald); }
loadout-switchboard .am { color: var(--amber); }
loadout-switchboard .ro { color: var(--rose); }
loadout-switchboard .sk { color: var(--sky); }

loadout-switchboard kbd {
  font: 11px var(--mono);
  padding: 1px 5px;
  border: 1px solid var(--border-2);
  border-bottom-width: 2px;
  border-radius: 4px;
  color: var(--dim);
  background: var(--surface-2);
}

loadout-switchboard ::-webkit-scrollbar { width: 8px; height: 8px; }
loadout-switchboard ::-webkit-scrollbar-track { background: var(--bg); }
loadout-switchboard ::-webkit-scrollbar-thumb { background: var(--border-2); border-radius: 4px; }
loadout-switchboard ::-webkit-scrollbar-thumb:hover { background: var(--faint); }

/* Standalone header bar */
loadout-switchboard .sk-header {
  display: flex;
  align-items: center;
  gap: 12px;
  padding: 9px 18px;
  background: var(--surface);
  border-bottom: 1px solid var(--border);
  min-height: 48px;
  flex-shrink: 0;
  z-index: 35;
}

loadout-switchboard .brand {
  display: inline-flex;
  align-items: center;
  gap: 8px;
  user-select: none;
}

loadout-switchboard .brand b {
  font: 700 18px var(--mono);
  letter-spacing: -.02em;
}

loadout-switchboard .brand b i {
  color: var(--brand-o);
  font-style: normal;
}

loadout-switchboard .live-dot {
  width: 6px;
  height: 6px;
  border-radius: 50%;
  background: var(--emerald);
  box-shadow: 0 0 8px var(--emerald-dim);
  display: inline-block;
}

loadout-switchboard .live-dot.off {
  background: var(--faint);
  box-shadow: none;
}

loadout-switchboard .stage-body {
  flex: 1;
  min-height: 0;
  overflow: auto;
  position: relative;
  display: flex;
  flex-direction: column;
}

/* Generic controls */
loadout-switchboard .btn {
  display: inline-flex;
  align-items: center;
  gap: 6px;
  height: 28px;
  padding: 0 10px;
  border-radius: 6px;
  border: 1px solid var(--border-2);
  background: var(--surface-2);
  font-size: 12px;
  white-space: nowrap;
}

loadout-switchboard .btn:hover:not(:disabled) { border-color: var(--faint); }

loadout-switchboard .btn.primary {
  background: var(--accent);
  border-color: var(--accent);
  color: var(--on-accent);
  font-weight: 600;
}

loadout-switchboard .btn.primary:hover:not(:disabled) { filter: brightness(1.08); }
loadout-switchboard .btn.ghost { background: none; border-color: transparent; color: var(--dim); }
loadout-switchboard .btn.ghost:hover:not(:disabled) { color: var(--text); background: var(--surface-2); }
loadout-switchboard .btn.sm { height: 24px; padding: 0 8px; font-size: 11.5px; }
loadout-switchboard .btn.danger { color: var(--rose); }
loadout-switchboard .btn:disabled { opacity: .45; cursor: not-allowed; }

loadout-switchboard .badge {
  display: inline-flex;
  align-items: center;
  gap: 4px;
  height: 18px;
  padding: 0 6px;
  border-radius: 4px;
  font: 500 10.5px var(--mono);
  border: 1px solid var(--border-2);
  color: var(--dim);
  white-space: nowrap;
}

loadout-switchboard .badge.em { color: var(--emerald); border-color: transparent; background: var(--emerald-dim); }
loadout-switchboard .badge.am { color: var(--amber); border-color: transparent; background: var(--amber-dim); }
loadout-switchboard .badge.ro { color: var(--rose); border-color: transparent; background: var(--rose-dim); }
loadout-switchboard .badge.sk { color: var(--sky); border-color: transparent; background: var(--sky-dim); }
loadout-switchboard .badge.btnish { cursor: pointer; }
loadout-switchboard .badge.btnish:hover { filter: brightness(1.15); }
loadout-switchboard .badge.trunc {
  display: inline-block;
  max-width: 100%;
  min-width: 0;
  line-height: 16px;
  overflow: hidden;
  text-overflow: ellipsis;
}

loadout-switchboard .seg {
  display: inline-flex;
  border: 1px solid var(--border-2);
  border-radius: 7px;
  padding: 2px;
  background: var(--surface-2);
  gap: 2px;
}

loadout-switchboard .seg button {
  border: 0;
  background: none;
  height: 24px;
  padding: 0 9px;
  border-radius: 5px;
  font-size: 11.5px;
  color: var(--dim);
  white-space: nowrap;
  display: inline-flex;
  align-items: center;
  gap: 6px;
}

loadout-switchboard .seg button[aria-pressed="true"] {
  background: var(--surface-3);
  color: var(--text);
  box-shadow: 0 0 0 1px var(--border-2);
}

loadout-switchboard .field { display: grid; gap: 5px; }
loadout-switchboard .field label { font-size: 11.5px; color: var(--dim); font-weight: 500; }

loadout-switchboard .input {
  height: 32px;
  border-radius: 6px;
  border: 1px solid var(--border-2);
  background: var(--bg);
  padding: 0 10px;
  width: 100%;
}

loadout-switchboard .input.mono { font-family: var(--mono); font-size: 12px; }
loadout-switchboard .input:focus { outline: none; border-color: var(--accent); box-shadow: 0 0 0 3px var(--accent-dim); }
loadout-switchboard .hint { font-size: 11px; color: var(--faint); }
loadout-switchboard .card { border: 1px solid var(--border); background: var(--surface); border-radius: 10px; }

loadout-switchboard .section-label {
  font: 600 10.5px var(--mono);
  text-transform: uppercase;
  letter-spacing: .08em;
  color: var(--faint);
  margin: 0 0 8px;
}

/* Switchboard top bar */
loadout-switchboard .topbar {
  position: sticky;
  top: 0;
  z-index: 30;
  display: flex;
  flex-wrap: wrap;
  align-items: center;
  gap: 8px;
  padding: 10px 18px;
  background: var(--bg);
  border-bottom: 1px solid var(--border);
  flex-shrink: 0;
}

loadout-switchboard .search { position: relative; width: 250px; }
loadout-switchboard .search input { padding-left: 28px; padding-right: 30px; height: 28px; }
loadout-switchboard .search svg { position: absolute; left: 9px; top: 7px; color: var(--faint); pointer-events: none; }
loadout-switchboard .search kbd { position: absolute; right: 6px; top: 5px; pointer-events: none; }

loadout-switchboard .pills { display: flex; gap: 4px; flex-wrap: wrap; }
loadout-switchboard .pill {
  height: 26px;
  padding: 0 9px;
  border-radius: 13px;
  border: 1px solid var(--border-2);
  background: none;
  font-size: 11.5px;
  color: var(--dim);
  display: inline-flex;
  align-items: center;
  gap: 6px;
}

loadout-switchboard .pill b { font: 600 10.5px var(--mono); color: var(--faint); }
loadout-switchboard .pill[aria-pressed="true"] { background: var(--text); color: var(--bg); border-color: var(--text); }
loadout-switchboard .pill[aria-pressed="true"] b { color: var(--bg); opacity: .7; }
loadout-switchboard .pill .dot { width: 6px; height: 6px; border-radius: 50%; }
loadout-switchboard .spacer { flex: 1; }

loadout-switchboard .select {
  height: 28px;
  border-radius: 6px;
  border: 1px solid var(--border-2);
  background: var(--surface-2);
  padding: 0 8px;
  font-size: 12px;
}

loadout-switchboard .profile {
  display: inline-flex;
  align-items: center;
  gap: 6px;
  font-size: 11.5px;
  color: var(--faint);
}


loadout-switchboard .undo-pill {
  display: inline-flex;
  align-items: center;
  gap: 8px;
  height: 28px;
  padding: 0 4px 0 10px;
  border-radius: 14px;
  border: 1px dashed var(--border-2);
  font-size: 11.5px;
  color: var(--dim);
  max-width: 300px;
}

loadout-switchboard .undo-pill span {
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
}

/* Unconfigured sibling strip */
loadout-switchboard .sibling-bar {
  display: flex;
  align-items: center;
  gap: 8px;
  flex-wrap: wrap;
  padding: 7px 18px;
  background: var(--surface);
  border-bottom: 1px solid var(--border);
  font-size: 12px;
}

loadout-switchboard .sibling-chip {
  display: inline-flex;
  align-items: center;
  gap: 7px;
  padding: 3px 5px 3px 9px;
  border-radius: 6px;
  background: var(--surface-2);
  border: 1px solid var(--border-2);
}

/* Matrix */
loadout-switchboard .matrix-wrap {
  padding: 0 18px 80px;
  flex: 1 0 auto;
}

loadout-switchboard table.matrix {
  border-collapse: separate;
  border-spacing: 0;
  min-width: 100%;
}

loadout-switchboard .matrix th,
loadout-switchboard .matrix td {
  border-bottom: 1px solid var(--border);
  padding: 0;
}

loadout-switchboard .matrix thead th {
  position: sticky;
  background: var(--bg);
  z-index: 10;
  vertical-align: bottom;
}

loadout-switchboard .matrix thead tr.groups th {
  top: var(--topbar-h, 49px);
  height: 26px;
  font: 600 10px var(--mono);
  text-transform: uppercase;
  letter-spacing: .08em;
  color: var(--faint);
  text-align: left;
  padding: 0 10px;
  border-bottom: 0;
}

loadout-switchboard .matrix thead tr.groups th.g span {
  display: block;
  border-top: 2px solid var(--border-2);
  padding-top: 4px;
}

loadout-switchboard .matrix thead tr.groups th.g.claude span,
loadout-switchboard .matrix thead tr.groups th.g.multi span {
  border-top-color: var(--accent);
  color: var(--dim);
}

loadout-switchboard .matrix thead tr.cols th {
  top: calc(var(--topbar-h, 49px) + 26px);
}

loadout-switchboard .matrix .skill-col {
  position: sticky;
  left: 0;
  z-index: 5;
  background: var(--bg);
  min-width: 340px;
  max-width: 340px;
  border-right: 1px solid var(--border);
}

loadout-switchboard .matrix thead .skill-col { z-index: 12; }

loadout-switchboard .colhead {
  width: 132px;
  min-width: 132px;
  padding: 8px 10px 10px !important;
  text-align: left;
  border-left: 1px solid var(--border);
  position: relative;
}

loadout-switchboard .colhead.cursor { background: var(--surface) !important; }

loadout-switchboard .colhead .name {
  display: flex;
  align-items: center;
  gap: 6px;
  font-weight: 600;
  font-size: 12px;
  white-space: nowrap;
  padding-right: 18px;
}

loadout-switchboard .colhead .glyph {
  width: 18px;
  height: 18px;
  border-radius: 4px;
  display: grid;
  place-items: center;
  font: 700 9px var(--mono);
  background: var(--surface-3);
  border: 1px solid var(--border-2);
  color: var(--dim);
  flex: none;
}

loadout-switchboard .colhead .path {
  font: 10.5px var(--mono);
  color: var(--faint);
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
  display: block;
  margin: 2px 0 6px;
  max-width: 112px;
}

loadout-switchboard .colhead .menu-btn {
  position: absolute;
  right: 4px;
  top: 6px;
  width: 20px;
  height: 20px;
  border: 0;
  background: none;
  color: var(--faint);
  border-radius: 4px;
}

loadout-switchboard .colhead .menu-btn:hover { background: var(--surface-3); color: var(--text); }

loadout-switchboard .meter {
  height: 4px;
  border-radius: 2px;
  background: var(--surface-3);
  overflow: hidden;
}

loadout-switchboard .meter i {
  display: block;
  height: 100%;
  background: var(--emerald);
  border-radius: 2px;
  transition: width .15s ease;
}

loadout-switchboard .meter.hot i { background: var(--amber); }

loadout-switchboard .budget {
  font: 10.5px var(--mono);
  color: var(--dim);
  margin-top: 4px;
  white-space: nowrap;
}

loadout-switchboard .budget.hot { color: var(--amber); }

loadout-switchboard .addcol {
  min-width: 120px;
  padding: 8px 10px 10px !important;
  border-left: 1px solid var(--border);
  text-align: left;
}

loadout-switchboard .addcol button {
  width: 100%;
  height: 58px;
  border: 1px dashed var(--border-2);
  border-radius: 8px;
  background: none;
  color: var(--dim);
  font-size: 12px;
}

loadout-switchboard .addcol button:hover { border-color: var(--accent); color: var(--accent); }

loadout-switchboard .skill-cell { padding: 8px 12px !important; cursor: pointer; }
loadout-switchboard .skill-cell .row1 { display: flex; align-items: center; gap: 6px; min-width: 0; }
loadout-switchboard .skill-cell .slug { font: 600 12.5px var(--mono); white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
loadout-switchboard .skill-cell .desc {
  color: var(--faint);
  font-size: 11.5px;
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
  margin: 1px 0 4px;
}
loadout-switchboard .skill-cell .badges { display: flex; gap: 4px; flex-wrap: wrap; min-width: 0; }

loadout-switchboard .matrix tbody tr:hover td { background: var(--surface); }
loadout-switchboard .matrix tbody tr.cursor-row td { background: var(--surface); }
loadout-switchboard .matrix tbody tr.selected td.skill-col { box-shadow: inset 2px 0 0 var(--accent); }
loadout-switchboard .matrix td.cell { text-align: center; border-left: 1px solid var(--border); height: 58px; }
loadout-switchboard .matrix td.cell.cursor-col { background: var(--surface); }
loadout-switchboard .empty-row td { padding: 40px !important; text-align: center; color: var(--faint); }

/* Patchbay Jack (intersection cell) */
loadout-switchboard .jack {
  width: 100%;
  height: 100%;
  min-height: 56px;
  border: 0;
  background: none;
  display: grid;
  place-items: center;
  position: relative;
}

loadout-switchboard .jack:disabled { opacity: .5; cursor: wait; }

loadout-switchboard .jack .j {
  width: 14px;
  height: 14px;
  border-radius: 50%;
  border: 1.5px solid var(--border-2);
  display: grid;
  place-items: center;
  font: 700 9px var(--mono);
  transition: transform .08s ease;
}

loadout-switchboard .jack:hover .j { border-color: var(--dim); transform: scale(1.1); }

loadout-switchboard .jack .lbl {
  position: absolute;
  bottom: 5px;
  left: 0;
  right: 0;
  font: 9.5px var(--mono);
  color: var(--faint);
  opacity: 0;
  transition: opacity .1s;
}

loadout-switchboard .jack:hover .lbl,
loadout-switchboard .jack.cur .lbl { opacity: 1; }

loadout-switchboard .jack.cur {
  box-shadow: inset 0 0 0 2px var(--accent);
  border-radius: 4px;
}

loadout-switchboard .jack[data-s="on"] .j {
  background: var(--emerald);
  border-color: var(--emerald);
  box-shadow: 0 0 0 4px var(--emerald-dim), 0 0 12px var(--emerald-dim);
}

loadout-switchboard .jack[data-s="on"]::before {
  content: "";
  position: absolute;
  left: 0;
  right: 50%;
  top: 50%;
  height: 1px;
  background: linear-gradient(90deg, transparent, var(--emerald));
  opacity: .35;
}

loadout-switchboard .jack[data-s="copy"] .j {
  border-radius: 3px;
  border-color: var(--amber);
  background: var(--amber-dim);
  width: 13px;
  height: 13px;
}

loadout-switchboard .jack[data-s="drift"] .j {
  width: 22px;
  border-radius: 7px;
  border-color: var(--amber);
  background: var(--amber-dim);
  color: var(--amber);
}

loadout-switchboard .jack[data-s="fm"] .j {
  border: 1.5px dashed var(--rose);
  color: var(--rose);
  background: var(--rose-dim);
}

loadout-switchboard .jack[data-s="broken"] .j {
  border: 1.5px dashed var(--rose);
  color: var(--rose);
}

loadout-switchboard .jack[data-s="shadow"] .j {
  border-color: var(--sky);
  color: var(--sky);
  background: var(--sky-dim);
}

/* Sticky Legend Bar */
loadout-switchboard .legend {
  position: sticky;
  bottom: 0;
  display: flex;
  flex-wrap: wrap;
  gap: 16px;
  padding: 8px 18px;
  border-top: 1px solid var(--border);
  background: var(--bg);
  font-size: 11px;
  color: var(--dim);
  z-index: 20;
  margin-top: auto;
}

loadout-switchboard .legend .jack {
  width: 22px;
  min-height: 18px;
  height: 18px;
  display: inline-grid;
  pointer-events: none;
}

loadout-switchboard .legend .jack::before { display: none; }
loadout-switchboard .legend span { display: inline-flex; align-items: center; gap: 4px; }
loadout-switchboard .legend .keys { margin-left: auto; color: var(--faint); }

/* Dropdown menu */
loadout-switchboard .menu {
  position: fixed;
  z-index: 80;
  min-width: 230px;
  background: var(--surface);
  border-radius: 8px;
  box-shadow: var(--shadow);
  padding: 4px;
}

loadout-switchboard .menu button {
  display: flex;
  width: 100%;
  align-items: center;
  gap: 8px;
  text-align: left;
  border: 0;
  background: none;
  height: 30px;
  padding: 0 10px;
  border-radius: 5px;
  font-size: 12px;
}

loadout-switchboard .menu button:hover:not(:disabled) { background: var(--surface-3); }
loadout-switchboard .menu button:disabled { opacity: .45; cursor: not-allowed; }
loadout-switchboard .menu hr { border: 0; border-top: 1px solid var(--border); margin: 4px 0; }
loadout-switchboard .menu .sub { padding-left: 24px; color: var(--dim); }

/* Overlays */
loadout-switchboard .scrim {
  position: fixed;
  inset: 0;
  background: rgba(5, 6, 8, .55);
  z-index: 60;
  animation: sk-fade .12s ease;
}

:root[data-theme="light"] loadout-switchboard .scrim,
loadout-switchboard[theme="light"] .scrim {
  background: rgba(20, 22, 26, .25);
}

loadout-switchboard .drawer {
  position: fixed;
  top: 0;
  right: 0;
  bottom: 0;
  width: min(720px, 100vw);
  background: var(--surface);
  border-left: 1px solid var(--border-2);
  z-index: 70;
  display: flex;
  flex-direction: column;
  animation: sk-slide .16s cubic-bezier(.2, .8, .2, 1);
  box-shadow: var(--shadow);
}

loadout-switchboard .drawer.wide { width: min(1080px, 100vw); }

loadout-switchboard .modal {
  position: fixed;
  left: 50%;
  top: 50%;
  transform: translate(-50%, -50%);
  width: min(760px, calc(100vw - 32px));
  max-height: calc(100vh - 48px);
  background: var(--surface);
  border-radius: 12px;
  z-index: 70;
  display: flex;
  flex-direction: column;
  box-shadow: var(--shadow);
  animation: sk-pop .14s ease;
}

@keyframes sk-fade { from { opacity: 0; } }
@keyframes sk-slide { from { transform: translateX(40px); opacity: 0; } }
@keyframes sk-pop { from { transform: translate(-50%, -48%); opacity: 0; } }

loadout-switchboard .ov-head {
  display: flex;
  align-items: center;
  gap: 10px;
  padding: 14px 18px;
  border-bottom: 1px solid var(--border);
}

loadout-switchboard .ov-head h2 { margin: 0; font-size: 14px; font-weight: 600; }
loadout-switchboard .ov-head .x { margin-left: auto; }
loadout-switchboard .ov-body { flex: 1; overflow: auto; padding: 16px 18px; min-height: 0; }
loadout-switchboard .ov-foot {
  border-top: 1px solid var(--border);
  padding: 12px 18px;
  display: flex;
  align-items: center;
  gap: 8px;
  flex-wrap: wrap;
}

/* Diff Resolver */
loadout-switchboard .instances {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(220px, 1fr));
  gap: 8px;
  margin-bottom: 14px;
}

loadout-switchboard .inst {
  padding: 10px 12px;
  border-radius: 8px;
  border: 1px solid var(--border);
  background: var(--surface-2);
  position: relative;
}

loadout-switchboard .inst .tag {
  font: 700 10px var(--mono);
  padding: 1px 6px;
  border-radius: 4px;
  background: var(--surface-3);
  color: var(--dim);
}

loadout-switchboard .inst.a { border-color: color-mix(in srgb, var(--emerald) 40%, var(--border)); }
loadout-switchboard .inst.b { border-color: color-mix(in srgb, var(--amber) 40%, var(--border)); }
loadout-switchboard .inst .p { font: 11.5px var(--mono); margin: 6px 0 2px; word-break: break-all; }

loadout-switchboard .diff-tabs { display: flex; align-items: center; gap: 8px; margin-bottom: 8px; flex-wrap: wrap; }

loadout-switchboard .diff {
  border: 1px solid var(--border);
  border-radius: 8px;
  overflow: auto;
  max-height: 360px;
  font: 11.5px/1.65 var(--mono);
}

loadout-switchboard .diff-head {
  display: grid;
  grid-template-columns: 1fr 1fr;
  background: var(--surface-2);
  border-bottom: 1px solid var(--border);
  position: sticky;
  top: 0;
}

loadout-switchboard .diff-head div { padding: 6px 10px; color: var(--dim); font-size: 11px; }
loadout-switchboard .diff-head div + div { border-left: 1px solid var(--border); }

loadout-switchboard .diff-row { display: grid; grid-template-columns: 1fr 1fr; }
loadout-switchboard .diff-row > div {
  padding: 0 10px 0 0;
  white-space: pre-wrap;
  word-break: break-word;
  display: grid;
  grid-template-columns: 34px 1fr;
  min-height: 19px;
}

loadout-switchboard .diff-row > div + div { border-left: 1px solid var(--border); }
loadout-switchboard .diff-row .ln { color: var(--faint); text-align: right; padding-right: 10px; user-select: none; }
loadout-switchboard .diff-row .add { background: var(--emerald-dim); }
loadout-switchboard .diff-row .del { background: var(--rose-dim); }
loadout-switchboard .diff-row .hunk { background: var(--sky-dim); color: var(--sky); grid-column: 1 / -1; padding: 2px 10px; display: block; }
loadout-switchboard .diff-row .blank { background: repeating-linear-gradient(135deg, transparent 0 6px, var(--surface-2) 6px 7px); }

loadout-switchboard .diff.unified .diff-row { grid-template-columns: 1fr; }
loadout-switchboard .u-line { display: grid; grid-template-columns: 34px 34px 16px 1fr; white-space: pre-wrap; min-height: 19px; }
loadout-switchboard .u-line.add { background: var(--emerald-dim); }
loadout-switchboard .u-line.del { background: var(--rose-dim); }
loadout-switchboard .u-line.hunk { background: var(--sky-dim); color: var(--sky); }
loadout-switchboard .u-line .ln { color: var(--faint); text-align: right; padding-right: 8px; }

loadout-switchboard .tree { font: 12px/1.9 var(--mono); }
loadout-switchboard .tree-row { display: grid; grid-template-columns: 1fr 110px 110px; padding: 0 10px; border-bottom: 1px solid var(--border); }
loadout-switchboard .tree-row.h { color: var(--faint); font-size: 10.5px; text-transform: uppercase; letter-spacing: .06em; background: var(--surface-2); }

loadout-switchboard .choices { display: grid; grid-template-columns: repeat(3, 1fr); gap: 8px; }
loadout-switchboard .choice {
  text-align: left;
  padding: 10px 12px;
  border-radius: 8px;
  border: 1px solid var(--border-2);
  background: var(--surface-2);
  display: grid;
  gap: 3px;
}

loadout-switchboard .choice[aria-pressed="true"] {
  border-color: var(--accent);
  box-shadow: 0 0 0 3px var(--accent-dim);
}

loadout-switchboard .choice b { font-size: 12.5px; }
loadout-switchboard .choice span { font-size: 11px; color: var(--faint); }

loadout-switchboard .ops {
  margin: 10px 0 0;
  padding: 10px 12px;
  border-radius: 8px;
  background: var(--bg);
  border: 1px solid var(--border);
  font: 11.5px/1.8 var(--mono);
}

loadout-switchboard .ops .op { display: grid; grid-template-columns: 96px 1fr; gap: 8px; }
loadout-switchboard .op-k { font-weight: 700; font-size: 10.5px; }

/* Inspector */
loadout-switchboard .compat { display: grid; grid-template-columns: repeat(auto-fill, minmax(150px, 1fr)); gap: 6px; }
loadout-switchboard .compat div {
  display: flex;
  align-items: center;
  gap: 6px;
  padding: 7px 9px;
  border-radius: 6px;
  background: var(--surface-2);
  border: 1px solid var(--border);
  font-size: 11.5px;
}

loadout-switchboard .warnbox {
  border: 1px solid color-mix(in srgb, var(--rose) 45%, var(--border));
  background: var(--rose-dim);
  border-radius: 8px;
  padding: 12px;
  margin: 12px 0;
}

loadout-switchboard .warnbox.ok {
  border-color: color-mix(in srgb, var(--emerald) 45%, var(--border));
  background: var(--emerald-dim);
}

loadout-switchboard .warnbox h4 { margin: 0 0 4px; font-size: 12.5px; }
loadout-switchboard .warnbox p { margin: 0 0 8px; font-size: 12px; color: var(--dim); }

loadout-switchboard .linkmap {
  font: 11.5px/1.5 var(--mono);
  border: 1px solid var(--border);
  border-radius: 8px;
  overflow: hidden;
}

loadout-switchboard .linkmap div {
  display: grid;
  grid-template-columns: 20px 1fr auto;
  gap: 6px;
  padding: 6px 10px;
  border-bottom: 1px solid var(--border);
  align-items: center;
}

loadout-switchboard .linkmap div:last-child { border-bottom: 0; }
loadout-switchboard .linkmap .arrow { color: var(--faint); }

loadout-switchboard .files {
  display: grid;
  grid-template-columns: 190px 1fr;
  border: 1px solid var(--border);
  border-radius: 8px;
  overflow: hidden;
  min-height: 260px;
}

loadout-switchboard .files nav {
  border-right: 1px solid var(--border);
  background: var(--surface-2);
  padding: 6px;
  font: 11.5px var(--mono);
}

loadout-switchboard .files nav button {
  display: block;
  width: 100%;
  text-align: left;
  border: 0;
  background: none;
  padding: 4px 8px;
  border-radius: 4px;
  color: var(--dim);
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
}

loadout-switchboard .files nav button[aria-current="true"] {
  background: var(--surface-3);
  color: var(--text);
}

loadout-switchboard .files nav .dir { padding: 6px 8px 2px; color: var(--faint); }

loadout-switchboard .files pre,
loadout-switchboard .files textarea {
  margin: 0;
  padding: 10px 12px;
  font: 11.5px/1.65 var(--mono);
  white-space: pre-wrap;
  word-break: break-word;
  background: var(--bg);
  border: 0;
  width: 100%;
  height: 100%;
  min-height: 260px;
  resize: vertical;
}

loadout-switchboard .files .viewer { display: flex; flex-direction: column; min-width: 0; }
loadout-switchboard .files .vbar {
  display: flex;
  align-items: center;
  gap: 6px;
  padding: 6px 8px;
  border-bottom: 1px solid var(--border);
  font: 11px var(--mono);
  color: var(--faint);
}

loadout-switchboard .kv { display: grid; grid-template-columns: 120px 1fr; gap: 4px 12px; font-size: 12px; }
loadout-switchboard .kv dt { color: var(--faint); }
loadout-switchboard .kv dd { margin: 0; font-family: var(--mono); font-size: 11.5px; word-break: break-all; }
loadout-switchboard .insp-sec { margin-bottom: 18px; }

/* Scan & Consolidate Wizard */
loadout-switchboard .wiz {
  max-width: 1120px;
  width: 100%;
  margin: 0 auto;
  padding: 22px 18px 60px;
}

loadout-switchboard .steps { display: flex; gap: 6px; margin-bottom: 18px; flex-wrap: wrap; }
loadout-switchboard .step {
  display: flex;
  align-items: center;
  gap: 8px;
  padding: 6px 12px 6px 6px;
  border-radius: 16px;
  border: 1px solid var(--border);
  color: var(--faint);
  font-size: 12px;
  background: none;
}

loadout-switchboard .step i {
  width: 20px;
  height: 20px;
  border-radius: 50%;
  display: grid;
  place-items: center;
  font: 600 10.5px var(--mono);
  font-style: normal;
  background: var(--surface-3);
}

loadout-switchboard .step[aria-current="true"] { color: var(--text); border-color: var(--border-2); background: var(--surface); }
loadout-switchboard .step[aria-current="true"] i { background: var(--accent); color: var(--on-accent); }
loadout-switchboard .step.done i { background: var(--emerald-dim); color: var(--emerald); }

loadout-switchboard .scanline {
  display: flex;
  align-items: baseline;
  gap: 14px;
  flex-wrap: wrap;
  padding: 18px 20px;
  margin-bottom: 14px;
}

loadout-switchboard .scanline .big { font: 700 34px/1 var(--mono); letter-spacing: -.03em; }
loadout-switchboard .scanline .big small { font-size: 14px; color: var(--dim); font-weight: 500; }

loadout-switchboard .stats {
  display: grid;
  grid-template-columns: repeat(5, 1fr);
  gap: 8px;
  margin-bottom: 14px;
}

loadout-switchboard .stat { padding: 12px 14px; }
loadout-switchboard .stat .v { font: 700 22px var(--mono); }
loadout-switchboard .stat .l { font-size: 11.5px; color: var(--dim); }
loadout-switchboard .stat .s { font: 10.5px var(--mono); color: var(--faint); margin-top: 4px; }

loadout-switchboard .roots { display: grid; grid-template-columns: 1.3fr 1fr; gap: 14px; }
loadout-switchboard .rootlist { font: 12px var(--mono); }
loadout-switchboard .rootlist div {
  display: grid;
  grid-template-columns: 1fr auto;
  gap: 8px;
  padding: 7px 14px;
  border-bottom: 1px solid var(--border);
  align-items: center;
}

loadout-switchboard .rootlist div:last-child { border-bottom: 0; }

loadout-switchboard .plan-table { width: 100%; border-collapse: collapse; font-size: 12px; }
loadout-switchboard .plan-table th {
  text-align: left;
  font: 600 10.5px var(--mono);
  text-transform: uppercase;
  letter-spacing: .06em;
  color: var(--faint);
  padding: 8px 10px;
  border-bottom: 1px solid var(--border);
  background: var(--surface-2);
  position: sticky;
  top: 0;
}

loadout-switchboard .plan-table td { padding: 7px 10px; border-bottom: 1px solid var(--border); vertical-align: middle; }
loadout-switchboard .plan-table td.p { font: 11.5px var(--mono); }
loadout-switchboard .plan-table tr.off td { opacity: .45; }

loadout-switchboard .opk { font: 700 10px var(--mono); padding: 2px 6px; border-radius: 4px; }
loadout-switchboard .opk.MOVE { background: var(--sky-dim); color: var(--sky); }
loadout-switchboard .opk.LINK { background: var(--emerald-dim); color: var(--emerald); }
loadout-switchboard .opk.QUARANTINE { background: var(--amber-dim); color: var(--amber); }
loadout-switchboard .opk.REPAIR { background: var(--rose-dim); color: var(--rose); }
loadout-switchboard .opk.REVIEW,
loadout-switchboard .opk.KEEP { background: var(--surface-3); color: var(--dim); }

loadout-switchboard .log {
  font: 12px/1.75 var(--mono);
  padding: 14px 16px;
  background: var(--bg);
  border-radius: 8px;
  border: 1px solid var(--border);
  min-height: 220px;
}

loadout-switchboard .safety {
  display: flex;
  gap: 16px;
  flex-wrap: wrap;
  font-size: 11.5px;
  color: var(--dim);
  padding: 10px 14px;
}

loadout-switchboard .safety span::before { content: "\2713 "; color: var(--emerald); }

/* Embedded T3 Code Drawer (mode="embedded", Artboard 6) */
loadout-switchboard .h-drawer {
  background: var(--surface);
  display: flex;
  flex-direction: column;
  height: 100%;
  min-height: 0;
}

loadout-switchboard .h-drawer header {
  padding: 10px 12px;
  border-bottom: 1px solid var(--border);
  display: grid;
  gap: 8px;
  flex-shrink: 0;
}

loadout-switchboard .h-drawer .tagname { font: 11px var(--mono); color: var(--faint); }

loadout-switchboard .mini { overflow: auto; flex: 1; }
loadout-switchboard .mini table { border-collapse: collapse; width: 100%; }
loadout-switchboard .mini th {
  font: 700 9.5px var(--mono);
  color: var(--faint);
  padding: 6px 0;
  position: sticky;
  top: 0;
  background: var(--surface);
  border-bottom: 1px solid var(--border);
  z-index: 4;
}

loadout-switchboard .mini th.active { color: var(--accent); }
loadout-switchboard .mini td { border-bottom: 1px solid var(--border); padding: 0; }
loadout-switchboard .mini td.s {
  font: 11.5px var(--mono);
  padding: 6px 10px;
  white-space: nowrap;
  max-width: 155px;
  overflow: hidden;
  text-overflow: ellipsis;
  cursor: pointer;
}

loadout-switchboard .mini td.s:hover { color: var(--accent); }
loadout-switchboard .mini .jack { min-height: 30px; height: 30px; }
loadout-switchboard .mini .jack .j { width: 11px; height: 11px; font-size: 8px; }
loadout-switchboard .mini .jack[data-s="drift"] .j { width: 17px; }
loadout-switchboard .mini .jack .lbl { display: none; }
loadout-switchboard .mini td.activecol { background: var(--accent-dim); }

loadout-switchboard .wt { padding: 12px; display: grid; gap: 8px; overflow: auto; flex: 1; align-content: start; }
loadout-switchboard .wt-item {
  display: grid;
  grid-template-columns: 1fr auto;
  gap: 6px;
  padding: 8px 10px;
  border-radius: 7px;
  border: 1px solid var(--border);
  background: var(--surface-2);
  align-items: center;
}

loadout-switchboard .h-foot {
  padding: 8px 12px;
  border-top: 1px solid var(--border);
  display: flex;
  gap: 6px;
  align-items: center;
  flex-shrink: 0;
}

/* Toast & Keys Popover */
loadout-switchboard .toasts {
  position: fixed;
  bottom: 46px;
  left: 50%;
  transform: translateX(-50%);
  z-index: 90;
  display: grid;
  gap: 6px;
  justify-items: center;
  pointer-events: none;
}

loadout-switchboard .toast {
  pointer-events: auto;
  display: flex;
  align-items: center;
  gap: 10px;
  padding: 8px 8px 8px 14px;
  border-radius: 9px;
  background: var(--text);
  color: var(--bg);
  font-size: 12px;
  box-shadow: var(--shadow);
  animation: sk-pop2 .16s ease;
  max-width: min(720px, calc(100vw - 32px));
}

loadout-switchboard .toast .mono { font-size: 11px; opacity: .75; }
loadout-switchboard .toast button {
  border: 0;
  background: rgba(127, 127, 127, .22);
  color: inherit;
  height: 24px;
  padding: 0 9px;
  border-radius: 5px;
  font-weight: 600;
  font-size: 11.5px;
}

@keyframes sk-pop2 { from { transform: translateY(8px); opacity: 0; } }

loadout-switchboard .help {
  position: fixed;
  right: 18px;
  bottom: 50px;
  z-index: 85;
  width: 300px;
  padding: 14px;
  box-shadow: var(--shadow);
}

loadout-switchboard .help div {
  display: flex;
  justify-content: space-between;
  padding: 3px 0;
  font-size: 12px;
  color: var(--dim);
}

@media (max-width: 900px) {
  loadout-switchboard .stats { grid-template-columns: repeat(2, 1fr); }
  loadout-switchboard .roots,
  loadout-switchboard .instances,
  loadout-switchboard .choices { grid-template-columns: 1fr; }
  loadout-switchboard .search { width: 100%; }
  loadout-switchboard .matrix .skill-col { min-width: 210px; max-width: 210px; }
  loadout-switchboard .files { grid-template-columns: 1fr; }
}
)LOADOUT_CSS";

inline constexpr std::string_view SWITCHBOARD_JS = R"LOADOUT_JS((() => {
  'use strict';

  const ADAPTERS = [
    { id: 'claude-code', label: 'Claude Code', defaultDir: '~/.claude-custom/skills', defaultEnv: 'CLAUDE_CONFIG_DIR=~/.claude-custom', strict: false },
    { id: 'antigravity', label: 'Antigravity (Strict)', defaultDir: '~/.gemini/config/skills', defaultEnv: '', strict: true },
    { id: 'codex', label: 'Codex', defaultDir: '~/.codex-work/skills', defaultEnv: 'CODEX_HOME=~/.codex-work', strict: false },
    { id: 'opencode', label: 'OpenCode', defaultDir: '~/.config/opencode/skills', defaultEnv: '', strict: false },
    { id: 'zcode', label: 'ZCode', defaultDir: '~/.zcode/skills', defaultEnv: '', strict: false },
    { id: 'cursor', label: 'Cursor', defaultDir: '~/.cursor/skills', defaultEnv: '', strict: false },
    { id: 'windsurf', label: 'Windsurf', defaultDir: '~/.codeium/windsurf/skills', defaultEnv: '', strict: false },
    { id: 'custom-lenient', label: 'Custom (Lenient)', defaultDir: '~/tools/my-agent/skills', defaultEnv: '', strict: false },
    { id: 'custom-strict', label: 'Custom (Strict)', defaultDir: '~/tools/my-strict-agent/skills', defaultEnv: '', strict: true },
  ];

  const STRICT_ADAPTERS = new Set(['antigravity', 'custom-strict']);
  const BUDGET_HOT = 3000;
  const BUDGET_SCALE = 4500;
  const LOADED_STATES = new Set(['on', 'copy', 'drift', 'shadow']);

  const STATE_LABEL = {
    on: 'wired',
    off: 'unwired',
    copy: 'physical copy',
    drift: 'drifted copy',
    fm: 'frontmatter \u2717',
    shadow: 'plugin shadow',
    broken: 'broken link',
  };
  const STATE_GLYPH = { on: '', off: '', copy: '', drift: '\u21c4', fm: '!', shadow: 'P', broken: '\u00d7' };

  const ICON_SEARCH = '<svg width="13" height="13" viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.6" aria-hidden="true"><circle cx="7" cy="7" r="5"/><path d="M11 11l3.5 3.5"/></svg>';

  function esc(s) {
    return String(s ?? '').replace(/[&<>"']/g, (ch) => ({
      '&': '&amp;',
      '<': '&lt;',
      '>': '&gt;',
      '"': '&quot;',
      "'": '&#39;',
    }[ch]));
  }

  function fmtScanMs(ms) {
    return Math.round(Number(ms) || 0).toLocaleString();
  }

  function fmtTok(n) {
    const v = Number(n) || 0;
    return v >= 1000 ? `${(v / 1000).toFixed(1)}k` : String(Math.round(v));
  }

  function fmtBytes(n) {
    const v = Number(n) || 0;
    if (v >= 1024 * 1024) return `${(v / (1024 * 1024)).toFixed(1)} MB`;
    if (v >= 1024) return `${Math.round(v / 1024)} KB`;
    return `${v} B`;
  }

  function fmtRelTime(epochSec) {
    if (!epochSec) return '';
    const ms = epochSec > 1e12 ? epochSec : epochSec * 1000;
    const diff = Math.max(0, Math.floor((Date.now() - ms) / 1000));
    if (diff < 60) return `${diff}s ago`;
    if (diff < 3600) return `${Math.floor(diff / 60)}m ago`;
    if (diff < 86400) return `${Math.floor(diff / 3600)}h ago`;
    return `${Math.floor(diff / 86400)}d ago`;
  }

  function shortenPath(p) {
    if (!p) return '';
    return String(p).replace(/^\/Users\/[^/]+/, '~').replace(/^\/home\/[^/]+/, '~');
  }

  function slugify(str) {
    return String(str || '')
      .toLowerCase()
      .replace(/[^a-z0-9]+/g, '-')
      .replace(/^-+|-+$/g, '')
      .slice(0, 48);
  }

  function targetGlyph(id, displayName, adapter) {
    const a = String(adapter || '').toLowerCase();
    const low = String(displayName || id || '').toLowerCase();
    if (a === 'antigravity') return 'AG';
    if (a === 'codex') return 'CX';
    if (a === 'opencode') return 'OC';
    if (a === 'zcode') return 'ZC';
    if (a === 'cursor') return 'CR';
    if (a === 'windsurf') return 'WS';
    if (a === 'claude-code') {
      if (low.includes('second') || low.includes('work')) return 'CW';
      if (low.includes('main') || low.includes('personal')) return 'CP';
      const words = String(displayName || '').replace(/[^A-Za-z0-9 ]/g, ' ').split(/\s+/).filter(Boolean);
      if (words.length >= 2) return (words[0][0] + words[1][0]).toUpperCase();
      return 'CL';
    }
    const words = String(displayName || id || 'NT').replace(/[^A-Za-z0-9 ]/g, ' ').split(/\s+/).filter(Boolean);
    if (words.length >= 2) return (words[0][0] + words[1][0]).toUpperCase();
    return (words[0] || 'NT').slice(0, 2).toUpperCase();
  }

  function inferGroup(adapter, displayName) {
    const a = String(adapter || '').toLowerCase();
    if (a.includes('claude') || String(displayName || '').toLowerCase().includes('claude')) return 'Claude';
    if (a.includes('antigravity') || a.includes('gemini')) return 'Gemini';
    if (a.includes('codex')) return 'OpenAI';
    if (a.includes('opencode')) return 'OpenCode';
    if (a.includes('zcode')) return 'Z.ai';
    if (a.includes('cursor')) return 'Cursor';
    if (a.includes('windsurf')) return 'Windsurf';
    return 'Custom';
  }

  function normEntryKind(raw) {
    const s = String(raw || '').toLowerCase().replace(/[_-\s]/g, '');
    if (s === 'canonical') return 'canonical';
    if (s === 'managedsymlink' || s === 'wired' || s === 'symlink' || s === 'externalsymlink' || s === 'on') return 'on';
    if (s === 'physicalcopy' || s === 'physical' || s === 'copy') return 'copy';
    if (s === 'drifted' || s === 'drift') return 'drift';
    if (s === 'danglingsymlink' || s === 'broken' || s === 'dangling') return 'broken';
    if (s === 'pluginshadow' || s === 'shadow' || s === 'plugin') return 'shadow';
    if (s === 'incompatible' || s === 'fm') return 'fm';
    return 'off';
  }

  function normalizeTarget(t) {
    const adapter = String(t.adapter || t.adapterKind || t.kind || 'custom-lenient').toLowerCase().replace(/_/g, '-');
    const displayName = t.displayName || t.display_name || t.name || t.id || 'Target';
    const id = String(t.id || slugify(displayName));
    const strict = STRICT_ADAPTERS.has(adapter) || Boolean(t.strict);
    return {
      id,
      displayName,
      group: t.group || t.vendor || inferGroup(adapter, displayName),
      glyph: t.glyph || targetGlyph(id, displayName, adapter),
      adapter,
      strict,
      skillsDir: shortenPath(t.skillsDir || t.skills_dir || t.path || ''),
      rawSkillsDir: t.skillsDir || t.skills_dir || t.path || '',
      configRoot: shortenPath(t.configRoot || t.config_root || ''),
      envHint: t.envHint || t.env_hint || t.env || '',
      autoDiscovered: Boolean(t.autoDiscovered ?? t.auto_discovered),
      configured: t.configured !== false,
      activeCount: Number(t.activeCount ?? t.active_count ?? 0),
      approxTokens: Number(t.approxTokens ?? t.approx_tokens ?? 0),
    };
  }

  function normalizeOccurrence(o) {
    return {
      slug: o.slug || '',
      path: shortenPath(o.path || ''),
      rawPath: o.path || '',
      realPath: shortenPath(o.realPath || o.real_path || ''),
      targetId: o.targetId || o.target_id || o.targetIdOrRepo || o.target_id_or_repo || '',
      entryKind: normEntryKind(o.entryKind || o.entry_kind || o.kind),
      isRepoLocal: Boolean(o.isRepoLocal ?? o.is_repo_local),
      repoRoot: shortenPath(o.repoRoot || o.repo_root || ''),
      rawRepoRoot: o.repoRoot || o.repo_root || '',
      treeHash: o.treeHash || o.tree_hash || '',
      fileCount: Number(o.fileCount ?? o.file_count ?? 0),
      totalBytes: Number(o.totalBytes ?? o.total_bytes ?? 0),
      mtimeEpoch: Number(o.mtimeEpoch ?? o.mtime_epoch ?? 0),
    };
  }

  function normalizeCatalog(raw) {
    const data = raw && typeof raw === 'object' ? (raw.catalog || raw) : {};
    const rawTargets = Array.isArray(data.targets)
      ? data.targets
      : (data.targets && typeof data.targets === 'object'
        ? Object.entries(data.targets).map(([id, v]) => ({ id, ...v }))
        : []);

    const allTargets = rawTargets.map(normalizeTarget);
    const targets = allTargets.filter((t) => t.configured);
    const extraSiblings = allTargets.filter((t) => !t.configured);

    const rawSiblings = Array.isArray(data.unconfiguredSiblings || data.unconfigured_siblings)
      ? (data.unconfiguredSiblings || data.unconfigured_siblings).map(normalizeTarget)
      : [];

    const targetIds = new Set(targets.map((t) => t.id));
    const siblingMap = new Map();
    for (const s of [...rawSiblings, ...extraSiblings]) {
      if (!targetIds.has(s.id) && !siblingMap.has(s.id)) siblingMap.set(s.id, s);
    }
    const unconfiguredSiblings = Array.from(siblingMap.values());

    const groupOrder = new Map();
    targets.forEach((t) => {
      if (!groupOrder.has(t.group)) groupOrder.set(t.group, groupOrder.size);
    });
    targets.sort((a, b) => {
      const ga = groupOrder.get(a.group) ?? 99;
      const gb = groupOrder.get(b.group) ?? 99;
      if (ga !== gb) return ga - gb;
      return a.displayName.localeCompare(b.displayName);
    });

    const driftedGroups = (data.driftedGroups || data.drifted_groups || []).map((g) => ({
      slug: g.slug || '',
      diff: g.diff || g.unifiedDiff || g.unified_diff || '',
      variants: (g.variants || g.occurrences || []).map((v) => ({
        path: shortenPath(v.path || ''),
        rawPath: v.path || '',
        targetId: v.targetId || v.target_id || v.targetIdOrRepo || v.target_id_or_repo || '',
        treeHash: v.treeHash || v.tree_hash || '',
        fileCount: Number(v.fileCount ?? v.file_count ?? 0),
        totalBytes: Number(v.totalBytes ?? v.total_bytes ?? 0),
        mtimeEpoch: Number(v.mtimeEpoch ?? v.mtime_epoch ?? 0),
        diffVsFirst: v.diffVsFirst || v.diff_vs_first || v.diff || '',
      })),
    }));

    const duplicateGroups = (data.duplicateGroups || data.duplicate_groups || []).map((g) => ({
      slug: g.slug || '',
      treeHash: g.treeHash || g.tree_hash || '',
      totalBytes: Number(g.totalBytes ?? g.total_bytes ?? 0),
      redundantBytes: Number(g.redundantBytes ?? g.redundant_bytes ?? 0),
      occurrences: (g.occurrences || []).map(normalizeOccurrence),
    }));

    const brokenLinks = (data.brokenLinks || data.broken_links || []).map((b) => ({
      slug: b.slug || '',
      path: shortenPath(b.path || ''),
      rawPath: b.path || '',
      targetId: b.targetId || b.target_id || '',
      danglingTarget: shortenPath(b.danglingTarget || b.dangling_target || b.target || ''),
    }));

    const driftedMap = new Map(driftedGroups.map((g) => [g.slug, g]));
    const dupSlugMap = new Map();
    duplicateGroups.forEach((g) => {
      dupSlugMap.set(g.slug, Math.max(dupSlugMap.get(g.slug) || 0, Math.max(1, (g.occurrences?.length || 2) - 1)));
    });
    const brokenSlugSet = new Set(brokenLinks.map((b) => b.slug));

    const rawSkills = Array.isArray(data.skills)
      ? data.skills
      : (data.skills && typeof data.skills === 'object'
        ? Object.entries(data.skills).map(([slug, v]) => ({ slug, ...v }))
        : []);

    const skills = rawSkills.map((s) => {
      const slug = String(s.slug || s.name || '');
      const fm = s.frontmatter || {};
      const occurrences = (s.occurrences || []).map(normalizeOccurrence);
      const issues = Array.isArray(s.frontmatterIssues || s.frontmatter_issues || fm.issues)
        ? (s.frontmatterIssues || s.frontmatter_issues || fm.issues)
        : [];
      const antigravityValid = Boolean(
        s.antigravityValid ?? s.antigravity_valid ?? fm.antigravityValid ?? fm.antigravity_valid ?? (issues.length === 0)
      );

      const enabledIn = s.enabledIn || s.enabled_in || {};
      const explicitStatus = s.statusByTarget || s.status_by_target || {};
      const driftGroup = driftedMap.get(slug);
      const driftPaths = new Set((driftGroup?.variants || []).slice(1).map((v) => v.targetId));
      const cells = {};

      for (const t of targets) {
        let state = 'off';
        const occ = occurrences.find((o) => o.targetId === t.id);
        if (explicitStatus[t.id]) state = normEntryKind(explicitStatus[t.id]);
        else if (occ) state = occ.entryKind;
        else if (enabledIn[t.id] === true) state = 'on';

        const brokenEntry = brokenLinks.find((b) => b.slug === slug && b.targetId === t.id);
        if (brokenEntry) state = 'broken';
        else if (driftPaths.has(t.id) && (state === 'copy' || state === 'on')) state = 'drift';
        else if (t.strict && !antigravityValid && (state === 'on' || state === 'fm')) state = 'fm';

        cells[t.id] = {
          state,
          occurrence: occ || null,
          broken: brokenEntry || null,
        };
      }

      const cellList = Object.values(cells);
      const hasPhysicalCopy = cellList.some((c) => c.state === 'copy') || occurrences.some((o) => o.entryKind === 'copy');
      const isRepoLocal = Boolean(s.isRepoLocal ?? s.is_repo_local ?? occurrences.some((o) => o.isRepoLocal));
      const repoRoot = shortenPath(s.repoRoot || s.repo_root || occurrences.find((o) => o.isRepoLocal)?.repoRoot || '');
      const rawRepoPath = occurrences.find((o) => o.isRepoLocal)?.rawPath || s.canonicalPath || s.canonical_path || '';
      const drifted = Boolean(s.drifted || driftedMap.has(slug) || cellList.some((c) => c.state === 'drift'));
      const duplicateCount = Number(s.duplicateCount ?? s.duplicate_count ?? dupSlugMap.get(slug) ?? 0);
      const hasBroken = brokenSlugSet.has(slug) || cellList.some((c) => c.state === 'broken');
      const hasShadow = cellList.some((c) => c.state === 'shadow');

      return {
        slug,
        name: s.name || fm.name || slug,
        description: s.description || fm.description || '',
        canonicalPath: shortenPath(s.canonicalPath || s.canonical_path || ''),
        rawCanonicalPath: s.canonicalPath || s.canonical_path || '',
        treeHash: s.treeHash || s.tree_hash || '',
        approxTokens: Number(s.approxTokens ?? s.approx_tokens ?? fm.approxTokens ?? fm.approx_tokens ?? 0),
        tags: Array.isArray(s.tags || fm.tags) ? (s.tags || fm.tags) : [],
        fileCount: Number(s.fileCount ?? s.file_count ?? 1),
        totalBytes: Number(s.totalBytes ?? s.total_bytes ?? 0),
        hasScripts: Boolean(s.hasScripts ?? s.has_scripts),
        hasRefs: Boolean(s.hasRefs ?? s.has_refs ?? s.hasReferences ?? s.has_references),
        isCanonical: Boolean(s.isCanonical ?? s.is_canonical ?? Boolean(s.canonicalPath || s.canonical_path)),
        isRepoLocal,
        repoRoot,
        rawRepoPath,
        antigravityValid,
        frontmatterIssues: issues,
        drifted,
        duplicateCount,
        hasPhysicalCopy,
        hasBroken,
        hasShadow,
        needsAttention: drifted || duplicateCount > 0 || hasBroken || !antigravityValid || hasPhysicalCopy,
        occurrences,
        cells,
      };
    });

    skills.sort((a, b) => a.slug.localeCompare(b.slug));

    for (const t of targets) {
      let active = 0;
      let toks = 0;
      for (const sk of skills) {
        const st = sk.cells[t.id]?.state || 'off';
        if (LOADED_STATES.has(st) || (st === 'fm' && !t.strict)) {
          active += 1;
          toks += sk.approxTokens;
        }
      }
      t.activeCount = active;
      t.approxTokens = toks;
    }

    const rawProfiles = Array.isArray(data.profiles)
      ? data.profiles.map((p) => (typeof p === 'string' ? { name: p, skills: [], wiring: {} } : p))
      : (data.profiles && typeof data.profiles === 'object'
        ? Object.entries(data.profiles).map(([name, val]) => ({
          name,
          skills: Array.isArray(val) ? val : (val?.skills || []),
          wiring: (val && !Array.isArray(val) && val.wiring) ? val.wiring : {},
        }))
        : []);

    const activeProfile = data.activeProfile || data.active_profile || (rawProfiles[0]?.name || 'default');

    const rawStats = data.stats || {};
    const repoSet = new Set(skills.filter((s) => s.repoRoot).map((s) => s.repoRoot));
    const stats = {
      uniqueSkills: Number(rawStats.uniqueSkills ?? rawStats.unique_skills ?? skills.length),
      targetCount: Number(rawStats.targetCount ?? rawStats.target_count ?? targets.length),
      repoCount: Number(rawStats.repoCount ?? rawStats.repo_count ?? repoSet.size),
      dirsScanned: Number(rawStats.dirsScanned ?? rawStats.dirs_scanned ?? 0),
      exactDuplicates: Number(rawStats.exactDuplicates ?? rawStats.exact_duplicates ?? duplicateGroups.length),
      duplicateBytes: Number(rawStats.duplicateBytes ?? rawStats.duplicate_bytes ?? duplicateGroups.reduce((a, g) => a + (g.redundantBytes || 0), 0)),
      driftedCount: Number(rawStats.driftedCount ?? rawStats.drifted_count ?? driftedGroups.length),
      brokenLinkCount: Number(rawStats.brokenLinkCount ?? rawStats.broken_link_count ?? brokenLinks.length),
      frontmatterIssueCount: Number(rawStats.frontmatterIssueCount ?? rawStats.frontmatter_issue_count ?? skills.filter((s) => !s.antigravityValid).length),
      projectLocalCount: Number(rawStats.projectLocalCount ?? rawStats.project_local_count ?? skills.filter((s) => s.isRepoLocal).length),
      unmanagedCount: Number(rawStats.unmanagedCount ?? rawStats.unmanaged_count ?? skills.filter((s) => s.hasPhysicalCopy).length),
    };

    return {
      stats,
      canonicalRoot: shortenPath(data.canonicalRoot || data.canonical_root || '~/.agents/skills'),
      lastScanMs: Number(data.lastScanMs ?? data.last_scan_ms ?? data.scanDurationMs ?? data.scan_duration_ms ?? 0),
      canUndo: Boolean(data.canUndo ?? data.can_undo),
      activeProfile,
      profiles: rawProfiles,
      targets,
      unconfiguredSiblings,
      skills,
      duplicateGroups,
      driftedGroups,
      brokenLinks,
    };
  }

  function createHttpTransport() {
    async function req(url, opts = {}) {
      const res = await fetch(url, {
        headers: { 'Content-Type': 'application/json', ...(opts.headers || {}) },
        ...opts,
      });
      const text = await res.text();
      let json = null;
      try { json = text ? JSON.parse(text) : {}; } catch { json = { raw: text }; }
      if (!res.ok) {
        const msg = (json && (json.error || json.message)) || `HTTP ${res.status}`;
        throw new Error(typeof msg === 'string' ? msg : JSON.stringify(msg));
      }
      return json;
    }
    return {
      getCatalog: () => req('/api/catalog'),
      scan: (params = {}) => req('/api/scan', { method: 'POST', body: JSON.stringify(params) }),
      toggle: (params) => req('/api/toggle', { method: 'POST', body: JSON.stringify(params) }),
      consolidate: (params) => req('/api/consolidate', { method: 'POST', body: JSON.stringify(params) }),
      resolveDrift: (params) => req('/api/resolve-drift', { method: 'POST', body: JSON.stringify(params) }),
      manageTarget: (params) => req('/api/targets', { method: 'POST', body: JSON.stringify(params) }),
      fixFrontmatter: (params) => req('/api/fix-frontmatter', { method: 'POST', body: JSON.stringify(params) }),
      getSkill: (slug) => req(`/api/skill?slug=${encodeURIComponent(slug)}`),
      saveSkill: (params) => req('/api/skill', { method: 'POST', body: JSON.stringify(params) }),
      listProfiles: () => req('/api/profiles'),
      manageProfile: (params) => req('/api/profiles', { method: 'POST', body: JSON.stringify(params) }),
      undo: () => req('/api/undo', { method: 'POST', body: '{}' }),
      subscribe(onEvent) {
        if (typeof EventSource === 'undefined') return () => {};
        const es = new EventSource('/api/events');
        es.onopen = () => onEvent({ type: 'connection', connected: true });
        es.onerror = () => onEvent({ type: 'connection', connected: false });
        const handleCatalog = (ev) => {
          try {
            const payload = ev.data ? JSON.parse(ev.data) : null;
            onEvent({ type: 'catalog', data: payload });
          } catch {
            onEvent({ type: 'refresh' });
          }
        };
        es.addEventListener('catalog', handleCatalog);
        es.onmessage = handleCatalog;
        return () => es.close();
      },
    };
  }

  function wrapTransport(custom) {
    if (!custom) return createHttpTransport();
    const rpcCall = typeof custom.rpc === 'function'
      ? custom.rpc.bind(custom)
      : (typeof custom.call === 'function'
        ? custom.call.bind(custom)
        : (typeof custom.request === 'function' ? custom.request.bind(custom) : null));

    return {
      getCatalog: () => (custom.getCatalog ? custom.getCatalog() : rpcCall('catalog.get', {})),
      scan: (p = {}) => (custom.scan ? custom.scan(p) : rpcCall('catalog.scan', p)),
      toggle: (p) => (custom.toggle ? custom.toggle(p) : rpcCall('skill.toggle', p)),
      consolidate: (p) => (custom.consolidate ? custom.consolidate(p) : rpcCall('skill.consolidate', p)),
      resolveDrift: (p) => (custom.resolveDrift ? custom.resolveDrift(p) : rpcCall('skill.resolveDrift', p)),
      manageTarget: (p) => {
        if (custom.manageTarget) return custom.manageTarget(p);
        if (custom.targets) return custom.targets(p);
        if (p.action === 'add' && custom.addTarget) return custom.addTarget(p);
        if (p.action === 'remove' && custom.removeTarget) return custom.removeTarget(p);
        if (p.action === 'clone' && custom.cloneTarget) return custom.cloneTarget({ fromId: p.fromId, toId: p.toId || p.id });
        if (rpcCall) {
          if (p.action === 'add') return rpcCall('target.add', p);
          if (p.action === 'remove') return rpcCall('target.remove', { id: p.id });
          if (p.action === 'clone') return rpcCall('target.clone', { fromId: p.fromId, toId: p.toId || p.id });
        }
        return Promise.reject(new Error('Transport does not support target management'));
      },
      fixFrontmatter: (p) => (custom.fixFrontmatter ? custom.fixFrontmatter(p) : rpcCall('skill.fixFrontmatter', p)),
      getSkill: (slug) => (custom.getSkill ? custom.getSkill(slug) : (rpcCall ? rpcCall('skill.get', { slug }) : Promise.resolve({ slug }))),
      saveSkill: (p) => (custom.saveSkill ? custom.saveSkill(p) : (rpcCall ? rpcCall('skill.save', p) : Promise.reject(new Error('saveSkill not supported')))),
      listProfiles: () => (custom.listProfiles ? custom.listProfiles() : (rpcCall ? rpcCall('profile.list', {}) : Promise.resolve({ profiles: [] }))),
      manageProfile: (p) => {
        if (custom.manageProfile) return custom.manageProfile(p);
        if (p.action === 'apply' && custom.applyProfile) return custom.applyProfile(p);
        if (p.action === 'save' && custom.saveProfile) return custom.saveProfile(p);
        if (p.action === 'delete' && custom.deleteProfile) return custom.deleteProfile(p);
        if (rpcCall) {
          if (p.action === 'apply') return rpcCall('profile.apply', { name: p.name });
          if (p.action === 'save') return rpcCall('profile.save', { name: p.name, skills: p.skills, wiring: p.wiring });
          if (p.action === 'delete') return rpcCall('profile.delete', { name: p.name });
        }
        return Promise.reject(new Error('Profiles not supported by transport'));
      },
      hydrateWorktree: (p) => (custom.hydrateWorktree ? custom.hydrateWorktree(p) : (rpcCall ? rpcCall('worktree.hydrate', p) : Promise.reject(new Error('Worktree hydration requires host transport')))),
      undo: () => (custom.undo ? custom.undo() : rpcCall('quarantine.undo', {})),
      subscribe: (cb) => (typeof custom.subscribe === 'function' ? custom.subscribe(cb) : () => {}),
    };
  }

  function parseUnifiedDiff(diffText) {
    const rawLines = String(diffText || '').split('\n');
    const unified = [];
    const split = [];
    let adds = 0;
    let dels = 0;
    let la = 0;
    let lb = 0;

    let i = 0;
    while (i < rawLines.length) {
      const line = rawLines[i];
      if (line.startsWith('---') || line.startsWith('+++')) {
        i += 1;
        continue;
      }
      if (line.startsWith('@@')) {
        const m = line.match(/@@ -(\d+)(?:,\d+)? \+(\d+)(?:,\d+)? @@/);
        if (m) {
          la = Number(m[1]) - 1;
          lb = Number(m[2]) - 1;
        }
        unified.push({ kind: 'hunk', text: line });
        split.push({ kind: 'hunk', text: line });
        i += 1;
        continue;
      }
      if (line.startsWith('-') || line.startsWith('+')) {
        const dBlock = [];
        const aBlock = [];
        while (i < rawLines.length && rawLines[i].startsWith('-') && !rawLines[i].startsWith('---')) {
          la += 1;
          dels += 1;
          const txt = rawLines[i].slice(1);
          dBlock.push({ ln: la, text: txt });
          unified.push({ kind: 'del', la, lb: '', text: txt });
          i += 1;
        }
        while (i < rawLines.length && rawLines[i].startsWith('+') && !rawLines[i].startsWith('+++')) {
          lb += 1;
          adds += 1;
          const txt = rawLines[i].slice(1);
          aBlock.push({ ln: lb, text: txt });
          unified.push({ kind: 'add', la: '', lb, text: txt });
          i += 1;
        }
        const n = Math.max(dBlock.length, aBlock.length);
        for (let k = 0; k < n; k += 1) {
          split.push({
            kind: 'pair',
            left: dBlock[k] || null,
            right: aBlock[k] || null,
          });
        }
        continue;
      }
      if (line !== '' || i < rawLines.length - 1) {
        la += 1;
        lb += 1;
        const txt = line.startsWith(' ') ? line.slice(1) : line;
        unified.push({ kind: 'same', la, lb, text: txt });
        split.push({ kind: 'same', la, lb, text: txt });
      }
      i += 1;
    }
    return { unified, split, adds, dels };
  }

  function renderUnifiedHtml(diffText, headerLabel) {
    if (!diffText || !diffText.trim()) {
      return '<div class="diff unified"><div class="diff-head" style="grid-template-columns:1fr"><div>No textual differences in SKILL.md</div></div></div>';
    }
    const { unified } = parseUnifiedDiff(diffText);
    const body = unified.map((row) => {
      if (row.kind === 'hunk') {
        return `<div class="u-line hunk"><span class="ln"></span><span class="ln"></span><span></span><span>${esc(row.text)}</span></div>`;
      }
      if (row.kind === 'del') {
        return `<div class="u-line del"><span class="ln">${row.la}</span><span class="ln"></span><span class="ro">-</span><span>${esc(row.text)}</span></div>`;
      }
      if (row.kind === 'add') {
        return `<div class="u-line add"><span class="ln"></span><span class="ln">${row.lb}</span><span class="em">+</span><span>${esc(row.text)}</span></div>`;
      }
      return `<div class="u-line"><span class="ln">${row.la}</span><span class="ln">${row.lb}</span><span> </span><span>${esc(row.text)}</span></div>`;
    }).join('');
    return `<div class="diff unified">${headerLabel ? `<div class="diff-head" style="grid-template-columns:1fr"><div>${esc(headerLabel)}</div></div>` : ''}${body}</div>`;
  }

  function renderSplitHtml(diffText, leftLabel, rightLabel) {
    if (!diffText || !diffText.trim()) {
      return '<div class="diff"><div class="diff-head" style="grid-template-columns:1fr"><div>No textual differences in SKILL.md</div></div></div>';
    }
    const { split } = parseUnifiedDiff(diffText);
    const body = split.map((row) => {
      if (row.kind === 'hunk') {
        return `<div class="diff-row"><div class="hunk">${esc(row.text)}</div></div>`;
      }
      if (row.kind === 'same') {
        return `<div class="diff-row"><div><span class="ln">${row.la}</span><span>${esc(row.text)}</span></div><div><span class="ln">${row.lb}</span><span>${esc(row.text)}</span></div></div>`;
      }
      const left = row.left
        ? `<div class="del"><span class="ln">${row.left.ln}</span><span>${esc(row.left.text)}</span></div>`
        : '<div class="blank"><span class="ln"></span><span></span></div>';
      const right = row.right
        ? `<div class="add"><span class="ln">${row.right.ln}</span><span>${esc(row.right.text)}</span></div>`
        : '<div class="blank"><span class="ln"></span><span></span></div>';
      return `<div class="diff-row">${left}${right}</div>`;
    }).join('');
    return `<div class="diff"><div class="diff-head"><div>${esc(leftLabel)}</div><div>${esc(rightLabel)}</div></div>${body}</div>`;
  }

  function matchesQuery(sk, q) {
    if (!q) return true;
    const hay = `${sk.slug} ${sk.name} ${sk.description} ${sk.tags.join(' ')} ${sk.repoRoot} ${sk.canonicalPath}`.toLowerCase();
    if (hay.includes(q)) return true;
    let i = 0;
    for (const ch of sk.slug.toLowerCase()) {
      if (ch === q[i]) i += 1;
      if (i === q.length) return true;
    }
    return false;
  }

  class LoadoutSwitchboard extends HTMLElement {
    static get observedAttributes() {
      return ['mode', 'theme', 'active-target', 'worktree-path', 'repo-name', 'scope'];
    }

    constructor() {
      super();
      this._customTransport = null;
      this._api = null;
      this.unsub = null;
      this._toastTimer = null;

      this.state = {
        loading: true,
        scanning: false,
        connected: false,
        view: 'board', // board | wizard
        embeddedScope: 'global', // global | worktree
        catalog: normalizeCatalog({}),
        query: '',
        filter: 'all',
        cursor: { r: 0, c: 0 },
        selected: null,
        lastUndoLabel: '',
        inFlight: new Set(),
        menu: null, // { t, x, y }
        help: false,
        overlay: null, // null | { type: 'diff'|'add'|'inspect', ... }
        wizard: {
          step: 1,
          loading: false,
          applying: false,
          plan: null,
          promoteSet: new Set(),
          log: [],
          receipt: '',
          applied: false,
          error: '',
        },
        toast: null,
      };

      this._onKeyDown = this._onKeyDown.bind(this);
      this._onDocClick = this._onDocClick.bind(this);
      this._onResize = this._onResize.bind(this);
    }

    get transport() {
      return this._customTransport;
    }

    set transport(t) {
      this._customTransport = t;
      if (this.isConnected) {
        this._initTransport();
        this.loadCatalog();
      }
    }

    get isEmbedded() {
      return this.getAttribute('mode') === 'embedded';
    }

    connectedCallback() {
      if (!this.getAttribute('mode')) this.setAttribute('mode', 'standalone');
      try {
        const savedTheme = localStorage.getItem('loadout-theme');
        if (savedTheme && !this.getAttribute('theme') && !this.isEmbedded) {
          document.documentElement.dataset.theme = savedTheme;
        }
      } catch {
        // storage unavailable
      }
      const attrScope = this.getAttribute('scope');
      if (attrScope === 'global' || attrScope === 'worktree') {
        this.state.embeddedScope = attrScope;
      }

      this._mountShell();
      this._initTransport();
      document.addEventListener('keydown', this._onKeyDown);
      document.addEventListener('click', this._onDocClick);
      window.addEventListener('resize', this._onResize);
      this.loadCatalog();
    }

    disconnectedCallback() {
      document.removeEventListener('keydown', this._onKeyDown);
      document.removeEventListener('click', this._onDocClick);
      window.removeEventListener('resize', this._onResize);
      if (this.unsub) {
        this.unsub();
        this.unsub = null;
      }
      clearTimeout(this._toastTimer);
    }

    attributeChangedCallback(name, _oldVal, newVal) {
      if (!this.isConnected) return;
      if (name === 'scope' && (newVal === 'global' || newVal === 'worktree')) {
        this.state.embeddedScope = newVal;
        this._renderMain();
      } else if (name === 'active-target' || name === 'worktree-path' || name === 'repo-name' || name === 'mode') {
        this._renderAll();
      }
    }

    _initTransport() {
      if (this.unsub) {
        this.unsub();
        this.unsub = null;
      }
      const ext = this._customTransport || (typeof window !== 'undefined' ? window.LOADOUT_TRANSPORT : null);
      this._api = wrapTransport(ext);
      this.state.connected = Boolean(ext);
      this.unsub = this._api.subscribe((ev) => {
        if (!ev) return;
        if (ev.type === 'connection') {
          this.state.connected = Boolean(ev.connected);
          this._renderHeader();
          return;
        }
        if (ev.type === 'catalog' && ev.data) {
          this.state.catalog = normalizeCatalog(ev.data);
          this.state.loading = false;
          this._renderAll();
        } else if (ev.type === 'refresh') {
          this.loadCatalog({ silent: true });
        }
      });
    }

    _mountShell() {
      this.innerHTML = `
        <div data-slot="header"></div>
        <div class="stage-body" data-slot="main"></div>
        <div data-slot="overlay"></div>
        <div data-slot="menu"></div>
        <div class="toasts" data-slot="toast" aria-live="polite"></div>
        <div data-slot="help"></div>
      `;

      this.addEventListener('click', (e) => this._handleClick(e));
      this.addEventListener('input', (e) => this._handleInput(e));
      this.addEventListener('change', (e) => this._handleChange(e));
    }

    _onResize() {
      this._syncTopbarHeight();
    }

    _syncTopbarHeight() {
      const stage = this.querySelector('[data-slot="main"]');
      const topbar = stage?.querySelector('.topbar');
      if (stage && topbar) {
        stage.style.setProperty('--topbar-h', `${topbar.offsetHeight}px`);
      }
    }

    showToast(text, opts = {}) {
      this.state.toast = { text, mono: opts.mono || '', undo: Boolean(opts.undo) };
      this._renderToast();
      clearTimeout(this._toastTimer);
      this._toastTimer = setTimeout(() => {
        this.state.toast = null;
        this._renderToast();
      }, opts.duration || 5200);
    }

    async loadCatalog({ silent = false } = {}) {
      if (!silent) {
        this.state.loading = true;
        this._renderMain();
      }
      try {
        const raw = await this._api.getCatalog();
        this.state.catalog = normalizeCatalog(raw);
      } catch (err) {
        if (!silent) this.showToast(`Catalog load failed: ${err.message}`);
      } finally {
        this.state.loading = false;
        this._renderAll();
      }
    }

    async triggerScan() {
      if (this.state.scanning) return;
      this.state.scanning = true;
      this._renderHeader();
      this._renderMain();
      try {
        const raw = await this._api.scan({});
        this.state.catalog = normalizeCatalog(raw);
        this.showToast(`Scanned ${this.state.catalog.skills.length} skills in ${fmtScanMs(this.state.catalog.lastScanMs)} ms`, {
          mono: 'TCC-safe: skipped ~/Library, ~/Pictures, ~/Music, ~/Movies, ~/.Trash',
        });
      } catch (err) {
        this.showToast(`Scan failed: ${err.message}`);
      } finally {
        this.state.scanning = false;
        this._renderAll();
      }
    }

    async triggerUndo() {
      if (!this.state.catalog.canUndo) {
        this.showToast('Nothing to undo');
        return;
      }
      try {
        const res = await this._api.undo();
        if (res && res.catalog) {
          this.state.catalog = normalizeCatalog(res.catalog);
        } else {
          await this.loadCatalog({ silent: true });
        }
        const label = this.state.lastUndoLabel || 'last action';
        this.state.lastUndoLabel = '';
        this.showToast(`Undid: ${label}`, { mono: 'restored from ~/.agents/quarantine' });
        this._renderAll();
      } catch (err) {
        this.showToast(`Undo failed: ${err.message}`);
      }
    }

    visibleSkills() {
      const f = this.state.filter;
      const q = this.state.query.trim().toLowerCase();
      return this.state.catalog.skills.filter((sk) => {
        if (f === 'attention' && !sk.needsAttention) return false;
        if (f === 'drift' && !sk.drifted) return false;
        if (f === 'dupes' && !(sk.duplicateCount > 0 || sk.hasPhysicalCopy)) return false;
        if (f === 'broken' && !sk.hasBroken) return false;
        if (f === 'fm' && sk.antigravityValid) return false;
        if (f === 'repo' && !sk.isRepoLocal) return false;
        return matchesQuery(sk, q);
      });
    }

    async activateCell(slug, targetId) {
      const sk = this.state.catalog.skills.find((s) => s.slug === slug);
      const t = this.state.catalog.targets.find((x) => x.id === targetId);
      if (!sk || !t) return;
      const cell = sk.cells[targetId];
      if (!cell) return;
      const st = cell.state;

      if (st === 'drift' || st === 'copy') {
        this.openDiffOverlay(slug);
        return;
      }
      if (st === 'fm') {
        this.openInspector(slug);
        return;
      }
      if (st === 'shadow') {
        this.showToast(`Shadowed by plugin in ${t.displayName}`, {
          mono: 'disable the plugin to wire the canonical skill',
        });
        return;
      }

      const key = `${slug}::${targetId}`;
      if (this.state.inFlight.has(key)) return;

      const turnOn = st === 'off' || st === 'broken';
      const prevState = st;
      cell.state = turnOn ? (t.strict && !sk.antigravityValid ? 'fm' : 'on') : 'off';
      this._recalcBudgets();
      this.state.inFlight.add(key);
      this._renderMain();
      if (this.state.overlay?.type === 'inspect' && this.state.overlay.slug === slug) this._renderOverlay();

      try {
        const res = await this._api.toggle({ skill: slug, targetId, enabled: turnOn });
        if (res && res.catalog) {
          this.state.catalog = normalizeCatalog(res.catalog);
        } else {
          await this.loadCatalog({ silent: true });
        }
        const actionDesc = prevState === 'broken'
          ? `repaired ${slug} in ${t.displayName}`
          : `${turnOn ? 'wired' : 'unwired'} ${slug} -> ${t.displayName}`;
        this.state.lastUndoLabel = actionDesc;
        const linkPath = `${t.skillsDir}/${slug}`;
        this.showToast(
          prevState === 'broken'
            ? `Repaired dangling link in ${t.displayName}`
            : (turnOn ? `Wired ${slug} -> ${t.displayName}` : `Unwired ${slug} from ${t.displayName}`),
          {
            mono: turnOn ? `ln -s ${this.state.catalog.canonicalRoot}/${slug} ${linkPath}` : `rm ${linkPath} (symlink only)`,
            undo: this.state.catalog.canUndo,
          }
        );
      } catch (err) {
        cell.state = prevState;
        this._recalcBudgets();
        this.showToast(`Toggle failed: ${err.message}`);
      } finally {
        this.state.inFlight.delete(key);
        this._renderAll();
      }
    }

    _recalcBudgets() {
      for (const t of this.state.catalog.targets) {
        let count = 0;
        let tok = 0;
        for (const sk of this.state.catalog.skills) {
          const st = sk.cells[t.id]?.state || 'off';
          if (LOADED_STATES.has(st) || (st === 'fm' && !t.strict)) {
            count += 1;
            tok += sk.approxTokens;
          }
        }
        t.activeCount = count;
        t.approxTokens = tok;
      }
    }

    async setColumn(targetId, turnOn) {
      const t = this.state.catalog.targets.find((x) => x.id === targetId);
      if (!t) return;
      this.state.menu = null;
      this._renderMenu();
      const rows = this.visibleSkills();
      let changed = 0;
      for (const sk of rows) {
        const st = sk.cells[targetId]?.state || 'off';
        if ((turnOn && st === 'off') || (!turnOn && st === 'on')) {
          try {
            await this._api.toggle({ skill: sk.slug, targetId, enabled: turnOn });
            changed += 1;
          } catch {
            // continue batch
          }
        }
      }
      await this.loadCatalog({ silent: true });
      this.state.lastUndoLabel = `${turnOn ? 'enabled' : 'disabled'} ${changed} in ${t.displayName}`;
      this.showToast(`${turnOn ? 'Wired' : 'Unwired'} ${changed} skills in ${t.displayName}`);
    }

    async cloneColumn(fromId, toId) {
      const from = this.state.catalog.targets.find((x) => x.id === fromId);
      const to = this.state.catalog.targets.find((x) => x.id === toId);
      this.state.menu = null;
      this._renderMenu();
      try {
        const res = await this._api.manageTarget({ action: 'clone', id: toId, toId, fromId });
        if (res && res.catalog) this.state.catalog = normalizeCatalog(res.catalog);
        else await this.loadCatalog({ silent: true });
        this.state.lastUndoLabel = `cloned ${from?.displayName || fromId} -> ${to?.displayName || toId}`;
        this.showToast(`Cloned wiring from ${from?.displayName || fromId} -> ${to?.displayName || toId}`);
        this._renderAll();
      } catch (err) {
        this.showToast(`Clone failed: ${err.message}`);
      }
    }

    async removeTarget(targetId) {
      const t = this.state.catalog.targets.find((x) => x.id === targetId);
      this.state.menu = null;
      this._renderMenu();
      if (!t) return;
      if (!window.confirm(`Remove target "${t.displayName}" (${t.skillsDir})?\nManaged symlinks will be unlinked; canonical skills remain untouched.`)) {
        return;
      }
      try {
        const res = await this._api.manageTarget({ action: 'remove', id: targetId });
        if (res && res.catalog) this.state.catalog = normalizeCatalog(res.catalog);
        else await this.loadCatalog({ silent: true });
        this.showToast(`Removed target ${t.displayName}`, { mono: 'managed symlinks unlinked, canonical skills untouched' });
        this._renderAll();
      } catch (err) {
        this.showToast(`Remove failed: ${err.message}`);
      }
    }

    async applyOrSaveProfile(value) {
      if (value === '__save') {
        const name = window.prompt('Save current wiring as profile:', 'my-profile');
        if (!name || !name.trim()) {
          this._renderMain();
          return;
        }
        try {
          const res = await this._api.manageProfile({ action: 'save', name: name.trim() });
          if (res && res.catalog) this.state.catalog = normalizeCatalog(res.catalog);
          else await this.loadCatalog({ silent: true });
          this.showToast(`Saved profile "${name.trim()}"`, { mono: '~/.agents/loadout.lock.json' });
          this._renderAll();
        } catch (err) {
          this.showToast(`Save profile failed: ${err.message}`);
          this._renderMain();
        }
        return;
      }
      try {
        const res = await this._api.manageProfile({ action: 'apply', name: value });
        if (res && res.catalog) this.state.catalog = normalizeCatalog(res.catalog);
        else await this.loadCatalog({ silent: true });
        this.state.lastUndoLabel = `profile -> ${value}`;
        this.showToast(`Profile -> ${value}`, { undo: this.state.catalog.canUndo });
        this._renderAll();
      } catch (err) {
        this.showToast(`Apply profile failed: ${err.message}`);
        this._renderMain();
      }
    }

    // ── Wizard (Scan & Consolidate) ─────────────────────────────────────────
    async openWizardStep(step) {
      this.state.view = 'wizard';
      this.state.overlay = null;
      this.state.wizard.step = step;
      this.state.wizard.error = '';
      this._renderAll();
      if (step === 2) {
        await this.loadConsolidateDryRun();
      }
    }

    async loadConsolidateDryRun() {
      const w = this.state.wizard;
      w.loading = true;
      w.error = '';
      this._renderMain();
      try {
        const promoteRepoSkills = Array.from(w.promoteSet);
        w.plan = await this._api.consolidate({ dryRun: true, promoteRepoSkills });
      } catch (err) {
        w.error = err.message;
      } finally {
        w.loading = false;
        this._renderMain();
      }
    }

    async applyWizardConsolidation() {
      const w = this.state.wizard;
      w.step = 3;
      w.applying = true;
      w.applied = false;
      w.log = [];
      w.error = '';
      this._renderMain();

      try {
        const promoteRepoSkills = Array.from(w.promoteSet);
        const res = await this._api.consolidate({ dryRun: false, promoteRepoSkills });
        const ops = this._extractPlanOps(res);
        w.log = ops.map((op) => {
          const cls = { MOVE: 'sk', LINK: 'em', QUARANTINE: 'am', REPAIR: 'ro', PROMOTE: 'sk' }[op.op] || 'dim';
          return `<span class="${cls}">${esc(op.op.padEnd(10))}</span> ${esc(op.src)} <span class="faint">${esc(op.dst)}</span>`;
        });
        const qPath = shortenPath(res.quarantineDir || res.quarantine_dir || '~/.agents/quarantine/');
        w.receipt = qPath;
        w.log.push(`<span class="em">\u2713 done</span> <span class="faint">completed ${ops.length} operations · quarantine ${esc(qPath)}</span>`);
        w.applied = true;
        this.state.lastUndoLabel = 'consolidation';
        if (res && res.catalog) {
          this.state.catalog = normalizeCatalog(res.catalog);
        } else {
          await this.loadCatalog({ silent: true });
        }
        this.showToast('Consolidated into ~/.agents/skills', { mono: `quarantine ${qPath}`, undo: true });
      } catch (err) {
        w.error = err.message;
        w.log.push(`<span class="ro">ERROR     </span> ${esc(err.message)}`);
        w.applied = true;
      } finally {
        w.applying = false;
        this._renderAll();
      }
    }

    _extractPlanOps(plan) {
      const raw = plan?.moves || plan?.actions || plan?.operations || plan?.plan?.actions || [];
      return raw.map((item, idx) => {
        const rawKind = String(item.op || item.action || item.kind || 'MOVE').toUpperCase();
        let op = 'MOVE';
        if (rawKind.includes('QUARANTINE')) op = 'QUARANTINE';
        else if (rawKind.includes('LINK') || rawKind.includes('SYMLINK')) op = 'LINK';
        else if (rawKind.includes('REPAIR') || rawKind.includes('FIX')) op = 'REPAIR';
        else if (rawKind.includes('PROMOTE')) op = 'MOVE';
        else if (rawKind.includes('REVIEW') || rawKind.includes('SKIP')) op = 'REVIEW';
        else if (rawKind.includes('KEEP')) op = 'KEEP';
        return {
          id: idx + 1,
          op,
          slug: item.slug || item.skill || '',
          src: shortenPath(item.src || item.from || item.source || item.path || ''),
          dst: shortenPath(item.dst || item.to || item.destination || item.target || ''),
          note: item.note || item.reason || item.detail || '',
        };
      });
    }

    // ── Overlays: Diff / Duplicate Resolver ─────────────────────────────────
    openDiffOverlay(slug) {
      const sk = this.state.catalog.skills.find((s) => s.slug === slug);
      const driftGroup = this.state.catalog.driftedGroups.find((g) => g.slug === slug);
      const isDrift = Boolean(driftGroup || sk?.drifted);
      const second = driftGroup?.variants?.[1];
      const suffix = slugify(second?.targetId || 'alt') || 'alt';

      this.state.menu = null;
      this.state.selected = slug;
      this.state.overlay = {
        type: 'diff',
        slug,
        isDrift,
        mode: 'split', // split | unified
        tab: 'skill', // skill | tree
        choice: isDrift ? 'a' : 'consolidate',
        keepBothSlug: `${slug}-${suffix}`,
        busy: false,
      };
      this._renderAll();
    }

    async applyDiffResolution() {
      const ov = this.state.overlay;
      if (!ov || ov.type !== 'diff' || ov.busy) return;
      ov.busy = true;
      this._renderOverlay();

      try {
        if (!ov.isDrift || ov.choice === 'consolidate') {
          const res = await this._api.consolidate({ dryRun: false, promoteRepoSkills: [] });
          if (res && res.catalog) this.state.catalog = normalizeCatalog(res.catalog);
          else await this.loadCatalog({ silent: true });
          this.state.lastUndoLabel = `consolidated ${ov.slug}`;
          this.state.overlay = null;
          this.showToast(`Consolidated ${ov.slug} into ~/.agents/skills`, { mono: 'superseded copies moved to ~/.agents/quarantine/', undo: true });
          this._renderAll();
          return;
        }

        const g = this.state.catalog.driftedGroups.find((x) => x.slug === ov.slug);
        const variants = g?.variants || [];
        const pickIdx = ov.choice === 'b' ? 1 : 0;
        const chosen = variants[pickIdx] || variants[0];
        if (!chosen) throw new Error('No variant path available');

        const payload = { slug: ov.slug, chosenPath: chosen.rawPath || chosen.path };
        if (ov.choice === 'both') {
          const clean = slugify(ov.keepBothSlug);
          if (!clean || clean === ov.slug) throw new Error('Enter a distinct new slug name');
          payload.keepBothSlug = clean;
          if (variants[1]) payload.chosenPath = variants[0].rawPath || variants[0].path;
        }

        const res = await this._api.resolveDrift(payload);
        if (res && res.catalog) this.state.catalog = normalizeCatalog(res.catalog);
        else await this.loadCatalog({ silent: true });
        this.state.lastUndoLabel = `resolved ${ov.slug}`;
        this.state.overlay = null;
        this.showToast(
          ov.choice === 'both'
            ? `Kept both: ${ov.slug} and ${payload.keepBothSlug}`
            : `Instance ${ov.choice.toUpperCase()} is now canonical for ${ov.slug}`,
          { mono: 'superseded copies moved to ~/.agents/quarantine/', undo: true }
        );
        this._renderAll();
      } catch (err) {
        if (this.state.overlay) {
          this.state.overlay.busy = false;
          this._renderOverlay();
        }
        this.showToast(`Resolution failed: ${err.message}`);
      }
    }

    // ── Overlays: Add Target / Account ──────────────────────────────────────
    openAddTargetOverlay(prefill = null) {
      const defaultClone = this.state.catalog.targets[0]?.id || '';
      const adapterId = prefill?.adapter || 'claude-code';
      this.state.menu = null;
      this.state.overlay = {
        type: 'add',
        name: prefill?.displayName || '',
        id: prefill?.id || '',
        idTouched: Boolean(prefill?.id),
        path: prefill?.rawSkillsDir || prefill?.skillsDir || '',
        adapter: adapterId,
        env: prefill?.envHint || '',
        initial: defaultClone ? 'clone' : 'empty',
        cloneFrom: defaultClone,
        error: '',
        busy: false,
      };
      this._renderOverlay();
      setTimeout(() => this.querySelector('#add-name')?.focus(), 10);
    }

    async quickAddSibling(siblingId) {
      const sib = this.state.catalog.unconfiguredSiblings.find((s) => s.id === siblingId);
      if (!sib) return;
      try {
        const cloneCandidate = this.state.catalog.targets.find((t) => t.adapter === sib.adapter)?.id || '';
        const res = await this._api.manageTarget({
          action: 'add',
          id: sib.id,
          displayName: sib.displayName,
          skillsDir: sib.rawSkillsDir || sib.skillsDir,
          adapter: sib.adapter,
          envHint: sib.envHint || undefined,
          cloneFrom: cloneCandidate || undefined,
        });
        if (res && res.catalog) this.state.catalog = normalizeCatalog(res.catalog);
        else await this.loadCatalog({ silent: true });
        if (this.state.overlay?.type === 'add') this.state.overlay = null;
        this.showToast(`Added target ${sib.displayName}`, { mono: `${sib.skillsDir} · ${sib.adapter}` });
        this._renderAll();
      } catch (err) {
        this.showToast(`Add failed: ${err.message}`);
      }
    }

    async submitAddTarget() {
      const a = this.state.overlay;
      if (!a || a.type !== 'add' || a.busy) return;
      const name = a.name.trim();
      const path = a.path.trim().replace(/\/$/, '');
      const id = slugify(a.id || name);
      if (!name) { a.error = 'Display name is required.'; this._renderOverlay(); return; }
      if (!id) { a.error = 'Target ID is required.'; this._renderOverlay(); return; }
      if (!/^(~|\/)/.test(path)) { a.error = 'Path must start with ~ or /.'; this._renderOverlay(); return; }

      a.busy = true;
      a.error = '';
      this._renderOverlay();

      try {
        const cloneFrom = a.initial === 'clone' ? a.cloneFrom : '';
        const res = await this._api.manageTarget({
          action: 'add',
          id,
          displayName: name,
          skillsDir: path,
          adapter: a.adapter,
          envHint: a.env.trim() || undefined,
          cloneFrom: cloneFrom || undefined,
        });
        if (a.initial === 'profile' && this.state.catalog.activeProfile) {
          try {
            await this._api.manageProfile({ action: 'apply', name: this.state.catalog.activeProfile });
          } catch {
            // profile optional
          }
        }
        if (res && res.catalog) this.state.catalog = normalizeCatalog(res.catalog);
        else await this.loadCatalog({ silent: true });
        this.state.overlay = null;
        this.showToast(`Added target ${name}`, { mono: `${shortenPath(path)} · ${a.adapter}` });
        this._renderAll();
      } catch (err) {
        a.busy = false;
        a.error = err.message;
        this._renderOverlay();
      }
    }

    // ── Overlays: Slide-Over Skill Inspector ────────────────────────────────
    async openInspector(slug) {
      const sk = this.state.catalog.skills.find((s) => s.slug === slug);
      this.state.menu = null;
      this.state.selected = slug;
      this.state.overlay = {
        type: 'inspect',
        slug,
        loading: true,
        detail: null,
        files: [],
        activeFile: 'SKILL.md',
        editing: false,
        draft: '',
        saving: false,
        showFix: Boolean(sk && !sk.antigravityValid),
        fmPreview: null,
        fmBusy: false,
      };
      this._renderOverlay();

      try {
        const detail = await this._api.getSkill(slug);
        if (this.state.overlay?.type === 'inspect' && this.state.overlay.slug === slug) {
          const files = this._normalizeSkillFiles(detail);
          this.state.overlay.loading = false;
          this.state.overlay.detail = detail;
          this.state.overlay.files = files;
          this.state.overlay.activeFile = files[0]?.path || 'SKILL.md';
          this.state.overlay.draft = files[0]?.content || '';
          this._renderOverlay();
          if (sk && !sk.antigravityValid) {
            this.loadFrontmatterPreview(slug);
          }
        }
      } catch (err) {
        if (this.state.overlay?.type === 'inspect' && this.state.overlay.slug === slug) {
          this.state.overlay.loading = false;
          this.state.overlay.error = err.message;
          this._renderOverlay();
        }
      }
    }

    _normalizeSkillFiles(detail) {
      if (!detail) return [{ path: 'SKILL.md', size: 0, isText: true, content: '' }];
      const md = detail.markdown ?? detail.content ?? detail.skillMd ?? '';
      if (Array.isArray(detail.files) && detail.files.length > 0) {
        if (typeof detail.files[0] === 'object' && detail.files[0] !== null) {
          return detail.files.map((f) => ({
            path: f.path || f.name || 'SKILL.md',
            size: Number(f.size || 0),
            isText: f.isText !== false && f.is_text !== false,
            content: f.content ?? (f.path === 'SKILL.md' ? md : ''),
          }));
        }
        return detail.files.map((p) => ({
          path: String(p),
          size: 0,
          isText: true,
          content: String(p) === 'SKILL.md' ? md : '',
        }));
      }
      return [{ path: 'SKILL.md', size: md.length, isText: true, content: md }];
    }

    async loadFrontmatterPreview(slug) {
      const ov = this.state.overlay;
      if (!ov || ov.type !== 'inspect' || ov.slug !== slug) return;
      ov.fmBusy = true;
      this._renderOverlay();
      try {
        const res = await this._api.fixFrontmatter({ skill: slug, apply: false });
        if (this.state.overlay?.type === 'inspect' && this.state.overlay.slug === slug) {
          this.state.overlay.fmPreview = res;
        }
      } catch {
        // non-fatal
      } finally {
        if (this.state.overlay?.type === 'inspect' && this.state.overlay.slug === slug) {
          this.state.overlay.fmBusy = false;
          this._renderOverlay();
        }
      }
    }

    async applyFrontmatterFix(slug) {
      const ov = this.state.overlay;
      if (!ov || ov.type !== 'inspect' || ov.fmBusy) return;
      ov.fmBusy = true;
      this._renderOverlay();
      try {
        const res = await this._api.fixFrontmatter({ skill: slug, apply: true });
        if (res && res.catalog) this.state.catalog = normalizeCatalog(res.catalog);
        else await this.loadCatalog({ silent: true });
        this.state.lastUndoLabel = `fixed frontmatter in ${slug}`;
        this.showToast(`Frontmatter fixed · ${slug} now loads in Antigravity`, {
          mono: `${this.state.catalog.canonicalRoot}/${slug}/SKILL.md`,
          undo: this.state.catalog.canUndo,
        });
        await this.openInspector(slug);
      } catch (err) {
        if (this.state.overlay) {
          this.state.overlay.fmBusy = false;
          this._renderOverlay();
        }
        this.showToast(`Frontmatter fix failed: ${err.message}`);
      }
    }

    async saveInspectorFile() {
      const ov = this.state.overlay;
      if (!ov || ov.type !== 'inspect' || ov.saving) return;
      const editor = this.querySelector('#insp-editor');
      const content = editor ? editor.value : ov.draft;
      const relPath = ov.activeFile || 'SKILL.md';
      ov.saving = true;
      this._renderOverlay();
      try {
        await this._api.saveSkill({
          slug: ov.slug,
          relPath,
          content,
          markdown: relPath === 'SKILL.md' ? content : undefined,
        });
        const entry = ov.files.find((f) => f.path === relPath);
        if (entry) entry.content = content;
        ov.draft = content;
        ov.editing = false;
        await this.loadCatalog({ silent: true });
        this.showToast(`Saved ${relPath}`, {
          mono: `${this.state.catalog.canonicalRoot}/${ov.slug}/${relPath} · all wired targets see it instantly`,
        });
      } catch (err) {
        this.showToast(`Save failed: ${err.message}`);
      } finally {
        if (this.state.overlay?.type === 'inspect') {
          this.state.overlay.saving = false;
          this._renderOverlay();
        }
      }
    }

    async triggerWorktreeHydrate() {
      const wtPath = this.getAttribute('worktree-path') || '';
      if (!wtPath) {
        this.showToast('No active worktree path configured');
        return;
      }
      try {
        await this._api.hydrateWorktree({
          worktreePath: wtPath,
          repoRoot: this.getAttribute('repo-name') || undefined,
        });
        this.showToast('Hydrated worktree symlinks', { mono: `${shortenPath(wtPath)} · .git/info/exclude updated` });
      } catch (err) {
        this.showToast(`Hydrate failed: ${err.message}`);
      }
    }

    // ── Events & Keyboard ───────────────────────────────────────────────────
    _onDocClick(e) {
      if (this.state.menu && !e.target.closest('.menu') && !e.target.closest('[data-action="colmenu"]')) {
        this.state.menu = null;
        this._renderMenu();
      }
      if (this.state.help && !e.target.closest('.help') && !e.target.closest('[data-action="help"]')) {
        this.state.help = false;
        this._renderHelp();
      }
    }

    _onKeyDown(e) {
      const tag = (e.target?.tagName || '').toLowerCase();
      const typing = tag === 'input' || tag === 'textarea' || tag === 'select' || e.target?.isContentEditable;

      if (e.key === 'Escape') {
        if (this.state.menu) { this.state.menu = null; this._renderMenu(); return; }
        if (this.state.help) { this.state.help = false; this._renderHelp(); return; }
        if (this.state.overlay) { this.state.overlay = null; this._renderOverlay(); return; }
        if (typing) { e.target.blur(); return; }
        return;
      }

      if (typing || e.metaKey || e.ctrlKey || e.altKey) return;
      if (e.key === '?') { this.state.help = !this.state.help; this._renderHelp(); return; }
      if (e.key === 'u' && !this.state.overlay) { this.triggerUndo(); return; }

      if (this.isEmbedded || this.state.view !== 'board' || this.state.overlay) return;

      const rows = this.visibleSkills();
      const ts = this.state.catalog.targets;
      const c = this.state.cursor;
      const s = rows[c.r];

      switch (e.key) {
        case '/':
          e.preventDefault();
          this.querySelector('#sk-search')?.focus();
          return;
        case 'j':
        case 'ArrowDown':
          c.r = Math.min(Math.max(0, rows.length - 1), c.r + 1);
          break;
        case 'k':
        case 'ArrowUp':
          c.r = Math.max(0, c.r - 1);
          break;
        case 'l':
        case 'ArrowRight':
          c.c = Math.min(Math.max(0, ts.length - 1), c.c + 1);
          break;
        case 'h':
        case 'ArrowLeft':
          c.c = Math.max(0, c.c - 1);
          break;
        case ' ':
          if (s && ts[c.c]) {
            e.preventDefault();
            this.activateCell(s.slug, ts[c.c].id);
          }
          return;
        case 'Enter':
          if (s) {
            e.preventDefault();
            this.openInspector(s.slug);
          }
          return;
        case 'd':
          if (s && (s.drifted || s.duplicateCount > 0 || s.hasPhysicalCopy)) {
            this.openDiffOverlay(s.slug);
          } else if (this.state.catalog.driftedGroups.length > 0) {
            this.openDiffOverlay(this.state.catalog.driftedGroups[0].slug);
          } else {
            this.showToast('No drift or duplicates for this skill');
          }
          return;
        default:
          return;
      }
      e.preventDefault();
      this._renderMain();
      this.querySelector('.jack.cur')?.scrollIntoView({ block: 'nearest', inline: 'nearest' });
    }

    _handleClick(e) {
      const el = e.target.closest('[data-action]');
      if (!el) return;
      const a = el.dataset.action;
      const ds = el.dataset;

      switch (a) {
        case 'view':
          if (ds.v === 'wizard') this.openWizardStep(this.state.wizard.step || 1);
          else { this.state.view = 'board'; this._renderAll(); }
          break;
        case 'theme': {
          const cur = (this.getAttribute('theme') || document.documentElement.dataset.theme || 'dark') === 'dark' ? 'light' : 'dark';
          document.documentElement.dataset.theme = cur;
          this.setAttribute('theme', cur);
          try { localStorage.setItem('loadout-theme', cur); } catch { /* noop */ }
          break;
        }
        case 'help':
          this.state.help = !this.state.help;
          this._renderHelp();
          break;
        case 'undo':
          this.triggerUndo();
          break;
        case 'scan':
          this.triggerScan();
          break;
        case 'filter':
          this.state.filter = ds.f;
          this.state.cursor.r = 0;
          this._renderMain();
          break;
        case 'clear-filters':
          this.state.filter = 'all';
          this.state.query = '';
          this._renderMain();
          break;
        case 'cell': {
          const rows = this.visibleSkills();
          const ri = rows.findIndex((s) => s.slug === ds.slug);
          const ci = this.state.catalog.targets.findIndex((t) => t.id === ds.t);
          if (ri >= 0 && ci >= 0) { this.state.cursor.r = ri; this.state.cursor.c = ci; }
          this.activateCell(ds.slug, ds.t);
          break;
        }
        case 'inspect':
          e.stopPropagation();
          this.openInspector(ds.slug);
          break;
        case 'diff':
          e.stopPropagation();
          this.openDiffOverlay(ds.slug);
          break;
        case 'add-target':
          this.openAddTargetOverlay();
          break;
        case 'add-detected':
          this.quickAddSibling(ds.id);
          break;
        case 'close':
          this.state.overlay = null;
          this._renderOverlay();
          break;
        case 'colmenu': {
          e.stopPropagation();
          const r = el.getBoundingClientRect();
          if (this.state.menu?.t === ds.t) this.state.menu = null;
          else this.state.menu = { t: ds.t, x: Math.min(r.left, window.innerWidth - 250), y: Math.min(r.bottom + 4, window.innerHeight - 240) };
          this._renderMenu();
          break;
        }
        case 'col-all':
          this.setColumn(ds.t, ds.on === '1');
          break;
        case 'col-clone':
          this.cloneColumn(ds.from, ds.t);
          break;
        case 'col-copy-env': {
          const t = this.state.catalog.targets.find((x) => x.id === ds.t);
          const text = t?.envHint || t?.skillsDir || '';
          navigator.clipboard?.writeText(text).catch(() => {});
          this.state.menu = null;
          this._renderMenu();
          this.showToast(`Copied: ${text}`);
          break;
        }
        case 'col-remove':
          this.removeTarget(ds.t);
          break;
        // Wizard
        case 'wiz-step':
          if (Number(ds.step) === 3 && !this.state.wizard.log.length) break;
          this.openWizardStep(Number(ds.step));
          break;
        case 'repo-choice': {
          const p = ds.path;
          if (ds.v === 'promote') this.state.wizard.promoteSet.add(p);
          else this.state.wizard.promoteSet.delete(p);
          this.loadConsolidateDryRun();
          break;
        }
        case 'wiz-apply':
          this.applyWizardConsolidation();
          break;
        // Diff resolver
        case 'diff-tab':
          if (this.state.overlay?.type === 'diff') { this.state.overlay.tab = ds.v; this._renderOverlay(); }
          break;
        case 'diff-mode':
          if (this.state.overlay?.type === 'diff') { this.state.overlay.mode = ds.v; this.state.overlay.tab = 'skill'; this._renderOverlay(); }
          break;
        case 'diff-choice':
          if (this.state.overlay?.type === 'diff') { this.state.overlay.choice = ds.v; this._renderOverlay(); }
          break;
        case 'resolve':
          this.applyDiffResolution();
          break;
        // Add target
        case 'add-adapter':
          if (this.state.overlay?.type === 'add') { this.state.overlay.adapter = ds.v; this._renderOverlay(); }
          break;
        case 'add-initial':
          if (this.state.overlay?.type === 'add') { this.state.overlay.initial = ds.v; this._renderOverlay(); }
          break;
        case 'add-submit':
          this.submitAddTarget();
          break;
        // Inspector
        case 'toggle-fix':
          if (this.state.overlay?.type === 'inspect') {
            this.state.overlay.showFix = !this.state.overlay.showFix;
            if (this.state.overlay.showFix && !this.state.overlay.fmPreview) {
              this.loadFrontmatterPreview(this.state.overlay.slug);
            } else {
              this._renderOverlay();
            }
          }
          break;
        case 'apply-fix':
          this.applyFrontmatterFix(ds.slug);
          break;
        case 'insp-file':
          if (this.state.overlay?.type === 'inspect') {
            const f = this.state.overlay.files.find((x) => x.path === ds.f);
            this.state.overlay.activeFile = ds.f;
            this.state.overlay.draft = f?.content || '';
            this.state.overlay.editing = false;
            this._renderOverlay();
          }
          break;
        case 'insp-edit':
          if (this.state.overlay?.type === 'inspect') {
            this.state.overlay.editing = true;
            this._renderOverlay();
            this.querySelector('#insp-editor')?.focus();
          }
          break;
        case 'insp-cancel':
          if (this.state.overlay?.type === 'inspect') {
            this.state.overlay.editing = false;
            this._renderOverlay();
          }
          break;
        case 'insp-save':
          this.saveInspectorFile();
          break;
        // Embedded drawer
        case 'emb-scope':
          this.state.embeddedScope = ds.v;
          this._renderMain();
          break;
        case 'emb-hydrate':
          this.triggerWorktreeHydrate();
          break;
        case 'emb-open-full':
          this.dispatchEvent(new CustomEvent('loadout-open-full', { bubbles: true, composed: true }));
          break;
        default:
          break;
      }
    }

    _handleInput(e) {
      const key = e.target?.dataset?.input;
      if (!key) return;
      if (key === 'query') {
        this.state.query = e.target.value;
        this.state.cursor.r = 0;
        this._renderMatrixOnly();
      } else if (key === 'diff.keepBothSlug' && this.state.overlay?.type === 'diff') {
        this.state.overlay.keepBothSlug = e.target.value;
      } else if (key.startsWith('add.') && this.state.overlay?.type === 'add') {
        const field = key.slice(4);
        this.state.overlay[field] = e.target.value;
        this.state.overlay.error = '';
        if (field === 'id') this.state.overlay.idTouched = true;
        if (field === 'name' && !this.state.overlay.idTouched) {
          this.state.overlay.id = slugify(e.target.value);
          const idEl = this.querySelector('#add-id');
          if (idEl) idEl.value = this.state.overlay.id;
        }
        const opsEl = this.querySelector('[data-slot="add-ops"]');
        if (opsEl) opsEl.innerHTML = this._tplAddOps(this.state.overlay);
      }
    }

    _handleChange(e) {
      const key = e.target?.dataset?.change;
      if (key === 'profile') {
        this.applyOrSaveProfile(e.target.value);
      } else if (key === 'add.cloneFrom' && this.state.overlay?.type === 'add') {
        this.state.overlay.cloneFrom = e.target.value;
        this._renderOverlay();
      }
    }

    // ── Rendering ───────────────────────────────────────────────────────────
    _renderAll() {
      this._renderHeader();
      this._renderMain();
      this._renderOverlay();
      this._renderMenu();
      this._renderHelp();
      this._renderToast();
    }

    _renderHeader() {
      const slot = this.querySelector('[data-slot="header"]');
      if (!slot) return;
      if (this.isEmbedded) {
        slot.innerHTML = '';
        return;
      }
      const c = this.state.catalog;
      const consolidatable = c.stats.exactDuplicates + c.stats.unmanagedCount;
      slot.innerHTML = `
        <header class="sk-header">
          <div class="brand">
            <b>l<i>o</i>adout</b>
            <span class="live-dot ${this.state.connected ? '' : 'off'}" title="${this.state.connected ? 'Live SSE synced' : 'Local mode'}"></span>
          </div>
          <div class="seg" role="group" aria-label="Workspace view">
            <button data-action="view" data-v="board" aria-pressed="${this.state.view === 'board'}">Switchboard</button>
            <button data-action="view" data-v="wizard" aria-pressed="${this.state.view === 'wizard'}">
              Scan &amp; Consolidate
              ${consolidatable > 0 ? `<span class="badge am">${consolidatable}</span>` : ''}
            </button>
          </div>
          <div class="spacer"></div>
          <button class="btn sm" data-action="scan" ${this.state.scanning ? 'disabled' : ''} title="Rescan laptop (last scan ${fmtScanMs(c.lastScanMs)} ms)">
            ${this.state.scanning ? 'Scanning...' : '\u21bb Rescan'}
          </button>
          <button class="btn sm" data-action="theme">Light / Dark</button>
          <button class="btn sm" data-action="help"><kbd>?</kbd> Keys</button>
        </header>
      `;
    }

    _renderMain() {
      const slot = this.querySelector('[data-slot="main"]');
      if (!slot) return;

      if (this.isEmbedded) {
        slot.innerHTML = this._tplEmbeddedDrawer();
        return;
      }
      if (this.state.view === 'wizard') {
        slot.innerHTML = this._tplWizard();
        return;
      }

      const activeEl = document.activeElement;
      const preserveSearch = activeEl && activeEl.id === 'sk-search';
      const selStart = preserveSearch ? activeEl.selectionStart : null;

      slot.innerHTML = this._tplBoard();
      this._syncTopbarHeight();

      if (preserveSearch) {
        const nextSearch = this.querySelector('#sk-search');
        if (nextSearch) {
          nextSearch.focus();
          if (selStart !== null) {
            try { nextSearch.setSelectionRange(selStart, selStart); } catch { /* noop */ }
          }
        }
      }
    }

    _renderMatrixOnly() {
      const wrap = this.querySelector('[data-slot="matrix-wrap"]');
      if (!wrap) { this._renderMain(); return; }
      wrap.innerHTML = this._tplMatrixTable();
    }

    _tplBoard() {
      const c = this.state.catalog;
      const skills = c.skills;
      const filters = [
        { id: 'all', label: 'All', count: skills.length },
        { id: 'attention', label: 'Needs Attention', count: skills.filter((s) => s.needsAttention).length, dot: 'var(--accent)' },
        { id: 'drift', label: 'Drifted', count: skills.filter((s) => s.drifted).length, dot: 'var(--amber)' },
        { id: 'dupes', label: 'Duplicates', count: skills.filter((s) => s.duplicateCount > 0 || s.hasPhysicalCopy).length, dot: 'var(--amber)' },
        { id: 'broken', label: 'Broken', count: skills.filter((s) => s.hasBroken).length, dot: 'var(--rose)' },
        { id: 'fm', label: 'Frontmatter Issues', count: skills.filter((s) => !s.antigravityValid).length, dot: 'var(--rose)' },
        { id: 'repo', label: 'Project-Local', count: skills.filter((s) => s.isRepoLocal).length, dot: 'var(--sky)' },
      ];

      const pillsHtml = filters.map((f) => `
        <button class="pill" data-action="filter" data-f="${f.id}" aria-pressed="${this.state.filter === f.id}">
          ${f.dot ? `<span class="dot" style="background:${f.dot}"></span>` : ''}${f.label} <b>${f.count}</b>
        </button>
      `).join('');

      const profList = c.profiles.length ? c.profiles : [{ name: c.activeProfile || 'default' }];
      const profileOpts = profList.map((p) => `
        <option value="${esc(p.name)}" ${p.name === c.activeProfile ? 'selected' : ''}>${esc(p.name)}</option>
      `).join('');

      const siblingsHtml = c.unconfiguredSiblings.length ? `
        <div class="sibling-bar">
          <span class="mono am" style="font-size:11px;font-weight:600">DETECTED ON DISK:</span>
          ${c.unconfiguredSiblings.map((s) => `
            <span class="sibling-chip" title="${esc(s.envHint || s.skillsDir)}">
              <b>${esc(s.displayName)}</b>
              <span class="mono faint">${esc(s.skillsDir)}</span>
              <button class="btn sm primary" data-action="add-detected" data-id="${esc(s.id)}">Add in 1 click</button>
            </span>
          `).join('')}
        </div>
      ` : '';

      return `
        <div class="topbar">
          <div class="search">
            ${ICON_SEARCH}
            <input id="sk-search" class="input" placeholder="Search skills, tags, paths" value="${esc(this.state.query)}" data-input="query" aria-label="Search skills" autocomplete="off" spellcheck="false">
            <kbd>/</kbd>
          </div>
          <div class="pills" role="group" aria-label="Filters">${pillsHtml}</div>
          <div class="spacer"></div>
          <label class="profile">
            Profile
            <select class="select" data-change="profile" aria-label="Active profile">
              ${profileOpts}
              <option value="__save">+ Save current wiring as profile...</option>
            </select>
          </label>
          ${c.canUndo ? `
            <div class="undo-pill">
              <span>\u21b6 ${esc(this.state.lastUndoLabel || 'Quarantine ready')}</span>
              <button class="btn sm" data-action="undo">Undo <kbd>u</kbd></button>
            </div>
          ` : `
            <div class="undo-pill"><span class="faint">No undo history</span></div>
          `}
        </div>
        ${siblingsHtml}
        <div class="matrix-wrap" data-slot="matrix-wrap">
          ${this._tplMatrixTable()}
        </div>
        <div class="legend">
          ${['on', 'off', 'copy', 'drift', 'fm', 'shadow', 'broken'].map((st) => `
            <span>
              <span class="jack" data-s="${st}"><span class="j">${STATE_GLYPH[st]}</span></span>
              ${{
                on: 'Wired symlink',
                off: 'Unwired',
                copy: 'Unmanaged physical copy',
                drift: 'Drifted copy',
                fm: 'Frontmatter incompatible',
                shadow: 'Shadowed by plugin',
                broken: 'Broken symlink',
              }[st]}
            </span>
          `).join('')}
          <span class="keys"><kbd>/</kbd> search <kbd>j</kbd><kbd>k</kbd><kbd>h</kbd><kbd>l</kbd> move <kbd>Space</kbd> toggle <kbd>Enter</kbd> inspect <kbd>d</kbd> diff <kbd>u</kbd> undo</span>
        </div>
      `;
    }

    _tplMatrixTable() {
      const c = this.state.catalog;
      const targets = c.targets;
      const rows = this.visibleSkills();

      if (this.state.loading) {
        return '<div style="padding:56px;text-align:center;color:var(--faint)" class="mono">Scanning skill directories...</div>';
      }

      this.state.cursor.r = Math.max(0, Math.min(this.state.cursor.r, Math.max(0, rows.length - 1)));
      this.state.cursor.c = Math.max(0, Math.min(this.state.cursor.c, Math.max(0, targets.length - 1)));

      const groups = [];
      targets.forEach((t) => {
        const g = groups[groups.length - 1];
        if (g && g.vendor === t.group) g.span += 1;
        else groups.push({ vendor: t.group, span: 1 });
      });

      const bodyHtml = rows.length ? rows.map((sk, ri) => {
        const badges = [`<span class="badge">~${fmtTok(sk.approxTokens)} tok</span>`];
        if (sk.hasScripts) badges.push('<span class="badge">scripts/</span>');
        if (sk.hasRefs) badges.push('<span class="badge">references/</span>');
        if (sk.isRepoLocal) badges.push(`<span class="badge sk trunc" title="${esc(sk.repoRoot)}">repo ${esc(sk.repoRoot)}</span>`);
        if (!sk.antigravityValid) badges.push(`<span class="badge ro btnish" data-action="inspect" data-slug="${esc(sk.slug)}">\u26a0 Frontmatter</span>`);
        if (sk.drifted) badges.push(`<span class="badge am btnish" data-action="diff" data-slug="${esc(sk.slug)}">\u21c4 drifted</span>`);
        if (sk.duplicateCount > 0 || sk.hasPhysicalCopy) {
          badges.push(`<span class="badge am btnish" data-action="diff" data-slug="${esc(sk.slug)}">${sk.duplicateCount > 0 ? `${sk.duplicateCount + 1} exact duplicates` : 'unmanaged copy'}</span>`);
        }
        if (sk.hasBroken) badges.push('<span class="badge ro">broken link</span>');
        if (sk.hasShadow) badges.push('<span class="badge sk">plugin shadow</span>');

        const cursorRow = ri === this.state.cursor.r;
        const cells = targets.map((t, ci) => {
          const st = sk.cells[t.id]?.state || 'off';
          const cur = cursorRow && ci === this.state.cursor.c;
          const busy = this.state.inFlight.has(`${sk.slug}::${t.id}`);
          return `
            <td class="cell ${ci === this.state.cursor.c ? 'cursor-col' : ''}">
              <button class="jack ${cur ? 'cur' : ''}" data-s="${st}" data-action="cell" data-slug="${esc(sk.slug)}" data-t="${esc(t.id)}" ${busy ? 'disabled' : ''} aria-label="${esc(sk.slug)} in ${esc(t.displayName)}: ${STATE_LABEL[st]}">
                <span class="j">${STATE_GLYPH[st]}</span>
                <span class="lbl">${STATE_LABEL[st]}</span>
              </button>
            </td>
          `;
        }).join('');

        return `
          <tr class="${cursorRow ? 'cursor-row' : ''} ${this.state.selected === sk.slug ? 'selected' : ''}">
            <td class="skill-col skill-cell" data-action="inspect" data-slug="${esc(sk.slug)}">
              <div class="row1"><span class="slug" title="${esc(sk.slug)}">${esc(sk.slug)}</span></div>
              <div class="desc" title="${esc(sk.description)}">${esc(sk.description || sk.canonicalPath || 'No description')}</div>
              <div class="badges">${badges.join('')}</div>
            </td>
            ${cells}
            <td></td>
          </tr>
        `;
      }).join('') : `
        <tr class="empty-row">
          <td colspan="${targets.length + 2}">
            ${c.skills.length === 0
              ? 'No skills discovered yet. <button class="btn sm primary" data-action="scan">Scan laptop</button>'
              : 'No skills match. <button class="btn sm" data-action="clear-filters">Clear filters</button>'}
          </td>
        </tr>
      `;

      return `
        <table class="matrix" aria-label="Skill wiring matrix">
          <thead>
            <tr class="groups">
              <th class="skill-col"></th>
              ${groups.map((g) => `<th class="g ${g.vendor === 'Claude' ? 'claude' : ''} ${g.span > 1 ? 'multi' : ''}" colspan="${g.span}"><span>${esc(g.vendor)}${g.span > 1 ? ` · ${g.span} accounts` : ''}</span></th>`).join('')}
              <th></th>
            </tr>
            <tr class="cols">
              <th class="skill-col" style="text-align:left;padding:8px 12px;vertical-align:bottom">
                <span class="section-label" style="margin:0">${esc(c.canonicalRoot)} · ${c.skills.length} skills</span>
              </th>
              ${targets.map((t, ci) => {
                const hot = t.approxTokens > BUDGET_HOT;
                const pct = Math.min(100, Math.round((t.approxTokens / BUDGET_SCALE) * 100));
                const shortDir = t.skillsDir.replace(/\/skills$/, '');
                const tip = [t.skillsDir, t.envHint ? `Env: ${t.envHint}` : '', `Adapter: ${t.adapter}`].filter(Boolean).join('\n');
                return `
                  <th class="colhead ${ci === this.state.cursor.c ? 'cursor' : ''}" scope="col" title="${esc(tip)}">
                    <span class="name"><span class="glyph">${esc(t.glyph)}</span><span style="overflow:hidden;text-overflow:ellipsis">${esc(t.displayName)}</span></span>
                    <span class="path">${esc(shortDir)}${t.strict ? ' · strict' : ''}</span>
                    <div class="meter ${hot ? 'hot' : ''}"><i style="width:${pct}%"></i></div>
                    <div class="budget ${hot ? 'hot' : ''}">${t.activeCount} active · ~${fmtTok(t.approxTokens)} tok${hot ? ' \u26a0' : ''}</div>
                    <button class="menu-btn" data-action="colmenu" data-t="${esc(t.id)}" aria-label="${esc(t.displayName)} menu">\u22ef</button>
                  </th>
                `;
              }).join('')}
              <th class="addcol"><button data-action="add-target">+ Add Target</button></th>
            </tr>
          </thead>
          <tbody>${bodyHtml}</tbody>
        </table>
      `;
    }

    // ── Wizard Template (Artboard 1) ────────────────────────────────────────
    _tplWizard() {
      const w = this.state.wizard;
      const steps = ['Scan summary', 'Dry-run plan', 'Apply & receipt'];
      return `
        <div class="wiz">
          <div class="steps">
            ${steps.map((label, idx) => `
              <button class="step ${w.step > idx + 1 ? 'done' : ''}" data-action="wiz-step" data-step="${idx + 1}" aria-current="${w.step === idx + 1}">
                <i>${w.step > idx + 1 ? '\u2713' : idx + 1}</i>${label}
              </button>
            `).join('')}
          </div>
          ${w.step === 1 ? this._tplWizScan() : w.step === 2 ? this._tplWizPlan() : this._tplWizApply()}
        </div>
      `;
    }

    _tplWizScan() {
      const c = this.state.catalog;
      const st = c.stats;
      const repos = new Map();
      for (const sk of c.skills) {
        if (sk.isRepoLocal && sk.repoRoot) {
          repos.set(sk.repoRoot, (repos.get(sk.repoRoot) || 0) + 1);
        }
      }
      const allRoots = [...c.targets, ...c.unconfiguredSiblings];

      return `
        <div class="card scanline">
          <div class="big">${fmtScanMs(c.lastScanMs)}<small> ms</small></div>
          <div>
            <div style="font-weight:600">${st.dirsScanned ? `Scanned ${st.dirsScanned.toLocaleString()} directories` : `Scanned ${allRoots.length} targets & ${repos.size} repos`} · macOS TCC-safe</div>
            <div class="mono faint">skipped ~/Pictures ~/Music ~/Movies ~/Library ~/.Trash · 0 permission prompts</div>
          </div>
          <div class="spacer"></div>
          <button class="btn" data-action="scan">Rescan</button>
          <button class="btn primary" data-action="wiz-step" data-step="2">Review consolidation plan \u2192</button>
        </div>

        <div class="stats">
          <div class="card stat"><div class="v">${st.uniqueSkills}</div><div class="l">unique skills</div><div class="s">${st.targetCount} targets · ${st.repoCount} git repos</div></div>
          <div class="card stat"><div class="v am">${st.exactDuplicates}</div><div class="l">exact duplicates</div><div class="s">${fmtBytes(st.duplicateBytes)} redundant copies</div></div>
          <div class="card stat"><div class="v am">${st.driftedCount}</div><div class="l">drifted copies</div><div class="s">content differs across tools</div></div>
          <div class="card stat"><div class="v ro">${st.brokenLinkCount}</div><div class="l">broken symlinks</div><div class="s">dangling targets</div></div>
          <div class="card stat"><div class="v ro">${st.frontmatterIssueCount}</div><div class="l">frontmatter warnings</div><div class="s">Antigravity drops these</div></div>
        </div>

        <div class="roots">
          <div class="card">
            <div style="padding:12px 14px 4px"><p class="section-label">Global roots</p></div>
            <div class="rootlist">
              ${allRoots.map((t) => {
                const badges = [];
                if (!t.configured) badges.push('<span class="badge am">auto-detected sibling</span>');
                if (t.strict) badges.push('<span class="badge ro">strict</span>');
                if (t.envHint) badges.push(`<span class="badge">${esc(t.envHint.split('=')[0])}</span>`);
                return `<div><span>${esc(t.skillsDir)} <span class="faint">· ${t.activeCount} active</span></span><span>${badges.join(' ')}</span></div>`;
              }).join('') || '<div class="faint">No global targets configured yet.</div>'}
            </div>
          </div>
          <div class="card">
            <div style="padding:12px 14px 4px"><p class="section-label">Project-local (git repos)</p></div>
            <div class="rootlist">
              ${Array.from(repos.entries()).map(([r, cnt]) => `<div><span>${esc(r)}</span><span class="faint">${cnt} skill${cnt === 1 ? '' : 's'}</span></div>`).join('') || '<div class="faint">No project-local repo skills discovered.</div>'}
            </div>
            <div style="padding:12px 14px" class="hint">Repo skills stay project-local by default. You choose per item in the next step.</div>
          </div>
        </div>
      `;
    }

    _tplWizPlan() {
      const w = this.state.wizard;
      const ops = this._extractPlanOps(w.plan);
      const count = (k) => ops.filter((o) => o.op === k).length;
      const repoSkills = this.state.catalog.skills.filter((s) => s.isRepoLocal);

      return `
        <div class="card safety">
          <span>dry run: nothing has changed yet</span>
          <span>superseded copies go to ~/.agents/quarantine/, never rm -rf</span>
          <span>loadout undo restores everything</span>
          <span>repo skills default to project-local</span>
        </div>
        <div style="height:12px"></div>
        ${w.error ? `<div class="warnbox"><p class="ro" style="margin:0">${esc(w.error)}</p></div>` : ''}
        <div class="card" style="overflow:auto">
          ${w.loading ? '<div style="padding:28px;text-align:center" class="mono faint">Computing dry-run plan...</div>' : `
            <table class="plan-table">
              <thead><tr><th style="width:104px">Op</th><th>Source</th><th>Destination</th><th>Note</th></tr></thead>
              <tbody>
                ${ops.length ? ops.map((p) => `
                  <tr>
                    <td><span class="opk ${p.op}">${esc(p.op)}</span></td>
                    <td class="p">${esc(p.src || p.slug)}</td>
                    <td class="p dim">${esc(p.dst)}</td>
                    <td class="faint" style="font-size:11.5px">${esc(p.note)}</td>
                  </tr>
                `).join('') : '<tr><td colspan="4" class="faint" style="padding:18px;text-align:center">All global skills are already canonicalized and wired via symlinks.</td></tr>'}
              </tbody>
            </table>
          `}
        </div>
        ${repoSkills.length ? `
          <div style="height:14px"></div>
          <div class="card" style="padding:12px 14px">
            <p class="section-label">Project-local skills found in git repos</p>
            ${repoSkills.map((r) => {
              const p = r.rawRepoPath || r.rawCanonicalPath || r.slug;
              const promoted = w.promoteSet.has(p);
              return `
                <div style="display:flex;align-items:center;gap:12px;padding:8px 0;border-bottom:1px solid var(--border);flex-wrap:wrap">
                  <div style="flex:1;min-width:240px">
                    <div class="mono" style="font-weight:600">${esc(r.slug)}</div>
                    <div class="mono faint">${esc(r.repoRoot)}</div>
                  </div>
                  <div class="seg" role="group" aria-label="${esc(r.slug)} placement">
                    <button data-action="repo-choice" data-path="${esc(p)}" data-v="keep" aria-pressed="${!promoted}">Keep Project-Local</button>
                    <button data-action="repo-choice" data-path="${esc(p)}" data-v="promote" aria-pressed="${promoted}">Promote to Global + Symlink</button>
                  </div>
                </div>
              `;
            }).join('')}
          </div>
        ` : ''}
        <div style="display:flex;gap:8px;align-items:center;margin-top:14px;flex-wrap:wrap">
          <span class="mono dim">${count('MOVE')} moves · ${count('LINK') + count('REPAIR')} symlinks · ${count('QUARANTINE')} quarantined · ${w.promoteSet.size} promoted · <span class="em">0 deletions</span></span>
          <div class="spacer"></div>
          <button class="btn" data-action="wiz-step" data-step="1">\u2190 Back</button>
          <button class="btn primary" data-action="wiz-apply" ${w.loading || (ops.length === 0 && w.promoteSet.size === 0) ? 'disabled' : ''}>Consolidate to ${esc(this.state.catalog.canonicalRoot)}</button>
        </div>
      `;
    }

    _tplWizApply() {
      const w = this.state.wizard;
      return `
        <div class="log" aria-live="polite">
          ${w.log.map((l) => `<div>${l}</div>`).join('')}
          ${w.applying ? '<div class="faint">\u258c</div>' : ''}
        </div>
        ${w.applied ? `
          <div class="card" style="padding:14px;margin-top:14px;display:flex;align-items:center;gap:12px;flex-wrap:wrap">
            <div style="flex:1">
              <div style="font-weight:600">Consolidation complete. Your laptop has one source of truth.</div>
              <div class="mono faint">quarantine ${esc(w.receipt || '~/.agents/quarantine/')} · run <span class="dim">loadout undo</span> to roll back</div>
            </div>
            ${this.state.catalog.canUndo ? '<button class="btn" data-action="undo">Undo consolidation</button>' : ''}
            <button class="btn primary" data-action="view" data-v="board">Open Switchboard \u2192</button>
          </div>
        ` : ''}
      `;
    }

    // ── Embedded Drawer Template (Artboard 6) ───────────────────────────────
    _tplEmbeddedDrawer() {
      const c = this.state.catalog;
      const scope = this.state.embeddedScope;
      const activeTid = this.getAttribute('active-target') || c.targets[0]?.id || '';
      const wtPath = this.getAttribute('worktree-path') || '';
      const repoName = this.getAttribute('repo-name') || '';

      const body = scope === 'global'
        ? this._tplEmbeddedMini(activeTid)
        : this._tplEmbeddedWorktree(wtPath, repoName);

      return `
        <aside class="h-drawer" aria-label="Skills drawer">
          <header>
            <div style="display:flex;align-items:center;gap:8px">
              <b>Skills</b>
              <span class="tagname">&lt;loadout-switchboard mode="embedded"&gt;</span>
              <span class="spacer"></span>
              <button class="btn sm ghost" data-action="scan" title="Rescan">\u21bb</button>
            </div>
            <div class="seg" role="group" aria-label="Embedded scope">
              <button data-action="emb-scope" data-v="global" aria-pressed="${scope === 'global'}">Global targets</button>
              <button data-action="emb-scope" data-v="worktree" aria-pressed="${scope === 'worktree'}">Current Git worktree</button>
            </div>
          </header>
          ${body}
          <div class="h-foot mono faint">
            <span class="em">\u25cf</span> synced · ${c.skills.length} skills
            <span class="spacer"></span>
            <button class="btn sm ghost" data-action="emb-open-full">Full switchboard \u2197</button>
          </div>
        </aside>
      `;
    }

    _tplEmbeddedMini(activeTid) {
      const c = this.state.catalog;
      const ts = c.targets;
      if (this.state.loading) {
        return '<div class="mini" style="padding:24px;text-align:center" class="mono faint">Loading catalog...</div>';
      }
      return `
        <div class="mini">
          <table>
            <thead>
              <tr>
                <th style="text-align:left;padding-left:10px">skill</th>
                ${ts.map((t) => `<th class="${t.id === activeTid ? 'active' : ''}" title="${esc(t.displayName)} (${esc(t.skillsDir)})">${esc(t.glyph)}</th>`).join('')}
              </tr>
            </thead>
            <tbody>
              ${c.skills.map((sk) => `
                <tr>
                  <td class="s" data-action="inspect" data-slug="${esc(sk.slug)}" title="${esc(sk.slug)}: ${esc(sk.description)}">${esc(sk.slug)}</td>
                  ${ts.map((t) => {
                    const st = sk.cells[t.id]?.state || 'off';
                    return `
                      <td class="${t.id === activeTid ? 'activecol' : ''}">
                        <button class="jack" data-s="${st}" data-action="cell" data-slug="${esc(sk.slug)}" data-t="${esc(t.id)}" aria-label="${esc(sk.slug)} in ${esc(t.displayName)}: ${STATE_LABEL[st]}">
                          <span class="j">${STATE_GLYPH[st]}</span>
                        </button>
                      </td>
                    `;
                  }).join('')}
                </tr>
              `).join('')}
            </tbody>
          </table>
        </div>
      `;
    }

    _tplEmbeddedWorktree(wtPath, repoName) {
      const repoSkills = this.state.catalog.skills.filter((s) => s.isRepoLocal && (!repoName || s.repoRoot.includes(repoName)));
      return `
        <div class="wt">
          <div class="mono faint" style="font-size:11px">${esc(shortenPath(wtPath) || 'Active thread worktree')}</div>
          ${repoSkills.map((s) => `
            <div class="wt-item">
              <div>
                <div class="mono" style="font-weight:600">${esc(s.slug)}</div>
                <div class="mono faint" style="font-size:10.5px">${esc(s.repoRoot)}</div>
              </div>
              <span class="badge em">hydrated</span>
            </div>
          `).join('')}
          ${!repoSkills.length ? '<div class="hint">No project-local skills in this repo. Global targets still apply.</div>' : ''}
          <div class="wt-item" style="grid-template-columns:1fr">
            <div class="mono" style="font-size:11px"><span class="em">\u2713</span> managed symlinks listed in <span class="dim">.git/info/exclude</span> · git status clean</div>
          </div>
          <div class="wt-item">
            <span style="font-size:12px">Hydrate on every new worktree</span>
            ${wtPath ? '<button class="btn sm primary" data-action="emb-hydrate">Hydrate now</button>' : '<span class="badge em">ON</span>'}
          </div>
        </div>
      `;
    }

    // ── Overlays & Popovers ─────────────────────────────────────────────────
    _renderOverlay() {
      const slot = this.querySelector('[data-slot="overlay"]');
      if (!slot) return;
      const ov = this.state.overlay;
      if (!ov) { slot.innerHTML = ''; return; }
      const scrim = '<div class="scrim" data-action="close"></div>';
      if (ov.type === 'diff') slot.innerHTML = scrim + this._tplDiffOverlay(ov);
      else if (ov.type === 'add') slot.innerHTML = scrim + this._tplAddOverlay(ov);
      else if (ov.type === 'inspect') slot.innerHTML = scrim + this._tplInspectorOverlay(ov);
      else slot.innerHTML = '';
    }

    _tplDiffOverlay(ov) {
      const sk = this.state.catalog.skills.find((s) => s.slug === ov.slug);
      if (!ov.isDrift) return this._tplDupeModal(ov, sk);

      const g = this.state.catalog.driftedGroups.find((x) => x.slug === ov.slug) || { slug: ov.slug, variants: [], diff: '' };
      const variants = g.variants || [];
      const diffText = g.diff || variants.find((v) => v.diffVsFirst)?.diffVsFirst || '';
      const parsed = parseUnifiedDiff(diffText);
      const labels = 'ABCDEFGHIJKLMNOPQRSTUVWXYZ';
      const instA = variants[0];
      const instB = variants[1];

      let bodyHtml = '';
      if (ov.tab === 'tree') {
        bodyHtml = `
          <div class="diff tree">
            <div class="tree-row h"><span>path</span><span>A · ${esc(instA?.targetId || 'primary')}</span><span>B · ${esc(instB?.targetId || 'secondary')}</span></div>
            <div class="tree-row"><span>SKILL.md</span><span class="am">${instA?.fileCount || 1} files · ${fmtBytes(instA?.totalBytes || 0)}</span><span class="am">${instB?.fileCount || 1} files · ${fmtBytes(instB?.totalBytes || 0)}</span></div>
            <div class="tree-row"><span>tree sha256</span><span class="faint">${esc((instA?.treeHash || '').slice(0, 12))}</span><span class="faint">${esc((instB?.treeHash || '').slice(0, 12))}</span></div>
          </div>
        `;
      } else if (ov.mode === 'split') {
        bodyHtml = renderSplitHtml(diffText, `A · ${instA?.path || ov.slug}/SKILL.md`, `B · ${instB?.path || ov.slug}/SKILL.md`);
      } else {
        bodyHtml = renderUnifiedHtml(diffText, `A -> B · ${ov.slug}/SKILL.md`);
      }

      const choices = [
        ['a', 'Keep A as canonical & wire all', `Move A to ${this.state.catalog.canonicalRoot}, quarantine others`],
        ['b', 'Keep B as canonical', 'Instance B wins; quarantine A & others'],
        ['both', 'Keep both', `Keep A as ${ov.slug}, save B as ${ov.keepBothSlug}`],
      ];

      const opRows = this._diffOpsPreview(ov, instA, instB);

      return `
        <aside class="drawer wide" role="dialog" aria-label="Resolve ${esc(ov.slug)}">
          <div class="ov-head">
            <span class="badge am">\u21c4 drifted</span>
            <h2 class="mono">${esc(ov.slug)}</h2>
            <span class="faint">${variants.length} instances</span>
            <button class="btn ghost x" data-action="close" aria-label="Close">Esc \u2715</button>
          </div>
          <div class="ov-body">
            <div class="instances">
              ${variants.map((v, idx) => `
                <div class="inst ${idx === 0 ? 'a' : idx === 1 ? 'b' : ''}">
                  <span class="tag">${labels[idx] || idx + 1}</span>
                  ${idx === 0 ? '<span class="badge em">primary</span>' : `<span class="badge am">${esc(v.targetId || 'variant')}</span>`}
                  <div class="p">${esc(v.path)}</div>
                  <div class="faint" style="font-size:11.5px">${v.mtimeEpoch ? `Modified ${fmtRelTime(v.mtimeEpoch)} · ` : ''}${v.fileCount} files · ${fmtBytes(v.totalBytes)}${v.treeHash ? ` · sha ${esc(v.treeHash.slice(0, 10))}` : ''}</div>
                </div>
              `).join('')}
            </div>

            <div class="diff-tabs">
              <div class="seg">
                <button data-action="diff-tab" data-v="skill" aria-pressed="${ov.tab === 'skill'}">SKILL.md</button>
                <button data-action="diff-tab" data-v="tree" aria-pressed="${ov.tab === 'tree'}">File tree</button>
              </div>
              <div class="seg">
                <button data-action="diff-mode" data-v="split" aria-pressed="${ov.mode === 'split'}">Side by side</button>
                <button data-action="diff-mode" data-v="unified" aria-pressed="${ov.mode === 'unified'}">Unified</button>
              </div>
              <span class="mono faint" style="margin-left:auto"><span class="em">+${parsed.adds}</span> <span class="ro">-${parsed.dels}</span></span>
            </div>
            ${bodyHtml}
          </div>

          <div class="ov-foot" style="display:block">
            <div class="choices" role="radiogroup" aria-label="Resolution">
              ${choices.map(([v, title, sub]) => `
                <button class="choice" role="radio" data-action="diff-choice" data-v="${v}" aria-pressed="${ov.choice === v}" aria-checked="${ov.choice === v}">
                  <b>${esc(title)}</b>
                  <span>${esc(sub)}</span>
                </button>
              `).join('')}
            </div>
            ${ov.choice === 'both' ? `
              <div style="display:flex;align-items:center;gap:8px;margin-top:10px">
                <label class="mono dim" for="keep-both-input">New slug for Instance B:</label>
                <input id="keep-both-input" class="input mono" style="width:240px;height:28px" data-input="diff.keepBothSlug" value="${esc(ov.keepBothSlug)}">
              </div>
            ` : ''}
            <div class="ops">${opRows}</div>
            <div style="display:flex;gap:8px;margin-top:10px;align-items:center">
              <span class="hint">Nothing is deleted. Undo restores every path from ~/.agents/quarantine/.</span>
              <div class="spacer"></div>
              <button class="btn" data-action="close">Cancel</button>
              <button class="btn primary" data-action="resolve" ${ov.busy ? 'disabled' : ''}>${ov.busy ? 'Applying...' : 'Apply resolution'}</button>
            </div>
          </div>
        </aside>
      `;
    }

    _diffOpsPreview(ov, a, b) {
      const root = this.state.catalog.canonicalRoot;
      const rows = ov.choice === 'b'
        ? [
          ['MOVE', `${b?.path || 'Instance B'} -> ${root}/${ov.slug}`],
          ['QUARANTINE', `${a?.path || 'Instance A'} -> ~/.agents/quarantine/<batch>/`],
          ['LINK', `wire symlinks -> ${root}/${ov.slug}`],
        ]
        : ov.choice === 'both'
          ? [
            ['MOVE', `${a?.path || 'Instance A'} -> ${root}/${ov.slug}`],
            ['MOVE', `${b?.path || 'Instance B'} -> ${root}/${ov.keepBothSlug}`],
            ['LINK', `wire both slugs cleanly across targets`],
          ]
          : [
            ['MOVE', `${a?.path || 'Instance A'} -> ${root}/${ov.slug}`],
            ['QUARANTINE', `${b?.path || 'Instance B'} -> ~/.agents/quarantine/<batch>/`],
            ['LINK', `wire symlinks -> ${root}/${ov.slug}`],
          ];
      const col = { MOVE: 'sk', QUARANTINE: 'am', LINK: 'em' };
      return rows.map(([k, v]) => `<div class="op"><span class="op-k ${col[k]}">${k}</span><span>${esc(v)}</span></div>`).join('');
    }

    _tplDupeModal(ov, sk) {
      const grp = this.state.catalog.duplicateGroups.find((g) => g.slug === ov.slug);
      const occs = grp?.occurrences || sk?.occurrences || [];
      return `
        <div class="modal" role="dialog" aria-label="Consolidate ${esc(ov.slug)}">
          <div class="ov-head">
            <span class="badge am">${occs.length || 2} physical copies</span>
            <h2 class="mono">${esc(ov.slug)}</h2>
            <button class="btn ghost x" data-action="close">Esc \u2715</button>
          </div>
          <div class="ov-body">
            <p class="dim" style="margin-top:0">Copies match byte-for-byte (${grp?.treeHash ? `SHA-256 <span class="mono">${esc(grp.treeHash.slice(0, 14))}</span>, ` : ''}${fmtBytes(grp?.totalBytes || sk?.totalBytes || 0)}). Consolidation moves one copy to <span class="mono">${esc(this.state.catalog.canonicalRoot)}/${esc(ov.slug)}</span>, quarantines redundant copies, and wires symlinks.</p>
            <div class="linkmap">
              ${occs.map((o) => `<div><span class="am">\u25a3</span><span>${esc(o.path)}</span><span class="faint">${esc(o.targetId || (o.isRepoLocal ? 'repo' : 'copy'))}</span></div>`).join('') || '<div><span class="am">\u25a3</span><span>Unmanaged physical copy detected</span><span class="faint">ready</span></div>'}
            </div>
          </div>
          <div class="ov-foot">
            <span class="hint">Reversible with loadout undo.</span>
            <div class="spacer"></div>
            <button class="btn" data-action="close">Cancel</button>
            <button class="btn primary" data-action="resolve" ${ov.busy ? 'disabled' : ''}>${ov.busy ? 'Consolidating...' : 'Consolidate & wire all'}</button>
          </div>
        </div>
      `;
    }

    _tplAddOps(a) {
      const cleanPath = (a.path || '').trim();
      const target = this.state.catalog.targets.find((t) => t.id === a.cloneFrom);
      const nOn = target?.activeCount || 0;
      const cmds = cleanPath ? [
        `mkdir -p ${cleanPath}`,
        a.initial === 'clone' && target
          ? `ln -s ${this.state.catalog.canonicalRoot}/<slug> ${cleanPath}/  x ${nOn} (cloned from ${target.displayName})`
          : a.initial === 'profile'
            ? `ln -s ${this.state.catalog.canonicalRoot}/<slug> ${cleanPath}/  (active profile)`
            : '# start empty: no symlinks created yet',
        `loadout target add ${slugify(a.id || a.name) || '<id>'} "${cleanPath}" --kind ${a.adapter}${a.env ? ` --env "${a.env}"` : ''}`,
      ] : ['# fill in a skills directory path to preview filesystem changes'];
      return cmds.map((c) => `<div class="${c.startsWith('#') ? 'faint' : ''}">${c.startsWith('#') ? '' : '<span class="faint">$ </span>'}${esc(c)}</div>`).join('');
    }

    _tplAddOverlay(a) {
      const sibs = this.state.catalog.unconfiguredSiblings;
      const targets = this.state.catalog.targets;
      const meta = ADAPTERS.find((x) => x.id === a.adapter) || ADAPTERS[0];
      const cloneTarget = targets.find((t) => t.id === a.cloneFrom);

      return `
        <div class="modal" role="dialog" aria-label="Add target or account">
          <div class="ov-head">
            <h2>Add Target / Account</h2>
            <span class="faint">a target is any folder an agent reads skills from</span>
            <button class="btn ghost x" data-action="close">Esc \u2715</button>
          </div>
          <div class="ov-body">
            <p class="section-label">Detected on disk, not yet wired</p>
            <div class="linkmap" style="margin-bottom:6px">
              ${sibs.map((d) => `
                <div style="grid-template-columns:20px 1fr auto">
                  <span class="am">\u25cf</span>
                  <span><b style="font-family:var(--sans)">${esc(d.displayName)}</b> <span class="dim">${esc(d.skillsDir)}</span>${d.envHint ? `<br><span class="faint" style="font-size:10.5px">${esc(d.envHint)}</span>` : ''}</span>
                  <button class="btn sm primary" data-action="add-detected" data-id="${esc(d.id)}">Add in 1 click</button>
                </div>
              `).join('') || '<div><span></span><span class="faint">No unconfigured sibling accounts detected on disk.</span><span></span></div>'}
            </div>

            <div style="height:14px"></div>
            <p class="section-label">Custom path</p>
            <div style="display:grid;grid-template-columns:1fr 1fr;gap:12px">
              <div class="field">
                <label for="add-name">Display name</label>
                <input id="add-name" class="input" placeholder="Claude (Second)" value="${esc(a.name)}" data-input="add.name">
              </div>
              <div class="field">
                <label for="add-id">Target ID</label>
                <input id="add-id" class="input mono" placeholder="claude-second" value="${esc(a.id)}" data-input="add.id">
              </div>
              <div class="field" style="grid-column:1 / -1">
                <label for="add-path">Skills directory path</label>
                <input id="add-path" class="input mono" placeholder="${esc(meta.defaultDir)}" value="${esc(a.path)}" data-input="add.path">
                <span class="hint">Missing parent folders are created automatically (mkdir -p).</span>
              </div>
            </div>

            <div class="field" style="margin-top:12px">
              <label>Adapter rules</label>
              <div class="seg" style="flex-wrap:wrap" role="group">
                ${ADAPTERS.map((x) => `<button data-action="add-adapter" data-v="${x.id}" aria-pressed="${a.adapter === x.id}">${esc(x.label)}</button>`).join('')}
              </div>
              <span class="hint">${meta.strict ? 'Strict: requires YAML name + description and name == folder slug. Non-compliant skills show a rose jack.' : 'Lenient: accepts any SKILL.md.'}</span>
            </div>

            <div class="field" style="margin-top:12px">
              <label for="add-env">Environment variable hint <span class="faint">(optional, used by T3 Code to map threads to this target)</span></label>
              <input id="add-env" class="input mono" placeholder="${esc(meta.defaultEnv || 'CLAUDE_CONFIG_DIR=~/.claude-second')}" value="${esc(a.env)}" data-input="add.env">
            </div>

            <div class="field" style="margin-top:12px">
              <label>Initial wiring</label>
              <div style="display:flex;gap:8px;align-items:center;flex-wrap:wrap">
                <div class="seg" role="group">
                  <button data-action="add-initial" data-v="empty" aria-pressed="${a.initial === 'empty'}">Start empty</button>
                  <button data-action="add-initial" data-v="clone" aria-pressed="${a.initial === 'clone'}">Clone active skills from</button>
                  <button data-action="add-initial" data-v="profile" aria-pressed="${a.initial === 'profile'}">Enable active profile</button>
                </div>
                ${a.initial === 'clone' && targets.length ? `
                  <select class="select" data-change="add.cloneFrom" aria-label="Clone from">
                    ${targets.map((t) => `<option value="${esc(t.id)}" ${a.cloneFrom === t.id ? 'selected' : ''}>${esc(t.displayName)}</option>`).join('')}
                  </select>
                  <span class="mono faint">${cloneTarget?.activeCount || 0} skills</span>
                ` : ''}
              </div>
            </div>

            <div class="ops" style="margin-top:14px" data-slot="add-ops">${this._tplAddOps(a)}</div>
            ${a.error ? `<p class="ro" style="margin:10px 0 0">${esc(a.error)}</p>` : ''}
          </div>
          <div class="ov-foot">
            <span class="hint">New column appears in the matrix next to its vendor group.</span>
            <div class="spacer"></div>
            <button class="btn" data-action="close">Cancel</button>
            <button class="btn primary" data-action="add-submit" ${a.busy ? 'disabled' : ''}>${a.busy ? 'Adding...' : 'Add target'}</button>
          </div>
        </div>
      `;
    }

    _tplInspectorOverlay(ins) {
      const sk = this.state.catalog.skills.find((s) => s.slug === ins.slug) || {
        slug: ins.slug,
        description: '',
        canonicalPath: '',
        approxTokens: 0,
        antigravityValid: true,
        frontmatterIssues: [],
        cells: {},
        treeHash: '',
        fileCount: 1,
        totalBytes: 0,
      };
      const files = ins.files.length ? ins.files : [{ path: 'SKILL.md', isText: true, content: '' }];
      const curFile = files.find((f) => f.path === ins.activeFile) || files[0];
      const fmDiff = ins.fmPreview?.diff || ins.fmPreview?.unifiedDiff || ins.fmPreview?.unified_diff || '';

      const compatHtml = this.state.catalog.targets.map((t) => {
        const st = sk.cells[t.id]?.state || 'off';
        const bad = !sk.antigravityValid && t.strict;
        return `
          <div title="${esc(t.adapter)}">
            <span class="${bad ? 'ro' : 'em'}">${bad ? '\u2717' : '\u2713'}</span>
            <span style="flex:1;overflow:hidden;text-overflow:ellipsis;white-space:nowrap">${esc(t.displayName)}</span>
            <span class="faint mono" style="font-size:10px">${bad ? 'dropped' : STATE_LABEL[st]}</span>
          </div>
        `;
      }).join('');

      const linkRows = this.state.catalog.targets.map((t) => {
        const cell = sk.cells[t.id] || { state: 'off' };
        const st = cell.state;
        const p = `${t.skillsDir}/${sk.slug}`;
        const canon = sk.canonicalPath || `${this.state.catalog.canonicalRoot}/${sk.slug}`;
        const map = {
          on: ['<span class="em">\u25cf</span>', `${esc(p)} <span class="arrow">\u2192</span> ${esc(cell.occurrence?.realPath || canon)}`, '<span class="em">readlink \u2713</span>'],
          off: ['<span class="faint">\u25cb</span>', `<span class="faint">${esc(p)}</span>`, '<span class="faint">not linked</span>'],
          copy: ['<span class="am">\u25a3</span>', `${esc(cell.occurrence?.path || p)} <span class="faint">(physical folder)</span>`, '<span class="am">unmanaged</span>'],
          drift: ['<span class="am">\u21c4</span>', `${esc(cell.occurrence?.path || p)} <span class="faint">(physical, differs)</span>`, '<span class="am">drifted</span>'],
          fm: ['<span class="ro">!</span>', `${esc(p)} <span class="arrow">\u2192</span> ${esc(canon)}`, `<span class="ro">linked · ignored by ${esc(t.displayName)}</span>`],
          shadow: ['<span class="sk">P</span>', `${esc(p)} <span class="faint">shadowed by plugin</span>`, '<span class="sk">plugin wins</span>'],
          broken: ['<span class="ro">\u00d7</span>', `${esc(p)} <span class="arrow">\u2192</span> ${esc(cell.broken?.danglingTarget || 'missing')}`, '<span class="ro">dangling</span>'],
        }[st] || ['<span class="faint">\u25cb</span>', esc(p), '<span class="faint">not linked</span>'];
        return `<div>${map[0]}<span>${map[1]}</span><span>${map[2]}</span></div>`;
      }).join('');

      const fixBox = !sk.antigravityValid ? `
        <div class="warnbox">
          <h4 class="ro">Gemini Antigravity will ignore this skill</h4>
          <p>${esc(sk.frontmatterIssues.join('; ') || 'name and description are missing or name != folder slug in YAML frontmatter. Strict adapters drop it silently.')}</p>
          ${ins.showFix ? (fmDiff ? `<div style="margin-bottom:10px">${renderUnifiedHtml(fmDiff, `${sk.canonicalPath || sk.slug}/SKILL.md · proposed`)}</div>` : '<div class="hint" style="margin-bottom:8px">Loading frontmatter diff preview...</div>') : ''}
          <div style="display:flex;gap:8px;align-items:center;flex-wrap:wrap">
            <button class="btn sm" data-action="toggle-fix">${ins.showFix ? 'Hide preview' : 'Preview fix'}</button>
            <button class="btn sm primary" data-action="apply-fix" data-slug="${esc(sk.slug)}" ${ins.fmBusy ? 'disabled' : ''}>${ins.fmBusy ? 'Fixing...' : 'Fix frontmatter'}</button>
            <span class="hint">non-destructive · body preserved</span>
          </div>
        </div>
      ` : `
        <div class="warnbox ok">
          <h4 class="em">Frontmatter valid for all adapters</h4>
          <p style="margin:0">name == folder slug · description present · ${sk.description.length} chars</p>
        </div>
      `;

      // Group file list by top-level directory for clean nav
      let lastDir = '';
      const navHtml = files.map((f) => {
        const slash = f.path.indexOf('/');
        const dir = slash >= 0 ? f.path.slice(0, slash + 1) : '';
        const dirHeader = dir && dir !== lastDir ? `<div class="dir">${esc(dir)}</div>` : '';
        lastDir = dir;
        const label = slash >= 0 ? `&nbsp;&nbsp;${esc(f.path.slice(slash + 1))}` : esc(f.path);
        return `${dirHeader}<button data-action="insp-file" data-f="${esc(f.path)}" aria-current="${f.path === curFile.path}">${label}</button>`;
      }).join('');

      return `
        <aside class="drawer" role="dialog" aria-label="Inspect ${esc(sk.slug)}">
          <div class="ov-head">
            <h2 class="mono">${esc(sk.slug)}</h2>
            <span class="badge">~${fmtTok(sk.approxTokens)} tok</span>
            ${sk.isRepoLocal ? `<span class="badge sk trunc" title="${esc(sk.repoRoot)}">repo ${esc(sk.repoRoot)}</span>` : ''}
            <button class="btn ghost x" data-action="close">Esc \u2715</button>
          </div>
          <div class="ov-body">
            ${ins.loading ? '<div style="padding:32px;text-align:center" class="mono faint">Loading skill bundle...</div>' : `
              <p class="dim" style="margin-top:0">${esc(sk.description || 'No description')}</p>
              <div class="insp-sec">
                <p class="section-label">Compatibility across adapters</p>
                <div class="compat">${compatHtml}</div>
                ${fixBox}
              </div>
              <div class="insp-sec">
                <p class="section-label">Symlink map · live readlink</p>
                <div class="linkmap">${linkRows}</div>
              </div>
              <div class="insp-sec">
                <p class="section-label">Files</p>
                <div class="files">
                  <nav aria-label="Files">${navHtml}</nav>
                  <div class="viewer">
                    <div class="vbar">
                      <span>${esc(sk.canonicalPath || `${this.state.catalog.canonicalRoot}/${sk.slug}`)}/${esc(curFile.path)}</span>
                      <span class="spacer"></span>
                      ${curFile.isText ? (ins.editing ? `
                        <button class="btn sm" data-action="insp-cancel">Cancel</button>
                        <button class="btn sm primary" data-action="insp-save" ${ins.saving ? 'disabled' : ''}>${ins.saving ? 'Saving...' : 'Save'}</button>
                      ` : '<button class="btn sm" data-action="insp-edit">Edit</button>') : '<span class="badge">binary</span>'}
                    </div>
                    ${!curFile.isText
                      ? `<pre class="faint">Binary file (${fmtBytes(curFile.size)})</pre>`
                      : (ins.editing
                        ? `<textarea id="insp-editor" aria-label="Edit ${esc(curFile.path)}" spellcheck="false">${esc(curFile.content)}</textarea>`
                        : `<pre>${esc(curFile.content || '')}</pre>`)}
                  </div>
                </div>
              </div>
              <div class="insp-sec">
                <p class="section-label">Provenance</p>
                <dl class="kv">
                  <dt>realpath</dt><dd>${esc(ins.detail?.canonicalPath || sk.canonicalPath || 'unmanaged')}</dd>
                  <dt>sha256 tree</dt><dd>${esc(sk.treeHash || ins.detail?.treeHash || '-')}</dd>
                  <dt>size</dt><dd>${files.length} files · ${fmtBytes(sk.totalBytes)}</dd>
                </dl>
              </div>
            `}
          </div>
        </aside>
      `;
    }

    _renderMenu() {
      const slot = this.querySelector('[data-slot="menu"]');
      if (!slot) return;
      if (!this.state.menu) { slot.innerHTML = ''; return; }
      const t = this.state.catalog.targets.find((x) => x.id === this.state.menu.t);
      if (!t) { slot.innerHTML = ''; return; }
      const others = this.state.catalog.targets.filter((o) => o.id !== t.id);

      slot.innerHTML = `
        <div class="menu" style="left:${this.state.menu.x}px;top:${this.state.menu.y}px" role="menu">
          <button data-action="col-all" data-t="${esc(t.id)}" data-on="1">Enable all</button>
          <button data-action="col-all" data-t="${esc(t.id)}" data-on="0">Disable all</button>
          ${others.length ? `
            <hr>
            <div class="faint" style="font-size:11px;padding:4px 10px">Clone wiring from...</div>
            ${others.map((o) => `<button class="sub" data-action="col-clone" data-from="${esc(o.id)}" data-t="${esc(t.id)}">${esc(o.displayName)}</button>`).join('')}
          ` : ''}
          <hr>
          <button data-action="col-copy-env" data-t="${esc(t.id)}">${t.envHint ? 'Copy env hint' : 'Copy path'} <span class="faint mono" style="margin-left:auto">${esc(t.skillsDir)}</span></button>
          <button data-action="col-remove" data-t="${esc(t.id)}" class="danger">Remove target...</button>
        </div>
      `;
    }

    _renderHelp() {
      const slot = this.querySelector('[data-slot="help"]');
      if (!slot) return;
      if (!this.state.help) { slot.innerHTML = ''; return; }
      const rows = [
        ['/', 'search skills'],
        ['j / k', 'move row'],
        ['h / l', 'move target'],
        ['Space', 'toggle wire / repair'],
        ['Enter', 'open inspector'],
        ['d', 'diff / resolve'],
        ['u', 'undo last action'],
        ['?', 'toggle keys'],
        ['Esc', 'close drawer / modal'],
      ];
      slot.innerHTML = `
        <div class="card help" role="dialog" aria-label="Keyboard shortcuts">
          <p class="section-label">Keyboard</p>
          ${rows.map(([k, v]) => `<div><span>${v}</span><kbd>${k}</kbd></div>`).join('')}
        </div>
      `;
    }

    _renderToast() {
      const slot = this.querySelector('[data-slot="toast"]');
      if (!slot) return;
      const t = this.state.toast;
      if (!t) { slot.innerHTML = ''; return; }
      slot.innerHTML = `
        <div class="toast" role="status">
          <div>
            <div>${esc(t.text)}</div>
            ${t.mono ? `<div class="mono">${esc(t.mono)}</div>` : ''}
          </div>
          ${t.undo ? '<button data-action="undo">Undo <span style="opacity:.6">u</span></button>' : ''}
        </div>
      `;
    }
  }

  if (!customElements.get('loadout-switchboard')) {
    customElements.define('loadout-switchboard', LoadoutSwitchboard);
  }
})();
)LOADOUT_JS";

}  // namespace loadout::ui

#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
