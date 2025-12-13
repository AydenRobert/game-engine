#include "containers/binarytree.h"
#include "core/kmemory.h"
#include "defines.h"

typedef struct node node;

typedef struct internal_state {
    binarytree_config config;
    node *root;
    node *nodes;
    void *values;
    node *free_nodes;
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
        node *children[2];
    };
};

#define BINARYTREE_NODE_LEFT 0
#define BINARYTREE_NODE_RIGHT 1

#define BINARYTREE_COLOUR_BLACK false
#define BINARYTREE_COLOUR_RED true

#define BIT_MASK 0x01

node *get_tree_node();
void return_tree_node(node *n);

void rotate(internal_state *state, node *z, b8 dir);
void insert(internal_state *state, node *z);
void insert_balance(internal_state *state, node *z);

void transplant(internal_state *state, node *x, node *y);
void delete(internal_state *state, node *z);
void delete_balance(internal_state *state, node *z);

node *minimum(internal_state *state, node *x);

u32 search(internal_state *state, u64 identifier);

b8 binarytree_create(binarytree_config config, u64 *memory_requirement,
                     void *memory, binarytree *out_tree) {
    u64 nodes_size = sizeof(node) * (config.max_nodes);
    u64 vals_size = config.value_size * config.max_nodes;
    *memory_requirement = sizeof(internal_state) + nodes_size + vals_size;

    if (!memory) {
        return true;
    }

    if (!out_tree) {
        return false;
    }

    // Setup state
    out_tree->internal_state = memory;
    internal_state *state = (internal_state *)out_tree->internal_state;
    state->config = config;
    state->nodes = (void *)((u64)memory + sizeof(internal_state));
    state->values = (void *)((u64)memory + sizeof(internal_state) + nodes_size);

    return true;
}

void *binarytree_destroy(binarytree *tree) {
    internal_state *state = (internal_state *)tree->internal_state;
    // zero out state
    u64 nodes_size = sizeof(node) * (state->config.max_nodes + 1);
    u64 vals_size = state->config.value_size * state->config.max_nodes;
    u64 size = sizeof(internal_state) + nodes_size + vals_size;
    kzero_memory(tree->internal_state, size);
    return (void *)state;
}

b8 binarytree_insert(binarytree *tree, u64 identifier, void *value) {}

void *binarytree_search(binarytree *tree, u64 identifier) {}

void *binarytree_delete(binarytree *tree, u64 identifier) {}

void rotate(internal_state *state, node *z, b8 dir) {
    node *y = z->children[!dir];
    z->children[!dir] = y->children[dir];
    if (y->children[dir] != 0) {
        y->children[dir]->parent = z;
    }
    y->parent = z->parent;
    if (!z->parent) {
        state->root = y;
    } else {
        z->parent->children[z->parent->children[1] == z] = y;
    }
    y->children[dir] = z;
    z->parent = y;
}

void insert(internal_state *state, node *z) {
    node *y = 0;
    node *x = state->root;
    while (x != 0) {
        y = x;
        x = x->children[z->identifier >= x->identifier];
    }
    z->parent = y;
    // I assume this will be compiled down into two cmove operations
    if (y == 0) {
        state->root = z;
    } else {
        y->children[z->identifier >= y->identifier] = z;
    }
    z->left = 0;
    z->right = 0;
    z->colour = BINARYTREE_COLOUR_RED;
    insert_balance(state, z);
}

void insert_balance(internal_state *state, node *z) {
    while (z->parent->colour) {
        node *parent = z->parent;
        node *grand_parent = parent->parent;

        // 0 -> left, 1 -> right
        b8 dir = (parent == grand_parent->right);

        node *uncle = grand_parent->children[!dir];

        if (uncle && uncle->colour) {
            parent->colour = BINARYTREE_COLOUR_BLACK;
            uncle->colour = BINARYTREE_COLOUR_BLACK;
            grand_parent->colour = BINARYTREE_COLOUR_RED;
            z = grand_parent;
            continue;
        }

        if (z == parent->children[!dir]) {
            z = z->parent;
            rotate(state, z, dir);
        }

        z->parent->colour = BINARYTREE_COLOUR_BLACK;
        grand_parent->colour = BINARYTREE_COLOUR_RED;
        rotate(state, grand_parent, !dir);
    }

    state->root->colour = BINARYTREE_COLOUR_BLACK;
}

void transplant(internal_state *state, node *x, node *y) {
    if (x->parent == 0) {
        state->root = y;
    } else {
        b8 dir = x == x->parent->right;
        x->parent->children[dir] = y;
    }
    y->parent = x->parent;
}

void delete(internal_state *state, node *z) {
    node *y = z;
    b8 y_orig_colour = y->colour;

    node *x;
    if (!z->left || !z->right) {
        // Case 1: one child is NIL
        b8 dir = z->right != 0;
        x = z->children[dir];
    } else {
        // Case 2: no child is NIL
        y = minimum(state, z->right);
        y_orig_colour = y->colour;
        x = y->right;

        if (y->parent == z) {
            x->parent = y;
        } else {
            transplant(state, y, y->right);
            y->right = z->right;
            y->right->parent = y;
        }

        transplant(state, z, y);
        y->left = z->left;
        y->left->parent = y;
        y->colour = z->colour;
    }

    if (!y_orig_colour) {
        delete_balance(state, x);
    }
}

void delete_balance(internal_state *state, node *z) {
}

u32 search(internal_state *state, u64 identifier) {
    node *z = state->root;
    while (z) {
        if (z->identifier == identifier) {
            return z->index;
        }
        z = z->children[identifier >= z->identifier];
    }
    return INVALID_ID;
}

node *minimum(internal_state *state, node *x) {
    while (x->left) {
        x = x->left;
    }
    return x;
}
