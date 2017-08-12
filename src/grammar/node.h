#ifndef GRAMMAR_NODE_H
#define GRAMMAR_NODE_H

#include <stdbool.h>
#include "tack.h"

enum node_type {
    SEQ, ALT, OPT, REP,
    LITERAL,
    RULE, LIST,
};

typedef struct node {
    enum node_type type;
    char *name, *key;
    uint32_t id;
    struct node *parent;
    tack_t children;
} Node;

Node *node_new(int type, char *name);
void node_free(Node *node);
void node_push(Node *node, Node *other);
void node_dump(Node *node);
Node *node_optimize(Node *node);

#define node_foreach(root, name) tack_foreach(&root->children, name)
#endif
