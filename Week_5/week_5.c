/*
 * Week 5: Symbol Table with Nested Scope Management
 * ---------------------------------------------------
 * Combines:
 *   Phase 1 - Lexical Analysis   (Week 1)
 *   Phase 2 - Syntax Analysis    (Week 2/3)
 *   Phase 3 - Syntax Error Detection (Week 3)
 *   Phase 4 - Symbol Table with Nested Block Scopes (Week 5 - main feature)
 *   Phase 5 - Interactive Symbol Table Lookup
 *
 * Symbol Table Design (linked-list of scope nodes):
 *   When '{' is processed:
 *       i)   S = top
 *       ii)  top = new ST node
 *       iii) top.next = S   (link new node to previous)
 *   When '}' is processed:
 *       i)   save = top.next
 *       ii)  delete top
 *       iii) top = save
 *   Variable lookup walks up the scope chain (top -> top.next -> ...)
 *
 * Each symbol entry stores: name, type, scope level, memory offset.
 *
 * Usage:
 *   ./week_5 eval.txt        (read from file)
 *   ./week_5                 (read from stdin)
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ========================================================================
 *  PART 1 — TOKEN DEFINITIONS
 * ======================================================================== */

typedef enum {
    TOK_INT, TOK_FLOAT, TOK_VOID, TOK_IF, TOK_ELSE, TOK_WHILE, TOK_PRINT,
    TOK_BOOL, TOK_CLASS, TOK_RETURN,
    TOK_LPAREN, TOK_RPAREN, TOK_LBRACE, TOK_RBRACE,
    TOK_ID, TOK_NUM,
    TOK_ASSIGN, TOK_PLUS, TOK_MINUS, TOK_MUL, TOK_DIV, TOK_MOD,
    TOK_LT, TOK_GT, TOK_LE, TOK_GE, TOK_EQ, TOK_NEQ,
    TOK_AND, TOK_OR, TOK_NOT,
    TOK_SEMI, TOK_COMMA, TOK_COLON, TOK_EOF, TOK_ERROR
} TokenType;

typedef struct {
    TokenType type;
    char lexeme[64];
    int  line;
} Token;

/* ========================================================================
 *  PART 2 — SYMBOL TABLE STRUCTURES
 * ======================================================================== */

#define MAX_SYMBOLS 128   /* max symbols per scope node */

typedef struct {
    char name[64];
    char type[16];        /* "int" or "float" */
    int  scope_level;
    int  offset;          /* memory offset in bytes */
} Symbol;

typedef struct ScopeNode {
    Symbol          symbols[MAX_SYMBOLS];
    int             count;
    int             scope_level;
    struct ScopeNode *next;   /* points to enclosing (parent) scope */
} ScopeNode;

/* ========================================================================
 *  GLOBALS
 * ======================================================================== */

Token  current_token;
char  *input_ptr;
int    current_line = 1;

/* Symbol-table state */
ScopeNode *top           = NULL;   /* current scope node */
int        scope_counter = 0;      /* monotonic scope id */
int        global_offset = 0;      /* next available memory offset */

/* ========================================================================
 *  PART 3 — SYMBOL TABLE OPERATIONS
 * ======================================================================== */

/* Create a new scope node and push it on top of the scope stack */
void push_scope(void) {
    ScopeNode *S  = top;                         /* i)  S = top */
    ScopeNode *nn = (ScopeNode*)malloc(sizeof(ScopeNode));
    nn->count       = 0;
    nn->scope_level = scope_counter++;
    nn->next        = S;                         /* iii) top.next = S */
    top = nn;                                    /* ii)  top = new ST node */
    printf("  [ST] >>> Entered new scope (level %d)\n", nn->scope_level);
}

/* Pop the current scope node */
void pop_scope(void) {
    if (!top) return;
    printf("  [ST] <<< Leaving scope (level %d) -- dropping %d symbol(s):\n",
           top->scope_level, top->count);
    for (int i = 0; i < top->count; i++) {
        printf("        - %s  (%s, offset=%d)\n",
               top->symbols[i].name,
               top->symbols[i].type,
               top->symbols[i].offset);
    }
    ScopeNode *save = top->next;                /* i)   save = top.next */
    free(top);                                  /* ii)  delete top */
    top = save;                                 /* iii) top = save */
}

/* Insert a symbol into the current (top) scope */
void insert_symbol(const char *name, const char *type) {
    if (!top) {
        printf("  [ST] ERROR: No active scope to insert '%s'\n", name);
        return;
    }
    /* Check for duplicate in the *current* scope only */
    for (int i = 0; i < top->count; i++) {
        if (strcmp(top->symbols[i].name, name) == 0) {
            printf("  [ST] WARNING: '%s' already declared in scope %d\n",
                   name, top->scope_level);
            return;
        }
    }
    if (top->count >= MAX_SYMBOLS) {
        printf("  [ST] ERROR: Scope %d symbol table full!\n", top->scope_level);
        return;
    }

    int size = (strcmp(type, "float") == 0) ? 4 : 4;  /* both 4 bytes */

    Symbol *sym  = &top->symbols[top->count++];
    strncpy(sym->name, name, 63); sym->name[63] = '\0';
    strncpy(sym->type, type, 15); sym->type[15] = '\0';
    sym->scope_level = top->scope_level;
    sym->offset      = global_offset;
    global_offset   += size;

    printf("  [ST] INSERT: name=%-8s type=%-6s scope=%-3d offset=%d\n",
           sym->name, sym->type, sym->scope_level, sym->offset);
}

/*
 * searchSym(S, var) — search for 'var' starting from scope S and walking
 * up through enclosing scopes.  Returns 1 if found (and prints info),
 * or -1 if not found.
 *
 * Because the search starts at the innermost scope and walks outward,
 * if the same variable name is declared in both an inner and outer scope,
 * the INNER scope's entry is found first — this correctly models
 * variable SHADOWING (inner declaration hides outer one).
 */
int searchSym(ScopeNode *S, const char *var) {
    ScopeNode *cur = S;
    while (cur) {
        for (int i = 0; i < cur->count; i++) {
            if (strcmp(cur->symbols[i].name, var) == 0) {
                printf("  [ST] LOOKUP '%s': FOUND in scope %d -- type=%s, offset=%d\n",
                       var, cur->scope_level,
                       cur->symbols[i].type,
                       cur->symbols[i].offset);
                return 1;   /* success */
            }
        }
        cur = cur->next;   /* move to enclosing scope */
    }
    printf("  [ST] LOOKUP '%s': NOT FOUND (undeclared variable)\n", var);
    return -1;             /* failure */
}

/* Print every scope from top downward (for debugging / demo) */
void print_all_scopes(void) {
    printf("\n  +====================================================+\n");
    printf("  |        CURRENT SYMBOL TABLE SNAPSHOT              |\n");
    printf("  +====================================================+\n");
    ScopeNode *cur = top;
    if (!cur) {
        printf("  |  (empty -- no active scopes)                     |\n");
    }
    while (cur) {
        printf("  |  Scope Level %d  (%d symbols)                     \n",
               cur->scope_level, cur->count);
        printf("  |  %-10s %-8s %-6s %-8s\n", "Name", "Type", "Scope", "Offset");
        printf("  |  %-10s %-8s %-6s %-8s\n", "--------", "------", "-----", "------");
        for (int i = 0; i < cur->count; i++) {
            printf("  |  %-10s %-8s %-6d %-8d\n",
                   cur->symbols[i].name,
                   cur->symbols[i].type,
                   cur->symbols[i].scope_level,
                   cur->symbols[i].offset);
        }
        cur = cur->next;
    }
    printf("  +====================================================+\n\n");
}

/* ========================================================================
 *  PART 4 — LEXICAL ANALYZER
 * ======================================================================== */

const char* token_type_to_string(TokenType type) {
    switch (type) {
        case TOK_INT:    return "int";
        case TOK_FLOAT:  return "float";
        case TOK_VOID:   return "void";
        case TOK_BOOL:   return "bool";
        case TOK_CLASS:  return "class";
        case TOK_RETURN: return "return";
        case TOK_IF:     return "if";
        case TOK_ELSE:   return "else";
        case TOK_WHILE:  return "while";
        case TOK_PRINT:  return "print";
        case TOK_LPAREN: return "'('";
        case TOK_RPAREN: return "')'";
        case TOK_LBRACE: return "'{'";
        case TOK_RBRACE: return "'}'";
        case TOK_ID:     return "Identifier";
        case TOK_NUM:    return "Number";
        case TOK_ASSIGN: return "'='";
        case TOK_PLUS:   return "'+'";
        case TOK_MINUS:  return "'-'";
        case TOK_MUL:    return "'*'";
        case TOK_DIV:    return "'/'";
        case TOK_MOD:    return "'%'";
        case TOK_LT:     return "'<'";
        case TOK_GT:     return "'>'";
        case TOK_LE:     return "'<='";
        case TOK_GE:     return "'>='";
        case TOK_EQ:     return "'=='";
        case TOK_NEQ:    return "'!='";
        case TOK_AND:    return "'&&'";
        case TOK_OR:     return "'||'";
        case TOK_NOT:    return "'!'";
        case TOK_SEMI:   return "';'";
        case TOK_COMMA:  return "','";
        case TOK_COLON:  return "':'";
        case TOK_EOF:    return "EOF";
        default:         return "Unknown";
    }
}

const char* token_category(TokenType type) {
    switch (type) {
        case TOK_INT: case TOK_FLOAT: case TOK_VOID: case TOK_IF:
        case TOK_ELSE: case TOK_WHILE: case TOK_PRINT:
        case TOK_BOOL: case TOK_CLASS: case TOK_RETURN:
            return "Keyword";
        case TOK_ID:     return "Identifier";
        case TOK_NUM:    return "Number";
        case TOK_ASSIGN: return "Assignment Op";
        case TOK_PLUS: case TOK_MINUS: case TOK_MUL:
        case TOK_DIV: case TOK_MOD:
            return "Arithmetic Op";
        case TOK_LT: case TOK_GT: case TOK_LE: case TOK_GE:
        case TOK_EQ: case TOK_NEQ:
            return "Relational Op";
        case TOK_AND: case TOK_OR: case TOK_NOT:
            return "Logical Op";
        case TOK_LPAREN: case TOK_RPAREN: case TOK_LBRACE:
        case TOK_RBRACE: case TOK_SEMI: case TOK_COMMA:
        case TOK_COLON:
            return "Delimiter";
        default:         return "Error";
    }
}

/* Tokeniser — advances to the next token */
void advance(void) {
    while (isspace(*input_ptr)) {
        if (*input_ptr == '\n') current_line++;
        input_ptr++;
    }

    current_token.line = current_line;

    if (*input_ptr == '\0') {
        current_token.type = TOK_EOF;
        strcpy(current_token.lexeme, "EOF");
        return;
    }

    /* Identifiers / Keywords */
    if (isalpha(*input_ptr) || *input_ptr == '_') {
        int i = 0;
        while (isalnum(*input_ptr) || *input_ptr == '_') {
            if (i < 63) current_token.lexeme[i++] = *input_ptr;
            input_ptr++;
        }
        current_token.lexeme[i] = '\0';

        if      (strcmp(current_token.lexeme, "if")     == 0)  current_token.type = TOK_IF;
        else if (strcmp(current_token.lexeme, "else")   == 0)  current_token.type = TOK_ELSE;
        else if (strcmp(current_token.lexeme, "while")  == 0)  current_token.type = TOK_WHILE;
        else if (strcmp(current_token.lexeme, "print")  == 0)  current_token.type = TOK_PRINT;
        else if (strcmp(current_token.lexeme, "int")    == 0)  current_token.type = TOK_INT;
        else if (strcmp(current_token.lexeme, "float")  == 0)  current_token.type = TOK_FLOAT;
        else if (strcmp(current_token.lexeme, "void")   == 0)  current_token.type = TOK_VOID;
        else if (strcmp(current_token.lexeme, "bool")   == 0)  current_token.type = TOK_BOOL;
        else if (strcmp(current_token.lexeme, "class")  == 0)  current_token.type = TOK_CLASS;
        else if (strcmp(current_token.lexeme, "return") == 0)  current_token.type = TOK_RETURN;
        else                                                   current_token.type = TOK_ID;
        return;
    }

    /* Numeric literals */
    if (isdigit(*input_ptr)) {
        int i = 0;
        while (isdigit(*input_ptr)) {
            if (i < 63) current_token.lexeme[i++] = *input_ptr;
            input_ptr++;
        }
        if (*input_ptr == '.') {
            if (i < 63) current_token.lexeme[i++] = *input_ptr;
            input_ptr++;
            while (isdigit(*input_ptr)) {
                if (i < 63) current_token.lexeme[i++] = *input_ptr;
                input_ptr++;
            }
        }
        current_token.lexeme[i] = '\0';
        current_token.type = TOK_NUM;
        return;
    }

    /* Operators and delimiters */
    current_token.lexeme[0] = *input_ptr;
    current_token.lexeme[1] = '\0';

    switch (*input_ptr) {
        case '(': current_token.type = TOK_LPAREN; input_ptr++; break;
        case ')': current_token.type = TOK_RPAREN; input_ptr++; break;
        case '{': current_token.type = TOK_LBRACE; input_ptr++; break;
        case '}': current_token.type = TOK_RBRACE; input_ptr++; break;
        case ';': current_token.type = TOK_SEMI;   input_ptr++; break;
        case ',': current_token.type = TOK_COMMA;  input_ptr++; break;
        case ':': current_token.type = TOK_COLON;  input_ptr++; break;
        case '+': current_token.type = TOK_PLUS;   input_ptr++; break;
        case '-': current_token.type = TOK_MINUS;  input_ptr++; break;
        case '*': current_token.type = TOK_MUL;    input_ptr++; break;
        case '/': current_token.type = TOK_DIV;    input_ptr++; break;
        case '%': current_token.type = TOK_MOD;    input_ptr++; break;
        case '=':
            input_ptr++;
            if (*input_ptr == '=') { current_token.type = TOK_EQ; strcpy(current_token.lexeme, "=="); input_ptr++; }
            else { current_token.type = TOK_ASSIGN; }
            break;
        case '<':
            input_ptr++;
            if (*input_ptr == '=') { current_token.type = TOK_LE; strcpy(current_token.lexeme, "<="); input_ptr++; }
            else { current_token.type = TOK_LT; }
            break;
        case '>':
            input_ptr++;
            if (*input_ptr == '=') { current_token.type = TOK_GE; strcpy(current_token.lexeme, ">="); input_ptr++; }
            else { current_token.type = TOK_GT; }
            break;
        case '!':
            input_ptr++;
            if (*input_ptr == '=') { current_token.type = TOK_NEQ; strcpy(current_token.lexeme, "!="); input_ptr++; }
            else { current_token.type = TOK_NOT; }
            break;
        case '&':
            input_ptr++;
            if (*input_ptr == '&') { current_token.type = TOK_AND; strcpy(current_token.lexeme, "&&"); input_ptr++; }
            else { current_token.type = TOK_ERROR; }
            break;
        case '|':
            input_ptr++;
            if (*input_ptr == '|') { current_token.type = TOK_OR; strcpy(current_token.lexeme, "||"); input_ptr++; }
            else { current_token.type = TOK_ERROR; }
            break;
        default:
            current_token.type = TOK_ERROR; input_ptr++; break;
    }
}

/* ========================================================================
 *  PART 4b — LEXICAL ANALYSIS PASS (Phase 1 output)
 * ======================================================================== */

typedef struct {
    Token tokens[2048];
    int   count;
} TokenList;

TokenList token_list;

void run_lexical_analysis(char *src) {
    printf("+================================================================+\n");
    printf("|            PHASE 1: LEXICAL ANALYSIS                       |\n");
    printf("+================================================================+\n");
    printf("|  %-6s  %-14s  %-18s  %-6s  |\n", "No.", "Lexeme", "Category", "Line");
    printf("+================================================================+\n");

    input_ptr    = src;
    current_line = 1;
    token_list.count = 0;

    advance();
    int idx = 1;
    while (current_token.type != TOK_EOF) {
        if (current_token.type == TOK_ERROR) {
            printf("|  %-6d  %-14s  %-18s  %-6d  |  *** LEXICAL ERROR\n",
                   idx, current_token.lexeme, "ERROR", current_token.line);
        } else {
            printf("|  %-6d  %-14s  %-18s  %-6d  |\n",
                   idx, current_token.lexeme,
                   token_category(current_token.type),
                   current_token.line);
        }
        if (token_list.count < 2048) {
            token_list.tokens[token_list.count++] = current_token;
        }
        idx++;
        advance();
    }
    /* store EOF */
    if (token_list.count < 2048) {
        token_list.tokens[token_list.count++] = current_token;
    }

    printf("+================================================================+\n");
    printf("|  Total tokens: %-4d                                        |\n", idx - 1);
    printf("+================================================================+\n\n");
}

/* ========================================================================
 *  PART 5 — SYNTAX ANALYSIS (Recursive-Descent Parser)
 *           + SYMBOL TABLE updates during parsing
 *
 *  NOTE: The parser validates syntax only — no parse tree is built.
 *        Functions return void.  Symbol table operations (insert, lookup,
 *        push/pop scope) are performed as declarations and variable
 *        usages are encountered during the recursive descent.
 * ======================================================================== */

/* Forward declarations */
void parse_BoolExpr(void);
void parse_StmtList(void);
void parse_Stmt(void);
void parse_Block(void);
void parse_ClassDef(void);
void parse_ReturnStmt(void);

/* match: consume a token of expected type, report syntax error otherwise */
void match(TokenType expected) {
    if (current_token.type == expected) {
        advance();
    } else {
        printf("\n  *** Syntax Error on line %d: Expected %s, but encountered '%s'\n",
               current_token.line, token_type_to_string(expected), current_token.lexeme);
        exit(1);
    }
}

/* ----- Expression Parsers ----- */

void parse_Factor(void) {
    if (current_token.type == TOK_ID) {
        /* Variable usage — look it up in symbol table */
        searchSym(top, current_token.lexeme);
        advance();
    } else if (current_token.type == TOK_NUM) {
        advance();
    } else if (current_token.type == TOK_LPAREN) {
        match(TOK_LPAREN);
        parse_BoolExpr();
        match(TOK_RPAREN);
    } else if (current_token.type == TOK_NOT) {
        match(TOK_NOT);
        parse_Factor();
    } else {
        printf("\n  *** Syntax Error on line %d: Expected ID, NUM, '(' or '!', "
               "but encountered '%s'\n", current_token.line, current_token.lexeme);
        exit(1);
    }
}

void parse_Term(void) {
    parse_Factor();
    while (current_token.type == TOK_MUL ||
           current_token.type == TOK_DIV ||
           current_token.type == TOK_MOD) {
        advance();   /* consume the operator */
        parse_Factor();
    }
}

void parse_ArithExpr(void) {
    parse_Term();
    while (current_token.type == TOK_PLUS ||
           current_token.type == TOK_MINUS) {
        advance();   /* consume the operator */
        parse_Term();
    }
}

void parse_RelExpr(void) {
    parse_ArithExpr();
    while (current_token.type == TOK_LT || current_token.type == TOK_GT ||
           current_token.type == TOK_LE || current_token.type == TOK_GE) {
        advance();   /* consume the operator */
        parse_ArithExpr();
    }
}

void parse_EqExpr(void) {
    parse_RelExpr();
    while (current_token.type == TOK_EQ || current_token.type == TOK_NEQ) {
        advance();   /* consume the operator */
        parse_RelExpr();
    }
}

void parse_AndExpr(void) {
    parse_EqExpr();
    while (current_token.type == TOK_AND) {
        advance();   /* consume && */
        parse_EqExpr();
    }
}

void parse_BoolExpr(void) {
    parse_AndExpr();
    while (current_token.type == TOK_OR) {
        advance();   /* consume || */
        parse_AndExpr();
    }
}

/* ----- Statement Parsers (with Symbol Table integration) ----- */

/*
 * parse_DeclOrFunc() handles both:
 *   1. Variable declaration:    int x;   or   float avg;
 *   2. Function/procedure def:  void pro_one() { ... }  or  int func() { ... }
 *
 * After consuming the type keyword and the identifier, we peek at the next
 * token: if it is '(' then this is a function/procedure definition —
 * the name is inserted into the symbol table with type "proc",
 * then the parameter list (currently empty) and the body block are parsed.
 * Otherwise it is a normal variable declaration.
 */
/*
 * parse_ParamList() — parse a comma-separated list of typed parameters
 * inside function parentheses.  E.g.  (int m)  or  (int a, float b)
 * Parameters are inserted into the CURRENT scope (the function body scope
 * is not yet open, so they go into the enclosing scope — we will re-insert
 * them into the function body scope after push_scope, or we can push first).
 * For simplicity, we push the function body scope BEFORE parsing params
 * so params live inside the function scope.
 */
void parse_ParamList(void) {
    /* If immediately ')' → no parameters */
    if (current_token.type == TOK_RPAREN) return;

    /* First parameter */
    if (current_token.type == TOK_INT || current_token.type == TOK_FLOAT ||
        current_token.type == TOK_VOID || current_token.type == TOK_BOOL) {
        char ptype[16];
        strcpy(ptype, current_token.lexeme);
        match(current_token.type);

        char pname[64];
        strncpy(pname, current_token.lexeme, 63); pname[63] = '\0';
        match(TOK_ID);
        insert_symbol(pname, ptype);

        /* Additional parameters separated by ',' */
        while (current_token.type == TOK_COMMA) {
            match(TOK_COMMA);
            strcpy(ptype, current_token.lexeme);
            if (current_token.type == TOK_INT || current_token.type == TOK_FLOAT ||
                current_token.type == TOK_VOID || current_token.type == TOK_BOOL) {
                match(current_token.type);
            }
            strncpy(pname, current_token.lexeme, 63); pname[63] = '\0';
            match(TOK_ID);
            insert_symbol(pname, ptype);
        }
    }
}

void parse_DeclOrFunc(void) {
    if (current_token.type == TOK_INT || current_token.type == TOK_FLOAT ||
        current_token.type == TOK_VOID || current_token.type == TOK_BOOL) {
        char declared_type[16];
        strcpy(declared_type, current_token.lexeme);   /* "int", "float", "void", or "bool" */
        match(current_token.type);

        /* Capture the name before matching */
        char name[64];
        strncpy(name, current_token.lexeme, 63); name[63] = '\0';
        match(TOK_ID);

        if (current_token.type == TOK_LPAREN) {
            /* ── Function / Procedure definition ── */
            insert_symbol(name, "proc");
            match(TOK_LPAREN);
            /* Push the function body scope BEFORE parsing params
             * so that parameters live inside the function scope */
            push_scope();
            parse_ParamList();
            match(TOK_RPAREN);
            /* Parse the function body '{' ... '}' inline (scope already pushed) */
            match(TOK_LBRACE);
            parse_StmtList();
            print_all_scopes();
            pop_scope();
            match(TOK_RBRACE);
        } else {
            /* ── Variable declaration ── */
            insert_symbol(name, declared_type);

            /* Support comma-separated declarations:  int x, y, z; */
            while (current_token.type == TOK_COMMA) {
                match(TOK_COMMA);
                char extra_name[64];
                strncpy(extra_name, current_token.lexeme, 63); extra_name[63] = '\0';
                match(TOK_ID);
                insert_symbol(extra_name, declared_type);
            }

            /* Optional initialisation:  int x = expr; */
            if (current_token.type == TOK_ASSIGN) {
                match(TOK_ASSIGN);
                parse_BoolExpr();
            }
            match(TOK_SEMI);
        }
    }
}

void parse_AssignStmt(void) {
    /* Left-hand side — look up variable being assigned to */
    searchSym(top, current_token.lexeme);

    match(TOK_ID);
    match(TOK_ASSIGN);
    parse_BoolExpr();
    match(TOK_SEMI);
}

void parse_Block(void) {
    /* '{' → push a new scope onto the symbol-table stack */
    match(TOK_LBRACE);
    push_scope();

    parse_StmtList();

    /* '}' → pop the current scope */
    print_all_scopes();                /* snapshot before we leave */
    pop_scope();
    match(TOK_RBRACE);
}

void parse_IfStmt(void) {
    match(TOK_IF);
    match(TOK_LPAREN);
    parse_BoolExpr();
    match(TOK_RPAREN);
    parse_Block();

    if (current_token.type == TOK_ELSE) {
        match(TOK_ELSE);
        parse_Block();
    }
}

void parse_WhileStmt(void) {
    match(TOK_WHILE);
    match(TOK_LPAREN);
    parse_BoolExpr();
    match(TOK_RPAREN);
    parse_Block();
}

void parse_PrintStmt(void) {
    match(TOK_PRINT);
    match(TOK_LPAREN);
    parse_BoolExpr();
    match(TOK_RPAREN);
    match(TOK_SEMI);
}

/*
 * parse_ReturnStmt() — handles:  return ;  or  return expr ;
 */
void parse_ReturnStmt(void) {
    match(TOK_RETURN);
    /* Optional return value — if next token is not ';', parse an expression */
    if (current_token.type != TOK_SEMI) {
        parse_BoolExpr();
    }
    match(TOK_SEMI);
}

/*
 * parse_ClassDef() — handles:  class ClassName { members }
 * Members can be variable declarations or method (function) definitions.
 * The class name is inserted into the current scope with type "class".
 * The class body gets its own scope.
 */
void parse_ClassDef(void) {
    match(TOK_CLASS);
    char class_name[64];
    strncpy(class_name, current_token.lexeme, 63); class_name[63] = '\0';
    match(TOK_ID);
    insert_symbol(class_name, "class");
    /* Class body — same as a block but also parses declarations/methods */
    match(TOK_LBRACE);
    push_scope();
    parse_StmtList();
    print_all_scopes();
    pop_scope();
    match(TOK_RBRACE);
}

void parse_Stmt(void) {
    if (current_token.type == TOK_INT || current_token.type == TOK_FLOAT ||
        current_token.type == TOK_VOID || current_token.type == TOK_BOOL) {
        parse_DeclOrFunc();
    } else if (current_token.type == TOK_CLASS) {
        parse_ClassDef();
    } else if (current_token.type == TOK_RETURN) {
        parse_ReturnStmt();
    } else if (current_token.type == TOK_ID) {
        /* Could be an assignment  OR  a label (id ':') */
        /* Save the identifier name */
        char id_name[64];
        strncpy(id_name, current_token.lexeme, 63); id_name[63] = '\0';
        advance();  /* consume the identifier */

        if (current_token.type == TOK_COLON) {
            /* ── Label definition: id ':' ── */
            printf("  [LABEL] '%s' on line %d\n", id_name, current_token.line);
            match(TOK_COLON);
            /* After a label, there may or may not be another statement */
        } else if (current_token.type == TOK_ASSIGN) {
            /* ── Assignment: id '=' expr ';' ── */
            searchSym(top, id_name);
            match(TOK_ASSIGN);
            parse_BoolExpr();
            match(TOK_SEMI);
        } else {
            /* Standalone identifier usage (expression statement) — look it up */
            searchSym(top, id_name);
            /* Could be part of an expression — consume remaining expression if any */
            /* For simplicity, expect a semicolon */
            match(TOK_SEMI);
        }
    } else if (current_token.type == TOK_IF) {
        parse_IfStmt();
    } else if (current_token.type == TOK_WHILE) {
        parse_WhileStmt();
    } else if (current_token.type == TOK_PRINT) {
        parse_PrintStmt();
    } else if (current_token.type == TOK_LBRACE) {
        parse_Block();
    } else {
        printf("\n  *** Syntax Error on line %d: Unexpected token '%s'\n",
               current_token.line, current_token.lexeme);
        exit(1);
    }
}

void parse_StmtList(void) {
    while (current_token.type != TOK_RBRACE && current_token.type != TOK_EOF) {
        parse_Stmt();
    }
}

/* ========================================================================
 *  PART 6 — MAIN
 * ======================================================================== */

int main(int argc, char **argv) {
    char source[16384];
    source[0] = '\0';

    /* --- Read input --- */
    if (argc > 1) {
        FILE *fp = fopen(argv[1], "r");
        if (!fp) { printf("Could not open file: %s\n", argv[1]); return 1; }
        size_t len = fread(source, 1, sizeof(source) - 1, fp);
        source[len] = '\0';
        fclose(fp);
    } else {
        printf("Enter source code (end with Ctrl+Z on Windows / Ctrl+D on Linux):\n");
        int i = 0; char ch;
        while ((ch = getchar()) != EOF && i < (int)sizeof(source) - 1)
            source[i++] = ch;
        source[i] = '\0';
    }

    if (strlen(source) == 0) { printf("No input.\n"); return 0; }

    /* ──────────────────────────────────────────────────────────────────
     *  PHASE 1: LEXICAL ANALYSIS
     * ────────────────────────────────────────────────────────────────── */
    run_lexical_analysis(source);

    /* ──────────────────────────────────────────────────────────────────
     *  PHASE 2 & 3: SYNTAX ANALYSIS + SYNTAX ERROR DETECTION
     *  PHASE 4:     SYMBOL TABLE CONSTRUCTION (concurrent with parsing)
     * ────────────────────────────────────────────────────────────────── */
    printf("+================================================================+\n");
    printf("|  PHASE 2: SYNTAX ANALYSIS  +  PHASE 3: ERROR DETECTION     |\n");
    printf("|  PHASE 4: SYMBOL TABLE CONSTRUCTION (during parsing)       |\n");
    printf("+================================================================+\n\n");

    /* Reset lexer state for second pass */
    input_ptr    = source;
    current_line = 1;
    advance();

    /* Create the global (top-level) scope -- scope 0 */
    push_scope();

    if (current_token.type != TOK_EOF) {
        parse_StmtList();

        if (current_token.type == TOK_EOF) {
            printf("\n========================================================\n");
            printf("  [OK] Parsing completed successfully -- syntax is valid.\n");
            printf("========================================================\n");

            /* Final symbol table snapshot */
            printf("\n  --- Final Symbol Table (global scope) ---\n");
            print_all_scopes();

            /* ──────────────────────────────────────────────────────────────
             *  PHASE 5: INTERACTIVE SYMBOL TABLE LOOKUP
             * ────────────────────────────────────────────────────────────── */
            printf("+================================================================+\n");
            printf("|          PHASE 5: SYMBOL TABLE LOOKUP                      |\n");
            printf("+================================================================+\n");
            printf("|  Enter a variable name to search in the symbol table.      |\n");
            printf("|  Type 'q' or 'quit' to exit.                               |\n");
            printf("+================================================================+\n");

            char search_var[64];
            while (1) {
                printf("\n  Search variable >>> ");
                if (scanf("%63s", search_var) != 1) break;   /* EOF / error */

                if (strcmp(search_var, "q") == 0 || strcmp(search_var, "quit") == 0) {
                    printf("  Exiting symbol table lookup.\n");
                    break;
                }

                int result = searchSym(top, search_var);
                if (result == -1) {
                    printf("  '%s' is not declared in any accessible scope.\n", search_var);
                }
            }

        } else {
            printf("\n  *** Syntax Error on line %d: Extraneous tokens -- found '%s'\n",
                   current_token.line, current_token.lexeme);
        }
    }

    /* Pop the global scope */
    pop_scope();

    return 0;
}
