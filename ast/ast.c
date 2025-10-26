#include "ast.h"

#include <assert.h>

Node_Metadata new_node_metadata(void) {
  return (Node_Metadata){
      .next_id = 0,
      .flags =
          {
              .len = 0,
              .cap = 0,
              .items = NULL,
          },
      .trees =
          {
              .len = 0,
              .cap = 0,
              .items = NULL,
          },
      .names =
          {
              .len = 0,
              .cap = 0,
              .items = NULL,
          },
  };
}

void node_metadata_free(Node_Metadata *m) {
  if (m->flags.items != NULL) {
    free(m->flags.items);
  }
  if (m->trees.items != NULL) {
    for (u32 id = 0; id < m->trees.len; id++) {
      Node_Children *children = get_node_children(m, id);
      free(children->items);
    }
    free(m->trees.items);
  }
  if (m->names.items != NULL) {
    free(m->names.items);
  }
}

void add_node_flags(Node_Metadata *m, Node_ID id, Node_Flag flags) {
  m->flags.items[id] |= flags;
}

bool has_error(Node_Metadata *m, void *node) {
  assert(node != NULL);
  Node_ID *id = node;
  return m->flags.items[*id] & NFLAG_ERROR;
}

void add_child(Node_Metadata *m, Node_ID id, Node_Child child) {
  Node_Children *children = &m->trees.items[id];
  APPEND(children, child);
}

Node_Child child_token(Tok token) {
  return (Node_Child){
      .t = CHILD_TOKEN,
      .token = token,
      .name.ok = false,
  };
}

Node_Child child_token_named(const char *name, Tok token) {
  return (Node_Child){
      .t = CHILD_TOKEN,
      .token = token,
      .name = {.value = name, .ok = true},
  };
}

Node_Child child_node(Node_ID id) {
  return (Node_Child){
      .t = CHILD_NODE,
      .id = id,
      .name.ok = false,
  };
}

Node_Child child_node_named(const char *name, Node_ID id) {
  return (Node_Child){
      .t = CHILD_NODE,
      .id = id,
      .name = {.value = name, .ok = true},
  };
}

typedef struct {
  usize size;
  usize align;
} Layout;

#define LAYOUT_OF(T) \
  (Layout) { .size = sizeof(T), .align = _Alignof(T) }

typedef struct {
  const char *name;
  Layout layout;
} Node_Descriptor;

static Node_Descriptor node_descriptors[];

const char *node_kind_name(Node_Kind kind) {
  return node_descriptors[kind].name;
}

void *new_node(Node_Metadata *m, Arena *a, Node_Kind kind) {
  Layout layout = node_descriptors[kind].layout;
  Node_ID *r = arena_alloc(a, layout.size, layout.align);
  APPEND(&m->flags, 0);
  APPEND(&m->names, node_kind_name(kind));
  APPEND(&m->trees, (Node_Children){.len = 0, .cap = 0, .items = NULL});
  *r = m->next_id++;
  return r;
}

const char *get_node_name(Node_Metadata *m, Node_ID id) {
  return *AT(m->names, id);
}

Node_Children *get_node_children(Node_Metadata *m, Node_ID id) {
  return AT(m->trees, id);
}

Node_Child *last_child(Node_Metadata *m, Node_ID id) {
  Node_Children *children = AT(m->trees, id);
  return AT(*children, children->len - 1);
}

static Node_Descriptor node_descriptors[NODE_KIND_COUNT] = {
    [NODE_SOURCE_FILE] = {"source_file", LAYOUT_OF(Source_File)},
    [NODE_IMPORTS] = {"imports", LAYOUT_OF(Imports)},
    [NODE_DECLARATIONS] = {"declarations", LAYOUT_OF(Declarations)},
    [NODE_IMPORT] = {"import", LAYOUT_OF(Import)},
    [NODE_DECLARATION] = {"declaration", LAYOUT_OF(Declaration)},
    [NODE_VARIABLE_DECLARATION] = {"variable_declaration",
                                   LAYOUT_OF(Variable_Declaration)},
    [NODE_FUNCTION_DECLARATION] = {"function_declaration",
                                   LAYOUT_OF(Function_Declaration)},
    [NODE_TYPE_PARAMETER_LIST] = {"type_parameter_list",
                                  LAYOUT_OF(Type_Parameter_List)},
    [NODE_STRUCT_DECLARATION] = {"struct_declaration",
                                 LAYOUT_OF(Struct_Declaration)},
    [NODE_STRUCT_BODY] = {"struct_body", LAYOUT_OF(Struct_Body)},
    [NODE_ENUM_DECLARATION] = {"enum_declaration", LAYOUT_OF(Enum_Declaration)},
    [NODE_ERROR_DECLARATION] = {"error_declaration",
                                LAYOUT_OF(Error_Declaration)},
    [NODE_UNION_DECLARATION] = {"union_declaration",
                                LAYOUT_OF(Union_Declaration)},
    [NODE_DESTRUCTURE_TUPLE] = {"destructure_tuple",
                                LAYOUT_OF(Destructure_Tuple)},
    [NODE_DESTRUCTURE_STRUCT] = {"destructure_struct",
                                 LAYOUT_OF(Destructure_Struct)},
    [NODE_DESTRUCTURE_UNION] = {"destructure_union",
                                LAYOUT_OF(Destructure_Union)},
    [NODE_BINDING] = {"binding", LAYOUT_OF(Binding)},
    [NODE_BINDING_LIST] = {"binding_list", LAYOUT_OF(Binding_List)},
    [NODE_ALIASED_BINDING] = {"aliased_binding", LAYOUT_OF(Aliased_Binding)},
    [NODE_ALIASED_BINDING_LIST] = {"aliased_binding_list",
                                   LAYOUT_OF(Aliased_Binding_List)},
    [NODE_VARIABLE_BINDING] = {"variable_binding", LAYOUT_OF(Variable_Binding)},
    [NODE_FUNCTION_PARAMETER_LIST] = {"function_parameter_list",
                                      LAYOUT_OF(Function_Parameter_List)},
    [NODE_FUNCTION_PARAMETER] = {"function_parameter",
                                 LAYOUT_OF(Function_Parameter)},
    [NODE_TYPE_LIST] = {"type_list", LAYOUT_OF(Type_List)},
    [NODE_FIELD_LIST] = {"field_list", LAYOUT_OF(Field_List)},
    [NODE_FIELD] = {"field", LAYOUT_OF(Field)},
    [NODE_IDENTIFIER_LIST] = {"identifier_list", LAYOUT_OF(Identifier_List)},
    [NODE_ERROR_LIST] = {"error_list", LAYOUT_OF(Error_List)},
    [NODE_ERROR] = {"error", LAYOUT_OF(Error)},
    [NODE_STATEMENT] = {"statement", LAYOUT_OF(Statement)},
    [NODE_IF_STATEMENT] = {"if_statement", LAYOUT_OF(If_Statement)},
    [NODE_CONDITION] = {"condition", LAYOUT_OF(Condition)},
    [NODE_UNION_TAG_CONDITION] = {"union_tag_condition",
                                  LAYOUT_OF(Union_Tag_Condition)},
    [NODE_RETURN_STATEMENT] = {"return_statement", LAYOUT_OF(Return_Statement)},
    [NODE_DEFER_STATEMENT] = {"defer_statement", LAYOUT_OF(Defer_Statement)},
    [NODE_COMPOUND_STATEMENT] = {"compound_statement",
                                 LAYOUT_OF(Compound_Statement)},
    [NODE_ELSE] = {"else", LAYOUT_OF(Else)},
    [NODE_TYPE] = {"type", LAYOUT_OF(Type)},
    [NODE_BUILTIN_TYPE] = {"builtin_type", LAYOUT_OF(Builtin_Type)},
    [NODE_COLLECTION_TYPE] = {"collection_type", LAYOUT_OF(Collection_Type)},
    [NODE_STRUCT_TYPE] = {"struct_type", LAYOUT_OF(Struct_Type)},
    [NODE_UNION_TYPE] = {"union_type", LAYOUT_OF(Union_Type)},
    [NODE_ENUM_TYPE] = {"enum_type", LAYOUT_OF(Enum_Type)},
    [NODE_ERROR_TYPE] = {"error_type", LAYOUT_OF(Error_Type)},
    [NODE_POINTER_TYPE] = {"pointer_type", LAYOUT_OF(Pointer_Type)},
    [NODE_FUNCTION_TYPE] = {"function_type", LAYOUT_OF(Function_Type)},
    [NODE_SCOPED_IDENTIFIER] = {"scoped_identifier",
                                LAYOUT_OF(Scoped_Identifier)},
    [NODE_EXPRESSION] = {"expression", LAYOUT_OF(Expression)},
    [NODE_BASIC_EXPRESSION] = {"basic_expression", LAYOUT_OF(Basic_Expression)},
    [NODE_PARENTHESIZED_EXPRESSION] = {"parenthesized_expression",
                                       LAYOUT_OF(Parenthesized_Expression)},
    [NODE_COMPOSITE_LITERAL_EXPRESSION] = {"composite_literal_expression",
                                           LAYOUT_OF(
                                               Composite_Literal_Expression)},
    [NODE_POSTFIX_EXPRESSION] = {"postfix_expression",
                                 LAYOUT_OF(Postfix_Expression)},
    [NODE_FUNCTION_CALL_EXPRESSION] = {"function_call_expression",
                                       LAYOUT_OF(Function_Call_Expression)},
    [NODE_FIELD_ACCESS_EXPRESSION] = {"field_access_expression",
                                      LAYOUT_OF(Field_Access_Expression)},
    [NODE_INDEX] = {"index", LAYOUT_OF(Index)},
    [NODE_ARRAY_ACCESS_EXPRESSION] = {"array_access_expression",
                                      LAYOUT_OF(Array_Access_Expression)},
    [NODE_UNARY_EXPRESSION] = {"unary_expression", LAYOUT_OF(Unary_Expression)},
    [NODE_BINARY_EXPRESSION] = {"binary_expression",
                                LAYOUT_OF(Binary_Expression)},
    [NODE_INITIALIZER_LIST] = {"initializer_list", LAYOUT_OF(Initializer_List)},
    [NODE_BRACED_LITERAL] = {"braced_literal", LAYOUT_OF(Braced_Literal)},
};
