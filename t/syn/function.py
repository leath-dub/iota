syntax_test(
    name = "basic_function",
    source = "fun main() {}",
    expected = """
    (imports)
    (declarations
      (declaration
        (function_declaration
          name: main
          (function_parameter_list)
          (compound_statement))))
    """,
)
