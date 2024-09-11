// RUN: %clang_cc1 -fsyntax-only -fpattern-matching -Wno-unused-value %s

void test_decltypes() {
  constexpr int x = 0;
  constexpr int y = 0;
  static_assert(__is_same(decltype(0 match _), bool));
  static_assert(__is_same(decltype(x match 0), bool));
  static_assert(__is_same(decltype(x match y), bool));
  static_assert(__is_same(decltype(&x match ? _), bool));
  static_assert(__is_same(decltype(&x match ? 0), bool));
}

static_assert([]() { return 0 match _; }());
static_assert([]() { int x = 0; return x match _; }());
static_assert([]() { int* p = nullptr; return p match _; }());

static_assert([]() { return 0 match 0; }());
static_assert(![]() { return 0 match 1; }());
static_assert([]() { int x = 0; return x match 0; }());
static_assert(![]() { int y = 1; return 0 match y; }());
static_assert([]() { int x = 0, y = 0; return x match y; }());
static_assert(![]() { int x = 0, y = 1; return x match y; }());

static_assert([]() { int x = 0; return &x match ? _; }());
static_assert([]() { int x = 0; return &x match ? 0; }());
static_assert(![]() { int x = 0; return &x match ? 1; }());
static_assert([]() { int x = 0, y = 0; return &x match ? y; }());
static_assert(![]() { int x = 0, y = 1; return &x match ? y; }());
static_assert(![]() { int* p = nullptr; return p match ? _; }());
static_assert(![]() { int* p = nullptr; return p match ? 0; }());

static_assert([]() { int x = 0, *p = &x; return &p match ?? _; }());
static_assert([]() { int x = 0, *p = &x; return &p match ?? 0; }());
static_assert(![]() { int x = 0, *p = &x; return &p match ?? 1; }());

static_assert([]() { int x = 0, *p = &x; return &p match ? _; }());
static_assert([]() { int x = 0, *p = &x; return &p match ?? 0; }());
static_assert(![]() { int x = 0, *p = &x; return &p match ?? 1; }());
static_assert(![]() { int** pp = nullptr; return pp match ? _; }());
static_assert(![]() { int** pp = nullptr; return pp match ?? _; }());
static_assert(![]() { int** pp = nullptr; return pp match ?? 0; }());

static_assert([]() { return 0 match let _; }());
static_assert([]() { return 0 match let x; }());
static_assert([]() { int x = 0; return &x match ? let x; }());

constexpr int test(char c) {
  return c match {
    'a' => 1;
    'b' => 2;
    let x => int(x);
  };
}

static_assert(test('a') == 1);
static_assert(test('b') == 2);
static_assert(test('c') == 99);

constexpr int test_decomposition_pattern(const int (&xs)[2]) {
  return xs match {
    [0, 0] => -1;
    [let x, 0] => x * 2;
    [0, let y] => y * 4;
    let [x, y] => x * y;
  };
}

static_assert(test_decomposition_pattern({0, 0}) == -1);
static_assert(test_decomposition_pattern({0, 0}) != 0);
static_assert(test_decomposition_pattern({1, 0}) == 2);
static_assert(test_decomposition_pattern({1, 0}) != 3);
static_assert(test_decomposition_pattern({2, 0}) == 4);
static_assert(test_decomposition_pattern({0, 1}) == 4);
static_assert(test_decomposition_pattern({0, 2}) == 8);
static_assert(test_decomposition_pattern({2, 3}) == 6);
static_assert(test_decomposition_pattern({3, 4}) == 12);

enum State { FizzBuzz, Fizz, Buzz, N };
constexpr int Size = 15;

constexpr bool fizzbuzz(const State (&states)[Size], const int (&elems)[Size]) {
  bool result = true;
  for (int i = 1; i <= N; ++i) {
    State s = states[i - 1];
    int n = elems[i - 1];
    result &= (int[2]){i % 3, i % 5} match {
      [0, 0] => s == FizzBuzz && n == 0;
      [0, let y] => s == Fizz && n == y;
      [let x, 0] => s == Buzz && n == x;
      let [x, y] => s == N && n == x + y;
    };
  }
  return result;
}

static_assert(fizzbuzz(
  {N, N, Fizz, N, Buzz, Fizz, N, N, Fizz, Buzz, N, Fizz, N, N, FizzBuzz},
  {2, 4, 3,    5, 2,    1,    3, 5, 4,    1,    3, 2,    4, 6, 0       }
));

static_assert(!fizzbuzz(
  {N, N, N, N, Buzz, Fizz, N, N, Fizz, Buzz, N, Fizz, N, N, FizzBuzz},
  {2, 4, 3, 5, 2,    1,    3, 5, 4,    1,    3, 2,    4, 6, 0       }
));
