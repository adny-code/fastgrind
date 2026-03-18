#!/usr/bin/env python3
"""fastgrind binary trace tooling.

Primary workflow:
1. `inspect` prints quick metadata from `fastgrind.fgb`.
2. `ui` launches the Python-first interactive experience.
3. `html` serves a lightweight browser viewer backed by the same query engine.
4. `export-html` writes a compact static HTML snapshot.
5. `export-json` regenerates a compatibility JSON artifact for debugging.
"""

from __future__ import annotations

import argparse
import bisect
import json
import mmap
import os
import struct
import sys
import urllib.parse
import webbrowser
import zlib
from collections import OrderedDict, defaultdict
from concurrent.futures import Future, ThreadPoolExecutor
from dataclasses import dataclass
from html import escape as html_escape
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from typing import Any, Iterable, Sequence


HEADER_STRUCT = struct.Struct("<4sHHBBHIQIIIIIIQQQQQQ")
SECTION_STRUCT = struct.Struct("<IIQQII")
STACK_NODE_STRUCT = struct.Struct("<IIIHH")
THREAD_ENTRY_STRUCT = struct.Struct("<QQQQQ")
TICK_DIRECTORY_STRUCT = struct.Struct("<QQII")
THREAD_BLOCK_STRUCT = struct.Struct("<QII")
FRAME_RECORD_STRUCT = struct.Struct("<IIQQ")
U32_STRUCT = struct.Struct("<I")

MAGIC = b"FGB1"

SECTION_FUNCTION_TABLE = 1
SECTION_STACK_TABLE = 2
SECTION_THREAD_TABLE = 3
SECTION_THREAD_FUNCTION_DIRECTORY = 4
SECTION_FUNCTION_STACK_POSTINGS = 5
SECTION_TICK_DATA = 6
SECTION_TICK_DIRECTORY = 7

SECTION_FLAG_CRC32 = 1 << 0


@dataclass(frozen=True)
class SectionEntry:
    section_type: int
    flags: int
    offset: int
    length: int
    crc32: int


@dataclass(frozen=True)
class StackNode:
    node_id: int
    parent_id: int
    function_id: int
    depth: int
    flags: int


@dataclass(frozen=True)
class ThreadInfo:
    thread_id: int
    first_tick_ms: int
    last_tick_ms: int
    total_malloc_bytes: int
    total_free_bytes: int


@dataclass(frozen=True)
class TickDirectoryEntry:
    tick_ms: int
    file_offset: int
    thread_block_count: int


@dataclass
class SeriesResponse:
    ticks: list[int]
    series: dict[str, list[float]]
    metric: str
    scope: str
    split: str


METRIC_OPTIONS = ("malloc", "free", "net", "live")
METRIC_ALIASES = {
    "tick_res": "net",
    "sum_res": "live",
}
METRIC_DISPLAY_NAMES = {
    "malloc": "malloc",
    "free": "free",
    "net": "tick_res",
    "live": "sum_res",
    "memory": "memory",
}


def _normalize_metric_name(metric: str) -> str:
    name = str(metric).strip().casefold()
    return METRIC_ALIASES.get(name, name)


def _metric_display_name(metric: str) -> str:
    normalized = _normalize_metric_name(metric)
    return METRIC_DISPLAY_NAMES.get(normalized, str(metric).strip())


def _format_bytes(value: int) -> str:
    units = ["B", "KiB", "MiB", "GiB", "TiB"]
    size = float(value)
    for unit in units:
        if abs(size) < 1024.0 or unit == units[-1]:
            return f"{size:.2f} {unit}"
        size /= 1024.0
    return f"{size:.2f} TiB"


def _format_value(value: float) -> str:
    if abs(value - round(value)) < 1e-9:
        return f"{int(round(value)):,}"
    return f"{value:,.2f}"


def _format_percent(value: float) -> str:
    return f"{value:.1f}%"


def _tick_axis_label() -> str:
    return "Tick (ms)"


def _metric_axis_label(metric: str) -> str:
    display_name = _metric_display_name(metric)
    if display_name == "memory":
        return "Memory (bytes)"
    return f"{display_name} (bytes)"


def _time_window_label(start_ms: int, end_ms: int) -> str:
    if start_ms == end_ms:
        return f"Tick {start_ms} ms"
    return f"Window {start_ms}..{end_ms} ms"


def _detail_context_payload(
    start_ms: int,
    end_ms: int,
    thread_ids: Sequence[int],
    focus_thread: int | None,
) -> dict[str, Any]:
    normalized_threads = [int(thread_id) for thread_id in thread_ids]
    if not normalized_threads:
        top_scope = "Top aggregates no threads"
    elif len(normalized_threads) == 1:
        top_scope = f"Top aggregates T{normalized_threads[0]}"
    else:
        top_scope = f"Top aggregates {len(normalized_threads)} threads"

    if focus_thread is None:
        stack_scope = "Stack has no focus thread"
    else:
        stack_scope = f"Stack shows T{int(focus_thread)}"

    return {
        "mode": "tick" if start_ms == end_ms else "window",
        "start_ms": int(start_ms),
        "end_ms": int(end_ms),
        "thread_ids": normalized_threads,
        "focus_thread": int(focus_thread) if focus_thread is not None else None,
        "summary": f"{_time_window_label(start_ms, end_ms)}. {top_scope}. {stack_scope}.",
    }


def _stack_node_name(node: dict[str, Any]) -> str:
    return str(node.get("display_name") or node.get("canonical_name") or "(unknown)")


def _format_stack_summary(node: dict[str, Any]) -> str:
    return (
        f"share {_format_percent(float(node.get('share_percent', 0.0)))} | "
        f"value {_format_value(float(node.get('value', 0.0)))} | "
        f"malloc {_format_value(float(node.get('malloc', 0.0)))} | "
        f"free {_format_value(float(node.get('free', 0.0)))}"
    )


def _stack_panel_script() -> str:
        return """
        function formatStackNumber(value) {
            const numericValue = Number(value);
            if (!Number.isFinite(numericValue)) {
                return '0';
            }
            return numericValue.toLocaleString(undefined, { maximumFractionDigits: 2 });
        }

        function formatStackPercent(value) {
            const numericValue = Number(value);
            if (!Number.isFinite(numericValue)) {
                return '0.0%';
            }
            return `${numericValue.toFixed(1)}%`;
        }

        function stackNodeLabel(node) {
            return node.display_name || node.canonical_name || '(unknown)';
        }

        function stackNodeCanonical(node) {
            return node.canonical_name || node.display_name || '(unknown)';
        }

        function stackNodeSummary(node) {
            return `share ${formatStackPercent(node.share_percent)} | value ${formatStackNumber(node.value)} | malloc ${formatStackNumber(node.malloc)} | free ${formatStackNumber(node.free)}`;
        }

        function updateStackSelected(selectedId, node) {
            const selected = document.getElementById(selectedId);
            if (!selected) {
                return;
            }
            selected.textContent = stackNodeCanonical(node);
        }

        function buildStackRow(node, selectedId) {
            const row = document.createElement('div');
            row.className = 'stack-row';

            const name = document.createElement('span');
            name.className = 'stack-name';
            name.textContent = stackNodeLabel(node);
            name.title = stackNodeCanonical(node);

            const share = document.createElement('span');
            share.className = 'stack-share';
            share.textContent = formatStackPercent(node.share_percent);

            const value = document.createElement('span');
            value.className = 'stack-value';
            value.textContent = formatStackNumber(node.value);

            const malloc = document.createElement('span');
            malloc.className = 'stack-metric';
            malloc.textContent = formatStackNumber(node.malloc);

            const free = document.createElement('span');
            free.className = 'stack-metric';
            free.textContent = formatStackNumber(node.free);

            row.appendChild(name);
            row.appendChild(share);
            row.appendChild(value);
            row.appendChild(malloc);
            row.appendChild(free);
            row.addEventListener('click', () => updateStackSelected(selectedId, node));
            return row;
        }

        function createStackNode(node, depth, selectedId) {
            const children = Array.isArray(node.children) ? node.children : [];
            if (children.length === 0) {
                const leaf = document.createElement('div');
                leaf.className = 'stack-leaf';
                leaf.style.marginLeft = `${depth * 14}px`;
                leaf.appendChild(buildStackRow(node, selectedId));
                return leaf;
            }

            const details = document.createElement('details');
            details.className = 'stack-node';
            details.open = depth < 1;
            details.style.marginLeft = `${depth * 14}px`;

            const summary = document.createElement('summary');
            summary.appendChild(buildStackRow(node, selectedId));
            details.appendChild(summary);

            const branch = document.createElement('div');
            branch.className = 'stack-children';
            for (const child of children) {
                branch.appendChild(createStackNode(child, depth + 1, selectedId));
            }
            details.appendChild(branch);
            return details;
        }

        function renderStackTree(containerId, summaryId, selectedId, tree) {
            const container = document.getElementById(containerId);
            const summary = document.getElementById(summaryId);
            if (!container || !summary) {
                return;
            }

            container.innerHTML = '';
            if (!tree) {
                summary.textContent = 'No stack data';
                updateStackSelected(selectedId, { canonical_name: 'No stack data' });
                return;
            }

            summary.textContent = `${stackNodeLabel(tree)} | ${stackNodeSummary(tree)}`;
            updateStackSelected(selectedId, tree);

            const children = Array.isArray(tree.children) ? tree.children : [];
            if (children.length === 0) {
                const empty = document.createElement('div');
                empty.className = 'stack-empty';
                empty.textContent = 'No stack frames in the selected window.';
                container.appendChild(empty);
                return;
            }

            for (const child of children) {
                container.appendChild(createStackNode(child, 0, selectedId));
            }
        }

        function toggleStackTree(containerId, open) {
            const container = document.getElementById(containerId);
            if (!container) {
                return;
            }
            for (const node of container.querySelectorAll('details')) {
                node.open = open;
            }
        }
        """


def _align_down(value: int, interval: int) -> int:
    if interval <= 0:
        return value
    return (value // interval) * interval


def _align_up(value: int, interval: int) -> int:
    if interval <= 0:
        return value
    return ((value + interval - 1) // interval) * interval


def _simplify_symbol(name: str) -> str:
    head = name.split("(", 1)[0].strip()
    return head or name


def _parse_csv_ints(raw: str | None) -> list[int]:
    if not raw:
        return []
    values: list[int] = []
    for chunk in raw.split(","):
        chunk = chunk.strip()
        if not chunk:
            continue
        values.append(int(chunk))
    return values


def _parse_csv_strings(raw: str | None) -> list[str]:
    if not raw:
        return []
    values: list[str] = []
    for chunk in raw.split(","):
        chunk = chunk.strip()
        if not chunk:
            continue
        values.append(chunk)
    return values


def _json_response(payload: Any) -> bytes:
    return json.dumps(payload, ensure_ascii=False).encode("utf-8")


def _json_for_html_script(payload: Any) -> str:
    return (
        json.dumps(payload, ensure_ascii=False)
        .replace("<", "\\u003c")
        .replace(">", "\\u003e")
        .replace("&", "\\u0026")
        .replace("\u2028", "\\u2028")
        .replace("\u2029", "\\u2029")
    )


class FastgrindBinaryReader:
    def __init__(self, path: str | Path):
        self.path = Path(path)
        if not self.path.is_file():
            raise FileNotFoundError(f"file not found: {self.path}")

        self._file = self.path.open("rb")
        self._mmap = mmap.mmap(self._file.fileno(), 0, access=mmap.ACCESS_READ)
        self._tick_cache: OrderedDict[int, list[tuple[int, list[tuple[int, int, int]]]]] = OrderedDict()

        self.header = self._parse_header()
        self.sections = self._parse_sections()
        self.section_by_type = {entry.section_type: entry for entry in self.sections}
        self._validate_sections()

        self.function_names = self._parse_function_table()
        self.display_names = self._derive_display_names(self.function_names)
        self.stack_nodes = self._parse_stack_table()
        self.node_paths, self.node_unique_functions = self._build_node_paths()
        self.thread_table = self._parse_thread_table()
        self.thread_function_directory = self._parse_thread_function_directory()
        self.thread_function_sets = {
            thread_id: frozenset(function_ids) for thread_id, function_ids in self.thread_function_directory.items()
        }
        self.function_stack_postings = self._parse_function_stack_postings()
        self.exclusive_function_postings = self._build_exclusive_function_postings()
        self.tick_directory = self._parse_tick_directory()
        self.tick_values = [entry.tick_ms for entry in self.tick_directory]

    def close(self) -> None:
        if getattr(self, "_mmap", None) is not None:
            try:
                self._mmap.close()
            except Exception:
                pass
            self._mmap = None
        if getattr(self, "_file", None) is not None:
            try:
                self._file.close()
            except Exception:
                pass
            self._file = None

    def __del__(self) -> None:
        try:
            self.close()
        except Exception:
            pass

    @property
    def sample_interval_ms(self) -> int:
        return int(self.header["sample_interval_ms"])

    @property
    def max_tick_ms(self) -> int:
        return int(self.header["max_tick_ms"])

    @property
    def tick_data_end(self) -> int:
        section = self.section_by_type[SECTION_TICK_DATA]
        return section.offset + section.length

    def default_start_tick(self) -> int:
        if not self.tick_values:
            return 0
        if self.tick_values[0] == 0:
            return 0
        return self.sample_interval_ms

    def meta(self) -> dict[str, Any]:
        return {
            "path": str(self.path),
            "sample_interval_ms": self.sample_interval_ms,
            "default_start_ms": self.default_start_tick(),
            "max_tick_ms": self.max_tick_ms,
            "function_count": self.header["function_count"],
            "stack_node_count": self.header["stack_node_count"],
            "tick_count": self.header["tick_count"],
            "thread_count": self.header["thread_count"],
            "section_count": self.header["section_count"],
        }

    def iter_tick_blocks_range(
        self, start_ms: int | None = None, end_ms: int | None = None
    ) -> Iterable[tuple[TickDirectoryEntry, list[tuple[int, list[tuple[int, int, int]]]]]]:
        if not self.tick_directory:
            return []

        start_value = self.default_start_tick() if start_ms is None else start_ms
        end_value = self.max_tick_ms if end_ms is None else end_ms
        start_index = bisect.bisect_left(self.tick_values, start_value)
        end_index = bisect.bisect_right(self.tick_values, end_value)
        return [
            (self.tick_directory[index], self.get_tick_block(index))
            for index in range(start_index, end_index)
        ]

    def get_tick_block(self, index: int) -> list[tuple[int, list[tuple[int, int, int]]]]:
        cached = self._tick_cache.get(index)
        if cached is not None:
            self._tick_cache.move_to_end(index)
            return cached

        entry = self.tick_directory[index]
        block_end = self.tick_directory[index + 1].file_offset if index + 1 < len(self.tick_directory) else self.tick_data_end
        pos = entry.file_offset
        blocks: list[tuple[int, list[tuple[int, int, int]]]] = []

        for _ in range(entry.thread_block_count):
            thread_id, record_count, _reserved = THREAD_BLOCK_STRUCT.unpack_from(self._mmap, pos)
            pos += THREAD_BLOCK_STRUCT.size
            records: list[tuple[int, int, int]] = []
            for _ in range(record_count):
                leaf_node_id, _record_reserved, malloc_bytes, free_bytes = FRAME_RECORD_STRUCT.unpack_from(self._mmap, pos)
                pos += FRAME_RECORD_STRUCT.size
                records.append((leaf_node_id, malloc_bytes, free_bytes))
            blocks.append((thread_id, records))

        if pos != block_end:
            raise ValueError(f"tick block {entry.tick_ms} bytes do not match directory")

        self._tick_cache[index] = blocks
        if len(self._tick_cache) > 128:
            self._tick_cache.popitem(last=False)
        return blocks

    def _parse_header(self) -> dict[str, int]:
        if len(self._mmap) < HEADER_STRUCT.size:
            raise ValueError("file is too small to contain an FGB header")

        unpacked = HEADER_STRUCT.unpack_from(self._mmap, 0)
        magic = unpacked[0]
        if magic != MAGIC:
            raise ValueError(f"unexpected magic {magic!r}, expected {MAGIC!r}")
        if unpacked[3] != 1:
            raise ValueError("only little-endian traces are supported")

        keys = [
            "magic",
            "version_major",
            "version_minor",
            "endian",
            "flags",
            "header_bytes",
            "sample_interval_ms",
            "max_tick_ms",
            "function_count",
            "stack_node_count",
            "tick_count",
            "thread_count",
            "section_count",
            "reserved0",
            "section_directory_offset",
            "function_table_offset",
            "stack_table_offset",
            "tick_directory_offset",
            "tick_data_offset",
            "reserved1",
        ]
        header = dict(zip(keys, unpacked))
        if header["header_bytes"] != HEADER_STRUCT.size:
            raise ValueError(
                f"unsupported header size {header['header_bytes']}, expected {HEADER_STRUCT.size}"
            )
        return header

    def _parse_sections(self) -> list[SectionEntry]:
        pos = int(self.header["section_directory_offset"])
        entries: list[SectionEntry] = []
        for _ in range(int(self.header["section_count"])):
            self._require_range(pos, SECTION_STRUCT.size)
            section_type, flags, offset, length, crc32, _reserved = SECTION_STRUCT.unpack_from(self._mmap, pos)
            entries.append(SectionEntry(section_type, flags, offset, length, crc32))
            pos += SECTION_STRUCT.size
        return entries

    def _validate_sections(self) -> None:
        for entry in self.sections:
            self._require_range(entry.offset, entry.length)
            if entry.flags & SECTION_FLAG_CRC32:
                view = memoryview(self._mmap)[entry.offset : entry.offset + entry.length]
                checksum = zlib.crc32(view) & 0xFFFFFFFF
                if checksum != entry.crc32:
                    raise ValueError(
                        f"CRC mismatch for section {entry.section_type}: expected {entry.crc32:#010x}, got {checksum:#010x}"
                    )

        required = [
            SECTION_FUNCTION_TABLE,
            SECTION_STACK_TABLE,
            SECTION_THREAD_TABLE,
            SECTION_THREAD_FUNCTION_DIRECTORY,
            SECTION_FUNCTION_STACK_POSTINGS,
            SECTION_TICK_DATA,
            SECTION_TICK_DIRECTORY,
        ]
        for section_type in required:
            if section_type not in self.section_by_type:
                raise ValueError(f"missing required section {section_type}")

        if self.section_by_type[SECTION_FUNCTION_TABLE].offset != self.header["function_table_offset"]:
            raise ValueError("function table offset does not match header")
        if self.section_by_type[SECTION_STACK_TABLE].offset != self.header["stack_table_offset"]:
            raise ValueError("stack table offset does not match header")
        if self.section_by_type[SECTION_TICK_DIRECTORY].offset != self.header["tick_directory_offset"]:
            raise ValueError("tick directory offset does not match header")
        if self.section_by_type[SECTION_TICK_DATA].offset != self.header["tick_data_offset"]:
            raise ValueError("tick data offset does not match header")

    def _parse_function_table(self) -> list[str | None]:
        section = self.section_by_type[SECTION_FUNCTION_TABLE]
        pos = section.offset
        end = pos + section.length
        names: list[str | None] = [None]
        for _ in range(int(self.header["function_count"])):
            self._require_range(pos, U32_STRUCT.size)
            (name_bytes,) = U32_STRUCT.unpack_from(self._mmap, pos)
            pos += U32_STRUCT.size
            self._require_range(pos, name_bytes)
            names.append(bytes(self._mmap[pos : pos + name_bytes]).decode("utf-8", errors="replace"))
            pos += name_bytes
        if pos != end:
            raise ValueError("function table size does not match the encoded payload")
        return names

    def _derive_display_names(self, function_names: Sequence[str | None]) -> list[str | None]:
        short_names = [_simplify_symbol(name) if name else None for name in function_names]
        counts = defaultdict(int)
        for short_name in short_names[1:]:
            if short_name:
                counts[short_name] += 1

        display_names: list[str | None] = [None]
        for function_id in range(1, len(function_names)):
            canonical = function_names[function_id] or ""
            short_name = short_names[function_id] or canonical
            display_names.append(canonical if counts[short_name] > 1 else short_name)
        return display_names

    def _parse_stack_table(self) -> list[StackNode]:
        section = self.section_by_type[SECTION_STACK_TABLE]
        pos = section.offset
        end = pos + section.length
        nodes: list[StackNode] = []
        for expected_node_id in range(int(self.header["stack_node_count"])):
            self._require_range(pos, STACK_NODE_STRUCT.size)
            node_id, parent_id, function_id, depth, flags = STACK_NODE_STRUCT.unpack_from(self._mmap, pos)
            pos += STACK_NODE_STRUCT.size
            if node_id != expected_node_id:
                raise ValueError(f"stack node id {node_id} does not match its ordinal {expected_node_id}")
            nodes.append(StackNode(node_id, parent_id, function_id, depth, flags))
        if pos != end:
            raise ValueError("stack table size does not match the encoded payload")
        return nodes

    def _build_node_paths(self) -> tuple[list[tuple[int, ...]], list[frozenset[int]]]:
        paths: list[tuple[int, ...]] = [tuple() for _ in self.stack_nodes]
        unique_sets: list[frozenset[int]] = [frozenset() for _ in self.stack_nodes]
        for node in self.stack_nodes[1:]:
            if node.parent_id >= node.node_id:
                raise ValueError(f"stack node {node.node_id} has an invalid parent {node.parent_id}")
            parent_path = paths[node.parent_id]
            path = parent_path + (node.function_id,)
            paths[node.node_id] = path
            unique_sets[node.node_id] = frozenset(path)
        return paths, unique_sets

    def _parse_thread_table(self) -> list[ThreadInfo]:
        section = self.section_by_type[SECTION_THREAD_TABLE]
        pos = section.offset
        end = pos + section.length
        threads: list[ThreadInfo] = []
        for _ in range(int(self.header["thread_count"])):
            self._require_range(pos, THREAD_ENTRY_STRUCT.size)
            threads.append(ThreadInfo(*THREAD_ENTRY_STRUCT.unpack_from(self._mmap, pos)))
            pos += THREAD_ENTRY_STRUCT.size
        if pos != end:
            raise ValueError("thread table size does not match the encoded payload")
        return threads

    def _parse_thread_function_directory(self) -> dict[int, list[int]]:
        section = self.section_by_type[SECTION_THREAD_FUNCTION_DIRECTORY]
        if section.length == 0:
            return {}
        pos = section.offset
        end = pos + section.length
        (thread_count,) = U32_STRUCT.unpack_from(self._mmap, pos)
        pos += U32_STRUCT.size
        directory: dict[int, list[int]] = {}
        for _ in range(thread_count):
            thread_id, function_count, _reserved = struct.unpack_from("<QII", self._mmap, pos)
            pos += struct.calcsize("<QII")
            function_ids: list[int] = []
            for _ in range(function_count):
                (function_id,) = U32_STRUCT.unpack_from(self._mmap, pos)
                pos += U32_STRUCT.size
                function_ids.append(function_id)
            directory[thread_id] = function_ids
        if pos != end:
            raise ValueError("thread-function directory size does not match the encoded payload")
        return directory

    def _parse_function_stack_postings(self) -> dict[int, list[int]]:
        section = self.section_by_type[SECTION_FUNCTION_STACK_POSTINGS]
        if section.length == 0:
            return {}
        pos = section.offset
        end = pos + section.length
        (function_count,) = U32_STRUCT.unpack_from(self._mmap, pos)
        pos += U32_STRUCT.size
        postings: dict[int, list[int]] = {}
        for _ in range(function_count):
            function_id, leaf_count = struct.unpack_from("<II", self._mmap, pos)
            pos += struct.calcsize("<II")
            leaf_ids: list[int] = []
            for _ in range(leaf_count):
                (leaf_id,) = U32_STRUCT.unpack_from(self._mmap, pos)
                pos += U32_STRUCT.size
                leaf_ids.append(leaf_id)
            postings[function_id] = leaf_ids
        if pos != end:
            raise ValueError("function-stack postings size does not match the encoded payload")
        return postings

    def _build_exclusive_function_postings(self) -> dict[int, tuple[int, ...]]:
        child_counts = [0] * len(self.stack_nodes)
        for node in self.stack_nodes[1:]:
            child_counts[node.parent_id] += 1

        postings: dict[int, list[int]] = defaultdict(list)
        for node in self.stack_nodes[1:]:
            if child_counts[node.node_id] == 0:
                postings[node.function_id].append(node.node_id)
        return {function_id: tuple(leaf_ids) for function_id, leaf_ids in postings.items()}

    def _parse_tick_directory(self) -> list[TickDirectoryEntry]:
        section = self.section_by_type[SECTION_TICK_DIRECTORY]
        pos = section.offset
        end = pos + section.length
        entries: list[TickDirectoryEntry] = []
        for _ in range(int(self.header["tick_count"])):
            self._require_range(pos, TICK_DIRECTORY_STRUCT.size)
            tick_ms, file_offset, thread_block_count, _reserved = TICK_DIRECTORY_STRUCT.unpack_from(self._mmap, pos)
            entries.append(TickDirectoryEntry(tick_ms, file_offset, thread_block_count))
            pos += TICK_DIRECTORY_STRUCT.size
        if pos != end:
            raise ValueError("tick directory size does not match the encoded payload")
        return entries

    def _require_range(self, offset: int, length: int) -> None:
        if offset < 0 or length < 0 or offset + length > len(self._mmap):
            raise ValueError(f"requested range [{offset}, {offset + length}) is outside the file")


class FastgrindQueryEngine:
    def __init__(self, reader: FastgrindBinaryReader):
        self.reader = reader
        self._all_thread_ids = [entry.thread_id for entry in reader.thread_table]

    def meta(self) -> dict[str, Any]:
        meta = self.reader.meta()
        total_malloc = sum(thread.total_malloc_bytes for thread in self.reader.thread_table)
        total_free = sum(thread.total_free_bytes for thread in self.reader.thread_table)
        meta.update(
            {
                "total_malloc_bytes": total_malloc,
                "total_free_bytes": total_free,
                "total_net_bytes": total_malloc - total_free,
            }
        )
        return meta

    def default_window(self) -> tuple[int, int]:
        return self.reader.default_start_tick(), self.reader.max_tick_ms

    def list_threads(self) -> list[dict[str, Any]]:
        return [
            {
                "thread_id": thread.thread_id,
                "first_tick_ms": thread.first_tick_ms,
                "last_tick_ms": thread.last_tick_ms,
                "total_malloc_bytes": thread.total_malloc_bytes,
                "total_free_bytes": thread.total_free_bytes,
                "net_bytes": thread.total_malloc_bytes - thread.total_free_bytes,
            }
            for thread in self.reader.thread_table
        ]

    def list_functions(
        self,
        thread_ids: Sequence[int] | None = None,
        query: str = "",
        limit: int | None = 200,
        sort: str = "alpha",
        start_ms: int | None = None,
        end_ms: int | None = None,
    ) -> list[dict[str, Any]]:
        if thread_ids:
            available: set[int] = set()
            for thread_id in thread_ids:
                available.update(self.reader.thread_function_directory.get(thread_id, []))
        else:
            available = set(range(1, len(self.reader.function_names)))

        query_lower = query.casefold().strip()
        items = []
        for function_id in sorted(available):
            canonical = self.reader.function_names[function_id] or ""
            display = self.reader.display_names[function_id] or canonical
            if query_lower and query_lower not in canonical.casefold() and query_lower not in display.casefold():
                continue
            items.append(
                {
                    "function_id": function_id,
                    "canonical_name": canonical,
                    "display_name": display,
                }
            )

        if sort == "hot" and items:
            scores = {
                row["function_id"]: row["value"]
                for row in self.top_functions(
                    start_ms=start_ms,
                    end_ms=end_ms,
                    thread_ids=thread_ids,
                    limit=max(limit or 200, len(items)),
                    metric="net",
                    scope="exclusive",
                )
            }
            items.sort(key=lambda item: (-abs(scores.get(item["function_id"], 0.0)), item["display_name"].casefold()))
        else:
            items.sort(key=lambda item: (item["display_name"].casefold(), item["canonical_name"].casefold()))

        if limit is not None:
            return items[:limit]
        return items

    def query_series(
        self,
        thread_ids: Sequence[int] | None = None,
        function_ids: Sequence[int] | None = None,
        start_ms: int | None = None,
        end_ms: int | None = None,
        metric: str = "malloc",
        scope: str = "inclusive",
        split: str = "auto",
        resolution: int | None = None,
    ) -> SeriesResponse:
        metric = _normalize_metric_name(metric)
        start_tick, end_tick = self._normalize_window(start_ms, end_ms)
        selected_threads = self._normalize_thread_ids(thread_ids)
        selected_functions = self._normalize_function_ids(function_ids)
        selected_threads = self._narrow_thread_ids_by_functions(selected_threads, selected_functions)
        split_mode = self._infer_split(split, selected_threads, selected_functions)
        leaf_match_lookup = self._build_leaf_match_lookup(selected_functions, scope)
        leaf_filter = frozenset(leaf_match_lookup) if leaf_match_lookup else None

        if end_tick < start_tick:
            end_tick = start_tick
        tick_count = ((end_tick - start_tick) // self.reader.sample_interval_ms) + 1 if self.reader.sample_interval_ms else 1
        ticks = [start_tick + index * self.reader.sample_interval_ms for index in range(tick_count)]
        raw_metric = "net" if metric == "live" else metric

        accumulator: dict[tuple[Any, ...], list[float]] = {}
        baselines: dict[tuple[Any, ...], float] = defaultdict(float)

        if metric == "live" and start_tick > self.reader.default_start_tick():
            for _tick_ms, thread_id, leaf_node_id, malloc_bytes, free_bytes in self._iter_records(
                self.reader.default_start_tick(),
                start_tick - self.reader.sample_interval_ms,
                selected_threads,
                leaf_filter=leaf_filter,
            ):
                keys = self._series_keys(
                    thread_id,
                    leaf_node_id,
                    selected_functions,
                    split_mode,
                    leaf_match_lookup,
                )
                if not keys:
                    continue
                delta = float(malloc_bytes - free_bytes)
                for key in keys:
                    baselines[key] += delta

        for tick_ms, thread_id, leaf_node_id, malloc_bytes, free_bytes in self._iter_records(
            start_tick, end_tick, selected_threads, leaf_filter=leaf_filter
        ):
            keys = self._series_keys(
                thread_id,
                leaf_node_id,
                selected_functions,
                split_mode,
                leaf_match_lookup,
            )
            if not keys:
                continue
            value = float(self._metric_value(malloc_bytes, free_bytes, raw_metric))
            tick_index = (tick_ms - start_tick) // self.reader.sample_interval_ms if self.reader.sample_interval_ms else 0
            for key in keys:
                accumulator.setdefault(key, [0.0] * tick_count)[tick_index] += value

        if not accumulator:
            default_key = self._default_empty_series_key(split_mode, selected_threads, selected_functions)
            accumulator[default_key] = [0.0] * tick_count

        if metric == "live":
            for key, values in accumulator.items():
                running = baselines.get(key, 0.0)
                for index, current in enumerate(values):
                    running += current
                    values[index] = running

        series = {self._format_series_key(key): values for key, values in accumulator.items()}
        if resolution and resolution > 0 and len(ticks) > resolution:
            ticks, series = self._downsample(ticks, series, resolution, metric)

        return SeriesResponse(ticks=ticks, series=series, metric=metric, scope=scope, split=split_mode)

    def top_functions(
        self,
        start_ms: int | None = None,
        end_ms: int | None = None,
        thread_ids: Sequence[int] | None = None,
        limit: int = 10,
        metric: str = "net",
        scope: str = "exclusive",
    ) -> list[dict[str, Any]]:
        metric = _normalize_metric_name(metric)
        start_tick, end_tick = self._normalize_window(start_ms, end_ms)
        selected_threads = self._normalize_thread_ids(thread_ids)
        totals: dict[int, float] = defaultdict(float)

        for _tick_ms, _thread_id, leaf_node_id, malloc_bytes, free_bytes in self._iter_records(
            start_tick, end_tick, selected_threads
        ):
            value = float(self._metric_value(malloc_bytes, free_bytes, "net" if metric == "live" else metric))
            if scope == "exclusive":
                function_ids = (self.reader.stack_nodes[leaf_node_id].function_id,)
            else:
                function_ids = self.reader.node_paths[leaf_node_id]
            for function_id in function_ids:
                totals[function_id] += value

        rows = []
        for function_id, value in totals.items():
            rows.append(
                {
                    "function_id": function_id,
                    "display_name": self.reader.display_names[function_id],
                    "canonical_name": self.reader.function_names[function_id],
                    "value": value,
                }
            )

        if metric == "net":
            rows.sort(key=lambda row: (-abs(row["value"]), row["display_name"].casefold()))
        else:
            rows.sort(key=lambda row: (-row["value"], row["display_name"].casefold()))
        return rows[:limit]

    def stack_breakdown(
        self,
        thread_id: int,
        start_ms: int | None = None,
        end_ms: int | None = None,
        metric: str = "net",
    ) -> str:
        return self._stack_tree_to_text(self.stack_tree(thread_id, start_ms=start_ms, end_ms=end_ms, metric=metric))

    def stack_tree(
        self,
        thread_id: int,
        start_ms: int | None = None,
        end_ms: int | None = None,
        metric: str = "net",
    ) -> dict[str, Any]:
        metric = _normalize_metric_name(metric)
        start_tick, end_tick = self._normalize_window(start_ms, end_ms)
        tree: dict[str, Any] = {
            "thread_id": thread_id,
            "function_id": None,
            "display_name": f"Thread {thread_id}",
            "canonical_name": f"Thread {thread_id}",
            "malloc": 0.0,
            "free": 0.0,
            "children": {},
        }

        for _tick_ms, current_thread_id, leaf_node_id, malloc_bytes, free_bytes in self._iter_records(
            start_tick, end_tick, (thread_id,)
        ):
            if current_thread_id != thread_id:
                continue
            node = tree
            node["malloc"] += float(malloc_bytes)
            node["free"] += float(free_bytes)
            for function_id in self.reader.node_paths[leaf_node_id]:
                child = node["children"].setdefault(function_id, self._new_stack_tree_node(function_id))
                child["malloc"] += float(malloc_bytes)
                child["free"] += float(free_bytes)
                node = child

        return self._apply_stack_tree_shares(self._finalize_stack_tree(tree, metric))

    def export_json(self, output: str | Path) -> Path:
        output_path = Path(output)
        data = self._compatibility_json_data()
        output_path.write_text(json.dumps(data, ensure_ascii=False, indent=2), encoding="utf-8")
        return output_path

    def export_html_snapshot(
        self,
        output: str | Path,
        start_ms: int | None = None,
        end_ms: int | None = None,
        thread_ids: Sequence[int] | None = None,
        function_ids: Sequence[int] | None = None,
        metric: str = "live",
        metrics: Sequence[str] | None = None,
        scope: str = "inclusive",
    ) -> Path:
        output_path = Path(output)
        start_tick, end_tick = self._normalize_window(start_ms, end_ms)
        selected_metrics = self._normalize_metrics(metrics, default=metric)
        series = self.query_multi_series(
            thread_ids=thread_ids,
            function_ids=function_ids,
            start_ms=start_tick,
            end_ms=end_tick,
            metrics=selected_metrics,
            scope=scope,
            split="auto",
            resolution=400,
        )
        primary_metric = self._primary_metric(selected_metrics)
        top_metric = "net" if primary_metric == "live" else primary_metric
        top = self.top_functions(start_tick, end_tick, thread_ids=thread_ids, limit=12, metric=top_metric, scope="exclusive")
        focus_thread = (thread_ids or self._all_thread_ids[:1])[:1]
        stack_tree = self.stack_tree(focus_thread[0], start_tick, end_tick, metric=top_metric) if focus_thread else None
        context = self.describe_detail_context(
            start_ms=start_tick,
            end_ms=end_tick,
            thread_ids=thread_ids,
            focus_thread=focus_thread[0] if focus_thread else None,
        )

        html = generate_snapshot_html(
            title=f"fastgrind snapshot: {self.reader.path.name}",
            meta=self.meta(),
            series=series,
            top_rows=top,
            stack_tree=stack_tree,
            detail_context=context,
        )
        output_path.write_text(html, encoding="utf-8")
        return output_path

    def query_bundle(
        self,
        thread_ids: Sequence[int] | None = None,
        function_ids: Sequence[int] | None = None,
        start_ms: int | None = None,
        end_ms: int | None = None,
        metric: str = "malloc",
        metrics: Sequence[str] | None = None,
        scope: str = "inclusive",
        resolution: int | None = None,
    ) -> dict[str, Any]:
        selected_threads = self._normalize_thread_ids(thread_ids)
        start_tick, end_tick = self._normalize_window(start_ms, end_ms)
        selected_metrics = self._normalize_metrics(metrics, default=metric)
        series = self.query_multi_series(
            thread_ids=selected_threads,
            function_ids=function_ids,
            start_ms=start_tick,
            end_ms=end_tick,
            metrics=selected_metrics,
            scope=scope,
            split="auto",
            resolution=resolution,
        )
        primary_metric = self._primary_metric(selected_metrics)
        top_metric = "net" if primary_metric == "live" else primary_metric
        top = self.top_functions(
            start_ms=start_tick,
            end_ms=end_tick,
            thread_ids=selected_threads,
            limit=15,
            metric=top_metric,
            scope="exclusive",
        )
        focus_thread = selected_threads[0] if selected_threads else (self._all_thread_ids[0] if self._all_thread_ids else None)
        stack_tree = self.stack_tree(focus_thread, start_tick, end_tick, metric=top_metric) if focus_thread else None
        return {
            "series": series,
            "top": top,
            "focus_thread": focus_thread,
            "stack_tree": stack_tree,
            "context": self.describe_detail_context(
                start_ms=start_tick,
                end_ms=end_tick,
                thread_ids=selected_threads,
                focus_thread=focus_thread,
            ),
            "selected_metrics": [_metric_display_name(metric_name) for metric_name in selected_metrics],
            "primary_metric": primary_metric,
        }

    def query_multi_series(
        self,
        thread_ids: Sequence[int] | None = None,
        function_ids: Sequence[int] | None = None,
        start_ms: int | None = None,
        end_ms: int | None = None,
        metrics: Sequence[str] | None = None,
        scope: str = "inclusive",
        split: str = "auto",
        resolution: int | None = None,
    ) -> SeriesResponse:
        selected_metrics = self._normalize_metrics(metrics)
        if len(selected_metrics) == 1:
            return self.query_series(
                thread_ids=thread_ids,
                function_ids=function_ids,
                start_ms=start_ms,
                end_ms=end_ms,
                metric=selected_metrics[0],
                scope=scope,
                split=split,
                resolution=resolution,
            )

        responses = [
            self.query_series(
                thread_ids=thread_ids,
                function_ids=function_ids,
                start_ms=start_ms,
                end_ms=end_ms,
                metric=metric_name,
                scope=scope,
                split=split,
                resolution=resolution,
            )
            for metric_name in selected_metrics
        ]

        merged_series: dict[str, list[float]] = {}
        for metric_name, response in zip(selected_metrics, responses):
            display_metric_name = _metric_display_name(metric_name)
            for label, values in response.series.items():
                merged_label = (
                    display_metric_name
                    if label == "Total" and len(response.series) == 1
                    else f"{display_metric_name}::{label}"
                )
                merged_series[merged_label] = values

        return SeriesResponse(
            ticks=responses[0].ticks,
            series=merged_series,
            metric="memory",
            scope=responses[0].scope,
            split=responses[0].split,
        )

    def describe_detail_context(
        self,
        start_ms: int | None = None,
        end_ms: int | None = None,
        thread_ids: Sequence[int] | None = None,
        focus_thread: int | None = None,
    ) -> dict[str, Any]:
        start_tick, end_tick = self._normalize_window(start_ms, end_ms)
        selected_threads = self._normalize_thread_ids(thread_ids)
        resolved_focus_thread = focus_thread if focus_thread is not None else (selected_threads[0] if selected_threads else None)
        return _detail_context_payload(start_tick, end_tick, selected_threads, resolved_focus_thread)

    def _normalize_thread_ids(self, thread_ids: Sequence[int] | None) -> tuple[int, ...]:
        if not thread_ids:
            return tuple(self._all_thread_ids)
        return tuple(sorted({int(thread_id) for thread_id in thread_ids}))

    def _normalize_metrics(self, metrics: Sequence[str] | None, default: str = "live") -> tuple[str, ...]:
        default_name = _normalize_metric_name(default)
        seen: set[str] = set()
        normalized: list[str] = []
        raw_metrics = metrics if metrics is not None else (default_name,)
        for metric_name in raw_metrics:
            name = _normalize_metric_name(metric_name)
            if name not in METRIC_OPTIONS or name in seen:
                continue
            seen.add(name)
            normalized.append(name)
        if normalized:
            return tuple(normalized)
        return (default_name,)

    def _primary_metric(self, metrics: Sequence[str]) -> str:
        selected = set(metrics)
        for metric_name in ("net", "live", "malloc", "free"):
            if metric_name in selected:
                return metric_name
        return metrics[0] if metrics else "net"

    def _normalize_function_ids(self, function_ids: Sequence[int] | None) -> tuple[int, ...]:
        if not function_ids:
            return tuple()
        max_function_id = len(self.reader.function_names) - 1
        return tuple(
            sorted({int(function_id) for function_id in function_ids if 1 <= int(function_id) <= max_function_id})
        )

    def _normalize_window(self, start_ms: int | None, end_ms: int | None) -> tuple[int, int]:
        default_start, default_end = self.default_window()
        start_tick = default_start if start_ms is None else _align_down(int(start_ms), self.reader.sample_interval_ms)
        end_tick = default_end if end_ms is None else _align_up(int(end_ms), self.reader.sample_interval_ms)
        if end_tick < start_tick:
            start_tick, end_tick = end_tick, start_tick
        return start_tick, end_tick

    def _infer_split(self, split: str, thread_ids: Sequence[int], function_ids: Sequence[int]) -> str:
        if split != "auto":
            return split
        if function_ids:
            if len(function_ids) > 1 and len(thread_ids) <= 1:
                return "function"
            if len(function_ids) == 1 and len(thread_ids) <= 1:
                return "function"
            return "pair"
        if len(thread_ids) > 1:
            return "thread"
        return "total"

    def _narrow_thread_ids_by_functions(
        self, thread_ids: Sequence[int], function_ids: Sequence[int]
    ) -> tuple[int, ...]:
        if not function_ids:
            return tuple(thread_ids)

        selected_functions = set(function_ids)
        narrowed = [
            thread_id
            for thread_id in thread_ids
            if self.reader.thread_function_sets.get(thread_id, frozenset()) & selected_functions
        ]
        return tuple(narrowed)

    def _build_leaf_match_lookup(
        self, function_ids: Sequence[int], scope: str
    ) -> dict[int, tuple[int, ...]]:
        if not function_ids:
            return {}

        postings = (
            self.reader.exclusive_function_postings
            if scope == "exclusive"
            else self.reader.function_stack_postings
        )

        matches: dict[int, list[int]] = defaultdict(list)
        for function_id in function_ids:
            for leaf_node_id in postings.get(function_id, []): 
                matches[leaf_node_id].append(function_id)
        return {leaf_node_id: tuple(matched_functions) for leaf_node_id, matched_functions in matches.items()}

    def _iter_records(
        self,
        start_ms: int,
        end_ms: int,
        thread_ids: Sequence[int] | None,
        leaf_filter: frozenset[int] | None = None,
    ) -> Iterable[tuple[int, int, int, int, int]]:
        thread_filter = None if thread_ids is None else set(thread_ids)
        if thread_filter is not None and not thread_filter:
            return
        for entry, blocks in self.reader.iter_tick_blocks_range(start_ms, end_ms):
            for thread_id, records in blocks:
                if thread_filter is not None and thread_id not in thread_filter:
                    continue
                for leaf_node_id, malloc_bytes, free_bytes in records:
                    if leaf_filter is not None and leaf_node_id not in leaf_filter:
                        continue
                    yield entry.tick_ms, thread_id, leaf_node_id, malloc_bytes, free_bytes

    def _metric_value(self, malloc_bytes: int | float, free_bytes: int | float, metric: str) -> float:
        if metric == "malloc":
            return float(malloc_bytes)
        if metric == "free":
            return float(free_bytes)
        if metric == "net":
            return float(malloc_bytes) - float(free_bytes)
        raise ValueError(f"unsupported metric: {metric}")

    def _series_keys(
        self,
        thread_id: int,
        leaf_node_id: int,
        selected_functions: Sequence[int],
        split: str,
        leaf_match_lookup: dict[int, tuple[int, ...]],
    ) -> list[tuple[Any, ...]]:
        if selected_functions:
            matches = leaf_match_lookup.get(leaf_node_id, ())
            if not matches:
                return []
            if split == "pair":
                return [("pair", thread_id, function_id) for function_id in matches]
            if split == "function":
                return [("function", function_id) for function_id in matches]
            if split == "thread":
                return [("thread", thread_id)]
            return [("total", 0)]
        if split == "thread":
            return [("thread", thread_id)]
        return [("total", 0)]

    def _default_empty_series_key(
        self, split: str, thread_ids: Sequence[int], function_ids: Sequence[int]
    ) -> tuple[Any, ...]:
        if split == "thread" and thread_ids:
            return ("thread", thread_ids[0])
        if split == "function" and function_ids:
            return ("function", function_ids[0])
        if split == "pair" and thread_ids and function_ids:
            return ("pair", thread_ids[0], function_ids[0])
        return ("total", 0)

    def _format_series_key(self, key: tuple[Any, ...]) -> str:
        kind = key[0]
        if kind == "total":
            return "Total"
        if kind == "thread":
            return f"T{key[1]}"
        if kind == "function":
            return self.reader.display_names[key[1]] or self.reader.function_names[key[1]] or str(key[1])
        if kind == "pair":
            display_name = self.reader.display_names[key[2]] or self.reader.function_names[key[2]] or str(key[2])
            return f"T{key[1]}::{display_name}"
        return str(key)

    def _downsample(
        self, ticks: list[int], series: dict[str, list[float]], resolution: int, metric: str
    ) -> tuple[list[int], dict[str, list[float]]]:
        bucket_size = max(1, (len(ticks) + resolution - 1) // resolution)
        sampled_ticks = []
        sampled_series = {label: [] for label in series}
        for index in range(0, len(ticks), bucket_size):
            slice_end = min(index + bucket_size, len(ticks))
            sampled_ticks.append(ticks[slice_end - 1])
            for label, values in series.items():
                chunk = values[index:slice_end]
                if metric == "live":
                    sampled_series[label].append(chunk[-1] if chunk else 0.0)
                else:
                    sampled_series[label].append(sum(chunk))
        return sampled_ticks, sampled_series

    def _new_stack_tree_node(self, function_id: int) -> dict[str, Any]:
        display_name = self.reader.display_names[function_id] or self.reader.function_names[function_id] or str(function_id)
        canonical_name = self.reader.function_names[function_id] or display_name
        return {
            "thread_id": None,
            "function_id": function_id,
            "display_name": display_name,
            "canonical_name": canonical_name,
            "malloc": 0.0,
            "free": 0.0,
            "children": {},
        }

    def _finalize_stack_tree(self, node: dict[str, Any], metric: str) -> dict[str, Any]:
        sort_metric = "net" if metric == "live" else metric
        children = [self._finalize_stack_tree(child, metric) for child in node.get("children", {}).values()]
        if sort_metric == "net":
            children.sort(key=lambda child: (-abs(float(child["value"])), _stack_node_name(child).casefold()))
        else:
            children.sort(key=lambda child: (-float(child["value"]), _stack_node_name(child).casefold()))
        return {
            "thread_id": node.get("thread_id"),
            "function_id": node.get("function_id"),
            "display_name": node.get("display_name"),
            "canonical_name": node.get("canonical_name"),
            "malloc": float(node.get("malloc", 0.0)),
            "free": float(node.get("free", 0.0)),
            "value": self._metric_value(node.get("malloc", 0.0), node.get("free", 0.0), sort_metric),
            "children": children,
            "child_count": len(children),
        }

    def _apply_stack_tree_shares(self, tree: dict[str, Any]) -> dict[str, Any]:
        total_value = float(tree.get("value", 0.0))
        self._annotate_stack_tree_share(tree, total_value)
        return tree

    def _annotate_stack_tree_share(self, node: dict[str, Any], total_value: float) -> None:
        current_value = float(node.get("value", 0.0))
        share_percent = 0.0 if abs(total_value) < 1e-12 else (current_value / total_value) * 100.0
        node["share_percent"] = share_percent
        for child in node.get("children", []):
            self._annotate_stack_tree_share(child, total_value)

    def _stack_tree_to_text(self, tree: dict[str, Any]) -> str:
        lines = [f"{_stack_node_name(tree)} {_format_stack_summary(tree)}"]
        self._render_stack_tree_text(tree, 0, lines)
        return "\n".join(lines)

    def _render_stack_tree_text(self, node: dict[str, Any], depth: int, lines: list[str]) -> None:
        for child in node.get("children", []):
            lines.append(f"{'  ' * depth}- {_stack_node_name(child)} {_format_stack_summary(child)}")
            self._render_stack_tree_text(child, depth + 1, lines)

    def _compatibility_json_data(self) -> dict[str, Any]:
        result: dict[str, Any] = {}
        if self.reader.max_tick_ms == 0 and not self.reader.tick_directory:
            return result

        start_tick = self.reader.default_start_tick()
        for tick_ms in range(start_tick, self.reader.max_tick_ms + self.reader.sample_interval_ms, self.reader.sample_interval_ms):
            result[str(tick_ms)] = {}

        for entry, blocks in self.reader.iter_tick_blocks_range(start_tick, self.reader.max_tick_ms):
            tick_key = str(entry.tick_ms)
            per_tick = result.setdefault(tick_key, {})
            for thread_id, records in blocks:
                root = {"name": "(null)", "malloc": 0, "free": 0, "children": []}
                child_maps: dict[int, dict[str, Any]] = {id(root): {}}
                for leaf_node_id, malloc_bytes, free_bytes in records:
                    root["malloc"] += malloc_bytes
                    root["free"] += free_bytes
                    parent = root
                    for function_id in self.reader.node_paths[leaf_node_id]:
                        mapping = child_maps.setdefault(id(parent), {})
                        key = self.reader.function_names[function_id] or str(function_id)
                        child = mapping.get(key)
                        if child is None:
                            child = {"name": key, "malloc": 0, "free": 0, "children": []}
                            mapping[key] = child
                            parent["children"].append(child)
                        child["malloc"] += malloc_bytes
                        child["free"] += free_bytes
                        parent = child
                per_tick[str(thread_id)] = root
        return result


def generate_snapshot_html(
    title: str,
    meta: dict[str, Any],
    series: SeriesResponse,
    top_rows: Sequence[dict[str, Any]],
    stack_tree: dict[str, Any] | None,
    detail_context: dict[str, Any],
) -> str:
    traces = [
        {
            "x": series.ticks,
            "y": values,
            "mode": "lines+markers",
            "name": label,
        }
        for label, values in series.series.items()
    ]
    top_html = "".join(
        "<tr>"
        f"<td>{html_escape(row['display_name'] or '')}</td>"
        f"<td>{html_escape(_format_value(row['value']))}</td>"
        "</tr>"
        for row in top_rows
    )
    meta_html = (
        f"sample interval {meta['sample_interval_ms']} ms, "
        f"threads {meta['thread_count']}, functions {meta['function_count']}, "
        f"total malloc {_format_bytes(meta['total_malloc_bytes'])}, total free {_format_bytes(meta['total_free_bytes'])}"
    )
    traces_json = _json_for_html_script(traces)
    stack_tree_json = _json_for_html_script(stack_tree)
    title_json = _json_for_html_script(title)
    tick_axis_json = _json_for_html_script(_tick_axis_label())
    metric_axis_json = _json_for_html_script(_metric_axis_label(series.metric))
    detail_context_html = html_escape(f"Context: {detail_context.get('summary', '')}")
    top_heading_html = html_escape(f"Top Function (by {_metric_display_name('net')})")
    return f"""<!DOCTYPE html>
<html lang=\"en\">
<head>
  <meta charset=\"utf-8\" />
  <title>{html_escape(title)}</title>
  <script src=\"https://cdn.plot.ly/plotly-2.30.0.min.js\"></script>
  <style>
    body {{ font-family: sans-serif; margin: 20px; background: #faf8f4; color: #1e1f24; }}
    h1 {{ margin-bottom: 6px; }}
    .meta {{ margin-bottom: 18px; color: #5e616d; }}
    .grid {{ display: grid; grid-template-columns: 2fr 1fr; gap: 20px; }}
    .panel {{ background: white; border: 1px solid #ddd6c9; border-radius: 12px; padding: 14px; box-shadow: 0 8px 24px rgba(0,0,0,0.05); }}
    table {{ width: 100%; border-collapse: collapse; }}
    td, th {{ padding: 6px 8px; border-bottom: 1px solid #eee5d7; text-align: left; }}
    .panel-context {{ margin: 0 0 10px; font-size: 12px; color: #5e616d; }}
        button {{ padding: 6px 10px; border: 1px solid #ccbfa9; border-radius: 10px; background: #fffaf2; cursor: pointer; }}
    #plot {{ width: 100%; height: 560px; }}
        .stack-toolbar {{ display: flex; gap: 8px; margin: 10px 0 8px; }}
        .stack-summary, .stack-selected {{ font-size: 12px; color: #5e616d; margin: 8px 0; }}
        .stack-selected {{ padding-top: 8px; border-top: 1px solid #eee5d7; }}
        .stack-headings {{ display: grid; grid-template-columns: minmax(0, 1fr) 72px 90px 90px 90px; gap: 8px; padding: 0 0 6px; border-bottom: 1px solid #eee5d7; font-size: 11px; text-transform: uppercase; letter-spacing: 0.04em; color: #7c7f89; }}
        .stack-tree {{ max-height: 480px; overflow: auto; background: #fcfbf8; border: 1px solid #eee5d7; border-radius: 10px; padding: 8px; }}
        .stack-node {{ margin: 0; }}
        .stack-node summary {{ cursor: pointer; list-style: none; }}
        .stack-node summary::-webkit-details-marker {{ color: #b3621d; }}
        .stack-children {{ margin-left: 10px; padding-left: 10px; border-left: 1px solid #efe6d6; }}
        .stack-leaf {{ margin: 0; }}
        .stack-row {{ display: grid; grid-template-columns: minmax(0, 1fr) 72px 90px 90px 90px; gap: 8px; align-items: center; padding: 4px 0; font-size: 12px; }}
        .stack-name {{ overflow: hidden; text-overflow: ellipsis; white-space: nowrap; font-family: ui-monospace, SFMono-Regular, Menlo, Consolas, monospace; }}
        .stack-share, .stack-value, .stack-metric {{ text-align: right; font-variant-numeric: tabular-nums; color: #5e616d; }}
        .stack-empty {{ padding: 8px 0; color: #5e616d; font-size: 12px; }}
        @media (max-width: 1200px) {{ .grid {{ grid-template-columns: 1fr; }} }}
  </style>
</head>
<body>
  <h1>{html_escape(title)}</h1>
  <div class=\"meta\">{html_escape(meta_html)}</div>
  <div class=\"grid\">
    <div class=\"panel\">
      <div id=\"plot\"></div>
    </div>
    <div class=\"panel\">
                                                <h3>{top_heading_html}</h3>
                        <div class=\"panel-context\">{detail_context_html}</div>
      <table>
        <thead><tr><th>Function</th><th>Value</th></tr></thead>
        <tbody>{top_html}</tbody>
      </table>
      <h3>Stack</h3>
            <div class=\"stack-toolbar\">
                <button type=\"button\" onclick=\"toggleStackTree('stackTree', true)\">Expand all</button>
                <button type=\"button\" onclick=\"toggleStackTree('stackTree', false)\">Collapse all</button>
            </div>
            <div id=\"stackSummary\" class=\"stack-summary\">Loading stack...</div>
            <div class=\"stack-headings\"><span>Frame</span><span>Share</span><span>Value</span><span>Malloc</span><span>Free</span></div>
            <div id=\"stackTree\" class=\"stack-tree\"></div>
            <div id=\"stackSelected\" class=\"stack-selected\">Click a frame to inspect the full symbol.</div>
    </div>
  </div>
  <script>
        {_stack_panel_script()}
        const traces = {traces_json};
        const snapshotStackTree = {stack_tree_json};
    Plotly.newPlot('plot', traces, {{
            title: {title_json},
            xaxis: {{ title: {tick_axis_json} }},
            yaxis: {{ title: {metric_axis_json} }},
      hovermode: 'closest'
    }}, {{responsive: true}});
        renderStackTree('stackTree', 'stackSummary', 'stackSelected', snapshotStackTree);
  </script>
</body>
</html>
"""


def generate_dynamic_html_page() -> str:
    return '''<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="utf-8" />
    <title>fastgrind viewer</title>
    <script src="https://cdn.plot.ly/plotly-2.30.0.min.js"></script>
    <style>
        body { font-family: sans-serif; margin: 18px; background: linear-gradient(180deg, #faf6ee, #f3efe7); color: #1d1d24; }
        .layout { display: grid; grid-template-columns: 320px minmax(0, 1fr) 400px; gap: 16px; }
        .panel { background: rgba(255,255,255,0.94); border: 1px solid #ddd4c4; border-radius: 14px; padding: 14px; box-shadow: 0 10px 28px rgba(0,0,0,0.06); }
        h2, h3 { margin-top: 0; }
        .controls { display: grid; gap: 10px; }
        select, input, button { width: 100%; box-sizing: border-box; padding: 8px 10px; border: 1px solid #ccbfa9; border-radius: 10px; background: white; }
        select[multiple] { min-height: 180px; }
        .metric-select { min-height: 120px !important; }
        .inline { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; }
        #plot { width: 100%; height: 720px; }
        table { width: 100%; border-collapse: collapse; font-size: 13px; }
        th, td { padding: 6px 8px; border-bottom: 1px solid #efe6d6; text-align: left; }
        .meta { font-size: 13px; color: #5d6170; margin-bottom: 12px; }
        .panel-context { font-size: 12px; color: #5d6170; margin: -4px 0 10px; }
        .stack-toolbar { display: flex; gap: 8px; margin: 10px 0 8px; }
        .stack-summary, .stack-selected { font-size: 12px; color: #5d6170; margin: 8px 0; }
        .stack-selected { padding-top: 8px; border-top: 1px solid #efe6d6; overflow-x: auto; white-space: nowrap; }
        .stack-headings { display: grid; grid-template-columns: minmax(0, 1fr) 72px 88px 88px 88px; gap: 8px; padding: 0 0 6px; border-bottom: 1px solid #efe6d6; font-size: 11px; text-transform: uppercase; letter-spacing: 0.04em; color: #7c7f89; }
        .stack-tree { max-height: 420px; overflow: auto; font-size: 12px; background: #fcfbf8; padding: 10px; border-radius: 10px; border: 1px solid #efe6d6; }
        .stack-node { margin: 0; }
        .stack-node summary { list-style: none; cursor: pointer; }
        .stack-node summary::-webkit-details-marker { color: #b3621d; }
        .stack-children { margin-left: 10px; padding-left: 10px; border-left: 1px solid #efe6d6; }
        .stack-row { display: grid; grid-template-columns: minmax(0, 1fr) 72px 88px 88px 88px; gap: 8px; align-items: center; padding: 4px 0; }
        .stack-name { overflow: hidden; text-overflow: ellipsis; white-space: nowrap; font-family: ui-monospace, SFMono-Regular, Menlo, Consolas, monospace; }
        .stack-share, .stack-value, .stack-metric { text-align: right; font-variant-numeric: tabular-nums; color: #5d6170; }
        .stack-empty { padding: 8px 0; color: #5d6170; }
        @media (max-width: 1280px) { .layout { grid-template-columns: 1fr; } }
    </style>
</head>
<body>
    <h2>fastgrind binary viewer</h2>
    <div id="meta" class="meta">Loading...</div>
    <div class="layout">
        <div class="panel">
            <div class="controls">
                <div>
                    <label>Threads</label>
                    <select id="threadSel" multiple></select>
                </div>
                <div>
                    <label>Functions search</label>
                    <input id="funcSearch" placeholder="Search functions" />
                </div>
                <div>
                    <label>Functions</label>
                    <select id="funcSel" multiple></select>
                </div>
                <div class="inline">
                    <div>
                        <label>Metrics</label>
                        <select id="metricSel" class="metric-select" multiple size="4">
                            <option value="malloc">malloc</option>
                            <option value="free">free</option>
                            <option value="tick_res">tick_res</option>
                            <option value="sum_res" selected>sum_res</option>
                        </select>
                    </div>
                    <div>
                        <label>Scope</label>
                        <select id="scopeSel">
                            <option value="inclusive" selected>inclusive</option>
                            <option value="exclusive">exclusive</option>
                        </select>
                    </div>
                </div>
                <div class="inline">
                    <div>
                        <label>Start</label>
                        <input id="startInput" type="number" />
                    </div>
                    <div>
                        <label>End</label>
                        <input id="endInput" type="number" />
                    </div>
                </div>
                <button id="plotBtn">Plot</button>
            </div>
        </div>
        <div class="panel">
            <div id="plot"></div>
        </div>
        <div class="panel">
            <h3>Top Function (by tick_res)</h3>
            <div id="detailContext" class="panel-context">Context: Loading...</div>
            <table>
                <thead><tr><th>Function</th><th>Value</th></tr></thead>
                <tbody id="topBody"></tbody>
            </table>
            <h3>Stack</h3>
            <div class="stack-toolbar">
                <button type="button" onclick="toggleStackTree('stackTree', true)">Expand all</button>
                <button type="button" onclick="toggleStackTree('stackTree', false)">Collapse all</button>
            </div>
            <div id="stackSummary" class="stack-summary">Loading stack...</div>
            <div class="stack-headings"><span>Frame</span><span>Share</span><span>Value</span><span>Malloc</span><span>Free</span></div>
            <div id="stackTree" class="stack-tree"></div>
            <div id="stackSelected" class="stack-selected">Click a frame to inspect the full symbol.</div>
        </div>
    </div>
    <script>
        function selectedValues(select) {
            return Array.from(select.selectedOptions).map(option => option.value);
        }

        function effectiveThreadValues() {
            const threadSel = document.getElementById('threadSel');
            const selected = selectedValues(threadSel);
            if (selected.length > 0) {
                return selected;
            }
            return Array.from(threadSel.options).map(option => option.value);
        }

        async function fetchJson(url) {
            const response = await fetch(url);
            if (!response.ok) {
                throw new Error(await response.text());
            }
            return await response.json();
        }

        function queryString(params) {
            const search = new URLSearchParams();
            for (const [key, value] of Object.entries(params)) {
                if (value === undefined || value === null || value === '') {
                    continue;
                }
                search.set(key, value);
            }
            return search.toString();
        }

        function formatDetailContext(context) {
            if (!context) {
                return 'Context: unavailable';
            }
            if (typeof context.summary === 'string' && context.summary.length > 0) {
                return `Context: ${context.summary}`;
            }

            const start = Number(context.start_ms);
            const end = Number(context.end_ms);
            const threadIds = Array.isArray(context.thread_ids) ? context.thread_ids : [];
            const focusThread = context.focus_thread;
            const timeLabel = start === end ? `Tick ${start} ms` : `Window ${start}..${end} ms`;

            let topLabel = 'Top aggregates no threads';
            if (threadIds.length === 1) {
                topLabel = `Top aggregates T${threadIds[0]}`;
            } else if (threadIds.length > 1) {
                topLabel = `Top aggregates ${threadIds.length} threads`;
            }

            const stackLabel = focusThread === undefined || focusThread === null
                ? 'Stack has no focus thread'
                : `Stack shows T${focusThread}`;
            return `Context: ${timeLabel}. ${topLabel}. ${stackLabel}.`;
        }

        function setDetailContext(context) {
            const node = document.getElementById('detailContext');
            if (!node) {
                return;
            }
            node.textContent = formatDetailContext(context);
        }

        function formatStackNumber(value) {
            const numericValue = Number(value);
            if (!Number.isFinite(numericValue)) {
                return '0';
            }
            return numericValue.toLocaleString(undefined, { maximumFractionDigits: 2 });
        }

        function formatStackPercent(value) {
            const numericValue = Number(value);
            if (!Number.isFinite(numericValue)) {
                return '0.0%';
            }
            return `${numericValue.toFixed(1)}%`;
        }

        function stackNodeLabel(node) {
            return node.display_name || node.canonical_name || '(unknown)';
        }

        function stackNodeCanonical(node) {
            return node.canonical_name || node.display_name || '(unknown)';
        }

        function stackNodeSummary(node) {
            return `share ${formatStackPercent(node.share_percent)} | value ${formatStackNumber(node.value)} | malloc ${formatStackNumber(node.malloc)} | free ${formatStackNumber(node.free)}`;
        }

        function updateStackSelected(node) {
            document.getElementById('stackSelected').textContent = stackNodeCanonical(node);
        }

        function buildStackRow(node) {
            const row = document.createElement('div');
            row.className = 'stack-row';

            const name = document.createElement('span');
            name.className = 'stack-name';
            name.textContent = stackNodeLabel(node);
            name.title = stackNodeCanonical(node);

            const share = document.createElement('span');
            share.className = 'stack-share';
            share.textContent = formatStackPercent(node.share_percent);

            const value = document.createElement('span');
            value.className = 'stack-value';
            value.textContent = formatStackNumber(node.value);

            const malloc = document.createElement('span');
            malloc.className = 'stack-metric';
            malloc.textContent = formatStackNumber(node.malloc);

            const free = document.createElement('span');
            free.className = 'stack-metric';
            free.textContent = formatStackNumber(node.free);

            row.appendChild(name);
            row.appendChild(share);
            row.appendChild(value);
            row.appendChild(malloc);
            row.appendChild(free);
            row.addEventListener('click', () => updateStackSelected(node));
            return row;
        }

        function createStackNode(node, depth) {
            const children = Array.isArray(node.children) ? node.children : [];
            if (children.length === 0) {
                const leaf = document.createElement('div');
                leaf.className = 'stack-leaf';
                leaf.style.marginLeft = `${depth * 14}px`;
                leaf.appendChild(buildStackRow(node));
                return leaf;
            }

            const details = document.createElement('details');
            details.className = 'stack-node';
            details.open = depth < 1;
            details.style.marginLeft = `${depth * 14}px`;

            const summary = document.createElement('summary');
            summary.appendChild(buildStackRow(node));
            details.appendChild(summary);

            const branch = document.createElement('div');
            branch.className = 'stack-children';
            for (const child of children) {
                branch.appendChild(createStackNode(child, depth + 1));
            }
            details.appendChild(branch);
            return details;
        }

        function renderStackTree(tree) {
            const container = document.getElementById('stackTree');
            const summary = document.getElementById('stackSummary');
            container.innerHTML = '';

            if (!tree) {
                summary.textContent = 'No stack data';
                document.getElementById('stackSelected').textContent = 'No stack data';
                return;
            }

            summary.textContent = `${stackNodeLabel(tree)} | ${stackNodeSummary(tree)}`;
            updateStackSelected(tree);

            const children = Array.isArray(tree.children) ? tree.children : [];
            if (children.length === 0) {
                const empty = document.createElement('div');
                empty.className = 'stack-empty';
                empty.textContent = 'No stack frames in the selected window.';
                container.appendChild(empty);
                return;
            }

            for (const child of children) {
                container.appendChild(createStackNode(child, 0));
            }
        }

        function toggleStackTree(containerId, open) {
            const container = document.getElementById(containerId);
            if (!container) {
                return;
            }
            for (const node of container.querySelectorAll('details')) {
                node.open = open;
            }
        }

        function metricAxisLabel(metric) {
            const displayMetric = metric === 'net' || metric === 'tick_res'
                ? 'tick_res'
                : metric === 'live' || metric === 'sum_res'
                    ? 'sum_res'
                    : metric;
            if (displayMetric === 'memory') {
                return 'Memory (bytes)';
            }
            return `${displayMetric} (bytes)`;
        }

        function selectedMetrics() {
            const metricSel = document.getElementById('metricSel');
            const values = selectedValues(metricSel);
            if (values.length > 0) {
                return values;
            }
            const fallback = Array.from(metricSel.options).find(option => option.value === 'sum_res');
            if (fallback) {
                fallback.selected = true;
                return ['sum_res'];
            }
            return [];
        }

        function focusThreadId() {
            const threadSel = document.getElementById('threadSel');
            const selected = selectedValues(threadSel);
            if (selected.length > 0) {
                return selected[0];
            }
            const first = threadSel.options[0];
            return first ? first.value : '';
        }

        function renderTop(rows) {
            const body = document.getElementById('topBody');
            body.innerHTML = '';
            for (const row of rows) {
                const tr = document.createElement('tr');
                const name = document.createElement('td');
                name.textContent = row.display_name;
                name.title = row.canonical_name;
                const value = document.createElement('td');
                value.textContent = row.value.toLocaleString(undefined, { maximumFractionDigits: 2 });
                tr.appendChild(name);
                tr.appendChild(value);
                body.appendChild(tr);
            }
        }

        async function refreshFunctions() {
            const threadSel = document.getElementById('threadSel');
            const funcSel = document.getElementById('funcSel');
            const search = document.getElementById('funcSearch').value;
            const params = queryString({
                threads: selectedValues(threadSel).join(','),
                q: search,
                limit: '200',
                sort: 'alpha'
            });
            const functions = await fetchJson('/api/functions?' + params);
            const prev = new Set(selectedValues(funcSel));
            funcSel.innerHTML = '';
            for (const row of functions) {
                const option = document.createElement('option');
                option.value = String(row.function_id);
                option.textContent = row.display_name;
                option.title = row.canonical_name;
                if (prev.has(option.value)) {
                    option.selected = true;
                }
                funcSel.appendChild(option);
            }
        }

        async function plotCurrent() {
            const params = queryString({
                threads: selectedValues(document.getElementById('threadSel')).join(','),
                functions: selectedValues(document.getElementById('funcSel')).join(','),
                start: document.getElementById('startInput').value,
                end: document.getElementById('endInput').value,
                metrics: selectedMetrics().join(','),
                scope: document.getElementById('scopeSel').value,
                split: 'auto',
                resolution: '500'
            });
            const payload = await fetchJson('/api/series?' + params);
            const traces = payload.series.map(row => ({
                x: payload.ticks,
                y: row.values,
                mode: 'lines+markers',
                name: row.label,
            }));
            Plotly.newPlot('plot', traces, {
                title: 'Memory vs Tick',
                xaxis: { title: 'Tick (ms)' },
                yaxis: { title: metricAxisLabel(payload.metric) },
                hovermode: 'closest'
            }, { responsive: true });
            await updateWindowDetails(payload.context);
            const plot = document.getElementById('plot');
            plot.on('plotly_click', async event => {
                const tick = event.points[0].x;
                await updateTickDetails(tick);
            });
        }

        async function updateWindowDetails(context) {
            setDetailContext(context);
            const params = queryString({
                threads: selectedValues(document.getElementById('threadSel')).join(','),
                start: document.getElementById('startInput').value,
                end: document.getElementById('endInput').value,
                limit: '12',
                metric: 'tick_res',
                scope: 'exclusive'
            });
            renderTop(await fetchJson('/api/top?' + params));

            const threadId = focusThreadId();
            if (!threadId) {
                renderStackTree(null);
                return;
            }

            const stack = await fetchJson('/api/stack?' + queryString({
                thread: threadId,
                start: document.getElementById('startInput').value,
                end: document.getElementById('endInput').value,
                metric: 'tick_res'
            }));
            renderStackTree(stack.tree);
        }

        async function updateTickDetails(tick) {
            const threadIds = selectedValues(document.getElementById('threadSel'));
            const threadId = focusThreadId();
            setDetailContext({
                start_ms: tick,
                end_ms: tick,
                thread_ids: effectiveThreadValues().map(value => Number(value)),
                focus_thread: threadId ? Number(threadId) : null,
            });
            const params = queryString({
                threads: threadIds.join(','),
                start: tick,
                end: tick,
                limit: '12',
                metric: 'tick_res',
                scope: 'exclusive'
            });
            renderTop(await fetchJson('/api/top?' + params));

            if (!threadId) {
                renderStackTree(null);
                return;
            }

            const stack = await fetchJson('/api/stack?' + queryString({
                thread: threadId,
                start: tick,
                end: tick,
                metric: 'tick_res'
            }));
            renderStackTree(stack.tree);
        }

        async function boot() {
            const meta = await fetchJson('/api/meta');
            document.getElementById('meta').textContent =
                `sample interval ${meta.sample_interval_ms} ms, threads ${meta.thread_count}, functions ${meta.function_count}, ` +
                `total malloc ${meta.total_malloc_bytes} bytes, total free ${meta.total_free_bytes} bytes`;
            document.getElementById('startInput').value = meta.default_start_ms;
            document.getElementById('endInput').value = meta.max_tick_ms;

            const threadRows = await fetchJson('/api/threads');
            const threadSel = document.getElementById('threadSel');
            for (const row of threadRows) {
                const option = document.createElement('option');
                option.value = String(row.thread_id);
                option.textContent = `T${row.thread_id}`;
                threadSel.appendChild(option);
            }

            threadSel.addEventListener('change', refreshFunctions);
            document.getElementById('funcSearch').addEventListener('input', refreshFunctions);
            document.getElementById('plotBtn').addEventListener('click', plotCurrent);

            await refreshFunctions();
            await plotCurrent();
        }

        boot().catch(error => {
            document.getElementById('meta').textContent = error.message;
        });
    </script>
</body>
</html>
'''


class FastgrindHtmlServer:
    def __init__(self, engine: FastgrindQueryEngine, port: int = 0):
        self.engine = engine
        self.port = port
        self.httpd = ThreadingHTTPServer(("127.0.0.1", port), self._make_handler())

    def serve(self, open_browser: bool = True) -> None:
        url = f"http://127.0.0.1:{self.httpd.server_port}/"
        print(f"Serving fastgrind viewer at {url}")
        if open_browser:
            try:
                webbrowser.open(url)
            except Exception:
                pass
        try:
            self.httpd.serve_forever()
        except KeyboardInterrupt:
            pass
        finally:
            self.httpd.server_close()

    def _make_handler(self):
        engine = self.engine
        page = generate_dynamic_html_page()

        class Handler(BaseHTTPRequestHandler):
            def do_GET(self) -> None:  # noqa: N802
                try:
                    self._dispatch()
                except Exception as exc:
                    body = str(exc).encode("utf-8")
                    self.send_response(500)
                    self.send_header("Content-Type", "text/plain; charset=utf-8")
                    self.send_header("Content-Length", str(len(body)))
                    self.end_headers()
                    self.wfile.write(body)

            def log_message(self, format: str, *args: Any) -> None:
                return

            def _dispatch(self) -> None:
                parsed = urllib.parse.urlparse(self.path)
                params = urllib.parse.parse_qs(parsed.query)

                if parsed.path in {"/", "/index.html"}:
                    body = page.encode("utf-8")
                    self.send_response(200)
                    self.send_header("Content-Type", "text/html; charset=utf-8")
                    self.send_header("Content-Length", str(len(body)))
                    self.end_headers()
                    self.wfile.write(body)
                    return

                if parsed.path == "/api/meta":
                    self._write_json(engine.meta())
                    return

                if parsed.path == "/api/threads":
                    self._write_json(engine.list_threads())
                    return

                if parsed.path == "/api/functions":
                    rows = engine.list_functions(
                        thread_ids=_parse_csv_ints(params.get("threads", [""])[0]),
                        query=params.get("q", [""])[0],
                        limit=int(params.get("limit", ["200"])[0]),
                        sort=params.get("sort", ["alpha"])[0],
                        start_ms=int(params["start"][0]) if "start" in params else None,
                        end_ms=int(params["end"][0]) if "end" in params else None,
                    )
                    self._write_json(rows)
                    return

                if parsed.path == "/api/series":
                    thread_ids = _parse_csv_ints(params.get("threads", [""])[0])
                    function_ids = _parse_csv_ints(params.get("functions", [""])[0])
                    start_ms = int(params["start"][0]) if "start" in params else None
                    end_ms = int(params["end"][0]) if "end" in params else None
                    response = engine.query_multi_series(
                        thread_ids=thread_ids,
                        function_ids=function_ids,
                        start_ms=start_ms,
                        end_ms=end_ms,
                        metrics=_parse_csv_strings(params.get("metrics", [params.get("value", ["malloc"])[0]])[0]),
                        scope=params.get("scope", ["inclusive"])[0],
                        split=params.get("split", ["auto"])[0],
                        resolution=int(params["resolution"][0]) if "resolution" in params else None,
                    )
                    self._write_json(
                        {
                            "ticks": response.ticks,
                            "series": [
                                {"label": label, "values": values} for label, values in response.series.items()
                            ],
                            "metric": response.metric,
                            "scope": response.scope,
                            "split": response.split,
                            "context": engine.describe_detail_context(start_ms=start_ms, end_ms=end_ms, thread_ids=thread_ids),
                        }
                    )
                    return

                if parsed.path == "/api/top":
                    rows = engine.top_functions(
                        start_ms=int(params["start"][0]) if "start" in params else None,
                        end_ms=int(params["end"][0]) if "end" in params else None,
                        thread_ids=_parse_csv_ints(params.get("threads", [""])[0]),
                        limit=int(params.get("limit", ["10"])[0]),
                        metric=params.get("metric", ["net"])[0],
                        scope=params.get("scope", ["exclusive"])[0],
                    )
                    self._write_json(rows)
                    return

                if parsed.path == "/api/stack":
                    thread_values = _parse_csv_ints(params.get("thread", [""])[0])
                    if not thread_values:
                        self._write_json({"thread_id": None, "text": "no thread selected", "tree": None})
                        return
                    tree = engine.stack_tree(
                        thread_values[0],
                        start_ms=int(params["start"][0]) if "start" in params else None,
                        end_ms=int(params["end"][0]) if "end" in params else None,
                        metric=params.get("metric", ["net"])[0],
                    )
                    self._write_json(
                        {
                            "thread_id": thread_values[0],
                            "text": engine._stack_tree_to_text(tree),
                            "tree": tree,
                        }
                    )
                    return

                self.send_response(404)
                self.end_headers()

            def _write_json(self, payload: Any) -> None:
                body = _json_response(payload)
                self.send_response(200)
                self.send_header("Content-Type", "application/json; charset=utf-8")
                self.send_header("Content-Length", str(len(body)))
                self.end_headers()
                self.wfile.write(body)

        return Handler


class FastgrindDesktopUI:
    def __init__(self, engine: FastgrindQueryEngine, source_path: Path):
        import tkinter as tk
        from tkinter import filedialog, messagebox, ttk

        import matplotlib

        matplotlib.use("TkAgg")  # type: ignore
        from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg, NavigationToolbar2Tk
        from matplotlib.figure import Figure
        from matplotlib.widgets import SpanSelector

        self.engine = engine
        self.source_path = source_path
        self.tk = tk
        self.ttk = ttk
        self.filedialog = filedialog
        self.messagebox = messagebox
        self.FigureCanvasTkAgg = FigureCanvasTkAgg
        self.NavigationToolbar2Tk = NavigationToolbar2Tk
        self.Figure = Figure
        self.SpanSelector = SpanSelector

        self.root = tk.Tk()
        self.root.title(f"fastgrind UI - {source_path.name}")
        self.root.geometry("1540x920")

        self.executor = ThreadPoolExecutor(max_workers=1)
        self.pending: tuple[str, Future[Any], Any] | None = None
        self.thread_ids: list[int] = [row["thread_id"] for row in engine.list_threads()]
        self.function_options: list[dict[str, Any]] = []
        self.function_index_by_id: dict[int, int] = {}
        self.top_function_ids: dict[str, int] = {}
        self.stack_item_nodes: dict[str, dict[str, Any]] = {}
        self.stack_column_widths: dict[str, int] = {
            "#0": 320,
            "share": 74,
            "value": 90,
            "malloc": 90,
            "free": 90,
        }
        self.stack_column_minwidths: dict[str, int] = {
            "#0": 180,
            "share": 56,
            "value": 72,
            "malloc": 72,
            "free": 72,
        }
        self.stack_columns_user_resized = False

        self.metric_vars = {
            metric_name: tk.BooleanVar(value=(metric_name == "live")) for metric_name in METRIC_OPTIONS
        }
        self.scope_var = tk.StringVar(value="inclusive")
        self.search_var = tk.StringVar(value="")
        default_start, default_end = engine.default_window()
        self.start_var = tk.StringVar(value=str(default_start))
        self.end_var = tk.StringVar(value=str(default_end))
        self.detail_context_var = tk.StringVar(value="Context: Loading...")
        self.stack_summary_var = tk.StringVar(value="No stack data loaded")
        self.stack_selected_var = tk.StringVar(value="Select a stack frame to inspect the full symbol")
        self.status_var = tk.StringVar(value="Ready")

        self.figure = Figure(figsize=(9.6, 7.2), dpi=100)
        self.overview_ax = self.figure.add_subplot(211)
        self.detail_ax = self.figure.add_subplot(212)

        self._build_layout()
        self._refresh_functions()
        self._draw_initial_overview()
        self.root.protocol("WM_DELETE_WINDOW", self._on_close)

    def run(self) -> None:
        self.root.mainloop()

    def _build_layout(self) -> None:
        root = self.root
        main = self.ttk.Frame(root)
        main.pack(fill=self.tk.BOTH, expand=True, padx=8, pady=8)

        left = self.ttk.Frame(main)
        left.pack(side=self.tk.LEFT, fill=self.tk.Y)

        center = self.ttk.Frame(main)
        center.pack(side=self.tk.LEFT, fill=self.tk.BOTH, expand=True, padx=10)

        right = self.ttk.Frame(main)
        right.pack(side=self.tk.LEFT, fill=self.tk.BOTH)

        self.ttk.Label(left, text="Threads").pack(anchor="w")
        self.thread_list = self.tk.Listbox(left, selectmode=self.tk.EXTENDED, exportselection=False, height=18, width=24)
        self.thread_list.pack(fill=self.tk.X)
        for thread_id in self.thread_ids:
            self.thread_list.insert(self.tk.END, str(thread_id))
        self.thread_list.bind("<<ListboxSelect>>", lambda _event: self._refresh_functions())

        self.ttk.Label(left, text="Function search").pack(anchor="w", pady=(10, 0))
        search_entry = self.ttk.Entry(left, textvariable=self.search_var)
        search_entry.pack(fill=self.tk.X)
        search_entry.bind("<KeyRelease>", lambda _event: self._refresh_functions())

        self.ttk.Label(left, text="Functions").pack(anchor="w", pady=(10, 0))
        self.function_list = self.tk.Listbox(left, selectmode=self.tk.EXTENDED, exportselection=False, height=26, width=40)
        self.function_list.pack(fill=self.tk.BOTH, expand=True)

        controls = self.ttk.Frame(center)
        controls.pack(fill=self.tk.X)
        self.ttk.Label(controls, text="Metrics").grid(row=0, column=0, sticky="w")
        metric_frame = self.ttk.Frame(controls)
        metric_frame.grid(row=1, column=0, sticky="w", padx=(0, 8))
        for index, metric_name in enumerate(METRIC_OPTIONS):
            self.ttk.Checkbutton(
                metric_frame,
                text=_metric_display_name(metric_name),
                variable=self.metric_vars[metric_name],
                command=lambda changed_metric=metric_name: self._on_metric_toggle(changed_metric),
            ).grid(row=index // 2, column=index % 2, sticky="w", padx=(0, 10))

        self.ttk.Label(controls, text="Scope").grid(row=0, column=1, sticky="w")
        self.ttk.Combobox(controls, textvariable=self.scope_var, values=["inclusive", "exclusive"], state="readonly", width=12).grid(row=1, column=1, sticky="we", padx=(0, 8))

        self.ttk.Label(controls, text="Start").grid(row=0, column=2, sticky="w")
        self.ttk.Entry(controls, textvariable=self.start_var, width=12).grid(row=1, column=2, sticky="we", padx=(0, 8))

        self.ttk.Label(controls, text="End").grid(row=0, column=3, sticky="w")
        self.ttk.Entry(controls, textvariable=self.end_var, width=12).grid(row=1, column=3, sticky="we", padx=(0, 8))

        self.ttk.Button(controls, text="Plot", command=self._plot_current).grid(row=1, column=4, sticky="we", padx=(0, 8))
        self.ttk.Button(controls, text="Reset Window", command=self._reset_window).grid(row=1, column=5, sticky="we", padx=(0, 8))
        self.ttk.Button(controls, text="Save PNG", command=self._save_png).grid(row=1, column=6, sticky="we", padx=(0, 8))
        self.ttk.Button(controls, text="Save HTML", command=self._save_html).grid(row=1, column=7, sticky="we")

        canvas_frame = self.ttk.Frame(center)
        canvas_frame.pack(fill=self.tk.BOTH, expand=True, pady=(8, 0))
        self.canvas = self.FigureCanvasTkAgg(self.figure, master=canvas_frame)
        self.canvas.get_tk_widget().pack(fill=self.tk.BOTH, expand=True)
        toolbar = self.NavigationToolbar2Tk(self.canvas, canvas_frame)
        toolbar.update()

        self.canvas.mpl_connect("button_press_event", self._on_plot_click)
        self.span_selector = self.SpanSelector(
            self.overview_ax,
            self._on_span_select,
            "horizontal",
            useblit=True,
            props=dict(alpha=0.2, facecolor="#d46a1f"),
        )

        self.ttk.Label(right, text=f"Top Function (by {_metric_display_name('net')})").pack(anchor="w")
        self.ttk.Label(right, textvariable=self.detail_context_var, justify="left", wraplength=380).pack(fill=self.tk.X, pady=(2, 8))
        self.top_tree = self.ttk.Treeview(right, columns=("value",), show="tree headings", height=20)
        self.top_tree.heading("#0", text="Function")
        self.top_tree.heading("value", text="Value")
        self.top_tree.column("#0", width=240)
        self.top_tree.column("value", width=90, anchor="e")
        self.top_tree.pack(fill=self.tk.X)
        self.top_tree.bind("<Double-1>", self._select_top_function)

        self.ttk.Label(right, text="Stack").pack(anchor="w", pady=(10, 0))
        self.ttk.Label(right, textvariable=self.stack_summary_var, justify="left", wraplength=380).pack(fill=self.tk.X, pady=(4, 6))

        stack_toolbar = self.ttk.Frame(right)
        stack_toolbar.pack(fill=self.tk.X, pady=(0, 6))
        self.ttk.Button(stack_toolbar, text="Expand All", command=lambda: self._set_stack_tree_open(True)).pack(side=self.tk.LEFT)
        self.ttk.Button(stack_toolbar, text="Collapse All", command=lambda: self._set_stack_tree_open(False)).pack(side=self.tk.LEFT, padx=(6, 0))

        stack_frame = self.ttk.Frame(right)
        stack_frame.pack(fill=self.tk.BOTH, expand=True)
        self.stack_tree = self.ttk.Treeview(
            stack_frame,
            columns=("share", "value", "malloc", "free"),
            show="tree headings",
            height=24,
            selectmode="browse",
        )
        self.stack_tree.heading("#0", text="Frame")
        self.stack_tree.heading("share", text="Share")
        self.stack_tree.heading("value", text="Value")
        self.stack_tree.heading("malloc", text="Malloc")
        self.stack_tree.heading("free", text="Free")
        self._configure_stack_tree_columns()

        stack_y_scroll = self.ttk.Scrollbar(stack_frame, orient="vertical", command=self.stack_tree.yview)
        stack_x_scroll = self.ttk.Scrollbar(stack_frame, orient="horizontal", command=self.stack_tree.xview)
        self.stack_tree.configure(yscrollcommand=stack_y_scroll.set, xscrollcommand=stack_x_scroll.set)
        self.stack_tree.grid(row=0, column=0, sticky="nsew")
        stack_y_scroll.grid(row=0, column=1, sticky="ns")
        stack_x_scroll.grid(row=1, column=0, sticky="ew")
        stack_frame.columnconfigure(0, weight=1)
        stack_frame.rowconfigure(0, weight=1)
        self.stack_tree.bind("<<TreeviewSelect>>", self._on_stack_tree_select)
        self.stack_tree.bind("<ButtonRelease-1>", self._on_stack_tree_resize_release, add="+")

        self.ttk.Label(right, text="Full Symbol").pack(anchor="w", pady=(8, 0))
        self.stack_selected_entry = self.ttk.Entry(right, textvariable=self.stack_selected_var, state="readonly")
        self.stack_selected_entry.pack(fill=self.tk.X)

        self.ttk.Label(root, textvariable=self.status_var, anchor="w").pack(fill=self.tk.X, padx=8, pady=(0, 8))

    def _on_close(self) -> None:
        try:
            self.executor.shutdown(wait=False, cancel_futures=True)
        except Exception:
            pass
        self.root.destroy()

    def _selected_threads(self) -> list[int]:
        indices = self.thread_list.curselection()
        if not indices:
            return list(self.thread_ids)
        return [int(self.thread_list.get(index)) for index in indices]

    def _selected_functions(self) -> list[int]:
        indices = self.function_list.curselection()
        return [self.function_options[index]["function_id"] for index in indices]

    def _selected_metrics(self) -> list[str]:
        return [metric_name for metric_name in METRIC_OPTIONS if self.metric_vars[metric_name].get()]

    def _on_metric_toggle(self, changed_metric: str) -> None:
        if self._selected_metrics():
            self._plot_current()
            return
        self.metric_vars[changed_metric].set(True)
        self.status_var.set("At least one metric must remain selected")

    def _refresh_functions(self) -> None:
        selected = set(self._selected_functions())
        self.function_options = self.engine.list_functions(
            thread_ids=self._selected_threads(),
            query=self.search_var.get(),
            limit=200,
            sort="alpha",
        )
        self.function_list.delete(0, self.tk.END)
        self.function_index_by_id.clear()
        for index, row in enumerate(self.function_options):
            self.function_index_by_id[row["function_id"]] = index
            self.function_list.insert(self.tk.END, row["display_name"])
            if row["function_id"] in selected:
                self.function_list.selection_set(index)

    def _draw_initial_overview(self) -> None:
        response = self._build_overview_response(self._selected_metrics())
        self._render_overview(response)
        self._plot_current()

    def _configure_stack_tree_columns(self) -> None:
        frame_stretch = not self.stack_columns_user_resized
        self.stack_tree.column(
            "#0",
            width=self.stack_column_widths["#0"],
            minwidth=self.stack_column_minwidths["#0"],
            stretch=frame_stretch,
        )
        self.stack_tree.column(
            "share",
            width=self.stack_column_widths["share"],
            minwidth=self.stack_column_minwidths["share"],
            anchor="e",
            stretch=False,
        )
        self.stack_tree.column(
            "value",
            width=self.stack_column_widths["value"],
            minwidth=self.stack_column_minwidths["value"],
            anchor="e",
            stretch=False,
        )
        self.stack_tree.column(
            "malloc",
            width=self.stack_column_widths["malloc"],
            minwidth=self.stack_column_minwidths["malloc"],
            anchor="e",
            stretch=False,
        )
        self.stack_tree.column(
            "free",
            width=self.stack_column_widths["free"],
            minwidth=self.stack_column_minwidths["free"],
            anchor="e",
            stretch=False,
        )

    def _on_stack_tree_resize_release(self, _event: Any) -> None:
        current_widths = {
            column_id: int(self.stack_tree.column(column_id, option="width"))
            for column_id in self.stack_column_widths
        }
        if current_widths == self.stack_column_widths:
            return
        self.stack_column_widths = current_widths
        self.stack_columns_user_resized = True
        self._configure_stack_tree_columns()

    def _build_overview_response(self, metrics: Sequence[str]) -> SeriesResponse:
        return self.engine.query_multi_series(
            metrics=metrics,
            scope="inclusive",
            split="total",
            resolution=320,
        )

    def _render_overview(self, response: SeriesResponse) -> None:
        self.overview_ax.clear()
        for label, values in response.series.items():
            self.overview_ax.plot(response.ticks, values, label=label)
        self.overview_ax.set_title("overview(all thread sum)")
        self.overview_ax.set_ylabel(_metric_axis_label(response.metric))
        self.overview_ax.grid(True, linestyle="--", alpha=0.25)
        if 1 < len(response.series) <= 12:
            self.overview_ax.legend(loc="upper right", fontsize="small")
        self.canvas.draw_idle()

    def _plot_current(self) -> None:
        try:
            start_ms = int(self.start_var.get())
            end_ms = int(self.end_var.get())
        except ValueError:
            self.messagebox.showerror("Invalid window", "Start and end must be integers")
            return

        selected_metrics = self._selected_metrics()
        if not selected_metrics:
            self.messagebox.showerror("Invalid metrics", "Select at least one metric")
            return

        self._submit_job(
            "plot",
            lambda: {
                "overview": self._build_overview_response(selected_metrics),
                "detail": self.engine.query_bundle(
                    thread_ids=self._selected_threads(),
                    function_ids=self._selected_functions(),
                    start_ms=start_ms,
                    end_ms=end_ms,
                    metrics=selected_metrics,
                    scope=self.scope_var.get(),
                    resolution=600,
                ),
            },
            self._render_plot_bundle,
        )

    def _render_plot_bundle(self, payload: dict[str, Any]) -> None:
        self._render_overview(payload["overview"])
        self._render_detail_bundle(payload["detail"])

    def _render_detail_bundle(self, payload: dict[str, Any]) -> None:
        response: SeriesResponse = payload["series"]
        self.detail_ax.clear()
        for label, values in response.series.items():
            self.detail_ax.plot(response.ticks, values, marker="o", label=label)
        self.detail_ax.set_title("Detail")
        self.detail_ax.set_xlabel(_tick_axis_label())
        self.detail_ax.set_ylabel(_metric_axis_label(response.metric))
        self.detail_ax.grid(True, linestyle="--", alpha=0.25)
        if len(response.series) <= 12:
            self.detail_ax.legend(loc="upper center", bbox_to_anchor=(0.5, 1.16), ncol=3, fontsize="small")

        self.top_tree.delete(*self.top_tree.get_children())
        self.top_function_ids.clear()
        for row in payload["top"]:
            item = self.top_tree.insert("", "end", text=row["display_name"], values=(_format_value(row["value"]),))
            self.top_function_ids[item] = row["function_id"]

        self.detail_context_var.set(f"Context: {payload['context']['summary']}")
        self._render_stack_tree_panel(payload.get("stack_tree"))
        self.status_var.set(
            f"Rendered {len(response.series)} series across {len(response.ticks)} ticks for {', '.join(payload.get('selected_metrics', []))}"
        )
        self.canvas.draw_idle()

    def _on_span_select(self, xmin: float, xmax: float) -> None:
        start_tick = _align_down(int(min(xmin, xmax)), self.engine.reader.sample_interval_ms)
        end_tick = _align_up(int(max(xmin, xmax)), self.engine.reader.sample_interval_ms)
        self.start_var.set(str(start_tick))
        self.end_var.set(str(end_tick))
        self._plot_current()

    def _reset_window(self) -> None:
        start_tick, end_tick = self.engine.default_window()
        self.start_var.set(str(start_tick))
        self.end_var.set(str(end_tick))
        self._plot_current()

    def _on_plot_click(self, event: Any) -> None:
        if event.inaxes != self.detail_ax or event.xdata is None:
            return
        tick = _align_down(int(event.xdata), self.engine.reader.sample_interval_ms)
        selected_threads = self._selected_threads()
        if not selected_threads:
            return

        self._submit_job(
            "tick",
            lambda: {
                "top": self.engine.top_functions(
                    start_ms=tick,
                    end_ms=tick,
                    thread_ids=selected_threads,
                    limit=12,
                    metric="net",
                    scope="exclusive",
                ),
                "stack_tree": self.engine.stack_tree(selected_threads[0], start_ms=tick, end_ms=tick, metric="net"),
                "context": self.engine.describe_detail_context(
                    start_ms=tick,
                    end_ms=tick,
                    thread_ids=selected_threads,
                    focus_thread=selected_threads[0],
                ),
                "tick": tick,
            },
            self._render_tick_details,
        )

    def _render_tick_details(self, payload: dict[str, Any]) -> None:
        self.top_tree.delete(*self.top_tree.get_children())
        self.top_function_ids.clear()
        for row in payload["top"]:
            item = self.top_tree.insert("", "end", text=row["display_name"], values=(_format_value(row["value"]),))
            self.top_function_ids[item] = row["function_id"]
        self.detail_context_var.set(f"Context: {payload['context']['summary']}")
        self._render_stack_tree_panel(payload.get("stack_tree"))
        self.status_var.set(f"Inspected tick {payload['tick']}")

    def _select_top_function(self, _event: Any) -> None:
        item = self.top_tree.focus()
        function_id = self.top_function_ids.get(item)
        if function_id is None:
            return
        index = self.function_index_by_id.get(function_id)
        if index is None:
            return
        self.function_list.selection_clear(0, self.tk.END)
        self.function_list.selection_set(index)
        self.function_list.see(index)
        self._plot_current()

    def _render_stack_tree_panel(self, tree: dict[str, Any] | None) -> None:
        self.stack_tree.delete(*self.stack_tree.get_children())
        self.stack_item_nodes.clear()

        if not tree:
            self.stack_summary_var.set("No stack data")
            self.stack_selected_var.set("No stack data")
            return

        self.stack_summary_var.set(f"{_stack_node_name(tree)} | {_format_stack_summary(tree)}")
        self.stack_selected_var.set(tree.get("canonical_name") or tree.get("display_name") or "")

        children = tree.get("children", [])
        if not children:
            self.stack_tree.insert("", "end", text="(no stack frames)", values=("0.0%", "0", "0", "0"))
            return

        for child in children:
            self._insert_stack_tree_node("", child, open_state=True)

        roots = self.stack_tree.get_children("")
        if roots:
            self.stack_tree.selection_set(roots[0])
            self.stack_tree.focus(roots[0])
            self._on_stack_tree_select(None)

    def _insert_stack_tree_node(self, parent: str, node: dict[str, Any], open_state: bool = False) -> None:
        item = self.stack_tree.insert(
            parent,
            "end",
            text=_stack_node_name(node),
            values=(
                _format_percent(float(node.get("share_percent", 0.0))),
                _format_value(float(node.get("value", 0.0))),
                _format_value(float(node.get("malloc", 0.0))),
                _format_value(float(node.get("free", 0.0))),
            ),
            open=open_state,
        )
        self.stack_item_nodes[item] = node
        for child in node.get("children", []):
            self._insert_stack_tree_node(item, child, open_state=False)

    def _on_stack_tree_select(self, _event: Any) -> None:
        selection = self.stack_tree.selection()
        if not selection:
            return
        node = self.stack_item_nodes.get(selection[0])
        if not node:
            return
        self.stack_selected_var.set(node.get("canonical_name") or node.get("display_name") or "")

    def _set_stack_tree_open(self, open_state: bool) -> None:
        for item in self.stack_tree.get_children(""):
            self._set_stack_branch_open(item, open_state)

    def _set_stack_branch_open(self, item: str, open_state: bool) -> None:
        self.stack_tree.item(item, open=open_state)
        for child in self.stack_tree.get_children(item):
            self._set_stack_branch_open(child, open_state)

    def _save_png(self) -> None:
        output = self.filedialog.asksaveasfilename(
            title="Save plot image",
            defaultextension=".png",
            filetypes=[("PNG image", "*.png")],
            initialfile=self.source_path.with_suffix(".png").name,
        )
        if not output:
            return
        self.figure.savefig(output, bbox_inches="tight")
        self.status_var.set(f"Saved {output}")

    def _save_html(self) -> None:
        output = self.filedialog.asksaveasfilename(
            title="Save HTML snapshot",
            defaultextension=".html",
            filetypes=[("HTML", "*.html")],
            initialfile=self.source_path.with_suffix(".html").name,
        )
        if not output:
            return
        self.engine.export_html_snapshot(
            output,
            start_ms=int(self.start_var.get()),
            end_ms=int(self.end_var.get()),
            thread_ids=self._selected_threads(),
            function_ids=self._selected_functions(),
            metrics=self._selected_metrics(),
            scope=self.scope_var.get(),
        )
        self.status_var.set(f"Saved {output}")

    def _submit_job(self, label: str, work: Any, callback: Any) -> None:
        if self.pending and not self.pending[1].done():
            self.status_var.set(f"Busy: waiting for {self.pending[0]}")
            return
        self.status_var.set(f"Running {label}...")
        future = self.executor.submit(work)
        self.pending = (label, future, callback)
        self.root.after(50, self._poll_pending)

    def _poll_pending(self) -> None:
        if not self.pending:
            return
        label, future, callback = self.pending
        if not future.done():
            self.root.after(50, self._poll_pending)
            return

        self.pending = None
        try:
            result = future.result()
        except Exception as exc:
            self.status_var.set(f"{label} failed")
            self.messagebox.showerror("fastgrind UI", str(exc))
            return

        callback(result)


def inspect_command(engine: FastgrindQueryEngine) -> None:
    meta = engine.meta()
    print(f"Trace: {meta['path']}")
    print(f"Sample interval: {meta['sample_interval_ms']} ms")
    print(f"Default window: {meta['default_start_ms']}..{meta['max_tick_ms']} ms")
    print(f"Functions: {meta['function_count']}")
    print(f"Stack nodes: {meta['stack_node_count']}")
    print(f"Threads: {meta['thread_count']}")
    print(f"Non-empty ticks: {meta['tick_count']}")
    print(f"Total malloc: {_format_bytes(meta['total_malloc_bytes'])}")
    print(f"Total free: {_format_bytes(meta['total_free_bytes'])}")
    print()
    for thread in engine.list_threads():
        print(
            f"T{thread['thread_id']}: {thread['first_tick_ms']}..{thread['last_tick_ms']} ms, "
            f"malloc {_format_bytes(thread['total_malloc_bytes'])}, free {_format_bytes(thread['total_free_bytes'])}"
        )


def desktop_ui_unavailable_reason() -> str | None:
    if not os.environ.get("DISPLAY") and sys.platform != "win32":
        return "no DISPLAY detected"

    try:
        import tkinter  # noqa: F401
    except Exception as exc:
        return f"tkinter unavailable: {exc}"

    try:
        import matplotlib

        matplotlib.use("TkAgg")
        from matplotlib.backends import backend_tkagg  # noqa: F401
    except Exception as exc:
        return f"TkAgg backend unavailable: {exc}"

    return None


def html_command(engine: FastgrindQueryEngine, port: int, open_browser: bool = True) -> None:
    FastgrindHtmlServer(engine, port=port).serve(open_browser=open_browser)


def ui_command(engine: FastgrindQueryEngine, path: Path, open_browser: bool = True, fallback_port: int = 0) -> None:
    reason = desktop_ui_unavailable_reason()
    if reason:
        print(f"Python UI unavailable ({reason}). Falling back to html mode.")
        html_command(engine, port=fallback_port, open_browser=open_browser)
        return

    try:
        FastgrindDesktopUI(engine, path).run()
    except Exception as exc:
        print(f"Python UI unavailable ({exc}). Falling back to html mode.")
        html_command(engine, port=fallback_port, open_browser=open_browser)


def export_json_command(engine: FastgrindQueryEngine, input_path: Path, output: str | None) -> None:
    output_path = Path(output) if output else input_path.with_suffix(".json")
    engine.export_json(output_path)
    print(f"JSON written: {output_path}")


def export_html_command(engine: FastgrindQueryEngine, input_path: Path, output: str | None) -> None:
    output_path = Path(output) if output else input_path.with_suffix(".html")
    engine.export_html_snapshot(output_path)
    print(f"HTML written: {output_path}")


def default_trace_path() -> Path:
    candidate = Path("fastgrind.fgb")
    if candidate.is_file():
        return candidate
    raise FileNotFoundError("File not found: fastgrind.fgb")


def resolve_input_path(raw: str | None) -> Path:
    if raw:
        return Path(raw)
    return default_trace_path()


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="fastgrind binary trace tooling")
    subparsers = parser.add_subparsers(dest="command")

    inspect_parser = subparsers.add_parser("inspect", help="Inspect a fastgrind binary trace")
    inspect_parser.add_argument("path", nargs="?", help="Path to fastgrind.fgb")

    ui_parser = subparsers.add_parser("ui", help="Launch the Python-first UI")
    ui_parser.add_argument("path", nargs="?", help="Path to fastgrind.fgb")
    ui_parser.add_argument("--no-browser", action="store_true", help="Do not open a browser when ui falls back to html mode")
    ui_parser.add_argument("--port", type=int, default=0, help="Fallback HTTP port when ui degrades to html mode")

    html_parser = subparsers.add_parser("html", help="Serve the browser viewer")
    html_parser.add_argument("path", nargs="?", help="Path to fastgrind.fgb")
    html_parser.add_argument("--port", type=int, default=0, help="HTTP port, 0 for an ephemeral port")
    html_parser.add_argument("--no-browser", action="store_true", help="Serve without opening a browser")

    export_html_parser = subparsers.add_parser("export-html", help="Write a compact HTML snapshot")
    export_html_parser.add_argument("path", nargs="?", help="Path to fastgrind.fgb")
    export_html_parser.add_argument("-o", "--output", help="Output HTML path")

    export_json_parser = subparsers.add_parser("export-json", help="Regenerate compatibility JSON")
    export_json_parser.add_argument("path", nargs="?", help="Path to fastgrind.fgb")
    export_json_parser.add_argument("-o", "--output", help="Output JSON path")

    return parser


def normalize_argv(argv: Sequence[str]) -> list[str]:
    commands = {"inspect", "ui", "html", "export-html", "export-json"}
    if len(argv) == 1:
        return [argv[0], "ui"]
    if argv[1] in commands:
        return list(argv)
    return [argv[0], "ui", *argv[1:]]


def main(argv: Sequence[str]) -> int:
    parser = build_parser()
    args = parser.parse_args(normalize_argv(argv)[1:])

    try:
        input_path = resolve_input_path(getattr(args, "path", None))
        reader = FastgrindBinaryReader(input_path)
    except Exception as exc:
        print(str(exc), file=sys.stderr)
        return 1

    engine = FastgrindQueryEngine(reader)
    try:
        if args.command == "inspect":
            inspect_command(engine)
        elif args.command == "ui":
            ui_command(engine, input_path, open_browser=not args.no_browser, fallback_port=args.port)
        elif args.command == "html":
            html_command(engine, args.port, open_browser=not args.no_browser)
        elif args.command == "export-html":
            export_html_command(engine, input_path, args.output)
        elif args.command == "export-json":
            export_json_command(engine, input_path, args.output)
        else:
            parser.error(f"unknown command: {args.command}")
    finally:
        reader.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))