#include "../ast/ast.h"

static void post_parse_enter(void *ctx, NodeMetadata *m, AnyNode node);
static void normalize_atom(NodeMetadata *m, Atom *atom);

void do_post_parse(NodeMetadata *m, AnyNode root) {
    ast_traverse_dfs(NULL, root, m,
                     (EnterExitVTable){
                         .enter = post_parse_enter,
                         .exit = NULL,
                     });
}

static void post_parse_enter(void *ctx, NodeMetadata *m, AnyNode node) {
    (void)ctx;
    switch (node.kind) {
        case NODE_ATOM:
            normalize_atom(m, (Atom *)node.data);
            break;
        default:
            break;
    }
}

// At syntax parse time, so as to not need backtracking, designator and
// scoped identifier atoms are parsed as the same node (a designator).
// This function just changes the atom to reference a scoped identifier
// if it is a designator without an initialiser list.
static void normalize_atom(NodeMetadata *m, Atom *atom) {
    if (atom->t == ATOM_DESIGNATOR && atom->designator->init.ptr == NULL) {
        // Sanity check that the scoped identifier is the sole child
        assert(get_node_children(m, atom->designator->id)->len == 1);
        assert(get_node_children(m, atom->id)->len == 1);
        remove_child(m, atom->id, atom->designator->id);
        atom->t = ATOM_SCOPED_IDENT;
        atom->scoped_ident = atom->designator->ident;
        add_child(m, atom->id, child_node(MAKE_ANY(atom->scoped_ident)));
    }
}
