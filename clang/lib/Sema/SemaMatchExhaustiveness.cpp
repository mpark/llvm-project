//===--- SemaMatchExhaustiveness.cpp - Pattern exhaustiveness checking ----===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Implements a conservative usefulness check for C++ pattern matching.
///
//===----------------------------------------------------------------------===//

#include "clang/AST/ExprCXX.h"
#include "clang/AST/MatchPattern.h"
#include "clang/AST/Type.h"
#include "clang/Sema/Sema.h"

#include <memory>
#include <optional>

using namespace clang;

namespace {

enum class Usefulness { NotUseful, MaybeUseful, Useful };
enum class ConstructorDomain { Required, RequiredAndResidual };

QualType typeIdType(ASTContext &Context, QualType Type) {
  Qualifiers Quals;
  return Context.getUnqualifiedArrayType(Type.getNonReferenceType(), Quals);
}

struct CtorKey {
  enum Kind {
    Wildcard,
    Bool,
    Integer,
    IntegerRest,
    Enum,
    EnumRest,
    Product,
    Alternative,
    AlternativeRest,
    OpenAlternative,
    OpenAlternativeRest,
    OpenAlternativeEmpty
  } K;
  bool BoolValue = false;
  llvm::APSInt IntegralValue;
  const EnumConstantDecl *Enumerator = nullptr;
  unsigned Arity = 0;
  SmallVector<QualType, 4> FieldTypes;
  QualType AlternativeOwnerType;
  unsigned AlternativeIndex = 0;
  SmallVector<QualType, 4> AlternativeTypes;
  SmallVector<unsigned char, 4> ProjectableAlternatives;
  bool IsExhaustive = true;
  QualType OpenAlternativeType;
  bool OpenAlternativeHasEmpty = false;

  static CtorKey wildcardCtor() {
    CtorKey C;
    C.K = Wildcard;
    return C;
  }

  static CtorKey boolCtor(bool Value) {
    CtorKey C;
    C.K = Bool;
    C.BoolValue = Value;
    return C;
  }

  static CtorKey integerCtor(llvm::APSInt Value) {
    CtorKey C;
    C.K = Integer;
    C.IntegralValue = std::move(Value);
    return C;
  }

  static CtorKey integerRestCtor(llvm::APSInt Witness) {
    CtorKey C;
    C.K = IntegerRest;
    C.IntegralValue = std::move(Witness);
    return C;
  }

  static CtorKey enumCtor(llvm::APSInt Value,
                          const EnumConstantDecl *Enumerator = nullptr) {
    CtorKey C;
    C.K = Enum;
    C.IntegralValue = std::move(Value);
    C.Enumerator = Enumerator;
    return C;
  }

  static CtorKey enumRestCtor() {
    CtorKey C;
    C.K = EnumRest;
    return C;
  }

  static CtorKey productCtor(unsigned Arity,
                             ArrayRef<QualType> FieldTypes = {}) {
    CtorKey C;
    C.K = Product;
    C.Arity = Arity;
    C.FieldTypes.append(FieldTypes.begin(), FieldTypes.end());
    return C;
  }

  static CtorKey alternativeCtor(QualType OwnerType, unsigned Index,
                                 ArrayRef<QualType> Types,
                                 ArrayRef<unsigned char> Projectable,
                                 bool IsExhaustive) {
    CtorKey C;
    C.K = Alternative;
    C.AlternativeOwnerType = OwnerType;
    C.AlternativeIndex = Index;
    C.AlternativeTypes.append(Types.begin(), Types.end());
    C.ProjectableAlternatives.append(Projectable.begin(), Projectable.end());
    C.IsExhaustive = IsExhaustive;
    return C;
  }

  static CtorKey alternativeRestCtor(QualType OwnerType) {
    CtorKey C;
    C.K = AlternativeRest;
    C.AlternativeOwnerType = OwnerType;
    return C;
  }

  static CtorKey openAlternativeCtor(QualType OwnerType, QualType Type,
                                     bool HasEmpty) {
    CtorKey C;
    C.K = OpenAlternative;
    C.AlternativeOwnerType = OwnerType;
    C.OpenAlternativeType = Type;
    C.OpenAlternativeHasEmpty = HasEmpty;
    return C;
  }

  static CtorKey openAlternativeRestCtor(QualType OwnerType, bool HasEmpty) {
    CtorKey C;
    C.K = OpenAlternativeRest;
    C.AlternativeOwnerType = OwnerType;
    C.OpenAlternativeHasEmpty = HasEmpty;
    return C;
  }

  static CtorKey openAlternativeEmptyCtor(QualType OwnerType) {
    CtorKey C;
    C.K = OpenAlternativeEmpty;
    C.AlternativeOwnerType = OwnerType;
    C.OpenAlternativeHasEmpty = true;
    return C;
  }

  bool operator==(const CtorKey &Other) const {
    if (K != Other.K)
      return false;
    switch (K) {
    case Wildcard:
      return true;
    case Bool:
      return BoolValue == Other.BoolValue;
    case Integer:
      return IntegralValue == Other.IntegralValue;
    case IntegerRest:
      return true;
    case Enum:
      return IntegralValue == Other.IntegralValue;
    case EnumRest:
      return true;
    case Product:
      return Arity == Other.Arity;
    case Alternative:
      return AlternativeOwnerType.getCanonicalType() ==
                 Other.AlternativeOwnerType.getCanonicalType() &&
             AlternativeIndex == Other.AlternativeIndex;
    case AlternativeRest:
      return AlternativeOwnerType.getCanonicalType() ==
             Other.AlternativeOwnerType.getCanonicalType();
    case OpenAlternative:
      return AlternativeOwnerType.getCanonicalType() ==
                 Other.AlternativeOwnerType.getCanonicalType() &&
             OpenAlternativeType.getCanonicalType() ==
                 Other.OpenAlternativeType.getCanonicalType();
    case OpenAlternativeRest:
    case OpenAlternativeEmpty:
      return AlternativeOwnerType.getCanonicalType() ==
             Other.AlternativeOwnerType.getCanonicalType();
    }
    llvm_unreachable("unhandled constructor kind");
  }
};

struct CoveragePattern {
  enum Kind { Wild, Ctor, OpenProjectable, Opaque } K = Opaque;
  CtorKey C = CtorKey::productCtor(0);
  SmallVector<std::shared_ptr<CoveragePattern>, 4> Fields;
  SmallVector<QualType, 4> FieldTypes;
  SourceLocation Loc;

  static CoveragePattern wild(SourceLocation Loc = {}) {
    CoveragePattern P;
    P.K = Wild;
    P.Loc = Loc;
    return P;
  }

  static CoveragePattern opaque(SourceLocation Loc = {}) {
    CoveragePattern P;
    P.K = Opaque;
    P.Loc = Loc;
    return P;
  }

  static CoveragePattern ctor(CtorKey C, SourceLocation Loc = {}) {
    CoveragePattern P;
    P.K = Ctor;
    P.C = std::move(C);
    P.Loc = Loc;
    return P;
  }

  static CoveragePattern openProjectable(CtorKey C, SourceLocation Loc = {}) {
    CoveragePattern P;
    P.K = OpenProjectable;
    P.C = std::move(C);
    P.Loc = Loc;
    return P;
  }
};

using PatternRow = SmallVector<CoveragePattern, 4>;
using TypeRow = SmallVector<QualType, 4>;
using CoveragePatterns = SmallVector<CoveragePattern, 4>;

struct IntegralValueDomain {
  llvm::APSInt Min;
  llvm::APSInt Max;

  bool contains(const llvm::APSInt &Value) const {
    return llvm::APSInt::compareValues(Min, Value) <= 0 &&
           llvm::APSInt::compareValues(Value, Max) <= 0;
  }
};

llvm::APSInt convertToIntegerLikeType(ASTContext &Ctx, llvm::APSInt V,
                                      QualType T) {
  T = T.getNonReferenceType().getUnqualifiedType();
  if (T->isBooleanType())
    return llvm::APSInt(llvm::APInt(1, !V.isZero()), true);

  QualType IntegerType =
      T->isEnumeralType() ? T->castAsEnumDecl()->getIntegerType() : T;
  unsigned Width = Ctx.getIntWidth(IntegerType);
  bool IsSigned = IntegerType->isSignedIntegerType();
  V = V.extOrTrunc(Width);
  V.setIsSigned(IsSigned);
  return V;
}

std::optional<IntegralValueDomain> integralValueDomain(ASTContext &Ctx,
                                                       QualType T) {
  T = T.getNonReferenceType().getUnqualifiedType();
  if (T->isBooleanType())
    return IntegralValueDomain{llvm::APSInt(llvm::APInt(1, 0), true),
                               llvm::APSInt(llvm::APInt(1, 1), true)};

  if (T->isIntegerType() && !T->isEnumeralType()) {
    unsigned Width = Ctx.getIntWidth(T);
    bool IsUnsigned = T->isUnsignedIntegerType();
    return IntegralValueDomain{llvm::APSInt::getMinValue(Width, IsUnsigned),
                               llvm::APSInt::getMaxValue(Width, IsUnsigned)};
  }

  if (!T->isEnumeralType())
    return std::nullopt;

  const EnumDecl *ED = T->castAsEnumDecl()->getDefinition();
  if (!ED || !ED->isCompleteDefinition())
    return std::nullopt;

  QualType IntegerType = ED->getIntegerType();
  unsigned Width = Ctx.getIntWidth(IntegerType);
  bool IsUnsigned = IntegerType->isUnsignedIntegerType();
  if (ED->isFixed())
    return IntegralValueDomain{llvm::APSInt::getMinValue(Width, IsUnsigned),
                               llvm::APSInt::getMaxValue(Width, IsUnsigned)};

  unsigned NumNegativeBits = ED->getNumNegativeBits();
  unsigned NumPositiveBits = ED->getNumPositiveBits();
  if (!NumNegativeBits) {
    llvm::APSInt Min(llvm::APInt::getZero(Width), IsUnsigned);
    llvm::APSInt Max(llvm::APInt::getLowBitsSet(Width, NumPositiveBits),
                     IsUnsigned);
    return IntegralValueDomain{std::move(Min), std::move(Max)};
  }

  unsigned NumBits = std::max(NumNegativeBits, NumPositiveBits + 1);
  llvm::APSInt Min(llvm::APInt::getSignedMinValue(NumBits).sextOrTrunc(Width),
                   false);
  llvm::APSInt Max(llvm::APInt::getSignedMaxValue(NumBits).sextOrTrunc(Width),
                   false);
  return IntegralValueDomain{std::move(Min), std::move(Max)};
}

const EnumConstantDecl *findEnumerator(ASTContext &Ctx, QualType T,
                                       llvm::APSInt &Value) {
  const EnumDecl *ED = T->castAsEnumDecl()->getDefinition();
  if (!ED)
    return nullptr;

  Value = convertToIntegerLikeType(Ctx, Value, T);
  for (const EnumConstantDecl *ECD : ED->enumerators()) {
    llvm::APSInt EnumValue = ECD->getInitVal();
    EnumValue = convertToIntegerLikeType(Ctx, EnumValue, T);
    if (EnumValue == Value)
      return ECD;
  }
  return nullptr;
}

std::optional<CoveragePatterns> constantPatternsFor(Sema &S, Expr *Condition,
                                                    QualType SubjectType,
                                                    SourceLocation Loc) {
  auto *Comparison = dyn_cast_or_null<BinaryOperator>(Condition);
  if (!Comparison || Comparison->getOpcode() != BO_EQ)
    return std::nullopt;

  Expr::EvalResult Result;
  if (!Comparison->getRHS()->EvaluateAsInt(Result, S.Context))
    return std::nullopt;

  SubjectType = SubjectType.getNonReferenceType().getUnqualifiedType();
  std::optional<IntegralValueDomain> Domain =
      integralValueDomain(S.Context, SubjectType);
  if (!Domain)
    return std::nullopt;

  // Reverse the conversions applied by built-in ==, then verify the candidate
  // by converting it forward again. This distinguishes an unsigned subject
  // compared with -1 from an unsigned char subject compared with 256.
  llvm::APSInt Compared = convertToIntegerLikeType(
      S.Context, Result.Val.getInt(), Comparison->getRHS()->getType());
  llvm::APSInt Value =
      convertToIntegerLikeType(S.Context, Compared, SubjectType);
  llvm::APSInt ConvertedSubject = convertToIntegerLikeType(
      S.Context, Value, Comparison->getLHS()->getType());
  if (!Domain->contains(Value) || ConvertedSubject != Compared)
    return CoveragePatterns{};

  if (SubjectType->isBooleanType())
    return CoveragePatterns{
        CoveragePattern::ctor(CtorKey::boolCtor(!Value.isZero()), Loc)};

  if (SubjectType->isIntegerType() && !SubjectType->isEnumeralType())
    return CoveragePatterns{
        CoveragePattern::ctor(CtorKey::integerCtor(std::move(Value)), Loc)};

  if (SubjectType->isEnumeralType()) {
    const EnumConstantDecl *ECD = findEnumerator(S.Context, SubjectType, Value);
    return CoveragePatterns{
        CoveragePattern::ctor(CtorKey::enumCtor(std::move(Value), ECD), Loc)};
  }

  return std::nullopt;
}

CoveragePatterns makePatterns(Sema &S, MatchPattern *Pattern,
                              const MatchPatternInstantiation *Instantiation,
                              QualType Type) {
  Type = Type.getNonReferenceType().getUnqualifiedType();
  switch (Pattern->getMatchPatternClass()) {
  case MatchPattern::WildcardPatternClass:
    return {CoveragePattern::wild(Pattern->getBeginLoc())};

  case MatchPattern::DeclarationPatternClass: {
    auto *P = static_cast<DeclarationPattern *>(Pattern);
    const MatchPatternInfo *Info = Instantiation->find(P);
    if (Info && Info->Projection &&
        Info->Projection->getKind() == MatchProjection::CastProjection)
      return {CoveragePattern::opaque(Pattern->getBeginLoc())};
    return {CoveragePattern::wild(Pattern->getBeginLoc())};
  }

  case MatchPattern::TypePatternClass: {
    auto *P = static_cast<TypePattern *>(Pattern);
    const MatchPatternInfo *Info = Instantiation->find(P);
    if (!Info || !Info->TypePatternResolved)
      return {CoveragePattern::opaque(Pattern->getBeginLoc())};
    if (Info->Projection)
      return {CoveragePattern::opaque(Pattern->getBeginLoc())};
    if (!Info->TypePatternMatches)
      return {};
    return {CoveragePattern::wild(Pattern->getBeginLoc())};
  }

  case MatchPattern::ExpressionPatternClass: {
    auto *P = static_cast<ExpressionPattern *>(Pattern);
    const MatchPatternInfo *Info = Instantiation->find(P);
    if (auto Constants = constantPatternsFor(
            S, Info ? Info->Condition : nullptr, Type, P->getBeginLoc()))
      return std::move(*Constants);
    return {CoveragePattern::opaque(Pattern->getBeginLoc())};
  }

  case MatchPattern::DecompositionPatternClass: {
    auto *P = static_cast<DecompositionPattern *>(Pattern);
    CoveragePattern Initial = CoveragePattern::ctor(
        CtorKey::productCtor(P->getNumPatterns()), P->getBeginLoc());
    CoveragePatterns Results = {std::move(Initial)};
    const MatchPatternInfo *Info = Instantiation->find(P);
    DecompositionDecl *DD = Info && Info->Projection
                                ? Info->Projection->getDecomposedDecl()
                                : nullptr;
    unsigned I = 0;
    for (MatchPattern *Child : P->children()) {
      QualType FieldType = S.Context.DependentTy;
      if (DD && I < DD->bindings().size())
        FieldType = DD->bindings()[I]->getBinding()->getType();
      CoveragePatterns Children =
          makePatterns(S, Child, Instantiation, FieldType);
      CoveragePatterns Expanded;
      for (const CoveragePattern &Result : Results) {
        for (const CoveragePattern &ChildPattern : Children) {
          CoveragePattern Copy = Result;
          Copy.FieldTypes.push_back(FieldType);
          Copy.Fields.push_back(
              std::make_shared<CoveragePattern>(ChildPattern));
          Expanded.push_back(std::move(Copy));
        }
      }
      Results = std::move(Expanded);
      ++I;
    }
    for (CoveragePattern &Result : Results)
      Result.C.FieldTypes = Result.FieldTypes;
    return Results;
  }

  case MatchPattern::AlternativePatternClass: {
    auto *P = static_cast<AlternativePattern *>(Pattern);
    const MatchPatternInfo *Info = Instantiation->find(P);
    if (Info && Info->IsOpenAlternative) {
      if (P->isEmpty())
        return {CoveragePattern::ctor(CtorKey::openAlternativeEmptyCtor(Type),
                                      P->getBeginLoc())};
      if (Info->OpenAlternativeProjectableWildcard)
        return {CoveragePattern::openProjectable(
            CtorKey::openAlternativeRestCtor(Type,
                                             Info->OpenAlternativeHasEmpty),
            P->getBeginLoc())};
      if (Info->OpenAlternativeType.isNull())
        return {CoveragePattern::opaque(P->getBeginLoc())};

      CtorKey C = CtorKey::openAlternativeCtor(
          Type, typeIdType(S.Context, Info->OpenAlternativeType),
          Info->OpenAlternativeHasEmpty);
      CoveragePattern Initial =
          CoveragePattern::ctor(std::move(C), P->getBeginLoc());
      CoveragePatterns Results;
      QualType FieldType = Info->OpenAlternativeType;
      if (Info->Projection && Info->Projection->getProjectedExpr())
        FieldType = Info->Projection->getProjectedExpr()->getType();
      for (CoveragePattern &Child :
           makePatterns(S, P->getSubPattern(), Instantiation, FieldType)) {
        CoveragePattern Result = Initial;
        Result.FieldTypes.push_back(FieldType);
        Result.Fields.push_back(
            std::make_shared<CoveragePattern>(std::move(Child)));
        Results.push_back(std::move(Result));
      }
      return Results;
    }
    if (!Info || Info->SelectedAlternatives.empty())
      return {CoveragePattern::opaque(Pattern->getBeginLoc())};

    CoveragePatterns Results;
    for (unsigned Index : Info->SelectedAlternatives) {
      CtorKey C = CtorKey::alternativeCtor(Type, Index, Info->AlternativeTypes,
                                           Info->ProjectableAlternatives,
                                           Info->IsExhaustive);
      CoveragePattern Result =
          CoveragePattern::ctor(std::move(C), P->getBeginLoc());
      if (!Info->ProjectableAlternatives[Index]) {
        Results.push_back(std::move(Result));
        continue;
      }

      QualType FieldType = Info->AlternativeTypes[Index];
      if (Info->Projection && Info->Projection->getProjectedExpr())
        FieldType = Info->Projection->getProjectedExpr()->getType();
      for (CoveragePattern &Child :
           makePatterns(S, P->getSubPattern(), Instantiation, FieldType)) {
        CoveragePattern Copy = Result;
        Copy.FieldTypes.push_back(FieldType);
        Copy.Fields.push_back(
            std::make_shared<CoveragePattern>(std::move(Child)));
        Results.push_back(std::move(Copy));
      }
    }
    return Results;
  }
  }
  llvm_unreachable("unhandled pattern kind");
}

void appendCtorFields(const CoveragePattern &P, const CtorKey &C,
                      PatternRow &OutPatterns, TypeRow &OutTypes) {
  switch (C.K) {
  case CtorKey::Wildcard:
    llvm_unreachable("wildcard witnesses are not pattern constructors");
  case CtorKey::Bool:
  case CtorKey::Integer:
  case CtorKey::IntegerRest:
  case CtorKey::Enum:
  case CtorKey::EnumRest:
    return;
  case CtorKey::Product:
    for (unsigned I = 0; I < C.Arity; ++I) {
      if (P.K == CoveragePattern::Ctor && P.C == C && I < P.Fields.size()) {
        OutPatterns.push_back(*P.Fields[I]);
        OutTypes.push_back(P.FieldTypes[I]);
      } else {
        OutPatterns.push_back(CoveragePattern::wild(P.Loc));
        OutTypes.push_back(I < P.FieldTypes.size()   ? P.FieldTypes[I]
                           : I < C.FieldTypes.size() ? C.FieldTypes[I]
                                                     : QualType());
      }
    }
    return;
  case CtorKey::Alternative:
    if (!C.ProjectableAlternatives[C.AlternativeIndex])
      return;
    if (P.K == CoveragePattern::Ctor && P.C == C && !P.Fields.empty()) {
      OutPatterns.push_back(*P.Fields.front());
      OutTypes.push_back(P.FieldTypes.front());
    } else {
      OutPatterns.push_back(CoveragePattern::wild(P.Loc));
      OutTypes.push_back(C.AlternativeTypes[C.AlternativeIndex]);
    }
    return;
  case CtorKey::AlternativeRest:
    return;
  case CtorKey::OpenAlternative:
    if (P.K == CoveragePattern::Ctor && P.C == C && !P.Fields.empty()) {
      OutPatterns.push_back(*P.Fields.front());
      OutTypes.push_back(P.FieldTypes.front());
    } else {
      OutPatterns.push_back(CoveragePattern::wild(P.Loc));
      OutTypes.push_back(C.OpenAlternativeType);
    }
    return;
  case CtorKey::OpenAlternativeRest:
  case CtorKey::OpenAlternativeEmpty:
    return;
  }
  llvm_unreachable("unhandled constructor kind");
}

bool specializePattern(const CoveragePattern &P, const CtorKey &C,
                       PatternRow &OutPatterns, TypeRow &OutTypes) {
  switch (P.K) {
  case CoveragePattern::Opaque:
    return false;
  case CoveragePattern::Wild:
    appendCtorFields(P, C, OutPatterns, OutTypes);
    return true;
  case CoveragePattern::OpenProjectable:
    if (C.K == CtorKey::OpenAlternative) {
      OutPatterns.push_back(CoveragePattern::wild(P.Loc));
      OutTypes.push_back(C.OpenAlternativeType);
      return true;
    }
    return C.K == CtorKey::OpenAlternativeRest;
  case CoveragePattern::Ctor:
    if (!(P.C == C))
      return false;
    appendCtorFields(P, C, OutPatterns, OutTypes);
    return true;
  }
  llvm_unreachable("unhandled coverage pattern kind");
}

std::optional<llvm::APSInt>
firstMissingValueInRange(const IntegralValueDomain &Domain,
                         ArrayRef<llvm::APSInt> SortedValues) {
  auto FindMissing =
      [&](llvm::APSInt Lower,
          const llvm::APSInt &Upper) -> std::optional<llvm::APSInt> {
    llvm::APSInt Expected = std::move(Lower);
    for (const llvm::APSInt &Value : SortedValues) {
      if (llvm::APSInt::compareValues(Value, Expected) < 0)
        continue;
      if (llvm::APSInt::compareValues(Value, Upper) > 0)
        break;
      if (Value != Expected)
        return Expected;
      if (Expected == Upper)
        return std::nullopt;
      ++Expected;
    }
    return Expected;
  };

  // Prefer a small nonnegative witness before looking through negative values.
  llvm::APSInt Zero(llvm::APInt::getZero(Domain.Min.getBitWidth()),
                    Domain.Min.isUnsigned());
  if (Domain.contains(Zero)) {
    if (std::optional<llvm::APSInt> Missing = FindMissing(Zero, Domain.Max))
      return Missing;
    if (Domain.Min != Zero) {
      llvm::APSInt MinusOne(llvm::APInt::getAllOnes(Domain.Min.getBitWidth()),
                            false);
      return FindMissing(Domain.Min, MinusOne);
    }
    return std::nullopt;
  }
  return FindMissing(Domain.Min, Domain.Max);
}

std::optional<SmallVector<CtorKey, 4>>
constructorsForType(Sema &S, QualType Type, ArrayRef<PatternRow> Matrix,
                    const CoveragePattern &Candidate,
                    ConstructorDomain Domain) {
  Type = Type.getNonReferenceType().getUnqualifiedType();
  SmallVector<CtorKey, 4> Ctors;

  if (Type->isVoidType()) {
    Ctors.push_back(CtorKey::productCtor(0));
    return Ctors;
  }

  if (Type->isBooleanType()) {
    Ctors.push_back(CtorKey::boolCtor(false));
    Ctors.push_back(CtorKey::boolCtor(true));
    return Ctors;
  }

  if (Type->isIntegerType() && !Type->isEnumeralType()) {
    std::optional<IntegralValueDomain> ValueDomain =
        integralValueDomain(S.Context, Type);
    if (!ValueDomain)
      return std::nullopt;

    auto AddPatternValue = [&](const CoveragePattern &P) {
      if (P.K != CoveragePattern::Ctor || P.C.K != CtorKey::Integer)
        return;
      if (!ValueDomain->contains(P.C.IntegralValue))
        return;
      bool Seen = llvm::any_of(Ctors, [&](const CtorKey &C) {
        return C.K == CtorKey::Integer && C.IntegralValue == P.C.IntegralValue;
      });
      if (!Seen)
        Ctors.push_back(CtorKey::integerCtor(P.C.IntegralValue));
    };
    AddPatternValue(Candidate);
    for (const PatternRow &Row : Matrix) {
      if (!Row.empty())
        AddPatternValue(Row.front());
    }

    SmallVector<llvm::APSInt, 4> Values;
    for (const CtorKey &C : Ctors)
      Values.push_back(C.IntegralValue);
    llvm::sort(Values, [](const llvm::APSInt &LHS, const llvm::APSInt &RHS) {
      return llvm::APSInt::compareValues(LHS, RHS) < 0;
    });
    if (std::optional<llvm::APSInt> Missing =
            firstMissingValueInRange(*ValueDomain, Values))
      Ctors.push_back(CtorKey::integerRestCtor(std::move(*Missing)));
    return Ctors;
  }

  if (Type->isEnumeralType()) {
    const EnumDecl *ED = Type->castAsEnumDecl()->getDefinition();
    if (!ED || !ED->isCompleteDefinition())
      return std::nullopt;
    std::optional<IntegralValueDomain> ValueDomain =
        integralValueDomain(S.Context, Type);
    if (!ValueDomain)
      return std::nullopt;

    auto AddEnumValue = [&](llvm::APSInt Value,
                            const EnumConstantDecl *Enumerator = nullptr) {
      Value = convertToIntegerLikeType(S.Context, std::move(Value), Type);
      auto Existing = llvm::find_if(Ctors, [&](const CtorKey &C) {
        return C.K == CtorKey::Enum && C.IntegralValue == Value;
      });
      if (Existing == Ctors.end())
        Ctors.push_back(CtorKey::enumCtor(std::move(Value), Enumerator));
      else if (!Existing->Enumerator)
        Existing->Enumerator = Enumerator;
    };

    for (const EnumConstantDecl *ECD : ED->enumerators()) {
      // Follow switch diagnostics for now: an unavailable enumerator cannot be
      // named, so it is not required coverage. Requiring a wildcard for such
      // values remains an open design question for hard exhaustiveness.
      if (ECD->getAvailability() == AR_Unavailable)
        continue;
      AddEnumValue(ECD->getInitVal(), ECD);
    }

    if (Domain == ConstructorDomain::RequiredAndResidual) {
      auto AddPatternValue = [&](const CoveragePattern &P) {
        if (P.K != CoveragePattern::Ctor || P.C.K != CtorKey::Enum)
          return;
        if (ValueDomain->contains(P.C.IntegralValue))
          AddEnumValue(P.C.IntegralValue, P.C.Enumerator);
      };
      AddPatternValue(Candidate);
      for (const PatternRow &Row : Matrix) {
        if (!Row.empty())
          AddPatternValue(Row.front());
      }

      SmallVector<llvm::APSInt, 4> Values;
      for (const CtorKey &C : Ctors) {
        if (C.K == CtorKey::Enum)
          Values.push_back(C.IntegralValue);
      }
      llvm::sort(Values, [](const llvm::APSInt &LHS, const llvm::APSInt &RHS) {
        return llvm::APSInt::compareValues(LHS, RHS) < 0;
      });

      // Unnamed enum values are useful to match but are not required coverage.
      if (firstMissingValueInRange(*ValueDomain, Values))
        Ctors.push_back(CtorKey::enumRestCtor());
    }
    return Ctors;
  }

  auto AddProduct = [&](const CoveragePattern &P) -> bool {
    if (P.K != CoveragePattern::Ctor || P.C.K != CtorKey::Product)
      return false;
    Ctors.push_back(P.C);
    return true;
  };

  if (AddProduct(Candidate))
    return Ctors;
  for (const PatternRow &Row : Matrix) {
    if (!Row.empty() && AddProduct(Row.front()))
      return Ctors;
  }

  auto AddAlternatives = [&](const CoveragePattern &P) -> bool {
    if (P.K != CoveragePattern::Ctor || P.C.K != CtorKey::Alternative)
      return false;
    for (unsigned I = 0; I < P.C.AlternativeTypes.size(); ++I)
      Ctors.push_back(CtorKey::alternativeCtor(
          P.C.AlternativeOwnerType, I, P.C.AlternativeTypes,
          P.C.ProjectableAlternatives, P.C.IsExhaustive));
    // A protocol can advertise an out-of-range state, such as a valueless
    // variant, without making that state required for exhaustiveness.
    if (Domain == ConstructorDomain::RequiredAndResidual && !P.C.IsExhaustive)
      Ctors.push_back(CtorKey::alternativeRestCtor(P.C.AlternativeOwnerType));
    return true;
  };

  if (AddAlternatives(Candidate))
    return Ctors;
  for (const PatternRow &Row : Matrix) {
    if (!Row.empty() && AddAlternatives(Row.front()))
      return Ctors;
  }

  std::optional<CtorKey> OpenPrototype;
  SmallVector<QualType, 4> OpenTypes;
  auto AddOpenAlternative = [&](const CoveragePattern &P) {
    bool IsOpen = P.K == CoveragePattern::OpenProjectable ||
                  (P.K == CoveragePattern::Ctor &&
                   (P.C.K == CtorKey::OpenAlternative ||
                    P.C.K == CtorKey::OpenAlternativeRest ||
                    P.C.K == CtorKey::OpenAlternativeEmpty));
    if (!IsOpen)
      return;
    if (!OpenPrototype)
      OpenPrototype = P.C;
    if (P.K == CoveragePattern::Ctor && P.C.K == CtorKey::OpenAlternative &&
        llvm::none_of(OpenTypes, [&](QualType Type) {
          return S.Context.hasSameType(Type, P.C.OpenAlternativeType);
        }))
      OpenTypes.push_back(P.C.OpenAlternativeType);
  };

  AddOpenAlternative(Candidate);
  for (const PatternRow &Row : Matrix)
    if (!Row.empty())
      AddOpenAlternative(Row.front());
  if (OpenPrototype) {
    for (QualType Type : OpenTypes)
      Ctors.push_back(CtorKey::openAlternativeCtor(
          OpenPrototype->AlternativeOwnerType, Type,
          OpenPrototype->OpenAlternativeHasEmpty));
    Ctors.push_back(CtorKey::openAlternativeRestCtor(
        OpenPrototype->AlternativeOwnerType,
        OpenPrototype->OpenAlternativeHasEmpty));
    if (OpenPrototype->OpenAlternativeHasEmpty)
      Ctors.push_back(CtorKey::openAlternativeEmptyCtor(
          OpenPrototype->AlternativeOwnerType));
    return Ctors;
  }

  return std::nullopt;
}

Usefulness isUseful(Sema &S, ArrayRef<PatternRow> Matrix, PatternRow Candidate,
                    TypeRow Types, ConstructorDomain Domain,
                    SmallVectorImpl<CtorKey> *Witness) {
  if (Candidate.empty())
    return Matrix.empty() ? Usefulness::Useful : Usefulness::NotUseful;
  if (Types.empty() || Types.front().isNull())
    return Usefulness::MaybeUseful;

  const CoveragePattern &Head = Candidate.front();
  if (Head.K == CoveragePattern::Opaque)
    return Usefulness::MaybeUseful;

  SmallVector<CtorKey, 4> Ctors;
  if (Head.K == CoveragePattern::Ctor) {
    Ctors.push_back(Head.C);
  } else {
    auto KnownCtors =
        constructorsForType(S, Types.front(), Matrix, Head, Domain);
    if (!KnownCtors) {
      SmallVector<PatternRow, 8> DefaultMatrix;
      for (const PatternRow &Row : Matrix) {
        if (Row.empty() || Row.front().K != CoveragePattern::Wild)
          continue;
        PatternRow DefaultRow;
        DefaultRow.append(Row.begin() + 1, Row.end());
        DefaultMatrix.push_back(std::move(DefaultRow));
      }

      PatternRow DefaultCandidate;
      DefaultCandidate.append(Candidate.begin() + 1, Candidate.end());
      TypeRow DefaultTypes;
      DefaultTypes.append(Types.begin() + 1, Types.end());
      SmallVector<CtorKey, 4> SubWitness;
      Usefulness Result = isUseful(
          S, DefaultMatrix, std::move(DefaultCandidate),
          std::move(DefaultTypes), Domain, Witness ? &SubWitness : nullptr);
      if (Result == Usefulness::Useful && Witness) {
        Witness->push_back(CtorKey::wildcardCtor());
        Witness->append(SubWitness.begin(), SubWitness.end());
      }
      return Result;
    }
    Ctors = *KnownCtors;
  }

  bool AnyMaybeUseful = false;
  for (const CtorKey &C : Ctors) {
    SmallVector<PatternRow, 8> SpecializedMatrix;
    for (const PatternRow &Row : Matrix) {
      if (Row.empty())
        continue;
      PatternRow Specialized;
      TypeRow SpecializedTypes;
      if (!specializePattern(Row.front(), C, Specialized, SpecializedTypes))
        continue;
      Specialized.append(Row.begin() + 1, Row.end());
      SpecializedMatrix.push_back(std::move(Specialized));
    }

    PatternRow SpecializedCandidate;
    TypeRow SpecializedTypes;
    if (!specializePattern(Head, C, SpecializedCandidate, SpecializedTypes))
      continue;
    SpecializedCandidate.append(Candidate.begin() + 1, Candidate.end());
    SpecializedTypes.append(Types.begin() + 1, Types.end());

    SmallVector<CtorKey, 4> SubWitness;
    Usefulness Result;
    if (C.K == CtorKey::Alternative &&
        C.ProjectableAlternatives[C.AlternativeIndex] &&
        SpecializedMatrix.empty()) {
      assert(!SpecializedCandidate.empty() && !SpecializedTypes.empty());
      SpecializedCandidate.erase(SpecializedCandidate.begin());
      SpecializedTypes.erase(SpecializedTypes.begin());
      SmallVector<CtorKey, 4> RemainingWitness;
      Result = isUseful(S, SpecializedMatrix, std::move(SpecializedCandidate),
                        std::move(SpecializedTypes), Domain,
                        Witness ? &RemainingWitness : nullptr);
      if (Result == Usefulness::Useful && Witness) {
        SubWitness.push_back(CtorKey::wildcardCtor());
        SubWitness.append(RemainingWitness.begin(), RemainingWitness.end());
      }
    } else {
      Result = isUseful(S, SpecializedMatrix, std::move(SpecializedCandidate),
                        std::move(SpecializedTypes), Domain,
                        Witness ? &SubWitness : nullptr);
    }
    if (Result == Usefulness::Useful) {
      if (Witness) {
        Witness->push_back(C);
        Witness->append(SubWitness.begin(), SubWitness.end());
      }
      return Usefulness::Useful;
    }
    AnyMaybeUseful |= Result == Usefulness::MaybeUseful;
  }

  return AnyMaybeUseful ? Usefulness::MaybeUseful : Usefulness::NotUseful;
}

std::string printCtor(const CtorKey &C) {
  switch (C.K) {
  case CtorKey::Wildcard:
    return "_";
  case CtorKey::Bool:
    return C.BoolValue ? "true" : "false";
  case CtorKey::Integer:
  case CtorKey::IntegerRest: {
    SmallString<32> Str;
    C.IntegralValue.toString(Str, 10);
    return std::string(Str);
  }
  case CtorKey::Enum: {
    if (C.Enumerator)
      return C.Enumerator->getNameAsString();
    SmallString<32> Str;
    C.IntegralValue.toString(Str, 10);
    return std::string(Str);
  }
  case CtorKey::EnumRest:
    return "_";
  case CtorKey::Product:
    return "_";
  case CtorKey::Alternative:
  case CtorKey::OpenAlternative:
    llvm_unreachable("alternative constructors are printed recursively");
  case CtorKey::AlternativeRest:
  case CtorKey::OpenAlternativeRest:
    return "_";
  case CtorKey::OpenAlternativeEmpty:
    return "{}";
  }
  llvm_unreachable("unhandled constructor kind");
}

std::string printTypePattern(ASTContext &Context, QualType Type) {
  Type = Type.getDesugaredType(Context);
  if (Type->hasUnnamedOrLocalType())
    return "_";
  return Type.getAsString(Context.getPrintingPolicy());
}

std::string printWitnessPattern(ASTContext &Context, ArrayRef<CtorKey> Witness,
                                unsigned &Offset) {
  if (Offset == Witness.size())
    return "_";

  const CtorKey &C = Witness[Offset++];
  if (C.K == CtorKey::Alternative) {
    if (C.ProjectableAlternatives[C.AlternativeIndex]) {
      if (Offset < Witness.size() && Witness[Offset].K == CtorKey::Wildcard) {
        ++Offset;
        return "{ " +
               printTypePattern(Context,
                                C.AlternativeTypes[C.AlternativeIndex]) +
               " }";
      }
      return "{ " + printWitnessPattern(Context, Witness, Offset) + " }";
    }
    return "{}";
  }
  if (C.K == CtorKey::OpenAlternative) {
    if (Offset < Witness.size() && Witness[Offset].K == CtorKey::Wildcard) {
      ++Offset;
      return "{ " + printTypePattern(Context, C.OpenAlternativeType) + " }";
    }
    return "{ " + printWitnessPattern(Context, Witness, Offset) + " }";
  }
  if (C.K == CtorKey::OpenAlternativeRest)
    return "{ _ }";
  if (C.K == CtorKey::OpenAlternativeEmpty)
    return "{}";
  if (C.K != CtorKey::Product)
    return printCtor(C);

  std::string Result = "[";
  for (unsigned I = 0; I < C.Arity; ++I) {
    if (I != 0)
      Result += ", ";
    Result += printWitnessPattern(Context, Witness, Offset);
  }
  Result += "]";
  return Result;
}

std::string printWitness(ASTContext &Context, ArrayRef<CtorKey> Witness) {
  unsigned Offset = 0;
  return printWitnessPattern(Context, Witness, Offset);
}

bool hasGuard(const MatchCaseInstantiation &Case) {
  return Case.Guard.hasGuard() || Case.IfLoc.isValid();
}

} // namespace

void Sema::CheckMatchSelectExhaustiveness(
    Expr *Subject, ArrayRef<MatchCase> Cases,
    ArrayRef<MatchCaseInstantiation> Instantiations) {
  if (!Subject || Subject->isTypeDependent() ||
      llvm::any_of(Instantiations, [](const MatchCaseInstantiation &Case) {
        ExprDependence Dependence = Case.Pattern->getDependence();
        return static_cast<bool>(Dependence & (ExprDependence::Instantiation |
                                               ExprDependence::Error));
      }))
    return;

  QualType SubjectType = Subject->getType();
  SmallVector<PatternRow, 8> DefiniteMatrix;
  SmallVector<PatternRow, 8> CoverageMatrix;
  TypeRow InitialTypes;
  InitialTypes.push_back(SubjectType);

  for (unsigned I = 0; I < Instantiations.size();) {
    assert(Instantiations[I].CaseIndex < Cases.size());
    unsigned CaseIndex = Instantiations[I].CaseIndex;
    unsigned GroupEnd = I + 1;
    while (GroupEnd < Instantiations.size() &&
           Instantiations[GroupEnd].CaseIndex == CaseIndex)
      ++GroupEnd;

    bool CheckedPattern = false;
    bool CheckedUsefulness = false;
    bool AnyPattern = false;
    bool AnyUseful = false;
    bool AnyMaybeUseful = false;
    for (unsigned J = I; J < GroupEnd; ++J) {
      const MatchCaseInstantiation &Case = Instantiations[J];
      CoveragePatterns Patterns = makePatterns(
          *this, Case.Pattern, Case.PatternInstantiation, SubjectType);
      CheckedPattern = true;
      AnyPattern |= !Patterns.empty();
      CheckedUsefulness = true;
      for (const CoveragePattern &Pattern : Patterns) {
        PatternRow Row;
        Row.push_back(Pattern);
        Usefulness Result =
            isUseful(*this, DefiniteMatrix, Row, InitialTypes,
                     ConstructorDomain::RequiredAndResidual, nullptr);
        AnyUseful |= Result == Usefulness::Useful;
        AnyMaybeUseful |= Result == Usefulness::MaybeUseful;
      }
      if (hasGuard(Case))
        continue;
      for (CoveragePattern &Pattern : Patterns) {
        if (Pattern.K != CoveragePattern::Opaque) {
          PatternRow Row;
          Row.push_back(Pattern);
          CoverageMatrix.push_back(Row);
          if (!Cases[CaseIndex].MaybeUseful)
            DefiniteMatrix.push_back(std::move(Row));
        }
      }
    }
    bool GroupIsMaybeUseful = Cases[CaseIndex].MaybeUseful;
    if (!GroupIsMaybeUseful && CheckedPattern && !AnyPattern)
      Diag(Cases[CaseIndex].Pattern->getBeginLoc(),
           diag::err_match_case_impossible)
          << SubjectType;
    else if (CheckedUsefulness && !GroupIsMaybeUseful && !AnyUseful &&
             !AnyMaybeUseful)
      Diag(Cases[CaseIndex].Pattern->getBeginLoc(),
           diag::err_match_case_redundant);
    I = GroupEnd;
  }

  PatternRow WildRow;
  WildRow.push_back(CoveragePattern::wild(Subject->getBeginLoc()));
  SmallVector<CtorKey, 4> Witness;
  Usefulness Exhaustive = isUseful(*this, CoverageMatrix, WildRow, InitialTypes,
                                   ConstructorDomain::Required, &Witness);
  if (Exhaustive == Usefulness::Useful) {
    Diag(Subject->getBeginLoc(), diag::err_match_not_exhaustive)
        << true << printWitness(Context, Witness);
  }
}
