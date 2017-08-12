#include <dlfcn.h>
#include <jansson.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "server.h"
#include "maclink.h"

static void *server_so = NULL;
drg_engine *_engine = NULL;
drg_engine *(*_DSXEngine_New)();
int (*_DSXEngine_LoadGrammar)(drg_engine *engine, int type, dsx_dataptr *data, drg_grammar **grammar_out);
int (*_DSXGrammar_Activate)(drg_grammar *grammar, uint64_t unk1, bool unk2, const char *main_rule);
int (*_DSXGrammar_Deactivate)(drg_grammar *grammar, uint64_t unk1, const char *main_rule);
int (*_DSXGrammar_Destroy)(drg_grammar *);
int (*_DSXGrammar_RegisterBeginPhraseCallback)(drg_grammar *grammar, void *cb, void *user, unsigned int *key);
int (*_DSXGrammar_RegisterEndPhraseCallback)(drg_grammar *grammar, void *cb, void *user, unsigned int *key);
int (*_DSXGrammar_RegisterPhraseHypothesisCallback)(drg_grammar *grammar, void *cb, void *user, unsigned int *key);
void *(*_DSXGrammar_SetSpecialGrammar)();
int (*_DSXGrammar_SetApplicationName)(drg_grammar *grammar, const char *name);
int (*_DSXGrammar_Unregister)(drg_grammar *grammar, unsigned int key);
int (*_DSXResult_BestPathWord)(dsx_result *result, int choice, uint32_t *path, size_t pathSize, size_t *needed);
int (*_DSXResult_GetWordNode)(dsx_result *result, uint32_t path, void *node, uint32_t *num, char **name);
int (*_DSXResult_Destroy)(dsx_result *result);
int (*_DSXGrammar_SetPriority)(drg_grammar *grammar, int priority);
int (*_DSXGrammar_SetList)(drg_grammar *grammar, const char *name, dsx_dataptr *data);
int (*_DSXGrammar_GetList)(drg_grammar *grammar, const char *name, dsx_dataptr *data);

__attribute__((constructor))
static void cons() {
    printf("[+] maclink init\n");
    server_so = dlopen("server.so", RTLD_LOCAL | RTLD_LAZY);
    dprintf("%46s %p\n", "server.so", server_so);
    #define load(x) do { _##x = dlsym(server_so, #x); dprintf("%46s %p\n", #x, _##x); } while (0)
    load(DSXEngine_New);
    load(DSXEngine_LoadGrammar);
    load(DSXGrammar_Activate);
    load(DSXGrammar_Deactivate);
    load(DSXGrammar_Destroy);
    load(DSXGrammar_GetList);
    load(DSXGrammar_RegisterBeginPhraseCallback);
    load(DSXGrammar_RegisterEndPhraseCallback);
    load(DSXGrammar_RegisterPhraseHypothesisCallback);
    load(DSXGrammar_SetApplicationName);
    load(DSXGrammar_SetList);
    load(DSXGrammar_SetPriority);
    load(DSXGrammar_SetSpecialGrammar);
    load(DSXGrammar_Unregister);
    load(DSXResult_BestPathWord);
    load(DSXResult_GetWordNode);
    load(DSXResult_Destroy);

    maclink_init();
    printf("[+] maclink attached\n");
}

void *DSXEngine_New() {
    _engine = _DSXEngine_New();
    printf("DSXEngine_New() = %p\n", _engine);
    return _engine;
}

// TODO: remove this?
int DSXGrammar_SetApplicationName(drg_grammar *grammar, const char *name) {
    return _DSXGrammar_SetApplicationName(grammar, name);
}

int DSXEngine_LoadGrammar(void *engine, int format, void *unk, drg_grammar **grammar) {
    return _DSXEngine_LoadGrammar(engine, format, unk, grammar);
}

// TODO: track which dragon grammars are active, so "dragon" pseudogrammar can activate them
int DSXGrammar_Activate(drg_grammar *grammar, uint64_t unk1, bool unk2, char *name) {
    // printf("hooked DSXGrammar_Activate(%p, %lld, %d, %s)\n", grammar, unk1, unk2, name);
    return 0; // return _DSXGrammar_Activate(grammar, unk1, unk2, name);
}

int DSXGrammar_RegisterEndPhraseCallback(drg_grammar *grammar, void *cb, void *user, unsigned int *key) {
    *key = 0;
    return 0;
    // return _DSXGrammar_RegisterEndPhraseCallback(grammar, cb, user, key);
}

int DSXGrammar_RegisterBeginPhraseCallback(drg_grammar *grammar, void *cb, void *user, unsigned int *key) {
    *key = 0;
    return 0;
    // return _DSXGrammar_RegisterBeginPhraseCallback(grammar, cb, user, key);
}

int DSXGrammar_RegisterPhraseHypothesisCallback(drg_grammar *grammar, void *cb, void *user, unsigned int *key) {
    *key = 0;
    return 0;
    // return _DSXGrammar_RegisterPhraseHypothesisCallback(grammar, cb, user, key);
}

int DSXGrammar_Unregister(drg_grammar *grammar, unsigned int key) {
    return 0;
}