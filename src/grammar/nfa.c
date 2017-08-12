#include <stdlib.h>
#include <stdio.h>

#include "grammar/node.h"
#include "node.h"
#include "nfa.h"

#define indent() for (int i = 0; i < level; i++) printf(" ")

void nfa_dump(const nfa *n, int level) {
    if (n) {
        if (n->node) {
            indent(); printf("-> ");
            switch (n->node->type) {
                case RULE:
                    printf("<%s>", n->node->name);
                    break;
                case LIST:
                    printf("{%s}", n->node->name);
                    break;
                case LITERAL:
                    printf("\"%s\"", n->node->name);
                    break;
                default: break;
            }
            level += 2;
            for (int i = 0; i < n->count; i++) {
                if (n->edges[i] == NULL) {
                    printf(" -> end");
                    break;
                }
            }
            printf("\n");
        }
        for (int i = 0; i < n->count; i++) {
            if (n->edges[i] == n) {
                indent(); printf("-> self\n");
            } else {
                nfa_dump(n->edges[i], level);
            }
        }
    }
}

#undef indent

static nfa *nfa_recurse(Node *node, nfa *next) {
    Node *child;
    size_t count = tack_len(&node->children);
    switch (node->type) {
        // SEQ is all children in sequence, chaining to next
        case SEQ:
            for (int i = count - 1; i >= 0; i--) {
                child = tack_get(&node->children, i);
                next = nfa_recurse(child, next);
            }
            return next;
        // ALT can reach all children, and all children can reach next
        case ALT: {
            nfa *alt = calloc(1, sizeof(nfa) + count * sizeof(nfa *));
            alt->count = count;
            tack_foreach(&node->children, child) {
                alt->edges[i] = nfa_recurse(child, next);
            }
            return alt;
        }
        // OPT is a skippable SEQ (front has second edge pointing to end)
        case OPT: {
            nfa *opt = calloc(1, sizeof(nfa) + 2 * sizeof(nfa *));
            opt->count = 2;
            opt->edges[1] = next;
            for (int i = count - 1; i >= 0; i--) {
                child = tack_get(&node->children, i);
                next = nfa_recurse(child, next);
            }
            opt->edges[0] = next;
            return opt;
        }
        // REP is a SEQ which adds an extra edge back to the start
        case REP: {
            nfa *rep = calloc(1, sizeof(nfa) + 2 * sizeof(nfa *));
            rep->count = 2;
            rep->edges[0] = rep;
            rep->edges[1] = next;
            next = rep;
            for (int i = count - 1; i >= 0; i--) {
                child = tack_get(&node->children, i);
                next = nfa_recurse(child, next);
            }
            return next;
        }
        case LITERAL:
        case RULE:
        case LIST: {
            nfa *leaf = calloc(1, sizeof(nfa) + sizeof(nfa *));
            leaf->node = node;
            leaf->edges[0] = next;
            leaf->count = 1;
            return leaf;
        }
    }
}

nfa *nfa_compile(Node *root) {
    return nfa_recurse(root, NULL);
}

void nfa_free(nfa *n) {
    for (int i = 0; i < n->count; i++) {
        if (n->edges[i] != n) {
            free(n->edges[i]);
        }
    }
    free(n);
}
