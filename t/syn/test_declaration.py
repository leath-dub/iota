# Note function declarations are tested in test_function.py

syntax_test(
    name="variable_declaration_primitive",
    source="let x;",
    expected="""
    source_file {
      imports {}
      decls {
        decl {
          var_decl {
            kind='let'
            binding:
              var_binding {
                name='x'
              }
          }
        }
      }
    }
    """,
)

syntax_test(
    name="variable_declaration_with_type",
    source="let x u32;",
    expected="""
    source_file {
      imports {}
      decls {
        decl {
          var_decl {
            kind='let'
            binding:
              var_binding {
                name='x'
              }
            type {
              builtin_type {
                'u32'
              }
            }
          }
        }
      }
    }
    """,
)

syntax_test(
    name="variable_declaration_with_value",
    source="let x = 10;",
    expected="""
    source_file {
      imports {}
      decls {
        decl {
          var_decl {
            kind='let'
            binding:
              var_binding {
                name='x'
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
    name="variable_declaration_mut",
    source="mut x;",
    expected="""
    source_file {
      imports {}
      decls {
        decl {
          var_decl {
            kind='mut'
            binding:
              var_binding {
                name='x'
              }
          }
        }
      }
    }
    """,
)

syntax_test(
    name="variable_declaration_destructure_struct",
    source="let { *px = x, *py = y, meta };",
    expected="""
    source_file {
      imports {}
      decls {
        decl {
          var_decl {
            kind='let'
            binding:
              var_binding {
                unpack_struct {
                  alias_bindings {
                    alias_binding {
                      binding {
                        kind='*'
                        name='px'
                      }
                      alias='x'
                    }
                    alias_binding {
                      binding {
                        kind='*'
                        name='py'
                      }
                      alias='y'
                    }
                    alias_binding {
                      binding {
                        name='meta'
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
    name="variable_declaration_destructure_tuple",
    source="let ( *px, y );",
    expected="""
    source_file {
      imports {}
      decls {
        decl {
          var_decl {
            kind='let'
            binding:
              var_binding {
                unpack_tuple {
                  bindings {
                    binding {
                      kind='*'
                      name='px'
                    }
                    binding {
                      name='y'
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
    name="variable_declaration_destructure_union",
    source="""
    let ok(foo);
    let err(*bar);
    """,
    expected="""
    source_file {
      imports {}
      decls {
        decl {
          var_decl {
            kind='let'
            binding:
              var_binding {
                unpack_union {
                  tag='ok'
                  binding {
                    name='foo'
                  }
                }
              }
          }
        }
        decl {
          var_decl {
            kind='let'
            binding:
              var_binding {
                unpack_union {
                  tag='err'
                  binding {
                    kind='*'
                    name='bar'
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
    name="struct_declaration",
    source="""
    struct Point {
        x f32,
        y f32,
    }
    """,
    expected="""
    source_file {
      imports {}
      decls {
        decl {
          struct_decl {
            struct_body {
              fields {
                field {
                  name='x'
                  type {
                    builtin_type {
                      'f32'
                    }
                  }
                }
                field {
                  name='y'
                  type {
                    builtin_type {
                      'f32'
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
    name="enum_declaration",
    source="""
    enum Direction {
        north,
        south,
        east,
        west,
    }
    """,
    expected="""
    source_file {
      imports {}
      decls {
        decl {
          enum_decl {
            name='Direction'
            idents {
              'north'
              'south'
              'east'
              'west'
            }
          }
        }
      }
    }
    """,
)

syntax_test(
    name="error_declaration",
    source="""
    error Parse_Error {
        Missing_Semicolon,
        Unmatched_Quote,
        !IO_Error,
    }
    """,
    expected="""
    source_file {
      imports {}
      decls {
        decl {
          err_decl {
            errs {
              err {
                scoped_ident {
                  'Missing_Semicolon'
                }
              }
              err {
                scoped_ident {
                  'Unmatched_Quote'
                }
              }
              err {
                kind='!'
                scoped_ident {
                  'IO_Error'
                }
              }
            }
          }
        }
      }
    }
    """,
)
