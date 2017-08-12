#ifndef PHRASE_END_H
#define PHRASE_END_H
#include "maclink.h"
#include "grammar/compile.h"

typedef struct {
    uint32_t id;
    char *word; // freed by DSXResult_Destroy
    uint32_t rule;
    const char *rule_name;
} result_node;

int phrase_hypothesis(Grammar *g, dsx_end_phrase *endphrase);
int phrase_end(Grammar *g, dsx_end_phrase *endphrase);
void phrase_begin(void *user);
#endif
