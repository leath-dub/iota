## Symbol table

The symbol table in iota is a mapping from `node -> scope`, where each scope
also keeps track of any "sub"-scopes.

Each scope is a mapping from `name -> shadow set`, where the "shadow set" is
an ordered list of bindings to an identifier in the current scope.

## Shadowing constraints

Shadowing of a name in a parent scope by entry in a sub scope is generally
legal.

Shadowing of a name in the same scope is only allowed inside of a statement
context. That is to say that you cannot shadow declarations at the top level,
for example:

```iota
let x = 10;
let x = "foo"; // Error cannot shadow declaration of 'x'

fun main() {
    let x = 10;
    let x = "foo"; // No error as in statement context
}
```

## Reference and Declaration order

References that resolve to declarations at the top level need not be after
the declaration.

References that resolve to non top level declarations must be after the declaration.
If a name resolves to a shadow set with more than 1 element, the closest declaration
of the shadow set is chosen.

In the right-hand side of a variable declaration, a reference cannot resolve
to the left-hand side. If a reference resolves to the shadow set which holds
the left-hand side, the declaration associated is skipped. For example:

```iota
// In statment context
let x = 10;
let x = x + 10;
     // ^- here 'x' is resolved to the first declaration

let y = y;
     // ^ here 'y' cannot be resolved, the only entry is skipped as it resolves
     // to the left-hand side of the declaration.
```

## Scoped identifier lookup

Iota supports qualified names which allow you to reference symbols inside of
sub scopes. The syntax for referencing a qualified name is as follows:

```iota
top_level_ns::sub_ns::symbol
```

The resolution process for this name is done like so:

1. Resolve `top_level_ns` lexically starting in the current scope which the reference
   appears in.
2. Resolve `sub_ns` inside of the scope associated with what `top_level_ns` resolves to.
   NOTE: it is an error if `top_level_ns` resolves to something without a scope.
3. Resolve `symbol` in the same way as (2).

### Special "empty" name prefix

There is a special syntax used to describe a context sensitive, inferred reference.
This allows you to access a scope which is inferred by the context in which
the reference is found. Consider the following code:

```iota
enum Foo {
    BAR,
    BAZ,
}

let x Foo = ::BAZ;
```

Typically to reference the name enumerator `BAZ` you would need to explicitly
access it via the scope it is defined in: `Foo::BAZ`. However once the types
are resolved the compiler can apply an inferred namespace to the `::BAZ`
expression given the knowledge that it should resolve to type `Foo`. This
inferred namespace is used to resolve any of these "empty" name prefixed
qualified references.

Another example of this for union alternatives:

```iota
union Foo {
    Bar s32,
    Baz string,
}

fun do_something() -> Foo {
    return ::Bar(10);
}
```

In this case the compiler can set the inferred namespace of the right-hand side
of the return statement to be `Foo`.
