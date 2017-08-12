#include <jansson.h>
#include "maclink.h"
#include "grammar/compile.h"
#include "grammar/nfa.h"
#include "phrase.h"
#include "server.h"

typedef struct ll {
    void *data;
    Node *node;
    struct ll *next;
} ll;

static ll *llink(ll *node, void *data) {
    ll *next = calloc(1, sizeof(ll));
    next->data = data;
    if (node)
        node->next = next;
    return next;
}

void lfree(ll *node) {
    while (node) {
        ll *next = node->next;
        free(node);
        node = next;
    }
}

#define printf(...)

static inline bool nfa_accept(Grammar *g, const nfa *n, result_node *rnode) {
    if (n->node == NULL)
        return true;

    printf("test \"%s\": ", rnode->word);
    // node_dump(n->node);
    switch (n->node->type) {
        case LITERAL:
            return rnode->id == n->node->id ||
                   (rnode->id == 0 && strcmp(rnode->word, n->node->name) == 0);
        case LIST: {
            tack_t *list = tack_get(&g->listdata, n->node->id - 1);
            return tack_hexists(list, rnode->word);
        }
        case RULE: return false;
        default: return true;
    }
}

#ifndef printf
#define printf(...) do { for (int i = 0; i < level; i++) printf(" "); printf(__VA_ARGS__); } while (0)
#endif

// returns ll and length of longest path in terms of [nfa *] -> [nfa *] -> ...
static int nfa_walk(Grammar *g, Node *rule_node, const nfa *n, ll *rnode, ll **rupdate, ll **path, ll **end, int level, int rule_level, bool *complete, char **rule_name) {
    int add = 0;
    // nfa_dump(n, level);
    if (nfa_accept(g, n, rnode->data)) {
        if (n->node != NULL) {
            printf("accepted %s\n", ((result_node *)rnode->data)->word);
            add = 1;
            *end = llink(*end, (void *)n);
            (*end)->node = rule_node;
            if (!*path)
                *path = *end;
            rnode = rnode->next;
            *rupdate = rnode;
        }
    } else if (n->node && n->node->type == RULE) {
        Node *attributed_rule = rule_node;
        if (rule_level < 2) {
            attributed_rule = n->node;
        }
        printf("rule recurse: <%s>\n", n->node->name);
        // recurse into rule, chaining back to this nfa node
        // unless it's an imported rule, in which case we match by rule num
        // (consume rnode until rule num changes)
        int id = n->node->id;
        // handle imports
        if (id > tack_len(&g->rules)) {
            node_id *nid = tack_hget(&g->rules, n->node->name);
            if (!nid) {
                printf("FATAL: rule <%s> referenced in nfa but not defined\n", n->node->name);
                return add;
            }
            id = nid->id;
        }
        const nfa *rule = tack_get(&g->nfa, id - 1);
        if (rule->count == 0) {
            // no edges = import - we just need to check for <rule>+ based on the rule number instead
            printf("%d %d\n", ((result_node *)rnode->data)->rule, n->node->id);
            while (rnode && ((result_node *)rnode->data)->rule == n->node->id) {
                *end = llink(*end, (void *)rule);
                (*end)->node = attributed_rule;
                if (!*path)
                    *path = *end;
                add++;
                rnode = rnode->next;
            }
            printf("sim rule <%s> matched %d words\n", n->node->name, add);
            // reject if no words matched rule
            if (!add) return add;
        } else {
            bool rule_complete = false;
            add = nfa_walk(g, attributed_rule, rule, rnode, &rnode, path, end, level + 1, rule_level + 1, &rule_complete, rule_name);
            if (!rule_complete) {
                printf("subrule <%s> incomplete: rejecting\n", n->node->name);
                return add;
            } else if (rule_level == 0) {
                *rule_name = n->node->name;
            }
            printf("rule <%s> recursion matched %d words\n", n->node->name, add);
        }
    } else { // reject if no accepts
        printf("no accepts: rejecting\n");
        *complete = false;
        return add;
    }
    // no more words: terminate this path
    if (!rnode) {
        if (n->count == 0) *complete = true;
        for (int i = 0; i < n->count; i++) {
            if (n->edges[i] == NULL) {
                *complete = true;
                break;
            }
        }
        printf("no more words: terminating %d\n", n->count);
        return add;
    }
    ll *best_path, *best_end, *best_rnode;
    int longest = 0;
    bool best_complete = false, zerocomplete = false;
    for (int i = 0; i < n->count; i++) {
        if (n->edges[i] == NULL) {
            zerocomplete = true;
            continue;
        }
        ll *child_rnode = NULL, *child_path = NULL, *child_pathend = NULL;
        bool child_complete = false;
        const nfa *edge = n->edges[i];
        printf("taking edge %d\n", i);
        int len = nfa_walk(g, rule_node, n->edges[i], rnode, &child_rnode, &child_path, &child_pathend, level + 1, rule_level, &child_complete, rule_name);
        printf("took edge %d: %d  complete=%d\n", i, len, child_complete);
        if (len > longest && ((!best_complete && !zerocomplete) || child_complete)) {
            best_path = child_path;
            best_end = child_pathend;
            best_rnode = child_rnode;
            best_complete = child_complete;
            longest = len;
        } else {
            lfree(child_path);
        }
    }
    if (longest && (best_complete || !zerocomplete)) {
        if (*end == NULL) {
            *path = best_path;
        } else {
            (*end)->next = best_path;
        }
        *end = best_end;
        *rupdate = best_rnode;
        *complete = best_complete;
    } else if (zerocomplete) {
        *complete = true;
    }
    printf("complete: %d\n", *complete);
    return longest + add;
}

#undef printf

static int nfa_match(Grammar *g, ll *rnode, char **rule_name) {
    // main rule should be last nfa entry, but let's be safe and look it up
    node_id *id = tack_hget(&g->rules, g->main_rule);
    const nfa *main = tack_get(&g->nfa, id->id - 1);

    ll *path = NULL, *end = NULL, *rend = NULL;
    bool complete = false;
    int len = nfa_walk(g, main->node, main, rnode, &rend, &path, &end, 0, 0, &complete, rule_name);
    ll *pos = path;
    while (pos && rnode) {
        nfa *n = pos->data;
        result_node *rn = rnode->data;
        const char *name = g->main_rule;
        if (pos->node != NULL) {
            name = pos->node->key ? pos->node->key : pos->node->name;
        }
        rn->rule_name = name;
        pos = pos->next;
        rnode = rnode->next;
    }
    lfree(path);
    return len;
}

json_t *phrase_to_json(char *phrase) {
    uint32_t len = *(uint32_t *)phrase;
    char *end = phrase + len;
    char *pos = phrase + 4;

    json_t *array = json_array();
    while (pos < end) {
        id_entry *ent = (id_entry *)pos;
        json_array_append_new(array, json_string(ent->name));
        pos += ent->size;
    }
    return array;
}

// TODO: each rule group should contain all subrules' words too
// and top rule should always match the phrase
// TODO: don't expose a group for top rule (that's just phrase)
// TODO: only show extras for the matched rule
// TODO: support node keys
json_t *result_to_json(Grammar *g, dsx_result *result) {
    uint32_t paths[1];
    size_t needed = 0;
    json_t *obj = json_object();
    json_t *groups = json_object();
    int rc = _DSXResult_BestPathWord(result, 0, paths, 1, &needed);
    if (rc == 33) {
        ll *rnodes = NULL, *rend = NULL;
        int rcount = 0;
        uint32_t *paths = calloc(1, needed);
        rc = _DSXResult_BestPathWord(result, 0, paths, needed, &needed);
        if (rc == 0) {
            dsx_word_node node;
            // get the rule number and cfg node information for each word
            for (uint32_t i = 0; i < needed / sizeof(uint32_t); i++) {
                result_node *rnode = calloc(1, sizeof(result_node));
                rc = _DSXResult_GetWordNode(result, paths[i], &node, &rnode->id, &rnode->word);
                rnode->rule = node.rule;
                rend = llink(rend, rnode);
                if (!rnodes) rnodes = rend;
                rcount++;
            }
        }
        free(paths);

        // apply recognized word node list to grammar's nfa
        char *rule_name = NULL;
        int len = nfa_match(g, rnodes, &rule_name);
        // printf("nfa match: %s %d == %d?\n", rule_name, len, rcount);

        json_object_set_new(obj, "rule", json_string(rule_name));
        while (rnodes) {
            ll *next = rnodes->next;
            result_node *rn = rnodes->data;

            if (rn->rule_name != rule_name) {
                json_t *array = json_object_get(groups, rn->rule_name);
                if (!array) {
                    array = json_array();
                    json_object_set_new(groups, rn->rule_name, array);
                }
                json_array_append_new(array, json_string(rn->word));
            }
            free(rnodes->data);
            free(rnodes);
            rnodes = next;
        }
    }
    if (json_object_size(groups) > 0) {
        json_object_set_new(obj, "groups", groups);
    } else {
        json_decref(groups);
    }
    return obj;
}

// TODO: phrase_hypothesis is wrapped into this atm
// TODO: reject end phrases not destined for our grammar
// TODO: expose word timings
int phrase_publish(Grammar *g, dsx_end_phrase *endphrase, const char *cmd) {
    json_t *obj;
    obj = result_to_json(g, endphrase->result);
    json_object_set_new(obj, "cmd", json_string(cmd));
    json_object_set_new(obj, "phrase", phrase_to_json(endphrase->phrase));

    char *str = json_dumps(obj, 0);
    json_decref(obj);

    _DSXResult_Destroy(endphrase->result);
    printf("-> %s\n", str);
    maclink_publish(cmd, str);
    free(str);
    return 0;
}

int phrase_end(Grammar *g, dsx_end_phrase *endphrase) {
    return phrase_publish(g, endphrase, "p.end");
}

int phrase_hypothesis(Grammar *g, dsx_end_phrase *endphrase) {
    return phrase_publish(g, endphrase, "p.hypothesis");
}

void phrase_begin(void *user) {
    maclink_publish("p.begin", "{\"cmd\": \"p.begin\"}");
}
