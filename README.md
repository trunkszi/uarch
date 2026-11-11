# uarch

C++26 primitives for explicit layout, integer bit algebra, predicated dataflow,
fixed-capacity storage, and architecture-specific kernels. `<uarch/primitives.hpp>`
is the runtime-free facade (`uarch::primitives`, also available as
`uarch::freestanding`): including it does not probe CPUs, start threads,
allocate a runtime, or install a dispatcher. `<uarch/uarch.hpp>` is the hosted
default (`uarch::uarch`). It includes that facade plus hosted detection.

```cpp
#include <uarch/uarch.hpp>

static_assert(uarch::PowerOfTwo<6>::value == 64);
static_assert(uarch::arithmetic::divisor_u64<7>().div(100) == 14);
```

The supported compiler configurations are Clang 21 with P2996 reflection and
libc++ 21, and GCC 16.2 with reflection and libstdc++. An ordinary compiler with
only `-std=c++26` is insufficient. CMake checks the required reflection features.
Library consumers compile without exceptions or RTTI.

## Architecture

```text
application / kernels / worker-owned storage
                 |
          <uarch/uarch.hpp>
                 |
     +-----------+-------------------------------+
     |                                           |
  uarch::arch                              uarch::core headers
  sealed permits                           reflect / layout / bits
  literal-target entries                   flow / arithmetic / mem
  native kernels                           views / simd / container
     |                                           |
     +-------------------+-----------------------+
                         |
                    uarch::tiers
             descriptive reflected metadata

hosted OS / CPU detection ---> minted permits ---> application
     (included by <uarch/uarch.hpp>, excluded from <uarch/primitives.hpp>)
```

CMake exports `uarch::tiers`, `uarch::core`, `uarch::arch`, `uarch::primitives`
(`uarch::freestanding`), and `uarch::uarch`. `uarch::uarch` links
`uarch::primitives` and `uarch::detect`. Core headers expose namespaces such as
`uarch::layout` directly. Use a narrow header for a narrow dependency,
`primitives.hpp` for the runtime-free surface, or `uarch.hpp` for the hosted
surface. `uarch::prelude` provides the common types and operations without
requiring a global using-directive.

## Reflection, layout and ABI

`TierMeta` annotations on `tiers::TierId` generate stable metadata tables.
`tiers::compiled_tier()` describes translation-unit compilation settings, not the
capabilities of the machine currently executing it. An ID or metadata record
never grants permission to execute a target instruction.

`layout::NoPadding<T>` recursively audits member offsets and storage size.
It rejects padding, unions, bitfields, `bool`, and `long double`; supported
empty `[[no_unique_address]]` members do not consume fictitious bytes.
`assert_no_padding<T>()` includes reflected type/member context in diagnostics.
This is a representation proof, not a serialization format or an assertion that
every byte sequence establishes a valid object lifetime.

`RegisterLeaf`, `RegisterPassable`, `GprPair`, `sysv_classify`, and
`aapcs64_classify` express separate ABI properties. Classification is conservative
and limited to at most 16 bytes. `CacheAligned<T>` and `Isolated<T>` provide
64-byte and 128-byte alignment respectively; they can introduce tail padding
and do not receive a blanket `NoPadding` proof.

## Reflected structure of arrays

```cpp
#include <uarch/uarch.hpp>

struct Sample {
  std::uint64_t timestamp;
  std::uint64_t count;
};

constexpr bool example() {
  uarch::SoaColumns<Sample, 3> samples;
  samples.set_row(1, Sample{100, 7});
  samples.column<^^Sample::count>()[1] = 9;
  auto matrix = samples.matrix();
  matrix[2, 0] = 101; // row, reflected member index
  return samples.row(1).count == 9 && samples.row(2).timestamp == 101;
}
static_assert(example());
```

`SoaColumns<Record, N>` owns its storage and requires a padding-free class,
positive capacity, and at least one reflected non-static member. Each member
has a separate `CacheAligned<std::array<MemberType, N>>` column. Access through
`column<M>()` returns a fixed-extent span; `column_aligned<M>()` exposes the
aligned wrapper and `column_data<M>()` preserves the 64-byte alignment promise.
Foreign member reflections are rejected. Row gather/scatter uses reflected
members, supports nested array copying, and aborts on an out-of-range index.
Records used for row operations must permit default construction and member
assignment.

For homogeneous non-array member types, `matrix()` returns an mdspan with
shape `[N, column_count()]`. It preserves references to the actual distinct
column arrays through a custom accessor; it does not flatten padding or cross
array-object boundaries with pointer arithmetic. Const containers produce const
views. All spans and matrices borrow the owning container; keep it alive and do
not retain views across its move or destruction. Copying the container copies
its values, not previously obtained views. Heterogeneous records have no matrix; standard mdspan also forbids array element types.

## Typed bit algebra

```cpp
#include <uarch/uarch.hpp>

struct Header {
  using Word = std::uint64_t;
  bool ready [[=uarch::bits::Flag{0}]];
  std::uint8_t count [[=uarch::bits::Field{1, 7}]];
};
constexpr auto header = uarch::BitWord<Header>{}.with_const<^^Header::count, 5>();
static_assert(header.get<^^Header::count>() == 5);
constexpr auto pattern = uarch::BitPattern<Header>{}.field_eq<^^Header::count>(5);
static_assert(pattern.matches(header));
```

`Flag` gives a bit index; `Field` gives a low bit and a width. Reflection rejects
invalid types, overlapping fields and invalid widths. `BitWord::with` explicitly
truncates modulo field width; `try_with` rejects overflow and `with_const` checks
constants. `BitPattern` represents read requirements and detects conflicting
requirements; `BitPatch` represents ordered clear/replace writes. Variadic
helpers update and query distinct reflected flags together:
`BitWord::with_flags`, `without_flags`, `all_of`, `any_of`, and `none_of`;
`BitPattern::flags_set`,
`flags_clear`, `try_flags_set`, and `try_flags_clear`, plus `BitPatch::set_flags`,
`clear_flags`, `try_set_flags`, and `try_clear_flags`. Checked APIs
return `std::expected`, while infallible conflict-sensitive APIs abort in every
profile. `wrapping_add_top_field` is restricted to fields ending at the backing
word's most significant bit. `Bitmap64` provides fixed-width bitmap operations
and `views::ones` iteration.

`Callback<R(Args...)>` is a non-owning, two-word `noexcept` callback; `Delegate<Signature>`
also supports potentially throwing signatures. Include `<uarch/core/flow/delegate.hpp>`
or use the facade exports. Bind a stable receiver with `bind<&T::method>(object)`,
a compile-time free function with `bind<&function>()`, or a captureless lambda with
`bind([](int x) noexcept { return x + 1; })`. Binding adjusts base pointers before
erasure. Targets must outlive invocation; copying a delegate does not extend their lifetime.
Small hardware-register values, including `bool` and trivial union-bearing
`optional`/`expected`/`variant` wrappers, stay by value. This transport eligibility
is separate from the strict padding-free layout concepts. Large value parameters use consuming
`CallArg<T> = T&&` at both erased interfaces: pass `std::move(value)` or explicitly
copy with `T{source}`. The final target's own by-value construction is still required;
a reference-taking target avoids it. Check `if (callback)` before optional invocation.
`from_raw(stub, context)` requires an exactly compatible transport signature.
GCC 16 UBSan still rejects constant evaluation of defaulted equality between
unequal function targets; runtime equality and unsanitized constant evaluation work.

## Predicated dataflow and views

`Predicate` normalizes a boolean into one byte. Its integer masks and
`select_sign_i64` use defined unsigned wrapping math, including `INT64_MIN`.
`select_register` accepts only the closed 32/64-bit machine integer set and emits
x86 conditional moves or AArch64 conditional selects. Both arms are evaluated;
selection does not guard an unsafe load. `select` provides a compiler hint.

`ColdArg` packages one argument for `cold_invoke`, which calls a cold function
without inlining it into the hot path. `cold_call<Fn>` passes machine-word
arguments by value into a cold, non-inlined function. `likely`, `unlikely`,
`cold_path`, `eager_all` and `eager_any` make
control-flow intent explicit. Eager folds evaluate all arguments.
`reduce_lanes<2/4/8>` seeds each lane with the identity, visits every input once,
assigns tails to the first lanes, and combines `init` once at the root. Its
operation must support associative and commutative reassociation; floating-point
addition does not promise bitwise equality after reassociation.

`views::chunk(n)`, `views::enumerate`, and `views::lanes<N>` compose as standard
range adaptor closures over forward ranges. Chunks retain references and the
final partial group. Chunk sizes must be positive. Rvalue containers are owned
through `views::all`; views of lvalue containers still borrow their lifetimes.

## Arithmetic, memory, SIMD and bounded containers

`FastDivisorU32/U64::try_new` rejects zero and precomputes reciprocal parameters.
`divisor_u32<D>()` and `divisor_u64<D>()` reject zero constants. Hot division and
remainder use shifts/multiplication rather than hardware division. The value
types are trivially copyable; valid construction retains exact division
semantics. `PowerOfTwo<Log>` provides division, remainder and checked alignment.
Saturating arithmetic bridges the standard-library naming variants, and
`checked_add/sub/mul` return optionals without exception-based failure paths.

`PoolIndex<Tag>` uses nonzero 32-bit physical slots; `OptionalPoolIndex<Tag>`
uses zero as its four-byte empty encoding. They retain scalar register ABI.
`Pool<T, Log>` borrows exactly `2^Log` initialized objects, including sentinel
slot zero. Checked access rejects zero and out-of-range slots; masked access
explicitly permits wrapping into the sentinel. The owner controls lifetime.

`prefetch_read` is a hint. `prefetch_write` additionally requires a
`PrefetchWrite` capability proving instruction support on every execution CPU;
on x86 it emits `prefetchw` independently of translation-unit target flags.
Both prefetch functions accept a `Locality` template argument: `L1`, `L2`, `L3`,
and `NonTemporal`. Hints do not provide synchronization, ownership or lifetime
extension.
`simd::vec<T, N>` provides fixed-width vector operations. Neither requires a
heap-backed runtime.

`container::inplace_vector<T, N>` is the C++26 P0843 bounded vector: iterators,
`data`/`operator[]`/`at`/`front`/`back`, `resize`/`reserve`, and the insert,
erase, assign, and append modifiers. Capacity and bounds violations call
`std::abort()` because the library is built with `-fno-exceptions`. Fallible
`try_emplace_back`, `try_push_back`, `try_append_range`, and `try_resize` provide
bounded failure handling. `try_resize` leaves the vector unchanged if the requested
size exceeds capacity; `unchecked_*` is the proven-capacity path.
`container::freestanding::inplace_vector<T, N>` deletes the unbounded
constructors and modifiers so a freestanding caller must use `try_*` or
`unchecked_*`. `N == 0` is an empty trivial type. Storage stays inline.

## ISA permits and entry thunks

```cpp
#include <uarch/uarch.hpp>

#if defined(__x86_64__)
struct Gather {
  [[gnu::target("arch=x86-64-v3"), gnu::always_inline]]
  std::uint64_t operator()(uarch::X86V3 token, std::uint64_t v,
                           std::uint64_t mask) const noexcept {
    return token.bit_gather(v, mask);
  }
};
std::uint64_t gather_proven(uarch::X86V3 token, std::uint64_t v,
                            std::uint64_t mask) {
  return uarch::run<Gather{}>(token, v, mask);
}
#endif
```

`Scalar`, `X86V3`, `X86V3SlowPdep`, `X86V4`, and `Aarch64Neon` form the closed
`Arch` set. ISA permits have private minting constructors and nontrivial
destructors, preventing `std::bit_cast` forgery. `Scalar` is safe to construct;
`assume_supported(unchecked_tag)` is an explicit caller proof obligation.
Pin a worker or prove support across every CPU in its affinity mask before
minting or retaining a permit. Tier dispatch performs no hidden CPUID or OS work.

`run_x86_v3`, `run_x86_v3_slow_pdep`, `run_x86_v4`, and `run_aarch64_neon`
provide literal-target entry boundaries. ISA-using Body overloads must declare
the same target as their entry: a generic baseline `always_inline` Body does
not inherit its caller's target in Clang or GCC. Direct baseline calls to x86
token ISA methods are deliberately rejected. Provide per-tier Body overloads
when using `dispatch_kernel`. SlowPdep uses `tzcnt`/`blsr` loops that cannot be
rewritten into microcoded `pext`/`pdep`. SIMD sums wrap modulo 2^64, and scans
return the first matching lane or an empty optional.

## Runtime-free text, hash and layout checks

`text::FixedAscii<N>` is a zero-tailed ASCII buffer of `N` chars. `try_from`
rejects input that is too long, contains an embedded NUL, or is not ASCII.
`from_truncating` keeps a prefix. `as_string_view()` and the member `fnv1a64()`
are constexpr. `hash::fnv1a64` hashes a byte span or `string_view`,
`fnv1a64_extend` continues a hash, and `fnv1a64_literal` hashes a string
literal at compile time. `layout::assert_layout` is a compile-time proof that a
type's reflected layout matches the stated contract.

## Hosted CPU detection

`<uarch/uarch.hpp>` and the `uarch::uarch` target include hosted detection.
Link `uarch::primitives` or `uarch::freestanding` when the binary must not
depend on the detection archive. `uarch::detect::detect()` returns
`std::expected<uarch::arch::Tier, DetectError>` after auditing the worker's
execution domain. It never changes thread affinity. On x86 Linux, a singleton
CPU mask uses direct CPUID/XGETBV; a multi-CPU mask requires readable
`/dev/cpu/N/cpuid` devices for every allowed CPU and an unchanged mask after the
audit. Preserve that domain and OS ISA state for the lifetime of each permit,
including when transferring it to another thread.

The default `CommonSubset` policy falls back to `Scalar` if evidence is
unavailable. `RequireHomogeneous` instead returns an error for unverified or
heterogeneous domains. `detect_prefetch_write()` separately verifies PREFETCHW
across all allowed x86 CPUs and returns an error when evidence is missing.
`classify(source, policy)` and `classify_domain(...)` accept synthetic evidence
for policy decisions, but return only descriptive `TierId` values, never permits.
`HostHardwareAudit::probe()` reports affinity, cache lines, SMT, huge pages,
ISA homogeneity and invariant TSC as `Known`, `Unknown`, or `Unsupported`;
missing files or permissions do not become fabricated hardware facts.

## Build and verification

CMake 4.0+ and Ninja are required. Select a preset for P2996-capable Clang
with `libc++` or GCC 16 with `-freflection`, targeting `glibc` or `musl`.
When the selected compiler is missing and this project is the top-level build,
CMake downloads the matching `v0.0.1` archive (`ubuntu22`, `centos7`,
`amazonlinux2`, or `alinux3`) from
`trunkszi/clang-cross`, `trunkszi/gnu-cross`, or `trunkszi/musl-cross` and
checks its SHA256 digest. `examples/` builds `uarch_example_basic_usage`, which
links `uarch::uarch` and therefore `uarch::detect`.

### Unified build CLI

Install `uv` and run the executable Bash launcher directly. It manages Python
3.12+ and the Typer/Rich/Invoke CLI dependencies under `scripts/cli/`.
Bare `./uarch` is equivalent to `./uarch build`.

```sh
./uarch build
./uarch test -c gcc
./uarch bench --list
./uarch bench -t soa_bench
./uarch verify
./uarch coverage --format html
./uarch format --check
./uarch tidy
./uarch setup
./uarch --format json build --list
```

Build, test, bench, verify and tidy accept `-c clang|gcc|musl-clang|musl-gcc`,
`-d` for debug, `-dev` for ASan/UBSan, and `--preset NAME` for an explicit
CMake preset. Musl development builds use the corresponding glibc sanitizer
preset with a warning. Build/test/bench use `-t NAME` to select a target;
`--help` lists each command's options. `--format json` is a global option placed
before the subcommand. Coverage requires Clang's `llvm-profdata` and `llvm-cov`;
tidy requires an installed reflection-capable `clang-tidy`.

The full compiler spellings `clang-glibc`, `gcc-glibc`, `clang-musl`, and
`gcc-musl` are aliases for those same toolchains. `build --no-tests` disables
both tests and native verifier tools; `test` enables both again, including in a
previously configured tree. `test --no-build` requires both components already
enabled. Coverage uses a separate instrumented tree with native tools disabled.
Default tidy analysis excludes intentional compile-fail sources; explicit source
paths can opt into checking them.

### Native verification tool

`./uarch verify` enables `UARCH_BUILD_TOOLS`, builds `uarch_probes` and
`uarch_verify`, then executes all gates. For direct CMake use:

```sh
cmake --preset clang-glibc-release -DUARCH_BUILD_TOOLS=ON
cmake --build --preset clang-glibc-release --target uarch_verify
build/clang-glibc-release/uarch_verify all
```

The executable supports the canonical gate names below and emits a JSON report.
The Python CLI also accepts the listed short aliases, for example
`./uarch verify abi` or `./uarch verify symbols`:

| Check | `uarch_verify` argument | Python CLI short alias |
| --- | --- | --- |
| All gates | `all` | `all` |
| Source rules | `lint` | `lint` |
| ABI signatures | `abi-check` | `abi` |
| Opcodes and cold sections | `asm-check` | `asm` |
| LLVM MCA analysis | `mca-gate` | `mca` |
| Native objects and symbol sizes | `size-report` | `symbols`, `size` |
| Hardware PMU and estimated Top-Down TMA | `pmu-gate` | `pmu`, `tma` |
| Parametric gem5 O3 differential simulation | `gem5-gate` | `gem5`, `o3` |

The verification pipeline runs source/compile-time contracts, ABI and assembly checks,
multi-probe MCA modeling, then real hardware PMU sampling. MCA visits every probe
with a throughput budget on `znver4`, `znver5`, and `sapphirerapids` by default
(`neoverse-v2` for AArch64). Set `UARCH_MCA_CPU=znver5,sapphirerapids` to select
models, or pass `--mca-cpu` directly to `uarch_verify`. Reference-model budgets
are strict; other selected models allow 25% tolerance. Reports include modeled
Block RThroughput, micro-ops, dispatch width, and bottleneck indicators. These are
[LLVM scheduling-model predictions](https://llvm.org/docs/CommandGuide/llvm-mca.html),
not elapsed hardware latency.

`./uarch verify pmu` checks deterministic probe checksums before collecting Linux
hardware cycles, instructions, branches, branch misses, and available stall/raw
counters. `UARCH_PMU_ITERATIONS` controls operations per probe (default `200000`);
`uarch_verify pmu-gate --pmu-iterations 500000` overrides it. Sampling pins and
restores the calling thread's affinity, replays the same workload for each event,
and includes loop/call overhead. The external `perf stat` fallback also includes
startup overhead. Samples with less than 99% counter running time report `NotRun`.
Kernel policy blocking PMU access likewise reports `NotRun` after checksum checks;
allow user PMU access (`kernel.perf_event_paranoid<=1` or `CAP_PERFMON`) to collect
hardware evidence. See the [Linux PMU interface](https://man7.org/linux/man-pages/man2/perf_event_open.2.html).

PMU reports cycles/op, instructions/op, IPC, branch misses/op, and normalized
Retiring, Bad Speculation, Frontend Bound, and Backend Bound estimates. Supported
Zen raw events feed the slot model; otherwise the generic model uses retired
instructions, stall cycles, and a 16-cycle branch-miss heuristic. These normalized
estimates are diagnostic indicators, not a vendor-certified Top-Down breakdown.
Branchless flow/bit-match probes enforce at most `0.005` branch misses/op.

`./uarch verify gem5` first runs ten mandatory scalar probes including `delegate_dispatch` in the dedicated
`uarch_gem5_probe` guest and checks their position-sensitive checksums against a
host oracle. The guest contains no oracle, CPU detector, affinity management, or
host verification CLI. It parses only a probe name and iteration count, and
emits one `UARCH_GEM5` result line. The freestanding static ELF uses raw Linux
syscalls without dynamic-loader or libc startup. Inputs repeat the byte `(17*i)&255`
across a 64-bit word, with a 256-operation period; SIMD probes use fixed arrays.

The gate locates `gem5.fast` or `gem5.opt` on `PATH`, or uses `UARCH_GEM5_BIN`.
A missing simulator produces `NotRun` after native checksum checks; an invalid
explicit binary path fails. `UARCH_GEM5_CPU` (or native `--gem5-cpu`) selects
comma-separated profiles. `./uarch verify gem5 --gem5-probe delegate_dispatch`
selects comma-separated probe names; omission runs all probes, and unknown names fail:

| Parametric profile | Inspired-by aliases | Width / ROB / IQ / LQ / SQ |
| --- | --- | --- |
| `o3-w6-rob320` | `zen4-o3`, `znver4` | 6 / 320 / 160 / 72 / 64 |
| `o3-w8-rob448` | `zen5-o3`, `znver5` | 8 / 448 / 192 / 96 / 72 |
| `o3-w6-rob512` | `sapphirerapids-o3`, `sapphirerapids` | 6 / 512 / 196 / 128 / 72 |
| `o3-a64-w6-rob320` | `neoverse-v2-o3`, `neoverse-v2` | 6 / 320 / 160 / 84 / 72 |

These are explicit [gem5 O3CPU](https://www.gem5.org/documentation/general_docs/cpu_models/O3CPU)
models, not replicas of vendor silicon. Integer ALU counts match each profile
width, with two integer multiply units. Inspect widths, cache sizes, FU counts,
latency overrides and predictor settings without gem5 using
`python3 scripts/gem5/o3_sim.py --dump-profile-json zen5-o3`.
The model uses TournamentBP by default, with LTAGE and TAGE_SC_L_64KB alternatives.
Its register files provide 768 integer, floating-point, vector and condition-code
entries, plus 128 vector-predicate and 16 matrix entries where supported.
L1 tag/data latencies are 2/2 cycles; L2 tag/data/response latencies are 2/8/2.
The clock is 2 GHz, with 512 MiB memory at 50 ns and 64 GB/s bandwidth.
The JSON export includes all functional-unit classes, latencies and port counts.

Each probe runs at fixed-width counts `0000000512`, `0000002560`, and
`0000004608`, with `GLIBC_TUNABLES=glibc.pthread.rseq=0`. The gate subtracts raw
additive counters, checks cycle/op stationarity within 5%, and recomputes metrics
from the final 2048-operation window. Its `o3-rename-slot` attribution uses rename
width, idle/squash cycles and committed operations; ROB/IQ/LQ/SQ full events stay
separate pressure indicators. Missing rename-slot statistics produce `NotRun`;
missing core counters, inconsistent slot accounting, or bad checksums in mandatory
probes fail. Branch-mispredict and memory-order-violation counters must be present.
Each simulation is bounded by a 120-second host timeout and five billion ticks.

Branchless probes allow at most `0.005` mispredictions/op;
memory-order violations must remain zero. Harness-inclusive ceilings are 256 cycles/op for
`uarch_arch_scalar_gather` and `bit_ones_sum`, and 32 cycles/op for all other probes;
positive finite `UARCH_GEM5_MAX_CYCLES_PER_OP` overrides them. Independent
simulations run on bounded worker threads and reports retain profile/probe sequence.
The profile JSON supplies the slot width used for attribution. Failures of execution, checksums or stats in the five extended BMI/VEX/AVX2
probes are reported per probe as `Unsupported`; scalar gather stays mandatory; all ten mandatory scalar probes including `delegate_dispatch` must pass.

`UARCH_BUILD_TOOLS` defaults to the tests setting when first configured; disable
it explicitly with `-DUARCH_BUILD_TOOLS=OFF` for a build without verifier tools.
The tool requires matching LLVM utilities and configured `clapp` headers (or the
local archive specified by `UARCH_CLAPP_ARCHIVE`). It does not download them.
Missing local clapp inputs produce a configuration error with installation
instructions. Native probes remain free of coverage instrumentation. Opcode and
MCA checks use exact ELF symbol extents, and ABI checking combines Clang IR with
active-compiler value-parameter checks. Exception tables and exception-runtime
references are rejected; ordinary unwind metadata is allowed.
The runtime-free facade does not require this hosted verification component.

### Direct CMake presets

```sh
set -eu
for preset in clang-glibc-release gcc-glibc-release \
              clang-glibc-asan-ubsan gcc-glibc-asan-ubsan; do
  cmake --preset "$preset" -DUARCH_BUILD_BENCHES=ON
  cmake --build --preset "$preset"
  ctest --preset "$preset"
done
```

Tests include runtime differential checks, constant evaluation, and negative
compilation contracts. ISA execution tests use runtime feature and OS-state
guards; unsupported tiers are skipped. An x86 run does not validate NEON
execution. Benchmarks report latency quantiles, throughput and allocation checks;
use native disassembly and PMU measurements to assess code generation and cost.

```sh
cmake --install build/clang-glibc-release --prefix /path/to/install
```

Consumers can use `find_package(uarch CONFIG REQUIRED)` and link `uarch::uarch`
for primitives plus detection, or `uarch::primitives` / `uarch::freestanding`
for the runtime-free surface. The installed package provides
`uarch_configure_runtime` from `UarchRuntime.cmake`. Public compiler options
propagate reflection, C++26, and the no-exceptions/no-RTTI configuration.
