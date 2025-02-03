// RUN: %clang_cc1 -std=c++2c -fsyntax-only %s

constexpr int f1() {
  return do -> int { do_return 0; };
}

static_assert(f1() == 0);

constexpr int f2() {
  return do { do_return 0; };
}

static_assert(f2() == 0);

constexpr int f3(int x) {
  return do {
    if (x > 0) {
        do_return x;
    } else {
        do_return -x;
    }
  };
}

static_assert(f3(1) == 1);
static_assert(f3(-1) == 1);

constexpr bool f4() {
  int x = 0;
  int &y = do -> int & { do_return x; };
  return &x == &y;
}

static_assert(f4());
