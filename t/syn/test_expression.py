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

# Index expression

syntax_test(
    name="index_expression_basic",
    source="let _ = foo[10];",
    expected="""
    source_file {
      imports {}
      decls {
        decl {
          var_decl {
            kind='let'
            binding:
              var_binding {
                name='_'
              }
            value:
              expr {
                coll_access {
                  expr {
                    atom {
                      ident='foo'
                    }
                  }
                  index {
                    expr {
                      atom {
                        '10'
                      }
                    }
                  }
                }
              }
          }
        }
      }
    }
    """,
)

syntax_test(
    name="index_expression_sub",
    source="let _ = (foo + bar)[10];",
    expected="""
    source_file {
      imports {}
      decls {
        decl {
          var_decl {
            kind='let'
            binding:
              var_binding {
                name='_'
              }
            value:
              expr {
                coll_access {
                  expr {
                    bin_expr {
                      op='+'
                      left:
                        expr {
                          atom {
                            ident='foo'
                          }
                        }
                      right:
                        expr {
                          atom {
                            ident='bar'
                          }
                        }
                    }
                  }
                  index {
                    expr {
                      atom {
                        '10'
                      }
                    }
                  }
                }
              }
          }
        }
      }
    }
    """,
)

syntax_test(
    name="index_expression_sub_inside",
    source="let _ = foo[10 + 30];",
    expected="""
    source_file {
      imports {}
      decls {
        decl {
          var_decl {
            kind='let'
            binding:
              var_binding {
                name='_'
              }
            value:
              expr {
                coll_access {
                  expr {
                    atom {
                      ident='foo'
                    }
                  }
                  index {
                    expr {
                      bin_expr {
                        op='+'
                        left:
                          expr {
                            atom {
                              '10'
                            }
                          }
                        right:
                          expr {
                            atom {
                              '30'
                            }
                          }
                      }
                    }
                  }
                }
              }
          }
        }
      }
    }
    """,
)

syntax_test(
    name="index_expression_sub_inside_from",
    source="let _ = foo[10 + 30:];",
    expected="""
    source_file {
      imports {}
      decls {
        decl {
          var_decl {
            kind='let'
            binding:
              var_binding {
                name='_'
              }
            value:
              expr {
                coll_access {
                  expr {
                    atom {
                      ident='foo'
                    }
                  }
                  index {
                    expr {
                      bin_expr {
                        op='+'
                        left:
                          expr {
                            atom {
                              '10'
                            }
                          }
                        right:
                          expr {
                            atom {
                              '30'
                            }
                          }
                      }
                    }
                    ':'
                  }
                }
              }
          }
        }
      }
    }
    """,
)


syntax_test(
    name="index_expression_from",
    source="let _ = foo[10:];",
    expected="""
    source_file {
      imports {}
      decls {
        decl {
          var_decl {
            kind='let'
            binding:
              var_binding {
                name='_'
              }
            value:
              expr {
                coll_access {
                  expr {
                    atom {
                      ident='foo'
                    }
                  }
                  index {
                    expr {
                      atom {
                        '10'
                      }
                    }
                    ':'
                  }
                }
              }
          }
        }
      }
    }
    """,
)

syntax_test(
    name="index_expression_to",
    source="let _ = foo[:10];",
    expected="""
    source_file {
      imports {}
      decls {
        decl {
          var_decl {
            kind='let'
            binding:
              var_binding {
                name='_'
              }
            value:
              expr {
                coll_access {
                  expr {
                    atom {
                      ident='foo'
                    }
                  }
                  index {
                    ':'
                    expr {
                      atom {
                        '10'
                      }
                    }
                  }
                }
              }
          }
        }
      }
    }
    """,
)

syntax_test(
    name="index_expression_from_to",
    source="let _ = foo[10:20];",
    expected="""
    source_file {
      imports {}
      decls {
        decl {
          var_decl {
            kind='let'
            binding:
              var_binding {
                name='_'
              }
            value:
              expr {
                coll_access {
                  expr {
                    atom {
                      ident='foo'
                    }
                  }
                  index {
                    expr {
                      atom {
                        '10'
                      }
                    }
                    ':'
                    expr {
                      atom {
                        '20'
                      }
                    }
                  }
                }
              }
          }
        }
      }
    }
    """,
)

# Function call expression

syntax_test(
    name="function_call_noargs",
    source="let _ = foo();",
    expected="""
    source_file {
      imports {}
      decls {
        decl {
          var_decl {
            kind='let'
            binding:
              var_binding {
                name='_'
              }
            value:
              expr {
                fn_call {
                  expr {
                    atom {
                      ident='foo'
                    }
                  }
                  args:
                    init {}
                }
              }
          }
        }
      }
    }
    """,
)

syntax_test(
    name="function_call_single_arg",
    source="let _ = foo(x);",
    expected="""
    source_file {
      imports {}
      decls {
        decl {
          var_decl {
            kind='let'
            binding:
              var_binding {
                name='_'
              }
            value:
              expr {
                fn_call {
                  expr {
                    atom {
                      ident='foo'
                    }
                  }
                  args:
                    init {
                      expr {
                        atom {
                          ident='x'
                        }
                      }
                    }
                }
              }
          }
        }
      }
    }
    """,
)

syntax_test(
    name="function_call_single_arg_with_init",
    source="let _ = foo(x = 10);",
    expected="""
    source_file {
      imports {}
      decls {
        decl {
          var_decl {
            kind='let'
            binding:
              var_binding {
                name='_'
              }
            value:
              expr {
                fn_call {
                  expr {
                    atom {
                      ident='foo'
                    }
                  }
                  args:
                    init {
                      expr {
                        bin_expr {
                          op='='
                          left:
                            expr {
                              atom {
                                ident='x'
                              }
                            }
                          right:
                            expr {
                              atom {
                                '10'
                              }
                            }
                        }
                      }
                    }
                }
              }
          }
        }
      }
    }
    """,
)

syntax_test(
    name="function_call_two_args",
    source="let _ = foo(x, y);",
    expected="""
    source_file {
      imports {}
      decls {
        decl {
          var_decl {
            kind='let'
            binding:
              var_binding {
                name='_'
              }
            value:
              expr {
                fn_call {
                  expr {
                    atom {
                      ident='foo'
                    }
                  }
                  args:
                    init {
                      expr {
                        atom {
                          ident='x'
                        }
                      }
                      expr {
                        atom {
                          ident='y'
                        }
                      }
                    }
                }
              }
          }
        }
      }
    }
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
    source_file {
      imports {}
      decls {
        decl {
          var_decl {
            kind='let'
            binding:
              var_binding {
                name='_'
              }
            value:
              expr {
                fn_call {
                  expr {
                    atom {
                      ident='foo'
                    }
                  }
                  args:
                    init {
                      expr {
                        atom {
                          ident='x'
                        }
                      }
                      expr {
                        bin_expr {
                          op='='
                          left:
                            expr {
                              atom {
                                ident='y'
                              }
                            }
                          right:
                            expr {
                              atom {
                                '10'
                              }
                            }
                        }
                      }
                      expr {
                        atom {
                          ident='y'
                        }
                      }
                    }
                }
              }
          }
        }
      }
    }
    """,
)

syntax_test(
    name="function_call_nested",
    source="let _ = foo(foo(foo(bar((*in + 10)()), foo())), foo());",
    expected="""
    source_file {
      imports {}
      decls {
        decl {
          var_decl {
            kind='let'
            binding:
              var_binding {
                name='_'
              }
            value:
              expr {
                fn_call {
                  expr {
                    atom {
                      ident='foo'
                    }
                  }
                  args:
                    init {
                      expr {
                        fn_call {
                          expr {
                            atom {
                              ident='foo'
                            }
                          }
                          args:
                            init {
                              expr {
                                fn_call {
                                  expr {
                                    atom {
                                      ident='foo'
                                    }
                                  }
                                  args:
                                    init {
                                      expr {
                                        fn_call {
                                          expr {
                                            atom {
                                              ident='bar'
                                            }
                                          }
                                          args:
                                            init {
                                              expr {
                                                fn_call {
                                                  expr {
                                                    bin_expr {
                                                      op='+'
                                                      left:
                                                        expr {
                                                          unary_expr {
                                                            op='*'
                                                            expr {
                                                              atom {
                                                                ident='in'
                                                              }
                                                            }
                                                          }
                                                        }
                                                      right:
                                                        expr {
                                                          atom {
                                                            '10'
                                                          }
                                                        }
                                                    }
                                                  }
                                                  args:
                                                    init {}
                                                }
                                              }
                                            }
                                        }
                                      }
                                      expr {
                                        fn_call {
                                          expr {
                                            atom {
                                              ident='foo'
                                            }
                                          }
                                          args:
                                            init {}
                                        }
                                      }
                                    }
                                }
                              }
                            }
                        }
                      }
                      expr {
                        fn_call {
                          expr {
                            atom {
                              ident='foo'
                            }
                          }
                          args:
                            init {}
                        }
                      }
                    }
                }
              }
          }
        }
      }
    }
    """,
)

# Atoms

syntax_test(
    name="atom_number",
    source="let _ = 10;",
    expected="""
    source_file {
      imports {}
      decls {
        decl {
          var_decl {
            kind='let'
            binding:
              var_binding {
                name='_'
              }
            value:
              expr {
                atom {
                  '10'
                }
              }
          }
        }
      }
    }
    """,
)

syntax_test(
    name="atom_string",
    source='let _ = "this is a string";',
    expected="""
    source_file {
      imports {}
      decls {
        decl {
          var_decl {
            kind='let'
            binding:
              var_binding {
                name='_'
              }
            value:
              expr {
                atom {
                  '"this is a string"'
                }
              }
          }
        }
      }
    }
    """,
)

syntax_test(
    name="atom_char",
    source="let _ = 'x';",
    expected="""
    source_file {
      imports {}
      decls {
        decl {
          var_decl {
            kind='let'
            binding:
              var_binding {
                name='_'
              }
            value:
              expr {
                atom {
                  char=''x''
                }
              }
          }
        }
      }
    }
    """,
)

syntax_test(
    name="atom_ident",
    source="let _ = Foo;",
    expected="""
    source_file {
      imports {}
      decls {
        decl {
          var_decl {
            kind='let'
            binding:
              var_binding {
                name='_'
              }
            value:
              expr {
                atom {
                  ident='Foo'
                }
              }
          }
        }
      }
    }
    """,
)

syntax_test(
    name="atom_braced_literal_ident",
    source="let _ = Foo.{ 10 };",
    expected="""
    source_file {
      imports {}
      decls {
        decl {
          var_decl {
            kind='let'
            binding:
              var_binding {
                name='_'
              }
            value:
              expr {
                atom {
                  braced_lit {
                    type {
                      scoped_ident {
                        'Foo'
                      }
                    }
                    init {
                      expr {
                        atom {
                          '10'
                        }
                      }
                    }
                  }
                }
              }
          }
        }
      }
    }
    """,
)

syntax_test(
    name="atom_braced_literal_scoped_ident",
    source="let _ = Foo.Bar.baz.{ 10 };",
    expected="""
    source_file {
      imports {}
      decls {
        decl {
          var_decl {
            kind='let'
            binding:
              var_binding {
                name='_'
              }
            value:
              expr {
                atom {
                  braced_lit {
                    type {
                      scoped_ident {
                        'Foo'
                        'Bar'
                        'baz'
                      }
                    }
                    init {
                      expr {
                        atom {
                          '10'
                        }
                      }
                    }
                  }
                }
              }
          }
        }
      }
    }
    """,
)

syntax_test(
    name="atom_braced_literal_array",
    source="let _ = []u32{ x, y, z };",
    expected="""
    source_file {
      imports {}
      decls {
        decl {
          var_decl {
            kind='let'
            binding:
              var_binding {
                name='_'
              }
            value:
              expr {
                atom {
                  braced_lit {
                    type {
                      coll_type {
                        type {
                          builtin_type {
                            'u32'
                          }
                        }
                      }
                    }
                    init {
                      expr {
                        atom {
                          ident='x'
                        }
                      }
                      expr {
                        atom {
                          ident='y'
                        }
                      }
                      expr {
                        atom {
                          ident='z'
                        }
                      }
                    }
                  }
                }
              }
          }
        }
      }
    }
    """,
)

syntax_test(
    name="atom_braced_literal_pointer_ambiguity0",
    source="let _ = x *Foo;",
    expected="""
    source_file {
      imports {}
      decls {
        decl {
          var_decl {
            kind='let'
            binding:
              var_binding {
                name='_'
              }
            value:
              expr {
                bin_expr {
                  op='*'
                  left:
                    expr {
                      atom {
                        ident='x'
                      }
                    }
                  right:
                    expr {
                      atom {
                        ident='Foo'
                      }
                    }
                }
              }
          }
        }
      }
    }
    """,
)

syntax_test(
    name="atom_braced_literal_pointer_ambiguity1",
    source="let _ = x * (*u32{ 10 });",
    expected="""
    source_file {
      imports {}
      decls {
        decl {
          var_decl {
            kind='let'
            binding:
              var_binding {
                name='_'
              }
            value:
              expr {
                bin_expr {
                  op='*'
                  left:
                    expr {
                      atom {
                        ident='x'
                      }
                    }
                  right:
                    expr {
                      unary_expr {
                        op='*'
                        expr {
                          atom {
                            braced_lit {
                              type {
                                builtin_type {
                                  'u32'
                                }
                              }
                              init {
                                expr {
                                  atom {
                                    '10'
                                  }
                                }
                              }
                            }
                          }
                        }
                      }
                    }
                }
              }
          }
        }
      }
    }
    """,
)

syntax_test(
    name="atom_braced_literal_pointer_ambiguity2",
    source="let _ = x * cons(*u32) { 10 };",
    expected="""
    source_file {
      imports {}
      decls {
        decl {
          var_decl {
            kind='let'
            binding:
              var_binding {
                name='_'
              }
            value:
              expr {
                bin_expr {
                  op='*'
                  left:
                    expr {
                      atom {
                        ident='x'
                      }
                    }
                  right:
                    expr {
                      atom {
                        braced_lit {
                          type {
                            ptr_type {
                              type {
                                builtin_type {
                                  'u32'
                                }
                              }
                            }
                          }
                          init {
                            expr {
                              atom {
                                '10'
                              }
                            }
                          }
                        }
                      }
                    }
                }
              }
          }
        }
      }
    }
    """,
)

syntax_test(
    name="atom_braced_literal_pointer_ambiguity3",
    source="let _ = *{ 10 };",
    expected="""
    source_file {
      imports {}
      decls {
        decl {
          var_decl {
            kind='let'
            binding:
              var_binding {
                name='_'
              }
            value:
              expr {
                unary_expr {
                  op='*'
                  expr {
                    atom {
                      braced_lit {
                        init {
                          expr {
                            atom {
                              '10'
                            }
                          }
                        }
                      }
                    }
                  }
                }
              }
          }
        }
      }
    }
    """,
)

syntax_test(
    name="atom_braced_literal_no_type",
    source="let _ = { x = 10 };",
    expected="""
    source_file {
      imports {}
      decls {
        decl {
          var_decl {
            kind='let'
            binding:
              var_binding {
                name='_'
              }
            value:
              expr {
                atom {
                  braced_lit {
                    init {
                      expr {
                        bin_expr {
                          op='='
                          left:
                            expr {
                              atom {
                                ident='x'
                              }
                            }
                          right:
                            expr {
                              atom {
                                '10'
                              }
                            }
                        }
                      }
                    }
                  }
                }
              }
          }
        }
      }
    }
    """,
)

syntax_test(
    name="address_of",
    source = "let _ = &(10 + 11) + &x - &Foo.{ 10 };",
    expected = """
    source_file {
      imports {}
      decls {
        decl {
          var_decl {
            kind='let'
            binding:
              var_binding {
                name='_'
              }
            value:
              expr {
                bin_expr {
                  op='-'
                  left:
                    expr {
                      bin_expr {
                        op='+'
                        left:
                          expr {
                            unary_expr {
                              op='&'
                              expr {
                                bin_expr {
                                  op='+'
                                  left:
                                    expr {
                                      atom {
                                        '10'
                                      }
                                    }
                                  right:
                                    expr {
                                      atom {
                                        '11'
                                      }
                                    }
                                }
                              }
                            }
                          }
                        right:
                          expr {
                            unary_expr {
                              op='&'
                              expr {
                                atom {
                                  ident='x'
                                }
                              }
                            }
                          }
                      }
                    }
                  right:
                    expr {
                      unary_expr {
                        op='&'
                        expr {
                          atom {
                            braced_lit {
                              type {
                                scoped_ident {
                                  'Foo'
                                }
                              }
                              init {
                                expr {
                                  atom {
                                    '10'
                                  }
                                }
                              }
                            }
                          }
                        }
                      }
                    }
                }
              }
          }
        }
      }
    }
    """,
)

syntax_test(
    name="field_access",
    source = "let _ = foo.bar.baz;",
    expected = """
    source_file {
      imports {}
      decls {
        decl {
          var_decl {
            kind='let'
            binding:
              var_binding {
                name='_'
              }
            value:
              expr {
                field_access {
                  lvalue:
                    expr {
                      field_access {
                        lvalue:
                          expr {
                            atom {
                              ident='foo'
                            }
                          }
                        field='bar'
                      }
                    }
                  field='baz'
                }
              }
          }
        }
      }
    }
    """,
)

syntax_test(
    name="post_increment",
    source = "let _ = foo++;",
    expected = """
    source_file {
      imports {}
      decls {
        decl {
          var_decl {
            kind='let'
            binding:
              var_binding {
                name='_'
              }
            value:
              expr {
                postfix_expr {
                  op='++'
                  expr {
                    atom {
                      ident='foo'
                    }
                  }
                }
              }
          }
        }
      }
    }
    """,
)
