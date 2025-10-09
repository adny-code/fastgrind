#!/usr/bin/env python3
"""fastgrind post-processing tool.

Purpose
-------
1. Parse a profiling output JSON file (``fastgrind.json`` by default).
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
                python fastgrind.py [path/to/fastgrind.json]

If the argument is omitted, the script looks for ``fastgrind.json`` in the
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

# Optional imports for the matplotlib interactive plot function. We import them lazily
# inside the plotting function to avoid forcing users to have a GUI/matplotlib when
# only generating HTML.

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
    try:
        text = Path(path).read_text(encoding="utf-8", errors="replace")
        return json.loads(text)
    except Exception as e:
        print(f"Failed to load JSON from {path}: {e}", file=sys.stderr)
        sys.exit(1)


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
	       <title>fastgrind memory plot</title>
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
	       <h2>fastgrind memory timeline (Show Thread::Functions allocation & deallocation by ticks)</h2>
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
				       xaxis: {{ title: 'Tick (ms)' }},
				       yaxis: {{ title: 'Memory Size (bytes)' }},
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


def plot_with_matplotlib(
    data: Any, agg_struct: AggType | None = None
):  # pragma: no cover
    """Interactive matplotlib based plot replicating the logic of ``generate_html``.

    This function opens a window containing:
      1. Three multi-select list boxes (Thread IDs, Function Names, Metrics).
      2. A dynamic Function Names list that refreshes whenever thread selection changes.
      3. A Plot button disabled until all three selections are non-empty.
      4. The x-axis is tick; y-axis is the sum of the selected metric for nodes whose
             name equals any selected function name per thread, per tick (summing multiple
             selected functions and threads produces multiple traces, one per combination).

    Notes
    -----
    * Implemented with the TkAgg backend (falls back if unavailable) using Tkinter
      widgets for the lists and button.
    * This is intentionally self-contained and does not require the HTML/Plotly stack.
    * Designed for exploratory local usage; not intended for headless environments.
    * All comments are in English as requested.
    """
    if agg_struct is None:
        agg_struct = _build_structure_from_loaded(data)

    # Lazy imports so script can run in environments without GUI libs if user only wants HTML.
    import importlib
    import matplotlib

    try:
        # Try to ensure an interactive backend (TkAgg preferred for Tk widgets).
        matplotlib.use("TkAgg")  # type: ignore
    except Exception:
        pass
    import matplotlib.pyplot as plt
    from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
    import tkinter as tk
    from tkinter import ttk

    # Collect sorted tick values as integers.
    ticks = sorted(int(t) for t in data.keys()) if isinstance(data, dict) else []
    thread_ids, _all_funcs = _collect_names(data)
    metrics = ["malloc", "free"]

    # Helper to traverse a thread root and sum metric for given function names.
    def sum_metric(root, fn_names, metric):
        total = 0
        stack = [root]
        while stack:
            n = stack.pop()
            if isinstance(n, dict):
                nm = n.get("name")
                if nm in fn_names and isinstance(n.get(metric), (int, float)):
                    total += n[metric]
                ch = n.get("children") or []
                if isinstance(ch, list):
                    stack.extend(ch)
        return total

    # Build mapping tick -> thread id -> root object.
    # Data layout: data[tick][threadIdStr] = rootNode
    # We will index directly when calculating traces.

    # GUI setup.
    root = tk.Tk()
    root.title(
        "fastgrind memory plot (Show Thread::Functions allocation & deallocation by ticks)"
    )

    # Containers.
    top_frame = ttk.Frame(root)
    top_frame.pack(side=tk.TOP, fill=tk.X, padx=8, pady=4)

    list_frame = ttk.Frame(top_frame)
    list_frame.pack(side=tk.LEFT, fill=tk.X, expand=True)

    plot_frame = ttk.Frame(root)
    plot_frame.pack(side=tk.TOP, fill=tk.BOTH, expand=True)

    # Listbox factory.
    def make_listbox(parent, title):
        frame = ttk.Frame(parent)
        lbl = ttk.Label(frame, text=title)
        lbl.pack(anchor="w")
        lb = tk.Listbox(
            frame, selectmode=tk.EXTENDED, exportselection=False, height=6, width=35
        )
        lb.pack(fill=tk.BOTH, expand=True)
        sb = ttk.Scrollbar(frame, orient=tk.VERTICAL, command=lb.yview)
        lb.configure(yscrollcommand=sb.set)
        sb.pack(side=tk.RIGHT, fill=tk.Y)
        frame.pack(side=tk.LEFT, padx=8)
        return lb

    thread_lb = make_listbox(list_frame, "Threads")
    func_lb = make_listbox(list_frame, "Functions")
    metric_lb = make_listbox(list_frame, "Metrics")

    # Populate thread and metric listboxes.
    for tid in thread_ids:
        thread_lb.insert(tk.END, tid)
    for m in metrics:
        metric_lb.insert(tk.END, m)

    # Figure and canvas.
    fig, ax = plt.subplots(figsize=(9, 5))
    canvas = FigureCanvasTkAgg(fig, master=plot_frame)
    canvas_widget = canvas.get_tk_widget()
    canvas_widget.pack(fill=tk.BOTH, expand=True)

    # Status label.
    status_var = tk.StringVar(value="Select items, then click Plot")
    status_lbl = ttk.Label(root, textvariable=status_var, anchor="w")
    status_lbl.pack(fill=tk.X, padx=8, pady=2)

    # Plot button.
    btn_frame = ttk.Frame(top_frame)
    btn_frame.pack(side=tk.LEFT, padx=8)
    plot_btn = ttk.Button(btn_frame, text="Plot")
    plot_btn.pack(pady=4)
    plot_btn.state(["disabled"])  # disabled until all three have selections

    # Utility to read current selections.
    def get_selected(listbox):
        return [listbox.get(i) for i in listbox.curselection()]

    # Update function list based on selected threads.
    def refresh_functions(*_):
        selected_threads = get_selected(thread_lb)
        func_set = set()
        if not selected_threads:
            for tdict in agg_struct.values():
                func_set.update(tdict.keys())
        else:
            for tid in selected_threads:
                try:
                    tid_int = int(tid)
                except ValueError:
                    continue
                tdict = agg_struct.get(tid_int, {})
                func_set.update(tdict.keys())
        prev = set(get_selected(func_lb))
        func_lb.delete(0, tk.END)
        for fn in sorted(func_set):
            func_lb.insert(tk.END, fn)
            if fn in prev:
                # Reselect if previously selected.
                last_index = func_lb.size() - 1
                func_lb.selection_set(last_index)
        evaluate_button_state()

    def evaluate_button_state(*_):
        if (
            get_selected(thread_lb)
            and get_selected(func_lb)
            and get_selected(metric_lb)
        ):
            plot_btn.state(["!disabled"])
            status_var.set("Ready to plot")
        else:
            plot_btn.state(["disabled"])
            status_var.set("Select at least one in each list")

    def do_plot():
        threads = get_selected(thread_lb)
        funcs = get_selected(func_lb)
        metrics_sel = get_selected(metric_lb)
        if not (threads and funcs and metrics_sel):
            return
        ax.clear()
        # Build traces similarly to generate_html (one trace per combination).
        for tid in threads:
            for fn in funcs:
                for metric in metrics_sel:
                    y_vals = []
                    for tk_val in ticks:
                        tk_str = str(tk_val)
                        thread_root = None
                        if tk_str in data:
                            thread_root = data[tk_str].get(tid)
                        if thread_root is None:
                            y_vals.append(0)
                            continue
                        y_vals.append(sum_metric(thread_root, {fn}, metric))
                    ax.plot(ticks, y_vals, marker="o", label=f"T{tid}::{fn}::{metric}")
        ax.set_xlabel("Tick (ms)")
        ax.set_ylabel("Memory Size (bytes)")
        ax.set_title("Memory vs Tick")
        ax.legend(
            loc="upper center", bbox_to_anchor=(0.5, 1.15), ncol=3, fontsize="small"
        )
        ax.grid(True, linestyle="--", alpha=0.3)
        canvas.draw()
        status_var.set("Plot updated")

    # Bind events.
    thread_lb.bind("<<ListboxSelect>>", refresh_functions)
    func_lb.bind("<<ListboxSelect>>", evaluate_button_state)
    metric_lb.bind("<<ListboxSelect>>", evaluate_button_state)
    plot_btn.configure(command=do_plot)

    refresh_functions()

    _closed_flag = {"done": False}

    def _on_close():
        if _closed_flag["done"]:
            return
        _closed_flag["done"] = True
        try:
            plt.close("all")
        except Exception:
            pass

        try:
            if root.winfo_exists():
                root.quit()
                root.destroy()
        except Exception:
            pass

    root.protocol("WM_DELETE_WINDOW", _on_close)
    root.bind("<Escape>", lambda _e: _on_close())
    try:
        fig.canvas.mpl_connect("close_event", lambda _evt: _on_close())
    except Exception:
        pass

    root.mainloop()

    return True


def main(argv: List[str]):
    if len(argv) > 2:
        print("too many arguments", file=sys.stderr)
        sys.exit(2)

    json_path = argv[1] if len(argv) == 2 else "fastgrind.json"
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

    plot_with_matplotlib(data, struct)


if __name__ == "__main__":
    main(sys.argv)
