/*
 * Week 6: Three-Address Code (TAC) Generation
 * ---------------------------------------------------------------------------
 * Builds on Week 5 (Symbol Table + Semantic Analysis) and adds:
 * Phase 6 - Three-Address Code (TAC) Generation
 *
 * Full compiler pipeline:
 * Phase 1 - Lexical Analysis    (tokeniser, enhanced: bool literals + comments)
 * Phase 2 - Syntax Analysis     (recursive-descent parser → full AST)
 * Phase 3 - Syntax Error Detection
 * Phase 4 - Symbol Table        (nested block scopes, shadowing)
 * Phase 5 - Semantic Analysis   (type checking, undeclared vars, bool conditions)
 * Phase 6 - TAC Generation      (NEW: temporary vars + jump labels)
 *
 * TAC handles:
 *   • Arithmetic expressions   (t1 = a + b)
 *   • Boolean expressions      (short-circuit &&, ||, !)
 *   • Relational comparisons   (if t1 < t2 goto L1)
 *   • Conditional branching    (if/else with true/false labels)
 *   • Loop control flow        (while with start/body/exit labels)
 *   • Print statements         (print t1)
 *
 * Usage:
 *   ./week_6 eval.txt     (read from file)
 *   ./week_6              (read from stdin)
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ========================================================================
 * PART 1 — TOKEN DEFINITIONS
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
 * PART 2 — SYMBOL TABLE (nested scopes)
 * ======================================================================== */
#define MAX_SYMBOLS 128

typedef struct {
    char name[64];
    char type[16];   /* "int", "float", "bool", "proc", "class" */
    int  scope_level;
    int  offset;
} Symbol;

typedef struct ScopeNode {
    Symbol          symbols[MAX_SYMBOLS];
    int             count;
    int             scope_level;
    struct ScopeNode *next;
} ScopeNode;

ScopeNode *top          = NULL;
int        scope_counter = 0;
int        global_offset = 0;
int        sem_error_count = 0;

void push_scope(void) {
    ScopeNode *nn = (ScopeNode*)malloc(sizeof(ScopeNode));
    nn->count       = 0;
    nn->scope_level = scope_counter++;
    nn->next        = top;
    top             = nn;
    printf(" [ST] >>> Entered new scope (level %d)\n", nn->scope_level);
}

void pop_scope(void) {
    if (!top) return;
    printf(" [ST] <<< Leaving scope (level %d) -- dropping %d symbol(s):\n",
           top->scope_level, top->count);
    for (int i = 0; i < top->count; i++)
        printf("       - %s (%s, offset=%d)\n",
               top->symbols[i].name,
               top->symbols[i].type,
               top->symbols[i].offset);
    ScopeNode *save = top->next;
    free(top);
    top = save;
}

void insert_symbol(const char *name, const char *type) {
    if (!top) return;
    /* duplicate check in current scope only */
    for (int i = 0; i < top->count; i++) {
        if (strcmp(top->symbols[i].name, name) == 0) {
            printf(" [ST] WARNING: '%s' already declared in scope %d\n",
                   name, top->scope_level);
            return;
        }
    }
    if (top->count >= MAX_SYMBOLS) return;
    Symbol *sym = &top->symbols[top->count++];
    strncpy(sym->name, name, 63);  sym->name[63]  = '\0';
    strncpy(sym->type, type, 15);  sym->type[15]  = '\0';
    sym->scope_level = top->scope_level;
    sym->offset      = global_offset;
    global_offset   += 4;
    printf(" [ST] INSERT: name=%-10s type=%-6s scope=%-3d offset=%d\n",
           sym->name, sym->type, sym->scope_level, sym->offset);
}

/* Search up the scope chain (supports shadowing) */
Symbol* searchSym(ScopeNode *S, const char *var) {
    for (ScopeNode *cur = S; cur; cur = cur->next)
        for (int i = 0; i < cur->count; i++)
            if (strcmp(cur->symbols[i].name, var) == 0) {
                printf(" [ST] LOOKUP '%s': FOUND in scope %d -- type=%s, offset=%d\n",
                       var, cur->symbols[i].scope_level,
                       cur->symbols[i].type, cur->symbols[i].offset);
                return &cur->symbols[i];
            }
    printf(" [ST] LOOKUP '%s': NOT FOUND (undeclared variable)\n", var);
    return NULL;
}

void print_all_scopes(void) {
    printf("\n +====================================================+\n");
    printf(" | CURRENT SYMBOL TABLE SNAPSHOT                     |\n");
    printf(" +====================================================+\n");
    ScopeNode *cur = top;
    if (!cur) {
        printf(" | (empty -- no active scopes)                        |\n");
    }
    while (cur) {
        printf(" | Scope Level %d (%d symbol(s))\n", cur->scope_level, cur->count);
        printf(" | %-12s %-8s %-6s %-8s\n", "Name", "Type", "Scope", "Offset");
        printf(" | %-12s %-8s %-6s %-8s\n", "----------", "------", "-----", "------");
        for (int i = 0; i < cur->count; i++)
            printf(" | %-12s %-8s %-6d %-8d\n",
                   cur->symbols[i].name,
                   cur->symbols[i].type,
                   cur->symbols[i].scope_level,
                   cur->symbols[i].offset);
        cur = cur->next;
    }
    printf(" +====================================================+\n\n");
}

void sem_error(int line, const char *msg) {
    sem_error_count++;
    printf("  [SEMANTIC ERROR #%d] Line %d: %s\n", sem_error_count, line, msg);
}

/* ========================================================================
 * PART 3 — AST NODE DEFINITIONS
 * ======================================================================== */
typedef enum {
    NODE_VAR_DECL, NODE_FUNC_DEF, NODE_CLASS_DEF,
    NODE_ASSIGN, NODE_IF, NODE_WHILE, NODE_PRINT, NODE_RETURN,
    NODE_BLOCK, NODE_STMT_LIST,
    NODE_BINARY_OP, NODE_UNARY_OP, NODE_VAR_REF, NODE_LITERAL,
    NODE_LABEL, NODE_PARAM_LIST
} NodeKind;

typedef struct ASTNode {
    NodeKind        kind;
    TokenType       op;
    char            lexeme[64];
    char            type_str[16];
    struct ASTNode *children[16];
    int             child_count;
    int             line;
} ASTNode;

ASTNode* create_node(NodeKind kind, int line) {
    ASTNode *n = (ASTNode*)malloc(sizeof(ASTNode));
    n->kind        = kind;
    n->line        = line;
    n->child_count = 0;
    n->op          = TOK_ERROR;
    n->lexeme[0]   = '\0';
    n->type_str[0] = '\0';
    for (int i = 0; i < 16; i++) n->children[i] = NULL;
    return n;
}

/* ========================================================================
 * PART 4 — LEXER
 * ======================================================================== */
Token  current_token;
char  *input_ptr;
int    current_line = 1;

void advance(void) {
    /* Skip whitespace and block comments */
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

    /* Identifiers and keywords */
    if (isalpha(*input_ptr) || *input_ptr == '_') {
        int i = 0;
        while (isalnum(*input_ptr) || *input_ptr == '_') {
            if (i < 63) current_token.lexeme[i++] = *input_ptr;
            input_ptr++;
        }
        current_token.lexeme[i] = '\0';
        if      (strcmp(current_token.lexeme, "int")    == 0) current_token.type = TOK_INT;
        else if (strcmp(current_token.lexeme, "float")  == 0) current_token.type = TOK_FLOAT;
        else if (strcmp(current_token.lexeme, "void")   == 0) current_token.type = TOK_VOID;
        else if (strcmp(current_token.lexeme, "bool")   == 0) current_token.type = TOK_BOOL;
        else if (strcmp(current_token.lexeme, "if")     == 0) current_token.type = TOK_IF;
        else if (strcmp(current_token.lexeme, "else")   == 0) current_token.type = TOK_ELSE;
        else if (strcmp(current_token.lexeme, "while")  == 0) current_token.type = TOK_WHILE;
        else if (strcmp(current_token.lexeme, "print")  == 0) current_token.type = TOK_PRINT;
        else if (strcmp(current_token.lexeme, "return") == 0) current_token.type = TOK_RETURN;
        else if (strcmp(current_token.lexeme, "class")  == 0) current_token.type = TOK_CLASS;
        else if (strcmp(current_token.lexeme, "true")   == 0) current_token.type = TOK_TRUE;
        else if (strcmp(current_token.lexeme, "false")  == 0) current_token.type = TOK_FALSE;
        else current_token.type = TOK_ID;
        return;
    }

    /* Numeric literals (int and float) */
    if (isdigit(*input_ptr)) {
        int i = 0;
        while (isdigit(*input_ptr)) {
            if (i < 63) current_token.lexeme[i++] = *input_ptr;
            input_ptr++;
        }
        if (*input_ptr == '.') {
            if (i < 63) { current_token.lexeme[i++] = *input_ptr; } input_ptr++;
            while (isdigit(*input_ptr)) {
                if (i < 63) current_token.lexeme[i++] = *input_ptr;
                input_ptr++;
            }
        }
        current_token.lexeme[i] = '\0';
        current_token.type = TOK_NUM;
        return;
    }

    /* Operators and punctuation */
    current_token.lexeme[0] = *input_ptr;
    current_token.lexeme[1] = '\0';
    switch (*input_ptr) {
        case '(': current_token.type = TOK_LPAREN;  input_ptr++; break;
        case ')': current_token.type = TOK_RPAREN;  input_ptr++; break;
        case '{': current_token.type = TOK_LBRACE;  input_ptr++; break;
        case '}': current_token.type = TOK_RBRACE;  input_ptr++; break;
        case ';': current_token.type = TOK_SEMI;    input_ptr++; break;
        case ',': current_token.type = TOK_COMMA;   input_ptr++; break;
        case ':': current_token.type = TOK_COLON;   input_ptr++; break;
        case '+': current_token.type = TOK_PLUS;    input_ptr++; break;
        case '-': current_token.type = TOK_MINUS;   input_ptr++; break;
        case '*': current_token.type = TOK_MUL;     input_ptr++; break;
        case '/': current_token.type = TOK_DIV;     input_ptr++; break;
        case '%': current_token.type = TOK_MOD;     input_ptr++; break;
        case '=':
            input_ptr++;
            if (*input_ptr == '=') { current_token.type = TOK_EQ;  strcpy(current_token.lexeme, "=="); input_ptr++; }
            else                     current_token.type = TOK_ASSIGN;
            break;
        case '<':
            input_ptr++;
            if (*input_ptr == '=') { current_token.type = TOK_LE;  strcpy(current_token.lexeme, "<="); input_ptr++; }
            else                     current_token.type = TOK_LT;
            break;
        case '>':
            input_ptr++;
            if (*input_ptr == '=') { current_token.type = TOK_GE;  strcpy(current_token.lexeme, ">="); input_ptr++; }
            else                     current_token.type = TOK_GT;
            break;
        case '!':
            input_ptr++;
            if (*input_ptr == '=') { current_token.type = TOK_NEQ; strcpy(current_token.lexeme, "!="); input_ptr++; }
            else                     current_token.type = TOK_NOT;
            break;
        case '&':
            input_ptr++;
            if (*input_ptr == '&') { current_token.type = TOK_AND; strcpy(current_token.lexeme, "&&"); input_ptr++; }
            else                     current_token.type = TOK_ERROR;
            break;
        case '|':
            input_ptr++;
            if (*input_ptr == '|') { current_token.type = TOK_OR;  strcpy(current_token.lexeme, "||"); input_ptr++; }
            else                     current_token.type = TOK_ERROR;
            break;
        default: current_token.type = TOK_ERROR; input_ptr++; break;
    }
}

/* ========================================================================
 * PHASE 1 — LEXICAL ANALYSIS DISPLAY (detailed token categories)
 * ======================================================================== */
static const char* token_category(TokenType t) {
    switch (t) {
        case TOK_INT: case TOK_FLOAT: case TOK_VOID: case TOK_BOOL:
        case TOK_IF: case TOK_ELSE: case TOK_WHILE:
        case TOK_PRINT: case TOK_RETURN: case TOK_CLASS:
            return "Keyword";
        case TOK_TRUE: case TOK_FALSE:
            return "Boolean Literal";
        case TOK_ID:
            return "Identifier";
        case TOK_NUM:
            return "Number";
        case TOK_ASSIGN:
            return "Assignment Op";
        case TOK_PLUS: case TOK_MINUS: case TOK_MUL:
        case TOK_DIV:  case TOK_MOD:
            return "Arithmetic Op";
        case TOK_LT: case TOK_GT: case TOK_LE:
        case TOK_GE: case TOK_EQ: case TOK_NEQ:
            return "Relational Op";
        case TOK_AND: case TOK_OR: case TOK_NOT:
            return "Logical Op";
        case TOK_LPAREN: case TOK_RPAREN:
        case TOK_LBRACE: case TOK_RBRACE:
        case TOK_SEMI:   case TOK_COMMA: case TOK_COLON:
            return "Punctuation";
        default:
            return "Unknown";
    }
}

void run_lexical_analysis(char *src) {
    printf("+====================================================================+\n");
    printf("| PHASE 1: LEXICAL ANALYSIS                                          |\n");
    printf("+====================================================================+\n");
    printf("| %-5s | %-16s | %-20s | %-4s |\n", "No.", "Lexeme", "Category", "Line");
    printf("+-------+-----------------+---------------------+------+\n");

    /* Save and restore state so the main parse is unaffected */
    char  *saved_ptr  = input_ptr;
    int    saved_line = current_line;
    Token  saved_tok  = current_token;

    input_ptr    = src;
    current_line = 1;
    advance();

    int idx = 1;
    while (current_token.type != TOK_EOF && current_token.type != TOK_ERROR) {
        printf("| %-5d | %-16s | %-20s | %-4d |\n",
               idx, current_token.lexeme,
               token_category(current_token.type),
               current_token.line);
        idx++;
        advance();
    }

    printf("+====================================================================+\n");
    printf("| Total tokens: %-4d                                                 |\n", idx - 1);
    printf("+====================================================================+\n\n");

    /* Restore lexer state */
    input_ptr    = saved_ptr;
    current_line = saved_line;
    current_token = saved_tok;
}

/* ========================================================================
 * PART 5 — PARSER (recursive-descent, builds AST)
 * ======================================================================== */
void match(TokenType expected) {
    if (current_token.type == expected) {
        advance();
    } else {
        printf("\n *** Syntax Error on line %d: Expected token %d, got '%s'\n",
               current_token.line, expected, current_token.lexeme);
        exit(1);
    }
}

ASTNode* parse_BoolExpr(void);
ASTNode* parse_Stmt(void);
ASTNode* parse_StmtList(void);
ASTNode* parse_Block(void);

ASTNode* parse_Factor(void) {
    ASTNode *node;
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
        node->child_count  = 1;
    } else if (current_token.type == TOK_MINUS) {
        /* Unary minus */
        node = create_node(NODE_UNARY_OP, current_token.line);
        node->op = TOK_MINUS;
        advance();
        node->children[0] = parse_Factor();
        node->child_count  = 1;
    } else {
        printf("\n *** Syntax Error on line %d: Unexpected token '%s' in expression\n",
               current_token.line, current_token.lexeme);
        exit(1);
    }
    return node;
}

ASTNode* parse_Term(void) {
    ASTNode *left = parse_Factor();
    while (current_token.type == TOK_MUL ||
           current_token.type == TOK_DIV ||
           current_token.type == TOK_MOD) {
        ASTNode *n = create_node(NODE_BINARY_OP, current_token.line);
        n->op          = current_token.type; advance();
        n->children[0] = left;
        n->children[1] = parse_Factor();
        n->child_count = 2;
        left = n;
    }
    return left;
}

ASTNode* parse_ArithExpr(void) {
    ASTNode *left = parse_Term();
    while (current_token.type == TOK_PLUS || current_token.type == TOK_MINUS) {
        ASTNode *n = create_node(NODE_BINARY_OP, current_token.line);
        n->op          = current_token.type; advance();
        n->children[0] = left;
        n->children[1] = parse_Term();
        n->child_count = 2;
        left = n;
    }
    return left;
}

ASTNode* parse_RelExpr(void) {
    ASTNode *left = parse_ArithExpr();
    while (current_token.type == TOK_LT || current_token.type == TOK_GT ||
           current_token.type == TOK_LE || current_token.type == TOK_GE) {
        ASTNode *n = create_node(NODE_BINARY_OP, current_token.line);
        n->op          = current_token.type; advance();
        n->children[0] = left;
        n->children[1] = parse_ArithExpr();
        n->child_count = 2;
        left = n;
    }
    return left;
}

ASTNode* parse_EqExpr(void) {
    ASTNode *left = parse_RelExpr();
    while (current_token.type == TOK_EQ || current_token.type == TOK_NEQ) {
        ASTNode *n = create_node(NODE_BINARY_OP, current_token.line);
        n->op          = current_token.type; advance();
        n->children[0] = left;
        n->children[1] = parse_RelExpr();
        n->child_count = 2;
        left = n;
    }
    return left;
}

ASTNode* parse_AndExpr(void) {
    ASTNode *left = parse_EqExpr();
    while (current_token.type == TOK_AND) {
        ASTNode *n = create_node(NODE_BINARY_OP, current_token.line);
        n->op          = TOK_AND; advance();
        n->children[0] = left;
        n->children[1] = parse_EqExpr();
        n->child_count = 2;
        left = n;
    }
    return left;
}

ASTNode* parse_BoolExpr(void) {
    ASTNode *left = parse_AndExpr();
    while (current_token.type == TOK_OR) {
        ASTNode *n = create_node(NODE_BINARY_OP, current_token.line);
        n->op          = TOK_OR; advance();
        n->children[0] = left;
        n->children[1] = parse_AndExpr();
        n->child_count = 2;
        left = n;
    }
    return left;
}

ASTNode* parse_Block(void) {
    ASTNode *n = create_node(NODE_BLOCK, current_token.line);
    match(TOK_LBRACE);
    n->children[0] = parse_StmtList();
    match(TOK_RBRACE);
    n->child_count = 1;
    return n;
}

ASTNode* parse_Stmt(void) {
    /* Variable / function declaration */
    if (current_token.type == TOK_INT  || current_token.type == TOK_FLOAT ||
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
            ASTNode *n = create_node(NODE_FUNC_DEF, line);
            strcpy(n->lexeme, name);
            strcpy(n->type_str, declared_type);
            match(TOK_LPAREN);
            ASTNode *params = create_node(NODE_PARAM_LIST, line);
            if (current_token.type != TOK_RPAREN) {
                do {
                    if (current_token.type == TOK_COMMA) match(TOK_COMMA);
                    ASTNode *p = create_node(NODE_VAR_DECL, current_token.line);
                    strcpy(p->type_str, current_token.lexeme);
                    match(current_token.type);
                    strncpy(p->lexeme, current_token.lexeme, 63); p->lexeme[63] = '\0';
                    match(TOK_ID);
                    params->children[params->child_count++] = p;
                } while (current_token.type == TOK_COMMA);
            }
            match(TOK_RPAREN);
            match(TOK_LBRACE);
            n->children[0] = params;
            n->children[1] = parse_StmtList();
            n->child_count  = 2;
            match(TOK_RBRACE);
            return n;
        } else {
            /* Variable declaration(s) with optional initialiser */
            ASTNode *list = create_node(NODE_STMT_LIST, line);
            ASTNode *d    = create_node(NODE_VAR_DECL, line);
            strcpy(d->lexeme, name); strcpy(d->type_str, declared_type);
            list->children[list->child_count++] = d;

            /* Multiple declarations: int a, b, c; */
            while (current_token.type == TOK_COMMA) {
                match(TOK_COMMA);
                ASTNode *d2 = create_node(NODE_VAR_DECL, current_token.line);
                strncpy(d2->lexeme, current_token.lexeme, 63); d2->lexeme[63] = '\0';
                strcpy(d2->type_str, declared_type);
                match(TOK_ID);
                list->children[list->child_count++] = d2;
            }

            /* Optional initialiser: int x = expr; */
            if (current_token.type == TOK_ASSIGN) {
                ASTNode *a = create_node(NODE_ASSIGN, current_token.line);
                strcpy(a->lexeme, name);
                match(TOK_ASSIGN);
                a->children[0] = parse_BoolExpr();
                a->child_count  = 1;
                list->children[list->child_count++] = a;
            }
            match(TOK_SEMI);
            return list;
        }

    } else if (current_token.type == TOK_ID) {
        char name[64]; strncpy(name, current_token.lexeme, 63); name[63] = '\0';
        int  line = current_token.line;
        advance();

        if (current_token.type == TOK_COLON) {
            /* User-defined label */
            ASTNode *n = create_node(NODE_LABEL, line);
            strcpy(n->lexeme, name);
            match(TOK_COLON);
            return n;
        } else if (current_token.type == TOK_ASSIGN) {
            /* Assignment */
            ASTNode *n = create_node(NODE_ASSIGN, line);
            strcpy(n->lexeme, name);
            match(TOK_ASSIGN);
            n->children[0] = parse_BoolExpr();
            n->child_count  = 1;
            match(TOK_SEMI);
            return n;
        } else {
            /* Expression-statement (identifier alone) */
            match(TOK_SEMI);
            return NULL;
        }

    } else if (current_token.type == TOK_IF) {
        ASTNode *n = create_node(NODE_IF, current_token.line); match(TOK_IF);
        match(TOK_LPAREN);
        n->children[0] = parse_BoolExpr();
        match(TOK_RPAREN);
        n->children[1] = parse_Block();
        n->child_count  = 2;
        if (current_token.type == TOK_ELSE) {
            match(TOK_ELSE);
            n->children[2] = parse_Block();
            n->child_count  = 3;
        }
        return n;

    } else if (current_token.type == TOK_WHILE) {
        ASTNode *n = create_node(NODE_WHILE, current_token.line); match(TOK_WHILE);
        match(TOK_LPAREN);
        n->children[0] = parse_BoolExpr();
        match(TOK_RPAREN);
        n->children[1] = parse_Block();
        n->child_count  = 2;
        return n;

    } else if (current_token.type == TOK_PRINT) {
        ASTNode *n = create_node(NODE_PRINT, current_token.line); match(TOK_PRINT);
        match(TOK_LPAREN);
        n->children[0] = parse_BoolExpr();
        match(TOK_RPAREN);
        match(TOK_SEMI);
        n->child_count = 1;
        return n;

    } else if (current_token.type == TOK_CLASS) {
        ASTNode *n = create_node(NODE_CLASS_DEF, current_token.line); match(TOK_CLASS);
        strncpy(n->lexeme, current_token.lexeme, 63); n->lexeme[63] = '\0';
        match(TOK_ID);
        match(TOK_LBRACE);
        n->children[0] = parse_StmtList();
        match(TOK_RBRACE);
        n->child_count = 1;
        return n;

    } else if (current_token.type == TOK_RETURN) {
        ASTNode *n = create_node(NODE_RETURN, current_token.line); match(TOK_RETURN);
        if (current_token.type != TOK_SEMI) {
            n->children[0] = parse_BoolExpr();
            n->child_count  = 1;
        }
        match(TOK_SEMI);
        return n;

    } else if (current_token.type == TOK_LBRACE) {
        return parse_Block();
    }
    return NULL;
}

ASTNode* parse_StmtList(void) {
    ASTNode *list = create_node(NODE_STMT_LIST, current_token.line);
    while (current_token.type != TOK_RBRACE && current_token.type != TOK_EOF) {
        ASTNode *s = parse_Stmt();
        if (s) list->children[list->child_count++] = s;
    }
    return list;
}

/* ========================================================================
 * PART 6 — SEMANTIC ANALYSIS (AST traversal: type checking + scope)
 * ======================================================================== */
static int is_num(const char *t) { return t && (strcmp(t,"int")==0 || strcmp(t,"float")==0); }
static int is_bl (const char *t) { return t && strcmp(t,"bool")==0; }

const char* eval_type(ASTNode *n) {
    if (!n) return "void";
    if (n->kind == NODE_LITERAL) return n->type_str;
    if (n->kind == NODE_VAR_REF) {
        Symbol *s = searchSym(top, n->lexeme);
        if (s) return s->type;
        char msg[256];
        sprintf(msg, "Undeclared variable '%s'", n->lexeme);
        sem_error(n->line, msg);
        return "error";
    }
    if (n->kind == NODE_BINARY_OP) {
        const char *t1 = eval_type(n->children[0]);
        const char *t2 = eval_type(n->children[1]);
        if (n->op == TOK_PLUS || n->op == TOK_MINUS ||
            n->op == TOK_MUL  || n->op == TOK_DIV   || n->op == TOK_MOD) {
            if (!is_num(t1) || !is_num(t2)) {
                sem_error(n->line, "Type mismatch: numeric operands required");
                return "error";
            }
            return (strcmp(t1,"float")==0 || strcmp(t2,"float")==0) ? "float" : "int";
        }
        /* Relational / logical → bool */
        return "bool";
    }
    if (n->kind == NODE_UNARY_OP) {
        if (n->op == TOK_NOT) return "bool";
        /* unary minus inherits child type */
        return eval_type(n->children[0]);
    }
    return "void";
}

void analyze(ASTNode *n) {
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
            analyze(n->children[0]);   /* params */
            analyze(n->children[1]);   /* body   */
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
            Symbol    *s  = searchSym(top, n->lexeme);
            const char *rt = eval_type(n->children[0]);
            if (!s) {
                char msg[256];
                sprintf(msg, "Assigning to undeclared variable '%s'", n->lexeme);
                sem_error(n->line, msg);
            } else if (strcmp(s->type, rt) != 0 &&
                       !(strcmp(s->type,"float")==0 && strcmp(rt,"int")==0)) {
                char msg[256];
                sprintf(msg, "Type mismatch on assignment to '%s' (expected %s, got %s)",
                        n->lexeme, s->type, rt);
                sem_error(n->line, msg);
            }
            break;
        }

        case NODE_IF:
        case NODE_WHILE: {
            const char *ct = eval_type(n->children[0]);
            if (!is_bl(ct)) {
                char msg[256];
                sprintf(msg, "Condition must be bool, got '%s'", ct);
                sem_error(n->line, msg);
            }
            analyze(n->children[1]);
            if (n->kind == NODE_IF && n->child_count == 3)
                analyze(n->children[2]);
            break;
        }

        default:
            for (int i = 0; i < n->child_count; i++) analyze(n->children[i]);
            break;
    }
}

/* ========================================================================
 * PART 7 — THREE-ADDRESS CODE (TAC) GENERATION
 * ======================================================================== */
typedef struct TAC {
    char op[16];
    char arg1[64];
    char arg2[64];
    char result[64];
    struct TAC *next;
} TAC;

TAC *tac_head    = NULL;
TAC *tac_tail    = NULL;
int  temp_counter  = 1;
int  label_counter = 1;

/* ── Quadruples table ───────────────────────────────────────────────── */
/*
 * A quadruple stores the same four fields as a TAC instruction but in a
 * flat indexed array.  Position (index) is the "address" of the quad.
 * This mirrors the textbook representation:
 *
 *   index | op       | arg1  | arg2  | result
 *   ------+----------+-------+-------+--------
 *      0  | +        | y     | z     | x
 *      1  | goto     |       |       | L1
 *   ...
 */
#define MAX_QUADS 1024

typedef struct {
    char op    [16];
    char arg1  [64];
    char arg2  [64];
    char result[64];
} Quadruple;

static Quadruple quad_table[MAX_QUADS];
static int       quad_count = 0;

char* new_temp(void) {
    char *t = (char*)malloc(16);
    sprintf(t, "t%d", temp_counter++);
    return t;
}

char* new_label(void) {
    char *l = (char*)malloc(16);
    sprintf(l, "L%d", label_counter++);
    return l;
}

/* Emit one TAC instruction — also records it in the quadruples table */
void emit(const char *op, const char *arg1, const char *arg2, const char *result) {
    /* ── linked-list TAC (existing) ──────────────────────────────────── */
    TAC *instr = (TAC*)malloc(sizeof(TAC));
    strncpy(instr->op,     op     ? op     : "", 15);  instr->op[15]     = '\0';
    strncpy(instr->arg1,   arg1   ? arg1   : "", 63);  instr->arg1[63]   = '\0';
    strncpy(instr->arg2,   arg2   ? arg2   : "", 63);  instr->arg2[63]   = '\0';
    strncpy(instr->result, result ? result : "", 63);  instr->result[63] = '\0';
    instr->next = NULL;
    if (!tac_head) tac_head = tac_tail = instr;
    else { tac_tail->next = instr; tac_tail = instr; }

    /* ── quadruples table (new) ──────────────────────────────────────── */
    if (quad_count < MAX_QUADS) {
        Quadruple *q = &quad_table[quad_count++];
        strncpy(q->op,     op     ? op     : "", 15);  q->op[15]     = '\0';
        strncpy(q->arg1,   arg1   ? arg1   : "", 63);  q->arg1[63]   = '\0';
        strncpy(q->arg2,   arg2   ? arg2   : "", 63);  q->arg2[63]   = '\0';
        strncpy(q->result, result ? result : "", 63);  q->result[63] = '\0';
    }
}

void print_tac(void) {
    printf("\n+====================================================================+\n");
    printf("| PHASE 6: GENERATED THREE-ADDRESS CODE (TAC)                        |\n");
    printf("+====================================================================+\n\n");

    int line_no = 1;
    for (TAC *cur = tac_head; cur; cur = cur->next) {

        if (strcmp(cur->op, "label") == 0) {
            /* Labels flush-left for readability */
            printf("%s:\n", cur->result);

        } else if (strcmp(cur->op, "goto") == 0) {
            printf("  %3d:  goto %s\n", line_no++, cur->result);

        } else if (strcmp(cur->op, "if") == 0) {
            printf("  %3d:  if %s goto %s\n", line_no++, cur->arg1, cur->result);

        } else if (strcmp(cur->op, "print") == 0) {
            printf("  %3d:  print %s\n", line_no++, cur->arg1);

        } else if (strcmp(cur->op, "=") == 0) {
            /* Simple copy: result = arg1 */
            printf("  %3d:  %s = %s\n", line_no++, cur->result, cur->arg1);

        } else if (strcmp(cur->op, "uminus") == 0) {
            printf("  %3d:  %s = -%s\n", line_no++, cur->result, cur->arg1);

        } else {
            /* Binary:  result = arg1 op arg2  */
            printf("  %3d:  %s = %s %s %s\n",
                   line_no++, cur->result, cur->arg1, cur->op, cur->arg2);
        }
    }

    printf("\n+====================================================================+\n");
    printf("| TAC Generation Complete. %3d instruction(s) emitted.               |\n",
           line_no - 1);
    printf("+====================================================================+\n");
}

/*
 * print_quadruples — display the quadruple table.
 *
 * Format mirrors the textbook exactly:
 *
 *   idx | op       | arg1        | arg2        | result
 *   ----+----------+-------------+-------------+----------
 *     0 | +        | y           | z           | x
 *     1 | goto     |             |             | L1
 *
 * Special cases (matching the slides):
 *   - Unary op   : arg2 is shown as  "-"
 *   - Labels      : op="label", result holds the label name
 *   - goto/jump   : arg1 and arg2 are "-"
 *   - print/return: arg2 and result may be "-"
 */
void print_quadruples(void) {
    printf("\n+====================================================================+\n");
    printf("| PHASE 6 (EXTRA): QUADRUPLES TABLE                                  |\n");
    printf("|  Each instruction = (op, arg1, arg2, result)                       |\n");
    printf("|  '-' means the field is unused for that instruction type.           |\n");
    printf("+====================================================================+\n");
    printf("| %-4s | %-10s | %-13s | %-13s | %-12s |\n",
           "idx", "op", "arg1", "arg2", "result");
    printf("+------+-----------+--------------+--------------+-------------+\n");

    for (int i = 0; i < quad_count; i++) {
        Quadruple *q = &quad_table[i];

        /* Determine what to display for each field, using "-" for unused slots */
        const char *disp_arg1   = (q->arg1[0]   != '\0') ? q->arg1   : "-";
        const char *disp_arg2   = (q->arg2[0]   != '\0') ? q->arg2   : "-";
        const char *disp_result = (q->result[0] != '\0') ? q->result : "-";

        /*
         * Instruction-specific overrides to match textbook conventions:
         *
         *  label          : (label, -, -, LabelName)
         *  goto           : (goto,  -, -, Target)
         *  if cond goto L : (if,    cond, -, L)
         *  unary / copy   : (op,    arg1, -, result)
         *  binary         : (op,    arg1, arg2, result)
         *  print          : (print, val,  -, -)
         *  return         : (return,val,  -, -)
         *  func_end       : (func_end, name, -, -)
         */
        if (strcmp(q->op, "label") == 0) {
            disp_arg1 = "-"; disp_arg2 = "-";
        } else if (strcmp(q->op, "goto") == 0) {
            disp_arg1 = "-"; disp_arg2 = "-";
        } else if (strcmp(q->op, "if") == 0 || strcmp(q->op, "iff") == 0) {
            /* arg1 holds the boolean temp/variable to test; arg2 unused */
            disp_arg2 = "-";
        } else if (strcmp(q->op, "=") == 0 ||
                   strcmp(q->op, "uminus") == 0 ||
                   strcmp(q->op, "!") == 0) {
            disp_arg2 = "-";
        } else if (strcmp(q->op, "print") == 0 ||
                   strcmp(q->op, "return") == 0 ||
                   strcmp(q->op, "func_end") == 0) {
            disp_arg2 = "-"; disp_result = "-";
        }

        printf("| %-4d | %-10s | %-13s | %-13s | %-12s |\n",
               i, q->op, disp_arg1, disp_arg2, disp_result);
    }

    printf("+------+-----------+--------------+--------------+-------------+\n");
    printf("| Total quadruples: %-3d                                             |\n",
           quad_count);
    printf("+====================================================================+\n");
}

/* Forward declarations */
char* generate_tac(ASTNode *n);
void  generate_bool_tac(ASTNode *n, const char *true_label, const char *false_label);

/*
 * generate_bool_tac — short-circuit Boolean evaluation for conditions.
 *
 * For a condition node, emits:
 *   "if <cond> goto true_label"   on true  path
 *   "goto false_label"            on false path
 *
 * Implements short-circuit evaluation for && and ||.
 *
 * QUADRUPLE FORMAT COMPLIANCE (per the PNG rules):
 *   • Relational:  if x < y goto L  →  (<, x, y, t1)  then  (if, t1, -, L)
 *   • Direct bool: if x goto L      →  (if, x, -, L)
 *   • Uncond jump: goto L           →  (goto, -, -, L)
 */
void generate_bool_tac(ASTNode *n, const char *true_label, const char *false_label) {
    if (!n) return;

    /* ── Logical AND (short-circuit) ─────────────────────────────────── */
    if (n->kind == NODE_BINARY_OP && n->op == TOK_AND) {
        char *mid = new_label();
        generate_bool_tac(n->children[0], mid, false_label);
        emit("label", "", "", mid);
        generate_bool_tac(n->children[1], true_label, false_label);
        free(mid);

    /* ── Logical OR (short-circuit) ──────────────────────────────────── */
    } else if (n->kind == NODE_BINARY_OP && n->op == TOK_OR) {
        char *mid = new_label();
        generate_bool_tac(n->children[0], true_label, mid);
        emit("label", "", "", mid);
        generate_bool_tac(n->children[1], true_label, false_label);
        free(mid);

    /* ── Logical NOT ───────────────────────────────────────────────────
     * Negation is handled by swapping the true/false target labels.
     * This is logically equivalent to inverting the condition without
     * needing a separate "not" quadruple in control-flow context.
     * ───────────────────────────────────────────────────────────────── */
    } else if (n->kind == NODE_UNARY_OP && n->op == TOK_NOT) {
        generate_bool_tac(n->children[0], false_label, true_label);

    /* ── Relational comparison ─────────────────────────────────────────
     * Per the PNG rule:  if x < y goto L
     *   0:  <   x   y   t1
     *   1:  if  t1  -   L
     * We emit the relational op into a temp, then test that temp.
     * ───────────────────────────────────────────────────────────────── */
    } else if (n->kind == NODE_BINARY_OP &&
               (n->op == TOK_LT  || n->op == TOK_GT  || n->op == TOK_LE ||
                n->op == TOK_GE  || n->op == TOK_EQ  || n->op == TOK_NEQ)) {
        char *arg1 = generate_tac(n->children[0]);
        char *arg2 = generate_tac(n->children[1]);
        const char *op_str;
        switch (n->op) {
            case TOK_LT:  op_str = "<";  break;
            case TOK_GT:  op_str = ">";  break;
            case TOK_LE:  op_str = "<="; break;
            case TOK_GE:  op_str = ">="; break;
            case TOK_EQ:  op_str = "=="; break;
            case TOK_NEQ: op_str = "!="; break;
            default:      op_str = "?";  break;
        }
        char *t = new_temp();
        emit(op_str, arg1, arg2, t);          /* (relop, arg1, arg2, t)   */
        emit("if",   t,    "", true_label);   /* (if,    t,    -,   Ltrue) */
        emit("goto", "",   "", false_label);  /* (goto,  -,    -,   Lfalse)*/
        if (arg1) free(arg1);
        if (arg2) free(arg2);
        free(t);

    /* ── Boolean literal / variable ────────────────────────────────────
     * A bool variable or literal already holds true/false.
     * Format: (if, val, -, Ltrue)  followed by  (goto, -, -, Lfalse)
     * ───────────────────────────────────────────────────────────────── */
    } else {
        char *val = generate_tac(n);
        if (val) {
            emit("if",   val, "", true_label);   /* (if,  val, -, Ltrue)  */
            emit("goto", "",  "", false_label);  /* (goto, -,  -, Lfalse) */
            free(val);
        }
    }
}

/*
 * generate_tac — main recursive TAC emitter.
 * Returns a char* (heap-allocated) naming the result temporary,
 * or NULL for statements that produce no value.
 * Caller must free() the returned pointer.
 */
char* generate_tac(ASTNode *n) {
    if (!n) return NULL;

    switch (n->kind) {

        /* ── Atomic values ──────────────────────────────────────────── */
        case NODE_LITERAL:
        case NODE_VAR_REF:
            return strdup(n->lexeme);

        /* ── Variable declaration: no TAC emitted ────────────────────  */
        case NODE_VAR_DECL:
            return NULL;

        /* ── Binary expression ──────────────────────────────────────── */
        case NODE_BINARY_OP: {
            /* Boolean / relational → materialise result into a temp */
            if (n->op == TOK_AND || n->op == TOK_OR  ||
                n->op == TOK_LT  || n->op == TOK_GT  || n->op == TOK_LE ||
                n->op == TOK_GE  || n->op == TOK_EQ  || n->op == TOK_NEQ) {
                char *tl  = new_label();
                char *fl  = new_label();
                char *el  = new_label();
                char *res = new_temp();
                generate_bool_tac(n, tl, fl);
                emit("label", "",      "", tl);
                emit("=",     "true",  "", res);
                emit("goto",  "",      "", el);
                emit("label", "",      "", fl);
                emit("=",     "false", "", res);
                emit("label", "",      "", el);
                free(tl); free(fl); free(el);
                return res;
            }
            /* Arithmetic: t = arg1 op arg2 */
            char *a1  = generate_tac(n->children[0]);
            char *a2  = generate_tac(n->children[1]);
            char *res = new_temp();
            const char *op_str;
            switch (n->op) {
                case TOK_PLUS:  op_str = "+"; break;
                case TOK_MINUS: op_str = "-"; break;
                case TOK_MUL:   op_str = "*"; break;
                case TOK_DIV:   op_str = "/"; break;
                case TOK_MOD:   op_str = "%"; break;
                default:        op_str = "?"; break;
            }
            emit(op_str, a1, a2, res);
            if (a1) free(a1);
            if (a2) free(a2);
            return res;
        }

        /* ── Unary expression ───────────────────────────────────────── */
        case NODE_UNARY_OP: {
            if (n->op == TOK_NOT) {
                char *tl  = new_label();
                char *fl  = new_label();
                char *el  = new_label();
                char *res = new_temp();
                generate_bool_tac(n, tl, fl);
                emit("label", "",      "", tl);
                emit("=",     "true",  "", res);
                emit("goto",  "",      "", el);
                emit("label", "",      "", fl);
                emit("=",     "false", "", res);
                emit("label", "",      "", el);
                free(tl); free(fl); free(el);
                return res;
            }
            /* Unary minus */
            char *a1  = generate_tac(n->children[0]);
            char *res = new_temp();
            emit("uminus", a1, "", res);
            if (a1) free(a1);
            return res;
        }

        /* ── Assignment: var = expr ─────────────────────────────────── */
        case NODE_ASSIGN: {
            char *rhs = generate_tac(n->children[0]);
            if (rhs) {
                emit("=", rhs, "", n->lexeme);
                free(rhs);
            }
            return NULL;
        }

        /* ── if / if-else ───────────────────────────────────────────── */
        case NODE_IF: {
            char *tl = new_label();   /* true  (then-branch) */
            char *fl = new_label();   /* false (else-branch / exit) */
            char *el = new_label();   /* exit after else */

            generate_bool_tac(n->children[0], tl, fl);

            emit("label", "", "", tl);
            generate_tac(n->children[1]);          /* then-body */

            if (n->child_count == 3) {             /* has else */
                emit("goto",  "", "", el);
                emit("label", "", "", fl);
                generate_tac(n->children[2]);      /* else-body */
                emit("label", "", "", el);
            } else {
                emit("label", "", "", fl);
            }

            free(tl); free(fl); free(el);
            return NULL;
        }

        /* ── while loop ─────────────────────────────────────────────── */
        case NODE_WHILE: {
            char *sl = new_label();   /* loop start (re-test condition) */
            char *bl = new_label();   /* body label */
            char *xl = new_label();   /* exit label */

            emit("label", "", "", sl);               /* L_start: */
            generate_bool_tac(n->children[0], bl, xl); /* test cond */
            emit("label", "", "", bl);               /* L_body: */
            generate_tac(n->children[1]);            /* body */
            emit("goto",  "", "", sl);               /* back to start */
            emit("label", "", "", xl);               /* L_exit: */

            free(sl); free(bl); free(xl);
            return NULL;
        }

        /* ── print(expr) ────────────────────────────────────────────── */
        case NODE_PRINT: {
            char *val = generate_tac(n->children[0]);
            if (val) {
                emit("print", val, "", "");
                free(val);
            }
            return NULL;
        }

        /* ── return ─────────────────────────────────────────────────── */
        case NODE_RETURN: {
            if (n->child_count > 0) {
                char *val = generate_tac(n->children[0]);
                if (val) { emit("return", val, "", ""); free(val); }
            } else {
                emit("return", "", "", "");
            }
            return NULL;
        }

        /* ── Block / StmtList: recurse into all children ────────────── */
        case NODE_BLOCK:
        case NODE_STMT_LIST:
            for (int i = 0; i < n->child_count; i++)
                generate_tac(n->children[i]);
            return NULL;

        /* ── Function definition ─────────────────────────────────────── */
        case NODE_FUNC_DEF: {
            /* Emit a function-begin label then generate the body */
            char label[80];
            sprintf(label, "func_%s", n->lexeme);
            emit("label", "", "", label);
            generate_tac(n->children[1]);          /* body */
            emit("func_end", n->lexeme, "", "");
            return NULL;
        }

        default:
            for (int i = 0; i < n->child_count; i++)
                generate_tac(n->children[i]);
            return NULL;
    }
}

/* ========================================================================
 * MAIN — full 6-phase compiler pipeline
 * ======================================================================== */
int main(int argc, char **argv) {
    char source[16384] = {0};

    printf("========================================\n");
    printf("  WEEK 6 COMPILER — Full Pipeline       \n");
    printf("  Phases 1-6 including TAC Generation   \n");
    printf("========================================\n\n");

    /* ── Read input ──────────────────────────────────────────────────── */
    if (argc > 1) {
        FILE *fp = fopen(argv[1], "r");
        if (!fp) { printf("Could not open file: %s\n", argv[1]); return 1; }
        size_t len = fread(source, 1, sizeof(source)-1, fp);
        source[len] = '\0';
        fclose(fp);
    } else {
        printf("Enter source code (end with Ctrl+D / Ctrl+Z):\n");
        int i = 0, ch;
        while ((ch = getchar()) != EOF && i < (int)sizeof(source)-1)
            source[i++] = (char)ch;
        source[i] = '\0';
    }
    if (strlen(source) == 0) { printf("No input.\n"); return 0; }

    printf("\n----------------------------------------\n");
    printf("INPUT PROGRAM:\n");
    printf("----------------------------------------\n%s\n", source);

    /* ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
     * PHASE 1 — Lexical Analysis
     * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */
    run_lexical_analysis(source);

    /* ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
     * PHASE 2/3 — Syntax Analysis + AST Build
     * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */
    printf("+====================================================================+\n");
    printf("| PHASE 2: SYNTAX ANALYSIS + AST CONSTRUCTION                        |\n");
    printf("| PHASE 3: SYNTAX ERROR DETECTION                                    |\n");
    printf("+====================================================================+\n\n");

    input_ptr    = source;
    current_line = 1;
    advance();

    push_scope();                       /* global scope (level 0) */
    ASTNode *root = parse_StmtList();

    if (current_token.type != TOK_EOF) {
        printf("\n *** Syntax Error: Extraneous tokens after program end: '%s'\n",
               current_token.lexeme);
        return 1;
    }
    printf("\n [OK] Parsing completed — syntax is valid.\n\n");

    /* ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
     * PHASE 4/5 — Symbol Table + Semantic Analysis
     * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */
    printf("+====================================================================+\n");
    printf("| PHASE 4: SYMBOL TABLE CONSTRUCTION                                 |\n");
    printf("| PHASE 5: SEMANTIC ANALYSIS (type checking)                         |\n");
    printf("+====================================================================+\n\n");

    analyze(root);

    if (sem_error_count == 0) {
        printf("\n [OK] Semantic analysis passed — no errors.\n");
    } else {
        printf("\n [FAIL] %d semantic error(s) detected.\n", sem_error_count);
    }

    /* Final symbol-table snapshot */
    printf("\n --- Final Symbol Table (global scope) ---\n");
    print_all_scopes();

    /* ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
     * PHASE 6 — Three-Address Code Generation
     * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */
    generate_tac(root);
    print_tac();
    print_quadruples();

    /* ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
     * Interactive Symbol Table Lookup
     * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */
    printf("\n+====================================================================+\n");
    printf("| INTERACTIVE SYMBOL TABLE LOOKUP                                    |\n");
    printf("| Enter a variable name to look up. Type 'q' to exit.               |\n");
    printf("+====================================================================+\n");
    char search_var[64];
    while (1) {
        printf("\n  Search variable >>> ");
        if (scanf("%63s", search_var) != 1) break;
        if (strcmp(search_var, "q") == 0 || strcmp(search_var, "quit") == 0) break;
        searchSym(top, search_var);
    }

    pop_scope();   /* clean up global scope */
    return 0;
}

/*
 * ============================================================
 * SAMPLE EVALUATION PROGRAM  (save as eval.txt and run):
 * ============================================================
 *
 * int x;
 * int y;
 * float result;
 * bool flag;
 *
 * x = 10;
 * y = 3;
 * flag = true;
 *
 * result = x + y * 2;
 *
 * if (flag) {
 *     result = result + 1;
 * } else {
 *     result = result - 1;
 * }
 *
 * while (x > 0) {
 *     x = x - 1;
 *     print(x);
 * }
 *
 * print(result);
 *
 * ============================================================
 * EXPECTED TAC SNIPPET (from x + y * 2 and the if block):
 *   t1 = y * 2
 *   t2 = x + t1
 *   result = t2
 *   if flag == true goto L1   <- or if condition is relational
 *   goto L2
 * L1:
 *   t3 = result + 1
 *   result = t3
 *   goto L3
 * L2:
 *   t4 = result - 1
 *   result = t4
 * L3:
 *   ...while loop...
 * ============================================================
 */