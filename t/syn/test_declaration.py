# Note function declarations are tested in test_function.py

syntax_test(
    name="variable_declaration_primitive",
    source="let x;",
    expected="""
    (imports)
    (declarations
      (declaration
        (variable_declaration
          mutable: false
          (variable_binding
            x))))
    """,
)

syntax_test(
    name="variable_declaration_with_type",
    source="let x u32;",
    expected="""
    (imports)
    (declarations
      (declaration
        (variable_declaration
          mutable: false
          (variable_binding
            x)
          (type
            (builtin_type
              u32)))))
    """,
)

syntax_test(
    name="variable_declaration_with_value",
    source="let x = 10;",
    expected="""
    (imports)
    (declarations
      (declaration
        (variable_declaration
          mutable: false
          (variable_binding
            x)
          (expression
            (basic_expression
              10)))))
    """,
)

syntax_test(
    name="variable_declaration_mut",
    source="mut x;",
    expected="""
    (imports)
    (declarations
      (declaration
        (variable_declaration
          mutable: true
          (variable_binding
            x))))
    """,
)

syntax_test(
    name="variable_declaration_destructure_struct",
    source="let { *px = x, *py = y, meta };",
    expected="""
    (imports)
    (declarations
      (declaration
        (variable_declaration
          mutable: false
          (variable_binding
            (destructure_struct
              (aliased_binding_list
                (aliased_binding
                  (binding
                    *
                    px)
                  alias: x)
                (aliased_binding
                  (binding
                    *
                    py)
                  alias: y)
                (aliased_binding
                  (binding
                    meta))))))))
    """,
)

syntax_test(
    name="variable_declaration_destructure_tuple",
    source="let ( *px, y );",
    expected="""
    (imports)
    (declarations
      (declaration
        (variable_declaration
          mutable: false
          (variable_binding
            (destructure_tuple
              (binding_list
                (binding
                  *
                  px)
                (binding
                  y)))))))
    """,
)

syntax_test(
    name="variable_declaration_destructure_union",
    source="""
    let ok(foo);
    let err(*bar);
    """,
    expected="""
    (imports)
    (declarations
      (declaration
        (variable_declaration
          mutable: false
          (variable_binding
            (destructure_union
              ok
              (binding
                foo)))))
      (declaration
        (variable_declaration
          mutable: false
          (variable_binding
            (destructure_union
              err
              (binding
                *
                bar))))))
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
    (imports)
    (declarations
      (declaration
        (struct_declaration
          name: Point
          (struct_body
            (field_list
              (field
                name: x
                (type
                  (builtin_type
                    f32)))
              (field
                name: y
                (type
                  (builtin_type
                    f32))))))))
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
    (imports)
    (declarations
      (declaration
        (enum_declaration
          name: Direction
          (identifier_list
            north
            south
            east
            west))))
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
    (imports)
    (declarations
      (declaration
        (error_declaration
          name: Parse_Error
          (error_list
            (error
              embedded: false
              (scoped_identifier
                Missing_Semicolon))
            (error
              embedded: false
              (scoped_identifier
                Unmatched_Quote))
            (error
              embedded: true
              (scoped_identifier
                IO_Error))))))
    """,
)
