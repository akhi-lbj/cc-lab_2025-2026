# AST-Based Semantic Analysis Implementation

This document provides a technical overview of the transition from a one-pass compiler to a robust two-pass architecture using an Abstract Syntax Tree (AST) for semantic validation.

## 1. Architectural Overview

The compiler has been refactored into three distinct phases:
1.  **Lexical Analysis:** Tokenizes source code, handles comments (`/**/`), and identifies boolean literals.
2.  **Syntax Analysis (AST Construction):** Parses tokens and builds a hierarchical tree representation of the program structure.
3.  **Semantic Analysis (AST Traversal):** Traverses the built tree to enforce type rules, scope constraints, and variable validity.

## 2. The Abstract Syntax Tree (AST)

The AST is defined by the `ASTNode` structure, which captures the "essence" of each language construct:

```c
typedef enum {
    NODE_VAR_DECL, NODE_FUNC_DEF, NODE_CLASS_DEF,
    NODE_ASSIGN, NODE_IF, NODE_WHILE, NODE_PRINT, NODE_RETURN,
    NODE_BLOCK, NODE_STMT_LIST,
    NODE_BINARY_OP, NODE_UNARY_OP, NODE_VAR_REF, NODE_LITERAL,
    NODE_LABEL, NODE_PARAM_LIST
} NodeKind;

typedef struct ASTNode {
    NodeKind kind;
    TokenType op;          /* Operator type for expressions */
    char lexeme[64];       /* Name or literal value */
    char type_str[16];     /* Declared or inferred type */
    struct ASTNode *children[16];
    int child_count;
    int line;
} ASTNode;
```

## 3. Semantic Rules Implemented

The semantic analyzer (`analyze()` and `eval_type()` functions) enforces the following rules:

### A. Declaration Checks
- **Undeclared Variables:** Every variable reference (`NODE_VAR_REF`) or assignment (`NODE_ASSIGN`) is checked against the symbol table.
- **Multiple Declarations:** Variable and function names are checked for uniqueness within their local scope.

### B. Type Compatibility
- **Assignment Compatibility:** 
    - `int` can be assigned to `float` (automatic widening).
    - `float` cannot be assigned to `int` without explicit casting (reported as error).
    - `bool` must be assigned `true`, `false`, or a boolean expression result.
- **Arithmetic Operations:** Operators like `+`, `-`, `*`, `/` require numeric operands (`int` or `float`).

### C. Control Flow Constraints
- **Boolean Conditions:** The conditions for `if` and `while` statements MUST evaluate to a `bool` type. Arithmetic values are NOT implicitly converted to booleans in this strict implementation.

## 4. Scope Management

The analyzer utilizes a linked-list of `ScopeNode` objects to manage nested scopes:
1.  **Global Scope:** Level 0.
2.  **Function/Class Scopes:** New levels pushed when entering a definition.
3.  **Block Scopes:** New levels pushed for `{ ... }` blocks.

Each scope maintains its own symbol list, allowing for variable shadowing where a local variable masks a global one with the same name.

## 5. Verification results

The implementation was verified using `test_semantic_errors.txt`, which correctly triggered:
- `[SEMANTIC ERROR #1]`: Multiple declaration of `x`.
- `[SEMANTIC ERROR #2]`: Assigning `5.5` (float) to `a` (int).
- `[SEMANTIC ERROR #3]`: Assigning `5` (int) to `c` (bool).
- `[SEMANTIC ERROR #4]`: Assigning to undeclared variable `d`.
- `[SEMANTIC ERROR #5]`: Using an arithmetic expression `a + b` as an `if` condition.
