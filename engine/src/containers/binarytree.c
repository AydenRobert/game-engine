#include "containers/binarytree.h"
#include "core/kmemory.h"
#include "defines.h"

typedef struct node node;

typedef struct internal_state {
    u32 max_nodes;
    node *root;
    node *nodes;
    node *free_nodes;
    node *nil;
} internal_state;

struct node {
    u64 identifier;
    u32 index;
    b8 colour;
    node *parent;
    union {
        struct {
            node *left;
            node *right;
        };
        // Allows for branchless code
        node *children[2];
    };
};

#define BINARYTREE_NODE_LEFT 0
#define BINARYTREE_NODE_RIGHT 1

#define BINARYTREE_COLOUR_BLACK false
#define BINARYTREE_COLOUR_RED true

#define BIT_MASK 0x01

node *get_tree_node(internal_state *state);
void return_tree_node(internal_state *state, node *n);

void rotate(internal_state *state, node *z, b8 dir);
b8 insert(internal_state *state, node *z);
void insert_balance(internal_state *state, node *z);

void transplant(internal_state *state, node *x, node *y);
void delete(internal_state *state, node *z);
void delete_balance(internal_state *state, node *z);

node *minimum(internal_state *state, node *x);

node *search(internal_state *state, u64 identifier);

b8 binarytree_create(u32 max_nodes, u64 *memory_requirement, void *memory,
                     binarytree *out_tree) {
    // Calculate memory sizes
    u64 nodes_size = sizeof(node) * (max_nodes + 1);
    *memory_requirement = sizeof(internal_state) + nodes_size;

    if (!memory) {
        return true;
    }

    if (!out_tree) {
        return false;
    }

    // Setup state
    out_tree->internal_state = memory;
    internal_state *state = (internal_state *)out_tree->internal_state;
    kzero_memory(state, sizeof(internal_state));
    state->max_nodes = max_nodes;
    state->nodes = (void *)((u64)memory + sizeof(internal_state));

    // Setup free nodes
    kzero_memory(state->nodes, nodes_size);
    state->free_nodes = state->nodes;
    for (u32 i = 0; i < state->max_nodes; i++) {
        state->nodes[i].right =
            (i == state->max_nodes - 1) ? 0 : &state->nodes[i + 1];
    }
    state->nil = &state->nodes[max_nodes];
    state->nil->colour = BINARYTREE_COLOUR_BLACK;
    state->root = state->nil;

    return true;
}

void *binarytree_destroy(binarytree *tree) {
    internal_state *state = (internal_state *)tree->internal_state;
    // zero out state
    u64 nodes_size = sizeof(node) * (state->max_nodes + 1);
    u64 size = sizeof(internal_state) + nodes_size;
    kzero_memory(tree->internal_state, size);
    // Return memory for freeing if needed
    return (void *)state;
}

b8 binarytree_insert(binarytree *tree, u64 identifier, u32 index) {
    if (!tree || !tree->internal_state) {
        return false;
    }
    internal_state *state = (internal_state *)tree->internal_state;

    if (!state->nodes || !state->free_nodes) {
        return false;
    }

    // Just checked free nodes, do not have to check this
    node *n = get_tree_node(state);

    // Setup node
    n->identifier = identifier;
    n->index = index;
    // Insert node
    if (!insert(state, n)) {
        return_tree_node(state, n);
        return false;
    }
    return true;
}

b8 binarytree_search(binarytree *tree, u64 identifier, u32 *out_index) {
    if (!tree || !tree->internal_state) {
        return false;
    }
    internal_state *state = (internal_state *)tree->internal_state;

    if (!state->nodes) {
        return false;
    }

    // get the node
    node *n = search(state, identifier);
    if (!n) {
        return false;
    }

    // Set the out index
    *out_index = n->index;
    return true;
}

b8 binarytree_delete(binarytree *tree, u64 identifier, u32 *out_index) {
    if (!tree || !tree->internal_state) {
        return false;
    }
    internal_state *state = (internal_state *)tree->internal_state;

    if (!state->nodes) {
        return false;
    }

    // Get the node
    node *n = search(state, identifier);
    if (!n) {
        return false;
    }
    *out_index = n->index;

    // Delete and return the node to state->free_nodes
    delete(state, n);
    return_tree_node(state, n);

    return true;
}

void rotate(internal_state *state, node *rotation_root, b8 direction) {
    node *pivot = rotation_root->children[!direction];

    rotation_root->children[!direction] = pivot->children[direction];
    if (pivot->children[direction] != state->nil) {
        pivot->children[direction]->parent = rotation_root;
    }
    pivot->parent = rotation_root->parent;
    if (rotation_root->parent == state->nil) {
        state->root = pivot;
    } else {
        rotation_root->parent
            ->children[rotation_root->parent->children[1] == rotation_root] =
            pivot;
    }
    pivot->children[direction] = rotation_root;
    rotation_root->parent = pivot;
}

b8 insert(internal_state *state, node *new_node) {
    node *parent = state->nil;
    node *iterator = state->root;

    while (iterator != state->nil) {
        parent = iterator;

        // Check for duplicates -> maybe will change behaviour later
        if (new_node->identifier == iterator->identifier) {
            return false;
        }

        iterator =
            iterator->children[new_node->identifier > iterator->identifier];
    }

    new_node->parent = parent;
    if (parent == state->nil) {
        state->root = new_node;
    } else {
        parent->children[new_node->identifier >= parent->identifier] = new_node;
    }

    new_node->left = state->nil;
    new_node->right = state->nil;
    new_node->colour = BINARYTREE_COLOUR_RED;
    insert_balance(state, new_node);
    return true;
}

void insert_balance(internal_state *state, node *current_node) {
    while (current_node->parent && current_node->parent->colour) {
        node *parent = current_node->parent;
        node *grand_parent = parent->parent;

        b8 direction = (parent == grand_parent->right);
        node *uncle = grand_parent->children[!direction];

        if (uncle && uncle->colour) {
            // Case 1: Uncle is red, recolour grand_parant, uncle and parent
            parent->colour = BINARYTREE_COLOUR_BLACK;
            uncle->colour = BINARYTREE_COLOUR_BLACK;
            grand_parent->colour = BINARYTREE_COLOUR_RED;
            current_node = grand_parent;
            continue;
        }

        if (current_node == parent->children[!direction]) {
            // Case 2: Uncle is black, and current node is the inner child
            // (triangle shape)
            current_node = current_node->parent;
            rotate(state, current_node, direction);
            // Update parent pointer
            parent = current_node->parent;
        }

        // Case 3: Uncle is black, and current node is the outer child (line
        // shape)
        parent->colour = BINARYTREE_COLOUR_BLACK;
        grand_parent->colour = BINARYTREE_COLOUR_RED;
        rotate(state, grand_parent, !direction);
    }

    state->root->colour = BINARYTREE_COLOUR_BLACK;
}

void transplant(internal_state *state, node *dest, node *src) {
    if (dest->parent == state->nil) {
        state->root = src;
    } else {
        b8 direction = dest == dest->parent->right;
        dest->parent->children[direction] = src;
    }

    if (src != state->nil) {
        src->parent = dest->parent;
    }
}

void delete(internal_state *state, node *target) {
    node *splice_node = target;
    node *child_node;
    b8 splice_original_colour = splice_node->colour;

    if (target->left == state->nil || target->right == state->nil) {
        // Case 1: one child is NIL, replace with non-NIL child
        b8 direction = target->right != 0;
        child_node = target->children[direction];
        transplant(state, target, child_node);
    } else {
        // Case 2: no child is NIL, find successor
        splice_node = minimum(state, target->right);
        splice_original_colour = splice_node->colour;
        child_node = splice_node->right;

        if (splice_node->parent == target) {
            child_node->parent = splice_node;
        } else {
            transplant(state, splice_node, splice_node->right);
            splice_node->right = target->right;
            splice_node->right->parent = splice_node;
        }

        transplant(state, target, splice_node);
        splice_node->left = target->left;
        splice_node->left->parent = splice_node;
        splice_node->colour = target->colour;
    }

    if (splice_original_colour == BINARYTREE_COLOUR_BLACK) {
        delete_balance(state, child_node);
    }
}

void delete_balance(internal_state *state, node *current_node) {
    while (current_node != state->root &&
           current_node->colour == BINARYTREE_COLOUR_BLACK) {
        b8 dir = current_node == current_node->parent->right;
        node *sibling = current_node->parent->children[!dir];

        if (sibling->colour == BINARYTREE_COLOUR_RED) {
            // Case 1: Sibling is red
            sibling->colour = BINARYTREE_COLOUR_BLACK;
            current_node->parent->colour = BINARYTREE_COLOUR_RED;
            rotate(state, current_node->parent, dir);
            sibling = current_node->parent->children[!dir];
        }

        if (sibling->left->colour == BINARYTREE_COLOUR_BLACK &&
            sibling->right->colour == BINARYTREE_COLOUR_BLACK) {
            // Case 2: Sibling and both children are black
            sibling->colour = BINARYTREE_COLOUR_RED;
            current_node = current_node->parent;
        } else {
            if (sibling->children[!dir]->colour == BINARYTREE_COLOUR_BLACK) {
                sibling->children[dir]->colour = BINARYTREE_COLOUR_BLACK;
                // Case 2: Sibling is black and near child is red.
                sibling->colour = BINARYTREE_COLOUR_RED;
                rotate(state, sibling, !dir);
                sibling = current_node->parent->children[!dir];
            }

            // Case 3: Sibling is black and far child is red.
            sibling->colour = current_node->parent->colour;
            current_node->parent->colour = BINARYTREE_COLOUR_BLACK;
            // if nil, just setting nil node to black
            sibling->children[!dir]->colour = BINARYTREE_COLOUR_BLACK;
            rotate(state, current_node->parent, dir);
            current_node = state->root;
        }
    }

    if (state->root != state->nil) {
        state->root->colour = BINARYTREE_COLOUR_BLACK;
    }
}

node *search(internal_state *state, u64 identifier) {
    node *iterator = state->root;
    while (iterator != state->nil) {
        if (iterator->identifier == identifier) {
            return iterator;
        }
        iterator = iterator->children[identifier >= iterator->identifier];
    }
    return 0;
}

node *minimum(internal_state *state, node *start_node) {
    while (start_node->left != state->nil) {
        start_node = start_node->left;
    }
    return start_node;
}

node *get_tree_node(internal_state *state) {
    // NOTE: only calling code checks state->free_nodes
    node *new_node = state->free_nodes;
    state->free_nodes = new_node->right;
    new_node->right = 0;
    return new_node;
}

void return_tree_node(internal_state *state, node *n) {
    kzero_memory(n, sizeof(node));
    n->right = state->free_nodes;
    state->free_nodes = n;
}
