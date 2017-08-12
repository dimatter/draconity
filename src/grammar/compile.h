#ifndef GRAMMAR_COMPILE_H
#define GRAMMAR_COMPILE_H

#include <jansson.h>
#include <stdbool.h>

#include "node.h"
#include "tack.h"
#include "maclink.h"

typedef struct {
    uint32_t size, id;
    char name[0];
} __attribute__((packed)) id_entry;

typedef struct {
    int id;
    const char *name;
    void *data;
} node_id;

typedef struct {
    uint8_t *data;
    size_t size;
} buffer;

typedef struct {
    const char *main_rule;
    const char *name;
    buffer *raw;
    tack_t imports,
           exports,
           rules,
           lists,
           words;
    tack_t nfa, nodes, listdata;
    drg_grammar *handle;

    bool active;
    int priority;
    const char *appname;
    unsigned int endkey, beginkey, hypokey;
} Grammar;

int grammar_compile(Grammar **grammar, json_t *j, char **err);
Node *grammar_parse(const char *text, char **err);
void grammar_free(Grammar *grammar);

#define align4(len) ((len + 4) & ~3);

#endif
