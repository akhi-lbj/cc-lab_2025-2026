/*
 * Week 5: Symbol Table with Nested Scope Management
 * ---------------------------------------------------
 * Combines:
 *   Phase 1 - Lexical Analysis   (Week 1)
 *   Phase 2 - Syntax Analysis    (Week 2/3)
 *   Phase 3 - Syntax Error Detection (Week 3)
 *   Phase 4 - Symbol Table with Nested Block Scopes (Week 5 - main feature)
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
    TOK_INT, TOK_FLOAT, TOK_IF, TOK_ELSE, TOK_WHILE, TOK_PRINT,
    TOK_LPAREN, TOK_RPAREN, TOK_LBRACE, TOK_RBRACE,
    TOK_ID, TOK_NUM,
    TOK_ASSIGN, TOK_PLUS, TOK_MINUS, TOK_MUL, TOK_DIV, TOK_MOD,
    TOK_LT, TOK_GT, TOK_LE, TOK_GE, TOK_EQ, TOK_NEQ,
    TOK_AND, TOK_OR, TOK_NOT,
    TOK_SEMI, TOK_EOF, TOK_ERROR
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
    printf("  [ST] <<< Leaving scope (level %d) — dropping %d symbol(s):\n",
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
 */
int searchSym(ScopeNode *S, const char *var) {
    ScopeNode *cur = S;
    while (cur) {
        for (int i = 0; i < cur->count; i++) {
            if (strcmp(cur->symbols[i].name, var) == 0) {
                printf("  [ST] LOOKUP '%s': FOUND in scope %d — type=%s, offset=%d\n",
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
    printf("\n  ╔══════════════════════════════════════════════════╗\n");
    printf("  ║        CURRENT SYMBOL TABLE SNAPSHOT             ║\n");
    printf("  ╠══════════════════════════════════════════════════╣\n");
    ScopeNode *cur = top;
    if (!cur) {
        printf("  ║  (empty — no active scopes)                     ║\n");
    }
    while (cur) {
        printf("  ║  Scope Level %d  (%d symbols)                     \n",
               cur->scope_level, cur->count);
        printf("  ║  %-10s %-8s %-6s %-8s\n", "Name", "Type", "Scope", "Offset");
        printf("  ║  %-10s %-8s %-6s %-8s\n", "────────", "──────", "─────", "──────");
        for (int i = 0; i < cur->count; i++) {
            printf("  ║  %-10s %-8s %-6d %-8d\n",
                   cur->symbols[i].name,
                   cur->symbols[i].type,
                   cur->symbols[i].scope_level,
                   cur->symbols[i].offset);
        }
        if (cur->next)
            printf("  ║  ── next ──> (enclosing scope level %d)\n", cur->next->scope_level);
        cur = cur->next;
    }
    printf("  ╚══════════════════════════════════════════════════╝\n\n");
}

/* ========================================================================
 *  PART 4 — LEXICAL ANALYZER
 * ======================================================================== */

const char* token_type_to_string(TokenType type) {
    switch (type) {
        case TOK_INT:    return "int";
        case TOK_FLOAT:  return "float";
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
        case TOK_EOF:    return "EOF";
        default:         return "Unknown";
    }
}

const char* token_category(TokenType type) {
    switch (type) {
        case TOK_INT: case TOK_FLOAT: case TOK_IF: case TOK_ELSE:
        case TOK_WHILE: case TOK_PRINT:
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
        case TOK_RBRACE: case TOK_SEMI:
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

        if      (strcmp(current_token.lexeme, "if")    == 0)  current_token.type = TOK_IF;
        else if (strcmp(current_token.lexeme, "else")  == 0)  current_token.type = TOK_ELSE;
        else if (strcmp(current_token.lexeme, "while") == 0)  current_token.type = TOK_WHILE;
        else if (strcmp(current_token.lexeme, "print") == 0)  current_token.type = TOK_PRINT;
        else if (strcmp(current_token.lexeme, "int")   == 0)  current_token.type = TOK_INT;
        else if (strcmp(current_token.lexeme, "float") == 0)  current_token.type = TOK_FLOAT;
        else                                                  current_token.type = TOK_ID;
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
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║            PHASE 1: LEXICAL ANALYSIS                       ║\n");
    printf("╠══════════════════════════════════════════════════════════════╣\n");
    printf("║  %-6s  %-14s  %-18s  %-6s  ║\n", "No.", "Lexeme", "Category", "Line");
    printf("╠══════════════════════════════════════════════════════════════╣\n");

    input_ptr    = src;
    current_line = 1;
    token_list.count = 0;

    advance();
    int idx = 1;
    while (current_token.type != TOK_EOF) {
        if (current_token.type == TOK_ERROR) {
            printf("║  %-6d  %-14s  %-18s  %-6d  ║  *** LEXICAL ERROR\n",
                   idx, current_token.lexeme, "ERROR", current_token.line);
        } else {
            printf("║  %-6d  %-14s  %-18s  %-6d  ║\n",
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

    printf("╠══════════════════════════════════════════════════════════════╣\n");
    printf("║  Total tokens: %-4d                                        ║\n", idx - 1);
    printf("╚══════════════════════════════════════════════════════════════╝\n\n");
}

/* ========================================================================
 *  PART 5 — SYNTAX ANALYSIS (Recursive-Descent Parser)
 *           + SYMBOL TABLE updates during parsing
 * ======================================================================== */

/* --- AST Node --- */
typedef struct Node {
    char name[64];
    struct Node* children[20];
    int num_children;
    int is_terminal;
} Node;

/* Forward declarations */
Node* parse_BoolExpr(void);
Node* parse_StmtList(void);
Node* parse_Stmt(void);
Node* parse_Block(void);

Node* create_node(const char* name, int is_terminal) {
    Node* n = (Node*)malloc(sizeof(Node));
    strncpy(n->name, name, 63); n->name[63] = '\0';
    n->num_children = 0;
    n->is_terminal  = is_terminal;
    return n;
}

void add_child(Node* parent, Node* child) {
    if (child && parent->num_children < 20)
        parent->children[parent->num_children++] = child;
}

void free_tree(Node* n) {
    if (!n) return;
    for (int i = 0; i < n->num_children; i++)
        free_tree(n->children[i]);
    free(n);
}

/* match: consume a token of expected type, report syntax error otherwise */
Node* match(TokenType expected) {
    if (current_token.type == expected) {
        Node* n = create_node(current_token.lexeme, 1);
        if (expected == TOK_ID) {
            char buf[64]; sprintf(buf, "ID(%s)", current_token.lexeme);
            strcpy(n->name, buf);
        } else if (expected == TOK_NUM) {
            char buf[64]; sprintf(buf, "NUM(%s)", current_token.lexeme);
            strcpy(n->name, buf);
        }
        advance();
        return n;
    } else {
        printf("\n  *** Syntax Error on line %d: Expected %s, but encountered '%s'\n",
               current_token.line, token_type_to_string(expected), current_token.lexeme);
        exit(1);
    }
}

/* ----- Expression Parsers ----- */

Node* parse_Factor(void) {
    Node* n = create_node("Factor", 0);
    if (current_token.type == TOK_ID) {
        /* Variable usage — look it up in symbol table */
        searchSym(top, current_token.lexeme);
        char buf[64]; sprintf(buf, "ID(%s)", current_token.lexeme);
        Node* c = create_node(buf, 1);
        add_child(n, c);
        advance();
    } else if (current_token.type == TOK_NUM) {
        char buf[64]; sprintf(buf, "NUM(%s)", current_token.lexeme);
        Node* c = create_node(buf, 1);
        add_child(n, c);
        advance();
    } else if (current_token.type == TOK_LPAREN) {
        add_child(n, match(TOK_LPAREN));
        add_child(n, parse_BoolExpr());
        add_child(n, match(TOK_RPAREN));
    } else if (current_token.type == TOK_NOT) {
        add_child(n, match(TOK_NOT));
        add_child(n, parse_Factor());
    } else {
        printf("\n  *** Syntax Error on line %d: Expected ID, NUM, '(' or '!', "
               "but encountered '%s'\n", current_token.line, current_token.lexeme);
        exit(1);
    }
    return n;
}

Node* parse_Term(void) {
    Node* left = parse_Factor();
    while (current_token.type == TOK_MUL ||
           current_token.type == TOK_DIV ||
           current_token.type == TOK_MOD) {
        Node* n = create_node("Term", 0);
        add_child(n, left);
        add_child(n, match(current_token.type));
        add_child(n, parse_Factor());
        left = n;
    }
    Node* n = create_node("Term", 0);
    add_child(n, left);
    return n;
}

Node* parse_ArithExpr(void) {
    Node* left = parse_Term();
    while (current_token.type == TOK_PLUS ||
           current_token.type == TOK_MINUS) {
        Node* n = create_node("ArithExpr", 0);
        add_child(n, left);
        add_child(n, match(current_token.type));
        add_child(n, parse_Term());
        left = n;
    }
    Node* n = create_node("ArithExpr", 0);
    add_child(n, left);
    return n;
}

Node* parse_RelExpr(void) {
    Node* left = parse_ArithExpr();
    while (current_token.type == TOK_LT || current_token.type == TOK_GT ||
           current_token.type == TOK_LE || current_token.type == TOK_GE) {
        Node* n = create_node("RelExpr", 0);
        add_child(n, left);
        add_child(n, match(current_token.type));
        add_child(n, parse_ArithExpr());
        left = n;
    }
    Node* n = create_node("RelExpr", 0);
    add_child(n, left);
    return n;
}

Node* parse_EqExpr(void) {
    Node* left = parse_RelExpr();
    while (current_token.type == TOK_EQ || current_token.type == TOK_NEQ) {
        Node* n = create_node("EqExpr", 0);
        add_child(n, left);
        add_child(n, match(current_token.type));
        add_child(n, parse_RelExpr());
        left = n;
    }
    Node* n = create_node("EqExpr", 0);
    add_child(n, left);
    return n;
}

Node* parse_AndExpr(void) {
    Node* left = parse_EqExpr();
    while (current_token.type == TOK_AND) {
        Node* n = create_node("AndExpr", 0);
        add_child(n, left);
        add_child(n, match(current_token.type));
        add_child(n, parse_EqExpr());
        left = n;
    }
    Node* n = create_node("AndExpr", 0);
    add_child(n, left);
    return n;
}

Node* parse_BoolExpr(void) {
    Node* left = parse_AndExpr();
    while (current_token.type == TOK_OR) {
        Node* n = create_node("BoolExpr", 0);
        add_child(n, left);
        add_child(n, match(current_token.type));
        add_child(n, parse_AndExpr());
        left = n;
    }
    Node* n = create_node("BoolExpr", 0);
    add_child(n, left);
    return n;
}

/* ----- Statement Parsers (with Symbol Table integration) ----- */

Node* parse_DeclStmt(void) {
    Node* n = create_node("DeclStmt", 0);
    if (current_token.type == TOK_INT || current_token.type == TOK_FLOAT) {
        char declared_type[16];
        strcpy(declared_type, current_token.lexeme);   /* "int" or "float" */
        add_child(n, match(current_token.type));

        /* Capture the variable name before matching */
        char var_name[64];
        strncpy(var_name, current_token.lexeme, 63); var_name[63] = '\0';
        add_child(n, match(TOK_ID));

        /* ── INSERT into symbol table ── */
        insert_symbol(var_name, declared_type);

        /* Optional initialisation:  int x = expr; */
        if (current_token.type == TOK_ASSIGN) {
            add_child(n, match(TOK_ASSIGN));
            add_child(n, parse_BoolExpr());
        }
        add_child(n, match(TOK_SEMI));
    }
    return n;
}

Node* parse_AssignStmt(void) {
    Node* n = create_node("AssignStmt", 0);

    /* Left-hand side — look up variable being assigned to */
    searchSym(top, current_token.lexeme);

    add_child(n, match(TOK_ID));
    add_child(n, match(TOK_ASSIGN));
    add_child(n, parse_BoolExpr());
    add_child(n, match(TOK_SEMI));
    return n;
}

Node* parse_Block(void) {
    Node* n = create_node("Block", 0);

    /* '{' → push a new scope onto the symbol-table stack */
    add_child(n, match(TOK_LBRACE));
    push_scope();

    add_child(n, parse_StmtList());

    /* '}' → pop the current scope */
    print_all_scopes();                /* snapshot before we leave */
    pop_scope();
    add_child(n, match(TOK_RBRACE));

    return n;
}

Node* parse_IfStmt(void) {
    Node* n = create_node("IfStmt", 0);
    add_child(n, match(TOK_IF));
    add_child(n, match(TOK_LPAREN));
    add_child(n, parse_BoolExpr());
    add_child(n, match(TOK_RPAREN));
    add_child(n, parse_Block());

    if (current_token.type == TOK_ELSE) {
        add_child(n, match(TOK_ELSE));
        add_child(n, parse_Block());
    }
    return n;
}

Node* parse_WhileStmt(void) {
    Node* n = create_node("WhileStmt", 0);
    add_child(n, match(TOK_WHILE));
    add_child(n, match(TOK_LPAREN));
    add_child(n, parse_BoolExpr());
    add_child(n, match(TOK_RPAREN));
    add_child(n, parse_Block());
    return n;
}

Node* parse_PrintStmt(void) {
    Node* n = create_node("PrintStmt", 0);
    add_child(n, match(TOK_PRINT));
    add_child(n, match(TOK_LPAREN));
    add_child(n, parse_BoolExpr());
    add_child(n, match(TOK_RPAREN));
    add_child(n, match(TOK_SEMI));
    return n;
}

Node* parse_Stmt(void) {
    Node* n = create_node("Stmt", 0);
    if (current_token.type == TOK_INT || current_token.type == TOK_FLOAT) {
        add_child(n, parse_DeclStmt());
    } else if (current_token.type == TOK_ID) {
        add_child(n, parse_AssignStmt());
    } else if (current_token.type == TOK_IF) {
        add_child(n, parse_IfStmt());
    } else if (current_token.type == TOK_WHILE) {
        add_child(n, parse_WhileStmt());
    } else if (current_token.type == TOK_PRINT) {
        add_child(n, parse_PrintStmt());
    } else if (current_token.type == TOK_LBRACE) {
        add_child(n, parse_Block());
    } else {
        printf("\n  *** Syntax Error on line %d: Unexpected token '%s'\n",
               current_token.line, current_token.lexeme);
        exit(1);
    }
    return n;
}

Node* parse_StmtList(void) {
    Node* n = create_node("StmtList", 0);
    while (current_token.type != TOK_RBRACE && current_token.type != TOK_EOF) {
        add_child(n, parse_Stmt());
    }
    return n;
}

/* ========================================================================
 *  PART 6 — INDENTED PARSE TREE PRINTER
 * ======================================================================== */

void print_tree(Node* n, int depth, FILE* out) {
    if (!n) return;
    for (int i = 0; i < depth; i++) { printf("  "); if (out) fprintf(out, "  "); }
    if (n->is_terminal) {
        printf("[%s]\n", n->name);
        if (out) fprintf(out, "[%s]\n", n->name);
    } else {
        printf("%s\n", n->name);
        if (out) fprintf(out, "%s\n", n->name);
    }
    for (int i = 0; i < n->num_children; i++)
        print_tree(n->children[i], depth + 1, out);
}

/* ========================================================================
 *  PART 7 — MAIN
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
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║  PHASE 2: SYNTAX ANALYSIS  +  PHASE 3: ERROR DETECTION     ║\n");
    printf("║  PHASE 4: SYMBOL TABLE CONSTRUCTION (during parsing)       ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n\n");

    /* Reset lexer state for second pass */
    input_ptr    = source;
    current_line = 1;
    advance();

    /* Create the global (top-level) scope — scope 0 */
    push_scope();

    if (current_token.type != TOK_EOF) {
        Node* ast = parse_StmtList();

        if (current_token.type == TOK_EOF) {
            printf("\n══════════════════════════════════════════════════════\n");
            printf("  ✓ Parsing completed successfully — syntax is valid.\n");
            printf("══════════════════════════════════════════════════════\n");

            /* Final symbol table snapshot */
            printf("\n  --- Final Symbol Table (global scope) ---\n");
            print_all_scopes();

            /* Print parse tree */
            printf("╔══════════════════════════════════════════════════════════════╗\n");
            printf("║                    PARSE TREE                              ║\n");
            printf("╚══════════════════════════════════════════════════════════════╝\n");
            FILE* treefile = fopen("parse_tree.txt", "w");
            print_tree(ast, 0, treefile);
            if (treefile) fclose(treefile);
            printf("\n[Parse tree saved to parse_tree.txt]\n");
        } else {
            printf("\n  *** Syntax Error on line %d: Extraneous tokens — found '%s'\n",
                   current_token.line, current_token.lexeme);
        }
        free_tree(ast);
    }

    /* Pop the global scope */
    pop_scope();

    return 0;
}
