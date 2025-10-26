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
      declarations {
        declaration {
          variable_declaration {
            kind='let'
            binding:
              variable_binding {
                name='_'
              }
            value:
              expression {
                array_access_expression {
                  expression {
                    basic_expression {
                      atom='foo'
                    }
                  }
                  index {
                    expression {
                      basic_expression {
                        atom='10'
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
      declarations {
        declaration {
          variable_declaration {
            kind='let'
            binding:
              variable_binding {
                name='_'
              }
            value:
              expression {
                array_access_expression {
                  expression {
                    binary_expression {
                      op='+'
                      left:
                        expression {
                          basic_expression {
                            atom='foo'
                          }
                        }
                      right:
                        expression {
                          basic_expression {
                            atom='bar'
                          }
                        }
                    }
                  }
                  index {
                    expression {
                      basic_expression {
                        atom='10'
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
      declarations {
        declaration {
          variable_declaration {
            kind='let'
            binding:
              variable_binding {
                name='_'
              }
            value:
              expression {
                array_access_expression {
                  expression {
                    basic_expression {
                      atom='foo'
                    }
                  }
                  index {
                    expression {
                      binary_expression {
                        op='+'
                        left:
                          expression {
                            basic_expression {
                              atom='10'
                            }
                          }
                        right:
                          expression {
                            basic_expression {
                              atom='30'
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
      declarations {
        declaration {
          variable_declaration {
            kind='let'
            binding:
              variable_binding {
                name='_'
              }
            value:
              expression {
                array_access_expression {
                  expression {
                    basic_expression {
                      atom='foo'
                    }
                  }
                  index {
                    expression {
                      binary_expression {
                        op='+'
                        left:
                          expression {
                            basic_expression {
                              atom='10'
                            }
                          }
                        right:
                          expression {
                            basic_expression {
                              atom='30'
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
      declarations {
        declaration {
          variable_declaration {
            kind='let'
            binding:
              variable_binding {
                name='_'
              }
            value:
              expression {
                array_access_expression {
                  expression {
                    basic_expression {
                      atom='foo'
                    }
                  }
                  index {
                    expression {
                      basic_expression {
                        atom='10'
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
      declarations {
        declaration {
          variable_declaration {
            kind='let'
            binding:
              variable_binding {
                name='_'
              }
            value:
              expression {
                array_access_expression {
                  expression {
                    basic_expression {
                      atom='foo'
                    }
                  }
                  index {
                    ':'
                    expression {
                      basic_expression {
                        atom='10'
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
      declarations {
        declaration {
          variable_declaration {
            kind='let'
            binding:
              variable_binding {
                name='_'
              }
            value:
              expression {
                array_access_expression {
                  expression {
                    basic_expression {
                      atom='foo'
                    }
                  }
                  index {
                    expression {
                      basic_expression {
                        atom='10'
                      }
                    }
                    ':'
                    expression {
                      basic_expression {
                        atom='20'
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
      declarations {
        declaration {
          variable_declaration {
            kind='let'
            binding:
              variable_binding {
                name='_'
              }
            value:
              expression {
                function_call_expression {
                  expression {
                    basic_expression {
                      atom='foo'
                    }
                  }
                  args:
                    initializer_list {}
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
      declarations {
        declaration {
          variable_declaration {
            kind='let'
            binding:
              variable_binding {
                name='_'
              }
            value:
              expression {
                function_call_expression {
                  expression {
                    basic_expression {
                      atom='foo'
                    }
                  }
                  args:
                    initializer_list {
                      expression {
                        basic_expression {
                          atom='x'
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
      declarations {
        declaration {
          variable_declaration {
            kind='let'
            binding:
              variable_binding {
                name='_'
              }
            value:
              expression {
                function_call_expression {
                  expression {
                    basic_expression {
                      atom='foo'
                    }
                  }
                  args:
                    initializer_list {
                      expression {
                        binary_expression {
                          op='='
                          left:
                            expression {
                              basic_expression {
                                atom='x'
                              }
                            }
                          right:
                            expression {
                              basic_expression {
                                atom='10'
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
      declarations {
        declaration {
          variable_declaration {
            kind='let'
            binding:
              variable_binding {
                name='_'
              }
            value:
              expression {
                function_call_expression {
                  expression {
                    basic_expression {
                      atom='foo'
                    }
                  }
                  args:
                    initializer_list {
                      expression {
                        basic_expression {
                          atom='x'
                        }
                      }
                      expression {
                        basic_expression {
                          atom='y'
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
      declarations {
        declaration {
          variable_declaration {
            kind='let'
            binding:
              variable_binding {
                name='_'
              }
            value:
              expression {
                function_call_expression {
                  expression {
                    basic_expression {
                      atom='foo'
                    }
                  }
                  args:
                    initializer_list {
                      expression {
                        basic_expression {
                          atom='x'
                        }
                      }
                      expression {
                        binary_expression {
                          op='='
                          left:
                            expression {
                              basic_expression {
                                atom='y'
                              }
                            }
                          right:
                            expression {
                              basic_expression {
                                atom='10'
                              }
                            }
                        }
                      }
                      expression {
                        basic_expression {
                          atom='y'
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
      declarations {
        declaration {
          variable_declaration {
            kind='let'
            binding:
              variable_binding {
                name='_'
              }
            value:
              expression {
                function_call_expression {
                  expression {
                    basic_expression {
                      atom='foo'
                    }
                  }
                  args:
                    initializer_list {
                      expression {
                        function_call_expression {
                          expression {
                            basic_expression {
                              atom='foo'
                            }
                          }
                          args:
                            initializer_list {
                              expression {
                                function_call_expression {
                                  expression {
                                    basic_expression {
                                      atom='foo'
                                    }
                                  }
                                  args:
                                    initializer_list {
                                      expression {
                                        function_call_expression {
                                          expression {
                                            basic_expression {
                                              atom='bar'
                                            }
                                          }
                                          args:
                                            initializer_list {
                                              expression {
                                                function_call_expression {
                                                  expression {
                                                    binary_expression {
                                                      op='+'
                                                      left:
                                                        expression {
                                                          unary_expression {
                                                            op='*'
                                                            expression {
                                                              basic_expression {
                                                                atom='in'
                                                              }
                                                            }
                                                          }
                                                        }
                                                      right:
                                                        expression {
                                                          basic_expression {
                                                            atom='10'
                                                          }
                                                        }
                                                    }
                                                  }
                                                  args:
                                                    initializer_list {}
                                                }
                                              }
                                            }
                                        }
                                      }
                                      expression {
                                        function_call_expression {
                                          expression {
                                            basic_expression {
                                              atom='foo'
                                            }
                                          }
                                          args:
                                            initializer_list {}
                                        }
                                      }
                                    }
                                }
                              }
                            }
                        }
                      }
                      expression {
                        function_call_expression {
                          expression {
                            basic_expression {
                              atom='foo'
                            }
                          }
                          args:
                            initializer_list {}
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
      declarations {
        declaration {
          variable_declaration {
            kind='let'
            binding:
              variable_binding {
                name='_'
              }
            value:
              expression {
                basic_expression {
                  atom='10'
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
      declarations {
        declaration {
          variable_declaration {
            kind='let'
            binding:
              variable_binding {
                name='_'
              }
            value:
              expression {
                basic_expression {
                  atom='"this is a string"'
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
      declarations {
        declaration {
          variable_declaration {
            kind='let'
            binding:
              variable_binding {
                name='_'
              }
            value:
              expression {
                basic_expression {
                  atom=''x''
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
      declarations {
        declaration {
          variable_declaration {
            kind='let'
            binding:
              variable_binding {
                name='_'
              }
            value:
              expression {
                basic_expression {
                  atom='Foo'
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
    source="let _ = Foo{ 10 };",
    expected="""
    source_file {
      imports {}
      declarations {
        declaration {
          variable_declaration {
            kind='let'
            binding:
              variable_binding {
                name='_'
              }
            value:
              expression {
                basic_expression {
                  braced_literal {
                    type {
                      scoped_identifier {
                        'Foo'
                      }
                    }
                    initializer_list {
                      expression {
                        basic_expression {
                          atom='10'
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
    source="let _ = Foo.Bar.baz{ 10 };",
    expected="""
    source_file {
      imports {}
      declarations {
        declaration {
          variable_declaration {
            kind='let'
            binding:
              variable_binding {
                name='_'
              }
            value:
              expression {
                basic_expression {
                  braced_literal {
                    type {
                      scoped_identifier {
                        'Foo'
                        'Bar'
                        'baz'
                      }
                    }
                    initializer_list {
                      expression {
                        basic_expression {
                          atom='10'
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
      declarations {
        declaration {
          variable_declaration {
            kind='let'
            binding:
              variable_binding {
                name='_'
              }
            value:
              expression {
                basic_expression {
                  braced_literal {
                    type {
                      collection_type {
                        type {
                          builtin_type {
                            name='u32'
                          }
                        }
                      }
                    }
                    initializer_list {
                      expression {
                        basic_expression {
                          atom='x'
                        }
                      }
                      expression {
                        basic_expression {
                          atom='y'
                        }
                      }
                      expression {
                        basic_expression {
                          atom='z'
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
      declarations {
        declaration {
          variable_declaration {
            kind='let'
            binding:
              variable_binding {
                name='_'
              }
            value:
              expression {
                binary_expression {
                  op='*'
                  left:
                    expression {
                      basic_expression {
                        atom='x'
                      }
                    }
                  right:
                    expression {
                      basic_expression {
                        atom='Foo'
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
      declarations {
        declaration {
          variable_declaration {
            kind='let'
            binding:
              variable_binding {
                name='_'
              }
            value:
              expression {
                binary_expression {
                  op='*'
                  left:
                    expression {
                      basic_expression {
                        atom='x'
                      }
                    }
                  right:
                    expression {
                      unary_expression {
                        op='*'
                        expression {
                          basic_expression {
                            braced_literal {
                              type {
                                builtin_type {
                                  name='u32'
                                }
                              }
                              initializer_list {
                                expression {
                                  basic_expression {
                                    atom='10'
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
      declarations {
        declaration {
          variable_declaration {
            kind='let'
            binding:
              variable_binding {
                name='_'
              }
            value:
              expression {
                binary_expression {
                  op='*'
                  left:
                    expression {
                      basic_expression {
                        atom='x'
                      }
                    }
                  right:
                    expression {
                      basic_expression {
                        braced_literal {
                          type {
                            pointer_type {
                              type {
                                builtin_type {
                                  name='u32'
                                }
                              }
                            }
                          }
                          initializer_list {
                            expression {
                              basic_expression {
                                atom='10'
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
      declarations {
        declaration {
          variable_declaration {
            kind='let'
            binding:
              variable_binding {
                name='_'
              }
            value:
              expression {
                unary_expression {
                  op='*'
                  expression {
                    basic_expression {
                      braced_literal {
                        initializer_list {
                          expression {
                            basic_expression {
                              atom='10'
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
      declarations {
        declaration {
          variable_declaration {
            kind='let'
            binding:
              variable_binding {
                name='_'
              }
            value:
              expression {
                basic_expression {
                  braced_literal {
                    initializer_list {
                      expression {
                        binary_expression {
                          op='='
                          left:
                            expression {
                              basic_expression {
                                atom='x'
                              }
                            }
                          right:
                            expression {
                              basic_expression {
                                atom='10'
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
    source = "let _ = &(10 + 11) + &x - &Foo { 10 };",
    expected = """
    source_file {
      imports {}
      declarations {
        declaration {
          variable_declaration {
            kind='let'
            binding:
              variable_binding {
                name='_'
              }
            value:
              expression {
                binary_expression {
                  op='-'
                  left:
                    expression {
                      binary_expression {
                        op='+'
                        left:
                          expression {
                            unary_expression {
                              op='&'
                              expression {
                                binary_expression {
                                  op='+'
                                  left:
                                    expression {
                                      basic_expression {
                                        atom='10'
                                      }
                                    }
                                  right:
                                    expression {
                                      basic_expression {
                                        atom='11'
                                      }
                                    }
                                }
                              }
                            }
                          }
                        right:
                          expression {
                            unary_expression {
                              op='&'
                              expression {
                                basic_expression {
                                  atom='x'
                                }
                              }
                            }
                          }
                      }
                    }
                  right:
                    expression {
                      unary_expression {
                        op='&'
                        expression {
                          basic_expression {
                            braced_literal {
                              type {
                                scoped_identifier {
                                  'Foo'
                                }
                              }
                              initializer_list {
                                expression {
                                  basic_expression {
                                    atom='10'
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
      declarations {
        declaration {
          variable_declaration {
            kind='let'
            binding:
              variable_binding {
                name='_'
              }
            value:
              expression {
                field_access_expression {
                  lvalue:
                    expression {
                      field_access_expression {
                        lvalue:
                          expression {
                            basic_expression {
                              atom='foo'
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
      declarations {
        declaration {
          variable_declaration {
            kind='let'
            binding:
              variable_binding {
                name='_'
              }
            value:
              expression {
                postfix_expression {
                  op='++'
                  expression {
                    basic_expression {
                      atom='foo'
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
