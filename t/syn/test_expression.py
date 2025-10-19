# A simple way to test precedence is by comparing the AST of an expression
# that has all implicit precedence (not parenthesis) against an expression with
# explicit precedence encoded by using parenthesis

syntax_test_equal( name="precedence_addmul",
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

# Index expression

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
    """,
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
    """,
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
    """,
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
    """,
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
    """,
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
    """,
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
    """,
)

# Function call expression

syntax_test(
    name="function_call_noargs",
    source="let _ = foo();",
    expected="""
    (imports)
    (declarations
      (declaration
        (variable_declaration
          (variable_binding
            _)
          (expression
            (function_call_expression
              (expression
                (basic_expression
                  foo))
              (initializer_list))))))
    """,
)

syntax_test(
    name="function_call_single_arg",
    source="let _ = foo(x);",
    expected="""
    (imports)
    (declarations
      (declaration
        (variable_declaration
          (variable_binding
            _)
          (expression
            (function_call_expression
              (expression
                (basic_expression
                  foo))
              (initializer_list
                (expression
                  (basic_expression
                    x))))))))
    """,
)

syntax_test(
    name="function_call_single_arg_with_init",
    source="let _ = foo(x = 10);",
    expected="""
    (imports)
    (declarations
      (declaration
        (variable_declaration
          (variable_binding
            _)
          (expression
            (function_call_expression
              (expression
                (basic_expression
                  foo))
              (initializer_list
                (expression
                  (binary_expression
                    (expression
                      (basic_expression
                        x))
                    op: =
                    (expression
                      (basic_expression
                        10))))))))))
    """,
)

syntax_test(
    name="function_call_two_args",
    source="let _ = foo(x, y);",
    expected="""
    (imports)
    (declarations
      (declaration
        (variable_declaration
          (variable_binding
            _)
          (expression
            (function_call_expression
              (expression
                (basic_expression
                  foo))
              (initializer_list
                (expression
                  (basic_expression
                    x))
                (expression
                  (basic_expression
                    y))))))))
    """,
)

syntax_test_equal(
    name="function_call_two_args_trailing_comma",
    lhs="let _ = foo(x, y);",
    rhs="let _ = foo(x, y,);",
)

syntax_test(
    name="function_call_with_init_in_middle",
    source="let _ = foo(x, y = 10, (y));",
    expected="""
    (imports)
    (declarations
      (declaration
        (variable_declaration
          (variable_binding
            _)
          (expression
            (function_call_expression
              (expression
                (basic_expression
                  foo))
              (initializer_list
                (expression
                  (basic_expression
                    x))
                (expression
                  (binary_expression
                    (expression
                      (basic_expression
                        y))
                    op: =
                    (expression
                      (basic_expression
                        10))))
                (expression
                  (basic_expression
                    y))))))))
    """,
)

syntax_test(
    name="function_call_nested",
    source="let _ = foo(foo(foo(bar((*in + 10)()), foo())), foo());",
    expected="""
    (imports)
    (declarations
      (declaration
        (variable_declaration
          (variable_binding
            _)
          (expression
            (function_call_expression
              (expression
                (basic_expression
                  foo))
              (initializer_list
                (expression
                  (function_call_expression
                    (expression
                      (basic_expression
                        foo))
                    (initializer_list
                      (expression
                        (function_call_expression
                          (expression
                            (basic_expression
                              foo))
                          (initializer_list
                            (expression
                              (function_call_expression
                                (expression
                                  (basic_expression
                                    bar))
                                (initializer_list
                                  (expression
                                    (function_call_expression
                                      (expression
                                        (binary_expression
                                          (expression
                                            (unary_expression
                                              op: *
                                              (expression
                                                (basic_expression
                                                  in))))
                                          op: +
                                          (expression
                                            (basic_expression
                                              10))))
                                      (initializer_list))))))
                            (expression
                              (function_call_expression
                                (expression
                                  (basic_expression
                                    foo))
                                (initializer_list)))))))))
                (expression
                  (function_call_expression
                    (expression
                      (basic_expression
                        foo))
                    (initializer_list)))))))))
    """,
)

# Atoms

syntax_test(
    name="atom_number",
    source="let _ = 10;",
    expected="""
    (imports)
    (declarations
      (declaration
        (variable_declaration
          (variable_binding
            _)
          (expression
            (basic_expression
              10)))))
    """,
)

syntax_test(
    name="atom_string",
    source='let _ = "this is a string";',
    expected="""
    (imports)
    (declarations
      (declaration
        (variable_declaration
          (variable_binding
            _)
          (expression
            (basic_expression
              "this is a string")))))
    """,
)

syntax_test(
    name="atom_char",
    source="let _ = 'x';",
    expected="""
    (imports)
    (declarations
      (declaration
        (variable_declaration
          (variable_binding
            _)
          (expression
            (basic_expression
              'x')))))
    """,
)
