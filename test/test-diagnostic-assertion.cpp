// Copyright (C) 2020  Matthew "strager" Glazar
// See end of file for extended copyright information.

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <quick-lint-js/diag/diagnostic-types.h>
#include <quick-lint-js/diagnostic-assertion.h>
#include <quick-lint-js/gtest.h>

namespace quick_lint_js {
namespace {
Diagnostic_Assertion parse_or_fail(
    const Char8* specification,
    Source_Location caller = Source_Location::current()) {
  Result<Diagnostic_Assertion, std::vector<std::string>> da =
      Diagnostic_Assertion::parse(specification);
  if (!da.ok()) {
    EXPECT_FALSE(da.error().empty())
        << "if parsing failed, there should be at least one error message";
    for (const std::string& s : da.error()) {
      ADD_FAILURE_AT_CALLER("") << "diagnostic_assertion::parse failed: " << s;
    }
  }
  return *da;
}

TEST(Test_Diagnostic_Assertion, parse_one_character_span) {
  Diagnostic_Assertion da = parse_or_fail(u8"^ Diag_Unexpected_Token");
  EXPECT_EQ(da.type, Diag_Type::Diag_Unexpected_Token);
  EXPECT_EQ(da.span_begin_offset, 0);
  EXPECT_EQ(da.span_end_offset, 1);
}

TEST(Test_Diagnostic_Assertion, parse_one_character_span_at_nonzero) {
  Diagnostic_Assertion da = parse_or_fail(u8"     ^ Diag_Unexpected_Token");
  EXPECT_EQ(da.type, Diag_Type::Diag_Unexpected_Token);
  EXPECT_EQ(da.span_begin_offset, 5);
  EXPECT_EQ(da.span_end_offset, 6);
}

TEST(Test_Diagnostic_Assertion, parse_multiple_character_span) {
  Diagnostic_Assertion da = parse_or_fail(u8"^^^^ Diag_Unexpected_Token");
  EXPECT_EQ(da.type, Diag_Type::Diag_Unexpected_Token);
  EXPECT_EQ(da.span_begin_offset, 0);
  EXPECT_EQ(da.span_end_offset, 4);
}

TEST(Test_Diagnostic_Assertion, parse_unit_character_span) {
  Diagnostic_Assertion da = parse_or_fail(u8"` Diag_Unexpected_Token");
  EXPECT_EQ(da.type, Diag_Type::Diag_Unexpected_Token);
  EXPECT_EQ(da.span_begin_offset, 0);
  EXPECT_EQ(da.span_end_offset, 0);
}

TEST(Test_Diagnostic_Assertion, parse_unit_character_span_at_nonzero) {
  Diagnostic_Assertion da = parse_or_fail(u8"    ` Diag_Unexpected_Token");
  EXPECT_EQ(da.type, Diag_Type::Diag_Unexpected_Token);
  EXPECT_EQ(da.span_begin_offset, 4);
  EXPECT_EQ(da.span_end_offset, 4);
}

TEST(Test_Diagnostic_Assertion, parse_spaces_between_caret_and_diag_type) {
  Diagnostic_Assertion da = parse_or_fail(u8"^     Diag_Unexpected_Token");
  EXPECT_EQ(da.type, Diag_Type::Diag_Unexpected_Token);
  EXPECT_EQ(da.span_begin_offset, 0);
  EXPECT_EQ(da.span_end_offset, 1);
}

TEST(Test_Diagnostic_Assertion, invalid_diag_type_fails) {
  Result<Diagnostic_Assertion, std::vector<std::string>> da =
      Diagnostic_Assertion::parse(u8"^ Diag_Does_Not_Exist");
  ASSERT_FALSE(da.ok());
  EXPECT_THAT(da.error(), ::testing::ElementsAreArray({
                              "invalid diagnostic type: 'Diag_Does_Not_Exist'",
                          }));
}

TEST(Test_Diagnostic_Assertion, trailing_whitespace_fails) {
  Result<Diagnostic_Assertion, std::vector<std::string>> da =
      Diagnostic_Assertion::parse(u8"^ Diag_Unexpected_Token   ");
  ASSERT_FALSE(da.ok());
  EXPECT_THAT(da.error(), ::testing::ElementsAreArray({
                              "trailing whitespace is not allowed in _diag",
                          }));
}

TEST(Test_Diagnostic_Assertion, stray_invalid_character_fails) {
  Result<Diagnostic_Assertion, std::vector<std::string>> da =
      Diagnostic_Assertion::parse(u8"^~ Diag_Unexpected_Token");
  ASSERT_FALSE(da.ok());
  EXPECT_THAT(da.error(), ::testing::ElementsAreArray({
                              "unexpected '~' in _diag",
                          }));
}

TEST(Test_Diagnostic_Assertion, diag_type_with_only_one_member_implicit) {
  Diagnostic_Assertion da = parse_or_fail(u8"^ Diag_Unexpected_Token");
  EXPECT_STREQ(da.member_name, "token");
  EXPECT_EQ(da.member_type, Diagnostic_Arg_Type::source_code_span);
  EXPECT_EQ(da.member_offset, offsetof(Diag_Unexpected_Token, token));
}

TEST(Test_Diagnostic_Assertion, diag_type_with_only_one_member_explicit) {
  Diagnostic_Assertion da = parse_or_fail(u8"^ Diag_Unexpected_Token.token");
  EXPECT_STREQ(da.member_name, "token");
  EXPECT_EQ(da.member_type, Diagnostic_Arg_Type::source_code_span);
  EXPECT_EQ(da.member_offset, offsetof(Diag_Unexpected_Token, token));
}

TEST(Test_Diagnostic_Assertion,
     diag_type_with_multiple_members_implicit_is_not_allowed) {
  {
    Result<Diagnostic_Assertion, std::vector<std::string>> da =
        Diagnostic_Assertion::parse(
            u8"^ Diag_Assignment_Before_Variable_Declaration");
    ASSERT_FALSE(da.ok());
    EXPECT_THAT(
        da.error(),
        ::testing::ElementsAreArray({
            "member required for Diag_Assignment_Before_Variable_Declaration; "
            "try .assignment or .declaration",
        }));
  }

  {
    Result<Diagnostic_Assertion, std::vector<std::string>> da =
        Diagnostic_Assertion::parse(u8"^ Diag_Assignment_To_Const_Variable");
    ASSERT_FALSE(da.ok());
    EXPECT_THAT(da.error(),
                ::testing::ElementsAreArray({
                    "member required for Diag_Assignment_To_Const_Variable; "
                    "try .declaration or .assignment",
                }))
        << ".var_kind should not be listed because it is not a "
           "Source_Code_Span member variable";
  }

  {
    Result<Diagnostic_Assertion, std::vector<std::string>> da =
        Diagnostic_Assertion::parse(
            u8"^ Diag_Expected_Parenthesis_Around_Do_While_Condition");
    ASSERT_FALSE(da.ok());
    EXPECT_THAT(
        da.error(),
        ::testing::ElementsAreArray({
            "member required for "
            "Diag_Expected_Parenthesis_Around_Do_While_Condition; try .where",
        }))
        << "error should occur even if there is only one Source_Code_Span "
           "member variable\n"
        << ".token should not be listed because it is not a Source_Code_Span "
           "member variable";
  }
}

TEST(Test_Diagnostic_Assertion, diag_type_with_multiple_members_explicit) {
  {
    Diagnostic_Assertion da = parse_or_fail(
        u8"^ Diag_Assignment_Before_Variable_Declaration.declaration");
    EXPECT_STREQ(da.member_name, "declaration");
    EXPECT_EQ(da.member_type, Diagnostic_Arg_Type::source_code_span);
    EXPECT_EQ(
        da.member_offset,
        offsetof(Diag_Assignment_Before_Variable_Declaration, declaration));
  }

  {
    Diagnostic_Assertion da = parse_or_fail(
        u8"^ Diag_Assignment_Before_Variable_Declaration.assignment");
    EXPECT_STREQ(da.member_name, "assignment");
    EXPECT_EQ(da.member_type, Diagnostic_Arg_Type::source_code_span);
    EXPECT_EQ(
        da.member_offset,
        offsetof(Diag_Assignment_Before_Variable_Declaration, assignment));
  }
}
}
}

// quick-lint-js finds bugs in JavaScript programs.
// Copyright (C) 2020  Matthew "strager" Glazar
//
// This file is part of quick-lint-js.
//
// quick-lint-js is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// quick-lint-js is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with quick-lint-js.  If not, see <https://www.gnu.org/licenses/>.