#ifndef AST_H
#define AST_H

#include "../common/common.h"
#include "../lex/lex.h"

typedef u32 Node_ID;

struct Imports;
struct Declarations;
struct Import;
struct Declaration;
struct Variable_Declaration;
struct Function_Declaration;
struct Type_Parameter_List;
struct Struct_Declaration;
struct Enum_Declaration;
struct Error_Declaration;
struct Union_Declaration;
struct Variable_Binding;
struct Destructure_Tuple;
struct Destructure_Struct;
struct Destructure_Union;
struct Binding;
struct Binding_List;
struct Aliased_Binding;
struct Aliased_Binding_List;
struct Function_Parameter_List;
struct Function_Parameter;
struct Type_List;
struct Field_List;
struct Field;
struct Identifier_List;
struct Error_List;
struct Error;
struct Statement;
struct Declaration;
struct If_Statement;
struct Return_Statement;
struct Defer_Statement;
struct Compound_Statement;
struct Else;
struct Type;
struct Builtin_Type;
struct Collection_Type;
struct Struct_Type;
struct Union_Type;
struct Enum_Type;
struct Error_Type;
struct Pointer_Type;
struct Scoped_Identifier;
struct Expression;
struct Basic_Expression;
struct Parenthesized_Expression;
struct Composite_Literal_Expression;
struct Postfix_Expression;
struct Function_Call_Expression;
struct Field_Access_Expression;
struct Array_Access_Expression;
struct Unary_Expression;
struct Binary_Expression;

struct Index;

typedef struct {
  Node_ID id;
  struct Imports *imports;
  struct Declarations *declarations;
} Source_File;

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
  Tok classifier; // let or mut
  struct Variable_Binding *binding;
  struct Type *type; // nullable
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
  struct { Tok value; bool ok; } alias;
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
  Tok reference; // could be a '*' meaning the binding is a reference
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
  struct { struct Type_Parameter_List *value; bool ok; } type_params_opt;
  struct Function_Parameter_List *params;
  struct Type *return_type; // nullable
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
  Tok identifier;
  bool variadic;
  struct Type *type;
};

struct Type_Parameter_List {
  Node_ID id;
  struct Type_List *types;
};

struct Struct_Declaration {
  Node_ID id;
  Tok identifier;
  struct { struct Type_Parameter_List *value; bool ok; } type_params_opt;
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
  bool embedded; // e.g. !Foo
  struct Scoped_Identifier *scoped_identifier;
};

struct Union_Declaration {
  Node_ID id;
  Tok identifier;
  struct { struct Type_Parameter_List *value; bool ok; } type_params_opt;
  struct Field_List *fields;
};

typedef enum {
  STATMENT_DECLARATION,
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
  struct Expression *condition;
  struct Compound_Statement *true_branch;
  struct Else *else_branch; // nullable
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
  struct Expr *expression; // nullable
};

struct Defer_Statement {
  Node_ID id;
  struct Statement *statement;
};

struct Compound_Statement {
  Node_ID id;
  u32 len;
  u32 cap;
  struct Statement *items;
};

typedef enum {
  TYPE_BUILTIN,
  TYPE_COLLECTION,
  TYPE_STRUCT,
  TYPE_UNION,
  TYPE_ENUM,
  TYPE_ERROR,
  TYPE_POINTER,
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
    struct Scoped_Identifier *scoped_identifier;
  };
};

struct Builtin_Type {
  Node_ID id;
  Tok token; // just stores the keyword token
};

struct Collection_Type {
  Node_ID id;
  struct Expression *index_expression; // nullable
  struct Type *element_type;
};

struct Struct_Type {
  Node_ID id;
  bool tuple_like;
  struct { struct Type_Parameter_List *value; bool ok; } type_params_opt;
  union {
    struct Type_List *type_list;
    struct Field_List *field_list;
  };
};

struct Type_List {
  Node_ID id;
  u32 len;
  u32 cap;
  struct Type **items;
};

struct Union_Type {
  Node_ID id;
  struct { struct Type_Parameter_List *value; bool ok; } type_params_opt;
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
  Tok classifier; // let or mut
  struct Type *referenced_type;
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

struct Basic_Expression {
  Node_ID id;
  Tok token; // Stores: identifier, number, string, enum, nil
};

struct Parenthesized_Expression {
  Node_ID id;
  struct Expression *inner_expression;
};

struct Composite_Literal_Expression {
  Node_ID id;
  struct { struct Type *value; bool ok; } explicit_type;
  struct Expression *value;
};

struct Function_Call_Expression {
  Node_ID id;
  struct Expression *function;
  struct Expression *arguments;
  // arguments to the function are an expression
  // as this allows you to have assignments and
  // comma separted values e.g.:
  //
  // foo(x = 10, 10)
  //
  // gives a parse tree like so:
  //
  // (function_call
  //   (comma
  //     (assignment ...)
  //     (basic ...)))
};

struct Postfix_Expression {
  Node_ID id;
  struct Expression *inner_expression;
  Tok op;
};

struct Field_Access_Expression {
  Node_ID id;
  struct Expression *value;
  Tok field;
};

struct Array_Access_Expression {
  Node_ID id;
  struct Expression *value;
  struct Index *index;
};

struct Index {
  Node_ID id;
  struct Expression *start; // nullable
  struct Expression *end; // nullable
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

typedef struct Imports Imports;
typedef struct Declarations Declarations;
typedef struct Import Import;
typedef struct Declaration Declaration;
typedef struct Variable_Declaration Variable_Declaration;
typedef struct Function_Declaration Function_Declaration;
typedef struct Type_Parameter_List Type_Parameter_List;
typedef struct Struct_Declaration Struct_Declaration;
typedef struct Enum_Declaration Enum_Declaration;
typedef struct Error_Declaration Error_Declaration;
typedef struct Union_Declaration Union_Declaration;
typedef struct Destructure_Tuple Destructure_Tuple;
typedef struct Destructure_Struct Destructure_Struct;
typedef struct Destructure_Union Destructure_Union;
typedef struct Binding Binding;
typedef struct Binding_List Binding_List;
typedef struct Aliased_Binding Aliased_Binding;
typedef struct Aliased_Binding_List Aliased_Binding_List;
typedef struct Variable_Binding Variable_Binding;
typedef struct Function_Parameter_List Function_Parameter_List;
typedef struct Function_Parameter Function_Parameter;
typedef struct Type_List Type_List;
typedef struct Field_List Field_List;
typedef struct Field Field;
typedef struct Identifier_List Identifier_List;
typedef struct Error_List Error_List;
typedef struct Error Error;
typedef struct Statement Statement;
typedef struct Declaration Declaration;
typedef struct If_Statement If_Statement;
typedef struct Return_Statement Return_Statement;
typedef struct Defer_Statement Defer_Statement;
typedef struct Compound_Statement Compound_Statement;
typedef struct Else Else;
typedef struct Type Type;
typedef struct Builtin_Type Builtin_Type;
typedef struct Collection_Type Collection_Type;
typedef struct Struct_Type Struct_Type;
typedef struct Union_Type Union_Type;
typedef struct Enum_Type Enum_Type;
typedef struct Error_Type Error_Type;
typedef struct Pointer_Type Pointer_Type;
typedef struct Scoped_Identifier Scoped_Identifier;
typedef struct Expression Expression;
typedef struct Basic_Expression Basic_Expression;
typedef struct Parenthesized_Expression Parenthesized_Expression;
typedef struct Composite_Literal_Expression Composite_Literal_Expression;
typedef struct Postfix_Expression Postfix_Expression;
typedef struct Function_Call_Expression Function_Call_Expression;
typedef struct Field_Access_Expression Field_Access_Expression;
typedef struct Index Index;
typedef struct Array_Access_Expression Array_Access_Expression;
typedef struct Unary_Expression Unary_Expression;
typedef struct Binary_Expression Binary_Expression;

typedef enum {
  NFLAG_NONE = 0,
  NFLAG_ERROR = 1,
} Node_Flags;

typedef struct {
  Node_Flags *items;
  u32 cap;
  u32 len;
} Node_Flags_Data;

typedef struct {
  Node_ID next_id;
  Node_Flags_Data flags;
} Node_Metadata;

Node_Metadata new_node_metadata(void);
void node_metadata_free(Node_Metadata *m);
void *new_node(Node_Metadata *m, Arena *a, usize size, usize align);
void add_node_flags(Node_Metadata *m, Node_ID id, Node_Flags flags);

bool has_error(Node_Metadata *m, void *node);

#endif
