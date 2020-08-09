// quick-lint-js finds bugs in JavaScript programs.
// Copyright (C) 2020  Matthew Glazar
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <optional>
#include <quick-lint-js/error-collector.h>
#include <quick-lint-js/error.h>
#include <quick-lint-js/location.h>
#include <quick-lint-js/parse.h>
#include <quick-lint-js/unreachable.h>
#include <string>
#include <string_view>
#include <vector>

using ::testing::IsEmpty;

namespace quick_lint_js {
namespace {
std::string summarize(const expression &);
std::string summarize(expression_ptr);
std::string summarize(std::optional<expression_ptr>);

class test_parser {
 public:
  explicit test_parser(const char *input) : parser_(input, &errors_) {}

  expression_ptr parse_expression() { return this->parser_.parse_expression(); }

  const std::vector<error_collector::error> &errors() const noexcept {
    return this->errors_.errors;
  }

  source_range error_range(int error_index) {
    return this->parser_.locator().range(this->errors().at(error_index).where);
  }

  source_range range(expression_ptr ast) {
    return this->parser_.locator().range(ast->span());
  }

  quick_lint_js::lexer &lexer() noexcept { return this->parser_.lexer(); }

 private:
  error_collector errors_;
  quick_lint_js::parser parser_;
};

TEST(test_parse_expression, parse_single_token_expression) {
  {
    test_parser p("x");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(ast->kind(), expression_kind::variable);
    EXPECT_EQ(ast->variable_identifier().string_view(), "x");
    EXPECT_THAT(p.errors(), IsEmpty());
    EXPECT_EQ(p.range(ast).begin_offset(), 0);
    EXPECT_EQ(p.range(ast).end_offset(), 1);
  }

  {
    test_parser p("42");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(ast->kind(), expression_kind::literal);
    EXPECT_THAT(p.errors(), IsEmpty());
    EXPECT_EQ(p.range(ast).begin_offset(), 0);
    EXPECT_EQ(p.range(ast).end_offset(), 2);
  }

  {
    test_parser p("'hello'");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(ast->kind(), expression_kind::literal);
    EXPECT_THAT(p.errors(), IsEmpty());
    EXPECT_EQ(p.range(ast).begin_offset(), 0);
    EXPECT_EQ(p.range(ast).end_offset(), 7);
  }

  {
    test_parser p("null");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(ast->kind(), expression_kind::literal);
    EXPECT_THAT(p.errors(), IsEmpty());
    EXPECT_EQ(p.range(ast).begin_offset(), 0);
    EXPECT_EQ(p.range(ast).end_offset(), 4);
  }

  {
    test_parser p("true");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(ast->kind(), expression_kind::literal);
    EXPECT_THAT(p.errors(), IsEmpty());
    EXPECT_EQ(p.range(ast).begin_offset(), 0);
    EXPECT_EQ(p.range(ast).end_offset(), 4);
  }

  {
    test_parser p("false");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(ast->kind(), expression_kind::literal);
    EXPECT_THAT(p.errors(), IsEmpty());
    EXPECT_EQ(p.range(ast).begin_offset(), 0);
    EXPECT_EQ(p.range(ast).end_offset(), 5);
  }

  {
    test_parser p("this");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(ast->kind(), expression_kind::literal);
    EXPECT_THAT(p.errors(), IsEmpty());
    EXPECT_EQ(p.range(ast).begin_offset(), 0);
    EXPECT_EQ(p.range(ast).end_offset(), 4);
  }

  {
    test_parser p("/regexp/");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(ast->kind(), expression_kind::literal);
    EXPECT_THAT(p.errors(), IsEmpty());
    EXPECT_EQ(p.range(ast).begin_offset(), 0);
    EXPECT_EQ(p.range(ast).end_offset(), 8);
  }
}

TEST(test_parse_expression, parse_math_expression) {
  {
    test_parser p("-x");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(ast->kind(), expression_kind::unary_operator);
    EXPECT_EQ(ast->child_0()->kind(), expression_kind::variable);
    EXPECT_THAT(p.errors(), IsEmpty());
    EXPECT_EQ(p.range(ast).begin_offset(), 0);
    EXPECT_EQ(p.range(ast).end_offset(), 2);
  }

  {
    test_parser p("+x");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(summarize(ast), "unary(var x)");
    EXPECT_THAT(p.errors(), IsEmpty());
  }

  {
    test_parser p("x+y");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(summarize(ast), "binary(var x, var y)");
    EXPECT_THAT(p.errors(), IsEmpty());
    EXPECT_EQ(p.range(ast).begin_offset(), 0);
    EXPECT_EQ(p.range(ast).end_offset(), 3);
  }

  {
    test_parser p("x+y-z");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(summarize(ast), "binary(var x, var y, var z)");
    EXPECT_THAT(p.errors(), IsEmpty());
  }

  {
    test_parser p("2-4+1");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(summarize(ast), "binary(literal, literal, literal)");
    EXPECT_THAT(p.errors(), IsEmpty());
  }

  {
    test_parser p("-x+y");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(summarize(ast), "binary(unary(var x), var y)");
    EXPECT_THAT(p.errors(), IsEmpty());
  }

  for (const char *input : {"2+2", "2-2", "2*2", "2/2", "2%2", "2**2", "2^2",
                            "2&2", "2|2", "2<<2", "2>>2", "2>>>2"}) {
    SCOPED_TRACE("input = " + std::string(input));
    test_parser p(input);
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(summarize(ast), "binary(literal, literal)");
    EXPECT_THAT(p.errors(), IsEmpty());
  }
}

TEST(test_parse_expression, parse_broken_math_expression) {
  {
    test_parser p("2+");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(summarize(ast), "binary(literal, ?)");
    ASSERT_EQ(p.errors().size(), 1);
    EXPECT_EQ(p.errors()[0].kind,
              error_collector::error_missing_operand_for_operator);
    EXPECT_EQ(p.error_range(0).begin_offset(), 1);
    EXPECT_EQ(p.error_range(0).end_offset(), 2);
  }

  {
    test_parser p("^2");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(summarize(ast), "binary(?, literal)");
    ASSERT_EQ(p.errors().size(), 1);
    EXPECT_EQ(p.errors()[0].kind,
              error_collector::error_missing_operand_for_operator);
    EXPECT_EQ(p.error_range(0).begin_offset(), 0);
    EXPECT_EQ(p.error_range(0).end_offset(), 1);
  }

  {
    test_parser p("2 * * 2");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(summarize(ast), "binary(literal, ?, literal)");
    ASSERT_EQ(p.errors().size(), 1);
    EXPECT_EQ(p.errors()[0].kind,
              error_collector::error_missing_operand_for_operator);
    EXPECT_EQ(p.error_range(0).begin_offset(), 2);
    EXPECT_EQ(p.error_range(0).end_offset(), 3);
  }

  {
    test_parser p("2 & & & 2");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(summarize(ast), "binary(literal, ?, ?, literal)");
    ASSERT_EQ(p.errors().size(), 2);

    EXPECT_EQ(p.errors()[0].kind,
              error_collector::error_missing_operand_for_operator);
    EXPECT_EQ(p.error_range(0).begin_offset(), 2);
    EXPECT_EQ(p.error_range(0).end_offset(), 3);

    EXPECT_EQ(p.errors()[1].kind,
              error_collector::error_missing_operand_for_operator);
    EXPECT_EQ(p.error_range(1).begin_offset(), 4);
    EXPECT_EQ(p.error_range(1).end_offset(), 5);
  }

  {
    test_parser p("(2*)");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(summarize(ast), "binary(literal, ?)");
    ASSERT_EQ(p.errors().size(), 1);
    EXPECT_EQ(p.errors()[0].kind,
              error_collector::error_missing_operand_for_operator);
    EXPECT_EQ(p.error_range(0).begin_offset(), 2);
    EXPECT_EQ(p.error_range(0).end_offset(), 3);
  }

  {
    test_parser p("2 * (3 + 4");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(summarize(ast), "binary(literal, binary(literal, literal))");
    ASSERT_EQ(p.errors().size(), 1);
    EXPECT_EQ(p.errors()[0].kind, error_collector::error_unmatched_parenthesis);
    EXPECT_EQ(p.error_range(0).begin_offset(), 4);
    EXPECT_EQ(p.error_range(0).end_offset(), 5);
  }

  {
    test_parser p("2 * (3 + (4");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(summarize(ast), "binary(literal, binary(literal, literal))");
    ASSERT_EQ(p.errors().size(), 2);

    EXPECT_EQ(p.errors()[0].kind, error_collector::error_unmatched_parenthesis);
    EXPECT_EQ(p.error_range(0).begin_offset(), 9);
    EXPECT_EQ(p.error_range(0).end_offset(), 10);

    EXPECT_EQ(p.errors()[1].kind, error_collector::error_unmatched_parenthesis);
    EXPECT_EQ(p.error_range(1).begin_offset(), 4);
    EXPECT_EQ(p.error_range(1).end_offset(), 5);
  }
}

TEST(test_parse_expression, parse_logical_expression) {
  for (const char *input : {"2==2", "2===2", "2!=2", "2!==2", "2>2", "2<2",
                            "2>=2", "2<=2", "2&&2", "2||2"}) {
    SCOPED_TRACE("input = " + std::string(input));
    test_parser p(input);
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(summarize(ast), "binary(literal, literal)");
    EXPECT_THAT(p.errors(), IsEmpty());
  }

  {
    test_parser p("!x");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(summarize(ast), "unary(var x)");
    EXPECT_THAT(p.errors(), IsEmpty());
  }
}

TEST(test_parse_expression, parse_keyword_binary_operators) {
  {
    test_parser p("prop in object");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(summarize(ast), "binary(var prop, var object)");
    EXPECT_THAT(p.errors(), IsEmpty());
  }

  {
    test_parser p("object instanceof Class");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(summarize(ast), "binary(var object, var Class)");
    EXPECT_THAT(p.errors(), IsEmpty());
  }
}

TEST(test_parse_expression, parse_typeof_unary_operator) {
  {
    test_parser p("typeof o");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(summarize(ast), "unary(var o)");
    EXPECT_THAT(p.errors(), IsEmpty());
  }

  {
    test_parser p("typeof o === 'number'");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(summarize(ast), "binary(unary(var o), literal)");
    EXPECT_THAT(p.errors(), IsEmpty());
  }
}

TEST(test_parse_expression, delete_unary_operator) {
  {
    test_parser p("delete variable");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(summarize(ast), "unary(var variable)");
    EXPECT_THAT(p.errors(), IsEmpty());
  }

  {
    test_parser p("delete variable.property");
    expression_ptr ast = p.parse_expression();
    if (false) {  // TODO(strager): Check AST.
      EXPECT_EQ(summarize(ast), "unary(dot(var variable, property))");
    }
    EXPECT_THAT(p.errors(), IsEmpty());
  }
}

TEST(test_parse_expression, void_unary_operator) {
  {
    test_parser p("void 0");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(summarize(ast), "unary(literal)");
    EXPECT_THAT(p.errors(), IsEmpty());
  }
}

TEST(test_parse_expression, spread) {
  {
    test_parser p("...args");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(summarize(ast), "spread(var args)");
    EXPECT_EQ(p.range(ast).begin_offset(), 0);
    EXPECT_EQ(p.range(ast).end_offset(), 7);
    EXPECT_THAT(p.errors(), IsEmpty());
  }
}

TEST(test_parse_expression, conditional_expression) {
  {
    test_parser p("x?y:z");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(ast->kind(), expression_kind::conditional);
    EXPECT_EQ(summarize(ast->child_0()), "var x");
    EXPECT_EQ(summarize(ast->child_1()), "var y");
    EXPECT_EQ(summarize(ast->child_2()), "var z");
    EXPECT_EQ(p.range(ast).begin_offset(), 0);
    EXPECT_EQ(p.range(ast).end_offset(), 5);
    EXPECT_THAT(p.errors(), IsEmpty());
  }

  {
    test_parser p("x+x?y+y:z+z");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(ast->kind(), expression_kind::conditional);
    EXPECT_EQ(summarize(ast->child_0()), "binary(var x, var x)");
    EXPECT_EQ(summarize(ast->child_1()), "binary(var y, var y)");
    EXPECT_EQ(summarize(ast->child_2()), "binary(var z, var z)");
    EXPECT_THAT(p.errors(), IsEmpty());
  }

  {
    test_parser p("a ? b : c ? d : e");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(summarize(ast), "cond(var a, var b, cond(var c, var d, var e))");
    EXPECT_THAT(p.errors(), IsEmpty());
  }
}

TEST(test_parse_expression, parse_function_call) {
  {
    test_parser p("f()");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(ast->kind(), expression_kind::call);
    EXPECT_EQ(summarize(ast->child_0()), "var f");
    EXPECT_EQ(ast->child_count(), 1);
    EXPECT_THAT(p.errors(), IsEmpty());
    EXPECT_EQ(p.range(ast).begin_offset(), 0);
    EXPECT_EQ(p.range(ast).end_offset(), 3);
  }

  {
    test_parser p("f(x)");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(ast->kind(), expression_kind::call);
    EXPECT_EQ(summarize(ast->child_0()), "var f");
    EXPECT_EQ(ast->child_count(), 2);
    EXPECT_EQ(summarize(ast->child(1)), "var x");
    EXPECT_THAT(p.errors(), IsEmpty());
  }

  {
    test_parser p("f(x,y)");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(ast->kind(), expression_kind::call);
    EXPECT_EQ(summarize(ast->child_0()), "var f");
    EXPECT_EQ(ast->child_count(), 3);
    EXPECT_EQ(summarize(ast->child(1)), "var x");
    EXPECT_EQ(summarize(ast->child(2)), "var y");
    EXPECT_THAT(p.errors(), IsEmpty());
  }
}

TEST(test_parse_expression, parse_dot_expressions) {
  {
    test_parser p("x.prop");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(ast->kind(), expression_kind::dot);
    EXPECT_EQ(summarize(ast->child_0()), "var x");
    EXPECT_EQ(ast->variable_identifier().string_view(), "prop");
    EXPECT_THAT(p.errors(), IsEmpty());
    EXPECT_EQ(p.range(ast).begin_offset(), 0);
    EXPECT_EQ(p.range(ast).end_offset(), 6);
  }

  {
    test_parser p("x.p1.p2");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(summarize(ast), "dot(dot(var x, p1), p2)");
    EXPECT_THAT(p.errors(), IsEmpty());
  }

  for (std::string keyword : {"catch", "class", "default", "get", "try"}) {
    SCOPED_TRACE(keyword);
    std::string code = "promise." + keyword;
    test_parser p(code.c_str());
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(summarize(ast), "dot(var promise, " + keyword + ")");
    EXPECT_THAT(p.errors(), IsEmpty());
  }
}

TEST(test_parse_expression, parse_indexing_expression) {
  {
    test_parser p("xs[i]");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(ast->kind(), expression_kind::index);
    EXPECT_EQ(summarize(ast->child_0()), "var xs");
    EXPECT_EQ(summarize(ast->child_1()), "var i");
    EXPECT_THAT(p.errors(), IsEmpty());
    EXPECT_EQ(p.range(ast).begin_offset(), 0);
    EXPECT_EQ(p.range(ast).end_offset(), 5);
  }
}

TEST(test_parse_expression, parse_parenthesized_expression) {
  {
    test_parser p("(x)");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(summarize(ast), "var x");
    EXPECT_THAT(p.errors(), IsEmpty());
    EXPECT_EQ(p.range(ast).begin_offset(), 1);
    EXPECT_EQ(p.range(ast).end_offset(), 2);
  }

  {
    test_parser p("x+(y)");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(summarize(ast), "binary(var x, var y)");
    EXPECT_THAT(p.errors(), IsEmpty());
  }

  {
    test_parser p("x+(y+z)");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(summarize(ast), "binary(var x, binary(var y, var z))");
    EXPECT_THAT(p.errors(), IsEmpty());
  }

  {
    test_parser p("(x+y)+z");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(summarize(ast), "binary(binary(var x, var y), var z)");
    EXPECT_THAT(p.errors(), IsEmpty());
  }

  {
    test_parser p("x+(y+z)+w");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(summarize(ast), "binary(var x, binary(var y, var z), var w)");
    EXPECT_THAT(p.errors(), IsEmpty());
  }
}

TEST(test_parse_expression, parse_await_expression) {
  {
    test_parser p("await myPromise");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(summarize(ast), "await(var myPromise)");
    EXPECT_EQ(ast->kind(), expression_kind::await);
    EXPECT_EQ(summarize(ast->child_0()), "var myPromise");
    EXPECT_EQ(p.range(ast).begin_offset(), 0);
    EXPECT_EQ(p.range(ast).end_offset(), 15);
    EXPECT_THAT(p.errors(), IsEmpty());
  }
}

TEST(test_parse_expression, parse_new_expression) {
  {
    test_parser p("new Date");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(ast->kind(), expression_kind::_new);
    EXPECT_EQ(ast->child_count(), 1);
    EXPECT_EQ(summarize(ast->child_0()), "var Date");
    EXPECT_EQ(p.range(ast).begin_offset(), 0);
    EXPECT_EQ(p.range(ast).end_offset(), 8);
    EXPECT_THAT(p.errors(), IsEmpty());
  }

  {
    test_parser p("new Date()");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(ast->kind(), expression_kind::_new);
    EXPECT_EQ(ast->child_count(), 1);
    EXPECT_EQ(summarize(ast->child_0()), "var Date");
    EXPECT_EQ(p.range(ast).begin_offset(), 0);
    EXPECT_EQ(p.range(ast).end_offset(), 10);
    EXPECT_THAT(p.errors(), IsEmpty());
  }

  {
    test_parser p("new Date(y,m,d)");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(summarize(ast), "new(var Date, var y, var m, var d)");
    EXPECT_THAT(p.errors(), IsEmpty());
  }
}

TEST(test_parse_expression, super) {
  {
    test_parser p("super()");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(summarize(ast), "call(super)");
    EXPECT_THAT(p.errors(), IsEmpty());
  }

  {
    test_parser p("super.method()");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(summarize(ast), "call(dot(super, method))");
    EXPECT_THAT(p.errors(), IsEmpty());
  }
}

TEST(test_parse_expression, import) {
  {
    test_parser p("import(url)");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(summarize(ast), "call(import, var url)");
    EXPECT_THAT(p.errors(), IsEmpty());
  }

  {
    test_parser p("import.meta");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(summarize(ast), "dot(import, meta)");
    EXPECT_THAT(p.errors(), IsEmpty());
  }
}

TEST(test_parse_expression, parse_assignment) {
  {
    test_parser p("x=y");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(ast->kind(), expression_kind::assignment);
    EXPECT_EQ(summarize(ast->child_0()), "var x");
    EXPECT_EQ(summarize(ast->child_1()), "var y");
    EXPECT_THAT(p.errors(), IsEmpty());
    EXPECT_EQ(p.range(ast).begin_offset(), 0);
    EXPECT_EQ(p.range(ast).end_offset(), 3);
  }

  {
    test_parser p("x.p=z");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(ast->kind(), expression_kind::assignment);
    EXPECT_EQ(summarize(ast->child_0()), "dot(var x, p)");
    EXPECT_EQ(summarize(ast->child_1()), "var z");
    EXPECT_THAT(p.errors(), IsEmpty());
  }

  {
    test_parser p("f().p=x");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(summarize(ast), "assign(dot(call(var f), p), var x)");
    EXPECT_THAT(p.errors(), IsEmpty());
  }

  {
    test_parser p("x=y=z");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(summarize(ast), "assign(var x, assign(var y, var z))");
    EXPECT_THAT(p.errors(), IsEmpty());
  }

  {
    test_parser p("x,y=z,w");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(summarize(ast), "binary(var x, assign(var y, var z), var w)");
    EXPECT_THAT(p.errors(), IsEmpty());
  }
}

TEST(test_parse_expression, parse_updating_assignment) {
  for (std::string op : {"*=", "/=", "%=", "+=", "-=", "<<=", ">>=", ">>>=",
                         "&=", "^=", "|=", "**="}) {
    SCOPED_TRACE(op);
    std::string code = "x " + op + " y";
    test_parser p(code.c_str());
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(ast->kind(), expression_kind::updating_assignment);
    EXPECT_EQ(summarize(ast->child_0()), "var x");
    EXPECT_EQ(summarize(ast->child_1()), "var y");
    EXPECT_THAT(p.errors(), IsEmpty());
    EXPECT_EQ(p.range(ast).begin_offset(), 0);
    EXPECT_EQ(p.range(ast).end_offset(), code.size());
  }
}

TEST(test_parse_expression, parse_invalid_assignment) {
  {
    test_parser p("x+y=z");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(summarize(ast), "assign(binary(var x, var y), var z)");

    ASSERT_EQ(p.errors().size(), 1);
    auto &error = p.errors()[0];
    EXPECT_EQ(error.kind,
              error_collector::error_invalid_expression_left_of_assignment);
    EXPECT_EQ(p.error_range(0).begin_offset(), 0);
    EXPECT_EQ(p.error_range(0).end_offset(), 3);
  }

  for (const char *code : {
           "f()=x",
           "-x=y",
           "42=y",
           "(x=y)=z",
       }) {
    test_parser p(code);
    p.parse_expression();

    ASSERT_EQ(p.errors().size(), 1);
    auto &error = p.errors()[0];
    EXPECT_EQ(error.kind,
              error_collector::error_invalid_expression_left_of_assignment);
  }
}

TEST(test_parse_expression, parse_prefix_plusplus_minusminus) {
  {
    test_parser p("++x");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(ast->kind(), expression_kind::rw_unary_prefix);
    EXPECT_EQ(summarize(ast->child_0()), "var x");
    EXPECT_EQ(p.range(ast).begin_offset(), 0);
    EXPECT_EQ(p.range(ast).end_offset(), 3);
    EXPECT_THAT(p.errors(), IsEmpty());
  }

  {
    test_parser p("--y");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(ast->kind(), expression_kind::rw_unary_prefix);
    EXPECT_EQ(summarize(ast->child_0()), "var y");
    EXPECT_EQ(p.range(ast).begin_offset(), 0);
    EXPECT_EQ(p.range(ast).end_offset(), 3);
    EXPECT_THAT(p.errors(), IsEmpty());
  }
}

TEST(test_parse_expression, parse_suffix_plusplus_minusminus) {
  {
    test_parser p("x++");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(ast->kind(), expression_kind::rw_unary_suffix);
    EXPECT_EQ(summarize(ast->child_0()), "var x");
    EXPECT_EQ(p.range(ast).begin_offset(), 0);
    EXPECT_EQ(p.range(ast).end_offset(), 3);
    EXPECT_THAT(p.errors(), IsEmpty());
  }
}

TEST(test_parse_expression, suffix_plusplus_minusminus_disallows_line_break) {
  {
    test_parser p("x\n++\ny");

    expression_ptr ast_1 = p.parse_expression();
    EXPECT_EQ(summarize(ast_1), "var x");

    ASSERT_EQ(p.lexer().peek().type, token_type::semicolon)
        << "Semicolon should be inserted (ASI)";
    p.lexer().skip();

    expression_ptr ast_2 = p.parse_expression();
    EXPECT_EQ(summarize(ast_2), "rwunary(var y)");

    EXPECT_THAT(p.errors(), IsEmpty());
  }
}

TEST(test_parse_expression, parse_template) {
  {
    test_parser p("`hello`");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(ast->kind(), expression_kind::literal);
    EXPECT_EQ(p.range(ast).begin_offset(), 0);
    EXPECT_EQ(p.range(ast).end_offset(), 7);
    EXPECT_THAT(p.errors(), IsEmpty());
  }

  {
    test_parser p("`hello${world}`");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(ast->kind(), expression_kind::_template);
    EXPECT_EQ(ast->child_count(), 1);
    EXPECT_EQ(summarize(ast->child(0)), "var world");
    EXPECT_EQ(p.range(ast).begin_offset(), 0);
    EXPECT_EQ(p.range(ast).end_offset(), 15);
    EXPECT_THAT(p.errors(), IsEmpty());
  }

  {
    test_parser p("`${one}${two}${three}`");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(summarize(ast), "template(var one, var two, var three)");
    EXPECT_THAT(p.errors(), IsEmpty());
  }
}

TEST(test_parse_expression, array_literal) {
  {
    test_parser p("[]");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(ast->kind(), expression_kind::array);
    EXPECT_EQ(ast->child_count(), 0);
    EXPECT_EQ(p.range(ast).begin_offset(), 0);
    EXPECT_EQ(p.range(ast).end_offset(), 2);
    EXPECT_THAT(p.errors(), IsEmpty());
  }

  {
    test_parser p("[x]");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(ast->kind(), expression_kind::array);
    EXPECT_EQ(ast->child_count(), 1);
    EXPECT_EQ(summarize(ast->child(0)), "var x");
    EXPECT_THAT(p.errors(), IsEmpty());
  }

  {
    test_parser p("[x, y]");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(ast->kind(), expression_kind::array);
    EXPECT_EQ(ast->child_count(), 2);
    EXPECT_EQ(summarize(ast->child(0)), "var x");
    EXPECT_EQ(summarize(ast->child(1)), "var y");
    EXPECT_THAT(p.errors(), IsEmpty());
  }

  {
    test_parser p("[,,x,,y,,]");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(summarize(ast), "array(var x, var y)");
    EXPECT_THAT(p.errors(), IsEmpty());
  }
}

TEST(test_parse_expression, object_literal) {
  {
    test_parser p("{}");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(ast->kind(), expression_kind::object);
    EXPECT_EQ(ast->object_entry_count(), 0);
    EXPECT_EQ(p.range(ast).begin_offset(), 0);
    EXPECT_EQ(p.range(ast).end_offset(), 2);
    EXPECT_THAT(p.errors(), IsEmpty());
  }

  {
    test_parser p("{key: value}");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(ast->kind(), expression_kind::object);
    EXPECT_EQ(ast->object_entry_count(), 1);
    EXPECT_EQ(summarize(ast->object_entry(0).property), "literal");
    EXPECT_EQ(summarize(ast->object_entry(0).value), "var value");
    EXPECT_THAT(p.errors(), IsEmpty());
  }

  {
    test_parser p("{key1: value1, key2: value2}");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(ast->kind(), expression_kind::object);
    EXPECT_EQ(ast->object_entry_count(), 2);
    EXPECT_EQ(summarize(ast->object_entry(0).property), "literal");
    EXPECT_EQ(summarize(ast->object_entry(0).value), "var value1");
    EXPECT_EQ(summarize(ast->object_entry(1).property), "literal");
    EXPECT_EQ(summarize(ast->object_entry(1).value), "var value2");
    EXPECT_THAT(p.errors(), IsEmpty());
  }

  {
    test_parser p("{'key': value}");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(ast->kind(), expression_kind::object);
    EXPECT_EQ(ast->object_entry_count(), 1);
    EXPECT_EQ(summarize(ast->object_entry(0).property), "literal");
    EXPECT_EQ(summarize(ast->object_entry(0).value), "var value");
    EXPECT_THAT(p.errors(), IsEmpty());
  }

  {
    test_parser p("{[key]: value}");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(ast->kind(), expression_kind::object);
    EXPECT_EQ(ast->object_entry_count(), 1);
    EXPECT_EQ(summarize(ast->object_entry(0).property), "var key");
    EXPECT_EQ(summarize(ast->object_entry(0).value), "var value");
    EXPECT_THAT(p.errors(), IsEmpty());
  }

  {
    test_parser p("{thing}");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(ast->kind(), expression_kind::object);
    EXPECT_EQ(ast->object_entry_count(), 1);
    auto entry = ast->object_entry(0);
    EXPECT_EQ(summarize(entry.property), "literal");
    EXPECT_EQ(p.range(entry.property.value()).begin_offset(), 1);
    EXPECT_EQ(p.range(entry.property.value()).end_offset(), 6);
    EXPECT_EQ(summarize(entry.value), "var thing");
    EXPECT_EQ(p.range(entry.value).begin_offset(), 1);
    EXPECT_EQ(p.range(entry.value).end_offset(), 6);
    EXPECT_THAT(p.errors(), IsEmpty());
  }

  {
    test_parser p("{key1: value1, thing2, key3: value3}");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(summarize(ast),
              "object(literal, var value1, literal, var thing2, literal, var "
              "value3)");
    EXPECT_THAT(p.errors(), IsEmpty());
  }

  {
    test_parser p("{key: variable = value}");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(ast->kind(), expression_kind::object);
    EXPECT_EQ(ast->object_entry_count(), 1);
    EXPECT_EQ(summarize(ast->object_entry(0).property), "literal");
    EXPECT_EQ(summarize(ast->object_entry(0).value),
              "assign(var variable, var value)");
    EXPECT_THAT(p.errors(), IsEmpty());
  }

  {
    test_parser p("{key = value}");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(ast->kind(), expression_kind::object);
    EXPECT_EQ(ast->object_entry_count(), 1);
    EXPECT_EQ(summarize(ast->object_entry(0).property), "literal");
    EXPECT_EQ(summarize(ast->object_entry(0).value),
              "assign(var key, var value)");
    EXPECT_THAT(p.errors(), IsEmpty());
  }

  {
    test_parser p("{...other, k: v}");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(ast->kind(), expression_kind::object);
    EXPECT_EQ(ast->object_entry_count(), 2);
    EXPECT_FALSE(ast->object_entry(0).property.has_value());
    EXPECT_EQ(summarize(ast->object_entry(0).value), "spread(var other)");
    EXPECT_EQ(summarize(ast->object_entry(1).property), "literal");
    EXPECT_EQ(summarize(ast->object_entry(1).value), "var v");
    EXPECT_THAT(p.errors(), IsEmpty());
  }
}

TEST(test_parse_expression, parse_comma_expression) {
  {
    test_parser p("x,y,z");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(ast->kind(), expression_kind::binary_operator);
    EXPECT_EQ(summarize(ast->child(0)), "var x");
    EXPECT_EQ(summarize(ast->child(1)), "var y");
    EXPECT_EQ(summarize(ast->child(2)), "var z");
    EXPECT_EQ(p.range(ast).begin_offset(), 0);
    EXPECT_EQ(p.range(ast).end_offset(), 5);
    EXPECT_THAT(p.errors(), IsEmpty());
  }

  {
    test_parser p("(x+(y,z)+w)");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(summarize(ast), "binary(var x, binary(var y, var z), var w)");
    EXPECT_THAT(p.errors(), IsEmpty());
  }

  {
    test_parser p("`${2+2, four}`");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(summarize(ast), "template(binary(literal, literal, var four))");
    EXPECT_THAT(p.errors(), IsEmpty());
  }
}

TEST(test_parse_expression, parse_function_expression) {
  {
    test_parser p("function(){}");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(ast->kind(), expression_kind::function);
    EXPECT_EQ(ast->attributes(), function_attributes::normal);
    EXPECT_EQ(p.range(ast).begin_offset(), 0);
    EXPECT_EQ(p.range(ast).end_offset(), 12);
    EXPECT_THAT(p.errors(), IsEmpty());
  }

  {
    test_parser p("function(x, y){}");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(ast->kind(), expression_kind::function);
    EXPECT_THAT(p.errors(), IsEmpty());
  }

  {
    test_parser p("function(){}()");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(ast->kind(), expression_kind::call);
    EXPECT_EQ(ast->child_count(), 1);
    EXPECT_EQ(ast->child_0()->kind(), expression_kind::function);
    EXPECT_THAT(p.errors(), IsEmpty());
  }

  {
    test_parser p("function f(){}");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(ast->kind(), expression_kind::named_function);
    EXPECT_EQ(ast->attributes(), function_attributes::normal);
    EXPECT_EQ(ast->variable_identifier().string_view(), "f");
    EXPECT_THAT(p.errors(), IsEmpty());
  }
}

TEST(test_parse_expression, async_function_expression) {
  {
    test_parser p("async function(){}");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(ast->kind(), expression_kind::function);
    EXPECT_EQ(ast->attributes(), function_attributes::async);
    EXPECT_EQ(p.range(ast).begin_offset(), 0);
    EXPECT_EQ(p.range(ast).end_offset(), 18);
    EXPECT_THAT(p.errors(), IsEmpty());
  }

  {
    test_parser p("async function f(){}");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(ast->kind(), expression_kind::named_function);
    EXPECT_EQ(ast->attributes(), function_attributes::async);
    EXPECT_EQ(p.range(ast).begin_offset(), 0);
    EXPECT_EQ(p.range(ast).end_offset(), 20);
    EXPECT_THAT(p.errors(), IsEmpty());
  }
}

TEST(test_parse_expression, arrow_function_with_expression) {
  {
    test_parser p("() => a");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(ast->kind(), expression_kind::arrow_function_with_expression);
    EXPECT_EQ(ast->attributes(), function_attributes::normal);
    EXPECT_EQ(ast->child_count(), 1);
    EXPECT_EQ(summarize(ast->child_0()), "var a");
    EXPECT_EQ(p.range(ast).begin_offset(), 0);
    EXPECT_EQ(p.range(ast).end_offset(), 7);
    EXPECT_THAT(p.errors(), IsEmpty());
  }

  {
    test_parser p("a => b");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(ast->kind(), expression_kind::arrow_function_with_expression);
    EXPECT_EQ(ast->attributes(), function_attributes::normal);
    EXPECT_EQ(ast->child_count(), 2);
    EXPECT_EQ(summarize(ast->child(0)), "var a");
    EXPECT_EQ(summarize(ast->child(1)), "var b");
    // TODO(strager): Implement begin_offset.
    EXPECT_EQ(p.range(ast).end_offset(), 6);
    EXPECT_THAT(p.errors(), IsEmpty());
  }

  {
    test_parser p("(a) => b");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(ast->kind(), expression_kind::arrow_function_with_expression);
    EXPECT_EQ(ast->attributes(), function_attributes::normal);
    EXPECT_EQ(ast->child_count(), 2);
    EXPECT_EQ(summarize(ast->child(0)), "var a");
    EXPECT_EQ(summarize(ast->child(1)), "var b");
    // TODO(strager): Implement begin_offset.
    EXPECT_EQ(p.range(ast).end_offset(), 8);
    EXPECT_THAT(p.errors(), IsEmpty());
  }

  {
    test_parser p("(a, b) => c");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(ast->kind(), expression_kind::arrow_function_with_expression);
    EXPECT_EQ(ast->attributes(), function_attributes::normal);
    EXPECT_EQ(ast->child_count(), 3);
    EXPECT_EQ(summarize(ast->child(0)), "var a");
    EXPECT_EQ(summarize(ast->child(1)), "var b");
    EXPECT_EQ(summarize(ast->child(2)), "var c");
    EXPECT_THAT(p.errors(), IsEmpty());
  }

  {
    test_parser p("() => a, b");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(summarize(ast), "binary(arrowexpr(var a), var b)");
    EXPECT_THAT(p.errors(), IsEmpty());
  }

  {
    test_parser p("a => b, c");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(summarize(ast), "binary(arrowexpr(var a, var b), var c)");
    EXPECT_THAT(p.errors(), IsEmpty());
  }
}

TEST(test_parse_expression, arrow_function_with_statements) {
  {
    test_parser p("() => { a; }");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(ast->kind(), expression_kind::arrow_function_with_statements);
    EXPECT_EQ(ast->attributes(), function_attributes::normal);
    EXPECT_EQ(ast->child_count(), 0);
    EXPECT_EQ(p.range(ast).begin_offset(), 0);
    EXPECT_EQ(p.range(ast).end_offset(), 12);
    EXPECT_THAT(p.errors(), IsEmpty());
  }

  {
    test_parser p("a => { b; }");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(ast->kind(), expression_kind::arrow_function_with_statements);
    EXPECT_EQ(ast->attributes(), function_attributes::normal);
    EXPECT_EQ(ast->child_count(), 1);
    EXPECT_EQ(summarize(ast->child(0)), "var a");
    // TODO(strager): Implement begin_offset.
    EXPECT_EQ(p.range(ast).end_offset(), 11);
    EXPECT_THAT(p.errors(), IsEmpty());
  }
}

TEST(test_parse_expression, arrow_function_with_destructuring_parameters) {
  {
    test_parser p("({a, b}) => c");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(ast->kind(), expression_kind::arrow_function_with_expression);
    EXPECT_EQ(ast->attributes(), function_attributes::normal);
    EXPECT_EQ(ast->child_count(), 2);
    EXPECT_EQ(summarize(ast->child(0)),
              "object(literal, var a, literal, var b)");
    EXPECT_EQ(summarize(ast->child(1)), "var c");
    EXPECT_THAT(p.errors(), IsEmpty());
  }

  {
    test_parser p("([a, b]) => c");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(ast->kind(), expression_kind::arrow_function_with_expression);
    EXPECT_EQ(ast->attributes(), function_attributes::normal);
    EXPECT_EQ(ast->child_count(), 2);
    EXPECT_EQ(summarize(ast->child(0)), "array(var a, var b)");
    EXPECT_EQ(summarize(ast->child(1)), "var c");
    EXPECT_THAT(p.errors(), IsEmpty());
  }
}

TEST(test_parse_expression, async_arrow_function) {
  {
    test_parser p("async () => { a; }");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(ast->kind(), expression_kind::arrow_function_with_statements);
    EXPECT_EQ(ast->attributes(), function_attributes::async);
    EXPECT_EQ(ast->child_count(), 0);
    EXPECT_EQ(p.range(ast).begin_offset(), 0);
    EXPECT_EQ(p.range(ast).end_offset(), 18);
    EXPECT_THAT(p.errors(), IsEmpty());
  }

  {
    test_parser p("async x => { y; }");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(ast->kind(), expression_kind::arrow_function_with_statements);
    EXPECT_EQ(ast->attributes(), function_attributes::async);
    EXPECT_EQ(ast->child_count(), 1);
    EXPECT_EQ(summarize(ast->child(0)), "var x");
    EXPECT_THAT(p.errors(), IsEmpty());
  }

  {
    test_parser p("async (x, y, z) => { w; }");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(summarize(ast), "asyncarrowblock(var x, var y, var z)");
    EXPECT_THAT(p.errors(), IsEmpty());
  }

  {
    test_parser p("async () => a");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(ast->kind(), expression_kind::arrow_function_with_expression);
    EXPECT_EQ(ast->attributes(), function_attributes::async);
    EXPECT_EQ(ast->child_count(), 1);
    EXPECT_EQ(summarize(ast->child(0)), "var a");
    EXPECT_EQ(p.range(ast).begin_offset(), 0);
    EXPECT_EQ(p.range(ast).end_offset(), 13);
    EXPECT_THAT(p.errors(), IsEmpty());
  }

  {
    test_parser p("async x => y");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(ast->kind(), expression_kind::arrow_function_with_expression);
    EXPECT_EQ(ast->attributes(), function_attributes::async);
    EXPECT_EQ(ast->child_count(), 2);
    EXPECT_EQ(summarize(ast->child(0)), "var x");
    EXPECT_EQ(summarize(ast->child(1)), "var y");
    EXPECT_THAT(p.errors(), IsEmpty());
  }

  {
    test_parser p("async (x, y, z) => w");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(summarize(ast), "asyncarrowexpr(var x, var y, var z, var w)");
    EXPECT_THAT(p.errors(), IsEmpty());
  }
}

TEST(test_parse_expression, parse_mixed_expression) {
  {
    test_parser p("a+f()");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(summarize(ast), "binary(var a, call(var f))");
    EXPECT_THAT(p.errors(), IsEmpty());
  }

  {
    test_parser p("a+f(x+y,-z-w)+b");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(summarize(ast),
              "binary(var a, call(var f, binary(var x, var y), "
              "binary(unary(var z), var w)), var b)");
    EXPECT_THAT(p.errors(), IsEmpty());
  }

  {
    test_parser p("(x+y).z");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(summarize(ast), "dot(binary(var x, var y), z)");
    EXPECT_THAT(p.errors(), IsEmpty());
  }

  {
    test_parser p("/hello/.test(string)");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(summarize(ast), "call(dot(literal, test), var string)");
    EXPECT_THAT(p.errors(), IsEmpty());
  }

  {
    test_parser p("!/hello/.test(string)");
    expression_ptr ast = p.parse_expression();
    if (false) {  // TODO(strager): Check AST.
      EXPECT_EQ(summarize(ast), "unary(call(dot(literal, test), var string))");
    }
    EXPECT_THAT(p.errors(), IsEmpty());
  }

  {
    test_parser p("{a: new A(), b: new B()}");
    expression_ptr ast = p.parse_expression();
    EXPECT_EQ(summarize(ast),
              "object(literal, new(var A), literal, new(var B))");
    EXPECT_THAT(p.errors(), IsEmpty());
  }
}

std::string summarize(const expression &expression) {
  auto children = [&] {
    std::string result;
    bool need_comma = false;
    for (int i = 0; i < expression.child_count(); ++i) {
      if (need_comma) {
        result += ", ";
      }
      result += summarize(expression.child(i));
      need_comma = true;
    }
    return result;
  };
  auto function_attributes = [&]() -> std::string {
    switch (expression.attributes()) {
      case function_attributes::normal:
        return "";
      case function_attributes::async:
        return "async";
    }
    QLJS_UNREACHABLE();
  };
  switch (expression.kind()) {
    case expression_kind::_invalid:
      return "?";
    case expression_kind::_new:
      return "new(" + children() + ")";
    case expression_kind::_template:
      return "template(" + children() + ")";
    case expression_kind::array:
      return "array(" + children() + ")";
    case expression_kind::arrow_function_with_expression:
      return function_attributes() + "arrowexpr(" + children() + ")";
    case expression_kind::arrow_function_with_statements:
      return function_attributes() + "arrowblock(" + children() + ")";
    case expression_kind::assignment:
      return "assign(" + children() + ")";
    case expression_kind::await:
      return "await(" + summarize(expression.child_0()) + ")";
    case expression_kind::call:
      return "call(" + children() + ")";
    case expression_kind::conditional:
      return "cond(" + summarize(expression.child_0()) + ", " +
             summarize(expression.child_1()) + ", " +
             summarize(expression.child_2()) + ")";
    case expression_kind::dot:
      return "dot(" + summarize(expression.child_0()) + ", " +
             std::string(expression.variable_identifier().string_view()) + ")";
    case expression_kind::function:
      return "function";
    case expression_kind::import:
      return "import";
    case expression_kind::index:
      return "index(" + children() + ")";
    case expression_kind::literal:
      return "literal";
    case expression_kind::named_function:
      return "function " +
             std::string(expression.variable_identifier().string_view());
    case expression_kind::object: {
      std::string result = "object(";
      bool need_comma = false;
      for (int i = 0; i < expression.object_entry_count(); ++i) {
        if (need_comma) {
          result += ", ";
        }
        auto entry = expression.object_entry(i);
        result += summarize(entry.property);
        result += ", ";
        result += summarize(entry.value);
        need_comma = true;
      }
      result += ")";
      return result;
    }
    case expression_kind::rw_unary_prefix:
      return "rwunary(" + summarize(expression.child_0()) + ")";
    case expression_kind::rw_unary_suffix:
      return "rwunarysuffix(" + summarize(expression.child_0()) + ")";
    case expression_kind::spread:
      return "spread(" + summarize(expression.child_0()) + ")";
    case expression_kind::super:
      return "super";
    case expression_kind::unary_operator:
      return "unary(" + summarize(expression.child_0()) + ")";
    case expression_kind::updating_assignment:
      return "upassign(" + children() + ")";
    case expression_kind::variable:
      return std::string("var ") +
             std::string(expression.variable_identifier().string_view());
    case expression_kind::binary_operator:
      return "binary(" + children() + ")";
  }
  QLJS_UNREACHABLE();
}

std::string summarize(expression_ptr expression) {
  return summarize(*expression);
}

std::string summarize(std::optional<expression_ptr> expression) {
  if (expression.has_value()) {
    return summarize(*expression);
  } else {
    return "(null)";
  }
}
}  // namespace
}  // namespace quick_lint_js
