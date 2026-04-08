# Comprehensive Code Explanation for `week_4.c`

This document provides a detailed breakdown of the internal workings of our syntax analyzer. The file implements two parsing techniques: an **LL(1) Predictive Parser** and a **Naive Shift-Reduce Parser**. Both use standard stacks to tokenize and process the source language. 

Below is an overview explaining the logic behind each major block of code, line by line conceptually.

---

## 1. Token and Lexer Definitions (Lines 1 - 138)
- **Lines 1-5**: Basic standard C libraries (`stdio.h`, `stdlib.h`, `ctype.h`, etc.) are imported.
- **Lines 7-16 (`TokenType` enum)**: Defines all specific recognizable terminal characters and keywords in the language. E.g., `TOK_INT`, `TOK_FLOAT`, control structures (`TOK_IF`, `TOK_WHILE`), symbols (`TOK_LPAREN`), and arithmetic.
- **Lines 18-23 (`token_names`)**: Provides string equivalents for the enums so we can easily print them.
- **Lines 25-28 (`Token` struct)**: The core struct that the Lexer returns; it associates an explicitly typed `TokenType` with its actual `lexeme` string value.
- **Lines 30-36 (Globals)**: Instantiates `current_token`, a buffer `all_tokens` (to hold the entire program before parsing), and iterators.
- **Lines 38-138 (`advance_lexer`)**: A hand-crafted lexical analyzer mimicking `flex`:
  - It scrubs whitespaces and newlines.
  - If a char string begins with an alphabet character, it pulls the complete alphanumeric string. It checks if it equals `if/else/while/int...` mapping it to reserved keywords. If non-reserved, it assigns `TOK_ID`.
  - It pulls contiguous digits into `TOK_NUM`.
  - Simple `switch(*input_ptr)` maps the basic arithmetic symbols (`+`, `<`, `=`) to corresponding atomic tokens. It checks combinations (`==`, `<=`, `&&`) with lookaheads.

## 2. Grammatical Framework (Lines 140 - 186)
- **Lines 142-144**: Pre-defines array capacities (`MAX_RULES`, `MAX_SYMBOLS`, `MAX_RHS`).
- **Lines 146-148 (`SymbolType`)**: Classifies the symbols making up grammar elements into Terminal, NonTerminal, Epsilon, or EOF.
- **Lines 150-154 (`Symbol`)**: Stores the data regarding each structural node in the grammar layout.
- **Lines 156-160 (`Rule`)**: Maps a rule out. E.g., `lhs` relates to one Symbol Array Index. The `rhs` is an array of its resulting children symbols.
- **Lines 168-186 (Helpers)**: Methods like `get_or_add_symbol`, `add_term`, and `add_nonterm` dynamically allocate slots in the parsing `symbols` dictionary so the system builds standard integer associations.

## 3. Initialization of the Context-Free Grammar (Lines 188 - 365)
*(This is where the actual Left-Factored language representation takes place!)*
- **Lines 188-192**: All Non-Terminals (`Stmt`, `DeclStmt`, `BoolExpr`, `Factor`...) are declared as integer indexes.
- **Lines 194-222 (`init_grammar`)**: Establishes Non-Terminals and associates Terminal string values to their Token counterparts.
- **Lines 224-365 (Rules)**: Registers exactly what RHS outputs map to LHS outputs. Each production rule explicitly breaks down into terminal indices exactly correlating to tokens. Left-Recursion has been completely eliminated in `ArithExpr`, `Term`, and `Factor` rules by injecting non-terminal `Tail` derivatives.

## 4. FIRST and FOLLOW Set Algorithms (Lines 367 - 485)
- **Lines 367-369**: Instantiates memory maps: `first[][]` and `follow[][]` 2D capability mapping boolean charts.
- **Lines 371-419 (`compute_first`)**: Runs an iterative fixed-point algorithm across all tokens:
  1. All terminals have themselves as `FIRST`.
  2. Iteratively loop through rules. For a rule `X -> Y1 Y2 Y3`, `FIRST(X)` includes `FIRST(Y1)`.
  3. If `Y1` inherently possesses an `EPSILON`, append `FIRST(Y2)`, looping dynamically until converging optimally.
- **Lines 421-460 (`compute_follow`)**: Calculates what structures can strictly pursue specific items:
  1. The Global Program receives `EOF`.
  2. In `A -> X Y Z`, the `FOLLOW(Y)` inherently absorbs `FIRST(Z)`.
  3. Uses a cascading resolution: if the entire right hand of a non-terminal dissipates functionally as `EPSILON`, `FOLLOW(A)` migrates to `FOLLOW(X)`.
- **Lines 462-485 (`print_first_follow`)**: Iteratively prints out sets beautifully.

## 5. LL(1) Parse Table Generator (Lines 487 - 548)
- **Lines 487 (`ll1_table[MAX][MAX]`)**: Mappings matrix `ll1_table[NonTerminal][Terminal]`. 
- **Lines 489-525 (`build_ll1_table`)**:
  - Initializes bounds to `-1` (invalid mapping).
  - Sweeps through rules dynamically predicting syntax flows depending on Terminal presence (`FIRST` map check).
  - Resolves `EPSILON` overlaps manually relying on the previously compiled `FOLLOW` map! Allows predicting Epsilon closure structures systematically avoiding crashes.

## 6. LL(1) Table-Driven Stack Parser (Lines 550 - 612)
- **Line 551 (`parse_ll1`)**: Initializes the stream tracker routing out to `ll1_trace.txt`.
- **Line 560**: The stack is heavily reliant on initially storing `EOF` terminating and `Program` root NonTerminal definitions strictly.
- **Lines 566-605 (Execution Loop)**:
  - Consistently peaks at `top()`.
  - Determines if parsed structure maps functionally (`current_token` aligns exactly with stack). Maps valid, increments reader sequentially.
  - Generates errors manually and breaks if prediction tables indicate `-1`.
  - Inverses standard RHS output predictions dynamically forcing predictions sequentially inverted so Stack peaks map Left-To-Right efficiently.

## 7. Naive Shift-Reduce Parser (Lines 614 - 679)
- **Lines 614-633 (`parse_shift_reduce`)**: Executes an extremely greedy heuristic bottom-up parsing sequence targeting `shift_reduce_trace.txt`.
- **Lines 634-659 (Reduce Check)**:
  - Traces the entire `stack` backwards to verify if the deepest pushed objects completely align identically backwards to a verified Rule RHS.
  - Applies naive matching: instantly replacing the components off the stack and slapping the resulting LHS token up continuously minimizing elements.
- **Lines 660-675 (Shift Action)**: Push the active character buffer safely internally.

## 8. Main Execution Wrapper (Lines 681 - 720)
- Takes user character input safely buffering directly via `getchar()`.
- Flushes the sequence across the lexer array systematically loading `all_tokens`.
- Sequentially processes `init_grammar()`, `compute_first()`, `compute_follow()`, and `build_ll1_table()`, generating dynamic logic rules.
- Triggers parsed executions visually reporting `SUCCESS` metrics sequentially.

---

## 💻 Example Input

If you execute `./week_4.exe` (or `week_4.exe` on Windows), standard CLI execution waits for you to insert syntax. You can optionally paste the execution parameters below. Hit `Ctrl+D` (Unix) or `Ctrl+Z` (Windows) followed by `Enter` to send `EOF` mapping metrics:

```c
int a; 
int b; 
int sum; 
float avg; 
 
a = 2 * (3 + 4); 
b = 15; 
sum = 0; 
 
while (a < b && b != 0) { 
    int temp; 
    temp = a * 2; 
 
    if ((temp % 3 == 0) || (a > 5)) { 
        sum = sum + temp; 
    } else { 
        sum = sum - 1; 
    } 
 
    a = a + 1; 
} 
 
avg = sum / (b - a); 
 
if (!(avg < 5.0)) { 
    print(sum); 
} else { 
    print(avg); 
} 
```

**What to expect on run:**
1. The console will output the dynamically computed **FIRST** and **FOLLOW** sets.
2. The partial **LL(1)** parsing table will trace onto stdout.
3. The executable will deposit thousands of exact parsing derivation lines directly mapping Stack traces down to `Week_4/ll1_trace.txt` efficiently proving 100% Top-Down execution mapped gracefully mapping all elements natively.
4. Heuristic tracking Shift traces map to `Week_4/shift_reduce_trace.txt` sequentially mapping standard structures automatically executing basic algorithms securely.
