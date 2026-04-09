#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// --- Lexer & Tokens ---

typedef enum {
    TOK_INT=0, TOK_FLOAT, TOK_IF, TOK_ELSE, TOK_WHILE, TOK_PRINT,
    TOK_LPAREN, TOK_RPAREN, TOK_LBRACE, TOK_RBRACE,
    TOK_ID, TOK_NUM,
    TOK_ASSIGN, TOK_PLUS, TOK_MINUS, TOK_MUL, TOK_DIV, TOK_MOD,
    TOK_LT, TOK_GT, TOK_LE, TOK_GE, TOK_EQ, TOK_NEQ,
    TOK_AND, TOK_OR, TOK_NOT,
    TOK_SEMI, TOK_EOF, TOK_ERROR
} TokenType;

const char *token_names[] = {
    "int", "float", "if", "else", "while", "print",
    "(", ")", "{", "}", "ID", "NUM", "=", "+", "-", "*", "/", "%",
    "<", ">", "<=", ">=", "==", "!=", "&&", "||", "!", ";", "$", "ERROR"
};

typedef struct {
    TokenType type;
    char lexeme[50];
} Token;

Token current_token;
char *input_ptr;
int current_line = 1;
Token all_tokens[10000];
int token_count = 0;
int read_index = 0;

void advance_lexer() {
    while (isspace(*input_ptr)) {
        if (*input_ptr == '\n') current_line++;
        input_ptr++;
    }

    if (*input_ptr == '\0') {
        current_token.type = TOK_EOF;
        strcpy(current_token.lexeme, "$");
        return;
    }

    if (isalpha(*input_ptr)) {
        int i = 0;
        while (isalnum(*input_ptr) || *input_ptr == '_') {
            if (i < 49) current_token.lexeme[i++] = *input_ptr;
            input_ptr++;
        }
        current_token.lexeme[i] = '\0';

        if (strcmp(current_token.lexeme, "if") == 0) current_token.type = TOK_IF;
        else if (strcmp(current_token.lexeme, "else") == 0) current_token.type = TOK_ELSE;
        else if (strcmp(current_token.lexeme, "while") == 0) current_token.type = TOK_WHILE;
        else if (strcmp(current_token.lexeme, "print") == 0) current_token.type = TOK_PRINT;
        else if (strcmp(current_token.lexeme, "int") == 0) current_token.type = TOK_INT;
        else if (strcmp(current_token.lexeme, "float") == 0) current_token.type = TOK_FLOAT;
        else current_token.type = TOK_ID;
        return;
    }

    if (isdigit(*input_ptr)) {
        int i = 0;
        while (isdigit(*input_ptr)) {
            if (i < 49) current_token.lexeme[i++] = *input_ptr;
            input_ptr++;
        }
        if (*input_ptr == '.') {
            if (i < 49) current_token.lexeme[i++] = *input_ptr;
            input_ptr++;
            while (isdigit(*input_ptr)) {
                if (i < 49) current_token.lexeme[i++] = *input_ptr;
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
        case ')': current_token.type = TOK_RPAREN; input_ptr++; break;
        case '{': current_token.type = TOK_LBRACE; input_ptr++; break;
        case '}': current_token.type = TOK_RBRACE; input_ptr++; break;
        case ';': current_token.type = TOK_SEMI; input_ptr++; break;
        case '+': current_token.type = TOK_PLUS; input_ptr++; break;
        case '-': current_token.type = TOK_MINUS; input_ptr++; break;
        case '*': current_token.type = TOK_MUL; input_ptr++; break;
        case '/': current_token.type = TOK_DIV; input_ptr++; break;
        case '%': current_token.type = TOK_MOD; input_ptr++; break;
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

// --- Grammar Definition Framework ---

#define MAX_RULES 100
#define MAX_RHS 10
#define MAX_SYMBOLS 100

typedef enum {
    SYM_TERMINAL, SYM_NONTERMINAL, SYM_EPSILON, SYM_EOF
} SymbolType;

typedef struct {
    char name[30];
    SymbolType type;
    int token_id; 
} Symbol;

typedef struct {
    int lhs; 
    int rhs[MAX_RHS]; 
    int rhs_count;
} Rule;

Symbol symbols[MAX_SYMBOLS];
int symbol_count = 0;

Rule rules[MAX_RULES];
int rule_count = 0;

int sym_epsilon, sym_eof;

int get_or_add_symbol(const char *name, SymbolType type, int token_id) {
    for (int i = 0; i < symbol_count; i++) {
        if (strcmp(symbols[i].name, name) == 0 && symbols[i].type == type) {
            return i;
        }
    }
    strcpy(symbols[symbol_count].name, name);
    symbols[symbol_count].type = type;
    symbols[symbol_count].token_id = token_id;
    return symbol_count++;
}

int add_term(TokenType tok) {
    return get_or_add_symbol(token_names[tok], SYM_TERMINAL, tok);
}

int add_nonterm(const char *name) {
    return get_or_add_symbol(name, SYM_NONTERMINAL, -1);
}

void add_rule(int lhs, int *rhs, int rhs_count) {
    rules[rule_count].lhs = lhs;
    for (int i = 0; i < rhs_count; i++) {
        rules[rule_count].rhs[i] = rhs[i];
    }
    rules[rule_count].rhs_count = rhs_count;
    rule_count++;
}

// Global Non-Terminals
int Program, StmtList, Stmt, DeclStmt, DeclTail, AssignStmt, IfStmt, IfTail;
int WhileStmt, PrintStmt, Block, BoolExpr, BoolExprTail, AndExpr, AndExprTail;
int EqExpr, EqExprTail, RelExpr, RelExprTail, ArithExpr, ArithExprTail;
int Term, TermTail, Factor;

void init_grammar() {
    sym_epsilon = get_or_add_symbol("EPSILON", SYM_EPSILON, -1);
    sym_eof = get_or_add_symbol("$", SYM_EOF, TOK_EOF);

    Program = add_nonterm("Program");
    StmtList = add_nonterm("StmtList");
    Stmt = add_nonterm("Stmt");
    DeclStmt = add_nonterm("DeclStmt");
    DeclTail = add_nonterm("DeclTail");
    AssignStmt = add_nonterm("AssignStmt");
    IfStmt = add_nonterm("IfStmt");
    IfTail = add_nonterm("IfTail");
    WhileStmt = add_nonterm("WhileStmt");
    PrintStmt = add_nonterm("PrintStmt");
    Block = add_nonterm("Block");
    BoolExpr = add_nonterm("BoolExpr");
    BoolExprTail = add_nonterm("BoolExprTail");
    AndExpr = add_nonterm("AndExpr");
    AndExprTail = add_nonterm("AndExprTail");
    EqExpr = add_nonterm("EqExpr");
    EqExprTail = add_nonterm("EqExprTail");
    RelExpr = add_nonterm("RelExpr");
    RelExprTail = add_nonterm("RelExprTail");
    ArithExpr = add_nonterm("ArithExpr");
    ArithExprTail = add_nonterm("ArithExprTail");
    Term = add_nonterm("Term");
    TermTail = add_nonterm("TermTail");
    Factor = add_nonterm("Factor");

    // Program -> StmtList
    { int rhs[] = {StmtList}; add_rule(Program, rhs, 1); }

    // StmtList -> Stmt StmtList | EPSILON
    { int rhs[] = {Stmt, StmtList}; add_rule(StmtList, rhs, 2); }
    { int rhs[] = {sym_epsilon}; add_rule(StmtList, rhs, 1); }

    // Stmt -> DeclStmt | AssignStmt | IfStmt | WhileStmt | PrintStmt | Block
    { int rhs[] = {DeclStmt}; add_rule(Stmt, rhs, 1); }
    { int rhs[] = {AssignStmt}; add_rule(Stmt, rhs, 1); }
    { int rhs[] = {IfStmt}; add_rule(Stmt, rhs, 1); }
    { int rhs[] = {WhileStmt}; add_rule(Stmt, rhs, 1); }
    { int rhs[] = {PrintStmt}; add_rule(Stmt, rhs, 1); }
    { int rhs[] = {Block}; add_rule(Stmt, rhs, 1); }

    // DeclStmt -> int ID DeclTail | float ID DeclTail
    { int rhs[] = {add_term(TOK_INT), add_term(TOK_ID), DeclTail}; add_rule(DeclStmt, rhs, 3); }
    { int rhs[] = {add_term(TOK_FLOAT), add_term(TOK_ID), DeclTail}; add_rule(DeclStmt, rhs, 3); }

    // DeclTail -> ; | = BoolExpr ;
    { int rhs[] = {add_term(TOK_SEMI)}; add_rule(DeclTail, rhs, 1); }
    { int rhs[] = {add_term(TOK_ASSIGN), BoolExpr, add_term(TOK_SEMI)}; add_rule(DeclTail, rhs, 3); }

    // AssignStmt -> ID = BoolExpr ;
    { int rhs[] = {add_term(TOK_ID), add_term(TOK_ASSIGN), BoolExpr, add_term(TOK_SEMI)}; add_rule(AssignStmt, rhs, 4); }

    // IfStmt -> if ( BoolExpr ) Block IfTail
    { int rhs[] = {add_term(TOK_IF), add_term(TOK_LPAREN), BoolExpr, add_term(TOK_RPAREN), Block, IfTail}; add_rule(IfStmt, rhs, 6); }
    { int rhs[] = {add_term(TOK_ELSE), Block}; add_rule(IfTail, rhs, 2); }
    { int rhs[] = {sym_epsilon}; add_rule(IfTail, rhs, 1); }

    // WhileStmt -> while ( BoolExpr ) Block
    { int rhs[] = {add_term(TOK_WHILE), add_term(TOK_LPAREN), BoolExpr, add_term(TOK_RPAREN), Block}; add_rule(WhileStmt, rhs, 5); }

    // PrintStmt -> print ( BoolExpr ) ;
    { int rhs[] = {add_term(TOK_PRINT), add_term(TOK_LPAREN), BoolExpr, add_term(TOK_RPAREN), add_term(TOK_SEMI)}; add_rule(PrintStmt, rhs, 5); }

    // Block -> { StmtList }
    { int rhs[] = {add_term(TOK_LBRACE), StmtList, add_term(TOK_RBRACE)}; add_rule(Block, rhs, 3); }

    // BoolExpr -> AndExpr BoolExprTail
    { int rhs[] = {AndExpr, BoolExprTail}; add_rule(BoolExpr, rhs, 2); }
    { int rhs[] = {add_term(TOK_OR), AndExpr, BoolExprTail}; add_rule(BoolExprTail, rhs, 3); }
    { int rhs[] = {sym_epsilon}; add_rule(BoolExprTail, rhs, 1); }

    // AndExpr -> EqExpr AndExprTail
    { int rhs[] = {EqExpr, AndExprTail}; add_rule(AndExpr, rhs, 2); }
    { int rhs[] = {add_term(TOK_AND), EqExpr, AndExprTail}; add_rule(AndExprTail, rhs, 3); }
    { int rhs[] = {sym_epsilon}; add_rule(AndExprTail, rhs, 1); }

    // EqExpr -> RelExpr EqExprTail
    { int rhs[] = {RelExpr, EqExprTail}; add_rule(EqExpr, rhs, 2); }
    { int rhs[] = {add_term(TOK_EQ), RelExpr, EqExprTail}; add_rule(EqExprTail, rhs, 3); }
    { int rhs[] = {add_term(TOK_NEQ), RelExpr, EqExprTail}; add_rule(EqExprTail, rhs, 3); }
    { int rhs[] = {sym_epsilon}; add_rule(EqExprTail, rhs, 1); }

    // RelExpr -> ArithExpr RelExprTail
    { int rhs[] = {ArithExpr, RelExprTail}; add_rule(RelExpr, rhs, 2); }
    { int rhs[] = {add_term(TOK_LT), ArithExpr, RelExprTail}; add_rule(RelExprTail, rhs, 3); }
    { int rhs[] = {add_term(TOK_GT), ArithExpr, RelExprTail}; add_rule(RelExprTail, rhs, 3); }
    { int rhs[] = {add_term(TOK_LE), ArithExpr, RelExprTail}; add_rule(RelExprTail, rhs, 3); }
    { int rhs[] = {add_term(TOK_GE), ArithExpr, RelExprTail}; add_rule(RelExprTail, rhs, 3); }
    { int rhs[] = {sym_epsilon}; add_rule(RelExprTail, rhs, 1); }

    // ArithExpr -> Term ArithExprTail
    { int rhs[] = {Term, ArithExprTail}; add_rule(ArithExpr, rhs, 2); }
    { int rhs[] = {add_term(TOK_PLUS), Term, ArithExprTail}; add_rule(ArithExprTail, rhs, 3); }
    { int rhs[] = {add_term(TOK_MINUS), Term, ArithExprTail}; add_rule(ArithExprTail, rhs, 3); }
    { int rhs[] = {sym_epsilon}; add_rule(ArithExprTail, rhs, 1); }

    // Term -> Factor TermTail
    { int rhs[] = {Factor, TermTail}; add_rule(Term, rhs, 2); }
    { int rhs[] = {add_term(TOK_MUL), Factor, TermTail}; add_rule(TermTail, rhs, 3); }
    { int rhs[] = {add_term(TOK_DIV), Factor, TermTail}; add_rule(TermTail, rhs, 3); }
    { int rhs[] = {add_term(TOK_MOD), Factor, TermTail}; add_rule(TermTail, rhs, 3); }
    { int rhs[] = {sym_epsilon}; add_rule(TermTail, rhs, 1); }

    // Factor -> ID | NUM | ( BoolExpr ) | ! Factor
    { int rhs[] = {add_term(TOK_ID)}; add_rule(Factor, rhs, 1); }
    { int rhs[] = {add_term(TOK_NUM)}; add_rule(Factor, rhs, 1); }
    { int rhs[] = {add_term(TOK_LPAREN), BoolExpr, add_term(TOK_RPAREN)}; add_rule(Factor, rhs, 3); }
    { int rhs[] = {add_term(TOK_NOT), Factor}; add_rule(Factor, rhs, 2); }
}

bool first[MAX_SYMBOLS][MAX_SYMBOLS];
bool has_epsilon_first[MAX_SYMBOLS];
bool follow[MAX_SYMBOLS][MAX_SYMBOLS];

void compute_first() {
    memset(first, 0, sizeof(first));
    memset(has_epsilon_first, 0, sizeof(has_epsilon_first));

    for (int i = 0; i < symbol_count; i++) {
        if (symbols[i].type == SYM_TERMINAL) {
            first[i][i] = true;
        } else if (symbols[i].type == SYM_EPSILON) {
            has_epsilon_first[i] = true;
        }
    }

    bool changed = true;
    while (changed) {
        changed = false;
        for (int i = 0; i < rule_count; i++) {
            int lhs = rules[i].lhs;
            int rhs_count = rules[i].rhs_count;
            if (rhs_count == 1 && symbols[rules[i].rhs[0]].type == SYM_EPSILON) {
                if (!has_epsilon_first[lhs]) {
                    has_epsilon_first[lhs] = true;
                    changed = true;
                }
                continue;
            }

            bool all_have_epsilon = true;
            for (int j = 0; j < rhs_count; j++) {
                int sym = rules[i].rhs[j];
                bool sym_has_eps = has_epsilon_first[sym];

                for (int k = 0; k < symbol_count; k++) {
                    if (symbols[k].type == SYM_TERMINAL && first[sym][k] && !first[lhs][k]) {
                        first[lhs][k] = true;
                        changed = true;
                    }
                }
                if (!sym_has_eps) {
                    all_have_epsilon = false;
                    break;
                }
            }
            if (all_have_epsilon && !has_epsilon_first[lhs]) {
                has_epsilon_first[lhs] = true;
                changed = true;
            }
        }
    }
}

void compute_follow() {
    memset(follow, 0, sizeof(follow));
    follow[Program][sym_eof] = true;

    bool changed = true;
    while (changed) {
        changed = false;
        for (int i = 0; i < rule_count; i++) {
            int lhs = rules[i].lhs;
            int rhs_count = rules[i].rhs_count;

            for (int j = 0; j < rhs_count; j++) {
                int sym_B = rules[i].rhs[j];
                if (symbols[sym_B].type != SYM_NONTERMINAL) continue;

                bool next_all_epsilon = true;
                for (int k = j + 1; k < rhs_count; k++) {
                    int sym_beta = rules[i].rhs[k];
                    
                    for (int t = 0; t < symbol_count; t++) {
                        if (symbols[t].type == SYM_TERMINAL && first[sym_beta][t] && !follow[sym_B][t]) {
                            follow[sym_B][t] = true;
                            changed = true;
                        }
                    }

                    if (!has_epsilon_first[sym_beta]) {
                        next_all_epsilon = false;
                        break;
                    }
                }

                if (next_all_epsilon) {
                    for (int t = 0; t < symbol_count; t++) {
                        if (symbols[t].type == SYM_TERMINAL || symbols[t].type == SYM_EOF) {
                            if (follow[lhs][t] && !follow[sym_B][t]) {
                                follow[sym_B][t] = true;
                                changed = true;
                            }
                        }
                    }
                }
            }
        }
    }
}

void print_first_follow() {
    FILE *fout = fopen("first_follow.txt", "w");
    if (!fout) { printf("Failed to open first_follow.txt\n"); return; }

    printf("\n--- FIRST and FOLLOW Sets ---\n");
    fprintf(fout, "--- Dynamic Generated FIRST and FOLLOW Sets ---\n\n");

    for (int i = 0; i < symbol_count; i++) {
        if (symbols[i].type == SYM_NONTERMINAL) {
            // Print FIRST
            printf("FIRST(%s)  = { ", symbols[i].name);
            fprintf(fout, "FIRST(%s)  = { ", symbols[i].name);
            bool first_item = true;
            for (int j = 0; j < symbol_count; j++) {
                if (first[i][j]) {
                    if (!first_item) { printf(", "); fprintf(fout, ", "); }
                    printf("%s", symbols[j].name);
                    fprintf(fout, "%s", symbols[j].name);
                    first_item = false;
                }
            }
            if (has_epsilon_first[i]) {
                if (!first_item) { printf(", "); fprintf(fout, ", "); }
                printf("EPSILON");
                fprintf(fout, "EPSILON");
            }
            printf(" }\n");
            fprintf(fout, " }\n");

            // Print FOLLOW
            printf("FOLLOW(%s) = { ", symbols[i].name);
            fprintf(fout, "FOLLOW(%s) = { ", symbols[i].name);
            first_item = true;
            for (int j = 0; j < symbol_count; j++) {
                if (follow[i][j]) {
                    if (!first_item) { printf(", "); fprintf(fout, ", "); }
                    printf("%s", symbols[j].name);
                    fprintf(fout, "%s", symbols[j].name);
                    first_item = false;
                }
            }
            printf(" }\n\n");
            fprintf(fout, " }\n\n");
        }
    }
    fclose(fout);
    printf("FIRST and FOLLOW sets saved to first_follow.txt\n");
}

int ll1_table[MAX_SYMBOLS][MAX_SYMBOLS];

void build_ll1_table() {
    for (int i = 0; i < MAX_SYMBOLS; i++) {
        for (int j = 0; j < MAX_SYMBOLS; j++) {
            ll1_table[i][j] = -1;
        }
    }

    for (int i = 0; i < rule_count; i++) {
        int lhs = rules[i].lhs;
        int rhs_count = rules[i].rhs_count;

        bool rhs_has_epsilon = true;
        for (int j = 0; j < rhs_count; j++) {
            int sym = rules[i].rhs[j];
            if (symbols[sym].type == SYM_EPSILON) {
                break;
            }

            for (int k = 0; k < symbol_count; k++) {
                if (symbols[k].type == SYM_TERMINAL && first[sym][k]) {
                    if (ll1_table[lhs][k] != -1 && ll1_table[lhs][k] != i) {
                        // NOTE: For simplicity of the assignment we just overwrite or ignore conflicts
                    }
                    ll1_table[lhs][k] = i;
                }
            }

            if (!has_epsilon_first[sym]) {
                rhs_has_epsilon = false;
                break;
            }
        }

        if (rhs_has_epsilon) {
            for (int k = 0; k < symbol_count; k++) {
                if ((symbols[k].type == SYM_TERMINAL || symbols[k].type == SYM_EOF) && follow[lhs][k]) {
                    ll1_table[lhs][k] = i;
                }
            }
        }
    }
}

void print_ll1_table() {
    printf("\n--- LL(1) Parsing Table (Partial View) ---\n");
    for (int i = 0; i < symbol_count; i++) {
        if (symbols[i].type == SYM_NONTERMINAL) {
            bool has_entries = false;
            for (int j = 0; j < symbol_count; j++) {
                if (ll1_table[i][j] != -1) {
                    if (!has_entries) {
                        printf("%-15s | ", symbols[i].name);
                        has_entries = true;
                    }
                    int r = ll1_table[i][j];
                    printf("%s -> ", symbols[j].name);
                    for(int k=0; k<rules[r].rhs_count; k++) {
                        printf("%s ", symbols[rules[r].rhs[k]].name);
                    }
                    printf("| ");
                }
            }
            if (has_entries) printf("\n");
        }
    }
}

// --- LL(1) Parsing ---
void parse_ll1() {
    FILE *fout = fopen("ll1_trace.txt", "w");
    if (!fout) { printf("Failed to open ll1_trace.txt\n"); return; }
    
    int stack[1000];
    int top = 0;

    stack[top++] = sym_eof;
    stack[top++] = Program;

    read_index = 0;
    Token curr = all_tokens[read_index];
    int curr_sym;

    fprintf(fout, "--- LL(1) Stack Trace ---\n\n");
    while (top > 0) {
        int X = stack[--top];

        for (int i = 0; i < symbol_count; i++) {
            if (symbols[i].token_id == curr.type) {
                curr_sym = i;
                break;
            }
        }
        if (curr.type == TOK_EOF) curr_sym = sym_eof;

        fprintf(fout, "Stack: [ ");
        for (int k = 0; k <= top; k++) {
            fprintf(fout, "%s ", symbols[stack[k]].name);
        }
        fprintf(fout, "]\nInput: %s (Type: %d)\n", curr.lexeme, curr.type);

        if (X == curr_sym) {
            fprintf(fout, "Action: Match %s\n\n", symbols[X].name);
            read_index++;
            if (read_index < token_count) curr = all_tokens[read_index];
            else { curr.type = TOK_EOF; strcpy(curr.lexeme, "$"); }
        } else if (symbols[X].type == SYM_TERMINAL || symbols[X].type == SYM_EOF) {
            fprintf(fout, "Action: ERROR! Expected %s but got %s\n\n", symbols[X].name, symbols[curr_sym].name);
            break;
        } else if (ll1_table[X][curr_sym] == -1) {
            fprintf(fout, "Action: ERROR! No rule in table for [%s, %s]\n\n", symbols[X].name, symbols[curr_sym].name);
            break; // Attempt panic mode recovery if needed, here we just break
        } else {
            int r = ll1_table[X][curr_sym];
            fprintf(fout, "Action: Predict Rule %s -> ", symbols[X].name);
            for(int k=0; k<rules[r].rhs_count; k++) fprintf(fout, "%s ", symbols[rules[r].rhs[k]].name);
            fprintf(fout, "\n\n");

            if (rules[r].rhs[0] != sym_epsilon) {
                for (int i = rules[r].rhs_count - 1; i >= 0; i--) {
                    stack[top++] = rules[r].rhs[i];
                }
            }
        }
    }
    
    if (top == 0) {
        fprintf(fout, "LL(1) Parsing Result: SUCCESS\n");
        printf("LL(1) Parsing completed successfully. Trace saved to ll1_trace.txt\n");
    } else {
        printf("LL(1) Parsing failed. Trace saved to ll1_trace.txt\n");
    }
    fclose(fout);
}

// --- Naive Shift-Reduce Parsing ---
void parse_shift_reduce() {
    FILE *fout = fopen("shift_reduce_trace.txt", "w");
    if (!fout) { printf("Failed to open shift_reduce_trace.txt\n"); return; }
    
    int stack[1000];
    int top = 0;
    
    read_index = 0;
    Token curr = all_tokens[read_index];

    fprintf(fout, "--- Shift-Reduce Stack Trace ---\n\n");

    while (true) {
        bool reduced = false;

        for (int r = rule_count - 1; r >= 0; r--) {
            int rhs_len = rules[r].rhs_count;
            if (rhs_len == 1 && rules[r].rhs[0] == sym_epsilon) continue; 

            if (top >= rhs_len) {
                bool match = true;
                for (int k = 0; k < rhs_len; k++) {
                    if (stack[top - rhs_len + k] != rules[r].rhs[k]) {
                        match = false;
                        break;
                    }
                }
                
                if (match && symbols[rules[r].lhs].name[0] == 'F' && top >= 2 && symbols[stack[top-rhs_len - 1]].token_id == TOK_ASSIGN) {
                    // special naive heuristic logic, ignoring
                }

                if (match) {
                    fprintf(fout, "Stack before Reduce: [ ");
                    for(int z=0; z<top; z++) fprintf(fout, "%s ", symbols[stack[z]].name);
                    fprintf(fout, "]\nAction: REDUCE %s -> ", symbols[rules[r].lhs].name);
                    for(int z=0; z<rhs_len; z++) fprintf(fout, "%s ", symbols[rules[r].rhs[z]].name);
                    fprintf(fout, "\n\n");

                    top -= rhs_len;
                    stack[top++] = rules[r].lhs;
                    reduced = true;
                    break;
                }
            }
        }

        if (!reduced) {
            if (curr.type == TOK_EOF) break;

            int curr_sym = -1;
            for (int i = 0; i < symbol_count; i++) {
                if (symbols[i].token_id == curr.type) {
                    curr_sym = i; break; 
                }
            }

            fprintf(fout, "Stack before Shift: [ ");
            for(int z=0; z<top; z++) fprintf(fout, "%s ", symbols[stack[z]].name);
            fprintf(fout, "]\nAction: SHIFT %s\n\n", curr.lexeme);

            stack[top++] = curr_sym;
            
            read_index++;
            if (read_index < token_count) curr = all_tokens[read_index];
            else { curr.type = TOK_EOF; strcpy(curr.lexeme, "$"); }
        }
    }
    
    if (top == 1 && stack[0] == Program) {
        fprintf(fout, "Shift-Reduce Parsing Result: SUCCESS\n");
        printf("Shift-Reduce Parsing completed successfully. Trace saved to shift_reduce_trace.txt\n");
    } else {
        printf("Shift-Reduce Parsing ended. Trace saved to shift_reduce_trace.txt\n");
    }
    fclose(fout);
}

// --- Main Execution ---
int main() {
    char user_input[4096] = {0};
    char ch;
    int i = 0;

    printf("Reading code snippet... \n");
    while ((ch = getchar()) != EOF && i < sizeof(user_input) - 1) {
        user_input[i++] = ch;
    }
    user_input[i] = '\0';

    if (strlen(user_input) == 0) {
        return 0;
    }

    input_ptr = user_input;
    advance_lexer();
    while (current_token.type != TOK_EOF) {
        all_tokens[token_count++] = current_token;
        advance_lexer();
    }
    current_token.type = TOK_EOF;
    strcpy(current_token.lexeme, "EOF");
    all_tokens[token_count++] = current_token;

    init_grammar();
    compute_first();
    compute_follow();
    build_ll1_table();

    print_first_follow();
    print_ll1_table();

    printf("\nRunning LL(1) Table Driven Parser...\n");
    parse_ll1();

    printf("\nRunning Naive Shift-Reduce Parser...\n");
    parse_shift_reduce();

    return 0;
}
