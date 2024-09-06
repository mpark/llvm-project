// RUN: %clang_cc1 -fsyntax-only -fpattern-matching -Wno-unused-value %s

static_assert([]() -> bool { return 0 match _; }());
static_assert([]() -> bool { return 0 match 0; }());
static_assert(![]() -> bool { return 0 match 1; }());

static_assert([]() -> bool {
  int x = 0;
  return x match 0;
}());

static_assert([]() -> bool {
  int y = 1;
  return !(0 match y);
}());

static_assert([]() -> bool {
  int x = 0;
  int y = 0;
  return x match y;
}());

static_assert([]() -> bool {
  int x = 0;
  int y = 1;
  return !(x match y);
}());
