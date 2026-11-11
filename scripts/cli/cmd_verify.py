"""Invoke the slice-owned uarch_verify executable when it is available."""

import typer

from .cmd_bench import find_binary
from .cmd_build import build_targets, ensure_configured
from .preset import resolve_preset
from .utils import list_ninja_targets


def verify_cmd(
    ctx: typer.Context,
    gate: str = typer.Argument(
        "all",
        help="Gate: all, lint, abi-check, asm-check, mca-gate, size-report, pmu-gate, gem5-gate; aliases: abi, asm, mca, symbols, size, pmu, tma, gem5, o3",
    ),
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
    gem5_probe: str | None = typer.Option(
        None, "--gem5-probe", help="Comma-separated probe names (default: all)"
    ),
    gem5_cpu: str | None = typer.Option(None, "--gem5-cpu", help="Comma-separated parametric O3 profiles"),
    verbose: bool = typer.Option(False, "-v", "--verbose", help="Show verbose command output"),
):
    """Run a native verification gate using its canonical name or short alias."""
    gates = {"all", "lint", "abi-check", "asm-check", "mca-gate", "size-report", "pmu-gate", "gem5-gate"}
    aliases = {
        "abi": "abi-check",
        "asm": "asm-check",
        "mca": "mca-gate",
        "symbols": "size-report",
        "size": "size-report",
        "pmu": "pmu-gate",
        "tma": "pmu-gate",
        "gem5": "gem5-gate",
        "o3": "gem5-gate",
    }
    gate = aliases.get(gate, gate)
    if gate not in gates:
        raise ValueError(f"Unknown verification gate; choose {', '.join(sorted(gates | aliases.keys()))}")
    name = resolve_preset(debug, dev, compiler, sanitizer, preset=preset)
    directory = ensure_configured(name, ctx.obj, components={"tools": True})
    targets = dict(list_ninja_targets(directory))
    if any(target not in targets for target in ["uarch_verify", "uarch_probes"]):
        raise ValueError(
            "uarch_verify/uarch_probes targets are not registered in this checkout; merge T8 first"
        )
    build_targets(directory, ctx.obj, ["uarch_probes", "uarch_verify"], verbose=verbose)
    command = [find_binary(directory, "uarch_verify"), gate]
    if gem5_probe is not None:
        command.extend(["--gem5-probe", gem5_probe])
    if gem5_cpu is not None:
        command.extend(["--gem5-cpu", gem5_cpu])
    ctx.obj.run_command(command)
