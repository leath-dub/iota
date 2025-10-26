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
struct Struct_Body;
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
struct Condition;
struct Union_Tag_Condition;
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
struct Function_Type;
struct Scoped_Identifier;
struct Expression;
struct Basic_Expression;
struct Parenthesized_Expression;
struct Composite_Literal_Expression;
struct Postfix_Expression;
struct Function_Call_Expression;
struct Initializer_List;
struct Field_Access_Expression;
struct Array_Access_Expression;
struct Unary_Expression;
struct Binary_Expression;
struct Braced_Literal;
struct Index;

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

typedef struct Source_File Source_File;
typedef struct Imports Imports;
typedef struct Declarations Declarations;
typedef struct Import Import;
typedef struct Declaration Declaration;
typedef struct Variable_Declaration Variable_Declaration;
typedef struct Function_Declaration Function_Declaration;
typedef struct Type_Parameter_List Type_Parameter_List;
typedef struct Struct_Declaration Struct_Declaration;
typedef struct Struct_Body Struct_Body;
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
typedef struct If_Statement If_Statement;
typedef struct Condition Condition;
typedef struct Union_Tag_Condition Union_Tag_Condition;
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
typedef struct Function_Type Function_Type;
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
typedef struct Initializer_List Initializer_List;
typedef struct Braced_Literal Braced_Literal;

typedef enum {
  NODE_SOURCE_FILE,
  NODE_IMPORTS,
  NODE_DECLARATIONS,
  NODE_IMPORT,
  NODE_DECLARATION,
  NODE_VARIABLE_DECLARATION,
  NODE_FUNCTION_DECLARATION,
  NODE_TYPE_PARAMETER_LIST,
  NODE_STRUCT_DECLARATION,
  NODE_STRUCT_BODY,
  NODE_ENUM_DECLARATION,
  NODE_ERROR_DECLARATION,
  NODE_UNION_DECLARATION,
  NODE_DESTRUCTURE_TUPLE,
  NODE_DESTRUCTURE_STRUCT,
  NODE_DESTRUCTURE_UNION,
  NODE_BINDING,
  NODE_BINDING_LIST,
  NODE_ALIASED_BINDING,
  NODE_ALIASED_BINDING_LIST,
  NODE_VARIABLE_BINDING,
  NODE_FUNCTION_PARAMETER_LIST,
  NODE_FUNCTION_PARAMETER,
  NODE_TYPE_LIST,
  NODE_FIELD_LIST,
  NODE_FIELD,
  NODE_IDENTIFIER_LIST,
  NODE_ERROR_LIST,
  NODE_ERROR,
  NODE_STATEMENT,
  NODE_IF_STATEMENT,
  NODE_CONDITION,
  NODE_UNION_TAG_CONDITION,
  NODE_RETURN_STATEMENT,
  NODE_DEFER_STATEMENT,
  NODE_COMPOUND_STATEMENT,
  NODE_ELSE,
  NODE_TYPE,
  NODE_BUILTIN_TYPE,
  NODE_COLLECTION_TYPE,
  NODE_STRUCT_TYPE,
  NODE_UNION_TYPE,
  NODE_ENUM_TYPE,
  NODE_ERROR_TYPE,
  NODE_POINTER_TYPE,
  NODE_FUNCTION_TYPE,
  NODE_SCOPED_IDENTIFIER,
  NODE_EXPRESSION,
  NODE_BASIC_EXPRESSION,
  NODE_PARENTHESIZED_EXPRESSION,
  NODE_COMPOSITE_LITERAL_EXPRESSION,
  NODE_POSTFIX_EXPRESSION,
  NODE_FUNCTION_CALL_EXPRESSION,
  NODE_FIELD_ACCESS_EXPRESSION,
  NODE_INDEX,
  NODE_ARRAY_ACCESS_EXPRESSION,
  NODE_UNARY_EXPRESSION,
  NODE_BINARY_EXPRESSION,
  NODE_INITIALIZER_LIST,
  NODE_BRACED_LITERAL,
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
