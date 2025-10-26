# Note function declarations are tested in test_function.py

syntax_test(
    name="variable_declaration_primitive",
    source="let x;",
    expected="""
    source_file {
      imports {}
      declarations {
        declaration {
          variable_declaration {
            kind='let'
            binding:
              variable_binding {
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
      declarations {
        declaration {
          variable_declaration {
            kind='let'
            binding:
              variable_binding {
                name='x'
              }
            type {
              builtin_type {
                name='u32'
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
      declarations {
        declaration {
          variable_declaration {
            kind='let'
            binding:
              variable_binding {
                name='x'
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
    name="variable_declaration_mut",
    source="mut x;",
    expected="""
    source_file {
      imports {}
      declarations {
        declaration {
          variable_declaration {
            kind='mut'
            binding:
              variable_binding {
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
      declarations {
        declaration {
          variable_declaration {
            kind='let'
            binding:
              variable_binding {
                destructure_struct {
                  aliased_binding_list {
                    aliased_binding {
                      binding {
                        kind='*'
                        name='px'
                      }
                      alias='x'
                    }
                    aliased_binding {
                      binding {
                        kind='*'
                        name='py'
                      }
                      alias='y'
                    }
                    aliased_binding {
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
      declarations {
        declaration {
          variable_declaration {
            kind='let'
            binding:
              variable_binding {
                destructure_tuple {
                  binding_list {
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
      declarations {
        declaration {
          variable_declaration {
            kind='let'
            binding:
              variable_binding {
                destructure_union {
                  tag='ok'
                  binding {
                    name='foo'
                  }
                }
              }
          }
        }
        declaration {
          variable_declaration {
            kind='let'
            binding:
              variable_binding {
                destructure_union {
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
      declarations {
        declaration {
          struct_declaration {
            struct_body {
              field_list {
                field {
                  name='x'
                  type {
                    builtin_type {
                      name='f32'
                    }
                  }
                }
                field {
                  name='y'
                  type {
                    builtin_type {
                      name='f32'
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
      declarations {
        declaration {
          enum_declaration {
            name='Direction'
            identifier_list {
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
      declarations {
        declaration {
          error_declaration {
            error_list {
              error {
                scoped_identifier {
                  'Missing_Semicolon'
                }
              }
              error {
                scoped_identifier {
                  'Unmatched_Quote'
                }
              }
              error {
                kind='!'
                scoped_identifier {
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
