"""CLI contracts exercised through pure resolvers and the real launcher."""

import json
import subprocess
from pathlib import Path

import pytest
from cli.config import PROJECT_ROOT


def test_preset_matrix():
    from cli.preset import resolve_preset

    for compiler, prefix in [
        ("clang", "clang-glibc"),
        ("gcc", "gcc-glibc"),
        ("musl-clang", "clang-musl"),
        ("musl-gcc", "gcc-musl"),
    ]:
        for alias in (compiler, prefix):
            assert resolve_preset(compiler=alias) == prefix + "-release"
            assert resolve_preset(debug=True, compiler=alias) == prefix + "-debug"
    with pytest.warns(UserWarning):
        assert resolve_preset(dev=True, compiler="musl-gcc") == "gcc-glibc-asan-ubsan"
    assert resolve_preset(dev=True) == "clang-glibc-asan-ubsan"
    with pytest.raises(ValueError):
        resolve_preset(debug=True, dev=True)
    with pytest.raises(ValueError):
        resolve_preset(preset="../outside")


def test_default_command():
    from cli.__main__ import _with_default_command

    assert _with_default_command([]) == ["build"]
    assert _with_default_command(["-d"]) == ["build", "-d"]
    assert _with_default_command(["--help"]) == ["--help"]
    assert _with_default_command(["--format", "json", "-t", "test"]) == [
        "--format",
        "json",
        "build",
        "-t",
        "test",
    ]
    assert _with_default_command(["--format=json", "test"]) == ["--format=json", "test"]


def launch(*args):
    return subprocess.run([str(PROJECT_ROOT / "uarch"), *args], text=True, capture_output=True, check=False)


@pytest.mark.parametrize(
    "command", [None, "build", "test", "bench", "verify", "coverage", "format", "tidy", "setup"]
)
def test_help(command):
    result = launch(*([command] if command else []), "--help")
    assert result.returncode == 0, result.stderr
    assert "Usage" in result.stdout


def test_json_list():
    result = launch("--format", "json", "build", "--list")
    payload = json.loads(result.stdout)
    assert result.returncode == 0, payload
    assert payload["success"] and any(e["level"] == "table" for e in payload["events"])


@pytest.mark.parametrize(
    "args", [("format", "--check"), ("test", "-c", "clang", "--filter", "reflect"), ("bench", "--list")]
)
def test_real_commands(args):
    result = launch(*args)
    assert result.returncode == 0, result.stdout + result.stderr


def test_comment_parser_preserves_urls():
    from cli.preset import _strip_json_comments

    assert json.loads(_strip_json_comments('{"url":"https://host/path" /*comment*/ // line\n}')) == {
        "url": "https://host/path"
    }


def test_duplicate_work_preserves_flags(tmp_path):
    from cli.build_reuse import duplicate_compilations

    database = tmp_path / "compile_commands.json"

    def entry(output, flag="-O3"):
        return {
            "directory": str(tmp_path),
            "file": "probe.cc",
            "arguments": ["clang++", flag, "-c", "probe.cc", "-o", output],
        }

    database.write_text(json.dumps([entry("a.o"), entry("b.o")]))
    assert len(duplicate_compilations(database)) == 1
    database.write_text(json.dumps([entry("a.o"), entry("b.o", "-O0")]))
    assert not duplicate_compilations(database)


def test_cache_drift(tmp_path):
    from cli.cmd_build import _check_cache_drift

    (tmp_path / "CMakeCache.txt").write_text("UARCH_BUILD_TESTS:BOOL=ON\nCMAKE_BUILD_TYPE:STRING=Release\n")
    assert not _check_cache_drift(tmp_path, {"UARCH_BUILD_TESTS": True, "CMAKE_BUILD_TYPE": "Release"})
    assert _check_cache_drift(tmp_path, {"UARCH_BUILD_TESTS": False})


@pytest.mark.parametrize(
    "args", [("--static",), ("--static", "-dev", "-c", "musl-clang"), ("--preset", "not-a-preset")]
)
def test_json_rejects_invalid_build(args):
    result = launch("--format", "json", "build", *args)
    payload = json.loads(result.stdout)
    assert result.returncode != 0 and not payload["success"]
    assert payload["exit_code"] != 0


def test_json_rejects_missing_test(tmp_path):
    result = launch("--format", "json", "test", "-t", "unknown_test", "--no-build")
    assert result.returncode != 0
    assert not json.loads(result.stdout)["success"]


def test_target_listing_is_not_compilation(monkeypatch, tmp_path):
    from cli.utils import list_ninja_targets

    monkeypatch.setattr(
        "cli.utils.capture",
        lambda *a: (
            "tests/demo: CXX_EXECUTABLE_LINKER__demo\n"
            "demo: phony\ncmake_object_order_depends_target_demo: phony\n"
        ),
    )
    targets = dict(list_ninja_targets(tmp_path))
    assert targets["demo"] == "executable"
    assert "cmake_object_order_depends_target_demo" not in targets


def test_listing_respects_filter():
    result = launch("--format", "json", "test", "--list", "--filter", "reflect")
    payload = json.loads(result.stdout)
    assert result.returncode == 0, payload
    table = [event for event in payload["events"] if event["level"] == "table"][-1]
    assert table["data"]["rows"] == [["reflect_test"]]


def test_invocation_error_is_json():
    result = launch("--format", "json", "build", "--unknown-option")
    payload = json.loads(result.stdout)
    assert result.returncode != 0 and not payload["success"]


def test_missing_verify_targets_is_explicit():
    result = launch("--format", "json", "verify")
    payload = json.loads(result.stdout)
    if "merge T8 first" in payload["events"][-1]["message"]:
        assert result.returncode != 0 and not payload["success"]
    else:
        assert result.returncode == 0, payload


def test_clean_subsystem_targets():
    from cli.config import get_build_directory
    from cli.utils import list_ctest_targets, list_ninja_targets

    result = launch("bench", "--list")
    assert result.returncode == 0, result.stdout + result.stderr
    directory = get_build_directory("clang-glibc-release")
    names = dict(list_ninja_targets(directory))
    assert {"layout_bench", "flow_bench", "fast_divisor_bench", "arch_bench", "soa_bench"} <= names.keys()
    cases = {case["name"] for case in list_ctest_targets(directory)}
    assert {
        "fast_divisor_u32_test",
        "fast_divisor_u64_test",
        "power_of_two_test",
        "saturation_test",
        "prefetch_test",
        "uarch_verify_all",
        "abi_cross_test",
        "facade_test",
        "soa_test",
    } <= cases
    assert not any(name.startswith(("t2_", "t4_", "t5_", "t6_", "t8_", "t9_")) for name in cases)
    assert not any(name.startswith(("t2_", "t4_", "t5_", "t6_", "t9_")) and "bench" in name for name in names)


@pytest.mark.parametrize(
    "alias,canonical",
    [
        ("all", "all"),
        ("lint", "lint"),
        ("abi-check", "abi-check"),
        ("asm-check", "asm-check"),
        ("mca-gate", "mca-gate"),
        ("size-report", "size-report"),
        ("abi", "abi-check"),
        ("asm", "asm-check"),
        ("mca", "mca-gate"),
        ("symbols", "size-report"),
        ("size", "size-report"),
        ("pmu-gate", "pmu-gate"),
        ("pmu", "pmu-gate"),
        ("tma", "pmu-gate"),
        ("gem5-gate", "gem5-gate"),
        ("gem5", "gem5-gate"),
        ("o3", "gem5-gate"),
    ],
)
def test_verify_gate_forwarding(monkeypatch, tmp_path, alias, canonical):
    from types import SimpleNamespace

    from cli import cmd_verify

    commands = []
    output = SimpleNamespace(run_command=lambda command: commands.append(command))
    monkeypatch.setattr(cmd_verify, "ensure_configured", lambda *a, **k: tmp_path)
    monkeypatch.setattr(cmd_verify, "build_targets", lambda *a, **k: None)
    monkeypatch.setattr(
        cmd_verify,
        "list_ninja_targets",
        lambda *a: [("uarch_verify", "executable"), ("uarch_probes", "target")],
    )
    monkeypatch.setattr(cmd_verify, "find_binary", lambda *a: tmp_path / "uarch_verify")
    cmd_verify.verify_cmd(
        SimpleNamespace(obj=output),
        gate=alias,
        debug=False,
        dev=False,
        compiler="clang",
        sanitizer="asan-ubsan",
        preset=None,
        verbose=False,
    )
    assert commands == [[tmp_path / "uarch_verify", canonical]]


def test_verify_invalid_gate_precedes_build(monkeypatch):
    from types import SimpleNamespace

    from cli import cmd_verify

    def forbidden(*args, **kwargs):
        pytest.fail("Invalid gate must be rejected before configuration")

    monkeypatch.setattr(cmd_verify, "ensure_configured", forbidden)
    with pytest.raises(ValueError, match="Unknown verification gate"):
        cmd_verify.verify_cmd(SimpleNamespace(obj=None), gate="unknown")


def invoke_command(function, out=None, **overrides):
    import inspect
    from types import SimpleNamespace

    values = {
        name: parameter.default.default
        for name, parameter in inspect.signature(function).parameters.items()
        if name != "ctx"
    }
    values.update(overrides)
    return function(SimpleNamespace(obj=out or SimpleNamespace(verbose=False)), **values)


def test_launcher_renamed():
    assert (PROJECT_ROOT / "uarch").is_file()
    assert not (PROJECT_ROOT / "build.py").exists()


@pytest.mark.parametrize("command", ["build", "test", "bench", "verify", "tidy", "setup"])
def test_compiler_help_lists_aliases(command):
    result = launch(command, "--help")
    assert result.returncode == 0
    assert "clang, gcc, musl-clang, musl-gcc" in " ".join(result.stdout.replace(chr(0x2502), " ").split())


def test_options_have_help():
    import ast

    for file in [PROJECT_ROOT / "scripts/cli/app.py", *(PROJECT_ROOT / "scripts/cli").glob("cmd_*.py")]:
        for node in ast.walk(ast.parse(file.read_text())):
            if (
                isinstance(node, ast.Call)
                and isinstance(node.func, ast.Attribute)
                and isinstance(node.func.value, ast.Name)
                and node.func.value.id == "typer"
                and node.func.attr in {"Option", "Argument"}
            ):
                assert any(k.arg == "help" for k in node.keywords), (file, node.lineno)


def test_clean_rejects_symlink_root(monkeypatch, tmp_path):
    from cli import cmd_build

    root, outside = tmp_path / "repo", tmp_path / "outside"
    root.mkdir()
    victim = outside / "clang-glibc-release"
    victim.mkdir(parents=True)
    marker = victim / "keep"
    marker.write_text("keep")
    (root / "build").symlink_to(outside, target_is_directory=True)
    monkeypatch.setattr(cmd_build, "PROJECT_ROOT", root)
    monkeypatch.setattr(cmd_build, "get_build_directory", lambda _: root / "build/clang-glibc-release")
    monkeypatch.setattr(cmd_build.shutil, "rmtree", lambda _: pytest.fail("unsafe deletion reached"))
    with pytest.raises(ValueError, match="Refusing to clean"):
        invoke_command(cmd_build.build_cmd, clean=True)
    assert marker.read_text() == "keep"


@pytest.mark.parametrize("cached", ["ON", "TRUE", "1", "YES", "true"])
def test_cache_boolean_aliases(tmp_path, cached):
    from cli.cmd_build import _check_cache_drift

    (tmp_path / "CMakeCache.txt").write_text(f"UARCH_BUILD_TESTS:BOOL={cached}\n")
    assert not _check_cache_drift(tmp_path, {"UARCH_BUILD_TESTS": True})
    assert _check_cache_drift(tmp_path, {"UARCH_BUILD_TESTS": False})


def test_compiler_drift_reconfigures(monkeypatch, tmp_path):
    from types import SimpleNamespace

    from cli import cmd_build

    (tmp_path / "CMakeCache.txt").write_text(
        "CMAKE_BUILD_TYPE:STRING=Release\nUARCH_ENABLE_SANITIZERS:BOOL=OFF\n"
        "CMAKE_CXX_COMPILER:FILEPATH=/wrong/compiler\n"
    )
    monkeypatch.setattr(cmd_build, "should_reconfigure", lambda _: False)
    monkeypatch.setattr(cmd_build, "verify_build_reuse", lambda _: None)
    calls = []
    cmd_build.ensure_configured(
        "clang-glibc-release",
        SimpleNamespace(run_command=lambda command, **_: calls.append(command)),
        build_dir=tmp_path,
    )
    assert calls and any(arg.startswith("-DCMAKE_CXX_COMPILER=") for arg in calls[0])


def test_test_enables_tools(monkeypatch, tmp_path):
    from types import SimpleNamespace

    from cli import cmd_test

    captured = []
    monkeypatch.setattr(cmd_test, "ensure_configured", lambda *a, **k: captured.append(k) or tmp_path)
    monkeypatch.setattr(cmd_test, "list_ctest_targets", lambda *a: [])
    invoke_command(cmd_test.test_cmd, SimpleNamespace(table=lambda *a: None), list_tests=True)
    assert captured[0]["components"] == {"tests": True, "tools": True}


def test_coverage_disables_tools(monkeypatch):
    from cli import cmd_coverage

    class Configured(Exception):
        pass

    def capture(*args, **kwargs):
        assert kwargs["components"] == {"tests": True, "bench": False, "tools": False}
        raise Configured

    monkeypatch.setattr(cmd_coverage, "find_clang_tool", lambda name: name)
    monkeypatch.setattr(cmd_coverage, "ensure_configured", capture)
    with pytest.raises(Configured):
        invoke_command(cmd_coverage.coverage_cmd)


@pytest.mark.parametrize("script", [False, True])
def test_ctest_maps_build_target(monkeypatch, tmp_path, script):
    from types import SimpleNamespace

    from cli import cmd_test

    calls = []
    case = "negative_case" if script else "verify_all"
    command = ["/usr/bin/cmake", "-P", "check.cmake"] if script else [str(tmp_path / "verify")]
    monkeypatch.setattr(cmd_test, "ensure_configured", lambda *a, **k: tmp_path)
    monkeypatch.setattr(cmd_test, "list_ctest_targets", lambda *a: [{"name": case, "command": command}])
    available = [("verify", "executable")]
    if script:
        available.append((case, "target"))
    monkeypatch.setattr(cmd_test, "list_ninja_targets", lambda *a: available)
    monkeypatch.setattr(cmd_test, "build_targets", lambda d, o, targets, *a: calls.append(targets))
    invoke_command(cmd_test.test_cmd, SimpleNamespace(run_command=lambda *a, **k: None), target=case)
    assert calls == [None if script else ["verify"]]


def test_tidy_omits_negative_sources(monkeypatch, tmp_path):
    from cli import cmd_tidy

    negative = tmp_path / "tests/compile_fail/bad.cc"
    positive = tmp_path / "src/good.cc"
    database = [
        {"directory": str(tmp_path), "file": str(p), "arguments": ["clang++", "-c", str(p)]}
        for p in (negative, positive)
    ]
    (tmp_path / "compile_commands.json").write_text(json.dumps(database))
    monkeypatch.setattr(cmd_tidy, "find_clang_tool", lambda _: "clang-tidy")
    monkeypatch.setattr(cmd_tidy, "ensure_configured", lambda *a, **k: tmp_path)
    calls = []

    async def capture(commands, *_):
        calls.extend(commands)

    monkeypatch.setattr(cmd_tidy, "_check_all", capture)
    invoke_command(cmd_tidy.tidy_cmd)
    assert [command[1] for command in calls] == [str(positive)]


@pytest.mark.parametrize(
    "fragment", ["tests/T9.cmake", "benches/T9.cmake", "tests/CMakeLists.txt", "benches/CMakeLists.txt"]
)
def test_reconfigure_watches_fragments(monkeypatch, tmp_path, fragment):
    import os

    from cli import utils

    for path in ("CMakeLists.txt", "CMakePresets.json", fragment):
        file = tmp_path / path
        file.parent.mkdir(parents=True, exist_ok=True)
        file.write_text("")
        os.utime(file, ns=(1, 1))
    directory = tmp_path / "build/demo"
    directory.mkdir(parents=True)
    for name in ("CMakeCache.txt", "build.ninja"):
        file = directory / name
        file.write_text("")
        os.utime(file, ns=(10, 10))
    monkeypatch.setattr(utils, "PROJECT_ROOT", tmp_path)
    assert not utils.should_reconfigure(directory)
    os.utime(tmp_path / fragment, ns=(20, 20))
    assert utils.should_reconfigure(directory)


def test_no_build_requires_tools(monkeypatch, tmp_path):
    from cli import cmd_test

    (tmp_path / "CTestTestfile.cmake").write_text("")
    (tmp_path / "CMakeCache.txt").write_text("UARCH_BUILD_TESTS:BOOL=ON\nUARCH_BUILD_TOOLS:BOOL=OFF\n")
    monkeypatch.setattr(cmd_test, "get_build_directory", lambda _: tmp_path)
    with pytest.raises(ValueError, match="tools are disabled"):
        invoke_command(cmd_test.test_cmd, no_build=True)


def test_toolchain_compiler_resolved(monkeypatch, tmp_path):
    from cli import preset

    toolchain = tmp_path / "toolchain.cmake"
    toolchain.write_text('set(CMAKE_CXX_COMPILER "${CMAKE_CURRENT_LIST_DIR}/bin/compiler")\n')
    monkeypatch.setattr(preset, "PROJECT_ROOT", tmp_path)
    (tmp_path / "CMakePresets.json").write_text(
        json.dumps(
            {
                "configurePresets": [
                    {"name": "base", "hidden": True, "toolchainFile": "${sourceDir}/toolchain.cmake"},
                    {"name": "child", "inherits": "base"},
                ]
            }
        )
    )
    assert preset.get_preset_compiler("child") == str(tmp_path / "bin/compiler")


def test_build_sets_tools(monkeypatch, tmp_path):
    from cli import cmd_build

    calls = []
    monkeypatch.setattr(cmd_build, "ensure_configured", lambda *a, **k: calls.append(k) or tmp_path)
    monkeypatch.setattr(cmd_build, "build_targets", lambda *a: None)
    invoke_command(cmd_build.build_cmd, tests=False)
    assert calls[0]["components"] == {"tests": False, "bench": False, "tools": False}


def test_no_clapp_fails_offline(tmp_path):
    import shlex

    installed = Path("/root/.vcpkg/packages/clapp_x64-linux-musl-clang-release/include")
    compiler = Path("/root/.local/toolchain/clang-glibc/bin/clang++")
    if not installed.is_dir() or not compiler.is_file():
        pytest.skip("local dependency sandbox requires the configured SDK")
    namespace = subprocess.run(["unshare", "-mn", "true"], capture_output=True, check=False)
    if namespace.returncode:
        pytest.skip("mount/network namespaces unavailable")
    command = [
        "cmake",
        "-S",
        str(PROJECT_ROOT),
        "-B",
        str(tmp_path / "build"),
        "-G",
        "Ninja",
        f"-DCMAKE_CXX_COMPILER={compiler}",
        "-DUARCH_BUILD_TESTS=OFF",
        "-DUARCH_BUILD_TOOLS=ON",
        "-DCMAKE_DISABLE_FIND_PACKAGE_clapp=TRUE",
        f"-DUARCH_CLAPP_ARCHIVE={tmp_path / 'missing.tar.gz'}",
    ]
    result = subprocess.run(
        [
            "unshare",
            "-mn",
            "sh",
            "-c",
            'mount --make-rprivate / && mount -t tmpfs none "$1" && shift && exec "$@"',
            "dependency-test",
            str(installed),
            *command,
        ],
        text=True,
        capture_output=True,
        check=False,
    )
    assert result.returncode != 0, shlex.join(command)
    assert "Local clapp archive or headers not found" in " ".join((result.stdout + result.stderr).split())


def test_compiler_command_matches_path(monkeypatch, tmp_path):
    from cli import cmd_build

    compiler = tmp_path / "compiler"
    compiler.write_text("")
    (tmp_path / "CMakeCache.txt").write_text(f"CMAKE_CXX_COMPILER:FILEPATH={compiler}\n")
    monkeypatch.setattr(cmd_build.shutil, "which", lambda _: str(compiler))
    assert not cmd_build._check_cache_drift(tmp_path, {"CMAKE_CXX_COMPILER": "compiler"})


@pytest.mark.parametrize(
    "name,width,rob,lq,l1d",
    [
        ("zen4-o3", 6, 320, 72, "32kB"),
        ("zen5-o3", 8, 448, 96, "48kB"),
        ("sapphirerapids-o3", 6, 512, 128, "48kB"),
        ("neoverse-v2-o3", 6, 320, 84, "64kB"),
    ],
)
def test_gem5_profiles(name, width, rob, lq, l1d):
    import subprocess
    import sys

    script = Path(__file__).resolve().parents[2] / "gem5" / "o3_sim.py"
    result = subprocess.run(
        [sys.executable, str(script), "--dump-profile-json", name], capture_output=True, text=True, check=True
    )
    profile = json.loads(result.stdout)
    assert profile["slot_width"] == profile["rename_width"] == profile["dispatch_width"] == width
    assert profile["int_alu_count"] == width
    assert profile["int_mult_count"] == 2
    assert (profile["rob"], profile["lq"], profile["l1d"]) == (rob, lq, l1d)
    assert profile["branch_predictor"] == "TournamentBP"
    assert profile["clock"] == "2GHz"
    assert profile["memory_latency"] == "50ns"
    assert profile["physical_vector_regs"] == profile["physical_cc_regs"] == 768
    units = {unit["name"]: unit for unit in profile["functional_units"]}
    assert {name: units[name]["count"] for name in ("ReadPort", "WritePort", "RdWrPort")} == {
        "ReadPort": 2,
        "WritePort": 2,
        "RdWrPort": 4,
    }
    assert units["IntMultDiv"]["operations"][0]["opLat"] == 3
    assert profile["l1_tag_latency"] == profile["l1_data_latency"] == 2
    alias = subprocess.run(
        [sys.executable, str(script), "--profile", name, "--dump-profile-json"],
        capture_output=True,
        text=True,
        check=True,
    )
    assert json.loads(alias.stdout) == profile
