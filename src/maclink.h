#ifndef MACLINK_H
#define MACLINK_H

#include <czmq.h>
#include <stdint.h>
#include "tack.h"

#ifdef VERBOSE
#define dprintf(...) printf(__VA_ARGS__);
#else
#define dprintf(...)
#endif

typedef struct {
    void *data;
    uint32_t size;
} dsx_dataptr;

typedef struct {
    uint32_t var1;
    uint32_t var2;
    uint32_t var3;
    uint32_t var4;
    uint32_t var5;
    uint32_t var6;
    uint64_t start_time;
    uint64_t end_time;
    uint32_t var9;
    uint32_t var10;
    uint32_t var11;
    uint32_t var12;
    uint32_t rule;
    uint32_t var14;
} __attribute__((packed)) dsx_word_node;

typedef struct {} drg_grammar;
typedef struct {} drg_engine;
typedef struct {} dsx_result;

typedef struct {
    void *var0;
    unsigned int var1;
    unsigned int flags;
    uint64_t var3;
    uint64_t var4;
    char *phrase;
    dsx_result *result;
    void *var7;
} dsx_end_phrase;

extern drg_engine *_engine;

extern drg_engine *(*_DSXEngine_New)();
extern int (*_DSXEngine_LoadGrammar)(drg_engine *engine, int type, dsx_dataptr *data, drg_grammar **grammar_out);
extern int (*_DSXGrammar_Activate)(drg_grammar *grammar, uint64_t unk1, bool unk2, const char *main_rule);
extern int (*_DSXGrammar_Deactivate)(drg_grammar *grammar, uint64_t unk1, const char *main_rule);
extern int (*_DSXGrammar_Destroy)(drg_grammar *grammar);
extern int (*_DSXGrammar_RegisterBeginPhraseCallback)(drg_grammar *grammar, void *cb, void *user, unsigned int *key);
extern int (*_DSXGrammar_RegisterEndPhraseCallback)(drg_grammar *grammar, void *cb, void *user, unsigned int *key);
extern int (*_DSXGrammar_RegisterPhraseHypothesisCallback)(drg_grammar *grammar, void *cb, void *user, unsigned int *key);
extern void *(*_DSXGrammar_SetSpecialGrammar)();
extern int (*_DSXGrammar_SetApplicationName)(drg_grammar *grammar, const char *name);
extern int (*_DSXGrammar_Unregister)(drg_grammar *grammar, unsigned int key);
extern int (*_DSXResult_BestPathWord)(dsx_result *result, int choice, uint32_t *path, size_t pathSize, size_t *needed);
extern int (*_DSXResult_GetWordNode)(dsx_result *result, uint32_t path, void *node, uint32_t *num, char **name);
extern int (*_DSXResult_Destroy)(dsx_result *result);
extern int (*_DSXGrammar_SetPriority)(drg_grammar *grammar, int priority);
extern int (*_DSXGrammar_SetList)(drg_grammar *grammar, const char *name, dsx_dataptr *data);
extern int (*_DSXGrammar_GetList)(drg_grammar *grammar, const char *name, dsx_dataptr *data);

struct state {
    zsock_t *pubsock;
    zsock_t *cmdsock;
    pthread_t tid;
    pthread_mutex_t publock;

    tack_t grammars;
};

#endif
