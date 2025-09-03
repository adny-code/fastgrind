#!/usr/bin/env python3
"""MERecorder post-processing tool.

Purpose
-------
1. Parse a profiling output JSON file (``merecorder.json`` by default).
2. Build an aggregated structure: ``Dict[int, Dict[str, List[str]]]`` where
         each key is a thread id and each nested key is a function name discovered
         anywhere in that thread across all time slices. The value is a static list
         placeholder ``["malloc", "free"]`` representing the available metrics.
         (The script does NOT currently sum or aggregate numeric values.)
3. Generate an interactive HTML page (Plotly) that lets a user select
         threads, functions, and metrics then plots per-tick sums of the selected
         metric for nodes whose name matches the chosen function(s).

CLI Usage
---------
                python merecorder.py [path/to/merecorder.json]

If the argument is omitted, the script looks for ``merecorder.json`` in the
current working directory. The resulting HTML filename is derived by replacing
``.json`` with ``.html`` (or appending ``.html`` if no ``.json`` extension is
present). The HTML file is written next to the input file.

Input JSON Layout (simplified)
------------------------------
{
    "0": {                 			# tick / time slice
        "1835879": {         		# thread id (string form)
            "name": "functionA",	# function name
            "malloc": 8,
            "free": 8,
            "children": [ ... recursive nodes ... ]
        }
    },
    "500": { ... }
}

Aggregation Semantics
---------------------
All ticks are scanned. For every thread id we collect the *set* of unique
function names found in that thread's tree(s). We do not perform numeric
aggregation here; the HTML plotting phase walks the raw tree each time you
click "Plot" and sums values for the selected metric(s) and function name(s)
per tick.
"""

from __future__ import annotations

import json
import sys
from pathlib import Path
from typing import Dict, List, Any

AggType = Dict[int, Dict[str, List[str]]]


def _walk_node(node: Any, names: set):
    """Recursive traversal collecting function names only."""
    if not isinstance(node, dict):
        return
    name = node.get("name")
    if name is not None:
        names.add(name)
    for child in node.get("children", []) or []:
        _walk_node(child, names)


def load_json(path: str | Path):
    return json.loads(Path(path).read_text(encoding="utf-8"))


def build_structure(path: str | Path) -> AggType:
    """Build aggregated structure from a JSON file path."""
    data = load_json(path)
    return _build_structure_from_loaded(data)


def _build_structure_from_loaded(data: Any) -> AggType:
    """Internal helper: same logic as ``build_structure`` but the input is
    already a parsed Python object (dict). Returns the aggregated mapping.
    """
    if not isinstance(data, dict):
        return {}
    result: AggType = {}
    for _slice, threads in data.items():
        if not isinstance(threads, dict):
            continue
        for tid_str, root in threads.items():
            try:
                tid = int(tid_str)
            except (TypeError, ValueError):
                continue
            per_thread = result.setdefault(tid, {})
            names: set = set()
            _walk_node(root, names)
            for fname in names:
                per_thread.setdefault(fname, ["malloc", "free"])
    return result


def _collect_names(data) -> tuple[list[str], list[str]]:
    """Collect thread ids (as strings) and function names across all ticks."""
    threads_set = set()
    func_set = set()

    def walk(node):
        if isinstance(node, dict):
            n = node.get("name")
            if n is not None:
                func_set.add(n)
            for c in node.get("children", []) or []:
                walk(c)

    for _tick, threads in data.items():
        if not isinstance(threads, dict):
            continue
        for tid, root in threads.items():
            threads_set.add(str(tid))
            walk(root)
    return sorted(threads_set, key=lambda x: int(x)), sorted(func_set)


def generate_html(data, output: str, agg_struct: AggType | None = None):
    thread_ids, func_names = _collect_names(data)
    metrics = ["malloc", "free"]
    embedded = json.dumps(data)
    if agg_struct is None:
        agg_struct = _build_structure_from_loaded(data)
    agg_struct_json = json.dumps(agg_struct)

    html = f"""<!DOCTYPE html>
	<html lang=\"en\">
	<head>
	       <meta charset=\"UTF-8\" />
	       <title>MERecorder Memory Plot</title>
	       <script src=\"https://cdn.plot.ly/plotly-2.30.0.min.js\"></script>
	       <style>
		       body {{ font-family: Arial, sans-serif; margin: 12px; }}
		       .selectors {{ display: flex; gap: 24px; margin-bottom: 8px; flex-wrap: wrap; }}
		       .selectors div {{ display: flex; flex-direction: column; font-size: 14px; }}
		       select {{ min-width: 180px; min-height: 160px; }}
		       #plot {{ width: 100%; height: 640px; }}
		       button {{ padding: 6px 16px; font-size: 14px; cursor: pointer; }}
		       .hint {{ font-size: 12px; color: #555; margin-top: 4px; }}
	       </style>
	</head>
	<body>
	       <h2>MERecorder Memory Timeline (Show Thread::Functions allocation & deallocation by ticks)</h2>
	       <div class=\"selectors\">
		       <div>
			       <label>Threads (multi-select)</label>
			       <select id=\"threadSel\" multiple>
				       {''.join(f'<option value="{tid}">{tid}</option>' for tid in thread_ids)}
			       </select>
		       </div>
		       <div>
			       <label>Functions (multi-select)</label>
			       <select id=\"funcSel\" multiple>
				       {''.join(f'<option value="{fn}">{fn}</option>' for fn in func_names)}
			       </select>
		       </div>
		       <div>
			       <label>Metrics (multi-select)</label>
			       <select id=\"metricSel\" multiple>
				       {''.join(f'<option value="{m}">{m}</option>' for m in metrics)}
			       </select>
		       </div>
		       <div style=\"align-self:flex-end;\">
			       <button id=\"plotBtn\">Plot</button>
			       <div class=\"hint\">Hold Ctrl/Shift for multi-select</div>
		       </div>
	       </div>
	       <div id=\"plot\"></div>
	       <script>
		       const rawData = {embedded};
		       const aggStruct = {agg_struct_json};

		       function collect(selectedThreads, selectedFuncs, selectedMetrics) {{
			       // ticks sorted numerically
			       const ticks = Object.keys(rawData).map(t=>parseInt(t,10)).sort((a,b)=>a-b);
			       const traces = [];
			       selectedThreads.forEach(tid => {{
				       selectedFuncs.forEach(fn => {{
					       selectedMetrics.forEach(metric => {{
						       const y = [];
						       ticks.forEach(tk => {{
							       const threadObj = rawData[tk] && rawData[tk][tid];
							       let val = 0;
							       if(threadObj) {{
								       // traverse to sum all nodes with name==fn
								       const stack = [threadObj];
								       while(stack.length) {{
									       const n = stack.pop();
									       if(n && typeof n === 'object') {{
										       if(n.name === fn && typeof n[metric] === 'number') val += n[metric];
										       if(Array.isArray(n.children)) n.children.forEach(c=>stack.push(c));
									       }}
								       }}
							       }}
							       y.push(val);
						       }});
						       traces.push({{
							       x: ticks,
							       y: y,
							       mode: 'lines+markers',
							       name: `T${{tid}}::${{fn}}::${{metric}}`,
							       hovertemplate: 'tick=%{{x}}<br>value=%{{y}}<extra>T'+tid+' '+fn+' '+metric+'</extra>'
						       }});
					       }});
				       }});
			       }});
			       return traces;
		       }}

		       function getSelectedValues(sel) {{
			       return Array.from(sel.selectedOptions).map(o=>o.value);
		       }}

		       // Dynamically refresh the Functions multi-select when thread selection changes
		       function updateFuncOptions() {{
			       const tSel = document.getElementById('threadSel');
			       const fSel = document.getElementById('funcSel');
			       const selectedThreads = getSelectedValues(tSel);
			       let funcSet = new Set();
			       if(selectedThreads.length === 0) {{
				       // No thread filter selected: show every function from all threads
				       Object.values(aggStruct).forEach(threadDict => {{
					       Object.keys(threadDict).forEach(fn => funcSet.add(fn));
				       }});
			       }} else {{
				       selectedThreads.forEach(tid => {{
					       const threadDict = aggStruct[tid];
					       if(threadDict) {{
						       Object.keys(threadDict).forEach(fn => funcSet.add(fn));
					       }}
				       }});
			       }}
			       // Keep original options
			       const prevSelected = getSelectedValues(fSel);
			       fSel.innerHTML = '';
			       Array.from(funcSet).sort().forEach(fn => {{
				       const opt = document.createElement('option');
				       opt.value = fn;
				       opt.textContent = fn;
				       if(prevSelected.includes(fn)) opt.selected = true;
				       fSel.appendChild(opt);
			       }});
		       }}

		       document.getElementById('threadSel').addEventListener('change', updateFuncOptions);

		       document.getElementById('plotBtn').addEventListener('click', () => {{
			       const tSel = document.getElementById('threadSel');
			       const fSel = document.getElementById('funcSel');
			       const mSel = document.getElementById('metricSel');
			       const threads = getSelectedValues(tSel);
			       const funcs = getSelectedValues(fSel);
			       const metrics = getSelectedValues(mSel);
			       if(!threads.length || !funcs.length || !metrics.length) {{
				       alert('Select at least one item in each list');
				       return;
			       }}
			       const traces = collect(threads, funcs, metrics);
			       const layout = {{
				       title: 'Memory vs Tick',
				       xaxis: {{ title: 'Tick' }},
				       yaxis: {{ title: 'Memory Size' }},
				       legend: {{ orientation: 'h', y: -0.2 }},
				       hovermode: 'closest'
			       }};
			       Plotly.newPlot('plot', traces, layout, {{responsive:true}});
		       }});
	       </script>
	</body>
	</html>"""
    Path(output).write_text(html, encoding="utf-8")
    return output


def main(argv: List[str]):
    if len(argv) > 2:
        print("too many arguments", file=sys.stderr)
        sys.exit(2)

    json_path = argv[1] if len(argv) == 2 else "merecorder.json"
    if not Path(json_path).is_file():
        print(f"File not found: {json_path}", file=sys.stderr)
        sys.exit(1)

    data = load_json(json_path)
    struct = _build_structure_from_loaded(data)
    # json.dump(struct, sys.stdout, ensure_ascii=False, indent=4)
    # print()

    if json_path.lower().endswith(".json"):
        html_path = json_path[:-5] + ".html"
    else:
        html_path = json_path + ".html"
    out = generate_html(data, html_path, struct)
    print(f"HTML written: {out}")


if __name__ == "__main__":  # pragma: no cover
    main(sys.argv)
