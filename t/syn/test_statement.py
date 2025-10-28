syntax_test(
    name="if_statement_basic",
    source="""
    fun _() {
        if foo; {}
    }
    """,
    expected="""
    source_file {
      imports {}
      decls {
        decl {
          fn_decl {
            name='_'
            fn_params {}
            comp_stmt {
              stmt {
                if_stmt {
                  cond {
                    expr {
                      atom {
                        designator {
                          scoped_ident {
                            'foo'
                          }
                        }
                      }
                    }
                  }
                  comp_stmt {}
                }
              }
            }
          }
        }
      }
    }
    """
)
