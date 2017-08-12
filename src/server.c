#include <czmq.h>
#include <jansson.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>

#include "phrase.h"
#include "server.h"
#include "maclink.h"
#include "tack.h"
#include "grammar/compile.h"

// #define NODRAGON

#ifndef streq
#define streq(a, b) !strcmp(a, b)
#endif

struct state state = {0};

void maclink_publish(const char *topic, const char *msg) {
    pthread_mutex_lock(&state.publock);
    zstr_sendx(state.pubsock, topic, msg, NULL);
    pthread_mutex_unlock(&state.publock);
}

static json_t *start_resp(bool success) {
    json_t *obj = json_object();
    json_object_set_new(obj, "success", json_boolean(success));
    return obj;
}

static void zjson_send(zsock_t *sock, json_t *obj) {
    char *str = json_dumps(obj, 0);
    zstr_send(sock, str);
    free(str);
}

static void zjson_send_decref(zsock_t *sock, json_t *obj) {
    zjson_send(sock, obj);
    json_decref(obj);
}

static void zjson_senderr(zsock_t *sock, const char *msg) {
    json_t *obj = start_resp(false);
    json_object_set_new(obj, "error", json_string(msg));
    zjson_send_decref(sock, obj);
}

static void handle(const char *msg) {
    json_error_t err;
    json_t *j = json_loads(msg, 0, &err);
    if (j == NULL) return zjson_senderr(state.cmdsock, err.text);

    // look up the grammar object if json "name" is set
    char *name;
    int name_err = json_unpack_ex(j, &err, 0, "{s:s}", "name", &name);
    Grammar *grammar = NULL;
    if (name_err == 0) {
        grammar = tack_hget(&state.grammars, name);
    }
    int priority = 0;
    int set_priority = !json_unpack(j, "{s:i}", "priority", &priority);

    char *cmd;
    if (json_unpack_ex(j, &err, 0, "{s:s}", "cmd", &cmd)) {
        zjson_senderr(state.cmdsock, err.text);
        goto end;
    }
    if (streq(cmd, "g.enable")) {
        if (!grammar) goto no_grammar;
        if (grammar->active) {
            zjson_senderr(state.cmdsock, "grammar already enabled");
            goto end;
        }
        if (_DSXGrammar_Activate(grammar->handle, 0, false, grammar->main_rule)) {
            zjson_senderr(state.cmdsock, "error activating grammar");
            goto end;
        }
        if (_DSXGrammar_RegisterEndPhraseCallback(grammar->handle, phrase_end, grammar, &grammar->endkey)) {
            zjson_senderr(state.cmdsock, "error registering end phrase callback");
            goto end;
        }
        if (_DSXGrammar_RegisterPhraseHypothesisCallback(grammar->handle, phrase_hypothesis, grammar, &grammar->hypokey)) {
            zjson_senderr(state.cmdsock, "error registering phrase hypothesis callback");
            goto end;
        }
        if (_DSXGrammar_RegisterBeginPhraseCallback(grammar->handle, phrase_begin, grammar, &grammar->beginkey)) {
            zjson_senderr(state.cmdsock, "error registering begin phrase callback");
            goto end;
        }
        grammar->active = true;
        if (set_priority) {
            grammar->priority = priority;
            _DSXGrammar_SetPriority(grammar->handle, priority);
        }
        zjson_send_decref(state.cmdsock, start_resp(true));
    } else if (streq(cmd, "g.disable")) {
        if (!grammar) goto no_grammar;
        if (_DSXGrammar_Deactivate(grammar->handle, 0, grammar->main_rule)) {
            zjson_senderr(state.cmdsock, "error deactivating grammar");
            goto end;
        }
        grammar->active = false;
        if (_DSXGrammar_Unregister(grammar->handle, grammar->endkey)) {
            zjson_senderr(state.cmdsock, "error unregistering handler");
            goto end;
        }
        if (_DSXGrammar_Unregister(grammar->handle, grammar->hypokey)) {
            zjson_senderr(state.cmdsock, "error unregistering handler");
            goto end;
        }
        if (_DSXGrammar_Unregister(grammar->handle, grammar->beginkey)) {
            zjson_senderr(state.cmdsock, "error unregistering handler");
            goto end;
        }
        zjson_send_decref(state.cmdsock, start_resp(true));
    } else if (streq(cmd, "g.list.set")) {
        if (!grammar) goto no_grammar;
        const char *list;
        json_t *items;
        if (json_unpack_ex(j, &err, 0, "{s:s, s:o}", "list", &list, "items", &items)) {
            zjson_senderr(state.cmdsock, err.text);
            goto end;
        }
        if (json_typeof(items) != JSON_ARRAY) {
            zjson_senderr(state.cmdsock, "items must be an array");
            goto end;
        }
        node_id *listid = tack_hget(&grammar->lists, list);
        if (!tack_hexists(&grammar->lists, list)) {
            zjson_senderr(state.cmdsock, "list does not exist in grammar");
            goto end;
        }
        tack_t *listdata = tack_get(&grammar->listdata, listid->id - 1);

        // get size of the new list's data block
        dsx_dataptr dp = {.data = NULL, .size = 0};
        int index;
        json_t *jword;
        json_array_foreach(items, index, jword) {
            const char *word = json_string_value(jword);
            dp.size += sizeof(id_entry) + align4(strlen(word));
        }
        dp.data = calloc(1, dp.size);
        uint8_t *pos = (uint8_t *)dp.data;
        json_array_foreach(items, index, jword) {
            const char *word = json_string_value(jword);
            id_entry *ent = (id_entry *)pos;
            ent->size = sizeof(id_entry) + align4(strlen(word));
            strcpy(ent->name, word);
            pos += ent->size;

            char *dataword = strdup(word);
            tack_push(listdata, dataword);
            tack_hset(listdata, dataword, NULL);
        }
        if (_DSXGrammar_SetList(grammar->handle, list, &dp)) {
            zjson_senderr(state.cmdsock, "error setting list");
        } else {
            zjson_send_decref(state.cmdsock, start_resp(true));
        }
        free(dp.data);
    } else if (streq(cmd, "g.list.get")) {
        if (!grammar) goto no_grammar;
        const char *list;
        if (json_unpack_ex(j, &err, 0, "{s:s}", "list", &list)) {
            zjson_senderr(state.cmdsock, err.text);
            goto end;
        }
        if (!tack_hexists(&grammar->lists, list)) {
            zjson_senderr(state.cmdsock, "list does not exist");
            goto end;
        }
        dsx_dataptr dp = {0};
        if (_DSXGrammar_GetList(grammar->handle, list, &dp)) {
            zjson_senderr(state.cmdsock, "error getting list");
        } else {
            json_t *array = json_array();
            uintptr_t pos = (uintptr_t)dp.data;
            while (pos < (uintptr_t)dp.data + dp.size) {
                id_entry *ent = (id_entry *)pos;
                json_array_append_new(array, json_string(ent->name));
                pos += ent->size;
            }
            json_t *resp = start_resp(true);
            json_object_set_new(resp, "items", array);
            zjson_send_decref(state.cmdsock, resp);
        }
    } else if (streq(cmd, "g.unload")) {
        if (!grammar) goto no_grammar;
        int rc = _DSXGrammar_Destroy(grammar->handle);
        printf("grammar destroy: %d\n", rc);
        tack_hdel(&state.grammars, name);
        tack_remove(&state.grammars, grammar);
        grammar_free(grammar);
        zjson_send_decref(state.cmdsock, start_resp(true));
    } else if (streq(cmd, "g.show")) {
        json_t *resp = start_resp(true);
        json_t *grammars = json_array();
        tack_foreach(&state.grammars, grammar) {
            json_t *obj = json_pack(
                "{s:s, s:b}",
                "name", grammar->name,
                "active", grammar->active);
            json_array_append_new(grammars, obj);
        }
        json_object_set_new(resp, "grammars", grammars);
        zjson_send_decref(state.cmdsock, resp);
    } else if (streq(cmd, "g.load")) {
        if (grammar) {
            zjson_senderr(state.cmdsock, "grammar by this name already exists");
            goto end;
        }
#ifndef NODRAGON
        if (_engine) {
#else
        if (true) {
#endif
            char *err;
            if (grammar_compile(&grammar, j, &err)) {
                zjson_senderr(state.cmdsock, err);
                free(err);
                goto end;
            }
#ifdef NODRAGON
            zjson_send_decref(state.cmdsock, start_resp(true));
            goto end;
#endif
            dsx_dataptr sd = {.data = grammar->raw->data, .size = grammar->raw->size};
            int ret = _DSXEngine_LoadGrammar(_engine, 1 /*cfg*/, &sd, &grammar->handle);
            if (ret > 0) {
                char *err;
                asprintf(&err, "error loading grammar: %d", ret);
                zjson_senderr(state.cmdsock, err);
                free(err);
                grammar_free(grammar);
                goto end;
            }
            if (set_priority) {
                grammar->priority = priority;
                _DSXGrammar_SetPriority(grammar->handle, priority);
            }
            tack_push(&state.grammars, grammar);
            tack_hset(&state.grammars, grammar->name, grammar);
            // printf("%d\n", _DSXGrammar_SetApplicationName(grammar->handle, grammar->name));
            zjson_send_decref(state.cmdsock, start_resp(true));
        } else {
            zjson_senderr(state.cmdsock, "engine not loaded");
        }
    } else {
        zjson_senderr(state.cmdsock, "unsupported command");
    }
end:
    json_decref(j);
    return;

no_grammar:
    zjson_senderr(state.cmdsock, "grammar not found");
    goto end;
}

static void *maclink_thread(void *user) {
    printf("[-] cmd thread launched\n");
    while (1) {
        char *msg = zstr_recv(state.cmdsock);
        if (msg != NULL) {
            printf("<- %s\n", msg);
            handle(msg);
            maclink_publish("cmd", msg);
            zstr_free(&msg);
        }
    }
}

void maclink_init() {
    state.pubsock = zsock_new_pub("ipc:///tmp/ml_pub");
    state.cmdsock = zsock_new_rep("ipc:///tmp/ml_cmd");
    if (state.pubsock == NULL || state.cmdsock == NULL) {
        printf("zmq init failed\n");
        exit(1);
    }
#ifdef NODRAGON
    maclink_thread(NULL);
    exit(0);
#endif

    int rc = pthread_create(&state.tid, NULL, maclink_thread, NULL);
    if (rc) {
        printf("thread creation failed\n");
        exit(1);
    }
}

__attribute__((destructor))
static void maclink_shutdown() {
    pthread_exit(NULL);
}
