#ifndef AST_H
#define AST_H

#include "../common/common.h"
#include "../lex/lex.h"

typedef u32 Node_ID;

// There are a bunch of places where we need to do something for every AST node
// type:
//
// * Forward declare all AST nodes - so we don't have to care about order of
// nodes
// * `typedef` all AST nodes - so no need for consumers of `ast` module to type
//   `struct` prefix on each type
// * Add enumerator in Node_Kind enum
//
// For these reasons, The weird and arcane trick known as "X macro" is used
// below (see here for more: info https://en.wikipedia.org/wiki/X_macro).

#define EACH_NODE                                                 \
  USE(Source_File, SOURCE_FILE)                                   \
  USE(Imports, IMPORTS)                                           \
  USE(Declarations, DECLARATIONS)                                 \
  USE(Import, IMPORT)                                             \
  USE(Declaration, DECLARATION)                                   \
  USE(Variable_Declaration, VARIABLE_DECLARATION)                 \
  USE(Function_Declaration, FUNCTION_DECLARATION)                 \
  USE(Type_Parameter_List, TYPE_PARAMETER_LIST)                   \
  USE(Struct_Declaration, STRUCT_DECLARATION)                     \
  USE(Struct_Body, STRUCT_BODY)                                   \
  USE(Enum_Declaration, ENUM_DECLARATION)                         \
  USE(Error_Declaration, ERROR_DECLARATION)                       \
  USE(Union_Declaration, UNION_DECLARATION)                       \
  USE(Variable_Binding, VARIABLE_BINDING)                         \
  USE(Destructure_Tuple, DESTRUCTURE_TUPLE)                       \
  USE(Destructure_Struct, DESTRUCTURE_STRUCT)                     \
  USE(Destructure_Union, DESTRUCTURE_UNION)                       \
  USE(Binding, BINDING)                                           \
  USE(Binding_List, BINDING_LIST)                                 \
  USE(Aliased_Binding, ALIASED_BINDING)                           \
  USE(Aliased_Binding_List, ALIASED_BINDING_LIST)                 \
  USE(Function_Parameter_List, FUNCTION_PARAMETER_LIST)           \
  USE(Function_Parameter, FUNCTION_PARAMETER)                     \
  USE(Type_List, TYPE_LIST)                                       \
  USE(Field_List, FIELD_LIST)                                     \
  USE(Field, FIELD)                                               \
  USE(Identifier_List, IDENTIFIER_LIST)                           \
  USE(Error_List, ERROR_LIST)                                     \
  USE(Error, ERROR)                                               \
  USE(Statement, STATEMENT)                                       \
  USE(If_Statement, IF_STATEMENT)                                 \
  USE(Condition, CONDITION)                                       \
  USE(Union_Tag_Condition, UNION_TAG_CONDITION)                   \
  USE(Return_Statement, RETURN_STATEMENT)                         \
  USE(Defer_Statement, DEFER_STATEMENT)                           \
  USE(Compound_Statement, COMPOUND_STATEMENT)                     \
  USE(Else, ELSE)                                                 \
  USE(Type, TYPE)                                                 \
  USE(Builtin_Type, BUILTIN_TYPE)                                 \
  USE(Collection_Type, COLLECTION_TYPE)                           \
  USE(Struct_Type, STRUCT_TYPE)                                   \
  USE(Union_Type, UNION_TYPE)                                     \
  USE(Enum_Type, ENUM_TYPE)                                       \
  USE(Error_Type, ERROR_TYPE)                                     \
  USE(Pointer_Type, POINTER_TYPE)                                 \
  USE(Function_Type, FUNCTION_TYPE)                               \
  USE(Scoped_Identifier, SCOPED_IDENTIFIER)                       \
  USE(Expression, EXPRESSION)                                     \
  USE(Basic_Expression, BASIC_EXPRESSION)                         \
  USE(Parenthesized_Expression, PARENTHESIZED_EXPRESSION)         \
  USE(Composite_Literal_Expression, COMPOSITE_LITERAL_EXPRESSION) \
  USE(Postfix_Expression, POSTFIX_EXPRESSION)                     \
  USE(Function_Call_Expression, FUNCTION_CALL_EXPRESSION)         \
  USE(Initializer_List, INITIALIZER_LIST)                         \
  USE(Field_Access_Expression, FIELD_ACCESS_EXPRESSION)           \
  USE(Array_Access_Expression, ARRAY_ACCESS_EXPRESSION)           \
  USE(Unary_Expression, UNARY_EXPRESSION)                         \
  USE(Binary_Expression, BINARY_EXPRESSION)                       \
  USE(Braced_Literal, BRACED_LITERAL)                             \
  USE(Index, INDEX)

#define USE(NODE, ...) struct NODE;
EACH_NODE
#undef USE

struct Source_File {
  Node_ID id;
  struct Imports *imports;
  struct Declarations *declarations;
};

struct Imports {
  Node_ID id;
  u32 len;
  u32 cap;
  struct Import **items;
};

struct Import {
  Node_ID id;
  bool aliased;
  struct {
    Tok alias;
  };
  Tok import_name;
};

struct Declarations {
  Node_ID id;
  u32 len;
  u32 cap;
  struct Declaration **items;
};

typedef enum {
  DECLARATION_VARIABLE,
  DECLARATION_FUNCTION,
  DECLARATION_STRUCT,
  DECLARATION_ENUM,
  DECLARATION_ERROR,
  DECLARATION_UNION,
} Declaration_Kind;

struct Declaration {
  Node_ID id;
  Declaration_Kind t;
  union {
    struct Variable_Declaration *variable_declaration;
    struct Function_Declaration *function_declaration;
    struct Struct_Declaration *struct_declaration;
    struct Enum_Declaration *enum_declaration;
    struct Error_Declaration *error_declaration;
    struct Union_Declaration *union_declaration;
  };
};

struct Variable_Declaration {
  Node_ID id;
  Tok classifier;  // let or mut
  struct Variable_Binding *binding;
  struct Type *type;  // nullable
  struct {
    Tok assign_token;
    struct Expression *expression;
    bool ok;
  } init;
};

typedef enum {
  VARIABLE_BINDING_BASIC,
  VARIABLE_BINDING_DESTRUCTURE_TUPLE,
  VARIABLE_BINDING_DESTRUCTURE_STRUCT,
  VARIABLE_BINDING_DESTRUCTURE_UNION,
} Variable_Binding_Kind;

struct Variable_Binding {
  Node_ID id;
  Variable_Binding_Kind t;
  union {
    Tok basic;
    struct Destructure_Tuple *destructure_tuple;
    struct Destructure_Struct *destructure_struct;
    struct Destructure_Union *destructure_union;
  };
};

struct Aliased_Binding {
  Node_ID id;
  struct Binding *binding;
  MAYBE(Tok) alias;
};

struct Aliased_Binding_List {
  Node_ID id;
  u32 len;
  u32 cap;
  struct Aliased_Binding **items;
};

struct Binding {
  Node_ID id;
  Tok identifier;
  Tok reference;  // could be a '*' meaning the binding is a reference
};

struct Binding_List {
  Node_ID id;
  u32 len;
  u32 cap;
  struct Binding **items;
};

struct Destructure_Tuple {
  Node_ID id;
  struct Binding_List *bindings;
};

struct Destructure_Struct {
  Node_ID id;
  struct Aliased_Binding_List *bindings;
};

struct Destructure_Union {
  Node_ID id;
  Tok tag;
  struct Binding *binding;
};

struct Function_Declaration {
  Node_ID id;
  Tok identifier;
  MAYBE(struct Type_Parameter_List *) type_params;
  struct Function_Parameter_List *parameters;
  MAYBE(struct Type *) return_type;
  struct Compound_Statement *body;
};

struct Function_Parameter_List {
  Node_ID id;
  u32 len;
  u32 cap;
  struct Function_Parameter **items;
};

struct Function_Parameter {
  Node_ID id;
  struct Variable_Binding *binding;
  struct Type *type;
  bool variadic;
};

struct Type_Parameter_List {
  Node_ID id;
  struct Type_List *types;
};

struct Struct_Declaration {
  Node_ID id;
  Tok identifier;
  MAYBE(struct Type_Parameter_List *) type_params;
  struct Struct_Body *body;
};

struct Struct_Body {
  Node_ID id;
  bool tuple_like;
  union {
    struct Type_List *type_list;
    struct Field_List *field_list;
  };
};

struct Field_List {
  Node_ID id;
  u32 len;
  u32 cap;
  struct Field **items;
};

struct Field {
  Node_ID id;
  Tok identifier;
  struct Type *type;
};

struct Enum_Declaration {
  Node_ID id;
  Tok identifier;
  struct Identifier_List *enumerators;
};

struct Identifier_List {
  Node_ID id;
  u32 len;
  u32 cap;
  Tok *items;
};

struct Error_Declaration {
  Node_ID id;
  Tok identifier;
  struct Error_List *errors;
};

struct Error_List {
  Node_ID id;
  u32 len;
  u32 cap;
  struct Error **items;
};

struct Error {
  Node_ID id;
  bool embedded;  // e.g. !Foo
  struct Scoped_Identifier *scoped_identifier;
};

struct Union_Declaration {
  Node_ID id;
  Tok identifier;
  MAYBE(struct Type_Parameter_List *) type_params;
  struct Field_List *fields;
};

typedef enum {
  STATEMENT_DECLARATION,
  STATEMENT_IF,
  STATEMENT_RETURN,
  STATEMENT_COMPOUND,
  STATEMENT_EXPRESSION,
} Statement_Kind;

struct Statement {
  Node_ID id;
  Statement_Kind t;
  union {
    struct Declaration *declaration;
    struct If_Statement *if_statement;
    struct Return_Statement *return_statement;
    struct Defer_Statement *defer_statement;
    struct Compound_Statement *compound_statement;
    struct Expression *expression;
  };
};

struct If_Statement {
  Node_ID id;
  struct Condition *condition;
  struct Compound_Statement *true_branch;
  MAYBE(struct Else *) else_branch;
};

typedef enum {
  CONDITION_UNION_TAG,
  CONDITION_EXPRESSION,
} Condition_Kind;

struct Condition {
  Node_ID id;
  Condition_Kind t;
  union {
    struct Union_Tag_Condition *union_tag;
    struct Expression *expression;
  };
};

struct Union_Tag_Condition {
  Node_ID id;
  Tok classifier;  // let or mut
  struct Destructure_Union *trigger;
  Tok assign_token;
  struct Expression *expression;
};

typedef enum {
  ELSE_IF,
  ELSE_COMPOUND,
} Else_Kind;

struct Else {
  Node_ID id;
  Else_Kind t;
  union {
    struct If_Statement *if_statement;
    struct Compound_Statement *compound_statement;
  };
};

struct Return_Statement {
  Node_ID id;
  struct Expr *expression;  // nullable
};

struct Defer_Statement {
  Node_ID id;
  struct Statement *statement;
};

struct Compound_Statement {
  Node_ID id;
  u32 len;
  u32 cap;
  struct Statement **items;
};

typedef enum {
  TYPE_BUILTIN,
  TYPE_COLLECTION,
  TYPE_STRUCT,
  TYPE_UNION,
  TYPE_ENUM,
  TYPE_ERROR,
  TYPE_POINTER,
  TYPE_FUNCTION,
  TYPE_SCOPED_IDENTIFIER,
} Type_Kind;

struct Type {
  Node_ID id;
  Type_Kind t;
  union {
    struct Builtin_Type *builtin_type;
    struct Collection_Type *collection_type;
    struct Struct_Type *struct_type;
    struct Union_Type *union_type;
    struct Enum_Type *enum_type;
    struct Error_Type *error_type;
    struct Pointer_Type *pointer_type;
    struct Function_Type *function_type;
    struct Scoped_Identifier *scoped_identifier;
  };
};

struct Builtin_Type {
  Node_ID id;
  Tok token;  // just stores the keyword token
};

struct Collection_Type {
  Node_ID id;
  MAYBE(struct Expression *) index_expression;
  struct Type *element_type;
};

struct Struct_Type {
  Node_ID id;
  MAYBE(struct Type_Parameter_List *) type_params;
  struct Struct_Body *body;
};

struct Type_List {
  Node_ID id;
  u32 len;
  u32 cap;
  struct Type **items;
};

struct Union_Type {
  Node_ID id;
  MAYBE(struct Type_Parameter_List *) type_params;
  struct Field_List *fields;
};

struct Enum_Type {
  Node_ID id;
  struct Identifier_List *enumerators;
};

struct Error_Type {
  Node_ID id;
  struct Error_List *errors;
};

struct Pointer_Type {
  Node_ID id;
  Tok classifier;  // let or mut
  struct Type *referenced_type;
};

struct Function_Type {
  Node_ID id;
  struct Type_List *parameters;
  MAYBE(struct Type *) return_type;
};

struct Scoped_Identifier {
  Node_ID id;
  u32 cap;
  u32 len;
  Tok *items;
};

typedef enum {
  EXPRESSION_BASIC,
  EXPRESSION_PARENTHESIZED,
  EXPRESSION_COMPOSITE_LITERAL,
  EXPRESSION_POSTFIX,
  EXPRESSION_FUNCTION_CALL,
  EXPRESSION_FIELD_ACCESS,
  EXPRESSION_ARRAY_ACCESS,
  EXPRESSION_UNARY,
  EXPRESSION_BINARY,
} Expression_Kind;

struct Expression {
  Node_ID id;
  Expression_Kind t;
  union {
    struct Basic_Expression *basic_expression;
    struct Parenthesized_Expression *parenthesized_expression;
    struct Composite_Literal_Expression *composite_literal_expression;
    struct Postfix_Expression *postfix_expression;
    struct Function_Call_Expression *function_call_expression;
    struct Field_Access_Expression *field_access_expression;
    struct Array_Access_Expression *array_access_expression;
    struct Unary_Expression *unary_expression;
    struct Binary_Expression *binary_expression;
  };
};

typedef enum {
  BASIC_EXPRESSION_TOKEN,
  BASIC_EXPRESSION_BRACED_LIT,
} Basic_Expression_Kind;

struct Basic_Expression {
  Node_ID id;
  Basic_Expression_Kind t;
  union {
    Tok token;  // Stores: identifier, number, string, enum, nil
    struct Braced_Literal *braced_lit;
  };
};

struct Braced_Literal {
  Node_ID id;
  MAYBE(struct Type *) type;
  struct Initializer_List *initializer;
};

struct Parenthesized_Expression {
  Node_ID id;
  struct Expression *inner_expression;
};

struct Composite_Literal_Expression {
  Node_ID id;
  MAYBE(struct Type *) explicit_type;
  struct Expression *value;
};

struct Function_Call_Expression {
  Node_ID id;
  struct Expression *lvalue;
  struct Initializer_List *arguments;
};

struct Initializer_List {
  Node_ID id;
  u32 len;
  u32 cap;
  struct Expression **items;
};

struct Postfix_Expression {
  Node_ID id;
  struct Expression *inner_expression;
  Tok op;
};

struct Field_Access_Expression {
  Node_ID id;
  struct Expression *lvalue;
  Tok field;
};

struct Array_Access_Expression {
  Node_ID id;
  struct Expression *lvalue;
  struct Index *index;
};

typedef enum {
  INDEX_SINGLE,
  INDEX_RANGE,
} Index_Kind;

struct Index {
  Node_ID id;
  Index_Kind t;
  MAYBE(struct Expression *) start;
  MAYBE(struct Expression *) end;
};

struct Unary_Expression {
  Node_ID id;
  Tok op;
  struct Expression *inner_expression;
};

struct Binary_Expression {
  Node_ID id;
  Tok op;
  struct Expression *left;
  struct Expression *right;
};

#define USE(NODE, ...) typedef struct NODE NODE;
EACH_NODE
#undef USE

typedef enum {
#define USE(_, UPPER_NAME) NODE_##UPPER_NAME,
  EACH_NODE
#undef USE
      NODE_KIND_COUNT,
} Node_Kind;

typedef enum {
  NFLAG_NONE = 0,
  NFLAG_ERROR = 1,
} Node_Flag;

typedef struct {
  Node_Flag *items;
  u32 cap;
  u32 len;
} Node_Flags;

typedef struct {
  const char **items;
  u32 cap;
  u32 len;
} Node_Names;

typedef enum {
  CHILD_TOKEN,
  CHILD_NODE,
} Child_Kind;

typedef struct {
  Child_Kind t;
  union {
    Tok token;
    Node_ID id;
  };
  MAYBE(const char *) name;
} Node_Child;

typedef struct {
  Node_Child *items;
  u32 cap;
  u32 len;
} Node_Children;

typedef struct {
  Node_Children *items;
  u32 cap;
  u32 len;
} Node_Trees;

typedef struct {
  Node_ID next_id;
  Node_Flags flags;
  Node_Trees trees;
  Node_Names names;
} Node_Metadata;

const char *node_kind_name(Node_Kind kind);

Node_Metadata new_node_metadata(void);
void node_metadata_free(Node_Metadata *m);
void *new_node(Node_Metadata *m, Arena *a, Node_Kind kind);
void add_node_flags(Node_Metadata *m, Node_ID id, Node_Flag flags);
bool has_error(Node_Metadata *m, void *node);

void add_child(Node_Metadata *m, Node_ID id, Node_Child child);
Node_Child child_token(Tok token);
Node_Child child_token_named(const char *name, Tok token);
Node_Child child_node(Node_ID node);
Node_Child child_node_named(const char *name, Node_ID node);

const char *get_node_name(Node_Metadata *m, Node_ID id);
Node_Children *get_node_children(Node_Metadata *m, Node_ID id);
Node_Child *last_child(Node_Metadata *m, Node_ID id);

typedef struct {
  FILE *fs;
  u32 indent_level;
  u8 indent_width;
  Node_Metadata *meta;
} Tree_Dump_Ctx;

void tree_dump(Tree_Dump_Ctx *ctx, Node_ID id);

#endif
