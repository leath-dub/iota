syntax_test(
    name="if_statement_basic",
    source="if foo; {}",
    expected="""
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
    """,
    node="STMT",
)

syntax_test(
    name="return_statement_basic",
    source="return x + 12;",
    expected="""
    stmt {
      return_stmt {
        expr {
          bin_expr {
            op='+'
            left:
              expr {
                atom {
                  designator {
                    scoped_ident {
                      'x'
                    }
                  }
                }
              }
            right:
              expr {
                atom {
                  '12'
                }
              }
          }
        }
      }
    }
    """,
    node="STMT",
)
