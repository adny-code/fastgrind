#!/usr/bin/env python3

import json
import re
import shlex
import sys


HIGH_RISK_RULES = [
    (re.compile(r"(^|[;&|])\s*(sudo|doas|su)\b", re.IGNORECASE), "Privileged commands require confirmation."),
    (re.compile(r"\brm\b[^\n]*\s-(?:[^\n]*r[^\n]*f|[^\n]*f[^\n]*r)", re.IGNORECASE), "Recursive force deletion requires confirmation."),
    (re.compile(r"\bgit\s+reset\s+--hard\b", re.IGNORECASE), "Hard reset requires confirmation."),
    (re.compile(r"\bgit\s+checkout\s+--(?:\s|$)", re.IGNORECASE), "Discarding file changes requires confirmation."),
    (re.compile(r"\bgit\s+clean\s+-[^\n]*(?:f[^\n]*d|d[^\n]*f)", re.IGNORECASE), "Git clean requires confirmation."),
    (re.compile(r"\b(curl|wget)\b[^\n|]*\|\s*(sh|bash)\b", re.IGNORECASE), "Piped remote shell execution requires confirmation."),
    (re.compile(r"\b(apt|apt-get|yum|dnf|pacman|zypper|brew)\b", re.IGNORECASE), "Package-manager changes require confirmation."),
    (re.compile(r"\b(pip|pip3|npm|pnpm|yarn|cargo)\s+(install|add|remove|uninstall|publish)\b", re.IGNORECASE), "Package installation or publishing requires confirmation."),
    (re.compile(r"\bpython3?\s+-m\s+pip\s+install\b", re.IGNORECASE), "Python package installation requires confirmation."),
    (re.compile(r"\b(systemctl|service|mount|umount|chown|kubectl|docker)\b", re.IGNORECASE), "System or container management requires confirmation."),
    (re.compile(r"\b(mkfs(?:\.[A-Za-z0-9_+-]+)?|fdisk|parted|shutdown|reboot|poweroff|halt|dd)\b", re.IGNORECASE), "System-destructive commands require confirmation."),
    (re.compile(r":\(\)\s*\{\s*:\s*\|\s*:\s*&\s*;\s*\}\s*;\s*:", re.IGNORECASE), "Fork bombs require confirmation."),
]

COMMON_BASH_ROOTS = {
    "[",
    "bash",
    "cat",
    "cd",
    "clear",
    "date",
    "echo",
    "env",
    "export",
    "false",
    "head",
    "id",
    "ls",
    "mkdir",
    "nproc",
    "pwd",
    "printf",
    "pushd",
    "popd",
    "readlink",
    "realpath",
    "set",
    "sh",
    "stat",
    "tail",
    "test",
    "touch",
    "true",
    "type",
    "uname",
    "wc",
    "which",
    "whoami",
}
COMMON_INSPECTION_ROOTS = {
    "awk",
    "command",
    "cut",
    "file",
    "find",
    "grep",
    "rg",
    "sed",
    "sort",
    "strings",
    "tree",
    "tr",
    "uniq",
    "xargs",
}
COMMON_BUILD_ROOTS = {
    "ar",
    "c++",
    "cc",
    "clang",
    "clang++",
    "cmake",
    "ctest",
    "g++",
    "gcc",
    "ld",
    "make",
    "meson",
    "ninja",
    "nm",
    "objdump",
    "pkg-config",
    "python",
    "python3",
    "ranlib",
    "readelf",
    "strip",
    "valgrind",
    "wine",
    "wine64",
}
SAFE_GIT_SUBCOMMANDS = {
    "blame",
    "describe",
    "diff",
    "log",
    "rev-parse",
    "show",
    "status",
}
SAFE_GIT_BRANCH_FLAGS = {"--all", "--list", "--show-current", "-a", "-vv"}
SAFE_RELATIVE_PREFIXES = (
    "./build/",
    "build/",
    "./demo/",
    "demo/",
    "./testcase/",
    "testcase/",
    "./tools/",
    "tools/",
)
SAFE_SCRIPT_SUFFIXES = (".py", ".sh")
SAFE_VARIABLE_PROBE_FLAGS = {"--help", "--version", "-h", "-V", "-v"}
CONTROL_FLOW_HEADER_KEYWORDS = {"for", "select", "case"}
CONTROL_FLOW_CONDITION_KEYWORDS = {"if", "elif", "while", "until"}
CONTROL_FLOW_PREFIX_KEYWORDS = {"do", "then", "else"}
CONTROL_FLOW_CLOSING_KEYWORDS = {"done", "fi", "esac"}
ASSIGNMENT_RE = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*=.*$")
COMPILER_ROOT_RE = re.compile(
    r"^(?:[A-Za-z0-9_+.-]+-)?(?:ar|c\+\+|cc|clang|clang\+\+|g\+\+|gcc|ld|nm|objdump|ranlib|readelf|strip)$"
)
CONTROL_FLOW_PREFIX_RE = re.compile(r"^\s*(?:(?:do|then|else)\b\s*)+", re.IGNORECASE)
LEADING_DIRECTORY_RE = re.compile(r"^\s*(?:cd|pushd)\s+[^;&|]+(?:\s*(?:&&|;)\s*)", re.IGNORECASE)
SEGMENT_SPLIT_RE = re.compile(r"(?:&&|\|\||;|\|)")
VARIABLE_COMMAND_RE = re.compile(r"^\$(?:[A-Za-z_][A-Za-z0-9_]*|\{[A-Za-z_][A-Za-z0-9_]*\})$")


def normalize_tool_name(raw_name: object) -> str:
    name = str(raw_name or "")
    return name.rsplit(".", 1)[-1]


def pretool_output(decision: str, reason: str) -> dict:
    return {
        "hookSpecificOutput": {
            "hookEventName": "PreToolUse",
            "permissionDecision": decision,
            "permissionDecisionReason": reason,
        }
    }


def extract_command(tool_name: str, tool_input: dict) -> str | None:
    if tool_name == "run_in_terminal":
        return str(tool_input.get("command") or "").strip()

    if tool_name == "create_and_run_task":
        task = tool_input.get("task") or {}
        command = str(task.get("command") or "").strip()
        args = task.get("args") or []
        rendered_args = [shlex.quote(str(arg)) for arg in args]
        return " ".join([command, *rendered_args]).strip()

    return None


def strip_leading_directory_commands(command: str) -> str:
    stripped = command.strip()
    while True:
        updated = LEADING_DIRECTORY_RE.sub("", stripped, count=1)
        if updated == stripped:
            return stripped
        stripped = updated.strip()


def strip_control_flow_prefixes(command: str) -> str:
    stripped = command.strip()
    while True:
        updated = CONTROL_FLOW_PREFIX_RE.sub("", stripped, count=1)
        if updated == stripped:
            return stripped
        stripped = updated.strip()


def split_segments(command: str) -> list[str]:
    return [segment.strip() for segment in SEGMENT_SPLIT_RE.split(command) if segment.strip()]


def first_token(segment: str) -> tuple[str, list[str]]:
    try:
        tokens = shlex.split(segment, posix=True)
    except ValueError:
        return "", []

    for token in tokens:
        if ASSIGNMENT_RE.match(token):
            continue
        if token == "env":
            continue
        return token, tokens

    return "", tokens


def classify_git(tokens: list[str]) -> tuple[bool, str]:
    if len(tokens) == 1:
        return True, "Git listing command is allowlisted."

    subcommand = tokens[1]
    if subcommand in SAFE_GIT_SUBCOMMANDS:
        return True, "Read-only git command is allowlisted."

    if subcommand == "branch":
        if len(tokens) == 2:
            return True, "Git branch inspection is allowlisted."

        trailing_tokens = tokens[2:]
        if trailing_tokens and all(token.startswith("-") and token in SAFE_GIT_BRANCH_FLAGS for token in trailing_tokens):
            return True, "Git branch inspection is allowlisted."

    return False, f"Git subcommand `{subcommand}` is outside the fastgrind allowlist."


def variable_probe_allowed(tokens: list[str]) -> bool:
    non_redirection_tokens = [token for token in tokens[1:] if "<" not in token and ">" not in token]
    if not non_redirection_tokens:
        return False

    if non_redirection_tokens[0] not in SAFE_VARIABLE_PROBE_FLAGS:
        return False

    return all(token in SAFE_VARIABLE_PROBE_FLAGS for token in non_redirection_tokens)


def classify_repo_path(root: str) -> tuple[bool, str]:
    if root.startswith(SAFE_RELATIVE_PREFIXES):
        return True, "Repository-local build, demo, testcase, or tool entrypoint is allowlisted."

    if root.startswith("./") and root.endswith(SAFE_SCRIPT_SUFFIXES):
        return True, "Repository-local script execution is allowlisted."

    return False, ""


def classify_segment(segment: str) -> tuple[bool, str]:
    normalized = strip_leading_directory_commands(segment)
    if not normalized:
        return True, "Directory navigation prefix is allowlisted."

    normalized = strip_control_flow_prefixes(normalized)
    if not normalized:
        return True, "Shell control-flow prefix is allowlisted."

    root, tokens = first_token(normalized)
    if not root:
        return False, "Unable to parse the shell segment for allowlist matching."

    if root in CONTROL_FLOW_CLOSING_KEYWORDS:
        return True, "Shell control-flow closing marker is allowlisted."

    if root in CONTROL_FLOW_HEADER_KEYWORDS:
        return True, "Shell control-flow header is allowlisted."

    if root in CONTROL_FLOW_CONDITION_KEYWORDS:
        remainder = normalized[len(root) :].strip()
        if not remainder:
            return True, "Shell control-flow condition header is allowlisted."
        return classify_segment(remainder)

    if VARIABLE_COMMAND_RE.match(root):
        if variable_probe_allowed(tokens):
            return True, "Variable-resolved version or help probe is allowlisted."
        return False, "Variable-resolved command is outside the fastgrind allowlist."

    if root == "git":
        return classify_git(tokens)

    if root == "command":
        if len(tokens) > 1 and tokens[1] == "-v":
            return True, "Command availability checks are allowlisted."
        return False, "Only `command -v` is auto-approved."

    if root == "chmod":
        if "-R" in tokens[1:]:
            return False, "Recursive chmod requires confirmation."
        if any("+x" in token for token in tokens[1:]):
            return True, "Non-recursive executable-bit changes are allowlisted."
        return False, "Only non-recursive script chmod is auto-approved."

    if root in COMMON_BASH_ROOTS:
        return True, "Common bash helper command is allowlisted."

    if root in COMMON_INSPECTION_ROOTS:
        return True, "Common inspection command is allowlisted."

    if root in COMMON_BUILD_ROOTS or COMPILER_ROOT_RE.match(root):
        return True, "Common build or compile command is allowlisted."

    repo_path_allowed, repo_path_reason = classify_repo_path(root)
    if repo_path_allowed:
        return True, repo_path_reason

    return False, f"Command `{root}` is not in the fastgrind allowlist."


def classify_command(command: str) -> tuple[bool, str]:
    if not command:
        return True, "Empty terminal command."

    for segment in split_segments(command):
        allowed, reason = classify_segment(segment)
        if not allowed:
            return False, reason

    return True, "Matched the fastgrind common compile and bash allowlist."


def main() -> None:
    payload = json.load(sys.stdin)
    tool_name = normalize_tool_name(payload.get("tool_name"))

    tool_input = payload.get("tool_input") or {}
    command = extract_command(tool_name, tool_input)
    if command is None:
        json.dump({"continue": True}, sys.stdout)
        return

    for pattern, reason in HIGH_RISK_RULES:
        if pattern.search(command):
            json.dump(pretool_output("ask", reason), sys.stdout)
            return

    allowed, reason = classify_command(command)
    if allowed:
        json.dump(pretool_output("allow", reason), sys.stdout)
        return

    json.dump(pretool_output("ask", reason), sys.stdout)


if __name__ == "__main__":
    main()