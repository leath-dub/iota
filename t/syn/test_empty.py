syntax_test(
    name = "empty_file",
    source = "",
    expected = """
    source_file {
      imports {}
      decls {}
    }
    """,
)
