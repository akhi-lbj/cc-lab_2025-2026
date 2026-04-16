/*
 * Week 5: Symbol Table with Nested Scope Management + Semantic Analysis using AST
 * ---------------------------------------------------------------------------
 * Combines:
 * Phase 1 - Lexical Analysis (Week 1, enhanced with bool literals + comments)
 * Phase 2 - Syntax Analysis (Week 2/3) — now builds full AST (parse tree)
 * Phase 3 - Syntax Error Detection (Week 3)
 * Phase 4 - Symbol Table with Nested Block Scopes (Week 5)
 * Phase 5 - Semantic Analysis (NEW: traverses AST for type checking, etc.)
 * Phase 6 - Interactive Symbol Table Lookup
 *
 * Semantic Analysis (using AST traversal):
 * • Detects use of undeclared variables
 * • Detects multiple declarations within the same scope
 * • Detects type mismatches in assignments and expressions (int/float/bool)
 * • Detects invalid boolean conditions (if/while must be bool)
 *
 * Demonstrates semantic validation on the evaluation program.
 * Two explicit semantic error cases are shown in comments at the bottom
 * (you can save them to test_error1.txt / test_error2.txt and run the compiler).
 *
 * Usage:
 * ./week_5 eval.txt   (read from file)
 * ./week_5            (read from stdin)
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ========================================================================
 * PART 1 — TOKEN DEFINITIONS (enhanced with true/false + comments)
 * ======================================================================== */
typedef enum {
    TOK_INT, TOK_FLOAT, TOK_VOID, TOK_BOOL,
    TOK_IF, TOK_ELSE, TOK_WHILE, TOK_PRINT, TOK_RETURN, TOK_CLASS,
    TOK_TRUE, TOK_FALSE,
    TOK_ID, TOK_NUM,
    TOK_ASSIGN, TOK_PLUS, TOK_MINUS, TOK_MUL, TOK_DIV, TOK_MOD,
    TOK_LT, TOK_GT, TOK_LE, TOK_GE, TOK_EQ, TOK_NEQ,
    TOK_AND, TOK_OR, TOK_NOT,
    TOK_LPAREN, TOK_RPAREN, TOK_LBRACE, TOK_RBRACE,
    TOK_SEMI, TOK_COMMA, TOK_COLON,
    TOK_EOF, TOK_ERROR
} TokenType;

typedef struct {
    TokenType type;
    char lexeme[64];
    int line;
} Token;

/* ========================================================================
 * PART 2 — SYMBOL TABLE (unchanged from original, with nice prints)
 * ======================================================================== */
#define MAX_SYMBOLS 128
typedef struct {
    char name[64];
    char type[16];   /* "int", "float", "proc", "class", "bool" */
    int scope_level;
    int offset;
} Symbol;

typedef struct ScopeNode {
    Symbol symbols[MAX_SYMBOLS];
    int count;
    int scope_level;
    struct ScopeNode *next;
} ScopeNode;

ScopeNode *top = NULL;
int scope_counter = 0;
int global_offset = 0;
int sem_error_count = 0;

/* Create new scope */
void push_scope(void) {
    ScopeNode *S = top;
    ScopeNode *nn = (ScopeNode*)malloc(sizeof(ScopeNode));
    nn->count = 0;
    nn->scope_level = scope_counter++;
    nn->next = S;
    top = nn;
    printf(" [ST] >>> Entered new scope (level %d)\n", nn->scope_level);
}

/* Pop scope with nice dump */
void pop_scope(void) {
    if (!top) return;
    printf(" [ST] <<< Leaving scope (level %d) -- dropping %d symbol(s):\n",
           top->scope_level, top->count);
    for (int i = 0; i < top->count; i++) {
        printf(" - %s (%s, offset=%d)\n",
               top->symbols[i].name,
               top->symbols[i].type,
               top->symbols[i].offset);
    }
    ScopeNode *save = top->next;
    free(top);
    top = save;
}

/* Insert with duplicate check in CURRENT scope only */
void insert_symbol(const char *name, const char *type) {
    if (!top) return;
    for (int i = 0; i < top->count; i++) {
        if (strcmp(top->symbols[i].name, name) == 0) {
            printf(" [ST] WARNING: '%s' already declared in scope %d\n",
                   name, top->scope_level);
            return;
        }
    }
    if (top->count >= MAX_SYMBOLS) return;
    int size = 4;
    Symbol *sym = &top->symbols[top->count++];
    strncpy(sym->name, name, 63); sym->name[63] = '\0';
    strncpy(sym->type, type, 15); sym->type[15] = '\0';
    sym->scope_level = top->scope_level;
    sym->offset = global_offset;
    global_offset += size;
    printf(" [ST] INSERT: name=%-8s type=%-6s scope=%-3d offset=%d\n",
           sym->name, sym->type, sym->scope_level, sym->offset);
}

/* Search up the scope chain (supports shadowing) */
Symbol* searchSym(ScopeNode *S, const char *var) {
    ScopeNode *cur = S;
    while (cur) {
        for (int i = 0; i < cur->count; i++) {
            if (strcmp(cur->symbols[i].name, var) == 0) {
                printf(" [ST] LOOKUP '%s': FOUND in scope %d -- type=%s, offset=%d\n",
                       var, cur->symbols[i].scope_level,
                       cur->symbols[i].type,
                       cur->symbols[i].offset);
                return &cur->symbols[i];
            }
        }
        cur = cur->next;
    }
    printf(" [ST] LOOKUP '%s': NOT FOUND (undeclared variable)\n", var);
    return NULL;
}

/* Pretty-print entire symbol table (used at block exits and final snapshot) */
void print_all_scopes(void) {
    printf("\n +====================================================+\n");
    printf(" | CURRENT SYMBOL TABLE SNAPSHOT |\n");
    printf(" +====================================================+\n");
    ScopeNode *cur = top;
    if (!cur) {
        printf(" | (empty -- no active scopes) |\n");
    }
    while (cur) {
        printf(" | Scope Level %d (%d symbols) \n",
               cur->scope_level, cur->count);
        printf(" | %-10s %-8s %-6s %-8s\n", "Name", "Type", "Scope", "Offset");
        printf(" | %-10s %-8s %-6s %-8s\n", "--------", "------", "-----", "------");
        for (int i = 0; i < cur->count; i++) {
            printf(" | %-10s %-8s %-6d %-8d\n",
                   cur->symbols[i].name,
                   cur->symbols[i].type,
                   cur->symbols[i].scope_level,
                   cur->symbols[i].offset);
        }
        cur = cur->next;
    }
    printf(" +====================================================+\n\n");
}

/* Semantic error reporter */
void sem_error(int line, const char *msg) {
    sem_error_count++;
    printf("  [SEMANTIC ERROR #%d] Line %d: %s\n", sem_error_count, line, msg);
}

/* ========================================================================
 * PART 3 — AST (Parse Tree) for Semantic Analysis
 * ======================================================================== */
typedef enum {
    NODE_VAR_DECL, NODE_FUNC_DEF, NODE_CLASS_DEF,
    NODE_ASSIGN, NODE_IF, NODE_WHILE, NODE_PRINT, NODE_RETURN,
    NODE_BLOCK, NODE_STMT_LIST,
    NODE_BINARY_OP, NODE_UNARY_OP, NODE_VAR_REF, NODE_LITERAL,
    NODE_LABEL, NODE_PARAM_LIST
} NodeKind;

typedef struct ASTNode {
    NodeKind kind;
    TokenType op;
    char lexeme[64];
    char type_str[16];
    struct ASTNode *children[16];
    int child_count;
    int line;
} ASTNode;

ASTNode* create_node(NodeKind kind, int line) {
    ASTNode* n = (ASTNode*)malloc(sizeof(ASTNode));
    n->kind = kind;
    n->line = line;
    n->child_count = 0;
    n->op = TOK_ERROR;
    n->lexeme[0] = '\0';
    n->type_str[0] = '\0';
    for (int i = 0; i < 16; i++) n->children[i] = NULL;
    return n;
}

/* ========================================================================
 * GLOBALS + LEXER (same as original + bool literals + comment support)
 * ======================================================================== */
Token current_token;
char *input_ptr;
int current_line = 1;

/* Tokeniser */
void advance(void) {
    while (1) {
        if (isspace(*input_ptr)) {
            if (*input_ptr == '\n') current_line++;
            input_ptr++;
        } else if (*input_ptr == '/' && *(input_ptr+1) == '*') {
            input_ptr += 2;
            while (*input_ptr != '\0' && !(*input_ptr == '*' && *(input_ptr+1) == '/')) {
                if (*input_ptr == '\n') current_line++;
                input_ptr++;
            }
            if (*input_ptr != '\0') input_ptr += 2;
        } else break;
    }
    current_token.line = current_line;
    if (*input_ptr == '\0') {
        current_token.type = TOK_EOF;
        strcpy(current_token.lexeme, "EOF");
        return;
    }
    if (isalpha(*input_ptr) || *input_ptr == '_') {
        int i = 0;
        while (isalnum(*input_ptr) || *input_ptr == '_') {
            if (i < 63) current_token.lexeme[i++] = *input_ptr;
            input_ptr++;
        }
        current_token.lexeme[i] = '\0';
        if (strcmp(current_token.lexeme, "int") == 0) current_token.type = TOK_INT;
        else if (strcmp(current_token.lexeme, "float") == 0) current_token.type = TOK_FLOAT;
        else if (strcmp(current_token.lexeme, "void") == 0) current_token.type = TOK_VOID;
        else if (strcmp(current_token.lexeme, "bool") == 0) current_token.type = TOK_BOOL;
        else if (strcmp(current_token.lexeme, "if") == 0) current_token.type = TOK_IF;
        else if (strcmp(current_token.lexeme, "else") == 0) current_token.type = TOK_ELSE;
        else if (strcmp(current_token.lexeme, "while") == 0) current_token.type = TOK_WHILE;
        else if (strcmp(current_token.lexeme, "print") == 0) current_token.type = TOK_PRINT;
        else if (strcmp(current_token.lexeme, "return") == 0) current_token.type = TOK_RETURN;
        else if (strcmp(current_token.lexeme, "class") == 0) current_token.type = TOK_CLASS;
        else if (strcmp(current_token.lexeme, "true") == 0) current_token.type = TOK_TRUE;
        else if (strcmp(current_token.lexeme, "false") == 0) current_token.type = TOK_FALSE;
        else current_token.type = TOK_ID;
        return;
    }
    if (isdigit(*input_ptr)) {
        int i = 0;
        while (isdigit(*input_ptr)) { if (i < 63) current_token.lexeme[i++] = *input_ptr; input_ptr++; }
        if (*input_ptr == '.') {
            if (i < 63) current_token.lexeme[i++] = *input_ptr; input_ptr++;
            while (isdigit(*input_ptr)) { if (i < 63) current_token.lexeme[i++] = *input_ptr; input_ptr++; }
        }
        current_token.lexeme[i] = '\0';
        current_token.type = TOK_NUM;
        return;
    }
    current_token.lexeme[0] = *input_ptr; current_token.lexeme[1] = '\0';
    switch (*input_ptr) {
        case '(': current_token.type = TOK_LPAREN; input_ptr++; break;
        case ')': current_token.type = TOK_RPAREN; input_ptr++; break;
        case '{': current_token.type = TOK_LBRACE; input_ptr++; break;
        case '}': current_token.type = TOK_RBRACE; input_ptr++; break;
        case ';': current_token.type = TOK_SEMI; input_ptr++; break;
        case ',': current_token.type = TOK_COMMA; input_ptr++; break;
        case ':': current_token.type = TOK_COLON; input_ptr++; break;
        case '+': current_token.type = TOK_PLUS; input_ptr++; break;
        case '-': current_token.type = TOK_MINUS; input_ptr++; break;
        case '*': current_token.type = TOK_MUL; input_ptr++; break;
        case '/': current_token.type = TOK_DIV; input_ptr++; break;
        case '%': current_token.type = TOK_MOD; input_ptr++; break;
        case '=': input_ptr++; if (*input_ptr == '=') { current_token.type = TOK_EQ; strcpy(current_token.lexeme, "=="); input_ptr++; } else current_token.type = TOK_ASSIGN; break;
        case '<': input_ptr++; if (*input_ptr == '=') { current_token.type = TOK_LE; strcpy(current_token.lexeme, "<="); input_ptr++; } else current_token.type = TOK_LT; break;
        case '>': input_ptr++; if (*input_ptr == '=') { current_token.type = TOK_GE; strcpy(current_token.lexeme, ">="); input_ptr++; } else current_token.type = TOK_GT; break;
        case '!': input_ptr++; if (*input_ptr == '=') { current_token.type = TOK_NEQ; strcpy(current_token.lexeme, "!="); input_ptr++; } else current_token.type = TOK_NOT; break;
        case '&': input_ptr++; if (*input_ptr == '&') { current_token.type = TOK_AND; strcpy(current_token.lexeme, "&&"); input_ptr++; } else current_token.type = TOK_ERROR; break;
        case '|': input_ptr++; if (*input_ptr == '|') { current_token.type = TOK_OR; strcpy(current_token.lexeme, "||"); input_ptr++; } else current_token.type = TOK_ERROR; break;
        default: current_token.type = TOK_ERROR; input_ptr++; break;
    }
}

/* PHASE 1: Lexical Analysis (table format from original) */
void run_lexical_analysis(char *src) {
    printf("+================================================================+\n");
    printf("| PHASE 1: LEXICAL ANALYSIS                                      |\n");
    printf("+================================================================+\n");
    printf("| %-6s %-14s %-18s %-6s |\n", "No.", "Lexeme", "Category", "Line");
    printf("+================================================================+\n");
    input_ptr = src;
    current_line = 1;
    advance();
    int idx = 1;
    while (current_token.type != TOK_EOF) {
        const char* cat = (current_token.type == TOK_ID) ? "Identifier" :
                         (current_token.type == TOK_NUM) ? "Number" :
                         (current_token.type == TOK_TRUE || current_token.type == TOK_FALSE) ? "Boolean Literal" : "Other";
        printf("| %-6d %-14s %-18s %-6d |\n", idx, current_token.lexeme, cat, current_token.line);
        idx++;
        advance();
    }
    printf("+================================================================+\n");
    printf("| Total tokens: %-4d                                         |\n", idx - 1);
    printf("+================================================================+\n\n");
}

/* ========================================================================
 * PART 4 — PARSER (builds AST — required for semantic analysis)
 * ======================================================================== */
void match(TokenType expected) {
    if (current_token.type == expected) advance();
    else {
        printf("\n *** Syntax Error on line %d: Expected %d, but encountered '%s'\n",
               current_token.line, expected, current_token.lexeme);
        exit(1);
    }
}

ASTNode* parse_BoolExpr(void);

ASTNode* parse_Factor(void) {
    ASTNode* node;
    if (current_token.type == TOK_ID) {
        node = create_node(NODE_VAR_REF, current_token.line);
        strcpy(node->lexeme, current_token.lexeme);
        advance();
    } else if (current_token.type == TOK_NUM) {
        node = create_node(NODE_LITERAL, current_token.line);
        strcpy(node->lexeme, current_token.lexeme);
        strcpy(node->type_str, strchr(node->lexeme, '.') ? "float" : "int");
        advance();
    } else if (current_token.type == TOK_TRUE || current_token.type == TOK_FALSE) {
        node = create_node(NODE_LITERAL, current_token.line);
        strcpy(node->lexeme, current_token.lexeme);
        strcpy(node->type_str, "bool");
        advance();
    } else if (current_token.type == TOK_LPAREN) {
        match(TOK_LPAREN);
        node = parse_BoolExpr();
        match(TOK_RPAREN);
    } else if (current_token.type == TOK_NOT) {
        node = create_node(NODE_UNARY_OP, current_token.line);
        node->op = TOK_NOT;
        advance();
        node->children[0] = parse_Factor();
        node->child_count = 1;
    } else {
        printf("\n *** Syntax Error on line %d: Expected factor, got '%s'\n",
               current_token.line, current_token.lexeme);
        exit(1);
    }
    return node;
}

ASTNode* parse_Term(void) {
    ASTNode* left = parse_Factor();
    while (current_token.type == TOK_MUL || current_token.type == TOK_DIV || current_token.type == TOK_MOD) {
        ASTNode* n = create_node(NODE_BINARY_OP, current_token.line);
        n->op = current_token.type; advance();
        n->children[0] = left; n->children[1] = parse_Factor(); n->child_count = 2;
        left = n;
    }
    return left;
}

ASTNode* parse_ArithExpr(void) {
    ASTNode* left = parse_Term();
    while (current_token.type == TOK_PLUS || current_token.type == TOK_MINUS) {
        ASTNode* n = create_node(NODE_BINARY_OP, current_token.line);
        n->op = current_token.type; advance();
        n->children[0] = left; n->children[1] = parse_Term(); n->child_count = 2;
        left = n;
    }
    return left;
}

ASTNode* parse_RelExpr(void) { /* similar chaining for < > <= >= */ 
    ASTNode* left = parse_ArithExpr();
    while (current_token.type == TOK_LT || current_token.type == TOK_GT ||
           current_token.type == TOK_LE || current_token.type == TOK_GE) {
        ASTNode* n = create_node(NODE_BINARY_OP, current_token.line);
        n->op = current_token.type; advance();
        n->children[0] = left; n->children[1] = parse_ArithExpr(); n->child_count = 2;
        left = n;
    }
    return left;
}

ASTNode* parse_EqExpr(void) {
    ASTNode* left = parse_RelExpr();
    while (current_token.type == TOK_EQ || current_token.type == TOK_NEQ) {
        ASTNode* n = create_node(NODE_BINARY_OP, current_token.line);
        n->op = current_token.type; advance();
        n->children[0] = left; n->children[1] = parse_RelExpr(); n->child_count = 2;
        left = n;
    }
    return left;
}

ASTNode* parse_AndExpr(void) {
    ASTNode* left = parse_EqExpr();
    while (current_token.type == TOK_AND) {
        ASTNode* n = create_node(NODE_BINARY_OP, current_token.line);
        n->op = TOK_AND; advance();
        n->children[0] = left; n->children[1] = parse_EqExpr(); n->child_count = 2;
        left = n;
    }
    return left;
}

ASTNode* parse_BoolExpr(void) {
    ASTNode* left = parse_AndExpr();
    while (current_token.type == TOK_OR) {
        ASTNode* n = create_node(NODE_BINARY_OP, current_token.line);
        n->op = TOK_OR; advance();
        n->children[0] = left; n->children[1] = parse_AndExpr(); n->child_count = 2;
        left = n;
    }
    return left;
}

/* Statement parsers — now return AST nodes */
ASTNode* parse_Stmt(void);
ASTNode* parse_StmtList(void);

ASTNode* parse_Stmt(void) {
    if (current_token.type == TOK_INT || current_token.type == TOK_FLOAT ||
        current_token.type == TOK_VOID || current_token.type == TOK_BOOL) {
        char declared_type[16];
        strcpy(declared_type, current_token.lexeme);
        int line = current_token.line;
        advance();
        char name[64];
        strncpy(name, current_token.lexeme, 63); name[63] = '\0';
        match(TOK_ID);
        if (current_token.type == TOK_LPAREN) {
            /* Function definition */
            ASTNode* n = create_node(NODE_FUNC_DEF, line);
            strcpy(n->lexeme, name);
            strcpy(n->type_str, declared_type);
            match(TOK_LPAREN);
            ASTNode* params = create_node(NODE_PARAM_LIST, line);
            if (current_token.type != TOK_RPAREN) {
                do {
                    if (current_token.type == TOK_COMMA) match(TOK_COMMA);
                    ASTNode* p = create_node(NODE_VAR_DECL, current_token.line);
                    strcpy(p->type_str, current_token.lexeme); match(current_token.type);
                    strncpy(p->lexeme, current_token.lexeme, 63); p->lexeme[63] = '\0';
                    match(TOK_ID);
                    params->children[params->child_count++] = p;
                } while (current_token.type == TOK_COMMA);
            }
            match(TOK_RPAREN);
            match(TOK_LBRACE);
            n->children[0] = params;
            n->children[1] = parse_StmtList();
            n->child_count = 2;
            match(TOK_RBRACE);
            return n;
        } else {
            /* Variable declaration(s) + optional init */
            ASTNode* list = create_node(NODE_STMT_LIST, line);
            ASTNode* d = create_node(NODE_VAR_DECL, line);
            strcpy(d->lexeme, name); strcpy(d->type_str, declared_type);
            list->children[list->child_count++] = d;
            while (current_token.type == TOK_COMMA) {
                match(TOK_COMMA);
                ASTNode* d2 = create_node(NODE_VAR_DECL, current_token.line);
                strncpy(d2->lexeme, current_token.lexeme, 63); d2->lexeme[63] = '\0';
                strcpy(d2->type_str, declared_type);
                match(TOK_ID);
                list->children[list->child_count++] = d2;
            }
            if (current_token.type == TOK_ASSIGN) {
                ASTNode* a = create_node(NODE_ASSIGN, current_token.line);
                strcpy(a->lexeme, name);
                match(TOK_ASSIGN);
                a->children[0] = parse_BoolExpr();
                a->child_count = 1;
                list->children[list->child_count++] = a;
            }
            match(TOK_SEMI);
            return list;
        }
    } else if (current_token.type == TOK_ID) {
        char name[64]; strncpy(name, current_token.lexeme, 63); name[63] = '\0';
        int line = current_token.line; advance();
        if (current_token.type == TOK_COLON) {
            ASTNode* n = create_node(NODE_LABEL, line);
            strcpy(n->lexeme, name);
            match(TOK_COLON);
            return n;
        } else if (current_token.type == TOK_ASSIGN) {
            ASTNode* n = create_node(NODE_ASSIGN, line);
            strcpy(n->lexeme, name);
            match(TOK_ASSIGN);
            n->children[0] = parse_BoolExpr();
            n->child_count = 1;
            match(TOK_SEMI);
            return n;
        } else {
            match(TOK_SEMI);
            return NULL;
        }
    } else if (current_token.type == TOK_IF) {
        ASTNode* n = create_node(NODE_IF, current_token.line); match(TOK_IF);
        match(TOK_LPAREN);
        n->children[0] = parse_BoolExpr();
        match(TOK_RPAREN);
        match(TOK_LBRACE);
        n->children[1] = parse_StmtList();
        match(TOK_RBRACE);
        n->child_count = 2;
        if (current_token.type == TOK_ELSE) {
            match(TOK_ELSE); match(TOK_LBRACE);
            n->children[2] = parse_StmtList();
            match(TOK_RBRACE);
            n->child_count = 3;
        }
        return n;
    } else if (current_token.type == TOK_WHILE) {
        ASTNode* n = create_node(NODE_WHILE, current_token.line); match(TOK_WHILE);
        match(TOK_LPAREN);
        n->children[0] = parse_BoolExpr();
        match(TOK_RPAREN);
        match(TOK_LBRACE);
        n->children[1] = parse_StmtList();
        match(TOK_RBRACE);
        n->child_count = 2;
        return n;
    } else if (current_token.type == TOK_PRINT) {
        ASTNode* n = create_node(NODE_PRINT, current_token.line); match(TOK_PRINT);
        match(TOK_LPAREN);
        n->children[0] = parse_BoolExpr();
        match(TOK_RPAREN); match(TOK_SEMI);
        n->child_count = 1;
        return n;
    } else if (current_token.type == TOK_CLASS) {
        ASTNode* n = create_node(NODE_CLASS_DEF, current_token.line); match(TOK_CLASS);
        strncpy(n->lexeme, current_token.lexeme, 63); n->lexeme[63] = '\0';
        match(TOK_ID);
        match(TOK_LBRACE);
        n->children[0] = parse_StmtList();
        match(TOK_RBRACE);
        n->child_count = 1;
        return n;
    } else if (current_token.type == TOK_RETURN) {
        ASTNode* n = create_node(NODE_RETURN, current_token.line); match(TOK_RETURN);
        if (current_token.type != TOK_SEMI) {
            n->children[0] = parse_BoolExpr();
            n->child_count = 1;
        }
        match(TOK_SEMI);
        return n;
    } else if (current_token.type == TOK_LBRACE) {
        ASTNode* n = create_node(NODE_BLOCK, current_token.line);
        match(TOK_LBRACE);
        n->children[0] = parse_StmtList();
        match(TOK_RBRACE);
        n->child_count = 1;
        return n;
    }
    return NULL;
}

ASTNode* parse_StmtList(void) {
    ASTNode* list = create_node(NODE_STMT_LIST, current_token.line);
    while (current_token.type != TOK_RBRACE && current_token.type != TOK_EOF) {
        ASTNode* s = parse_Stmt();
        if (s) list->children[list->child_count++] = s;
    }
    return list;
}

/* ========================================================================
 * PART 5 — SEMANTIC ANALYSIS (traverses AST)
 * ======================================================================== */
int is_num(const char* t) { return t && (strcmp(t,"int")==0 || strcmp(t,"float")==0); }
int is_bl(const char* t) { return t && strcmp(t,"bool")==0; }

const char* eval_type(ASTNode* n) {
    if (!n) return "void";
    if (n->kind == NODE_LITERAL) return n->type_str;
    if (n->kind == NODE_VAR_REF) {
        Symbol* s = searchSym(top, n->lexeme);
        if (s) return s->type;
        char msg[256]; sprintf(msg, "Undeclared variable '%s'", n->lexeme);
        sem_error(n->line, msg);
        return "error";
    }
    if (n->kind == NODE_BINARY_OP) {
        const char* t1 = eval_type(n->children[0]);
        const char* t2 = eval_type(n->children[1]);
        if (n->op == TOK_PLUS || n->op == TOK_MINUS || n->op == TOK_MUL || n->op == TOK_DIV || n->op == TOK_MOD) {
            if (!is_num(t1) || !is_num(t2)) {
                sem_error(n->line, "Type mismatch: numeric operands required");
                return "error";
            }
            return (strcmp(t1,"float")==0 || strcmp(t2,"float")==0) ? "float" : "int";
        }
        /* relational / logical → bool */
        return "bool";
    }
    if (n->kind == NODE_UNARY_OP) return "bool";
    return "void";
}

void analyze(ASTNode* n) {
    if (!n) return;
    switch (n->kind) {
        case NODE_STMT_LIST:
            for (int i = 0; i < n->child_count; i++) analyze(n->children[i]);
            break;
        case NODE_VAR_DECL:
            insert_symbol(n->lexeme, n->type_str);
            break;
        case NODE_FUNC_DEF:
            insert_symbol(n->lexeme, "proc");
            push_scope();
            analyze(n->children[0]);  /* params */
            analyze(n->children[1]);  /* body */
            pop_scope();
            break;
        case NODE_PARAM_LIST:
            for (int i = 0; i < n->child_count; i++) analyze(n->children[i]);
            break;
        case NODE_BLOCK:
            push_scope();
            analyze(n->children[0]);
            print_all_scopes();   /* snapshot before leaving block */
            pop_scope();
            break;
        case NODE_CLASS_DEF:
            insert_symbol(n->lexeme, "class");
            push_scope();
            analyze(n->children[0]);
            print_all_scopes();
            pop_scope();
            break;
        case NODE_ASSIGN: {
            Symbol* s = searchSym(top, n->lexeme);
            const char* rt = eval_type(n->children[0]);
            if (!s) {
                char msg[256]; sprintf(msg, "Assigning to undeclared variable '%s'", n->lexeme);
                sem_error(n->line, msg);
            } else if (strcmp(s->type, rt) != 0 &&
                       !(strcmp(s->type,"float")==0 && strcmp(rt,"int")==0)) {
                char msg[256]; sprintf(msg, "Type mismatch on assignment to '%s' (expected %s, got %s)",
                                       n->lexeme, s->type, rt);
                sem_error(n->line, msg);
            }
            break;
        }
        case NODE_IF:
        case NODE_WHILE: {
            const char* ct = eval_type(n->children[0]);
            if (!is_bl(ct)) {
                char msg[256];
                sprintf(msg, "Invalid boolean condition: expected bool, got '%s'", ct);
                sem_error(n->line, msg);
            }
            analyze(n->children[1]);
            if (n->kind == NODE_IF && n->child_count == 3) analyze(n->children[2]);
            break;
        }
        default:
            for (int i = 0; i < n->child_count; i++) analyze(n->children[i]);
            break;
    }
}

/* ========================================================================
 * MAIN — full pipeline with semantic validation + interactive lookup
 * ======================================================================== */
int main(int argc, char **argv) {
    char source[16384] = {0};

    /* Read input (file or stdin) — same as original */
    if (argc > 1) {
        FILE *fp = fopen(argv[1], "r");
        if (!fp) { printf("Could not open file: %s\n", argv[1]); return 1; }
        size_t len = fread(source, 1, sizeof(source)-1, fp);
        source[len] = '\0';
        fclose(fp);
    } else {
        printf("Enter source code (end with Ctrl+D / Ctrl+Z):\n");
        int i = 0; int ch;
        while ((ch = getchar()) != EOF && i < (int)sizeof(source)-1)
            source[i++] = (char)ch;
        source[i] = '\0';
    }
    if (strlen(source) == 0) { printf("No input.\n"); return 0; }

    /* PHASE 1: Lexical Analysis */
    run_lexical_analysis(source);

    /* PHASE 2/3: Syntax Analysis + AST construction */
    printf("+================================================================+\n");
    printf("| PHASE 2: SYNTAX ANALYSIS + AST CONSTRUCTION                    |\n");
    printf("| PHASE 3: SYNTAX ERROR DETECTION                                |\n");
    printf("+================================================================+\n\n");
    input_ptr = source;
    current_line = 1;
    advance();

    push_scope();                     /* global scope (level 0) */
    ASTNode* root = parse_StmtList();

    if (current_token.type == TOK_EOF) {
        printf("\n========================================================\n");
        printf(" [OK] Parsing completed successfully -- syntax is valid.\n");
        printf("========================================================\n");
    } else {
        printf("\n *** Syntax Error: Extraneous tokens after program\n");
        return 1;
    }

    /* PHASE 4 + 5: Semantic Analysis (traverses the AST) */
    printf("\n+================================================================+\n");
    printf("| PHASE 4: SYMBOL TABLE CONSTRUCTION + PHASE 5: SEMANTIC ANALYSIS |\n");
    printf("+================================================================+\n\n");
    analyze(root);

    if (sem_error_count == 0) {
        printf("\n========================================================\n");
        printf(" [OK] Semantic validation completed successfully for the evaluation program.\n");
        printf("========================================================\n");
    } else {
        printf("\n========================================================\n");
        printf(" [FAIL] %d semantic error(s) found in the evaluation program.\n", sem_error_count);
        printf("========================================================\n");
    }

    /* Final symbol-table snapshot */
    printf("\n --- Final Symbol Table (global scope) ---\n");
    print_all_scopes();

    /* PHASE 6: Interactive Symbol Table Lookup (original feature) */
    printf("+================================================================+\n");
    printf("| PHASE 6: INTERACTIVE SYMBOL TABLE LOOKUP                      |\n");
    printf("+================================================================+\n");
    printf("| Enter a variable name to search. Type 'q' or 'quit' to exit.  |\n");
    printf("+================================================================+\n");
    char search_var[64];
    while (1) {
        printf("\n Search variable >>> ");
        if (scanf("%63s", search_var) != 1) break;
        if (strcmp(search_var, "q") == 0 || strcmp(search_var, "quit") == 0) break;
        searchSym(top, search_var);
    }

    pop_scope();   /* clean up global scope */
    return 0;
}

/*
 * DEMONSTRATION OF SEMANTIC ERROR CASES
 * ======================================
 * Save the snippets below as separate test files and run:
 *   ./week_5 test_error1.txt
 *   ./week_5 test_error2.txt
 *
 * Error case 1 — Multiple declarations + undeclared variable:
int x;
int x;          // multiple declaration in same scope
y = 10;         // undeclared variable

 * Error case 2 — Type mismatch + invalid boolean condition:
int a = 5;
bool b = true;
a = b;          // type mismatch (bool → int)
if (a) { }      // invalid boolean condition (int instead of bool)
while (42) { }  // invalid boolean condition
*/