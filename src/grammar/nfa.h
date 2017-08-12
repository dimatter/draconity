#ifndef NFA_H
#define NFA_H

#include <stdint.h>

typedef struct nfa {
    uint32_t count;
    Node *node;
    struct nfa *edges[0];
} nfa;

void nfa_dump(const nfa *n, int level);
nfa *nfa_compile(Node *root);
static nfa *nfa_recurse(Node *node, nfa *next);
void nfa_free(nfa *n);

#endif
