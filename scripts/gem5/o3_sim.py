#!/usr/bin/env python3
"""Explicit parametric O3 profiles; aliases describe inspiration, not vendor equivalence."""

import argparse
import json
import sys


def functional_units(width):
    def unit(name, count, operations, latency=1):
        return {
            "name": name,
            "count": count,
            "operations": [{"opClass": op, "opLat": latency, "pipelined": True} for op in operations],
        }

    multiply = unit("IntMultDiv", 2, ["IntMult"], 3)
    multiply["operations"].append({"opClass": "IntDiv", "opLat": 12, "pipelined": False})
    floating = unit("FP_MultDiv", 2, ["FloatMult"], 4)
    floating["operations"].extend(
        [
            {"opClass": "FloatMultAcc", "opLat": 5, "pipelined": True},
            {"opClass": "FloatMisc", "opLat": 3, "pipelined": True},
            {"opClass": "FloatDiv", "opLat": 12, "pipelined": False},
            {"opClass": "FloatSqrt", "opLat": 24, "pipelined": False},
        ]
    )
    system = unit("IprPort", 1, ["System"], 3)
    system["operations"][0]["pipelined"] = False
    simd = [
        "SimdAdd",
        "SimdAddAcc",
        "SimdAlu",
        "SimdCmp",
        "SimdCvt",
        "SimdMisc",
        "SimdMult",
        "SimdMultAcc",
        "SimdMatMultAcc",
        "SimdShift",
        "SimdShiftAcc",
        "SimdDiv",
        "SimdSqrt",
        "SimdFloatAdd",
        "SimdFloatAlu",
        "SimdFloatCmp",
        "SimdFloatCvt",
        "SimdFloatDiv",
        "SimdFloatMisc",
        "SimdFloatMult",
        "SimdFloatMultAcc",
        "SimdFloatMatMultAcc",
        "SimdFloatSqrt",
        "SimdReduceAdd",
        "SimdReduceAlu",
        "SimdReduceCmp",
        "SimdFloatReduceAdd",
        "SimdFloatReduceCmp",
        "SimdAes",
        "SimdAesMix",
        "SimdSha1Hash",
        "SimdSha1Hash2",
        "SimdSha256Hash",
        "SimdSha256Hash2",
        "SimdShaSigma2",
        "SimdShaSigma3",
        "SimdPredAlu",
    ]
    return [
        unit("IntAlu", width, ["IntAlu"]),
        multiply,
        unit("FP_ALU", 4, ["FloatAdd", "FloatCmp", "FloatCvt"], 2),
        floating,
        unit("SIMD_Unit", 4, simd),
        unit("ReadPort", 2, ["MemRead", "FloatMemRead"]),
        unit("WritePort", 2, ["MemWrite", "FloatMemWrite"]),
        unit("RdWrPort", 4, ["MemRead", "MemWrite", "FloatMemRead", "FloatMemWrite"]),
        system,
    ]


def profile(width, rob, iq, lq, sq, l1d, l2, inspired_by, isa="x86"):
    return {
        "isa": isa,
        "slot_width": width,
        "inspired_by": inspired_by,
        "fetch_width": width,
        "decode_width": width,
        "rename_width": width,
        "dispatch_width": width,
        "issue_width": width,
        "commit_width": width,
        "squash_width": width,
        "rob": rob,
        "iq": iq,
        "lq": lq,
        "sq": sq,
        "l1i": "32kB" if isa == "x86" else "64kB",
        "l1d": l1d,
        "l2": l2,
        "l1i_assoc": 8,
        "l1d_assoc": 12 if l1d == "48kB" else 8,
        "l2_assoc": 16,
        "cache_line_bytes": 64,
        "l1_tag_latency": 2,
        "l1_data_latency": 2,
        "l1_response_latency": 1,
        "l2_tag_latency": 2,
        "l2_data_latency": 8,
        "l2_response_latency": 2,
        "memory": "512MiB",
        "memory_latency": "50ns",
        "memory_bandwidth": "64GB/s",
        "clock": "2GHz",
        "branch_predictor": "TournamentBP",
        "int_alu_count": width,
        "int_mult_count": 2,
        "physical_int_regs": 768,
        "physical_float_regs": 768,
        "physical_vector_regs": 768,
        "physical_vector_predicate_regs": 128,
        "physical_matrix_regs": 16,
        "physical_cc_regs": 768,
        "functional_units": functional_units(width),
    }


PROFILES = {
    "o3-w6-rob320": profile(6, 320, 160, 72, 64, "32kB", "1MB", "znver4"),
    "o3-w8-rob448": profile(8, 448, 192, 96, 72, "48kB", "1MB", "znver5"),
    "o3-w6-rob512": profile(6, 512, 196, 128, 72, "48kB", "2MB", "sapphirerapids"),
    "o3-a64-w6-rob320": profile(6, 320, 160, 84, 72, "64kB", "1MB", "neoverse-v2", "aarch64"),
}
ALIASES = {
    "zen4-o3": "o3-w6-rob320",
    "znver4": "o3-w6-rob320",
    "zen5-o3": "o3-w8-rob448",
    "znver5": "o3-w8-rob448",
    "sapphirerapids-o3": "o3-w6-rob512",
    "sapphirerapids": "o3-w6-rob512",
    "neoverse-v2-o3": "o3-a64-w6-rob320",
    "neoverse-v2": "o3-a64-w6-rob320",
}


def simulate(args, settings):
    import m5
    from m5 import objects as obj

    system = obj.System()
    system.clk_domain = obj.SrcClockDomain(clock=settings["clock"], voltage_domain=obj.VoltageDomain())
    system.mem_mode = "timing"
    system.mem_ranges = [obj.AddrRange(settings["memory"])]
    cpu_type = obj.ArmO3CPU if settings["isa"] == "aarch64" else obj.X86O3CPU
    system.cpu = cpu_type()
    for key in ("fetch", "decode", "rename", "dispatch", "issue", "commit", "squash"):
        setattr(system.cpu, f"{key}Width", settings[f"{key}_width"])
    system.cpu.wbWidth = settings["issue_width"]
    system.cpu.numROBEntries = settings["rob"]
    pool = obj.FUPool(
        FUList=[
            obj.FUDesc(count=unit["count"], opList=[obj.OpDesc(**op) for op in unit["operations"]])
            for unit in settings["functional_units"]
        ]
    )
    if hasattr(obj, "IQUnit"):
        system.cpu.instQueues = [obj.IQUnit(numEntries=settings["iq"], fuPool=pool)]
    else:
        system.cpu.numIQEntries = settings["iq"]
    system.cpu.LQEntries = settings["lq"]
    system.cpu.SQEntries = settings["sq"]
    for parameter, key in (
        ("numPhysIntRegs", "physical_int_regs"),
        ("numPhysFloatRegs", "physical_float_regs"),
        ("numPhysVecRegs", "physical_vector_regs"),
        ("numPhysVecPredRegs", "physical_vector_predicate_regs"),
        ("numPhysMatRegs", "physical_matrix_regs"),
        ("numPhysCCRegs", "physical_cc_regs"),
    ):
        if hasattr(system.cpu, parameter):
            setattr(system.cpu, parameter, settings[key])
    bp = getattr(obj, args.branch_predictor or settings["branch_predictor"])()
    if hasattr(obj, "BranchPredictor") and "conditionalBranchPred" in obj.BranchPredictor._params:
        system.cpu.branchPred = obj.BranchPredictor(conditionalBranchPred=bp)
    else:
        system.cpu.branchPred = bp
    if not hasattr(obj, "IQUnit"):
        system.cpu.fuPool = pool
    system.cache_line_size = settings["cache_line_bytes"]
    system.membus = obj.SystemXBar()
    system.l2bus = obj.L2XBar()
    for name, size in (("icache", settings["l1i"]), ("dcache", settings["l1d"])):
        cache = obj.Cache(
            size=size,
            assoc=settings["l1i_assoc"] if name == "icache" else settings["l1d_assoc"],
            tag_latency=settings["l1_tag_latency"],
            data_latency=settings["l1_data_latency"],
            response_latency=settings["l1_response_latency"],
            mshrs=16,
            tgts_per_mshr=16,
        )
        setattr(system.cpu, name, cache)
        cache.mem_side = system.l2bus.cpu_side_ports
        setattr(system.cpu, f"{name}_port", cache.cpu_side)
    system.l2 = obj.Cache(
        size=settings["l2"],
        assoc=settings["l2_assoc"],
        tag_latency=settings["l2_tag_latency"],
        data_latency=settings["l2_data_latency"],
        response_latency=settings["l2_response_latency"],
        mshrs=32,
        tgts_per_mshr=16,
    )
    system.l2.cpu_side = system.l2bus.mem_side_ports
    system.l2.mem_side = system.membus.cpu_side_ports
    system.cpu.createInterruptController()
    if settings["isa"] == "x86":
        system.cpu.interrupts[0].pio = system.membus.mem_side_ports
        system.cpu.interrupts[0].int_requestor = system.membus.cpu_side_ports
        system.cpu.interrupts[0].int_responder = system.membus.mem_side_ports
    system.memory = obj.SimpleMemory(
        range=system.mem_ranges[0], latency=settings["memory_latency"], bandwidth=settings["memory_bandwidth"]
    )
    system.memory.port = system.membus.mem_side_ports
    system.system_port = system.membus.cpu_side_ports
    system.workload = obj.SEWorkload.init_compatible(args.binary)
    process = obj.Process(cmd=[args.binary, "--probe", args.probe, "--iterations", args.iterations])
    if hasattr(process, "release"):
        process.release = "6.8.0"
    process.env = ["GLIBC_TUNABLES=glibc.pthread.rseq=0"]
    system.cpu.workload = process
    system.cpu.createThreads()
    root = obj.Root(full_system=False, system=system)
    m5.disableAllListeners()
    m5.instantiate()
    event = m5.simulate(5_000_000_000)
    if event.getCause() != "exiting with last active thread context" or event.getCode() != 0:
        print(f"guest failed: {event.getCause()} code={event.getCode()}", file=sys.stderr)
        sys.exit(2)
    return root


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dump-profile-json", nargs="?", const="")
    parser.add_argument("--cpu", "--profile", default="o3-w6-rob320")
    parser.add_argument("--binary")
    parser.add_argument("--probe")
    parser.add_argument("--iterations", default="0000000512")
    parser.add_argument("--branch-predictor", choices=("TournamentBP", "LTAGE", "TAGE_SC_L_64KB"))
    args = parser.parse_args()
    name = ALIASES.get(args.dump_profile_json or args.cpu, args.dump_profile_json or args.cpu)
    if name not in PROFILES:
        parser.error("unknown O3 profile")
    if args.dump_profile_json is not None:
        print(json.dumps({"name": name, "profile": name, **PROFILES[name]}, sort_keys=True))
        return
    if not args.binary or not args.probe:
        parser.error("--binary and --probe are required")
    simulate(args, PROFILES[name])


if __name__ == "__main__" or __name__ == "__m5_main__":
    main()
