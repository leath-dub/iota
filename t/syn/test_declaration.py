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
            binding:
              var_binding {
                ident {
                  'x'
                }
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
            binding:
              var_binding {
                ident {
                  'x'
                }
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
            binding:
              var_binding {
                ident {
                  'x'
                }
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
    name="variable_declaration_destructure_struct",
    source="let { *px = x, *py = y, meta };",
    expected="""
    source_file {
      imports {}
      decls {
        decl {
          var_decl {
            binding:
              var_binding {
                unpack_struct {
                  alias_bindings {
                    alias_binding {
                      binding {
                        kind='*'
                        ident {
                          'px'
                        }
                      }
                      alias='x'
                    }
                    alias_binding {
                      binding {
                        kind='*'
                        ident {
                          'py'
                        }
                      }
                      alias='y'
                    }
                    alias_binding {
                      binding {
                        ident {
                          'meta'
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
    name="variable_declaration_destructure_tuple",
    source="let ( *px, y );",
    expected="""
    source_file {
      imports {}
      decls {
        decl {
          var_decl {
            binding:
              var_binding {
                unpack_tuple {
                  bindings {
                    binding {
                      kind='*'
                      ident {
                        'px'
                      }
                    }
                    binding {
                      ident {
                        'y'
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
            ident {
              'Point'
            }
            struct_body {
              fields {
                field {
                  ident {
                    'x'
                  }
                  type {
                    builtin_type {
                      'f32'
                    }
                  }
                }
                field {
                  ident {
                    'y'
                  }
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
            ident {
              'Direction'
            }
            idents {
              ident {
                'north'
              }
              ident {
                'south'
              }
              ident {
                'east'
              }
              ident {
                'west'
              }
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
            ident {
              'Parse_Error'
            }
            errs {
              err {
                scoped_ident {
                  ident {
                    'Missing_Semicolon'
                  }
                }
              }
              err {
                scoped_ident {
                  ident {
                    'Unmatched_Quote'
                  }
                }
              }
              err {
                kind='!'
                scoped_ident {
                  ident {
                    'IO_Error'
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
