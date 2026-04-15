# `week_5.c` — Complete Code Walkthrough

> **File:** `Week_5/week_5.c` · **Total lines:** ~740  
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
The program prints five phases to the terminal:

| Phase | Output |
|-------|--------|
| **Phase 1** | A formatted token table (lexeme, category, line number) |
| **Phase 2** | Syntax analysis — the recursive-descent parser validates grammar |
| **Phase 3** | Syntax errors with line numbers (program exits on first error) |
| **Phase 4** | `[ST]` symbol-table events (INSERT, LOOKUP, scope push/pop) and final symbol table |
| **Phase 5** | Interactive prompt to search for variables by name using `searchSym()` |

---

## Architecture Overview

```
week_5.c
├── PART 1  — Token Definitions       (lines   1–52)
├── PART 2  — Symbol Table Structures (lines  54–72)
├── GLOBALS                           (lines  74–85)
├── PART 3  — Symbol Table Operations (lines  87–201)
│   ├── push_scope()
│   ├── pop_scope()
│   ├── insert_symbol()
│   ├── searchSym()
│   └── print_all_scopes()
├── PART 4  — Lexical Analyzer        (lines 203–371)
│   ├── token_type_to_string()
│   ├── token_category()
│   └── advance()
├── PART 4b — Lexical Analysis Pass   (lines 373–421)
│   └── run_lexical_analysis()
├── PART 5  — Syntax Analysis/Parser  (lines 423–637)
│   ├── Forward declarations
│   ├── match()
│   ├── parse_Factor / Term / ArithExpr / RelExpr / EqExpr
│   ├── parse_AndExpr / BoolExpr
│   └── parse_DeclOrFunc / AssignStmt / Block / IfStmt
│       parse_WhileStmt / PrintStmt / Stmt / StmtList
└── PART 6  — main()                  (lines 639–735)
```

---

## Line-by-Line Explanation

---

### Lines 1–27 — File Header Comment

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

This block is a **multi-line block comment** — the C compiler ignores everything between `/*` and `*/`. It serves as a documentation header for the entire file. It describes the **five compiler phases** the program implements: Lexical Analysis (tokenising raw text), Syntax Analysis (checking grammar), Syntax Error Detection (reporting grammar violations), the Symbol Table (the main Week 5 feature), and Interactive Symbol Table Lookup (Phase 5 — letting the user search for variables by name after parsing). The comment also documents the exact **algorithm for scope management**: when a `{` is encountered the program saves the current top of the scope stack into a temporary `S`, allocates a brand-new scope node `nn`, and links `nn->next = S` so the chain is preserved; when `}` is encountered the current top is freed and the pointer is restored to the previous node. Finally, the **usage instructions** are embedded right in the file header so any reader immediately knows how to invoke the binary.

---

### Lines 29–32 — Standard Library Includes

```c
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
```

These four `#include` directives pull in C standard library headers. **`<ctype.h>`** provides character-classification functions like `isspace()`, `isalpha()`, `isdigit()`, and `isalnum()` — all essential for the lexer. **`<stdio.h>`** provides I/O functions (`printf`, `fprintf`, `fopen`, `fclose`, `fread`, `getchar`, `scanf`) used throughout for printing output, reading files, and reading interactive user input during Phase 5. **`<stdlib.h>`** is needed for dynamic memory management: `malloc()` (to create scope nodes on the heap) and `free()` (to destroy them when a scope is exited), plus `exit()` to terminate on syntax errors. **`<string.h>`** provides string utilities like `strcmp()`, `strcpy()`, `strncpy()`, and `strlen()` — all used extensively by the lexer and symbol table to compare and copy token lexemes and variable names.

---

### Lines 34–46 — Token Type Enumeration

```c
typedef enum {
    TOK_INT, TOK_FLOAT, TOK_VOID, TOK_IF, TOK_ELSE, TOK_WHILE, TOK_PRINT,
    TOK_LPAREN, TOK_RPAREN, TOK_LBRACE, TOK_RBRACE,
    TOK_ID, TOK_NUM,
    TOK_ASSIGN, TOK_PLUS, TOK_MINUS, TOK_MUL, TOK_DIV, TOK_MOD,
    TOK_LT, TOK_GT, TOK_LE, TOK_GE, TOK_EQ, TOK_NEQ,
    TOK_AND, TOK_OR, TOK_NOT,
    TOK_SEMI, TOK_EOF, TOK_ERROR
} TokenType;
```

`typedef enum { ... } TokenType;` creates a new **named integer type** where each name is automatically assigned a distinct integer starting from 0. This is the **complete vocabulary** of the mini-language the compiler understands. The first row covers **keywords**: `int`, `float`, `void`, `if`, `else`, `while`, `print`. The `TOK_VOID` token is used when recognising the `void` keyword — it enables the compiler to parse **function/procedure definitions** like `void pro_one() { ... }`. The second row covers **delimiters**: left/right parentheses and left/right curly braces. `TOK_ID` represents any user-defined identifier (variable or function name), while `TOK_NUM` represents any numeric literal (integer or float). The third row covers the **assignment operator** `=` and the five **arithmetic operators** `+`, `-`, `*`, `/`, `%`. The fourth row covers the six **relational/comparison operators**: `<`, `>`, `<=`, `>=`, `==`, `!=`. The fifth row covers the three **logical operators**: `&&`, `||`, `!`. Finally, `TOK_SEMI` is the semicolon `;`, `TOK_EOF` signals end of input, and `TOK_ERROR` is used for any character the lexer cannot classify.

---

### Lines 48–52 — Token Structure

```c
typedef struct {
    TokenType type;
    char lexeme[64];
    int  line;
} Token;
```

This `struct` is the **data unit** that the lexer produces and the parser consumes. It has three fields. **`type`** is a `TokenType` enum value identifying what kind of token this is (e.g. `TOK_INT`, `TOK_ID`). **`lexeme[64]`** is a fixed-size character array holding the actual text of the token as it appeared in the source — for example, `"while"`, `"avg"`, or `"15"`. The 64-byte limit is more than enough for any reasonable identifier or number in the mini-language. **`line`** is an integer recording which line of the source file this token was found on — this is crucial for reporting **syntax errors with accurate line numbers** later. The global variable `current_token` of this type holds the token most recently produced by the lexer.

---

### Lines 54–65 — Symbol Table: `Symbol` Structure

```c
#define MAX_SYMBOLS 128   /* max symbols per scope node */

typedef struct {
    char name[64];
    char type[16];        /* "int", "float", or "proc" */
    int  scope_level;
    int  offset;          /* memory offset in bytes */
} Symbol;
```

`#define MAX_SYMBOLS 128` sets a **compile-time constant** — each scope node can hold at most 128 symbol entries. The `Symbol` struct is the **fundamental record** stored for every declared variable or function. **`name[64]`** holds the identifier text (e.g. `"temp"`, `"avg"`, or `"pro_one"`). **`type[16]`** stores the declared type as a string — `"int"` for integer variables, `"float"` for floating-point variables, or `"proc"` for function/procedure names. When a function definition like `void pro_one() { ... }` is parsed, the function name is inserted into the symbol table with type `"proc"` — exactly as shown in the lecture slides where procedures appear in the global scope with type `proc`. **`scope_level`** is an integer recording which scope level this symbol belongs to — `0` for global, `1` for the first nested block (e.g. a function body), and so on. **`offset`** is the simulated **memory address** of this symbol; it increments by 4 bytes for every new entry, representing how a real compiler would lay out variables in a stack frame.

---

### Lines 67–72 — Symbol Table: `ScopeNode` Structure

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

### Lines 74–85 — Global Variables

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

### Lines 87–100 — `push_scope()`

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

### Lines 102–116 — `pop_scope()`

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

### Lines 118–148 — `insert_symbol()`

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

`insert_symbol()` adds a new variable record to the **current (top) scope** whenever the parser processes a declaration. Three guard checks come first: (1) if `top` is `NULL` there is no active scope — this would indicate a bug; (2) the duplicate check iterates all existing symbols in the *current* scope only using `strcmp` — the program deliberately allows the same name in an *inner* scope to shadow an *outer* one (valid C scoping behaviour), so only duplicates within the **same** scope are rejected; (3) the overflow check prevents writing past the `symbols[128]` boundary. Once safety is confirmed, `size` is computed — both `int` and `float` get 4 bytes here. The `&top->symbols[top->count++]` idiom atomically gets a pointer to the next empty slot *and* increments the count. Fields are filled with `strncpy` (safe bounded copy), and `global_offset` advances by `size` so the next variable gets the next free address. The `printf` with `%-8s` and `%-3d` specifiers left-aligns each field producing a neat table.

---

### Lines 150–176 — `searchSym()`

```c
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

This function implements **scoped variable lookup** — the most important algorithm in scope management. It is called every time a variable is *used* in an expression or assigned to, and also during the interactive Phase 5 lookup. The design exactly follows the rule from the header comment: *"search starting from the current ST; if unsuccessful, move to the ST of the enclosing block"*. `cur` is initialised to the scope `S` passed in (always `top` from call sites). The outer `while (cur)` loop walks up the scope chain one node at a time. The inner `for` loop does a linear search through every `Symbol` in that scope using `strcmp`. As soon as a match is found, the type and offset are printed and the function returns `1` (success). If the inner loop exhausts all symbols in a scope without finding the variable, `cur = cur->next` climbs one level up to the enclosing scope. If the outer loop exhausts the entire chain without finding anything, the variable is **undeclared** — `NOT FOUND` is printed and `-1` is returned.

**Variable shadowing**: Because the search starts at the **innermost** scope and walks outward, if the same variable name is declared in both an inner block (e.g. `float a;` in scope 1) and an outer block (e.g. `int a;` in scope 0), the inner scope's entry is found **first** — the function returns immediately with the inner scope's type and offset. This correctly models C's **variable shadowing** semantics where an inner declaration hides an outer one of the same name, even if the types differ.

---

### Lines 178–201 — `print_all_scopes()`

```c
void print_all_scopes(void) {
    printf("\n  +====================================================+\n");
    printf("  |        CURRENT SYMBOL TABLE SNAPSHOT              |\n");
    printf("  +====================================================+\n");
    ScopeNode *cur = top;
    if (!cur) { printf("  |  (empty -- no active scopes)  |\n"); }
    while (cur) {
        printf("  |  Scope Level %d  (%d symbols)\n", cur->scope_level, cur->count);
        printf("  |  %-10s %-8s %-6s %-8s\n", "Name", "Type", "Scope", "Offset");
        printf("  |  %-10s %-8s %-6s %-8s\n", "--------", "------", "-----", "------");
        for (int i = 0; i < cur->count; i++) {
            printf("  |  %-10s %-8s %-6d %-8d\n", ...);
        }
        cur = cur->next;
    }
    printf("  +====================================================+\n\n");
}
```

`print_all_scopes()` is a **diagnostic display function** called immediately before `pop_scope()` inside `parse_Block()` — so you see a full snapshot of the entire scope chain at the moment a block closes. It is also called at the end of successful parsing to show the final global symbol table. The function uses a **vertical display** — it walks the scope chain from `top` downward using the same `cur = cur->next` traversal as `searchSym`, printing each scope's contents one below the other (innermost scope first, outermost/global scope last). For each scope node it prints a header row showing the scope level and symbol count, then column headings (`Name`, `Type`, `Scope`, `Offset`) with separator lines, then one data row per symbol. This vertical layout mirrors the **scope stack** structure from the lecture slides, where each scope's symbol table appears as a separate table stacked vertically.

---

### Lines 206–243 — `token_type_to_string()`

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

### Lines 245–266 — `token_category()`

```c
const char* token_category(TokenType type) {
    switch (type) {
        case TOK_INT: case TOK_FLOAT: case TOK_VOID: case TOK_IF:
        case TOK_ELSE: case TOK_WHILE: case TOK_PRINT:
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

While `token_type_to_string()` returns the precise token value, `token_category()` returns a broader **lexical class** — the kind of label a textbook uses when classifying tokens. Multiple `case` labels can share one `return` in a C `switch` because of **fall-through**: all keyword token types (including `TOK_VOID`) fall into the `"Keyword"` bucket, all arithmetic operators into `"Arithmetic Op"`, etc. This function drives the **Category** column in the Phase 1 lexical analysis table printed to the terminal.

---

### Lines 268–300 — `advance()` — Whitespace, EOF, and Identifiers

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
        else if (strcmp(current_token.lexeme, "void")  == 0)  current_token.type = TOK_VOID;
        else                                                  current_token.type = TOK_ID;
        return;
    }
```

`advance()` is the **lexer (tokeniser)** — the heart of Phase 1. Each call consumes enough characters from `input_ptr` to produce exactly one token and stores the result in `current_token`. The function begins by **skipping whitespace**: the `while (isspace(*input_ptr))` loop advances past spaces, tabs, and newlines. The newline check `if (*input_ptr == '\n') current_line++` accurately tracks source line numbers. After skipping whitespace, `current_token.line = current_line` stamps the line onto the token. The null-terminator check `if (*input_ptr == '\0')` detects **end of input** and produces a `TOK_EOF` sentinel. The identifier section starts with `if (isalpha(*input_ptr) || *input_ptr == '_')` which matches any word-start character. The inner `while` loop then accumulates up to 63 characters. After the loop, a chain of `strcmp` comparisons checks whether the word is one of the seven **reserved keywords** (`if`, `else`, `while`, `print`, `int`, `float`, `void`); if none match, it is classified as `TOK_ID`. The `void` keyword comparison produces `TOK_VOID`, enabling the parser to recognise function/procedure definitions.

---

### Lines 302–370 — `advance()` — Numeric Literals and Operators

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

### Lines 372–420 — `TokenList` and `run_lexical_analysis()`

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

### Lines 422–447 — Parser Forward Declarations and `match()`

```c
/* Forward declarations */
void parse_BoolExpr(void);
void parse_StmtList(void);
void parse_Stmt(void);
void parse_Block(void);

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
```

The **forward declarations** are necessary because the parser functions call each other in a mutually recursive way — without them the C compiler would see unknown function names. All four forward-declared functions return `void` because this version of the compiler does **not** build a parse tree; instead it validates syntax and operates on the symbol table as a side effect.

`match()` is the **gatekeeper** of the recursive-descent parser. If `current_token.type == expected`, the match succeeds: `advance()` is called to consume the current token and move to the next one. If types do not match, **Phase 3 syntax error detection** activates: the exact line number, expected token (converted to a readable string by `token_type_to_string()`), and the actual token are printed in a clear error message, and `exit(1)` terminates the program immediately. This "exit on first error" strategy is typical of teaching compilers — it stops at the first grammar violation rather than attempting error recovery.

---

### Lines 449–470 — `parse_Factor()`

```c
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
```

`parse_Factor()` handles the **atomic units of expressions** — the highest-precedence elements. It branches on the current token: an identifier `TOK_ID` triggers `searchSym(top, current_token.lexeme)` to verify the variable is declared in the current scope chain — **the symbol table is consulted on every variable use**. After the lookup, `advance()` consumes the identifier token. A number `TOK_NUM` simply consumes the token with `advance()`. A left parenthesis `TOK_LPAREN` handles **sub-expressions**: it matches `(`, recursively calls `parse_BoolExpr()` (the top of the expression hierarchy) to parse the inner expression, then matches `)` — this is how **operator precedence via grouping** works (parenthesised expressions are evaluated first). The logical-not `TOK_NOT` is a **unary prefix operator** handled by consuming `!` and then recursively calling `parse_Factor()` for its operand. If none of these token types match, a detailed syntax error is printed and the program exits.

---

### Lines 472–480 — `parse_Term()`

```c
void parse_Term(void) {
    parse_Factor();
    while (current_token.type == TOK_MUL ||
           current_token.type == TOK_DIV ||
           current_token.type == TOK_MOD) {
        advance();   /* consume the operator */
        parse_Factor();
    }
}
```

`parse_Term()` implements **multiplicative expressions** with left-to-right association. It first calls `parse_Factor()` to parse the first operand. Then it enters a `while` loop that continues as long as the current token is `*`, `/`, or `%`. Each iteration calls `advance()` to consume the operator token and then calls `parse_Factor()` again to parse the right operand. This loop correctly handles chains like `a * 2 / b % 3` — evaluating them left to right as `((a * 2) / b) % 3`. Because `parse_Factor()` is called from within `parse_Term()`, multiplicative operators have **higher precedence** than additive ones.

---

### Lines 482–489 — `parse_ArithExpr()`

```c
void parse_ArithExpr(void) {
    parse_Term();
    while (current_token.type == TOK_PLUS ||
           current_token.type == TOK_MINUS) {
        advance();   /* consume the operator */
        parse_Term();
    }
}
```

`parse_ArithExpr()` handles **additive expressions** (`+` and `-`). It follows the same pattern as `parse_Term()`: parse the first `Term`, then loop over any `+` or `-` operators, consuming each and parsing the next `Term`. Because `parse_ArithExpr()` calls `parse_Term()` (which calls `parse_Factor()`), the precedence is correctly established: multiplication and division bind more tightly than addition and subtraction.

---

### Lines 491–498 — `parse_RelExpr()`

```c
void parse_RelExpr(void) {
    parse_ArithExpr();
    while (current_token.type == TOK_LT || current_token.type == TOK_GT ||
           current_token.type == TOK_LE || current_token.type == TOK_GE) {
        advance();   /* consume the operator */
        parse_ArithExpr();
    }
}
```

`parse_RelExpr()` handles **relational comparison expressions** — `<`, `>`, `<=`, `>=`. It calls `parse_ArithExpr()` for both sides, ensuring that arithmetic is fully evaluated before the comparison is applied. The `while` loop allows chaining (though in practice relational comparisons rarely chain), maintaining the left-to-right parsing pattern consistent with the other expression parsers.

---

### Lines 500–506 — `parse_EqExpr()`

```c
void parse_EqExpr(void) {
    parse_RelExpr();
    while (current_token.type == TOK_EQ || current_token.type == TOK_NEQ) {
        advance();   /* consume the operator */
        parse_RelExpr();
    }
}
```

`parse_EqExpr()` handles **equality and inequality operators** — `==` and `!=`. It calls `parse_RelExpr()` for both operands, ensuring that relational comparisons are evaluated before equality checks. This gives `==` and `!=` **lower precedence** than `<`, `>`, `<=`, `>=`, matching the C standard's precedence rules.

---

### Lines 508–514 — `parse_AndExpr()`

```c
void parse_AndExpr(void) {
    parse_EqExpr();
    while (current_token.type == TOK_AND) {
        advance();   /* consume && */
        parse_EqExpr();
    }
}
```

`parse_AndExpr()` handles the **logical AND operator** `&&`. It calls `parse_EqExpr()` for both sides, giving `&&` lower precedence than all comparison and arithmetic operators. The `while` loop allows multiple AND conditions to be chained, such as `a < b && b != 0 && c > 1`, evaluated left to right.

---

### Lines 516–522 — `parse_BoolExpr()`

```c
void parse_BoolExpr(void) {
    parse_AndExpr();
    while (current_token.type == TOK_OR) {
        advance();   /* consume || */
        parse_AndExpr();
    }
}
```

`parse_BoolExpr()` handles the **logical OR operator** `||` — the **lowest-precedence** binary operator in the language. It calls `parse_AndExpr()`, which calls `parse_EqExpr()`, which calls `parse_RelExpr()`, which calls `parse_ArithExpr()`, which calls `parse_Term()`, which calls `parse_Factor()`. This six-level chain is the **recursive-descent encoding of operator precedence**: the deeper a parser is in the call chain, the higher the precedence of its operator. The result is that `a < b && b != 0` is correctly parsed as `(a < b) && (b != 0)` because relational operators bind more tightly than AND, which binds more tightly than OR.

---

### Lines 524–567 — `parse_DeclOrFunc()` — Declaration OR Function/Procedure Definition

```c
/*
 * parse_DeclOrFunc() handles both:
 *   1. Variable declaration:    int x;   or   float avg;
 *   2. Function/procedure def:  void pro_one() { ... }  or  int func() { ... }
 *
 * After consuming the type keyword and the identifier, we peek at the next
 * token: if it is '(' then this is a function/procedure definition --
 * the name is inserted into the symbol table with type "proc",
 * then the parameter list (currently empty) and the body block are parsed.
 * Otherwise it is a normal variable declaration.
 */
void parse_DeclOrFunc(void) {
    if (current_token.type == TOK_INT || current_token.type == TOK_FLOAT ||
        current_token.type == TOK_VOID) {
        char declared_type[16];
        strcpy(declared_type, current_token.lexeme);   /* "int", "float", or "void" */
        match(current_token.type);

        char name[64];
        strncpy(name, current_token.lexeme, 63); name[63] = '\0';
        match(TOK_ID);

        if (current_token.type == TOK_LPAREN) {
            /* -- Function / Procedure definition -- */
            insert_symbol(name, "proc");
            match(TOK_LPAREN);
            match(TOK_RPAREN);
            parse_Block();   /* pushes a new scope for the function body */
        } else {
            /* -- Variable declaration -- */
            insert_symbol(name, declared_type);

            if (current_token.type == TOK_ASSIGN) {
                match(TOK_ASSIGN);
                parse_BoolExpr();
            }
            match(TOK_SEMI);
        }
    }
}
```

`parse_DeclOrFunc()` is the **primary integration point between the parser and the symbol table**. It handles **both** variable declarations (`int x;`, `float avg;`) **and** function/procedure definitions (`void pro_one() { ... }`, `int compute() { ... }`). The function begins by checking if the current token is a type keyword (`TOK_INT`, `TOK_FLOAT`, or `TOK_VOID`). The type keyword's text is captured into `declared_type` *before* calling `match()` — this is necessary because `match()` calls `advance()` which overwrites `current_token`. Similarly, `name` is copied before `match(TOK_ID)`, which consumes the identifier.

The critical **decision point** is the next token after the identifier: if it is `(` (a left parenthesis), this is a **function/procedure definition**. In that case, `insert_symbol(name, "proc")` inserts the function name into the current scope with type `"proc"` — matching the symbol table layout from the lecture slides where procedures like `pro_one` and `pro_two` appear in the global scope with type `proc`. Then `match(TOK_LPAREN)` and `match(TOK_RPAREN)` consume the empty parameter list, and `parse_Block()` parses the function body — which pushes a new scope, allowing the function's local variables to live in their own scope.

If the next token is **not** `(`, the statement is a **normal variable declaration**: `insert_symbol(name, declared_type)` registers the variable with its actual type (`"int"` or `"float"`). An optional initialisation (`int x = expr;`) is supported — if `=` follows, the right-hand expression is parsed via `parse_BoolExpr()`. Finally, `match(TOK_SEMI)` consumes the terminating semicolon.

---

### Lines 549–570 — `parse_AssignStmt()` and `parse_Block()`

```c
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
```

`parse_AssignStmt()` handles statements like `a = a + 1;`. Before consuming the identifier, `searchSym(top, current_token.lexeme)` is called to **verify the variable is declared** — detecting use-before-declaration. Then `match(TOK_ID)` consumes the variable name, `match(TOK_ASSIGN)` consumes the `=`, `parse_BoolExpr()` parses the right-hand-side expression, and `match(TOK_SEMI)` consumes the semicolon.

`parse_Block()` is the **most important function for scope management** — it is where `push_scope()` and `pop_scope()` are called. The sequence is: (1) `match(TOK_LBRACE)` consumes `{`; (2) `push_scope()` immediately creates and installs a new inner scope; (3) `parse_StmtList()` parses all statements inside — any declarations here go into the new inner scope; (4) `print_all_scopes()` captures a snapshot of the *entire scope chain* at this moment (innermost to outermost), letting you see the nested symbol table in action; (5) `pop_scope()` frees the inner scope and restores `top` to the parent; (6) `match(TOK_RBRACE)` consumes `}`. This perfectly models how a real compiler creates and destroys activation records.

---

### Lines 572–625 — Statement Parsers and Top-Level List

```c
void parse_IfStmt(void) {
    /* if ( BoolExpr ) Block [ else Block ] */
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

void parse_WhileStmt(void) { /* while ( BoolExpr ) Block */ }
void parse_PrintStmt(void) { /* print ( BoolExpr ) ; */ }

void parse_Stmt(void) {
    /* Dispatcher: peek at current token to decide which statement follows */
    if      (INT, FLOAT, or VOID)  -> parse_DeclOrFunc()
    else if (TOK_ID)               -> parse_AssignStmt()
    else if (TOK_IF)               -> parse_IfStmt()
    else if (TOK_WHILE)            -> parse_WhileStmt()
    else if (TOK_PRINT)            -> parse_PrintStmt()
    else if (TOK_LBRACE)           -> parse_Block()
    else                           -> syntax error + exit(1)
}

void parse_StmtList(void) {
    while (current_token.type != TOK_RBRACE && current_token.type != TOK_EOF)
        parse_Stmt();
}
```

`parse_IfStmt()` matches `if ( BoolExpr ) Block [ else Block ]`. The `else` branch is **optional** — it is only parsed if the current token is `TOK_ELSE` after the first block. Because `parse_Block()` pushes a new scope, both the `then`-block and the `else`-block each get their own independent scope — variables declared in the `then`-block are not visible in the `else`-block.

`parse_WhileStmt()` handles `while ( BoolExpr ) Block` — the body block also gets its own scope.

`parse_PrintStmt()` matches `print ( BoolExpr ) ;` — a built-in function call with exactly one argument. It uses `match()` for `print`, `(`, `)`, and `;`, with `parse_BoolExpr()` for the argument expression.

`parse_Stmt()` is the **dispatcher** — it peeks at the current token to decide which parse function to call. This is **predictive parsing**: because each statement type starts with a distinct token, the parser can always make the right choice with one token of lookahead, without backtracking. `TOK_INT`/`TOK_FLOAT`/`TOK_VOID` → declaration or function definition (handled by `parse_DeclOrFunc()`), `TOK_ID` → assignment, `TOK_IF` → if statement, `TOK_WHILE` → while loop, `TOK_PRINT` → print statement, `TOK_LBRACE` → standalone block. If none match, a syntax error is reported.

`parse_StmtList()` repeatedly calls `parse_Stmt()` until it sees `}` (end-of-block) or `EOF` (end-of-file) — making the statement list a **zero-or-more repetition**.

---

### Lines 627–723 — `main()` — The Compiler Driver

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
        parse_StmtList();

        if (current_token.type == TOK_EOF) {
            printf("✓ Parsing completed successfully\n");
            print_all_scopes();      /* final global scope snapshot */

            /* PHASE 5 — Interactive symbol table lookup */
            printf("╔═════════════════════════════════════════╗\n");
            printf("║  PHASE 5: SYMBOL TABLE LOOKUP          ║\n");
            ...

            char search_var[64];
            while (1) {
                printf("\n  Search variable >>> ");
                if (scanf("%63s", search_var) != 1) break;

                if (strcmp(search_var, "q") == 0 || strcmp(search_var, "quit") == 0) {
                    printf("  Exiting symbol table lookup.\n");
                    break;
                }

                int result = searchSym(top, search_var);
                if (result == -1) {
                    printf("  '%s' is not declared in any accessible scope.\n",
                           search_var);
                }
            }
        } else {
            printf("*** Syntax Error: Extraneous tokens — found '%s'\n", ...);
        }
    }

    pop_scope();     /* clean up global scope */
    return 0;
}
```

`main()` is the **orchestration layer** that ties all five phases together. The 16 KB `source` array on the stack holds the entire input program — 16384 bytes is enough for any practical mini-language program. **Input reading** supports two modes: if `argc > 1` (a file argument was provided), the file is opened and read in one shot with `fread` — `sizeof(source) - 1` leaves room for the null terminator. If no argument is given, characters are read one by one from `stdin` until EOF. After confirming the input is non-empty, **Phase 1** runs via `run_lexical_analysis(source)`.

For **Phases 2–4**, the lexer state is reset again (`input_ptr = source; current_line = 1; advance()`) — starting the **second pass** through the same source string, this time driving the parser. `push_scope()` creates the **global scope (level 0)** for the entire program. `parse_StmtList()` drives the recursive-descent parser across all top-level statements, validating syntax and updating the symbol table concurrently.

After parsing, if `current_token.type == TOK_EOF` the entire input was consumed without error — success is announced and the final global symbol table is displayed with `print_all_scopes()`. If tokens remain, those are extraneous tokens — a syntax error is reported.

**Phase 5 — Interactive Lookup**: If parsing succeeds, the user enters an interactive loop. A `while (1)` loop repeatedly prints `Search variable >>> ` and reads a variable name using `scanf("%63s", search_var)`. If the user types `"q"` or `"quit"`, the loop exits cleanly with a message. Otherwise, `searchSym(top, search_var)` is called to search the global scope (which is the only scope remaining after all blocks have been exited). If `searchSym` returns `-1` (not found), an additional message confirms the variable is undeclared. If the variable is found, `searchSym` itself prints the type, scope level, and memory offset. The `scanf` return value is checked — if it returns anything other than `1` (e.g. on EOF from piped input), the loop breaks gracefully.

Finally, `pop_scope()` releases the global scope node, and `return 0` signals success to the shell.

---

## Variable Shadowing — How Inner Scopes Override Outer Declarations

A key feature of the symbol table is its support for **variable shadowing**. Consider this example:

```c
int a;          /* global scope 0: a is int, offset 0 */
while (a < 10) {
    float a;    /* inner scope 1: a is float, offset X (shadows outer a) */
    ...
}
```

When the parser encounters `float a;` inside the while-block, `insert_symbol("a", "float")` is called on the **inner scope** (scope level 1). This succeeds because `insert_symbol()` only checks for duplicates within the *current* scope — it does not check enclosing scopes. The outer `int a` in scope 0 is untouched.

When `searchSym(top, "a")` is later called while inside the while-block, the search starts at `top` (scope 1). It finds `float a` in scope 1 *first* and returns immediately — never reaching scope 0. The inner declaration **shadows** the outer one.

When the while-block's `}` is encountered, `pop_scope()` drops scope 1 (and its `float a`). After that, `searchSym(top, "a")` starts at scope 0 and finds the original `int a` — the shadowing is correctly lifted.

---

## Function/Procedure Support — How Functions Are Treated in the Symbol Table

The compiler supports **function/procedure definitions** using the syntax `type name() { body }`. When the parser encounters a type keyword (`int`, `float`, or `void`) followed by an identifier and then a `(`, it recognises this as a function/procedure definition rather than a variable declaration. For example:

```c
int value;
value = 10;

void pro_one() {
    int one_1;
    int one_2;
    {
        int one_3;
        int one_4;
    }
    int one_5;
}
```

The function name `pro_one` is inserted into the **global scope** with type `"proc"` — exactly as shown in the lecture slides where the global symbol table contains:

```
| Name       Type     Scope  Offset  |
| value      int      0      0       |
| pro_one    proc     0      4       |
```

When the function body `{ ... }` is parsed by `parse_Block()`, a new scope is pushed — so the local variables `one_1`, `one_2`, `one_5` get their own scope level (e.g. level 1). Any nested blocks inside the function (like `{ int one_3; int one_4; }`) push additional inner scope levels. When those blocks close, their scope nodes are popped and the local variables are dropped — but the function name `pro_one` persists in the global scope because it was inserted before the body was entered.

The `searchSym()` function can look up function names just like variables. After parsing completes, searching for `pro_one` in Phase 5 returns:
```
  [ST] LOOKUP 'pro_one': FOUND in scope 0 -- type=proc, offset=4
```

---

## Summary Table of All Functions

| Function | Phase | Purpose |
|---|---|---|
| `push_scope()` | ST | Push a new scope node when `{` is encountered |
| `pop_scope()` | ST | Pop current scope node when `}` is encountered |
| `insert_symbol()` | ST | Add a variable or function to the current scope |
| `searchSym()` | ST | Look up a symbol walking up the scope chain |
| `print_all_scopes()` | ST | Print a vertical snapshot of all active scopes |
| `token_type_to_string()` | Lex | Map enum to human-readable token name |
| `token_category()` | Lex | Map enum to lexical class name |
| `advance()` | Lex | Consume one token from `input_ptr` |
| `run_lexical_analysis()` | Lex | Phase 1 driver: print full token table |
| `match()` | Syn | Consume expected token or trigger syntax error |
| `parse_Factor()` | Syn | Parse atoms: ID, NUM, `(expr)`, `!factor` |
| `parse_Term()` | Syn | Parse `*`, `/`, `%` expressions |
| `parse_ArithExpr()` | Syn | Parse `+`, `-` expressions |
| `parse_RelExpr()` | Syn | Parse `<`, `>`, `<=`, `>=` |
| `parse_EqExpr()` | Syn | Parse `==`, `!=` |
| `parse_AndExpr()` | Syn | Parse `&&` |
| `parse_BoolExpr()` | Syn | Parse `\|\|` -- top of expression hierarchy |
| `parse_DeclOrFunc()` | Syn+ST | Parse `type id;` (variable) or `type id() block` (function) |
| `parse_AssignStmt()` | Syn+ST | Parse `id = expr;` and lookup in symbol table |
| `parse_Block()` | Syn+ST | Parse `{ stmts }` and push/pop scope |
| `parse_IfStmt()` | Syn | Parse `if (cond) block [else block]` |
| `parse_WhileStmt()` | Syn | Parse `while (cond) block` |
| `parse_PrintStmt()` | Syn | Parse `print(expr);` |
| `parse_Stmt()` | Syn | Dispatch to the correct statement parser |
| `parse_StmtList()` | Syn | Parse zero or more statements |
| `main()` | All | Orchestrate all five compiler phases |
