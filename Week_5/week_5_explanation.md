# `week_5.c` — Iterative Code & Technical Walkthrough

This document provides an exhaustive, line-by-line explanation of the `week_5.c` compiler. Every segment of code is presented alongside a detailed breakdown of its logic, architectural purpose, and implementation details.

---

## Segment 1: File Header & Overview
**Lines 1–25**

```c
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
```

### Detailed Explanation:
- **Line 1-2**: Specifies the project title, highlighting the two major additions for Week 5: **Nested Scope Management** and **AST-based Semantic Analysis**.
- **Line 4-10**: Lists the six phases of the compiler pipeline. Note the shift from one-pass to two-pass (AST traversal).
- **Line 12-16**: Details the specific semantic rules enforced by Phase 5. This includes variable existence, uniqueness, type compatibility, and boolean condition validation.
- **Line 18-20**: Practical notes on the included test cases and how they demonstrate error detection.
- **Line 22-24**: Instructions for executing the program via terminal, supporting both file input and interactive stdin.

---

## Segment 2: Library Includes & Token Definitions
**Lines 26–52**

```c
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
```

### Detailed Explanation:
- **Line 26-29**:
    - `<ctype.h>` used for character classification (isspace, isalpha, etc.) in the lexer.
    - `<stdio.h>` for file I/O and standard console printing.
    - `<stdlib.h>` for dynamic memory allocation (`malloc`, `free`) used in AST and Symbol Table nodes.
    - `<string.h>` for string manipulation (`strcpy`, `strcmp`) to handle token text.
- **Line 34-45 (`TokenType`)**: An enumeration of all terminal symbols the compiler recognizes.
    - **Keywords**: `TOK_INT`, `TOK_BOOL`, etc.
    - **Literals**: `TOK_TRUE`, `TOK_FALSE`, and `TOK_NUM`.
    - **Operators**: Comprehensive list including logical operators (`TOK_AND`, `TOK_OR`).
- **Line 47-51 (`Token` struct)**:
    - `type`: The category of the token from the enum.
    - `lexeme`: The actual string of characters found in the source code (limit 64 chars).
    - `line`: The exact line number in the source where this token was found, used for accurate error reporting.

---

## Segment 3: Symbol Table Structures & Globals
**Lines 53–74**

```c
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
```

### Detailed Explanation:
- **Line 56**: Defines a capacity limit of 128 symbols per scope to prevent overflow while keeping the array-based approach simple.
- **Line 57-62 (`Symbol` struct)**:
    - `name`: The name of the variable or identifier.
    - `type`: The data type, stored as a string for easy comparison.
    - `scope_level`: The depth of the scope where the symbol is defined (0 = global).
    - `offset`: A simulated memory address (increments by 4 bytes per variable).
- **Line 64-69 (`ScopeNode` struct)**: Represents a single level of scope (e.g., a function body or a block).
    - `symbols[]`: An array of all symbols declared in *this specific* scope.
    - `next`: Pointer to the enclosing (outer) scope. This creates a linked list (stack) of scopes.
- **Line 71-74**:
    - `top`: Points to the current innermost scope.
    - `scope_counter`: Used to assign unique IDs to every new scope created.
    - `global_offset`: Tracks simulated memory usage across the entire program.
    - `sem_error_count`: Tracks the total number of errors found during Phase 5.

---

## Segment 4: Scope Management Functions
**Lines 75–101**

```c
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
```

### Detailed Explanation:
- **Line 77-85 (`push_scope`)**:
    - Allocates memory for a new `ScopeNode`.
    - Initializes the symbol count to 0 and assigns a new level from `scope_counter`.
    - **Line 82-83**: Links the new node to the previous `top` and updates `top`. This is a classic "stack push" operation.
- **Line 88-101 (`pop_scope`)**:
    - **Line 90-97**: Before deleting the scope, it prints a diagnostic "symbol dump" of everything defined in that scope. This is crucial for verifying that variables correctly "go out of scope."
    - **Line 98-100**: Moves the `top` pointer back to the outer scope (`top->next`) and frees the memory of the inner scope. This is a "stack pop."

---

## Segment 5: Symbol Insertion & Deep Search
**Lines 102–143**

```c
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
```

### Detailed Explanation:
- **Line 104-123 (`insert_symbol`)**:
    - **Line 106-112**: Iterates only through the current (innermost) scope to check for re-declarations. This allows **variable shadowing** (declaring a variable with the same name in an inner scope as in an outer scope).
    - **Line 115-120**: Sets the properties of the new symbol and advances the `global_offset`.
- **Line 126-142 (`searchSym`)**:
    - Iteratively searches from the provided scope node `S` and moves "upward" through `next` pointers.
    - Because it starts at the inner scope, it correctly finds the *shadowing* variable first if it exists.
    - If it traverses all nodes and finds nothing, it reports an "undeclared variable" state.

---

## Segment 6: Table Printing & Semantic Error Reporter
**Lines 144–175**

```c
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
```

### Detailed Explanation:
- **Line 145-168 (`print_all_scopes`)**:
    - Traverses the entire scope stack from innermost to outermost.
    - Prints a structured table for each scope level, identifying every symbol currently visible or defined at that stage of the program.
- **Line 171-174 (`sem_error`)**:
    - Increments the error counter.
    - Prints a formatted error message that includes the count and line number. This is the primary reporting tool for Phase 5.

---

## Segment 7: AST Structure & Node Creation
**Lines 176–208**

```c
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
```

### Detailed Explanation:
- **Line 179-185 (`NodeKind`)**: Defines all types of "concepts" our Abstract Syntax Tree can represent.
    - `NODE_STMT_LIST`: A sequence of statements.
    - `NODE_BINARY_OP`: Equations (+, -, *, etc.).
    - `NODE_VAR_REF`: Mentioning a variable by name.
    - `NODE_LITERAL`: A hardcoded number or boolean.
- **Line 187-195 (`ASTNode` struct)**:
    - `kind`: The enum above.
    - `children[16]`: Pointers to other nodes (e.g., an `if` node has a condition child and a body child).
    - `lexeme`: Stores the name of a variable or the text of a number.
    - `type_str`: Used to store the *inferred* type during semantic analysis.
- **Line 197-207 (`create_node`)**:
    - Helper function that allocates a new node and zeros out all fields to prevent garbage values from causing crashes.

---

## Segment 8: Lexer State & Comment Skipping
**Lines 209–236**

```c
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
```

### Detailed Explanation:
- **Line 212-214**: Global state for the lexer. `input_ptr` tracks where we are in the source code.
- **Line 217-236 (`advance`)**:
    - The `while(1)` loop at the start is designed to skip "non-token" content.
    - **Line 219-221**: Skips whitespace and updates the line counter.
    - **Line 222-228**: **Block Comment Support**. If it sees `/*`, it enters a inner loop that consumes characters until it sees `*/`. This ensures that comments do not generate tokens.
    - **Line 231**: Stamps the final line number onto the current token before we identify what the token is.

---

## Segment 9: Lexer - Identifiers & Keywords
**Lines 237–265**

```c
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
```

### Detailed Explanation:
- **Line 237-241**: Handles the end of the source string, generating the `TOK_EOF` sentinel.
- **Line 242-264**: **Identifier and Keyword Scanner**.
    - If the first character is a letter or underscore, it consumes all alphanumeric characters into a string.
    - It then performs a series of `strcmp` calls to check if this string matches a **Reserved Keyword** (like `if`, `while`, or `int`).
    - Note the inclusion of `true` and `false` as boolean literals and `bool` as a type keyword.
    - If no keyword matches, it is classified as a general `TOK_ID`.

---

## Segment 10: Lexer - Numeric Literals
**Lines 266–292**

```c
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
```

### Detailed Explanation:
- **Line 266-276**: **Number Scanner**.
    - It handles integers by consuming consecutive digits.
    - If it hits a `.` (dot), it treats the rest of the digits as a fractional part, essentially supporting floating-point numbers.
- **Line 277-292**: **Single-Character Operator Scanner**.
    - Uses a `switch` statement to identify basic punctuation and arithmetic operators.
    - `current_token.lexeme` is initialized to the single character found before the switch.

---

## Segment 11: Lexer - Multi-Character Operators
**Lines 293–317**

```c
        case '=': input_ptr++; if (*input_ptr == '=') { current_token.type = TOK_EQ; strcpy(current_token.lexeme, "=="); input_ptr++; } else current_token.type = TOK_ASSIGN; break;
        case '<': input_ptr++; if (*input_ptr == '=') { current_token.type = TOK_LE; strcpy(current_token.lexeme, "<="); input_ptr++; } else current_token.type = TOK_LT; break;
        case '>': input_ptr++; if (*input_ptr == '=') { current_token.type = TOK_GE; strcpy(current_token.lexeme, ">="); input_ptr++; } else current_token.type = TOK_GT; break;
        case '!': input_ptr++; if (*input_ptr == '=') { current_token.type = TOK_NEQ; strcpy(current_token.lexeme, "!="); input_ptr++; } else current_token.type = TOK_NOT; break;
        case '&': input_ptr++; if (*input_ptr == '&') { current_token.type = TOK_AND; strcpy(current_token.lexeme, "&&"); input_ptr++; } else current_token.type = TOK_ERROR; break;
        case '|': input_ptr++; if (*input_ptr == '|') { current_token.type = TOK_OR; strcpy(current_token.lexeme, "||"); input_ptr++; } else current_token.type = TOK_ERROR; break;
        default: current_token.type = TOK_ERROR; input_ptr++; break;
    }
}
```

### Detailed Explanation:
- **Line 293-298**: Handles **Lookahead** for operators that can have two characters.
    - For example, if it sees `=`, it checks the *next* character. If that is also `=`, it produces `TOK_EQ` (==). If not, it returns `TOK_ASSIGN` (=).
    - Similar logic applies to `<=`, `>=`, `!=`, and the logical `&&` and `||`.
- **Line 299-301**: The `default` case handles any character the lexer cannot identify (like `@` or `$`), marking it as a `TOK_ERROR`.

---

## Segment 12: Lexer Driver & Parser Matcher
**Lines 318–364**

```c
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
```

### Detailed Explanation:
- **Line 318-341 (`run_lexical_analysis`)**:
    - This is Phase 1. It scans the entire source once and prints a summarized table of every token found.
    - It Categorizes tokens as "Identifier", "Number", or "Boolean Literal" for clarity.
- **Line 344-352 (`match`)**:
    - A critical utility for recursive descent parsing.
    - If the current token matches what the grammar expected, it moves to the next token.
    - If there is a mismatch, the program stops immediately with a **Syntax Error**, as the grammar has been violated.

---

## Segment 13: Factor Parsing
**Lines 353–385**

```c
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
```

### Detailed Explanation:
- **Line 353-385 (`parse_Factor`)**:
    - The most basic unit of an expression.
    - **Variable Reference (357-360)**: Creates a `NODE_VAR_REF` node storing the variable's name.
    - **Literal Values (361-370)**: Parses numbers and booleans, assigning an initial "type string" (like "int" or "float") to help Phase 5.
    - **Parentheses (371-374)**: Recursively handles nested expressions.
    - **Unary NOT (375-380)**: Handles the logical NOT operator `!`.

---

## Segment 14: Term & Arithmetic Expression Parsing
**Lines 386–431**

```c
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
```

### Detailed Explanation:
- **Line 386-397 (`parse_Term`)**:
    - Parses symbols that bind tightly (Multiplication, Division, Modulo).
    - It implements **Left-Associativity** by repeatedly grabbing the current `left` node and making it a child of a new operator node.
- **Line 399-410 (`parse_ArithExpr`)**:
    - Parses Addition and Subtraction.
    - It calls `parse_Term()`, which ensures that multiplication/division is calculated *before* addition/subtraction (standard PEMDAS).

---

## Segment 15: Relational & Equality Parsing
**Lines 416–445**

```c
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
```

### Detailed Explanation:
- **Line 411-422 (`parse_RelExpr`)**:
    - Handles `<, >, <=, >=`.
    - Note that arithmetic expressions are evaluated first on both sides before the comparison happens.
- **Line 424-434 (`parse_EqExpr`)**:
    - Handles `==` and `!=`. This further lowers the precedence (comparisons happen before equality checks).

---

## Segment 16: Logical Expressions & Statement Entry
**Lines 446–475**

```c
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
```

### Detailed Explanation:
- **Line 436-447 (`parse_AndExpr`)**: Handles the logical `&&` operator.
- **Line 449-460 (`parse_BoolExpr`)**: Handles the logical `||` operator. This is the top of the expression hierarchy; it has the lowest precedence.
- **Line 462-463**: Forward declarations for statement parsers, allowing mutual recursion (e.g., an `if` block can contain a list of `Stmt`s).

---

## Segment 17: Declarations vs Function Definitions
**Lines 476–505**

```c
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
```

### Detailed Explanation:
- **Line 465-475**: Checks for typed statements (declarations). It captures the base type (int, float, bool, or void).
- **Line 476-480**: Identifies the variable or function name.
- **Line 482-505**: **The Dispatcher**.
    - If the next token is `(`, this is a **Function Definition**.
    - It creates a `NODE_FUNC_DEF` and then handles the **Parameter List** inside the parentheses.
    - Each parameter is treated as a `NODE_VAR_DECL` and stored in a specialized `NODE_PARAM_LIST` parent.

---

## Segment 18: Variable Initialization & Function Body
**Lines 506–535**

```c
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
```

### Detailed Explanation:
- **Line 506-512**: Completes the function definition by parsing the block of code (the `StmtList`) inside the curly braces.
- **Line 514-535**: **Variable Declarations**.
    - Specifically handles **Comma-Separated lists** (e.g., `int x, y, z;`). It creates a separate `NODE_VAR_DECL` for every name.
    - Also handles **inline initialization** (e.g., `int x = 10;`). If an `=` is present, it adds a `NODE_ASSIGN` to the statement list, essentially splitting the declaration and assignment into two AST nodes.

---

## Segment 19: Assignments & Labels
**Lines 536–565**

```c
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
```

### Detailed Explanation:
- **Line 538-551**: Handles identifiers that are not at the start of a declaration.
    - If followed by `:`, it builds a `NODE_LABEL`.
- **Line 552-563**: Handles standard **Assignment Statements** (e.g., `x = 20;`).
    - Creates a `NODE_ASSIGN` node. The RHS of the assignment is parsed as a full `BoolExpr` and attached as child 0.
- **Line 564-567**: Handles "Dangling Statements" that end in a semicolon but do nothing. These return `NULL` to avoid adding clutter to the AST.

---

## Segment 20: If Statements & While Loops
**Lines 566–595**

```c
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
```

### Detailed Explanation:
- **Line 569-583 (`NODE_IF`)**:
    - Child 0 = The condition (between `(` and `)`).
    - Child 1 = The "then" block (between `{` and `}`).
    - **Line 577-581**: If an `else` keyword exists, it parses a second block and attaches it as Child 2.
- **Line 584-594 (`NODE_WHILE`)**:
    - Child 0 = The condition.
    - Child 1 = The loop body block.

---

## Segment 21: Print, Class, & Return
**Lines 596–625**

```c
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
```

### Detailed Explanation:
- **Line 596-602 (`NODE_PRINT`)**: Represents a print statement. The content to be printed is parsed as an expression and attached as Child 0.
- **Line 603-611 (`NODE_CLASS_DEF`)**: Parses a class definition. Note that everything inside the class is stored in a `NODE_STMT_LIST` as Child 0.
- **Line 612-619 (`NODE_RETURN`)**: Parses return statements. It allows for an optional expression (child 0). If no expression is provided (e.g., `return;`), the node simply has zero children.

---

## Segment 22: Blocks & Statement List Closure
**Lines 626–655**

```c
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
```

### Detailed Explanation:
- **Line 620-625 (`NODE_BLOCK`)**: Handles standalone nested blocks `{ ... }`. This is important for **Scope Level Increments**.
- **Line 628-636 (`parse_StmtList`)**:
    - The "Engine" of the parser. It keeps calling `parse_Stmt` until it hits a `}` or the end of the file.
    - Each statement is appended to the `children` array of the `NODE_STMT_LIST`.
- **Line 641-642 (Helper Functions)**: Provides simple string-matching helpers to differentiate between numeric types (int, float) and boolean types during semantic analysis.

---

## Segment 23: Semantic Analysis - Expression Type Inference
**Lines 656–685**

```c
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
```

### Detailed Explanation:
- **Line 644-685 (`eval_type`)**: This is Phase 5's recursive pass. It determines the resulting "type" of any part of the expression tree.
    - **Line 646**: Literal nodes already have their type assigned by the parser.
    - **Line 647-653**: **Undeclared Variable Check**. If a variable reference is found, it calls `searchSym()`. 
    - If the variable isn't in any visible scope, it generates a **Semantic Error**.

---

## Segment 24: Semantic Analysis - Operation Validation
**Lines 686–715**

```c
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
```

### Detailed Explanation:
- **Line 654-666 (Arithmetic Checks)**:
    - For operators like `+` or `*`, it evaluates the types of both children.
    - If either child is NOT a number, it reports a **Type Mismatch Error**.
    - If both are numbers, it promotes the type to "float" if either side is a float; otherwise, it's an "int".
- **Line 668-670 (Relational Checks)**: Logical AND, OR, and comparisons always evaluate to a "bool" type, regardless of the input types (provided they are compatible).

---

## Segment 25: Semantic Analysis - Visitor Pass (Part A)
**Lines 716–745**

```c
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
```

### Detailed Explanation:
- **Line 673-715 (`analyze`)**: The main traversal visitor. It walks through every node and enforces semantic laws.
- **Line 677-679 (`NODE_STMT_LIST`)**: Simply traverses into every statement in the list.
- **Line 680-682 (`NODE_VAR_DECL`)**: **Declaration Pass**. Calls `insert_symbol()`, which will automatically check for **Multiple Declaration Errors** (duplicate names in the same scope).
- **Line 683-689 (`NODE_FUNC_DEF`)**: 
    - Inserts the function name as type "proc".
    - Calls `push_scope()`, then visits the parameters and body inside that new scope, then `pop_scope()`. This is how **Function Scoping** is implemented.

---

## Segment 26: Semantic Analysis - Visitor Pass (Part B)
**Lines 746–775**

```c
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
```

### Detailed Explanation:
- **Line 694-704**: Handles `NODE_BLOCK` and `NODE_CLASS_DEF`. Both create a new scope and perform a "final snapshot" of that scope before discarding it.
- **Line 705-717 (`NODE_ASSIGN`)**: **Assignment Compatibility Check**.
    - It evaluates the RHS type using `eval_type()`.
    - It enforces that `LHS:int` cannot receive a `RHS:float` (loss of precision).
    - It allows `LHS:float` to receive a `RHS:int` (widening).
    - It ensures `bool` variables only receive `bool` values.

---

## Segment 27: Semantic Analysis - Conditional & Main Driver
**Lines 776–805**

```c
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
```

### Detailed Explanation:
- **Line 719-725**: **Condition Validation**.
    - For `if` and `while`, it evaluates Child 0 (the condition expression).
    - If the result is NOT "bool", it generates a **Semantic Error**. This fulfills the requirement that control flow conditions must be strictly boolean.
- **Line 726-727**: After validating the condition, it proceeds to check the contents of the loop or if blocks.
- **Line 737**: Header for the final driver.

---

## Segment 28: Input Reading & Lexical Pass
**Lines 806–835**

```c
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
```

### Detailed Explanation:
- **Line 741-758**: Handles program input. It can read from a file passed as a Command Line Argument (argv[1]) or from direct User keyboard input. It loads the source into a zero-initialized buffer.
- **Line 761**: Initiates **Phase 1** (Lexical analysis pass), printing the summary table and categorizing tokens.

---

## Segment 29: Syntax & AST Construction Pass
**Lines 836–865**

```c
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
```

### Detailed Explanation:
- **Line 764-771**: Resets the lexer state to start **Phase 2** (Parsing).
- **Line 773-774**: Manually pushes the **Global Scope** and starts the recursive descent from `parse_StmtList()`. This returns the root of the entire program's tree.
- **Line 776-783**: Verifies that the parser consumed the *entire* input. If extra characters (tokens) remain after the valid program structure, a **Syntax Error** is reported for extraneous tokens.

---

## Segment 30: Semantic Pass & Final Summary
**Lines 866–End**

```c
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
```

### Detailed Explanation:
- **Line 787-789**: Initiates **Phase 4 & 5**. It traverses the AST root created in Phase 2. This is where all the logic for types, scopes, and variable checks actually runs.
- **Line 791-799**: Prints the final verdict of the Semantic analyzer.
- **Line 802-803**: Prints the **Final Global Symbol Table** (Phase 4 final result).
- **Line 806-817**: Starts the **Interactive Phase 6**. It allows the user to manually type variable names to see their type and memory offset, verifying that the symbol table was correctly built and persisted.
- **Line 819**: Pops the global scope and cleans up memory before exiting.
