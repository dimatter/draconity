%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "node.h"
#include "parser.h"

typedef struct yy_buffer_state * YY_BUFFER_STATE;
extern int yyparse();
extern int yylex();
extern YY_BUFFER_STATE yy_scan_string(const char * str);
extern void yy_delete_buffer(YY_BUFFER_STATE buffer);

void yyerror(Node **root, const char **err, const char *str) {
    *err = strdup(str);
}

int yywrap() {
    return 1;
}

Node *grammar_parse(const char *text, const char **err) {
    YY_BUFFER_STATE buf = yy_scan_string(text);
    Node *root = NULL;
    yyparse(&root, err);
    yy_delete_buffer(buf);
    return root;
}

%}

%error-verbose
%parse-param {Node **root}
%parse-param {const char **err}

%union  {
    char c;
    char *string;
    Node *node;
}
%token ':' '(' ')' '|' '<' '>' '[' ']' '*' '+' WORD UNKNOWN
%type <node> Sequence Statement Term Optional Group RuleName ListName Alternates Literal
%type <c> Repetition '*' '+'
%type <string> WORD
%start Root

%nonassoc WORD
%nonassoc '<'

%%

Root: Sequence { *root = $1; }

Sequence: Statement {
    $$ = node_new(SEQ, NULL);
    node_push($$, $1);
} | Sequence Statement {
    node_push($1, $2);
    $$ = $1;
}

Statement: Term {
    $$ = $1;
} | Term Repetition {
    if ($2 == '*') {
        Node *opt = node_new(OPT, NULL);
        node_push(opt, $1);
        $1 = opt;
    }
    Node *rep = node_new(REP, NULL);
    node_push(rep, $1);
    $$ = rep;
}
Term: Optional | Group | RuleName | ListName | Literal

Optional: '[' Sequence ']' {
    $$ = $2;
    $$->type = OPT;
}
Group: '(' Alternates ')' { $$ = $2; }
Alternates: Sequence {
    $$ = node_new(ALT, NULL);
    node_push($$, $1);
} | Alternates '|' Sequence {
    node_push($1, $3);
}

Literal: WORD {
    $$ = node_new(LITERAL, $1);
} | WORD ':' Literal {
    char *tmp = calloc(1, strlen($1) + 1 + strlen($3->name) + 1);
    strcat(tmp, $1);
    strcat(tmp, ":");
    strcat(tmp, $3->name);
    free($1);
    free($3->name);
    $3->name = tmp;
    $$ = $3;
}
RuleName: '<' WORD '>' {
    $$ = node_new(RULE, $2);
} | WORD ':' RuleName {
    $$ = $3;
    $$->type = RULE;
    $$->key = $1;
}
ListName: '{' WORD '}' {
    $$ = node_new(LIST, $2);
} | WORD ':' ListName {
    $$ = $3;
    $$->type = LIST;
    $$->key = $1;
}

Repetition: '*' | '+'
