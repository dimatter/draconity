#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "node.h"

Node *node_new(int type, char *name) {
    Node *node = calloc(1, sizeof(Node));
    node->type = type;
    if (name != NULL) {
        node->name = name;
    }
    return node;
}

void node_push(Node *node, Node *child) {
    child->parent = node;
    tack_push(&node->children, child);
}

void node_free(Node *node) {
    if (node == NULL)
        return;
    free(node->name);
    Node *child;
    node_foreach(node, child) {
        node_free(child);
    }
    tack_clear(&node->children);
    free(node);
}

static void dump_children(Node *node) {
    Node *child;
    node_foreach(node, child) {
        node_dump(child);
    }
}

void node_dump(Node *node) {
    Node *child;
    if (node == NULL) {
        printf("(NULL)\n");
    } else {
        switch (node->type) {
        case SEQ:
            printf("<seq>\n");
            node_foreach(node, child) { node_dump(child); }
            printf("</seq>\n");
            break;
        case OPT:
            printf("<opt>\n");
            node_foreach(node, child) { node_dump(child); }
            printf("</opt>\n");
            break;
        case ALT:
            printf("<alt>\n");
            node_foreach(node, child) { node_dump(child); }
            printf("</alt>\n");
            break;
        case REP:
            printf("<rep>\n");
            node_foreach(node, child) { node_dump(child); }
            printf("</rep>\n");
            break;
        case LITERAL:
            printf("lit('%s')\n", node->name);
            break;
        case RULE:
            printf("rule('%s')\n", node->name);
            break;
        case LIST:
            printf("list('%s')\n", node->name);
            break;
        }
    }
}

static void node_combine_literals(Node *node) {
    // combine adjacent literals
    // (can't be done with ALT, which doesn't matter because ALT will contain SEQ)
    Node *child;
    if ((node->type == SEQ || node->type == OPT || node->type == REP) && tack_len(&node->children) > 1) {
        tack_t tmp = {0};
        tack_t strjoin = {0};
        bool literal = false;
        node_foreach(node, child) {
            if (child->type == LITERAL) {
                tack_push(&strjoin, child->name);
                free(child);
                literal = true;
            } else {
                if (literal) {
                    char *str = tack_str_join(&strjoin, " ");
                    tack_push(&tmp, node_new(LITERAL, str));
                    for (int i = 0; (str = tack_get(&strjoin, i)); i++) {
                        free(str);
                    }
                    tack_clear(&strjoin);
                }
                tack_push(&tmp, child);
                literal = false;
            }
        }
        if (literal) {
            char *str = tack_str_join(&strjoin, " ");
            tack_push(&tmp, node_new(LITERAL, str));
            for (int i = 0; (str = tack_get(&strjoin, i)); i++) {
                free(str);
            }
            tack_clear(&strjoin);
        }
        tack_clear(&node->children);
        memcpy(&node->children, &tmp, sizeof(tack_t));
    }
}

static Node *node_opt_priv(Node *node, bool top) {
    if (node == NULL)
        return NULL;

    Node *child;
    node_combine_literals(node);
    // can't collapse OPT
    if ((node->type == SEQ || node->type == ALT) && tack_len(&node->children) == 1 && !top) {
        Node *top = tack_pop(&node->children);
        node_free(node);
        node = node_opt_priv(top, false);
    } else {
        tack_t tmp = {0};
        node_foreach(node, child) {
            child = node_opt_priv(child, false);
            child->parent = node;
            tack_push(&tmp, child);
        }
        tack_clear(&node->children);
        memcpy(&node->children, &tmp, sizeof(tack_t));
    }
    return node;
}
Node *node_optimize(Node *node) { return node_opt_priv(node, true); }
