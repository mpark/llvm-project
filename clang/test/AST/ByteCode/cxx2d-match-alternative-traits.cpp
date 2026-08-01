// RUN: %clang_cc1 -std=c++2d -fsyntax-only -fpattern-matching %s

namespace std {
template<class T>
struct alternative_traits;
}

struct Choice {
  unsigned state;
  int first;
  int second;
};

template<__SIZE_TYPE__ I>
struct ChoiceAlternative;

template<>
struct ChoiceAlternative<0> {
  using type = int;
};

template<>
struct ChoiceAlternative<1> {
  using type = int;
};

template<>
struct std::alternative_traits<Choice> {
  static constexpr __SIZE_TYPE__ size = 2;

  template<__SIZE_TYPE__ I>
  using projection_type = typename ChoiceAlternative<I>::type;

  struct names {
    static constexpr __SIZE_TYPE__ first = 0;
    static constexpr __SIZE_TYPE__ second = 1;
  };

  static constexpr __SIZE_TYPE__ index(const Choice& choice) noexcept {
    return choice.state;
  }

  template<__SIZE_TYPE__ I, class Self>
  static constexpr decltype(auto) get(Self&& choice) {
    if constexpr (I == 0)
      return (static_cast<Self&&>(choice).first);
    else
      return (static_cast<Self&&>(choice).second);
  }
};

struct MaybeInt {
  bool engaged;
  int value;
};

template<>
struct std::alternative_traits<MaybeInt> {
  static constexpr __SIZE_TYPE__ size = 2;

  template<__SIZE_TYPE__ I>
    requires (I == 0)
  using projection_type = int;

  static constexpr __SIZE_TYPE__ index(const MaybeInt& value) noexcept {
    return value.engaged ? 0 : 1;
  }

  template<__SIZE_TYPE__ I, class Self>
    requires (I == 0)
  static constexpr decltype(auto) get(Self&& value) {
    return (static_cast<Self&&>(value).value);
  }
};

constexpr int match_choice(Choice choice) {
  return choice match {
    case { .first: int value } => value;
    case { .second: int value } => value + 10;
  };
}

constexpr int match_maybe(MaybeInt value) {
  return value match {
    case { int number } => number;
    case {} => -1;
  };
}

static_assert(match_choice({0, 3, 4}) == 3);
static_assert(match_choice({1, 3, 4}) == 14);
static_assert(match_maybe({true, 5}) == 5);
static_assert(match_maybe({false, 5}) == -1);
