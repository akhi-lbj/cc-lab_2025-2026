/*
 * Week 6: Three-Address Code (TAC) Generation + Quadruples + Triples
 * ---------------------------------------------------------------------------
 * Builds on Week 5 (Symbol Table + Semantic Analysis) and adds:
 * Phase 6 - Three-Address Code (TAC) Generation
 * Phase 6b - Quadruple Table
 * Phase 6c - Triples Table (Added feature)
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ========================================================================
 * PART 1 — TOKEN DEFINITIONS
 * ======================================================================== */
typedef enum {
  TOK_INT,
  TOK_FLOAT,
  TOK_VOID,
  TOK_BOOL,
  TOK_IF,
  TOK_ELSE,
  TOK_WHILE,
  TOK_PRINT,
  TOK_RETURN,
  TOK_CLASS,
  TOK_TRUE,
  TOK_FALSE,
  TOK_ID,
  TOK_NUM,
  TOK_ASSIGN,
  TOK_PLUS,
  TOK_MINUS,
  TOK_MUL,
  TOK_DIV,
  TOK_MOD,
  TOK_LT,
  TOK_GT,
  TOK_LE,
  TOK_GE,
  TOK_EQ,
  TOK_NEQ,
  TOK_AND,
  TOK_OR,
  TOK_NOT,
  TOK_LPAREN,
  TOK_RPAREN,
  TOK_LBRACE,
  TOK_RBRACE,
  TOK_SEMI,
  TOK_COMMA,
  TOK_COLON,
  TOK_EOF,
  TOK_ERROR
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
  char type[16];
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
int lex_error_count = 0;
int sem_error_count = 0;
int syntax_error_count = 0;
int has_error = 0; /* Global error flag: if set, stop all further processing */

void push_scope(void) {
  ScopeNode *nn = (ScopeNode *)malloc(sizeof(ScopeNode));
  nn->count = 0;
  nn->scope_level = scope_counter++;
  nn->next = top;
  top = nn;
  printf(" [ST] >>> Entered new scope (level %d)\n", nn->scope_level);
}

void pop_scope(void) {
  if (!top)
    return;
  printf(" [ST] <<< Leaving scope (level %d) -- dropping %d symbol(s):\n",
         top->scope_level, top->count);
  for (int i = 0; i < top->count; i++)
    printf("       - %s (%s, offset=%d)\n", top->symbols[i].name,
           top->symbols[i].type, top->symbols[i].offset);
  ScopeNode *save = top->next;
  free(top);
  top = save;
}

void insert_symbol(const char *name, const char *type) {
  if (!top)
    return;
  for (int i = 0; i < top->count; i++) {
    if (strcmp(top->symbols[i].name, name) == 0) {
      printf(" [ST] WARNING: '%s' already declared in scope %d\n", name,
             top->scope_level);
      return;
    }
  }
  if (top->count >= MAX_SYMBOLS)
    return;
  Symbol *sym = &top->symbols[top->count++];
  strncpy(sym->name, name, 63);
  sym->name[63] = '\0';
  strncpy(sym->type, type, 15);
  sym->type[15] = '\0';
  sym->scope_level = top->scope_level;
  sym->offset = global_offset;
  global_offset += 4;
  printf(" [ST] INSERT: name=%-10s type=%-6s scope=%-3d offset=%d\n", sym->name,
         sym->type, sym->scope_level, sym->offset);
}

Symbol *searchSym(ScopeNode *S, const char *var) {
  for (ScopeNode *cur = S; cur; cur = cur->next)
    for (int i = 0; i < cur->count; i++)
      if (strcmp(cur->symbols[i].name, var) == 0) {
        printf(" [ST] LOOKUP '%s': FOUND in scope %d -- type=%s, offset=%d\n",
               var, cur->symbols[i].scope_level, cur->symbols[i].type,
               cur->symbols[i].offset);
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
    printf(" | %-12s %-8s %-6s %-8s\n", "----------", "------", "-----",
           "------");
    for (int i = 0; i < cur->count; i++)
      printf(" | %-12s %-8s %-6d %-8d\n", cur->symbols[i].name,
             cur->symbols[i].type, cur->symbols[i].scope_level,
             cur->symbols[i].offset);
    cur = cur->next;
  }
  printf(" +====================================================+\n\n");
}

void sem_error(int line, const char *msg) {
  sem_error_count++;
  has_error = 1;
  printf("  [SEMANTIC ERROR #%d] Line %d: %s\n", sem_error_count, line, msg);
}

void syntax_error(int line, const char *msg) {
  syntax_error_count++;
  has_error = 1;
  printf("  [SYNTAX ERROR #%d] Line %d: %s\n", syntax_error_count, line, msg);
}

void lex_error(int line, const char *msg) {
  lex_error_count++;
  has_error = 1;
  printf("  [LEXICAL ERROR #%d] Line %d: %s\n", lex_error_count, line, msg);
}

void tac_error(int line, const char *msg) {
  has_error = 1;
  printf("  [TAC GENERATION ERROR #%d] Line %d: %s\n", 1, line, msg);
}

void optimize_error(int line, const char *msg) {
  has_error = 1;
  printf("  [OPTIMIZATION ERROR #%d] Line %d: %s\n", 1, line, msg);
}

/* ========================================================================
 * PART 3 — AST NODE DEFINITIONS
 * ======================================================================== */
typedef enum {
  NODE_VAR_DECL,
  NODE_FUNC_DEF,
  NODE_CLASS_DEF,
  NODE_ASSIGN,
  NODE_IF,
  NODE_WHILE,
  NODE_PRINT,
  NODE_RETURN,
  NODE_BLOCK,
  NODE_STMT_LIST,
  NODE_BINARY_OP,
  NODE_UNARY_OP,
  NODE_VAR_REF,
  NODE_LITERAL,
  NODE_LABEL,
  NODE_PARAM_LIST
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

ASTNode *create_node(NodeKind kind, int line) {
  ASTNode *n = (ASTNode *)malloc(sizeof(ASTNode));
  n->kind = kind;
  n->line = line;
  n->child_count = 0;
  n->op = TOK_ERROR;
  n->lexeme[0] = '\0';
  n->type_str[0] = '\0';
  for (int i = 0; i < 16; i++)
    n->children[i] = NULL;
  return n;
}

/* ========================================================================
 * PART 4 — LEXER
 * ======================================================================== */
Token current_token;
char *input_ptr;
int current_line = 1;

void advance(void) {
  while (1) {
    if (isspace(*input_ptr)) {
      if (*input_ptr == '\n')
        current_line++;
      input_ptr++;
    } else if (*input_ptr == '/' && *(input_ptr + 1) == '*') {
      input_ptr += 2;
      while (*input_ptr != '\0' &&
             !(*input_ptr == '*' && *(input_ptr + 1) == '/')) {
        if (*input_ptr == '\n')
          current_line++;
        input_ptr++;
      }
      if (*input_ptr != '\0')
        input_ptr += 2;
    } else
      break;
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
      if (i < 63)
        current_token.lexeme[i++] = *input_ptr;
      input_ptr++;
    }
    current_token.lexeme[i] = '\0';
    if (strcmp(current_token.lexeme, "int") == 0)
      current_token.type = TOK_INT;
    else if (strcmp(current_token.lexeme, "float") == 0)
      current_token.type = TOK_FLOAT;
    else if (strcmp(current_token.lexeme, "void") == 0)
      current_token.type = TOK_VOID;
    else if (strcmp(current_token.lexeme, "bool") == 0)
      current_token.type = TOK_BOOL;
    else if (strcmp(current_token.lexeme, "if") == 0)
      current_token.type = TOK_IF;
    else if (strcmp(current_token.lexeme, "else") == 0)
      current_token.type = TOK_ELSE;
    else if (strcmp(current_token.lexeme, "while") == 0)
      current_token.type = TOK_WHILE;
    else if (strcmp(current_token.lexeme, "print") == 0)
      current_token.type = TOK_PRINT;
    else if (strcmp(current_token.lexeme, "return") == 0)
      current_token.type = TOK_RETURN;
    else if (strcmp(current_token.lexeme, "class") == 0)
      current_token.type = TOK_CLASS;
    else if (strcmp(current_token.lexeme, "true") == 0)
      current_token.type = TOK_TRUE;
    else if (strcmp(current_token.lexeme, "false") == 0)
      current_token.type = TOK_FALSE;
    else
      current_token.type = TOK_ID;
    return;
  }

  if (isdigit(*input_ptr)) {
    int i = 0;
    while (isdigit(*input_ptr)) {
      if (i < 63)
        current_token.lexeme[i++] = *input_ptr;
      input_ptr++;
    }
    if (*input_ptr == '.') {
      if (i < 63) {
        current_token.lexeme[i++] = *input_ptr;
      }
      input_ptr++;
      while (isdigit(*input_ptr)) {
        if (i < 63)
          current_token.lexeme[i++] = *input_ptr;
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
  case '(':
    current_token.type = TOK_LPAREN;
    input_ptr++;
    break;
  case ')':
    current_token.type = TOK_RPAREN;
    input_ptr++;
    break;
  case '{':
    current_token.type = TOK_LBRACE;
    input_ptr++;
    break;
  case '}':
    current_token.type = TOK_RBRACE;
    input_ptr++;
    break;
  case ';':
    current_token.type = TOK_SEMI;
    input_ptr++;
    break;
  case ',':
    current_token.type = TOK_COMMA;
    input_ptr++;
    break;
  case ':':
    current_token.type = TOK_COLON;
    input_ptr++;
    break;
  case '+':
    current_token.type = TOK_PLUS;
    input_ptr++;
    break;
  case '-':
    current_token.type = TOK_MINUS;
    input_ptr++;
    break;
  case '*':
    current_token.type = TOK_MUL;
    input_ptr++;
    break;
  case '/':
    current_token.type = TOK_DIV;
    input_ptr++;
    break;
  case '%':
    current_token.type = TOK_MOD;
    input_ptr++;
    break;
  case '=':
    input_ptr++;
    if (*input_ptr == '=') {
      current_token.type = TOK_EQ;
      strcpy(current_token.lexeme, "==");
      input_ptr++;
    } else
      current_token.type = TOK_ASSIGN;
    break;
  case '<':
    input_ptr++;
    if (*input_ptr == '=') {
      current_token.type = TOK_LE;
      strcpy(current_token.lexeme, "<=");
      input_ptr++;
    } else
      current_token.type = TOK_LT;
    break;
  case '>':
    input_ptr++;
    if (*input_ptr == '=') {
      current_token.type = TOK_GE;
      strcpy(current_token.lexeme, ">=");
      input_ptr++;
    } else
      current_token.type = TOK_GT;
    break;
  case '!':
    input_ptr++;
    if (*input_ptr == '=') {
      current_token.type = TOK_NEQ;
      strcpy(current_token.lexeme, "!=");
      input_ptr++;
    } else
      current_token.type = TOK_NOT;
    break;
  case '&':
    input_ptr++;
    if (*input_ptr == '&') {
      current_token.type = TOK_AND;
      strcpy(current_token.lexeme, "&&");
      input_ptr++;
    } else
      current_token.type = TOK_ERROR;
    break;
  case '|':
    input_ptr++;
    if (*input_ptr == '|') {
      current_token.type = TOK_OR;
      strcpy(current_token.lexeme, "||");
      input_ptr++;
    } else
      current_token.type = TOK_ERROR;
    break;
  default:
    current_token.type = TOK_ERROR;
    input_ptr++;
    break;
  }
}

static const char *token_category(TokenType t) {
  switch (t) {
  case TOK_INT:
  case TOK_FLOAT:
  case TOK_VOID:
  case TOK_BOOL:
  case TOK_IF:
  case TOK_ELSE:
  case TOK_WHILE:
  case TOK_PRINT:
  case TOK_RETURN:
  case TOK_CLASS:
    return "Keyword";
  case TOK_TRUE:
  case TOK_FALSE:
    return "Boolean Literal";
  case TOK_ID:
    return "Identifier";
  case TOK_NUM:
    return "Number";
  case TOK_ASSIGN:
    return "Assignment Op";
  case TOK_PLUS:
  case TOK_MINUS:
  case TOK_MUL:
  case TOK_DIV:
  case TOK_MOD:
    return "Arithmetic Op";
  case TOK_LT:
  case TOK_GT:
  case TOK_LE:
  case TOK_GE:
  case TOK_EQ:
  case TOK_NEQ:
    return "Relational Op";
  case TOK_AND:
  case TOK_OR:
  case TOK_NOT:
    return "Logical Op";
  case TOK_LPAREN:
  case TOK_RPAREN:
  case TOK_LBRACE:
  case TOK_RBRACE:
  case TOK_SEMI:
  case TOK_COMMA:
  case TOK_COLON:
    return "Punctuation";
  default:
    return "Unknown";
  }
}

int run_lexical_analysis(char *src) {
  printf("+===================================================================="
         "+\n");
  printf("| PHASE 1: LEXICAL ANALYSIS                                          "
         "|\n");
  printf("+===================================================================="
         "+\n");
  printf("| %-5s | %-16s | %-20s | %-4s |\n", "No.", "Lexeme", "Category",
         "Line");
  printf("+-------+-----------------+---------------------+------+\n");

  char *saved_ptr = input_ptr;
  int saved_line = current_line;
  Token saved_tok = current_token;

  input_ptr = src;
  current_line = 1;
  advance();
  lex_error_count = 0;

  int idx = 1;
  while (current_token.type != TOK_EOF && current_token.type != TOK_ERROR) {
    printf("| %-5d | %-16s | %-20s | %-4d |\n", idx, current_token.lexeme,
           token_category(current_token.type), current_token.line);
    idx++;
    advance();
  }

  if (current_token.type == TOK_ERROR) {
    lex_error_count = 1;
    has_error = 1;
    lex_error(current_token.line, current_token.lexeme);
    printf("+===================================================================="
           "+\n");
    printf(" *** Lexical Error on line %d: Invalid token '%s'\n\n",
           current_token.line, current_token.lexeme);
  } else {
    printf("+===================================================================="
           "+\n");
    printf("| Total tokens: %-4d                                                 "
           "|\n",
           idx - 1);
    printf("+===================================================================="
           "+\n\n");
  }

  input_ptr = saved_ptr;
  current_line = saved_line;
  current_token = saved_tok;
  return lex_error_count;
}

/* ========================================================================
 * PART 5 — PARSER (recursive-descent, builds AST)
 * ======================================================================== */
void match(TokenType expected) {
  if (current_token.type == expected) {
    advance();
  } else {
    syntax_error(current_token.line, 
      "Expected token, got unexpected token");
    printf(" *** Syntax Error on line %d: Expected token %d, got '%s'\n",
           current_token.line, expected, current_token.lexeme);
  }
}

ASTNode *parse_BoolExpr(void);
ASTNode *parse_Stmt(void);
ASTNode *parse_StmtList(void);
ASTNode *parse_Block(void);

ASTNode *parse_Factor(void) {
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
  } else if (current_token.type == TOK_TRUE ||
             current_token.type == TOK_FALSE) {
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
  } else if (current_token.type == TOK_MINUS) {
    node = create_node(NODE_UNARY_OP, current_token.line);
    node->op = TOK_MINUS;
    advance();
    node->children[0] = parse_Factor();
    node->child_count = 1;
  } else {
    syntax_error(current_token.line, "Unexpected token in expression");
    printf(
        " *** Syntax Error on line %d: Unexpected token '%s' in expression\n",
        current_token.line, current_token.lexeme);
  }
  return node;
}

ASTNode *parse_Term(void) {
  ASTNode *left = parse_Factor();
  while (current_token.type == TOK_MUL || current_token.type == TOK_DIV ||
         current_token.type == TOK_MOD) {
    ASTNode *n = create_node(NODE_BINARY_OP, current_token.line);
    n->op = current_token.type;
    advance();
    n->children[0] = left;
    n->children[1] = parse_Factor();
    n->child_count = 2;
    left = n;
  }
  return left;
}

ASTNode *parse_ArithExpr(void) {
  ASTNode *left = parse_Term();
  while (current_token.type == TOK_PLUS || current_token.type == TOK_MINUS) {
    ASTNode *n = create_node(NODE_BINARY_OP, current_token.line);
    n->op = current_token.type;
    advance();
    n->children[0] = left;
    n->children[1] = parse_Term();
    n->child_count = 2;
    left = n;
  }
  return left;
}

ASTNode *parse_RelExpr(void) {
  ASTNode *left = parse_ArithExpr();
  while (current_token.type == TOK_LT || current_token.type == TOK_GT ||
         current_token.type == TOK_LE || current_token.type == TOK_GE) {
    ASTNode *n = create_node(NODE_BINARY_OP, current_token.line);
    n->op = current_token.type;
    advance();
    n->children[0] = left;
    n->children[1] = parse_ArithExpr();
    n->child_count = 2;
    left = n;
  }
  return left;
}

ASTNode *parse_EqExpr(void) {
  ASTNode *left = parse_RelExpr();
  while (current_token.type == TOK_EQ || current_token.type == TOK_NEQ) {
    ASTNode *n = create_node(NODE_BINARY_OP, current_token.line);
    n->op = current_token.type;
    advance();
    n->children[0] = left;
    n->children[1] = parse_RelExpr();
    n->child_count = 2;
    left = n;
  }
  return left;
}

ASTNode *parse_AndExpr(void) {
  ASTNode *left = parse_EqExpr();
  while (current_token.type == TOK_AND) {
    ASTNode *n = create_node(NODE_BINARY_OP, current_token.line);
    n->op = TOK_AND;
    advance();
    n->children[0] = left;
    n->children[1] = parse_EqExpr();
    n->child_count = 2;
    left = n;
  }
  return left;
}

ASTNode *parse_BoolExpr(void) {
  ASTNode *left = parse_AndExpr();
  while (current_token.type == TOK_OR) {
    ASTNode *n = create_node(NODE_BINARY_OP, current_token.line);
    n->op = TOK_OR;
    advance();
    n->children[0] = left;
    n->children[1] = parse_AndExpr();
    n->child_count = 2;
    left = n;
  }
  return left;
}

ASTNode *parse_Block(void) {
  ASTNode *n = create_node(NODE_BLOCK, current_token.line);
  match(TOK_LBRACE);
  n->children[0] = parse_StmtList();
  match(TOK_RBRACE);
  n->child_count = 1;
  return n;
}

ASTNode *parse_Stmt(void) {
  if (current_token.type == TOK_INT || current_token.type == TOK_FLOAT ||
      current_token.type == TOK_VOID || current_token.type == TOK_BOOL) {

    char declared_type[16];
    strcpy(declared_type, current_token.lexeme);
    int line = current_token.line;
    advance();

    char name[64];
    strncpy(name, current_token.lexeme, 63);
    name[63] = '\0';
    match(TOK_ID);

    if (current_token.type == TOK_LPAREN) {
      ASTNode *n = create_node(NODE_FUNC_DEF, line);
      strcpy(n->lexeme, name);
      strcpy(n->type_str, declared_type);
      match(TOK_LPAREN);
      ASTNode *params = create_node(NODE_PARAM_LIST, line);
      if (current_token.type != TOK_RPAREN) {
        do {
          if (current_token.type == TOK_COMMA)
            match(TOK_COMMA);
          ASTNode *p = create_node(NODE_VAR_DECL, current_token.line);
          strcpy(p->type_str, current_token.lexeme);
          match(current_token.type);
          strncpy(p->lexeme, current_token.lexeme, 63);
          p->lexeme[63] = '\0';
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
      ASTNode *list = create_node(NODE_STMT_LIST, line);
      ASTNode *d = create_node(NODE_VAR_DECL, line);
      strcpy(d->lexeme, name);
      strcpy(d->type_str, declared_type);
      list->children[list->child_count++] = d;

      while (current_token.type == TOK_COMMA) {
        match(TOK_COMMA);
        ASTNode *d2 = create_node(NODE_VAR_DECL, current_token.line);
        strncpy(d2->lexeme, current_token.lexeme, 63);
        d2->lexeme[63] = '\0';
        strcpy(d2->type_str, declared_type);
        match(TOK_ID);
        list->children[list->child_count++] = d2;
      }

      if (current_token.type == TOK_ASSIGN) {
        ASTNode *a = create_node(NODE_ASSIGN, current_token.line);
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
    char name[64];
    strncpy(name, current_token.lexeme, 63);
    name[63] = '\0';
    int line = current_token.line;
    advance();

    if (current_token.type == TOK_COLON) {
      ASTNode *n = create_node(NODE_LABEL, line);
      strcpy(n->lexeme, name);
      match(TOK_COLON);
      return n;
    } else if (current_token.type == TOK_ASSIGN) {
      ASTNode *n = create_node(NODE_ASSIGN, line);
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
    ASTNode *n = create_node(NODE_IF, current_token.line);
    match(TOK_IF);
    match(TOK_LPAREN);
    n->children[0] = parse_BoolExpr();
    match(TOK_RPAREN);
    n->children[1] = parse_Block();
    n->child_count = 2;
    if (current_token.type == TOK_ELSE) {
      match(TOK_ELSE);
      n->children[2] = parse_Block();
      n->child_count = 3;
    }
    return n;

  } else if (current_token.type == TOK_WHILE) {
    ASTNode *n = create_node(NODE_WHILE, current_token.line);
    match(TOK_WHILE);
    match(TOK_LPAREN);
    n->children[0] = parse_BoolExpr();
    match(TOK_RPAREN);
    n->children[1] = parse_Block();
    n->child_count = 2;
    return n;

  } else if (current_token.type == TOK_PRINT) {
    ASTNode *n = create_node(NODE_PRINT, current_token.line);
    match(TOK_PRINT);
    match(TOK_LPAREN);
    n->children[0] = parse_BoolExpr();
    match(TOK_RPAREN);
    match(TOK_SEMI);
    n->child_count = 1;
    return n;

  } else if (current_token.type == TOK_CLASS) {
    ASTNode *n = create_node(NODE_CLASS_DEF, current_token.line);
    match(TOK_CLASS);
    strncpy(n->lexeme, current_token.lexeme, 63);
    n->lexeme[63] = '\0';
    match(TOK_ID);
    match(TOK_LBRACE);
    n->children[0] = parse_StmtList();
    match(TOK_RBRACE);
    n->child_count = 1;
    return n;

  } else if (current_token.type == TOK_RETURN) {
    ASTNode *n = create_node(NODE_RETURN, current_token.line);
    match(TOK_RETURN);
    if (current_token.type != TOK_SEMI) {
      n->children[0] = parse_BoolExpr();
      n->child_count = 1;
    }
    match(TOK_SEMI);
    return n;

  } else if (current_token.type == TOK_LBRACE) {
    return parse_Block();
  }
  return NULL;
}

ASTNode *parse_StmtList(void) {
  ASTNode *list = create_node(NODE_STMT_LIST, current_token.line);
  while (current_token.type != TOK_RBRACE && current_token.type != TOK_EOF) {
    ASTNode *s = parse_Stmt();
    if (s)
      list->children[list->child_count++] = s;
  }
  return list;
}

/* ========================================================================
 * PART 6 — SEMANTIC ANALYSIS (AST traversal: type checking + scope)
 * ======================================================================== */
static int is_num(const char *t) {
  return t && (strcmp(t, "int") == 0 || strcmp(t, "float") == 0);
}
static int is_bl(const char *t) { return t && strcmp(t, "bool") == 0; }

const char *eval_type(ASTNode *n) {
  if (!n)
    return "void";
  if (n->kind == NODE_LITERAL)
    return n->type_str;
  if (n->kind == NODE_VAR_REF) {
    Symbol *s = searchSym(top, n->lexeme);
    if (s)
      return s->type;
    char msg[256];
    sprintf(msg, "Undeclared variable '%s'", n->lexeme);
    sem_error(n->line, msg);
    return "error";
  }
  if (n->kind == NODE_BINARY_OP) {
    const char *t1 = eval_type(n->children[0]);
    const char *t2 = eval_type(n->children[1]);
    if (n->op == TOK_PLUS || n->op == TOK_MINUS || n->op == TOK_MUL ||
        n->op == TOK_DIV || n->op == TOK_MOD) {
      if (!is_num(t1) || !is_num(t2)) {
        sem_error(n->line, "Type mismatch: numeric operands required");
        return "error";
      }
      return (strcmp(t1, "float") == 0 || strcmp(t2, "float") == 0) ? "float"
                                                                    : "int";
    }
    return "bool";
  }
  if (n->kind == NODE_UNARY_OP) {
    if (n->op == TOK_NOT)
      return "bool";
    return eval_type(n->children[0]);
  }
  return "void";
}

void analyze(ASTNode *n) {
  if (!n)
    return;
  switch (n->kind) {
  case NODE_STMT_LIST:
    for (int i = 0; i < n->child_count; i++)
      analyze(n->children[i]);
    break;

  case NODE_VAR_DECL:
    insert_symbol(n->lexeme, n->type_str);
    break;

  case NODE_FUNC_DEF:
    insert_symbol(n->lexeme, "proc");
    push_scope();
    analyze(n->children[0]);
    analyze(n->children[1]);
    pop_scope();
    break;

  case NODE_PARAM_LIST:
    for (int i = 0; i < n->child_count; i++)
      analyze(n->children[i]);
    break;

  case NODE_BLOCK:
    push_scope();
    analyze(n->children[0]);
    print_all_scopes();
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
    Symbol *s = searchSym(top, n->lexeme);
    const char *rt = eval_type(n->children[0]);
    if (!s) {
      char msg[256];
      sprintf(msg, "Assigning to undeclared variable '%s'", n->lexeme);
      sem_error(n->line, msg);
    } else if (strcmp(s->type, rt) != 0 &&
               !(strcmp(s->type, "float") == 0 && strcmp(rt, "int") == 0)) {
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
    for (int i = 0; i < n->child_count; i++)
      analyze(n->children[i]);
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

TAC *tac_head = NULL;
TAC *tac_tail = NULL;
int temp_counter = 1;
int label_counter = 1;

#define MAX_QUADS 1024

typedef struct {
  char op[16];
  char arg1[64];
  char arg2[64];
  char result[64];
} Quadruple;

static Quadruple quad_table[MAX_QUADS];
static int quad_count = 0;

/* ── Triples table (NEW FEATURE) ─────────────────────────────────── */
typedef struct {
  int index; // Uses -1 to represent labels visually without advancing index
  char op[16];
  char arg1[64];
  char arg2[64];
} Triple;

static Triple triple_table[MAX_QUADS * 3];
static int triple_count = 0;
static int next_triple_idx = 0;

// Mapping to translate 't' temporaries into Triple indices like (0), (1)...
static char temp_map_name[MAX_QUADS][64];
static int temp_map_idx[MAX_QUADS];
static int temp_map_count = 0;

void add_temp_map(const char *name, int idx) {
  strcpy(temp_map_name[temp_map_count], name);
  temp_map_idx[temp_map_count] = idx;
  temp_map_count++;
}

int get_triple_idx(const char *name) {
  if (!name || name[0] != 't' || !isdigit(name[1]))
    return -1;
  for (int i = 0; i < temp_map_count; i++) {
    if (strcmp(temp_map_name[i], name) == 0)
      return temp_map_idx[i];
  }
  return -1;
}

char *new_temp(void) {
  char *t = (char *)malloc(16);
  sprintf(t, "t%d", temp_counter++);
  return t;
}

char *new_label(void) {
  char *l = (char *)malloc(16);
  sprintf(l, "L%d", label_counter++);
  return l;
}

void emit(const char *op, const char *arg1, const char *arg2,
          const char *result) {
  TAC *instr = (TAC *)malloc(sizeof(TAC));
  strncpy(instr->op, op ? op : "", 15);
  instr->op[15] = '\0';
  strncpy(instr->arg1, arg1 ? arg1 : "", 63);
  instr->arg1[63] = '\0';
  strncpy(instr->arg2, arg2 ? arg2 : "", 63);
  instr->arg2[63] = '\0';
  strncpy(instr->result, result ? result : "", 63);
  instr->result[63] = '\0';
  instr->next = NULL;
  if (!tac_head)
    tac_head = tac_tail = instr;
  else {
    tac_tail->next = instr;
    tac_tail = instr;
  }

  if (quad_count < MAX_QUADS) {
    Quadruple *q = &quad_table[quad_count++];
    strncpy(q->op, op ? op : "", 15);
    q->op[15] = '\0';
    strncpy(q->arg1, arg1 ? arg1 : "", 63);
    q->arg1[63] = '\0';
    strncpy(q->arg2, arg2 ? arg2 : "", 63);
    q->arg2[63] = '\0';
    strncpy(q->result, result ? result : "", 63);
    q->result[63] = '\0';
  }
}

void print_tac(void) {
  printf("\n+=================================================================="
         "==+\n");
  printf("| PHASE 6: GENERATED THREE-ADDRESS CODE (TAC)                        "
         "|\n");
  printf("+===================================================================="
         "+\n\n");

  int line_no = 1;
  for (TAC *cur = tac_head; cur; cur = cur->next) {
    if (strcmp(cur->op, "label") == 0) {
      printf("%s:\n", cur->result);
    } else if (strcmp(cur->op, "goto") == 0) {
      printf("  %3d:  goto %s\n", line_no++, cur->result);
    } else if (strcmp(cur->op, "if") == 0) {
      printf("  %3d:  if %s goto %s\n", line_no++, cur->arg1, cur->result);
    } else if (strcmp(cur->op, "print") == 0) {
      printf("  %3d:  print %s\n", line_no++, cur->arg1);
    } else if (strcmp(cur->op, "=") == 0) {
      printf("  %3d:  %s = %s\n", line_no++, cur->result, cur->arg1);
    } else if (strcmp(cur->op, "uminus") == 0) {
      printf("  %3d:  %s = -%s\n", line_no++, cur->result, cur->arg1);
    } else {
      printf("  %3d:  %s = %s %s %s\n", line_no++, cur->result, cur->arg1,
             cur->op, cur->arg2);
    }
  }
  printf("\n+=================================================================="
         "==+\n");
  printf("| TAC Generation Complete. %3d instruction(s) emitted.               "
         "|\n",
         line_no - 1);
  printf("+===================================================================="
         "+\n");
}

void print_quadruples(void) {
  printf("\n+=================================================================="
         "==+\n");
  printf("| PHASE 6 (EXTRA): QUADRUPLES TABLE                                  "
         "|\n");
  printf("|  Each instruction = (op, arg1, arg2, result)                       "
         "|\n");
  printf("+===================================================================="
         "+\n");
  printf("| %-4s | %-10s | %-13s | %-13s | %-12s |\n", "idx", "op", "arg1",
         "arg2", "result");
  printf("+------+-----------+--------------+--------------+-------------+\n");

  for (int i = 0; i < quad_count; i++) {
    Quadruple *q = &quad_table[i];
    const char *disp_arg1 = (q->arg1[0] != '\0') ? q->arg1 : "-";
    const char *disp_arg2 = (q->arg2[0] != '\0') ? q->arg2 : "-";
    const char *disp_result = (q->result[0] != '\0') ? q->result : "-";

    if (strcmp(q->op, "label") == 0) {
      disp_arg1 = "-";
      disp_arg2 = "-";
    } else if (strcmp(q->op, "goto") == 0) {
      disp_arg1 = "-";
      disp_arg2 = "-";
    } else if (strcmp(q->op, "if") == 0 || strcmp(q->op, "iff") == 0) {
      disp_arg2 = "-";
    } else if (strcmp(q->op, "=") == 0 || strcmp(q->op, "uminus") == 0 ||
               strcmp(q->op, "!") == 0) {
      disp_arg2 = "-";
    } else if (strcmp(q->op, "print") == 0 || strcmp(q->op, "return") == 0 ||
               strcmp(q->op, "func_end") == 0) {
      disp_arg2 = "-";
      disp_result = "-";
    }

    printf("| %-4d | %-10s | %-13s | %-13s | %-12s |\n", i, q->op, disp_arg1,
           disp_arg2, disp_result);
  }
  printf("+------+-----------+--------------+--------------+-------------+\n");
  printf("| Total quadruples: %-3d                                             "
         "|\n",
         quad_count);
  printf("+===================================================================="
         "+\n");
}

/*
 * BUILD TRIPLES: Transliterates Quadruples into the Triples structure
 * requested. Automatically resolves temps to their triple execution index e.g.,
 * (0), (1).
 */
void build_triples(void) {
  for (int i = 0; i < quad_count; i++) {
    Quadruple *q = &quad_table[i];
    char a1[64];
    strcpy(a1, q->arg1);
    char a2[64];
    strcpy(a2, q->arg2);

    // Map Quadruple arguments to Triple index pointers like "(1)" if they are
    // temporaries
    int idx1 = get_triple_idx(a1);
    if (idx1 != -1)
      sprintf(a1, "(%d)", idx1);

    int idx2 = get_triple_idx(a2);
    if (idx2 != -1)
      sprintf(a2, "(%d)", idx2);

    if (strcmp(q->op, "label") == 0) {
      Triple *t = &triple_table[triple_count++];
      t->index = -1; // -1 denotes a standalone label line
      strcpy(t->op, "label");
      strcpy(t->arg1, q->result);
      strcpy(t->arg2, "-");
    } else if (strcmp(q->op, "goto") == 0) {
      Triple *t = &triple_table[triple_count++];
      t->index = next_triple_idx++;
      strcpy(t->op, "goto");
      strcpy(t->arg1, q->result);
      strcpy(t->arg2, "-");
    } else if (strcmp(q->op, "if") == 0 || strcmp(q->op, "iff") == 0) {
      // Per the image: an 'if' becomes two triples -> `==, cond, 1` then `if,
      // (0), L`
      Triple *t1 = &triple_table[triple_count++];
      t1->index = next_triple_idx++;
      strcpy(t1->op, "==");
      strcpy(t1->arg1, a1);
      strcpy(t1->arg2, "1"); // Testing truthiness against 1

      Triple *t2 = &triple_table[triple_count++];
      t2->index = next_triple_idx++;
      strcpy(t2->op, q->op);
      sprintf(t2->arg1, "(%d)", t1->index);
      strcpy(t2->arg2, q->result);
    } else if (strcmp(q->op, "=") == 0) {
      // Assignment maps result as arg1 and right-side (or mapped index) as arg2
      Triple *t = &triple_table[triple_count++];
      t->index = next_triple_idx++;
      strcpy(t->op, "=");
      strcpy(t->arg1, q->result);
      strcpy(t->arg2, a1);
    } else if (strcmp(q->op, "print") == 0 || strcmp(q->op, "return") == 0) {
      Triple *t = &triple_table[triple_count++];
      t->index = next_triple_idx++;
      strcpy(t->op, q->op);
      strcpy(t->arg1, a1);
      strcpy(t->arg2, "-");
    } else if (strcmp(q->op, "uminus") == 0 || strcmp(q->op, "!") == 0) {
      Triple *t = &triple_table[triple_count++];
      t->index = next_triple_idx++;
      strcpy(t->op, q->op);
      strcpy(t->arg1, a1);
      strcpy(t->arg2, "-");
      if (q->result[0] == 't')
        add_temp_map(q->result, t->index);
    } else if (strcmp(q->op, "func_end") == 0) {
      Triple *t = &triple_table[triple_count++];
      t->index = -1;
      strcpy(t->op, q->op);
      strcpy(t->arg1, q->arg1);
      strcpy(t->arg2, "-");
    } else {
      // Standard binary operation resolving to a Triple structure
      Triple *t = &triple_table[triple_count++];
      t->index = next_triple_idx++;
      strcpy(t->op, q->op);
      strcpy(t->arg1, a1);
      strcpy(t->arg2, a2);

      // Map the resolved destination temporary 'tX' to point at this Triple's
      // index
      if (q->result[0] == 't')
        add_temp_map(q->result, t->index);
    }
  }
}

void print_triples(void) {
  printf("\n+=================================================================="
         "==+\n");
  printf("| PHASE 6 (EXTRA): TRIPLES TABLE                                     "
         "|\n");
  printf("|  Resolves temporary variables implicitly to sequence indices e.g. "
         "(0)|\n");
  printf("+===================================================================="
         "+\n");
  printf("| %-4s | %-10s | %-13s | %-13s |\n", "idx", "op", "arg1", "arg2");
  printf("+------+------------+---------------+---------------+\n");

  for (int i = 0; i < triple_count; i++) {
    Triple *t = &triple_table[i];

    // Handle labels visually
    if (t->index == -1) {
      if (strcmp(t->op, "label") == 0) {
        printf("| %-4s | %-10s | %-13s | %-13s |\n", "", t->arg1, "-", "-");
      }
      continue;
    }

    const char *disp_arg1 = (t->arg1[0] != '\0') ? t->arg1 : "-";
    const char *disp_arg2 = (t->arg2[0] != '\0') ? t->arg2 : "-";

    printf("| %-4d | %-10s | %-13s | %-13s |\n", t->index, t->op, disp_arg1,
           disp_arg2);
  }
  printf("+------+------------+---------------+---------------+\n");
  printf("| Total triples generated: %-3d                                      "
         "|\n",
         next_triple_idx);
  printf("+===================================================================="
         "+\n");
}

#define MAX_TEMP_VALUES (MAX_QUADS * 2)

static Quadruple optimized_quad_table[MAX_QUADS];
static Quadruple optimized_quad_buffer[MAX_QUADS];
static int optimized_quad_count = 0;
static int optimization_fold_count = 0;
static int optimization_dce_count = 0;
static int temp_definition_count[MAX_TEMP_VALUES];
static int temp_const_known[MAX_TEMP_VALUES];
static char temp_const_value[MAX_TEMP_VALUES][64];

static int is_temp_name(const char *name) {
  return name && name[0] == 't' && isdigit((unsigned char)name[1]);
}

static int temp_slot(const char *name) {
  int slot;
  if (!is_temp_name(name))
    return -1;
  slot = atoi(name + 1);
  if (slot < 0 || slot >= MAX_TEMP_VALUES)
    return -1;
  return slot;
}

static int is_integer_literal(const char *text) {
  char *endptr;
  if (!text || !text[0])
    return 0;
  strtoll(text, &endptr, 10);
  return *endptr == '\0';
}

static int is_numeric_literal(const char *text) {
  char *endptr;
  if (!text || !text[0])
    return 0;
  strtod(text, &endptr);
  return *endptr == '\0';
}

static int is_bool_literal(const char *text) {
  return text && (strcmp(text, "true") == 0 || strcmp(text, "false") == 0);
}

static int is_constant_literal(const char *text) {
  return is_bool_literal(text) || is_numeric_literal(text);
}

static int can_track_temp_constant(const char *name) {
  int slot = temp_slot(name);
  return slot != -1 && temp_definition_count[slot] == 1;
}

static void format_numeric_result(double value, int as_int, char *out,
                                  size_t out_size) {
  if (as_int)
    snprintf(out, out_size, "%lld", (long long)value);
  else
    snprintf(out, out_size, "%.6g", value);
}

static int literal_truth_value(const char *text, int *is_const) {
  if (is_bool_literal(text)) {
    *is_const = 1;
    return strcmp(text, "true") == 0;
  }
  if (is_numeric_literal(text)) {
    *is_const = 1;
    return strtod(text, NULL) != 0.0;
  }
  *is_const = 0;
  return 0;
}

static void forget_temp_constant(const char *name) {
  int slot = temp_slot(name);
  if (slot != -1)
    temp_const_known[slot] = 0;
}

static void remember_temp_constant(const char *name, const char *value) {
  int slot = temp_slot(name);
  if (slot != -1 && can_track_temp_constant(name)) {
    temp_const_known[slot] = 1;
    strncpy(temp_const_value[slot], value, 63);
    temp_const_value[slot][63] = '\0';
  }
}

static const char *lookup_temp_constant(const char *name) {
  int slot = temp_slot(name);
  if (slot == -1 || !temp_const_known[slot])
    return NULL;
  return temp_const_value[slot];
}

static void substitute_temp_constant(char *operand) {
  const char *replacement = lookup_temp_constant(operand);
  if (replacement) {
    strncpy(operand, replacement, 63);
    operand[63] = '\0';
  }
}

static int is_relational_op(const char *op) {
  return strcmp(op, "<") == 0 || strcmp(op, ">") == 0 ||
         strcmp(op, "<=") == 0 || strcmp(op, ">=") == 0 ||
         strcmp(op, "==") == 0 || strcmp(op, "!=") == 0;
}

static int is_removable_temp_definition(const Quadruple *q) {
  return is_temp_name(q->result) && strcmp(q->op, "label") != 0 &&
         strcmp(q->op, "goto") != 0 && strcmp(q->op, "if") != 0 &&
         strcmp(q->op, "iff") != 0 && strcmp(q->op, "print") != 0 &&
         strcmp(q->op, "return") != 0 && strcmp(q->op, "func_end") != 0;
}

static void copy_quad(Quadruple *dst, const Quadruple *src) { *dst = *src; }

static void append_optimized_quad(const Quadruple *q) {
  if (optimized_quad_count < MAX_QUADS)
    optimized_quad_table[optimized_quad_count++] = *q;
}

static int try_fold_unary(const char *op, const char *arg1, char *out) {
  if (strcmp(op, "uminus") == 0 && is_numeric_literal(arg1)) {
    if (is_integer_literal(arg1))
      snprintf(out, 64, "%lld", -strtoll(arg1, NULL, 10));
    else
      format_numeric_result(-strtod(arg1, NULL), 0, out, 64);
    return 1;
  }

  if (strcmp(op, "!") == 0) {
    int is_const;
    int truth = literal_truth_value(arg1, &is_const);
    if (is_const) {
      strcpy(out, truth ? "false" : "true");
      return 1;
    }
  }

  return 0;
}

static int try_fold_binary(const char *op, const char *arg1, const char *arg2,
                           char *out) {
  if (strcmp(op, "&&") == 0 || strcmp(op, "||") == 0) {
    int left_is_const, right_is_const;
    int left = literal_truth_value(arg1, &left_is_const);
    int right = literal_truth_value(arg2, &right_is_const);
    if (left_is_const && right_is_const) {
      if (strcmp(op, "&&") == 0)
        strcpy(out, (left && right) ? "true" : "false");
      else
        strcpy(out, (left || right) ? "true" : "false");
      return 1;
    }
  }

  if (strcmp(op, "==") == 0 || strcmp(op, "!=") == 0) {
    if (is_bool_literal(arg1) && is_bool_literal(arg2)) {
      int equal = strcmp(arg1, arg2) == 0;
      strcpy(out, ((strcmp(op, "==") == 0) == equal) ? "true" : "false");
      return 1;
    }
  }

  if (!is_numeric_literal(arg1) || !is_numeric_literal(arg2))
    return 0;

  if (strcmp(op, "%") == 0) {
    long long left, right;
    if (!is_integer_literal(arg1) || !is_integer_literal(arg2))
      return 0;
    left = strtoll(arg1, NULL, 10);
    right = strtoll(arg2, NULL, 10);
    if (right == 0)
      return 0;
    snprintf(out, 64, "%lld", left % right);
    return 1;
  }

  if (strcmp(op, "+") == 0 || strcmp(op, "-") == 0 || strcmp(op, "*") == 0 ||
      strcmp(op, "/") == 0) {
    int both_ints = is_integer_literal(arg1) && is_integer_literal(arg2);
    if (both_ints) {
      long long left = strtoll(arg1, NULL, 10);
      long long right = strtoll(arg2, NULL, 10);
      if (strcmp(op, "+") == 0)
        snprintf(out, 64, "%lld", left + right);
      else if (strcmp(op, "-") == 0)
        snprintf(out, 64, "%lld", left - right);
      else if (strcmp(op, "*") == 0)
        snprintf(out, 64, "%lld", left * right);
      else {
        if (right == 0)
          return 0;
        snprintf(out, 64, "%lld", left / right);
      }
      return 1;
    } else {
      double left = strtod(arg1, NULL);
      double right = strtod(arg2, NULL);
      if (strcmp(op, "+") == 0)
        format_numeric_result(left + right, 0, out, 64);
      else if (strcmp(op, "-") == 0)
        format_numeric_result(left - right, 0, out, 64);
      else if (strcmp(op, "*") == 0)
        format_numeric_result(left * right, 0, out, 64);
      else {
        if (right == 0.0)
          return 0;
        format_numeric_result(left / right, 0, out, 64);
      }
      return 1;
    }
  }

  if (is_relational_op(op)) {
    double left = strtod(arg1, NULL);
    double right = strtod(arg2, NULL);
    int result = 0;

    if (strcmp(op, "<") == 0)
      result = left < right;
    else if (strcmp(op, ">") == 0)
      result = left > right;
    else if (strcmp(op, "<=") == 0)
      result = left <= right;
    else if (strcmp(op, ">=") == 0)
      result = left >= right;
    else if (strcmp(op, "==") == 0)
      result = left == right;
    else if (strcmp(op, "!=") == 0)
      result = left != right;

    strcpy(out, result ? "true" : "false");
    return 1;
  }

  return 0;
}

static void eliminate_dead_temp_code(void) {
  int removed;

  do {
    int used_temp[MAX_TEMP_VALUES] = {0};
    int new_count = 0;

    for (int i = 0; i < optimized_quad_count; i++) {
      int slot1 = temp_slot(optimized_quad_table[i].arg1);
      int slot2 = temp_slot(optimized_quad_table[i].arg2);
      if (slot1 != -1)
        used_temp[slot1] = 1;
      if (slot2 != -1)
        used_temp[slot2] = 1;
    }

    removed = 0;
    for (int i = 0; i < optimized_quad_count; i++) {
      Quadruple *q = &optimized_quad_table[i];
      int result_slot = temp_slot(q->result);

      if (is_removable_temp_definition(q) && result_slot != -1 &&
          !used_temp[result_slot]) {
        removed++;
        continue;
      }

      optimized_quad_buffer[new_count++] = *q;
    }

    if (removed > 0) {
      memcpy(optimized_quad_table, optimized_quad_buffer,
             sizeof(Quadruple) * new_count);
      optimized_quad_count = new_count;
      optimization_dce_count += removed;
    }
  } while (removed > 0);
}

void optimize_quadruples(void) {
  optimized_quad_count = 0;
  optimization_fold_count = 0;
  optimization_dce_count = 0;
  memset(temp_definition_count, 0, sizeof(temp_definition_count));
  memset(temp_const_known, 0, sizeof(temp_const_known));

  for (int i = 0; i < quad_count; i++) {
    int slot = temp_slot(quad_table[i].result);
    if (slot != -1)
      temp_definition_count[slot]++;
  }

  for (int i = 0; i < quad_count; i++) {
    Quadruple work;
    char folded_value[64];
    int emit_current = 1;

    copy_quad(&work, &quad_table[i]);
    substitute_temp_constant(work.arg1);
    substitute_temp_constant(work.arg2);

    if (strcmp(work.op, "=") == 0) {
      if (is_temp_name(work.result)) {
        if (is_constant_literal(work.arg1))
          remember_temp_constant(work.result, work.arg1);
        else
          forget_temp_constant(work.result);
      }
    } else if (strcmp(work.op, "uminus") == 0 || strcmp(work.op, "!") == 0) {
      if (try_fold_unary(work.op, work.arg1, folded_value)) {
        strcpy(work.op, "=");
        strcpy(work.arg1, folded_value);
        work.arg2[0] = '\0';
        if (is_temp_name(work.result))
          remember_temp_constant(work.result, work.arg1);
        optimization_fold_count++;
      } else if (is_temp_name(work.result)) {
        forget_temp_constant(work.result);
      }
    } else if (strcmp(work.op, "if") == 0 || strcmp(work.op, "iff") == 0) {
      int is_const;
      int truth = literal_truth_value(work.arg1, &is_const);
      if (is_const) {
        optimization_fold_count++;
        if ((strcmp(work.op, "if") == 0 && truth) ||
            (strcmp(work.op, "iff") == 0 && !truth)) {
          strcpy(work.op, "goto");
          work.arg1[0] = '\0';
          work.arg2[0] = '\0';
        } else {
          emit_current = 0;
        }
      }
    } else if (strcmp(work.op, "print") == 0 || strcmp(work.op, "return") == 0 ||
               strcmp(work.op, "goto") == 0 || strcmp(work.op, "label") == 0 ||
               strcmp(work.op, "func_end") == 0) {
      /* No-op: only temp substitution was needed above. */
    } else {
      if (try_fold_binary(work.op, work.arg1, work.arg2, folded_value)) {
        strcpy(work.op, "=");
        strcpy(work.arg1, folded_value);
        work.arg2[0] = '\0';
        if (is_temp_name(work.result))
          remember_temp_constant(work.result, work.arg1);
        optimization_fold_count++;
      } else if (is_temp_name(work.result)) {
        forget_temp_constant(work.result);
      }
    }

    if (emit_current)
      append_optimized_quad(&work);

    if (strcmp(work.op, "label") == 0 || strcmp(work.op, "goto") == 0 ||
        strcmp(work.op, "if") == 0 || strcmp(work.op, "iff") == 0 ||
        strcmp(work.op, "func_end") == 0) {
      memset(temp_const_known, 0, sizeof(temp_const_known));
    }
  }

  eliminate_dead_temp_code();
}

void print_optimized_quadruples(void) {
  printf("\n+=================================================================="
         "==+\n");
  printf("| PHASE 7: OPTIMIZED INTERMEDIATE CODE                               "
         "|\n");
  printf("|  Technique: Constant folding with dead temp elimination            "
         "|\n");
  printf("+===================================================================="
         "+\n");
  printf("| %-4s | %-10s | %-13s | %-13s | %-12s |\n", "idx", "op", "arg1",
         "arg2", "result");
  printf("+------+-----------+--------------+--------------+-------------+\n");

  for (int i = 0; i < optimized_quad_count; i++) {
    Quadruple *q = &optimized_quad_table[i];
    const char *disp_arg1 = q->arg1[0] ? q->arg1 : "-";
    const char *disp_arg2 = q->arg2[0] ? q->arg2 : "-";
    const char *disp_result = q->result[0] ? q->result : "-";

    if (strcmp(q->op, "label") == 0 || strcmp(q->op, "goto") == 0) {
      disp_arg1 = "-";
      disp_arg2 = "-";
    } else if (strcmp(q->op, "if") == 0 || strcmp(q->op, "iff") == 0 ||
               strcmp(q->op, "=") == 0 || strcmp(q->op, "uminus") == 0 ||
               strcmp(q->op, "!") == 0) {
      disp_arg2 = "-";
    } else if (strcmp(q->op, "print") == 0 || strcmp(q->op, "return") == 0 ||
               strcmp(q->op, "func_end") == 0) {
      disp_arg2 = "-";
      disp_result = "-";
    }

    printf("| %-4d | %-10s | %-13s | %-13s | %-12s |\n", i, q->op, disp_arg1,
           disp_arg2, disp_result);
  }

  printf("+------+-----------+--------------+--------------+-------------+\n");
  printf("| Folds applied: %-3d  Dead temp instructions removed: %-3d          "
         "|\n",
         optimization_fold_count, optimization_dce_count);
  printf("| Optimized quadruples: %-3d                                         "
         "|\n",
         optimized_quad_count);
  printf("+===================================================================="
         "+\n");
}

void print_target_code(void) {
  printf("\n+=================================================================="
         "==+\n");
  printf("| PHASE 8: TARGET CODE GENERATION (Pseudo Assembly)                  "
         "|\n");
  printf("|  Generated from the optimized quadruples                           "
         "|\n");
  printf("+===================================================================="
         "+\n\n");

  for (int i = 0; i < optimized_quad_count; i++) {
    Quadruple *q = &optimized_quad_table[i];

    if (strcmp(q->op, "label") == 0) {
      printf("%s:\n", q->result);
    } else if (strcmp(q->op, "goto") == 0) {
      printf("  JMP %s\n", q->result);
    } else if (strcmp(q->op, "if") == 0) {
      printf("  CMP %s, 0\n", q->arg1);
      printf("  JNE %s\n", q->result);
    } else if (strcmp(q->op, "iff") == 0) {
      printf("  CMP %s, 0\n", q->arg1);
      printf("  JE %s\n", q->result);
    } else if (strcmp(q->op, "=") == 0) {
      printf("  MOV %s, %s\n", q->result, q->arg1);
    } else if (strcmp(q->op, "uminus") == 0) {
      printf("  MOV R1, %s\n", q->arg1);
      printf("  NEG R1\n");
      printf("  MOV %s, R1\n", q->result);
    } else if (strcmp(q->op, "!") == 0) {
      printf("  CMP %s, 0\n", q->arg1);
      printf("  SETE R1\n");
      printf("  MOV %s, R1\n", q->result);
    } else if (strcmp(q->op, "+") == 0 || strcmp(q->op, "-") == 0 ||
               strcmp(q->op, "*") == 0 || strcmp(q->op, "/") == 0 ||
               strcmp(q->op, "%") == 0) {
      const char *mnemonic = strcmp(q->op, "+") == 0
                                 ? "ADD"
                                 : strcmp(q->op, "-") == 0
                                       ? "SUB"
                                       : strcmp(q->op, "*") == 0
                                             ? "MUL"
                                             : strcmp(q->op, "/") == 0 ? "DIV"
                                                                        : "MOD";
      printf("  MOV R1, %s\n", q->arg1);
      printf("  %s R1, %s\n", mnemonic, q->arg2);
      printf("  MOV %s, R1\n", q->result);
    } else if (is_relational_op(q->op)) {
      const char *set_mnemonic =
          strcmp(q->op, "<") == 0
              ? "SETLT"
              : strcmp(q->op, ">") == 0
                    ? "SETGT"
                    : strcmp(q->op, "<=") == 0
                          ? "SETLE"
                          : strcmp(q->op, ">=") == 0
                                ? "SETGE"
                                : strcmp(q->op, "==") == 0 ? "SETE" : "SETNE";
      printf("  CMP %s, %s\n", q->arg1, q->arg2);
      printf("  %s R1\n", set_mnemonic);
      printf("  MOV %s, R1\n", q->result);
    } else if (strcmp(q->op, "print") == 0) {
      printf("  PRINT %s\n", q->arg1);
    } else if (strcmp(q->op, "return") == 0) {
      if (q->arg1[0])
        printf("  RETURN %s\n", q->arg1);
      else
        printf("  RETURN\n");
    } else if (strcmp(q->op, "func_end") == 0) {
      printf("  END_FUNC %s\n", q->arg1);
    }
  }

  printf("\n+=================================================================="
         "==+\n");
}

char *generate_tac(ASTNode *n);
void generate_bool_tac(ASTNode *n, const char *true_label,
                       const char *false_label);

void generate_bool_tac(ASTNode *n, const char *true_label,
                       const char *false_label) {
  if (!n)
    return;
  if (n->kind == NODE_BINARY_OP && n->op == TOK_AND) {
    char *mid = new_label();
    generate_bool_tac(n->children[0], mid, false_label);
    emit("label", "", "", mid);
    generate_bool_tac(n->children[1], true_label, false_label);
    free(mid);
  } else if (n->kind == NODE_BINARY_OP && n->op == TOK_OR) {
    char *mid = new_label();
    generate_bool_tac(n->children[0], true_label, mid);
    emit("label", "", "", mid);
    generate_bool_tac(n->children[1], true_label, false_label);
    free(mid);
  } else if (n->kind == NODE_UNARY_OP && n->op == TOK_NOT) {
    generate_bool_tac(n->children[0], false_label, true_label);
  } else if (n->kind == NODE_BINARY_OP &&
             (n->op == TOK_LT || n->op == TOK_GT || n->op == TOK_LE ||
              n->op == TOK_GE || n->op == TOK_EQ || n->op == TOK_NEQ)) {
    char *arg1 = generate_tac(n->children[0]);
    char *arg2 = generate_tac(n->children[1]);
    const char *op_str;
    switch (n->op) {
    case TOK_LT:
      op_str = "<";
      break;
    case TOK_GT:
      op_str = ">";
      break;
    case TOK_LE:
      op_str = "<=";
      break;
    case TOK_GE:
      op_str = ">=";
      break;
    case TOK_EQ:
      op_str = "==";
      break;
    case TOK_NEQ:
      op_str = "!=";
      break;
    default:
      op_str = "?";
      break;
    }
    char *t = new_temp();
    emit(op_str, arg1, arg2, t);
    emit("if", t, "", true_label);
    emit("goto", "", "", false_label);
    if (arg1)
      free(arg1);
    if (arg2)
      free(arg2);
    free(t);
  } else {
    char *val = generate_tac(n);
    if (val) {
      emit("if", val, "", true_label);
      emit("goto", "", "", false_label);
      free(val);
    }
  }
}

char *generate_tac(ASTNode *n) {
  if (!n)
    return NULL;
  switch (n->kind) {
  case NODE_LITERAL:
  case NODE_VAR_REF:
    return strdup(n->lexeme);
  case NODE_VAR_DECL:
    return NULL;
  case NODE_BINARY_OP: {
    if (n->op == TOK_AND || n->op == TOK_OR || n->op == TOK_LT ||
        n->op == TOK_GT || n->op == TOK_LE || n->op == TOK_GE ||
        n->op == TOK_EQ || n->op == TOK_NEQ) {
      char *tl = new_label();
      char *fl = new_label();
      char *el = new_label();
      char *res = new_temp();
      generate_bool_tac(n, tl, fl);
      emit("label", "", "", tl);
      emit("=", "true", "", res);
      emit("goto", "", "", el);
      emit("label", "", "", fl);
      emit("=", "false", "", res);
      emit("label", "", "", el);
      free(tl);
      free(fl);
      free(el);
      return res;
    }
    char *a1 = generate_tac(n->children[0]);
    char *a2 = generate_tac(n->children[1]);
    char *res = new_temp();
    const char *op_str;
    switch (n->op) {
    case TOK_PLUS:
      op_str = "+";
      break;
    case TOK_MINUS:
      op_str = "-";
      break;
    case TOK_MUL:
      op_str = "*";
      break;
    case TOK_DIV:
      op_str = "/";
      break;
    case TOK_MOD:
      op_str = "%";
      break;
    default:
      op_str = "?";
      break;
    }
    emit(op_str, a1, a2, res);
    if (a1)
      free(a1);
    if (a2)
      free(a2);
    return res;
  }
  case NODE_UNARY_OP: {
    if (n->op == TOK_NOT) {
      char *tl = new_label();
      char *fl = new_label();
      char *el = new_label();
      char *res = new_temp();
      generate_bool_tac(n, tl, fl);
      emit("label", "", "", tl);
      emit("=", "true", "", res);
      emit("goto", "", "", el);
      emit("label", "", "", fl);
      emit("=", "false", "", res);
      emit("label", "", "", el);
      free(tl);
      free(fl);
      free(el);
      return res;
    }
    char *a1 = generate_tac(n->children[0]);
    char *res = new_temp();
    emit("uminus", a1, "", res);
    if (a1)
      free(a1);
    return res;
  }
  case NODE_ASSIGN: {
    char *rhs = generate_tac(n->children[0]);
    if (rhs) {
      emit("=", rhs, "", n->lexeme);
      free(rhs);
    }
    return NULL;
  }
  case NODE_IF: {
    char *tl = new_label();
    char *fl = new_label();
    char *el = new_label();
    generate_bool_tac(n->children[0], tl, fl);
    emit("label", "", "", tl);
    generate_tac(n->children[1]);
    if (n->child_count == 3) {
      emit("goto", "", "", el);
      emit("label", "", "", fl);
      generate_tac(n->children[2]);
      emit("label", "", "", el);
    } else {
      emit("label", "", "", fl);
    }
    free(tl);
    free(fl);
    free(el);
    return NULL;
  }
  case NODE_WHILE: {
    char *sl = new_label();
    char *bl = new_label();
    char *xl = new_label();
    emit("label", "", "", sl);
    generate_bool_tac(n->children[0], bl, xl);
    emit("label", "", "", bl);
    generate_tac(n->children[1]);
    emit("goto", "", "", sl);
    emit("label", "", "", xl);
    free(sl);
    free(bl);
    free(xl);
    return NULL;
  }
  case NODE_PRINT: {
    char *val = generate_tac(n->children[0]);
    if (val) {
      emit("print", val, "", "");
      free(val);
    }
    return NULL;
  }
  case NODE_RETURN: {
    if (n->child_count > 0) {
      char *val = generate_tac(n->children[0]);
      if (val) {
        emit("return", val, "", "");
        free(val);
      }
    } else {
      emit("return", "", "", "");
    }
    return NULL;
  }
  case NODE_BLOCK:
  case NODE_STMT_LIST:
    for (int i = 0; i < n->child_count; i++)
      generate_tac(n->children[i]);
    return NULL;
  case NODE_FUNC_DEF: {
    char label[80];
    sprintf(label, "func_%s", n->lexeme);
    emit("label", "", "", label);
    generate_tac(n->children[1]);
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
  printf("  Phases 1-8 including optimization     \n");
  printf("========================================\n\n");

  if (argc > 1) {
    FILE *fp = fopen(argv[1], "r");
    if (!fp) {
      printf("Could not open file: %s\n", argv[1]);
      return 1;
    }
    size_t len = fread(source, 1, sizeof(source) - 1, fp);
    source[len] = '\0';
    fclose(fp);
  } else {
    printf("Enter source code (end with Ctrl+D / Ctrl+Z):\n");
    int i = 0, ch;
    while ((ch = getchar()) != EOF && i < (int)sizeof(source) - 1)
      source[i++] = (char)ch;
    source[i] = '\0';
  }
  if (strlen(source) == 0) {
    printf("No input.\n");
    return 0;
  }

  printf("\n----------------------------------------\n");
  printf("INPUT PROGRAM:\n");
  printf("----------------------------------------\n%s\n", source);

  if (run_lexical_analysis(source) != 0) {
    printf("\n [FAIL] Compilation halted due to lexical error(s).\n");
    return 1;
  }

  printf("+===================================================================="
         "+\n");
  printf("| PHASE 2: SYNTAX ANALYSIS + AST CONSTRUCTION                        "
         "|\n");
  printf("| PHASE 3: SYNTAX ERROR DETECTION                                    "
         "|\n");
  printf("+===================================================================="
         "+\n\n");

  input_ptr = source;
  current_line = 1;
  advance();

  push_scope();
  ASTNode *root = parse_StmtList();

  if (has_error) {
    printf("\n [FAIL] Compilation halted due to syntax error(s).\n");
    pop_scope();
    return 1;
  }

  if (current_token.type != TOK_EOF) {
    syntax_error(current_token.line, "Extraneous tokens after program end");
    printf(" *** Syntax Error: Extraneous tokens after program end: '%s'\n",
           current_token.lexeme);
    printf("\n [FAIL] Compilation halted due to syntax error(s).\n");
    pop_scope();
    return 1;
  }
  printf("\n [OK] Parsing completed — syntax is valid.\n\n");

  printf("+===================================================================="
         "+\n");
  printf("| PHASE 4: SYMBOL TABLE CONSTRUCTION                                 "
         "|\n");
  printf("| PHASE 5: SEMANTIC ANALYSIS (type checking)                         "
         "|\n");
  printf("+===================================================================="
         "+\n\n");

  analyze(root);

  /* Check for semantic errors BEFORE printing symbol table */
  if (has_error || sem_error_count > 0) {
    printf("\n [FAIL] %d semantic error(s) detected. Compilation halted.\n", 
           sem_error_count);
    printf(" *** Symbol table generation aborted due to semantic error(s).\n");
    pop_scope();
    return 1;
  } else {
    printf("\n [OK] Semantic analysis passed — no errors.\n");
  }

  printf("\n --- Final Symbol Table (global scope) ---\n");
  print_all_scopes();

  /* Phase 6: TAC Generation - only if no errors so far */
  if (!has_error) {
    generate_tac(root);
    print_tac();
    print_quadruples();
  } else {
    printf("\n [SKIP] TAC generation aborted due to earlier error(s).\n");
    printf(" *** Intermediate code generation skipped.\n");
  }

  /* Phase 7: Triples - only if no errors so far */
  if (!has_error) {
    build_triples();
    print_triples();
  } else {
    printf("\n [SKIP] Triples generation aborted due to earlier error(s).\n");
    printf(" *** Triples table generation skipped.\n");
  }

  /* Phase 8: Optimization - only if no errors so far */
  if (!has_error) {
    optimize_quadruples();
    print_optimized_quadruples();
    print_target_code();
  } else {
    printf("\n [SKIP] Code optimization aborted due to earlier error(s).\n");
    printf(" *** Code optimization skipped.\n");
  }

  /* Only allow interactive lookup if no errors occurred */
  if (!has_error) {
    printf("\n+===================================================================="
           "==+\n");
    printf("| INTERACTIVE SYMBOL TABLE LOOKUP                                    "
           "|\n");
    printf("| Enter a variable name to look up. Type 'q' to exit.               "
           "|\n");
    printf("+===================================================================="
           "+\n");
    char search_var[64];
    while (1) {
      printf("\n  Search variable >>> ");
      if (scanf("%63s", search_var) != 1)
        break;
      if (strcmp(search_var, "q") == 0 || strcmp(search_var, "quit") == 0)
        break;
      searchSym(top, search_var);
    }
  } else {
    printf("\n [SKIP] Interactive symbol table lookup skipped due to error(s).\n");
  }

  pop_scope();
  return 0;
}
