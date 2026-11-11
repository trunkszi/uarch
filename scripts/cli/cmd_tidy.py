"""Bounded parallel clang-tidy checks against the selected compile database."""

import asyncio
import json
import re
import shlex
import subprocess
import tempfile
from pathlib import Path

import typer

from .cmd_build import ensure_configured
from .config import PROJECT_ROOT
from .preset import resolve_preset
from .utils import cpp_files, find_clang_tool, get_concurrency_budget

DIAGNOSTIC_CHECK_RE = re.compile(r"(?:warning|error):\s+.*\[([^\],\s]+)(?:,[^\]]*)?\]\s*$")
_STOCK_REPLACED_ARGS = {"--extra-arg=-freflection", "--warnings-as-errors=*"}
_STOCK_EXTRA_ARG = "--extra-arg=-D__is_consteval_only(...)=false"


def _supports_reflection(tool):
    with tempfile.TemporaryDirectory(prefix="uarch-tidy-probe-") as temporary:
        probe = Path(temporary) / "probe.cc"
        probe.write_text("[[maybe_unused]] constexpr auto kProbe = ^^int;\n")
        result = subprocess.run(
            [str(tool), str(probe), "--", "-std=c++26", "-freflection"],
            cwd=temporary,
            text=True,
            capture_output=True,
            check=False,
        )
        return result.returncode == 0


def _adapt_stock_tidy(commands):
    database_path = Path(commands[0][3]) / "compile_commands.json"
    database = json.loads(database_path.read_text())
    for entry in database:
        arguments = entry.get("arguments")
        if arguments is None and "command" in entry:
            arguments = shlex.split(entry.pop("command"))
        if arguments is not None:
            entry["arguments"] = [arg for arg in arguments if arg != "-freflection"]
    database_path.write_text(json.dumps(database))
    adapted = []
    for command in commands:
        rewritten = []
        inserted = False
        for arg in command:
            if arg in _STOCK_REPLACED_ARGS:
                if not inserted:
                    rewritten.append(_STOCK_EXTRA_ARG)
                    inserted = True
                continue
            rewritten.append(arg)
        adapted.append(rewritten)
    return adapted


def _project_check(line):
    match = DIAGNOSTIC_CHECK_RE.search(line)
    if not match or match.group(1).startswith("clang-diagnostic-"):
        return False
    location = line.split(":", 1)[0].strip()
    root = str(PROJECT_ROOT)
    return location == root or location.startswith(root + "/")


def _stock_tidy_failed(output, returncode):
    if returncode < 0 or "fatal error:" in output:
        return True
    return any(_project_check(line) for line in output.splitlines())


def _stock_display(output):
    shown = [line for line in output.splitlines() if "fatal error:" in line or _project_check(line)]
    return "\n".join(shown)


async def _check_all(commands, jobs, out):
    limit = asyncio.Semaphore(jobs)
    capable = True if not commands else _supports_reflection(commands[0][0])
    planned = commands if capable else _adapt_stock_tidy(commands)

    async def check(command):
        async with limit:
            process = await asyncio.create_subprocess_exec(
                *command, cwd=PROJECT_ROOT, stdout=asyncio.subprocess.PIPE, stderr=asyncio.subprocess.PIPE
            )
            stdout, stderr = await process.communicate()
            output = stdout.decode(errors="replace") + stderr.decode(errors="replace")
            failed = process.returncode != 0 if capable else _stock_tidy_failed(output, process.returncode)
            out.emit(
                "command",
                shlex.join(command),
                exit_code=process.returncode,
                stdout=stdout.decode(errors="replace"),
                stderr=stderr.decode(errors="replace"),
            )
            if out.fmt == "text":
                display = output if capable else _stock_display(output)
                if display:
                    out.console.print(display, markup=False)
            return 1 if failed else 0

    codes = await asyncio.gather(*(check(command) for command in planned))
    if any(codes):
        raise RuntimeError("clang-tidy checks failed")


def tidy_cmd(
    ctx: typer.Context,
    paths: list[str] | None = typer.Argument(None, help="Source files or directories to process"),
    debug: bool = typer.Option(False, "-d", "--debug", help="Use the debug preset"),
    dev: bool = typer.Option(
        False, "-dev", "--dev", "--development", help="Use a development build with sanitizers"
    ),
    compiler: str = typer.Option(
        "clang",
        "-c",
        "--compiler",
        help="Compiler toolchain (clang, gcc, musl-clang, musl-gcc, clang-glibc, gcc-glibc, clang-musl, gcc-musl)",
    ),
    sanitizer: str = typer.Option(
        "asan-ubsan", "-s", "--sanitizer", help="Sanitizer for --dev builds (asan-ubsan)"
    ),
    preset: str | None = typer.Option(
        None,
        "--preset",
        help="Explicit CMake preset override (e.g. clang-glibc-release, gcc-glibc-release, clang-musl-release, gcc-musl-release)",
    ),
    jobs: int | None = typer.Option(
        None, "-j", "--jobs", min=1, help="Maximum parallel build or analysis jobs"
    ),
    verbose: bool = typer.Option(False, "-v", "--verbose", help="Show verbose command output"),
):
    """Run clang-tidy in parallel; reflection flags are adapted in a temporary database."""
    tool = find_clang_tool("clang-tidy")
    if not tool:
        raise ValueError("clang-tidy is unavailable; install a reflection-capable version or add it to PATH")
    name = resolve_preset(debug, dev, compiler, sanitizer, preset=preset)
    directory = ensure_configured(name, ctx.obj)
    database = json.loads((directory / "compile_commands.json").read_text())
    selected = set(cpp_files(paths)) if paths else None
    explicit_negative = any("compile_fail" in Path(path).resolve().parts for path in (paths or []))
    files = set()
    for entry in database:
        source = (Path(entry["directory"]) / entry["file"]).resolve()
        if "compile_fail" in source.parts and not explicit_negative:
            continue
        if selected is None or source in selected:
            files.add(source)
        arguments = entry.get("arguments") or shlex.split(entry.pop("command"))
        # Equivalent to --removed-arg=-freflection-latest; clang-tidy's standard CLI lacks that flag.
        entry["arguments"] = [arg for arg in arguments if arg != "-freflection-latest"]
    if not files:
        raise ValueError("No requested translation units appear in compile_commands.json")
    with tempfile.TemporaryDirectory(prefix="tidy-", dir=directory) as temporary:
        (Path(temporary) / "compile_commands.json").write_text(json.dumps(database))
        commands = [
            [tool, str(file), "-p", temporary, "--extra-arg=-freflection", "--warnings-as-errors=*"]
            for file in sorted(files)
        ]
        asyncio.run(_check_all(commands, jobs or get_concurrency_budget(), ctx.obj))
