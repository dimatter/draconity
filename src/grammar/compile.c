#include <jansson.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "compile.h"
#include "nfa.h"

enum gram_type {
    start_type = 1,
    end_type = 2,
    word_type = 3,
    rule_type = 4,
    list_type = 6,
};

enum gram_val {
    seq_val = 1,
    alt_val = 2,
    rep_val = 3,
    opt_val = 4,
};

typedef struct {
    uint16_t type, prob;
    uint32_t val;
} __attribute__((packed)) rule_def;

typedef struct {
    uint32_t size, id;
} __attribute__((packed)) rule_header;

typedef struct {
    uint32_t type, size;
    uint8_t data[0];
} __attribute__((packed)) chunk_header;

typedef struct {
    uint32_t type, flags;
} __attribute__((packed)) grammar_header;

void pbuf(buffer *buf) {
    for (int i = 0; i < buf->size; i++) {
        printf("%02x", buf->data[i]);
    }
    printf("\n");
}

node_id *id_new(const char *name, int id) {
    node_id *new = malloc(sizeof(node_id));
    new->id = id;
    new->name = name ? strdup(name) : NULL;
    new->data = NULL;
    return new;
}

void id_free(node_id *ent) {
    free(ent->data);
    free((void *)ent->name);
    free(ent);
}

static int get_id(tack_t *list, char *name) {
    node_id *ent;
    if (! tack_hexists(list, name)) {
        ent = id_new(name, tack_len(list) + 1);
        tack_push(list, ent);
        tack_hset(list, name, ent);
    } else {
        ent = tack_hget(list, name);
    }
    return ent->id;
}

static buffer *pack_ids(tack_t *list, int type) {
    buffer *buf = calloc(1, sizeof(buffer));
    if (tack_len(list) == 0) {
        buf->data = malloc(0);
        buf->size = 0;
        return buf;
    }
    node_id *el;
    tack_foreach(list, el) {
        if (!el->name) {
            printf("%p, %d has no name!\n", el, el->id);
            continue;
        }
        // name size is aligned to word
        buf->size += sizeof(id_entry) + align4(strlen(el->name));
    }
    buf->data = calloc(1, sizeof(chunk_header) + buf->size);
    chunk_header *chunk = (chunk_header *)buf->data;
    // chunk types: words=2, rules=3, exports=4, imports=5, lists=6
    chunk->type = type;
    chunk->size = buf->size;
    buf->size += sizeof(chunk_header);

    uint8_t *pos = chunk->data;
    tack_foreach(list, el) {
        id_entry *entry = (id_entry *)pos;
        size_t len = strlen(el->name);
        // name size is word-aligned
        entry->size = sizeof(id_entry) + align4(strlen(el->name));
        entry->id = el->id;
        strcpy(entry->name, el->name);
        pos += entry->size;
    }
    return buf;
}

static buffer *pack_rules(tack_t *list, int type) {
    buffer *buf = calloc(1, sizeof(buffer));
    node_id *el;
    tack_foreach(list, el) {
        buffer *rule = el->data;
        if (rule) {
            buf->size += rule->size;
        }
    }
    buf->data = calloc(1, sizeof(chunk_header) + buf->size);
    chunk_header *chunk = (chunk_header *)buf->data;
    chunk->type = type;
    chunk->size = buf->size;
    buf->size += sizeof(chunk_header);

    uint8_t *pos = chunk->data;
    tack_foreach(list, el) {
        buffer *rule = el->data;
        if (rule) {
            memcpy(pos, rule->data, rule->size);
            pos += rule->size;
        }
    }
    return buf;
}

static buffer *buf_concat_free(buffer *first, ...) {
    size_t size = first->size;
    va_list args;
    buffer *cur;
    va_start(args, first);
    while ((cur = va_arg(args, buffer *)))
        size += cur->size;
    va_end(args);

    buffer *ret = malloc(sizeof(buffer));
    ret->data = malloc(size);
    ret->size = size;

    uintptr_t pos = (uintptr_t)ret->data;
    memcpy((void *)pos, first->data, first->size);
    pos += first->size;
    free(first);

    va_start(args, first);
    while ((cur = va_arg(args, buffer *))) {
        memcpy((void *)pos, cur->data, cur->size);
        pos += cur->size;
        free(cur);
    }
    va_end(args);
    return ret;
}

static size_t node_sizeof(Node *node) {
    size_t size = 0;
    Node *child;
    switch (node->type) {
        // these types have both start and end
        case SEQ:
        case OPT:
        case ALT:
        case REP:
            size = sizeof(rule_def) * 2;
            node_foreach(node, child) {
                size += node_sizeof(child);
            }
            break;
        default:
            size = sizeof(rule_def);
            break;
    }
    return size;
}

#define emit(typ, value) do { \
    def->type = typ;        \
    def->val = value;         \
    def++;                  \
} while (0)

#define emit_id(typ, list) do { \
    int id = get_id(list, node->name); \
    def->type = typ;            \
    def->val = id;              \
    node->id = id;              \
    def++;                      \
} while (0)

static int node_compile(Grammar *g, rule_def **pos, Node *node) {
    rule_def *def = *pos;
    // start_type
    switch (node->type) {
        case SEQ: emit(start_type, seq_val); break;
        case OPT: emit(start_type, opt_val); break;
        case ALT: emit(start_type, alt_val); break;
        case REP: emit(start_type, rep_val); break;
        default: break;
    }

    // compile body / children
    Node *child;
    switch (node->type) {
        case SEQ:
        case OPT:
        case ALT:
        case REP:
            *pos = def;
            node_foreach(node, child) {
                if (node_compile(g, pos, child)) {
                    return -1;
                }
            }
            def = *pos;
            break;
        case LITERAL:
            emit_id(word_type, &g->words);
            break;
        case RULE:
            emit_id(rule_type, &g->rules);
            // make sure g->nfa keeps up on IDs
            if (node->id >= tack_len(&g->nfa)) {
                tack_push(&g->nfa, NULL);
            }
            if (strcmp(node->name, "dgndictation") == 0) {
                node->id = 1000000;
            } else if (strcmp(node->name, "dgnwords") == 0) {
                node->id = 1000001;
            } else if (strcmp(node->name, "dgnletters") == 0) {
                node->id = 1000002;
            }
            break;
        case LIST:
            emit_id(list_type, &g->lists);
            break;
    }

    // end_type
    switch (node->type) {
        case SEQ: emit(end_type, seq_val); break;
        case OPT: emit(end_type, opt_val); break;
        case ALT: emit(end_type, alt_val); break;
        case REP: emit(end_type, rep_val); break;
        default: break;
    }
    *pos = def;
    return 0;
}

#undef emit
#undef emit_id

static int root_compile(Grammar *g, Node *root, const char *name, char **err) {
    if (!root)
        return -1;

    // add rule to grammar
    node_id *ent = tack_hget(&g->rules, name);
    if (ent == NULL) {
        ent = id_new(name, tack_len(&g->rules) + 1);
        tack_push(&g->rules, ent);
        tack_hset(&g->rules, name, ent);
    } else if (ent->data != NULL) {
        printf("warning: duplicate rule %s\n", name);
        node_free(root);
        return 0;
    }
    // node_dump(root);
    root = node_optimize(root);

    // nfa is used to triage recognized phrases
    // nfa indices MUST be consistent with rule numbers
    tack_push(&g->nfa, nfa_compile(root));

    // compile rule
    buffer *buf = ent->data = calloc(1, sizeof(buffer));
    buf->size = node_sizeof(root) + sizeof(rule_header);
    buf->data = calloc(1, buf->size);
    rule_header *header = (rule_header *)buf->data;
    header->size = buf->size;
    header->id = ent->id;
    rule_def *def = (rule_def *)(buf->data + sizeof(rule_header));
    int rc = node_compile(g, &def, root);
    if (rc) {
        free(buf->data);
        free(buf);
    }
    // nodes are referenced by NFAs, so we need a list to free later
    tack_push(&g->nodes, root);
    return rc;
}

static int rule_compile(Grammar *g, const char *rule, const char *name, char **err) {
    Node *root = grammar_parse(rule, err);
    return root_compile(g, root, name, err);
}

static int json_rule_compile(Grammar *g, json_t *rule, const char *name, char **err) {
    switch (json_typeof(rule)) {
        case JSON_ARRAY: {
            int index;
            json_t *ent;
            json_array_foreach(rule, index, ent) {
                if (json_typeof(ent) != JSON_STRING) {
                    asprintf(err, "Rule array \"%s\" contains a non-string rule.", name);
                    return -1;
                }
            }
            Node *root = node_new(ALT, NULL);
            json_array_foreach(rule, index, ent) {
                Node *node = grammar_parse(json_string_value(ent), err);
                if (!node) {
                    node_free(root);
                    return -1;
                }
                node_push(root, node);
            }
            return root_compile(g, root, name, err);
        }
        case JSON_STRING:
            return rule_compile(g, json_string_value(rule), name, err);
        default:
            asprintf(err, "Rule \"%s\" has unsupported json type. Must be a string or array of strings.", name);
            return -1;
    }
}

static char *global_imports[] = {
    "dgndictation",
    "dgnletters",
    "dgnwords",
    NULL,
};

int grammar_compile(Grammar **grammar, json_t *j, char **err) {
    int ret = 0;

    char *data = NULL, *name = NULL;
    json_t *public = NULL, *private = NULL;
    json_error_t json_err;
    if (json_unpack_ex(j, &json_err, 0, "{s:s, s:o}", "name", &name, "public", &public)) {
        *err = strdup(json_err.text);
        return -1;
    }
    json_unpack(j, "{s:o}", "private", &private);
    Grammar *g = *grammar = calloc(1, sizeof(Grammar));
    g->name = strdup(name);

    // when we see a rule ref during rule_compile:
    // 1. tack_hget(rules, "rulename") - if this exists, that's the `node_id`
    // 2. tack_hset(rules, "rulename", id_new("rulename", tack_len())); tack_push(rules, id_ent); - that's the rule_id
    // add to exports if this is an exported rule, but with the same id
    // same with a list ref
    // same with a word (dedup words)
    // warn on: duplicate rule
    // warn on: undefined rule (at end of grammar compile) - this means undefined rules need their name in the iterable list
    /*
    tack_hset(&g->exports, "rulename", id_new("rulename", tack_len(...)));
    tack_hset(&g->rules, "rulename", id_new("rulename", tack_len(...)));
    tack_hset(&g->lists, "listname", id_new("rulename", tack_len(...)));
    tack_hset(&g->words, "wordname", id_new("rulename", tack_len(...)));
    */

    // parse private rules
    const char *key;
    json_t *value;
    if (private) {
        json_object_foreach(private, key, value) {
            if (json_rule_compile(g, value, key, err)) {
                ret = -1;
                goto cleanup;
            }
        }
    }

    // parse public rules
    json_object_foreach(public, key, value) {
        if (json_rule_compile(g, value, key, err)) {
            ret = -1;
            goto cleanup;
        }
        // add to exports
        if (tack_hexists(&g->exports, key)) {
            printf("warning: skipping duplicate export of \"%s\"\n", key);
        } else {
            node_id *ent = tack_hget(&g->rules, key);
            node_id *export = id_new(NULL, ent->id);
            asprintf((char **)&export->name, "%s:%s", name, ent->name);
            tack_hset(&g->exports, key, export);
            tack_push(&g->exports, export);
            // leave a trail back so final rule compile will get the right name
            tack_hset(&g->exports, export->name, ent);
        }
    }

    // sanity check rules and autoimport
    node_id *ent;
    tack_foreach(&g->rules, ent) {
        if (ent->data == NULL) {
            // autoimport global dragon rules
            char **pos = global_imports;
            bool dgn = false;
            while (*pos) {
                if (strcmp(*pos++, ent->name) == 0) {
                    tack_push(&g->imports, ent);
                    dgn = true;
                }
            }
            if (dgn) {
                // compile a RULESTUB nfa for imports (nfa->edges = NULL, nfa->node = RULE)
                nfa *n = calloc(1, sizeof(nfa));
                n->count = 0;
                n->node = calloc(1, sizeof(Node));
                n->node->type = RULE;
                n->node->id = ent->id;
                n->node->name = strdup(ent->name);
                tack_set(&g->nfa, ent->id - 1, n);
                tack_push(&g->nodes, n->node);
            } else {
                asprintf(err, "rule referenced but not defined \"%s\"", ent->name);
                ret = -1;
                goto cleanup;
            }
        }
    }

    // now create and export one big ALT rule for all public rules
    Node *root = node_new(ALT, NULL);
    tack_foreach(&g->exports, ent) {
        // look up original rule ent for export
        ent = tack_hget(&g->exports, ent->name);
        node_push(root, node_new(RULE, strdup(ent->name)));
    }
    asprintf((char **)&g->main_rule, "%s::main", name);
    if (root_compile(g, root, g->main_rule, err)) {
        ret = -1;
        goto cleanup;
    }
    ent = tack_hget(&g->rules, g->main_rule);
    // copy ent because exports are freed separately (as they are renamed)
    ent = id_new(ent->name, ent->id);
    tack_hset(&g->exports, g->main_rule, ent);
    tack_push(&g->exports, ent);

    // pack into binary grammar blob
    grammar_header header = {.type = 0, .flags = 0};
    buffer *hdrbuf = malloc(sizeof(buffer));
    hdrbuf->data = (void *)&header;
    hdrbuf->size = sizeof(grammar_header);
    g->raw = buf_concat_free(
        hdrbuf,
        pack_ids(&g->exports, 4),
        pack_ids(&g->imports, 5),
        pack_ids(&g->lists, 6),
        pack_ids(&g->words, 2),
        pack_rules(&g->rules, 3),
        NULL);
    // printf("raw grammar: ");
    // pbuf(g->raw);

    tack_t *_;
    tack_foreach(&g->lists, _) {
        tack_push(&g->listdata, calloc(1, sizeof(tack_t)));
    }
cleanup:
    if (ret != 0) {
        grammar_free(g);
        *grammar = NULL;
    }
    return ret;
}

void grammar_free(Grammar *g) {
    if (g->raw) {
        free(g->raw->data);
        free(g->raw);
    }
    free((void *)g->main_rule);
    free((void *)g->name);
    free((void *)g->appname);

    node_id *ent;
    Node *node;
    nfa *n;
    // import id structs are reused from rules
    // tack_foreach(&g->imports, ent) id_free(ent);
    tack_foreach(&g->exports, ent) id_free(ent);
    tack_foreach(&g->rules, ent) id_free(ent);
    tack_foreach(&g->lists, ent) id_free(ent);
    tack_foreach(&g->words, ent) id_free(ent);
    tack_foreach(&g->nodes, node) node_free(node);
    tack_foreach(&g->nfa, n) nfa_free(n);

    char *word;
    tack_t *listdata;
    tack_foreach(&g->listdata, listdata) {
        tack_foreach(listdata, word) {
            free(word);
        }
        tack_clear(listdata);
    }

    tack_clear(&g->imports);
    tack_clear(&g->exports);
    tack_clear(&g->rules);
    tack_clear(&g->lists);
    tack_clear(&g->words);
    tack_clear(&g->nodes);
    tack_clear(&g->nfa);
    tack_clear(&g->listdata);

    free(g);
}
