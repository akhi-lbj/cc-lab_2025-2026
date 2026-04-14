# `week_5.c` — Complete Code Walkthrough

> **File:** `Week_5/week_5.c` · **Total lines:** 813  
> **Language:** C (C99)  
> **Author:** 2023A7PS0013U Akhil

---

## How to Run

### Step 1 — Compile
Open a terminal (PowerShell, Command Prompt, or any shell with GCC available) and navigate to the `Week_5` directory:

```powershell
cd "c:\Akhil\BITSian\BITS\Junior\6th Sem\CC\Lab\Lab_Assignment\Week_5"
gcc -Wall -o week_5.exe week_5.c
```

> Add `-Wall` for all warnings and `-g` if you want to debug with GDB.

### Step 2 — Run with a source file (recommended)
The program accepts your mini-language program as a file argument:

```powershell
.\week_5.exe eval.txt
```

`eval.txt` is the evaluation program (included in the folder) that contains declarations, assignments, a `while` loop, `if/else`, and `print` statements.

### Step 3 — Run interactively (type code manually)
If you pass no argument, the program reads from standard input:

```powershell
.\week_5.exe
```
Then type your code and press **Ctrl+Z** (Windows) or **Ctrl+D** (Linux/Mac) to signal EOF.

### Step 4 — What you will see
The program prints four phases to the terminal:

| Phase | Output |
|-------|--------|
| **Phase 1** | A formatted token table (lexeme, category, line number) |
| **Phase 2** | `[ST]` symbol-table events (INSERT, LOOKUP, scope push/pop) |
| **Phase 3** | Syntax errors with line numbers (program exits on first error) |
| **Phase 4** | Full indented parse tree, also saved to `parse_tree.txt` |

---

## Architecture Overview

```
week_5.c
├── PART 1  — Token Definitions       (lines   1–51)
├── PART 2  — Symbol Table Structures (lines  53–71)
├── GLOBALS                           (lines  73–84)
├── PART 3  — Symbol Table Operations (lines  86–198)
│   ├── push_scope()
│   ├── pop_scope()
│   ├── insert_symbol()
│   ├── searchSym()
│   └── print_all_scopes()
├── PART 4  — Lexical Analyzer        (lines 200–364)
│   ├── token_type_to_string()
│   ├── token_category()
│   └── advance()
├── PART 4b — Lexical Analysis Pass   (lines 366–414)
│   └── run_lexical_analysis()
├── PART 5  — Syntax Analysis/Parser  (lines 416–714)
│   ├── AST Node struct + helpers
│   ├── match()
│   ├── parse_Factor / Term / ArithExpr / RelExpr / EqExpr
│   ├── parse_AndExpr / BoolExpr
│   └── parse_DeclStmt / AssignStmt / Block / IfStmt
│       parse_WhileStmt / PrintStmt / Stmt / StmtList
├── PART 6  — Parse Tree Printer      (lines 716–732)
│   └── print_tree()
└── PART 7  — main()                  (lines 734–813)
```

---

## Line-by-Line Explanation

---

### Lines 1–26 — File Header Comment

```c
/*
 * Week 5: Symbol Table with Nested Scope Management
 * ...
 * Symbol Table Design (linked-list of scope nodes):
 *   When '{' is processed:
 *       i)   S = top
 *       ii)  top = new ST node
 *       iii) top.next = S
 *   When '}' is processed:
 *       i)   save = top.next
 *       ii)  delete top
 *       iii) top = save
 */
```

This block is a **multi-line block comment** — the C compiler ignores everything between `/*` and `*/`. It serves as a documentation header for the entire file. It describes the **four compiler phases** the program implements: Lexical Analysis (tokenising raw text), Syntax Analysis (checking grammar), Syntax Error Detection (reporting grammar violations), and the Symbol Table (the main Week 5 feature). The comment also documents the exact **algorithm for scope management**: when a `{` is encountered the program saves the current top of the scope stack into a temporary `S`, allocates a brand-new scope node `nn`, and links `nn->next = S` so the chain is preserved; when `}` is encountered the current top is freed and the pointer is restored to the previous node. Finally, the **usage instructions** are embedded right in the file header so any reader immediately knows how to invoke the binary.

---

### Lines 28–31 — Standard Library Includes

```c
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
```

These four `#include` directives pull in C standard library headers. **`<ctype.h>`** provides character-classification functions like `isspace()`, `isalpha()`, `isdigit()`, and `isalnum()` — all essential for the lexer. **`<stdio.h>`** provides I/O functions (`printf`, `fprintf`, `fopen`, `fclose`, `fread`, `getchar`) used throughout for printing output and reading files. **`<stdlib.h>`** is needed for dynamic memory management: `malloc()` (to create AST nodes and scope nodes) and `free()` (to destroy them). **`<string.h>`** provides string utilities like `strcmp()`, `strcpy()`, `strncpy()`, and `strlen()` — all used extensively by the lexer and symbol table to compare and copy token lexemes and variable names.

---

### Lines 33–45 — Token Type Enumeration

```c
typedef enum {
    TOK_INT, TOK_FLOAT, TOK_IF, TOK_ELSE, TOK_WHILE, TOK_PRINT,
    TOK_LPAREN, TOK_RPAREN, TOK_LBRACE, TOK_RBRACE,
    TOK_ID, TOK_NUM,
    TOK_ASSIGN, TOK_PLUS, TOK_MINUS, TOK_MUL, TOK_DIV, TOK_MOD,
    TOK_LT, TOK_GT, TOK_LE, TOK_GE, TOK_EQ, TOK_NEQ,
    TOK_AND, TOK_OR, TOK_NOT,
    TOK_SEMI, TOK_EOF, TOK_ERROR
} TokenType;
```

`typedef enum { ... } TokenType;` creates a new **named integer type** where each name is automatically assigned a distinct integer starting from 0. This is the **complete vocabulary** of the mini-language the compiler understands. The first row covers **keywords**: `int`, `float`, `if`, `else`, `while`, `print`. The second row covers **delimiters**: left/right parentheses and left/right curly braces. `TOK_ID` represents any user-defined identifier (variable name), while `TOK_NUM` represents any numeric literal (integer or float). The third row covers the **assignment operator** `=` and the five **arithmetic operators** `+`, `-`, `*`, `/`, `%`. The fourth row covers the six **relational/comparison operators**: `<`, `>`, `<=`, `>=`, `==`, `!=`. The fifth row covers the three **logical operators**: `&&`, `||`, `!`. Finally, `TOK_SEMI` is the semicolon `;`, `TOK_EOF` signals end of input, and `TOK_ERROR` is used for any character the lexer cannot classify.

---

### Lines 47–51 — Token Structure

```c
typedef struct {
    TokenType type;
    char lexeme[64];
    int  line;
} Token;
```

This `struct` is the **data unit** that the lexer produces and the parser consumes. It has three fields. **`type`** is a `TokenType` enum value identifying what kind of token this is (e.g. `TOK_INT`, `TOK_ID`). **`lexeme[64]`** is a fixed-size character array holding the actual text of the token as it appeared in the source — for example, `"while"`, `"avg"`, or `"15"`. The 64-byte limit is more than enough for any reasonable identifier or number in the mini-language. **`line`** is an integer recording which line of the source file this token was found on — this is crucial for reporting **syntax errors with accurate line numbers** later. The global variable `current_token` of this type holds the token most recently produced by the lexer.

---

### Lines 53–64 — Symbol Table: `Symbol` Structure

```c
#define MAX_SYMBOLS 128   /* max symbols per scope node */

typedef struct {
    char name[64];
    char type[16];        /* "int" or "float" */
    int  scope_level;
    int  offset;          /* memory offset in bytes */
} Symbol;
```

`#define MAX_SYMBOLS 128` sets a **compile-time constant** — each scope node can hold at most 128 symbol entries. The `Symbol` struct is the **fundamental record** stored for every declared variable. **`name[64]`** holds the variable's identifier text (e.g. `"temp"`, `"avg"`). **`type[16]`** stores the declared type as a string — either `"int"` or `"float"`. **`scope_level`** is an integer recording which scope level this variable belongs to — `0` for global, `1` for the first nested block, and so on. **`offset`** is the simulated **memory address** of this variable; it increments by 4 bytes for every new variable (since both `int` and `float` are 4 bytes wide in this model), representing how a real compiler would lay out variables in a stack frame.

---

### Lines 66–71 — Symbol Table: `ScopeNode` Structure

```c
typedef struct ScopeNode {
    Symbol          symbols[MAX_SYMBOLS];
    int             count;
    int             scope_level;
    struct ScopeNode *next;   /* points to enclosing (parent) scope */
} ScopeNode;
```

This is the **core data structure** of the Week 5 feature. A `ScopeNode` represents one **block scope** (one set of curly braces). It contains an array of up to `MAX_SYMBOLS` (128) `Symbol` records, a **`count`** of how many are currently stored, and its own **`scope_level`** identifier. The most important field is **`next`**: a self-referential pointer to the `ScopeNode` that *encloses* this one. This makes the entire symbol table a **singly-linked list of scopes**, also known as a **scope stack**. The `top` global always points to the innermost (most recent) scope. When you enter a block, a new node is pushed in front; when you leave a block, that node is freed and `top` reverts to the previous one. To find a variable you walk from `top` through each `->next` until either the variable is found or you run out of scopes.

---

### Lines 73–84 — Global Variables

```c
Token  current_token;
char  *input_ptr;
int    current_line = 1;

ScopeNode *top           = NULL;
int        scope_counter = 0;
int        global_offset = 0;
```

These six global variables manage the shared state of the entire compiler pipeline. **`current_token`** holds the token the parser is currently examining — it is updated every time `advance()` is called. **`input_ptr`** is a pointer into the source string; the lexer reads characters by dereferencing this pointer and moves it forward one character at a time. **`current_line`** counts newlines so every token knows its line number for error messages — it starts at `1`. **`top`** is initialised to `NULL` (no scopes yet) and will point to the innermost `ScopeNode` once parsing begins. **`scope_counter`** is a monotonically increasing integer that generates unique scope-level IDs — every time a new scope is pushed it gets `scope_counter` then increments it (post-increment), so scopes are numbered 0, 1, 2, 3 in order of creation. **`global_offset`** is the simulated memory allocator: it starts at 0 and grows by 4 every time a variable is declared, giving each variable a distinct simulated address.

---

### Lines 86–99 — `push_scope()`

```c
void push_scope(void) {
    ScopeNode *S  = top;                         /* i)  S = top */
    ScopeNode *nn = (ScopeNode*)malloc(sizeof(ScopeNode));
    nn->count       = 0;
    nn->scope_level = scope_counter++;
    nn->next        = S;                         /* iii) top.next = S */
    top = nn;                                    /* ii)  top = new ST node */
    printf("  [ST] >>> Entered new scope (level %d)\n", nn->scope_level);
}
```

`push_scope()` is called every time the parser encounters an opening curly brace `{`, implementing steps **i–iii of the scope-entry algorithm** from the header comment. First, the current `top` pointer is saved into a local `S` — this is the *parent* scope. Then `malloc(sizeof(ScopeNode))` dynamically allocates memory for a brand-new node `nn` on the heap. `nn->count` is set to zero because the new scope starts empty. `nn->scope_level` is assigned the current value of `scope_counter` and then *post-increments* it, so each scope gets a unique ID. The linking step `nn->next = S` connects the new node to the enclosing scope — this is what makes the structure a linked list. Finally, `top = nn` **installs** the new node at the front of the chain. The `printf` prints a `[ST] >>>` banner to the terminal so you can visually trace when each scope opens.

---

### Lines 101–115 — `pop_scope()`

```c
void pop_scope(void) {
    if (!top) return;
    printf("  [ST] <<< Leaving scope (level %d) — dropping %d symbol(s):\n",
           top->scope_level, top->count);
    for (int i = 0; i < top->count; i++) {
        printf("        - %s  (%s, offset=%d)\n",
               top->symbols[i].name, top->symbols[i].type, top->symbols[i].offset);
    }
    ScopeNode *save = top->next;                /* i)   save = top.next */
    free(top);                                  /* ii)  delete top */
    top = save;                                 /* iii) top = save */
}
```

`pop_scope()` is called every time the parser encounters a closing curly brace `}`, implementing the **scope-exit algorithm**. The guard `if (!top) return;` ensures we never dereference a null pointer. The `printf` announces which scope level is being removed and how many symbols it contained. The `for` loop then **lists every variable** being discarded, showing its name, type, and memory offset — mimicking what a real compiler does when a variable's **lifetime ends** (it goes out of scope). Then the three critical steps: `save = top->next` grabs the parent before anything is destroyed; `free(top)` releases the heap memory for the current scope node; `top = save` makes the parent the new active scope — exactly mirroring the destruction of a stack frame.

---

### Lines 117–147 — `insert_symbol()`

```c
void insert_symbol(const char *name, const char *type) {
    if (!top) { ... return; }
    for (int i = 0; i < top->count; i++) {
        if (strcmp(top->symbols[i].name, name) == 0) {
            printf("  [ST] WARNING: '%s' already declared in scope %d\n", ...);
            return;
        }
    }
    if (top->count >= MAX_SYMBOLS) { ... return; }

    int size = (strcmp(type, "float") == 0) ? 4 : 4;

    Symbol *sym  = &top->symbols[top->count++];
    strncpy(sym->name, name, 63); sym->name[63] = '\0';
    strncpy(sym->type, type, 15); sym->type[15] = '\0';
    sym->scope_level = top->scope_level;
    sym->offset      = global_offset;
    global_offset   += size;
    printf("  [ST] INSERT: name=%-8s type=%-6s scope=%-3d offset=%d\n", ...);
}
```

`insert_symbol()` adds a new variable record to the **current (top) scope** whenever the parser processes a declaration. Three guard checks come first: (1) if `top` is `NULL` there is no active scope — this would indicate a bug; (2) the duplicate check iterates all existing symbols in the *current* scope only using `strcmp` — the program deliberately allows the same name in an *inner* scope to shadow an *outer* one (valid C scoping behaviour); (3) the overflow check prevents writing past the `symbols[128]` boundary. Once safety is confirmed, `size` is computed — both `int` and `float` get 4 bytes here. The `&top->symbols[top->count++]` idiom atomically gets a pointer to the next empty slot *and* increments the count. Fields are filled with `strncpy` (safe bounded copy), and `global_offset` advances by `size` so the next variable gets the next free address. The `printf` with `%-8s` and `%-3d` specifiers left-aligns each field producing a neat table.

---

### Lines 149–170 — `searchSym()`

```c
int searchSym(ScopeNode *S, const char *var) {
    ScopeNode *cur = S;
    while (cur) {
        for (int i = 0; i < cur->count; i++) {
            if (strcmp(cur->symbols[i].name, var) == 0) {
                printf("  [ST] LOOKUP '%s': FOUND in scope %d — type=%s, offset=%d\n", ...);
                return 1;
            }
        }
        cur = cur->next;   /* move to enclosing scope */
    }
    printf("  [ST] LOOKUP '%s': NOT FOUND (undeclared variable)\n", var);
    return -1;
}
```

This function implements **scoped variable lookup** — the most important algorithm in scope management. It is called every time a variable is *used* in an expression or assigned to. The design exactly follows the rule from the header comment: *"search starting from the current ST; if unsuccessful, move to the ST of the enclosing block"*. `cur` is initialised to the scope `S` passed in (always `top` from call sites). The outer `while (cur)` loop walks up the scope chain one node at a time. The inner `for` loop does a linear search through every `Symbol` in that scope using `strcmp`. As soon as a match is found, the type and offset are printed and the function returns `1` (success). If the inner loop exhausts all symbols in a scope without finding the variable, `cur = cur->next` climbs one level up to the enclosing scope. If the outer loop exhausts the entire chain without finding anything, the variable is **undeclared** — `NOT FOUND` is printed and `-1` is returned.

---

### Lines 172–198 — `print_all_scopes()`

```c
void print_all_scopes(void) {
    printf("\n  ╔═══════════════════════════════════╗\n");
    printf("  ║  CURRENT SYMBOL TABLE SNAPSHOT   ║\n");
    printf("  ╠═══════════════════════════════════╣\n");
    ScopeNode *cur = top;
    if (!cur) { printf("  ║  (empty)  ║\n"); }
    while (cur) {
        printf("  ║  Scope Level %d  (%d symbols)\n", cur->scope_level, cur->count);
        printf("  ║  %-10s %-8s %-6s %-8s\n", "Name", "Type", "Scope", "Offset");
        for (int i = 0; i < cur->count; i++) {
            printf("  ║  %-10s %-8s %-6d %-8d\n", ...);
        }
        if (cur->next)
            printf("  ║  ── next ──> (enclosing scope level %d)\n", cur->next->scope_level);
        cur = cur->next;
    }
    printf("  ╚═══════════════════════════════════╝\n\n");
}
```

`print_all_scopes()` is a **diagnostic display function** called immediately before `pop_scope()` inside `parse_Block()` — so you see a full snapshot of the entire scope chain at the moment a block closes. The Unicode box-drawing characters (`╔`, `║`, `╠`, `╚`) create a visually distinctive bordered table in the terminal. The function walks the scope chain from `top` downward using the same `cur = cur->next` traversal as `searchSym`. For each scope node it prints a header row showing the scope level and symbol count, then column headings (`Name`, `Type`, `Scope`, `Offset`) with separator lines, then one data row per symbol. The `if (cur->next)` line prints an arrow `── next ──>` showing the link to the enclosing scope — making the **linked-list structure** visually apparent.

---

### Lines 200–237 — `token_type_to_string()`

```c
const char* token_type_to_string(TokenType type) {
    switch (type) {
        case TOK_INT:    return "int";
        case TOK_FLOAT:  return "float";
        ...
        case TOK_EOF:    return "EOF";
        default:         return "Unknown";
    }
}
```

This utility function converts a `TokenType` enum value into a **human-readable string**. It is used exclusively by the **syntax error messages** in `match()` — when the parser expected one token type but found another, it calls this function to produce messages like `"Expected ';', but encountered '}'"`. The `switch` statement covers every enum value. The `default` arm catches any future enum values that were added but not yet handled. Returning `const char*` to a **string literal** is safe in C because string literals have static storage duration — they exist for the lifetime of the program. No heap allocation is needed.

---

### Lines 239–260 — `token_category()`

```c
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
```

While `token_type_to_string()` returns the precise token value, `token_category()` returns a broader **lexical class** — the kind of label a textbook uses when classifying tokens. Multiple `case` labels can share one `return` in a C `switch` because of **fall-through**: all keyword token types fall into the `"Keyword"` bucket, all arithmetic operators into `"Arithmetic Op"`, etc. This function drives the **Category** column in the Phase 1 lexical analysis table printed to the terminal.

---

### Lines 262–294 — `advance()` — Whitespace, EOF, and Identifiers

```c
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
```

`advance()` is the **lexer (tokeniser)** — the heart of Phase 1. Each call consumes enough characters from `input_ptr` to produce exactly one token and stores the result in `current_token`. The function begins by **skipping whitespace**: the `while (isspace(*input_ptr))` loop advances past spaces, tabs, and newlines. The newline check `if (*input_ptr == '\n') current_line++` accurately tracks source line numbers. After skipping whitespace, `current_token.line = current_line` stamps the line onto the token. The null-terminator check `if (*input_ptr == '\0')` detects **end of input** and produces a `TOK_EOF` sentinel. The identifier section starts with `if (isalpha(*input_ptr) || *input_ptr == '_')` which matches any word-start character. The inner `while` loop then accumulates up to 63 characters. After the loop, a chain of `strcmp` comparisons checks whether the word is one of the six **reserved keywords**; if none match, it is classified as `TOK_ID`.

---

### Lines 296–364 — `advance()` — Numeric Literals and Operators

```c
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

    current_token.lexeme[0] = *input_ptr;
    current_token.lexeme[1] = '\0';

    switch (*input_ptr) {
        case '(': current_token.type = TOK_LPAREN; input_ptr++; break;
        ...
        case '=':
            input_ptr++;
            if (*input_ptr == '=') { current_token.type = TOK_EQ; strcpy(..., "=="); input_ptr++; }
            else { current_token.type = TOK_ASSIGN; }
            break;
        ...
        default:
            current_token.type = TOK_ERROR; input_ptr++; break;
    }
}
```

The **numeric literal** section handles both integers and floats. The first `while (isdigit(*input_ptr))` loop gathers the integer part. Then `if (*input_ptr == '.')` peeks at the next character: if it is a dot, the dot is appended and a second `while` loop gathers the fractional digits. This correctly tokenises values like `15`, `3`, and `5.0`. Both produce `TOK_NUM`. For **single-character operators** like `(`, `)`, `+`, `-`, etc., the lexeme is pre-set before the switch. Simple operators are handled with one-liner cases. **Two-character operators** (`==`, `<=`, `>=`, `!=`, `&&`, `||`) require a **lookahead**: the first character is consumed with `input_ptr++`, then the next character is checked. If it matches the expected second character, the two-character token is produced; otherwise the single-character version (or `TOK_ERROR`) is produced. The `default` arm handles **unknown characters** by generating `TOK_ERROR` and advancing so tokenisation can continue.

---

### Lines 366–414 — `TokenList` and `run_lexical_analysis()`

```c
typedef struct {
    Token tokens[2048];
    int   count;
} TokenList;

TokenList token_list;

void run_lexical_analysis(char *src) {
    printf("╔══════════════════════════════════════════════════╗\n");
    printf("║           PHASE 1: LEXICAL ANALYSIS             ║\n");
    ...
    input_ptr    = src;
    current_line = 1;
    token_list.count = 0;

    advance();
    int idx = 1;
    while (current_token.type != TOK_EOF) {
        if (current_token.type == TOK_ERROR) {
            printf("║  ... *** LEXICAL ERROR\n", ...);
        } else {
            printf("║  %-6d  %-14s  %-18s  %-6d  ║\n", ...);
        }
        if (token_list.count < 2048)
            token_list.tokens[token_list.count++] = current_token;
        idx++;
        advance();
    }
    if (token_list.count < 2048)
        token_list.tokens[token_list.count++] = current_token; /* EOF */
    printf("║  Total tokens: %-4d  ║\n", idx - 1);
    printf("╚══════════════════════════════════════════════════╝\n");
}
```

`TokenList` is a simple struct holding an array of up to 2048 `Token`s and a count — it acts as a **token buffer** for potential future reuse. `run_lexical_analysis()` is the **Phase 1 driver**: it resets `input_ptr` to the start of the source and `current_line` to 1, then calls `advance()` to load the first token. The `while` loop then repeatedly prints the current token's index, lexeme, category, and line number into a formatted table; appends the token to `token_list`; increments `idx`; and calls `advance()` to load the next token. The `if (current_token.type == TOK_ERROR)` branch adds a `*** LEXICAL ERROR` annotation for any unrecognised character — allowing the Phase 1 pass to complete even in the presence of errors. After the loop exits on `TOK_EOF`, the EOF token itself is stored and the total count is printed. This is the **first of two tokenisation passes** over the source string.

---

### Lines 416–446 — AST Node Struct and Helpers

```c
typedef struct Node {
    char name[64];
    struct Node* children[20];
    int num_children;
    int is_terminal;
} Node;

Node* parse_BoolExpr(void);   /* forward declarations */
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
```

The `Node` struct is the building block of the **Parse Tree**. **`name[64]`** stores the label — for non-terminals this is the grammar rule name (e.g. `"IfStmt"`, `"BoolExpr"`); for terminals it is the lexeme with decoration (e.g. `"ID(a)"`, `"NUM(15)"`). **`children[20]`** is a fixed array of up to 20 child pointers — sufficient for all grammar rules. **`num_children`** tracks occupied slots. **`is_terminal`** is a boolean flag (1 = leaf, 0 = internal). The **forward declarations** are necessary because the parser functions call each other in a mutually recursive way — without them the C compiler would see unknown function names. `create_node` allocates a Node on the heap, copies the name safely with `strncpy`, zeros `num_children`, and sets `is_terminal`. `add_child` appends a child pointer with a safety check that `child` is not null and the array is not full.

---

### Lines 448–473 — `free_tree()` and `match()`

```c
void free_tree(Node* n) {
    if (!n) return;
    for (int i = 0; i < n->num_children; i++)
        free_tree(n->children[i]);
    free(n);
}

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
```

`free_tree()` performs a **post-order recursive traversal**: it first recursively frees all children (deepest nodes first), then frees the current node — preventing use-after-free. The `if (!n) return;` guard handles null slots or a null root gracefully. `match()` is the **gatekeeper** of the recursive-descent parser. If `current_token.type == expected`, the match succeeds: a terminal leaf node is created; identifiers and numbers get a decorative prefix (`ID(...)` or `NUM(...)`) to make the tree readable; `advance()` is called to move to the next token; and the node is returned. If types do not match, **Phase 3 syntax error detection** activates: the exact line number, expected token, and actual token are printed, and `exit(1)` terminates the program. This "exit on first error" strategy is typical of teaching compilers.

---

### Lines 475–520 — `parse_Factor()` and `parse_Term()`

```c
Node* parse_Factor(void) {
    Node* n = create_node("Factor", 0);
    if (current_token.type == TOK_ID) {
        searchSym(top, current_token.lexeme);    /* symbol table lookup on use */
        char buf[64]; sprintf(buf, "ID(%s)", current_token.lexeme);
        Node* c = create_node(buf, 1);
        add_child(n, c);
        advance();
    } else if (current_token.type == TOK_NUM) { ... }
    else if (current_token.type == TOK_LPAREN) {
        add_child(n, match(TOK_LPAREN));
        add_child(n, parse_BoolExpr());
        add_child(n, match(TOK_RPAREN));
    } else if (current_token.type == TOK_NOT) {
        add_child(n, match(TOK_NOT));
        add_child(n, parse_Factor());
    } else { printf("Syntax Error..."); exit(1); }
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
```

`parse_Factor()` handles the **atomic units of expressions**. It branches on the current token: an identifier `TOK_ID` triggers `searchSym(top, ...)` to verify the variable is declared — **the symbol table is consulted on every variable use**. A number `TOK_NUM` creates a leaf node. A left parenthesis `TOK_LPAREN` handles sub-expressions by matching `(`, recursively calling `parse_BoolExpr()` (the top of the expression hierarchy), then matching `)` — this is how **operator precedence via grouping** works. The logical-not `TOK_NOT` is a **unary prefix operator** handled by recursively calling `parse_Factor()` for its operand. `parse_Term()` implements **multiplicative expressions** with left-association. It first parses a Factor, then enters a `while` loop consuming `*`, `/`, or `%` operators. Each iteration wraps the accumulated left subtree and a new right factor into a new `Term` node — this **left-recursive loop** builds a left-leaning tree for chains like `a * b / c`.

---

### Lines 522–592 — Expression Parsers: ArithExpr through BoolExpr

```c
Node* parse_ArithExpr(void)  { /* handles + and - */  }
Node* parse_RelExpr(void)    { /* handles <, >, <=, >= */ }
Node* parse_EqExpr(void)     { /* handles ==, != */ }
Node* parse_AndExpr(void)    { /* handles && */ }
Node* parse_BoolExpr(void)   { /* handles || */ }
```

These five functions form the **expression precedence hierarchy**, each one calling the next-higher-precedence parser. This is the **recursive-descent encoding of operator precedence**. The lower a parser is in the call chain, the higher the precedence of its operator. `parse_ArithExpr()` handles `+` and `-` (additive), wrapping left and right `Term` nodes. `parse_RelExpr()` handles `<`, `>`, `<=`, `>=`, calling `parse_ArithExpr()` for both sides so arithmetic is evaluated before comparison. `parse_EqExpr()` handles `==` and `!=`, calling `parse_RelExpr()`. `parse_AndExpr()` handles `&&`, calling `parse_EqExpr()`. `parse_BoolExpr()` handles `||` — the lowest-precedence operator — calling `parse_AndExpr()`. All five use the same **left-to-right loop and node-wrapping pattern** as `parse_Term()`. The result is that `a < b && b != 0` is correctly parsed as `(a < b) && (b != 0)` because relational operators bind more tightly than AND, which binds more tightly than OR.

---

### Lines 594–619 — `parse_DeclStmt()` — Declaration + Symbol Table Insert

```c
Node* parse_DeclStmt(void) {
    Node* n = create_node("DeclStmt", 0);
    if (current_token.type == TOK_INT || current_token.type == TOK_FLOAT) {
        char declared_type[16];
        strcpy(declared_type, current_token.lexeme);   /* save "int" or "float" */
        add_child(n, match(current_token.type));

        char var_name[64];
        strncpy(var_name, current_token.lexeme, 63); var_name[63] = '\0';
        add_child(n, match(TOK_ID));

        insert_symbol(var_name, declared_type);   /* << symbol table INSERT */

        if (current_token.type == TOK_ASSIGN) {
            add_child(n, match(TOK_ASSIGN));
            add_child(n, parse_BoolExpr());
        }
        add_child(n, match(TOK_SEMI));
    }
    return n;
}
```

`parse_DeclStmt()` handles variable declaration statements like `int a;` or `float avg;`. It is the **primary integration point between the parser and the symbol table**. The type keyword (`int` or `float`) is captured into `declared_type` *before* calling `match()` — this is necessary because `match()` advances past the token and we need the lexeme for `insert_symbol()`. Similarly, `var_name` is copied from `current_token.lexeme` *before* calling `match(TOK_ID)` to consume the identifier. With both type and name in hand, `insert_symbol(var_name, declared_type)` registers the new variable in the current scope. The function also supports **optional initialisation** — if the next token is `=`, it parses the right-hand expression using `parse_BoolExpr()`. The careful "capture lexeme, then match and advance" ordering is a critical idiom — getting this order wrong would capture the *next* token's text instead.

---

### Lines 621–649 — `parse_AssignStmt()` and `parse_Block()`

```c
Node* parse_AssignStmt(void) {
    Node* n = create_node("AssignStmt", 0);
    searchSym(top, current_token.lexeme);  /* verify variable is declared */
    add_child(n, match(TOK_ID));
    add_child(n, match(TOK_ASSIGN));
    add_child(n, parse_BoolExpr());
    add_child(n, match(TOK_SEMI));
    return n;
}

Node* parse_Block(void) {
    Node* n = create_node("Block", 0);
    add_child(n, match(TOK_LBRACE));
    push_scope();                       /* '{' -> push new scope */

    add_child(n, parse_StmtList());

    print_all_scopes();                 /* snapshot before closing */
    pop_scope();                        /* '}' -> pop scope */
    add_child(n, match(TOK_RBRACE));
    return n;
}
```

`parse_AssignStmt()` handles statements like `a = a + 1;`. Before consuming the identifier, `searchSym(top, current_token.lexeme)` is called to **verify the variable is declared** — detecting use-before-declaration. `parse_Block()` is the **most important function for scope management** — it is where `push_scope()` and `pop_scope()` are called. The sequence is: (1) `match(TOK_LBRACE)` consumes `{`; (2) `push_scope()` immediately creates and installs a new inner scope; (3) `parse_StmtList()` parses all statements inside — any declarations here go into the new inner scope; (4) `print_all_scopes()` captures a snapshot of the *entire scope chain* at this moment (innermost to outermost), letting you see the nested symbol table in action; (5) `pop_scope()` frees the inner scope and restores `top` to the parent; (6) `match(TOK_RBRACE)` consumes `}`. This perfectly models how a real compiler creates and destroys activation records.

---

### Lines 651–714 — Statement Parsers and Top-Level List

```c
Node* parse_IfStmt(void) {
    /* if ( BoolExpr ) Block [ else Block ] */
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

Node* parse_WhileStmt(void) { /* while ( BoolExpr ) Block */ }
Node* parse_PrintStmt(void) { /* print ( BoolExpr ) ; */ }

Node* parse_Stmt(void) {
    /* Dispatcher: peek at current token to decide which statement follows */
    if      (INT or FLOAT)  -> parse_DeclStmt()
    else if (TOK_ID)        -> parse_AssignStmt()
    else if (TOK_IF)        -> parse_IfStmt()
    else if (TOK_WHILE)     -> parse_WhileStmt()
    else if (TOK_PRINT)     -> parse_PrintStmt()
    else if (TOK_LBRACE)    -> parse_Block()
    else                    -> syntax error + exit(1)
}

Node* parse_StmtList(void) {
    Node* n = create_node("StmtList", 0);
    while (current_token.type != TOK_RBRACE && current_token.type != TOK_EOF)
        add_child(n, parse_Stmt());
    return n;
}
```

`parse_IfStmt()` matches `if ( BoolExpr ) Block [ else Block ]`. The `else` branch is **optional** — it is only parsed if the current token is `TOK_ELSE` after the first block. Because `parse_Block()` pushes a new scope, both the `then`-block and the `else`-block each get their own independent scope — variables declared in the `then`-block are not visible in the `else`-block. `parse_WhileStmt()` handles `while ( BoolExpr ) Block` — the body block also gets its own scope. `parse_PrintStmt()` matches `print ( BoolExpr ) ;` — a built-in function call with exactly one argument. `parse_Stmt()` is the **dispatcher** — it peeks at the current token to decide which parse function to call. This is **predictive parsing**: because each statement type starts with a distinct token, the parser can always make the right choice with one token of lookahead, without backtracking. `parse_StmtList()` repeatedly calls `parse_Stmt()` until it sees `}` (end-of-block) or `EOF` (end-of-file) — making the statement list a **zero-or-more repetition**.

---

### Lines 716–732 — `print_tree()`

```c
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
```

`print_tree()` is a **recursive pre-order traversal** of the parse tree that produces an indented-tree representation. The `depth` parameter controls the indentation level — it starts at 0 for the root and increments by 1 for each level of children. The indentation loop prints two spaces per level, so each level is visually offset from its parent. Terminal nodes (leaves) are printed in square brackets `[ID(a)]` to distinguish them from non-terminal (internal) nodes like `StmtList`, `IfStmt`. The `FILE* out` parameter enables **dual output**: everything is printed to both `stdout` (terminal) and the file (when `out` points to `parse_tree.txt`). The function never allocates memory and never modifies the tree — it is a purely read-only visitor.

---

### Lines 738–813 — `main()` — The Compiler Driver

```c
int main(int argc, char **argv) {
    char source[16384];
    source[0] = '\0';

    if (argc > 1) {
        FILE *fp = fopen(argv[1], "r");
        if (!fp) { printf("Could not open file: %s\n", argv[1]); return 1; }
        size_t len = fread(source, 1, sizeof(source) - 1, fp);
        source[len] = '\0';
        fclose(fp);
    } else {
        /* read from stdin until EOF */
        ...
    }

    if (strlen(source) == 0) { printf("No input.\n"); return 0; }

    /* PHASE 1 */
    run_lexical_analysis(source);

    /* PHASES 2-4 — reset lexer and begin parsing */
    input_ptr    = source;
    current_line = 1;
    advance();
    push_scope();                    /* global scope 0 */

    if (current_token.type != TOK_EOF) {
        Node* ast = parse_StmtList();

        if (current_token.type == TOK_EOF) {
            printf("✓ Parsing completed successfully\n");
            print_all_scopes();      /* final global scope snapshot */
            FILE* treefile = fopen("parse_tree.txt", "w");
            print_tree(ast, 0, treefile);
            fclose(treefile);
            printf("[Parse tree saved to parse_tree.txt]\n");
        } else {
            printf("*** Syntax Error: Extraneous tokens — found '%s'\n", ...);
        }
        free_tree(ast);
    }

    pop_scope();     /* clean up global scope */
    return 0;
}
```

`main()` is the **orchestration layer** that ties all phases together. The 16 KB `source` array on the stack holds the entire input program — 16384 bytes is enough for any practical mini-language program. **Input reading** supports two modes: if `argc > 1` (a file argument was provided), the file is opened and read in one shot with `fread` — `sizeof(source) - 1` leaves room for the null terminator. If no argument is given, characters are read one by one from `stdin` until EOF. After confirming the input is non-empty, **Phase 1** runs via `run_lexical_analysis(source)`. For **Phases 2–4**, the lexer state is reset again (`input_ptr = source; current_line = 1; advance()`) — starting the **second pass** through the same source string, this time driving the parser. `push_scope()` creates the **global scope (level 0)** for the entire program. `parse_StmtList()` drives the recursive-descent parser across all top-level statements, building the AST and updating the symbol table concurrently. After parsing, if `current_token.type == TOK_EOF` the entire input was consumed without error — success is announced, the final global symbol table is displayed with `print_all_scopes()`, and the parse tree is printed and saved to `parse_tree.txt`. If tokens remain, those are extraneous tokens — a syntax error is reported. `free_tree(ast)` releases all AST heap memory. `pop_scope()` releases the global scope node, and `return 0` signals success to the shell.

---

## Summary Table of All Functions

| Function | Lines | Phase | Purpose |
|---|---|---|---|
| `push_scope()` | 91–99 | ST | Push a new scope node when `{` is encountered |
| `pop_scope()` | 102–115 | ST | Pop current scope node when `}` is encountered |
| `insert_symbol()` | 118–147 | ST | Add a declared variable to the current scope |
| `searchSym()` | 154–170 | ST | Look up a variable walking up the scope chain |
| `print_all_scopes()` | 173–198 | ST | Print a snapshot of all active scopes |
| `token_type_to_string()` | 204–237 | Lex | Map enum to human-readable token name |
| `token_category()` | 239–260 | Lex | Map enum to lexical class name |
| `advance()` | 263–364 | Lex | Consume one token from `input_ptr` |
| `run_lexical_analysis()` | 377–414 | Lex | Phase 1 driver: print full token table |
| `create_node()` | 435–441 | Syn | Allocate and initialise a parse tree node |
| `add_child()` | 443–446 | Syn | Attach a child node to a parent |
| `free_tree()` | 448–453 | Syn | Recursively free the entire parse tree |
| `match()` | 456–473 | Syn | Consume expected token or trigger syntax error |
| `parse_Factor()` | 477–504 | Syn | Parse atoms: ID, NUM, `(expr)`, `!factor` |
| `parse_Term()` | 506–520 | Syn | Parse `*`, `/`, `%` expressions |
| `parse_ArithExpr()` | 522–535 | Syn | Parse `+`, `-` expressions |
| `parse_RelExpr()` | 537–550 | Syn | Parse `<`, `>`, `<=`, `>=` |
| `parse_EqExpr()` | 552–564 | Syn | Parse `==`, `!=` |
| `parse_AndExpr()` | 566–578 | Syn | Parse `&&` |
| `parse_BoolExpr()` | 580–592 | Syn | Parse `\|\|` — top of expression hierarchy |
| `parse_DeclStmt()` | 596–619 | Syn+ST | Parse `int/float id;` and insert into symbol table |
| `parse_AssignStmt()` | 621–632 | Syn+ST | Parse `id = expr;` and lookup in symbol table |
| `parse_Block()` | 634–649 | Syn+ST | Parse `{ stmts }` and push/pop scope |
| `parse_IfStmt()` | 651–664 | Syn | Parse `if (cond) block [else block]` |
| `parse_WhileStmt()` | 666–674 | Syn | Parse `while (cond) block` |
| `parse_PrintStmt()` | 676–684 | Syn | Parse `print(expr);` |
| `parse_Stmt()` | 686–706 | Syn | Dispatch to the correct statement parser |
| `parse_StmtList()` | 708–714 | Syn | Parse zero or more statements |
| `print_tree()` | 720–732 | Out | Recursively print the indented parse tree |
| `main()` | 738–813 | All | Orchestrate all four compiler phases |
