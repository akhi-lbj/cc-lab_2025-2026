# README for cfg.c

Source: [Week_2/cfg.c](Week_2/cfg.c#L1-L400)

**Overview:**
- `cfg.c` is a simple hand-written lexer + recursive-descent parser for a tiny C-like subset supporting:
  - `if` statements with parenthesized boolean expressions and `{}` blocks
  - assignment statements (`id = expr;`)
  - expressions with `+` and numeric or identifier factors
  - relational `>` operator in boolean expressions
- The program reads a single input line from stdin, tokenizes it, parses it, and prints parsing traces or syntax errors.

**File structure & explained snippets**

- Token definitions (lines 5-28):
  - `TokenType` enum lists token kinds used across lexer and parser (e.g., `TOK_IF`, `TOK_ID`, `TOK_NUM`, `TOK_PLUS`, `TOK_SEMI`, `TOK_EOF`, `TOK_ERROR`).
  - `Token` struct holds a `type` and a small `lexeme` buffer for the token text.

- Global parser state (lines 30-33):
  - `current_token` stores the token currently being examined by parser routines.
  - `input_ptr` is a pointer that walks the input string (used by lexer `advance()`).

- Lexical analyzer `advance()` (lines 36-95):
  - Skips whitespace using `isspace()`.
  - If at end-of-string (`'\\0'`) sets `TOK_EOF` and lexeme "EOF".
  - Recognizes identifiers/keywords: collects alphanumeric run into `current_token.lexeme`. Then distinguishes keywords `if` and `while` (others are `TOK_ID`).
  - Recognizes number literals: run of `isdigit` becomes `TOK_NUM`.
  - For single-character tokens, it sets `lexeme` to that char and sets token type based on `switch` for `(`, `)`, `{`, `}`, `>`, `=`, `+`, `;`.
  - Any unrecognized character sets token type `TOK_ERROR`.

- `match()` utility (lines 99-107):
  - Verifies that `current_token.type` equals the expected token type.
  - If matched, calls `advance()` to consume token; otherwise prints a syntax error and exits with `exit(1)`.

- Parser forward declarations (lines 109-111):
  - `parse_Expr()`, `parse_StmtList()`, `parse_Stmt()` — main parsing entry points.

- `parse_Factor()` (lines 113-125):
  - Accepts either an identifier (`TOK_ID`) or number (`TOK_NUM`).
  - Prints `Parsed Factor: <lexeme>` then `advance()`.
  - On mismatch prints an error and exits.

- `parse_Term()` (line 127):
  - Currently just delegates to `parse_Factor()`; left as separate function for future operator precedence extensions.

- `parse_Expr()` (lines 129-137):
  - Parses a term, then handles zero-or-more `+` plus-operations: when `TOK_PLUS` is found it prints the operator and matches `TOK_PLUS`, then parses the next term.
  - This implements left-associative `+`-chained expressions.

- `parse_BoolExpr()` (lines 139-146):
  - Parses an arithmetic expression, then optionally a `>` relational operator (prints it) followed by another expression.
  - This supports boolean conditions like `x + 1 > y`.

- `parse_Assign()` (lines 148-156):
  - Assumes the current token is an identifier to be assigned to.
  - Prints the assignment ID, matches `TOK_ID` then `TOK_ASSIGN` (`=`), parses an expression, and then matches the terminating semicolon `TOK_SEMI`.

- `parse_Block()` (lines 158-165):
  - Matches `{`, prints "Entering Block...", calls `parse_StmtList()` to parse statements inside, then matches `}`, prints "Exiting Block.".

- `parse_IfStmt()` (lines 167-174):
  - Matches `if ( BoolExpr ) Block` in sequence using `match()` and parser helpers.

- `parse_Stmt()` (lines 176-190):
  - Dispatches on statement type: `if` → `parse_IfStmt()`, identifier → `parse_Assign()`, `{` → `parse_Block()`.
  - Otherwise produces an error and exits.

- `parse_StmtList()` (lines 192-196):
  - Repeatedly parses statements until it finds a closing `}` or `EOF` (so blocks and the top-level input are handled).

- `main()` (lines 199-240):
  - Reads a single line from stdin into `user_input` using `fgets()`.
  - Initializes `input_ptr` to the start of `user_input`.
  - Calls `advance()` to load the first token and then `parse_Stmt()` if input is not empty.
  - After parsing, checks if the `current_token` is `TOK_EOF` and prints success or a trailing token error.

**Usage & examples**
- Example valid input to try at program prompt:
  - if ( x > 0 ) { y = x + 1 ; }
  - a = 5 + 3 ;
- When you run the compiled program, type one of the above lines then press Enter.

**Limitations & notes**
- The lexer uses a fixed `char lexeme[20]` — identifiers or numbers longer than 19 chars will overflow; increase buffer size for real use.
- No support for multi-character relational operators (`>=`, `==`, etc.).
- No operator precedence beyond `+` vs factor; `parse_Term()` is a placeholder for `*`/`/` if needed.
- Error handling is immediate `exit(1)` with a printed message; a production parser would prefer graceful recovery.
- Whitespace and newline handling: `advance()` skips whitespace. The input line must end (or include) `;` for assignments.

**Files created**
- This README: `Week_2/README_cfg.md` (this file)

**If you want a PDF of this README**
- I will try to run `pandoc Week_2/README_cfg.md -o Week_2/README_cfg.pdf` to create `Week_2/README_cfg.pdf`.
- If `pandoc` is not installed, use one of:
  - Install Pandoc and run the command above.
  - Use an editor (VS Code / Typora) and export to PDF.

---

If you want, I can now attempt to convert this README to PDF automatically.  
# README for cfg.c

Source: [Week_2/cfg.c](Week_2/cfg.c#L1-L400)

**Overview:**
- `cfg.c` is a simple hand-written lexer + recursive-descent parser for a tiny C-like subset supporting:
  - `if` statements with parenthesized boolean expressions and `{}` blocks
  - assignment statements (`id = expr;`)
  - expressions with `+` and numeric or identifier factors
  - relational `>` operator in boolean expressions
- The program reads a single input line from stdin, tokenizes it, parses it, and prints parsing traces or syntax errors.

**File structure & explained snippets**

- Token definitions (lines 5-28):
  - `TokenType` enum lists token kinds used across lexer and parser (e.g., `TOK_IF`, `TOK_ID`, `TOK_NUM`, `TOK_PLUS`, `TOK_SEMI`, `TOK_EOF`, `TOK_ERROR`).
  - `Token` struct holds a `type` and a small `lexeme` buffer for the token text.

- Global parser state (lines 30-33):
  - `current_token` stores the token currently being examined by parser routines.
  - `input_ptr` is a pointer that walks the input string (used by lexer `advance()`).

- Lexical analyzer `advance()` (lines 36-95):
  - Skips whitespace using `isspace()`.
  - If at end-of-string (`'\0'`) sets `TOK_EOF` and lexeme "EOF".
  - Recognizes identifiers/keywords: collects alphanumeric run into `current_token.lexeme`. Then distinguishes keywords `if` and `while` (others are `TOK_ID`).
  - Recognizes number literals: run of `isdigit` becomes `TOK_NUM`.
  - For single-character tokens, it sets `lexeme` to that char and sets token type based on `switch` for `(`, `)`, `{`, `}`, `>`, `=`, `+`, `;`.
  - Any unrecognized character sets token type `TOK_ERROR`.

- `match()` utility (lines 99-107):
  - Verifies that `current_token.type` equals the expected token type.
  - If matched, calls `advance()` to consume token; otherwise prints a syntax error and exits with `exit(1)`.

- Parser forward declarations (lines 109-111):
  - `parse_Expr()`, `parse_StmtList()`, `parse_Stmt()` — main parsing entry points.

- `parse_Factor()` (lines 113-125):
  - Accepts either an identifier (`TOK_ID`) or number (`TOK_NUM`).
  - Prints `Parsed Factor: <lexeme>` then `advance()`.
  - On mismatch prints an error and exits.

- `parse_Term()` (line 127):
  - Currently just delegates to `parse_Factor()`; left as separate function for future operator precedence extensions.

- `parse_Expr()` (lines 129-137):
  - Parses a term, then handles zero-or-more `+` plus-operations: when `TOK_PLUS` is found it prints the operator and matches `TOK_PLUS`, then parses the next term.
  - This implements left-associative `+`-chained expressions.

- `parse_BoolExpr()` (lines 139-146):
  - Parses an arithmetic expression, then optionally a `>` relational operator (prints it) followed by another expression.
  - This supports boolean conditions like `x + 1 > y`.

- `parse_Assign()` (lines 148-156):
  - Assumes the current token is an identifier to be assigned to.
  - Prints the assignment ID, matches `TOK_ID` then `TOK_ASSIGN` (`=`), parses an expression, and then matches the terminating semicolon `TOK_SEMI`.

- `parse_Block()` (lines 158-165):
  - Matches `{`, prints "Entering Block...", calls `parse_StmtList()` to parse statements inside, then matches `}`, prints "Exiting Block.".

- `parse_IfStmt()` (lines 167-174):
  - Matches `if ( BoolExpr ) Block` in sequence using `match()` and parser helpers.

- `parse_Stmt()` (lines 176-190):
  - Dispatches on statement type: `if` &rarr; `parse_IfStmt()`, identifier &rarr; `parse_Assign()`, `{` &rarr; `parse_Block()`.
  - Otherwise produces an error and exits.

- `parse_StmtList()` (lines 192-196):
  - Repeatedly parses statements until it finds a closing `}` or `EOF` (so blocks and the top-level input are handled).

- `main()` (lines 199-240):
  - Reads a single line from stdin into `user_input` using `fgets()`.
  - Initializes `input_ptr` to the start of `user_input`.
  - Calls `advance()` to load the first token and then `parse_Stmt()` if input is not empty.
  - After parsing, checks if the `current_token` is `TOK_EOF` and prints success or a trailing token error.

**Usage & examples**
- Example valid input to try at program prompt:
  - if ( x > 0 ) { y = x + 1 ; }
  - a = 5 + 3 ;
- When you run the compiled program, type one of the above lines then press Enter.

**Limitations & notes**
- The lexer uses a fixed `char lexeme[20]` — identifiers or numbers longer than 19 chars will overflow; increase buffer size for real use.
- No support for multi-character relational operators (`>=`, `==`, etc.).
- No operator precedence beyond `+` vs factor; `parse_Term()` is a placeholder for `*`/`/` if needed.
- Error handling is immediate `exit(1)` with a printed message; a production parser would prefer graceful recovery.
- Whitespace and newline handling: `advance()` skips whitespace. The input line must end (or include) `;` for assignments.

**Files created**
- This README: `Week_2/README_cfg.md` (this file)

**If you want a PDF of this README**
- I will try to run `pandoc Week_2/README_cfg.md -o Week_2/README_cfg.pdf` to create `Week_2/README_cfg.pdf`.
- If `pandoc` is not installed, use one of:
  - Install Pandoc and run the command above.
  - Use an editor (VS Code / Typora) and export to PDF.

---

If you want, I can now attempt to convert this README to PDF automatically.  
