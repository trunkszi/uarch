"""Bounded parallel clang-tidy checks against the selected compile database."""

import asyncio
import json
import shlex
import tempfile
from pathlib import Path

import typer

from .cmd_build import ensure_configured
from .config import PROJECT_ROOT
from .preset import resolve_preset
from .utils import cpp_files, find_clang_tool, get_concurrency_budget


async def _check_all(commands, jobs, out):
    limit = asyncio.Semaphore(jobs)

    async def check(command):
        async with limit:
            process = await asyncio.create_subprocess_exec(
                *command, cwd=PROJECT_ROOT, stdout=asyncio.subprocess.PIPE, stderr=asyncio.subprocess.PIPE
            )
            stdout, stderr = await process.communicate()
            out.emit(
                "command",
                shlex.join(command),
                exit_code=process.returncode,
                stdout=stdout.decode(errors="replace"),
                stderr=stderr.decode(errors="replace"),
            )
            if out.fmt == "text":
                out.console.print(
                    stdout.decode(errors="replace") + stderr.decode(errors="replace"), markup=False
                )
            return process.returncode

    codes = await asyncio.gather(*(check(command) for command in commands))
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
