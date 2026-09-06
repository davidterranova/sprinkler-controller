#pragma once
//
//  A ~60-line test harness, on purpose.
//
//  The alternative is a dependency (gtest, Catch2, doctest) that CI must fetch
//  before it can tell you whether the DST table still passes. For a handful of
//  pure functions that trade is a bad one: this file has no build step, no
//  network access, and nothing to pin.
//
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace testing {

struct Case {
  const char *name;
  void (*fn)();
};

inline std::vector<Case> &registry() {
  static std::vector<Case> cases;
  return cases;
}

inline int &failures() {
  static int n = 0;
  return n;
}

inline const char *&current() {
  static const char *name = "";
  return name;
}

struct Registrar {
  Registrar(const char *name, void (*fn)()) { registry().push_back({name, fn}); }
};

inline void fail(const char *file, int line, const std::string &what) {
  failures()++;
  std::fprintf(stderr, "  FAIL %s\n    %s:%d: %s\n", current(), file, line, what.c_str());
}

template<typename A, typename B>
void check_eq(const char *file, int line, const char *expr, const A &a, const B &b) {
  if (!(a == b)) {
    char buf[512];
    std::snprintf(buf, sizeof(buf), "%s -- got %lld, want %lld", expr, static_cast<long long>(a),
                  static_cast<long long>(b));
    fail(file, line, buf);
  }
}

inline int run_all(const char *suite) {
  std::printf("== %s ==\n", suite);
  for (auto &c : registry()) {
    current() = c.name;
    const int before = failures();
    c.fn();
    std::printf("  %s %s\n", failures() == before ? "ok  " : "FAIL", c.name);
  }
  if (failures() > 0)
    std::printf("== %s: %d failure(s) ==\n", suite, failures());
  else
    std::printf("== %s: all %zu tests passed ==\n", suite, registry().size());
  return failures() == 0 ? 0 : 1;
}

}  // namespace testing

#define TEST(name)                                             \
  static void name();                                          \
  static ::testing::Registrar registrar_##name(#name, &name);  \
  static void name()

#define EXPECT_EQ(a, b) ::testing::check_eq(__FILE__, __LINE__, #a " == " #b, (a), (b))
#define EXPECT_TRUE(a)                                                     \
  do {                                                                     \
    if (!(a))                                                              \
      ::testing::fail(__FILE__, __LINE__, "expected true: " #a);           \
  } while (0)
#define EXPECT_FALSE(a)                                                    \
  do {                                                                     \
    if (a)                                                                 \
      ::testing::fail(__FILE__, __LINE__, "expected false: " #a);          \
  } while (0)
#define EXPECT_STREQ(a, b)                                                              \
  do {                                                                                  \
    if (std::strcmp((a), (b)) != 0)                                                     \
      ::testing::fail(__FILE__, __LINE__, std::string("expected \"") + (b) +            \
                                              "\", got \"" + (a) + "\"");               \
  } while (0)
