# uarch

面向 C++26 的布局证明、整数位代数、谓词数据流、定长存储和架构内核原语。
公共门面是纯头文件、无运行时框架的接口：包含头文件不会探测 CPU、启动线程、
分配调度运行时或安装自动分派器。

```cpp
#include <uarch/uarch.hpp>

static_assert(uarch::PowerOfTwo<6>::value == 64);
static_assert(uarch::arithmetic::divisor_u64<7>().div(100) == 14);
```

支持配置为带 P2996 反射的 Clang 21 + libc++ 21，以及带反射的 GCC 16.2 +
libstdc++。普通编译器仅传入 `-std=c++26` 并不足够；CMake 会检测所需反射特性。
库的使用方采用禁用异常和 RTTI 的配置。

## 分层架构

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
          (separate; excluded from the umbrella header)
```

CMake 导出 `uarch::tiers`、`uarch::core`、`uarch::arch` 和 `uarch::uarch`。
Core 头文件直接提供 `uarch::layout` 等命名空间。可以使用窄头文件控制依赖，
也可以包含 umbrella header 获得全部无运行时接口。`uarch::prelude` 提供常用类型和
操作。宿主 OS 检测属于独立组件，门面不会包含 `uarch/detect/**`。

## 反射、布局与 ABI

`tiers::TierId` 上的 `TierMeta` 注解生成稳定的元数据表。`tiers::compiled_tier()`
只描述当前翻译单元的编译配置；枚举 ID 和元数据都不能证明执行 CPU 的能力。

`layout::NoPadding<T>` 递归检查成员偏移与对象大小，拒绝填充、联合体、位域、
`bool` 和 `long double`；受支持的空 `[[no_unique_address]]` 成员不会虚增字节。
`assert_no_padding<T>()` 在错误中给出反射得到的类型及成员上下文。这是表示证明，
并不定义序列化协议，也不保证任意字节序列能建立有效对象生命周期。

`RegisterLeaf`、`RegisterPassable`、`GprPair`、`sysv_classify` 与
`aapcs64_classify` 表达不同的 ABI 属性。分类器采用保守策略，最大处理 16 字节。
`CacheAligned<T>` 和 `Isolated<T>` 分别提供 64 与 128 字节对齐；它们可能产生
尾部填充，因此不自动获得 `NoPadding` 证明。

## 反射生成的列式存储

```cpp
#include <uarch/uarch.hpp>

struct Quote {
  std::uint64_t price;
  std::uint64_t quantity;
};

constexpr bool example() {
  uarch::SoaColumns<Quote, 3> quotes;
  quotes.set_row(1, Quote{100, 7});
  quotes.column<^^Quote::quantity>()[1] = 9;
  auto matrix = quotes.matrix();
  matrix[2, 0] = 101;
  return quotes.row(1).quantity == 9 && quotes.row(2).price == 101;
}
static_assert(example());
```

`SoaColumns<Record, N>` 拥有存储，要求 Record 是无填充类、容量为正，且至少具有
一个非静态成员。每列独立存放于 `CacheAligned<std::array<MemberType, N>>`。
`column<M>()` 返回定长 span，`column_aligned<M>()` 返回对齐包装器，
`column_data<M>()` 保留 64 字节对齐承诺。外来类型的成员反射会被编译期拒绝。
行读取与写入通过反射遍历成员，支持嵌套数组复制；索引越界会终止程序。
行操作要求记录允许默认构造和成员赋值。

所有成员为相同的非数组类型时，`matrix()` 返回形状为 `[N, column_count()]`
的 mdspan。自定义 accessor 保持对独立列数组的引用，不会把填充区或不同数组
强行扁平化，也不会跨数组对象边界做指针运算。const 容器产生只读视图。
span 和 matrix 都借用容器存储，必须避免悬空或在容器移动、销毁后继续使用。
容器复制会复制值，之前取得的视图不会自动改为指向副本。异构记录不提供 matrix；标准 mdspan 同样禁止数组元素类型。

## 类型化位代数

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

`Flag` 声明位索引，`Field` 声明低位与位宽。反射检查成员类型、位域重叠及位宽。
`BitWord::with` 明确执行按位宽截断，`try_with` 拒绝溢出，`with_const` 检查常量。
`BitPattern` 表达读取约束并检测冲突，`BitPatch` 表达有顺序的清除与替换写入。
可失败接口返回 `std::expected`；要求不发生冲突的直接接口在所有配置中都会检查
冲突并终止。`wrapping_add_top_field` 仅适用于结束于底层字最高位的字段。
`Bitmap64` 提供定宽位图操作，`views::ones` 提供置位索引迭代。

## 谓词数据流与视图

`Predicate` 把布尔值归一为一个字节，整数掩码和 `select_sign_i64` 使用定义明确的
无符号回绕运算，包括 `INT64_MIN`。`select_register` 只接受封闭的 32/64 位机器
整数集合，在 x86 与 AArch64 上分别使用条件移动和条件选择。两个候选表达式均会
求值，因此它不能保护非法内存读取。`select` 仅提供编译器优化提示。

`cold_call<Fn>` 按值传递机器字到冷路径的非内联函数；另有 `likely`、`unlikely`、
`cold_path`、`eager_all` 与 `eager_any`。Eager 操作会求值全部参数。
`reduce_lanes<2/4/8>` 为每条 lane 放置单位元，每个输入只访问一次，尾元素交给
前几条 lane，初值 `init` 只在根部合并一次。运算需要支持结合律与交换律；浮点
加法的重新结合不保证逐位相同。

`views::chunk(n)`、`views::enumerate` 和 `views::lanes<N>` 使用同一套标准
range adaptor closure 实现，支持 forward range 管道组合，保留引用及最后的不足
整组尾块。块大小必须为正。右值容器由 `views::all` 持有，左值容器的视图仍借用
原容器生命周期。

## 算术、内存、SIMD 与定长容器

`FastDivisorU32/U64::try_new` 拒绝零并预计算倒数参数；`divisor_u32<D>()` 和
`divisor_u64<D>()` 在编译期拒绝零常量。热路径除法和余数使用移位与乘法，无需
硬件除法。这些值类型可平凡复制，有效构造的对象具有精确除法语义。
`PowerOfTwo<Log>` 提供除法、余数和可检查的对齐操作。饱和算术兼容两套标准库的
命名，`checked_add/sub/mul` 使用 optional 表达失败，不依赖异常。

`PoolIndex<Tag>` 是非零 32 位物理槽，`OptionalPoolIndex<Tag>` 用零表示空值，
大小保持四字节；二者均保留标量寄存器 ABI。`Pool<T, Log>` 借用恰好 `2^Log`
个已初始化对象，包括零号哨兵。检查访问拒绝零值与越界索引，掩码访问明确允许
回绕到哨兵。对象生命周期由外部所有者控制。

`prefetch_read` 是缓存提示。`prefetch_write` 还需要 `PrefetchWrite` 能力令牌，
证明所有执行 CPU 支持对应指令；x86 路径独立于翻译单元 target 标志发出
`prefetchw`。提示不提供同步、所有权或生命周期延长。
`simd::vec<T, N>` 提供定宽向量，`container::inplace_vector<T, N>` 提供内联的
定长存储与可失败的 `try_*` 插入，不需要堆分配运行时。

## ISA 能力令牌与入口

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

`Scalar`、`X86V3`、`X86V3SlowPdep`、`X86V4` 和 `Aarch64Neon` 构成封闭的
`Arch` 集合。ISA 令牌只有私有铸造构造函数，并具有非平凡析构函数，因而不能通过
`std::bit_cast` 伪造。`Scalar` 可以安全构造；调用
`assume_supported(unchecked_tag)` 则意味着调用者承担 ISA 支持的证明责任。
获得并保留令牌前，应绑定工作线程 CPU，或证明其亲和性掩码内所有 CPU 都支持该
能力。Tier 分派不隐式执行 CPUID 或 OS 检测。

`run_x86_v3`、`run_x86_v3_slow_pdep`、`run_x86_v4` 与 `run_aarch64_neon`
提供使用字面量 target 的入口。调用 ISA 方法的 Body 重载必须声明与入口匹配的
目标；Clang 和 GCC 都不会让 baseline 的通用 `always_inline` Body 自动继承
调用者 target。baseline 直接调用 x86 token 的 ISA 方法会被编译期拒绝。
`dispatch_kernel` 的 Body 应提供各 tier 的重载。SlowPdep 用 `tzcnt`/`blsr`
汇编循环防止被优化为微码 `pext`/`pdep`。SIMD 求和按模 2^64 回绕，查找返回首个
匹配 lane 或空 optional。

## 宿主 CPU 检测

包含 `<uarch/detect/detect.hpp>` 并链接 `uarch::detect`，即可使用独立的宿主检测
静态库。`uarch::detect::detect()` 审计工作线程的执行域后返回
`std::expected<uarch::arch::Tier, DetectError>`，不会修改线程亲和性。
在 x86 Linux 上，单 CPU 掩码直接使用 CPUID/XGETBV；多 CPU 掩码需要读取所有
允许 CPU 对应的 `/dev/cpu/N/cpuid`，并在审计结束时确认掩码未变。
令牌存活期间必须保持已审计的执行域和 OS ISA 状态，跨线程传递令牌也必须满足此条件。

默认的 `CommonSubset` 策略在证据不可用时回退到 `Scalar`；
`RequireHomogeneous` 对无法验证或不一致的执行域返回错误。
`detect_prefetch_write()` 独立验证所有允许 x86 CPU 的 PREFETCHW 支持，
缺失证据时返回错误。`classify(source, policy)` 与 `classify_domain(...)`
可使用合成证据进行策略判断，但只返回描述性的 `TierId`，不会签发能力令牌。
`HostHardwareAudit::probe()` 将亲和性、缓存行、SMT、大页、ISA 一致性及恒定 TSC
报告为 `Known`、`Unknown` 或 `Unsupported`；缺失文件或权限不会被解释为硬件事实。

## 构建与验证

需要 CMake 3.30+、Ninja 与启用反射的工具链。仓库预设使用
`/root/.local/toolchain` 下的工具链；其他环境可以在配置时覆盖编译器路径。

### 统一构建 CLI

安装 `uv` 后直接运行这个可执行 Bash 启动器。它在 `scripts/cli/` 下管理
Python 3.12+ 以及 Typer/Rich/Invoke CLI 依赖。不带子命令的 `./uarch`
等价于 `./uarch build`。

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

build、test、bench、verify 和 tidy 支持 `-c clang|gcc|musl-clang|musl-gcc`、
调试模式 `-d`、ASan/UBSan 模式 `-dev`，以及显式 CMake 预设 `--preset NAME`。
Musl 开发模式会发出警告并使用对应的 glibc 消毒器预设。
build/test/bench 使用 `-t NAME` 选择目标；各子命令的 `--help` 列出完整选项。
`--format json` 是全局选项，放在子命令前。覆盖率报告需要 Clang 的
`llvm-profdata` 和 `llvm-cov`；tidy 需要安装支持反射的 `clang-tidy`。

完整名称 `clang-glibc`、`gcc-glibc`、`clang-musl` 和 `gcc-musl` 也可作为同一工具链的
别名。`build --no-tests` 同时关闭测试和原生验证工具；`test` 会重新启用二者，
包括已有缓存的构建目录。`test --no-build` 要求二者已启用。覆盖率使用独立插桩目录，
关闭原生验证工具。tidy 默认跳过故意不合法的 compile-fail 源文件；显式传入源路径
可以选择检查这些文件。

### 原生验证工具

`./uarch verify` 启用 `UARCH_BUILD_TOOLS`，构建 `uarch_probes` 与
`uarch_verify`，然后执行全部门禁。也可以直接使用 CMake：

```sh
cmake --preset clang-glibc-release -DUARCH_BUILD_TOOLS=ON
cmake --build --preset clang-glibc-release --target uarch_verify
build/clang-glibc-release/uarch_verify all
```

原生工具支持以下规范门禁参数，并输出 JSON 报告。Python CLI 也支持表中的简写，
例如 `./uarch verify abi` 或 `./uarch verify symbols`：

| 检查 | `uarch_verify` 参数 | Python CLI 简写 |
| --- | --- | --- |
| 全部门禁 | `all` | `all` |
| 源码规则 | `lint` | `lint` |
| ABI 签名 | `abi-check` | `abi` |
| 指令与冷代码段 | `asm-check` | `asm` |
| LLVM MCA 分析 | `mca-gate` | `mca` |
| 原生对象与符号大小 | `size-report` | `symbols`、`size` |
| 硬件 PMU 与 Top-Down TMA 估计 | `pmu-gate` | `pmu`、`tma` |
| 参数化 gem5 O3 差分仿真 | `gem5-gate` | `gem5`、`o3` |

验证流水线依次执行源码与编译期契约、ABI/汇编检查、多探针 MCA 建模，以及真实硬件 PMU 采样。
MCA 默认逐探针建模 `znver4`、`znver5`、`sapphirerapids`，AArch64 默认使用 `neoverse-v2`。
通过 `UARCH_MCA_CPU=znver5,sapphirerapids` 或 `uarch_verify --mca-cpu` 选择模型。
参考模型严格执行预算，其他模型允许 25% 容差；报告吞吐、微操作数、分派宽度与瓶颈指标。
这些是 [LLVM 调度模型预测](https://llvm.org/docs/CommandGuide/llvm-mca.html)，不代表真实硬件耗时。

`./uarch verify pmu` 首先校验确定性探针校验和，再采集硬件周期、指令、分支、分支失误与可用停顿/原始事件。
`UARCH_PMU_ITERATIONS` 设置每个探针的操作数（默认 `200000`）；直接指定
`uarch_verify pmu-gate --pmu-iterations 500000` 可覆盖环境变量。
采样会固定并恢复调用线程亲和性，各事件分别重放同一工作负载，计数包含循环及调用开销；
外部 `perf stat` 回退还包含进程启动开销。计数器运行比例低于 99% 时报告 `NotRun`。
内核策略禁止 PMU 时也会在校验和检查后报告 `NotRun`，可配置 `kernel.perf_event_paranoid<=1`
或授予 `CAP_PERFMON` 后取得硬件证据。参见 [Linux PMU 接口](https://man7.org/linux/man-pages/man2/perf_event_open.2.html)。

报告包含 cycles/op、instructions/op、IPC、branch-misses/op，以及归一化的 Retiring、Bad Speculation、
Frontend Bound、Backend Bound 估计与主导瓶颈。可用 Zen 原始事件用于槽位模型；否则采用指令、停顿周期及
每次分支失误 16 周期的启发式模型。这些诊断估计并非厂商认证的 Top-Down 分解。
无分支选择和位匹配探针要求 branch-misses/op 不超过 `0.005`。

`./uarch verify gem5` 首先运行专用 `uarch_gem5_probe` 的七个必选标量探针，将位置敏感校验和与宿主参考计算比较。
客体程序不运行参考循环，不查询 CPU，不操作亲和性，也不链接宿主验证 CLI，只输出一行 `UARCH_GEM5` 结果。输入将字节 `(17*i)&255` 重复铺入 64 位字，周期为 256 次操作，SIMD 探针使用固定数组。
门禁从 `PATH` 查找 `gem5.fast` / `gem5.opt`，或使用 `UARCH_GEM5_BIN`；未安装时在原生校验后报告 `NotRun`，
显式指定无效路径则失败。`UARCH_GEM5_CPU` 或原生 `--gem5-cpu` 可指定逗号分隔的参数化模型：

| 参数化模型 | 启发来源别名 | 宽度 / ROB / IQ / LQ / SQ |
| --- | --- | --- |
| `o3-w6-rob320` | `zen4-o3`、`znver4` | 6 / 320 / 160 / 72 / 64 |
| `o3-w8-rob448` | `zen5-o3`、`znver5` | 8 / 448 / 192 / 96 / 72 |
| `o3-w6-rob512` | `sapphirerapids-o3`、`sapphirerapids` | 6 / 512 / 196 / 128 / 72 |
| `o3-a64-w6-rob320` | `neoverse-v2-o3`、`neoverse-v2` | 6 / 320 / 160 / 84 / 72 |

这些是显式配置的 [gem5 O3CPU](https://www.gem5.org/documentation/general_docs/cpu_models/O3CPU) 模型，并非厂商芯片复刻。
整数 ALU 数量与配置宽度一致，整数乘法单元数量为二。无需安装 gem5 即可通过 `python3 scripts/gem5/o3_sim.py --dump-profile-json zen5-o3` 查看缓存、FU 延迟与预测器配置；
模型默认使用 TournamentBP，并支持 LTAGE / TAGE_SC_L_64KB。
整数、浮点、向量与条件码寄存器各 768 项；支持时配置 128 项向量谓词与 16 项矩阵寄存器。
L1 标签/数据延迟为 2/2 周期，L2 标签/数据/响应延迟为 2/8/2 周期。
时钟为 2 GHz，内存为 512 MiB、50 ns 延迟与 64 GB/s 带宽；JSON 导出完整功能单元与端口配置。

每个探针执行 `0000000512`、`0000002560`、`0000004608` 三个固定宽度参数，设置
`GLIBC_TUNABLES=glibc.pthread.rseq=0`。只对原始可加计数器作差，周期/操作斜率漂移不得超过 5%，
再从最后 2048 次操作计算指标。`o3-rename-slot` 使用重命名宽度、空闲/冲刷周期及提交操作数归因，
ROB/IQ/LQ/SQ 满事件仅作为独立压力指标。缺少重命名槽位统计报告 `NotRun`；必选探针的核心统计缺失、槽位不一致或校验和错误均失败。分支误预测及内存顺序违规计数器必须存在。仿真受 120 秒宿主超时及 50 亿 tick 上限约束。
无分支探针要求误预测/操作不超过 `0.005`，内存顺序违规必须为零。
含循环开销的预算为 `uarch_arch_scalar_gather` 与 `bit_ones_sum` 256 cycles/op，其余探针 32 cycles/op，
可用正有限数 `UARCH_GEM5_MAX_CYCLES_PER_OP` 覆盖。独立仿真通过有限工作线程并行执行，报告按配置与探针顺序输出。
归因槽位宽度来自配置 JSON；独立静态 ELF 使用原始 Linux 系统调用，避免动态加载器与 libc 启动开销。
仅五个扩展 BMI/VEX/AVX2 探针的执行、校验和或统计失败逐探针报告 `Unsupported`；标量 gather 仍为必选探针，七个必选标量探针必须全部通过。

首次配置时，`UARCH_BUILD_TOOLS` 默认与测试开关一致；无需验证工具时可显式设置
`-DUARCH_BUILD_TOOLS=OFF`。工具需要匹配的 LLVM 工具，以及已配置的 `clapp`
头文件或 `UARCH_CLAPP_ARCHIVE` 指定的本地归档，不会自动下载这些依赖。
缺少本地 clapp 时，配置会明确报错并提示安装方式。原生探针不继承覆盖率插桩。
指令及 MCA 检查按 ELF 符号的精确大小切片，ABI 检查结合 Clang IR 与当前编译器的
值参数访问检查。异常表和异常运行时依赖会被拒绝，普通栈展开元数据则允许保留。
无运行时框架的公共门面不依赖这个宿主验证组件。

### 直接使用 CMake 预设

```sh
set -eu
for preset in clang-glibc-release gcc-glibc-release \
              clang-glibc-asan-ubsan gcc-glibc-asan-ubsan; do
  cmake --preset "$preset" -DUARCH_BUILD_BENCHES=ON
  cmake --build --preset "$preset"
  ctest --preset "$preset"
done
```

测试包含运行期差分、常量求值和编译失败契约。ISA 执行测试按 CPU 特性及 OS 状态
进行守卫，跳过不支持的 tier；x86 测试不能证明 NEON 的执行正确性。基准提供延迟
分位数、吞吐量及分配检查；指令生成与成本需要结合原生反汇编和 PMU 测量判断。

```sh
cmake --install build/clang-glibc-release --prefix /path/to/install
```

使用方可通过 `find_package(uarch CONFIG REQUIRED)` 和 `uarch::uarch` 接入，
也可选择更窄的接口目标。公共选项会传递反射、C++26 和禁用异常、RTTI 的配置。
