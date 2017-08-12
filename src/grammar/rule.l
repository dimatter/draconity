%{
#include <stdio.h>
#include "node.h"
#include "parser.h"
%}
%%
[A-Za-z0-9_\-']+ yylval.string = strdup(yytext); return WORD;
[()|{}<>[\]*+:]  return yytext[0]; /* tokens */
[ \t]+           /* whitespace */
.                return UNKNOWN;
%%
