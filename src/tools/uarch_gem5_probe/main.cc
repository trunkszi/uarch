#include <array>
#include <string_view>
#include <uarch/core/arithmetic/fast_divisor_u64.hpp>
#if defined(__x86_64__)
asm(".text\n.global _start\n.type _start,@function\n_start:\n"
    "xor %ebp,%ebp\nmov (%rsp),%rdi\nlea 8(%rsp),%rsi\nand $-16,%rsp\n"
    "call uarch_gem5_entry\nmov %eax,%edi\nmov $231,%eax\nsyscall\nud2\n.size _start,.-_start\n");
long write_stdout(const char* data, std::size_t size) noexcept {
  long result;
  asm volatile("syscall" : "=a"(result) : "a"(1L), "D"(1L), "S"(data), "d"(size) : "rcx", "r11", "memory");
  return result;
}
#elif defined(__aarch64__)
asm(".text\n.global _start\n.type _start,%function\n_start:\n"
    "ldr x0,[sp]\nadd x1,sp,#8\nbl uarch_gem5_entry\nmov x8,#94\nsvc #0\nbrk #0\n.size _start,.-_start\n");
long write_stdout(const char* data, std::size_t size) noexcept {
  register long x0 asm("x0") = 1;
  register const char* x1 asm("x1") = data;
  register std::size_t x2 asm("x2") = size;
  register long x8 asm("x8") = 64;
  asm volatile("svc #0" : "+r"(x0) : "r"(x1), "r"(x2), "r"(x8) : "memory");
  return x0;
}
#endif
extern "C" std::size_t strlen(const char* text) noexcept {
  std::size_t n = 0;
  while (text[n]) {
    asm volatile("" ::: "memory");
    ++n;
  }
  return n;
}
extern "C" void* memcpy(void* dst, const void* src, std::size_t n) noexcept {
  auto* d = static_cast<unsigned char*>(dst);
  const auto* s = static_cast<const unsigned char*>(src);
  for (std::size_t i = 0; i < n; ++i) {
    asm volatile("" ::: "memory");
    d[i] = s[i];
  }
  return dst;
}
extern "C" void* memset(void* dst, int value, std::size_t n) noexcept {
  auto* d = static_cast<unsigned char*>(dst);
  for (std::size_t i = 0; i < n; ++i) {
    asm volatile("" ::: "memory");
    d[i] = static_cast<unsigned char>(value);
  }
  return dst;
}
extern "C" int memcmp(const void* a, const void* b, std::size_t n) noexcept {
  const auto* x = static_cast<const unsigned char*>(a);
  const auto* y = static_cast<const unsigned char*>(b);
  for (std::size_t i = 0; i < n; ++i) {
    asm volatile("" ::: "memory");
    if (x[i] != y[i]) return static_cast<int>(x[i]) - static_cast<int>(y[i]);
  }
  return 0;
}
extern "C" int bcmp(const void* a, const void* b, std::size_t n) noexcept { return memcmp(a, b, n); }
extern "C" {
bool bitword_low_matches(std::uint64_t);
bool bitword_high_matches(std::uint64_t);
std::uint64_t flow_select_register_u64(bool, std::uint64_t, std::uint64_t);
std::uint64_t flow_select_u64(bool, std::uint64_t, std::uint64_t);
std::uint64_t fast_divisor_u64_div(uarch::arithmetic::FastDivisorU64, std::uint64_t);
std::uint32_t fast_divisor_u32_rem(uarch::arithmetic::FastDivisorU32, std::uint32_t);
std::uint64_t uarch_arch_scalar_gather(std::uint64_t, std::uint64_t);
#if defined(__x86_64__)
std::uint32_t bit_ones_sum(std::uint64_t);
std::uint64_t x86_v3_gather(std::uint64_t, std::uint64_t);
std::uint64_t x86_v3_slow_pdep_gather(std::uint64_t, std::uint64_t);
std::int64_t uarch_arch_masked_sum(const std::int64_t*, const std::int64_t*, std::int64_t);
std::uint32_t uarch_arch_first_ge(const std::int64_t*, std::int64_t);
#endif
}
constexpr std::array names{"bitword_low_matches",      "bitword_high_matches",  "flow_select_register_u64",
                           "flow_select_u64",          "fast_divisor_u64_div",  "fast_divisor_u32_rem",
                           "uarch_arch_scalar_gather", "bit_ones_sum",          "x86_v3_gather",
                           "x86_v3_slow_pdep_gather",  "uarch_arch_masked_sum", "uarch_arch_first_ge"};
constexpr std::uint64_t combine(std::uint64_t acc, std::uint64_t index, std::uint64_t value) noexcept {
  return acc * 0x9E3779B97F4A7C15ULL + (value ^ ((index + 1ULL) * 0xBF58476D1CE4E5B9ULL));
}
template <std::size_t Probe>
std::uint64_t run(std::uint64_t n) noexcept {
  constexpr auto d64 = uarch::arithmetic::divisor_u64<97>();
  constexpr auto d32 = uarch::arithmetic::divisor_u32<97>();
  const std::array<std::int64_t, 4> keys{-4, 0, 3, 9}, values{5, 7, 11, 13};
  std::uint64_t acc = 0;
  for (std::uint64_t i = 0; i < n; ++i) {
    std::uint64_t input = ((i * 17) & 255) * 0x0101010101010101ULL;
    asm volatile("" : "+r"(input) : : "memory");
    std::uint64_t value = 0;
    if constexpr (Probe == 0) value = bitword_low_matches(input);
    if constexpr (Probe == 1) value = bitword_high_matches(input);
    if constexpr (Probe == 2) value = flow_select_register_u64((i & 1) == 0, input, ~input);
    if constexpr (Probe == 3) value = flow_select_u64((i & 1) == 0, input, ~input);
    if constexpr (Probe == 4) value = fast_divisor_u64_div(d64, input);
    if constexpr (Probe == 5) value = fast_divisor_u32_rem(d32, static_cast<std::uint32_t>(input));
    if constexpr (Probe == 6) value = uarch_arch_scalar_gather(input, 0x5555555555555555ULL);
#if defined(__x86_64__)
    if constexpr (Probe == 7) value = bit_ones_sum(input);
    if constexpr (Probe == 8) value = x86_v3_gather(input, 0x5555555555555555ULL);
    if constexpr (Probe == 9) value = x86_v3_slow_pdep_gather(input, 0x5555555555555555ULL);
    if constexpr (Probe == 10)
      value =
          static_cast<std::uint64_t>(uarch_arch_masked_sum(keys.data(), values.data(), static_cast<std::int64_t>(i & 7) - 2));
    if constexpr (Probe == 11) value = uarch_arch_first_ge(keys.data(), static_cast<std::int64_t>(i & 7) - 2);
#endif
    asm volatile("" : "+r"(value));
    acc = combine(acc, i, value);
  }
  return acc;
}
extern "C" int uarch_gem5_entry(int argc, char** argv) noexcept {
  if (argc != 5) return 2;
  std::string_view name, number;
  for (int i = 1; i < 5; i += 2) {
    const std::string_view option(argv[i]);
    if (option == "--probe" && name.empty())
      name = argv[i + 1];
    else if (option == "--iterations" && number.empty())
      number = argv[i + 1];
    else
      return 2;
  }
  int base = 10;
  if (number.starts_with("0x") || number.starts_with("0X")) {
    number.remove_prefix(2);
    base = 16;
  }
  if (number.empty() || name.empty()) return 2;
  std::uint64_t n = 0;
  for (char c : number) {
    const unsigned digit = c >= '0' && c <= '9'   ? static_cast<unsigned>(c - '0')
                           : c >= 'a' && c <= 'f' ? static_cast<unsigned>(c - 'a' + 10)
                           : c >= 'A' && c <= 'F' ? static_cast<unsigned>(c - 'A' + 10)
                                                  : 255U;
    if (digit >= static_cast<unsigned>(base) || n > (UINT64_MAX - digit) / static_cast<unsigned>(base)) return 2;
    n = n * static_cast<unsigned>(base) + digit;
  }
  std::size_t probe = 0;
  while (probe < names.size() && name != names[probe]) ++probe;
  std::uint64_t acc = 0;
  switch (probe) {
    case 0:
      acc = run<0>(n);
      break;
    case 1:
      acc = run<1>(n);
      break;
    case 2:
      acc = run<2>(n);
      break;
    case 3:
      acc = run<3>(n);
      break;
    case 4:
      acc = run<4>(n);
      break;
    case 5:
      acc = run<5>(n);
      break;
    case 6:
      acc = run<6>(n);
      break;
#if defined(__x86_64__)
    case 7:
      acc = run<7>(n);
      break;
    case 8:
      acc = run<8>(n);
      break;
    case 9:
      acc = run<9>(n);
      break;
    case 10:
      acc = run<10>(n);
      break;
    case 11:
      acc = run<11>(n);
      break;
#endif
    default:
      return 2;
  }
  char output[256];
  char* end = output;
  auto append = [&](std::string_view text) {
    for (char c : text) *end++ = c;
  };
  auto decimal = [&](std::uint64_t value) {
    char digits[20];
    for (unsigned k = 0; k < 20; ++k) {
      asm volatile("" : "+r"(value));
      digits[19 - k] = static_cast<char>('0' + value % 10);
      value /= 10;
    }
    for (char digit : digits) *end++ = digit;
  };
  append("UARCH_GEM5 probe=");
  append(name);
  append(" n=");
  decimal(n);
  append(" checksum=");
  decimal(acc);
  append("\n");
  const char* start = output;
  while (start != end) {
    const auto written = write_stdout(start, static_cast<std::size_t>(end - start));
    if (written == -4) continue;
    if (written <= 0) return 2;
    start += written;
  }
  return 0;
}
