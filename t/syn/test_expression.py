# A simple way to test precedence is by comparing the AST of an expression
# that has all implicit precedence (not parenthesis) against an expression with
# explicit precedence encoded by using parenthesis

syntax_test_equal(
    name="precedence_addmul",
    lhs="let _ = 10 + (11 * 12) + 10;",
    rhs="let _ = 10 + 11 * 12 + 10;",
)

syntax_test_equal(
    name="precedence_addsub",
    lhs="let _ = ((10 - 11) + 12) + 10;",
    rhs="let _ = 10 - 11 + 12 + 10;",
)

syntax_test_equal(
    name="precedence_subdiv",
    lhs="let _ = 10 - ((11 / 12) / 10);",
    rhs="let _ = 10 - 11 / 12 / 10;",
)

syntax_test_equal(
    name="precedence_submul",
    lhs="let _ = (((10 * 11) / 12) / 10);",
    rhs="let _ = 10 * 11 / 12 / 10;",
)

syntax_test_equal(
    name="precedence_index",
    lhs="let _ = ((x + ((foo)[10])) - 12);",
    rhs="let _ = x + foo[10] - 12;",
)

syntax_test_equal(
    name="precedence_assign",
    lhs="""
    fun _() {
        (x + (10[*30:])) = (12 + (15 * 3));
    }
    """,
    rhs="""
    fun _() {
        x + 10[*30:] = 12 + 15 * 3;
    }
    """,
)

syntax_test(
    name="index_expression_basic",
    source="let _ = foo[10];",
    expected="""
    (imports)
    (declarations
      (declaration
        (variable_declaration
          (variable_binding
            _)
          (expression
            (array_access_expression
              (expression
                (basic_expression
                  foo))
              (index
                (expression
                  (basic_expression
                    10))))))))
    """
)

syntax_test(
    name="index_expression_sub",
    source="let _ = (foo + bar)[10];",
    expected="""
    (imports)
    (declarations
      (declaration
        (variable_declaration
          (variable_binding
            _)
          (expression
            (array_access_expression
              (expression
                (binary_expression
                  (expression
                    (basic_expression
                      foo))
                  op: +
                  (expression
                    (basic_expression
                      bar))))
              (index
                (expression
                  (basic_expression
                    10))))))))
    """
)

syntax_test(
    name="index_expression_sub_inside",
    source="let _ = foo[10 + 30];",
    expected="""
    (imports)
    (declarations
      (declaration
        (variable_declaration
          (variable_binding
            _)
          (expression
            (array_access_expression
              (expression
                (basic_expression
                  foo))
              (index
                (expression
                  (binary_expression
                    (expression
                      (basic_expression
                        10))
                    op: +
                    (expression
                      (basic_expression
                        30))))))))))
    """
)

syntax_test(
    name="index_expression_sub_inside_from",
    source="let _ = foo[10 + 30:];",
    expected="""
    (imports)
    (declarations
      (declaration
        (variable_declaration
          (variable_binding
            _)
          (expression
            (array_access_expression
              (expression
                (basic_expression
                  foo))
              (index
                start: (expression
                  (binary_expression
                    (expression
                      (basic_expression
                        10))
                    op: +
                    (expression
                      (basic_expression
                        30))))))))))
    """
)


syntax_test(
    name="index_expression_from",
    source="let _ = foo[10:];",
    expected="""
    (imports)
    (declarations
      (declaration
        (variable_declaration
          (variable_binding
            _)
          (expression
            (array_access_expression
              (expression
                (basic_expression
                  foo))
              (index
                start: (expression
                  (basic_expression
                    10))))))))
    """
)

syntax_test(
    name="index_expression_to",
    source="let _ = foo[:10];",
    expected="""
    (imports)
    (declarations
      (declaration
        (variable_declaration
          (variable_binding
            _)
          (expression
            (array_access_expression
              (expression
                (basic_expression
                  foo))
              (index
                end: (expression
                  (basic_expression
                    10))))))))
    """
)

syntax_test(
    name="index_expression_from_to",
    source="let _ = foo[10:20];",
    expected="""
    (imports)
    (declarations
      (declaration
        (variable_declaration
          (variable_binding
            _)
          (expression
            (array_access_expression
              (expression
                (basic_expression
                  foo))
              (index
                start: (expression
                  (basic_expression
                    10))
                end: (expression
                  (basic_expression
                    20))))))))
    """
)
