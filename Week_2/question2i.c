#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Token types
typedef enum {
  TOK_IF,
  TOK_WHILE,
  TOK_LPAREN,
  TOK_RPAREN,
  TOK_LBRACE,
  TOK_RBRACE,
  TOK_ID,
  TOK_NUM,
  TOK_GT,
  TOK_ASSIGN,
  TOK_PLUS,
  TOK_SEMI,
  TOK_EOF,
  TOK_ERROR
} TokenType;

typedef struct {
  TokenType type;
  char lexeme[20];
} Token;

Token current_token;
char *input_ptr; // Pointer to traverse the user input string

// --- Basic Lexical Analyzer ---
void advance() {
  while (isspace(*input_ptr))
    input_ptr++; // Skip whitespace

  if (*input_ptr == '\0') {
    current_token.type = TOK_EOF;
    strcpy(current_token.lexeme, "EOF");
    return;
  }

  if (isalpha(*input_ptr)) {
    int i = 0;
    while (isalnum(*input_ptr)) {
      current_token.lexeme[i++] = *input_ptr++;
    }
    current_token.lexeme[i] = '\0';

    if (strcmp(current_token.lexeme, "if") == 0)
      current_token.type = TOK_IF;
    else if (strcmp(current_token.lexeme, "while") == 0)
      current_token.type = TOK_WHILE;
    else
      current_token.type = TOK_ID;
    return;
  }

  if (isdigit(*input_ptr)) {
    int i = 0;
    while (isdigit(*input_ptr)) {
      current_token.lexeme[i++] = *input_ptr++;
    }
    current_token.lexeme[i] = '\0';
    current_token.type = TOK_NUM;
    return;
  }

  // Single character tokens
  current_token.lexeme[0] = *input_ptr;
  current_token.lexeme[1] = '\0';

  switch (*input_ptr++) {
  case '(':
    current_token.type = TOK_LPAREN;
    break;
  case ')':
    current_token.type = TOK_RPAREN;
    break;
  case '{':
    current_token.type = TOK_LBRACE;
    break;
  case '}':
    current_token.type = TOK_RBRACE;
    break;
  case '>':
    current_token.type = TOK_GT;
    break;
  case '=':
    current_token.type = TOK_ASSIGN;
    break;
  case '+':
    current_token.type = TOK_PLUS;
    break;
  case ';':
    current_token.type = TOK_SEMI;
    break;
  default:
    current_token.type = TOK_ERROR;
    break;
  }
}

// --- Syntax Analyzer (Parser) ---
void match(TokenType expected) {
  if (current_token.type == expected) {
    advance();
  } else {
    printf("Syntax Error: Unexpected token '%s'\n", current_token.lexeme);
    exit(1);
  }
}

// (Forward declarations)
void parse_Expr();
void parse_StmtList();
void parse_Stmt();

void parse_Factor() {
  if (current_token.type == TOK_ID || current_token.type == TOK_NUM) {
    printf("Parsed Factor: %s\n", current_token.lexeme);
    advance();
  } else {
    printf("Syntax Error: Expected ID or NUM, got '%s'\n",
           current_token.lexeme);
    exit(1);
  }
}

void parse_Term() { parse_Factor(); }

void parse_Expr() {
  parse_Term();
  while (current_token.type == TOK_PLUS) {
    printf("Parsed Operator: %s\n", current_token.lexeme);
    match(TOK_PLUS);
    parse_Term();
  }
}

void parse_BoolExpr() {
  parse_Expr();
  if (current_token.type == TOK_GT) {
    printf("Parsed Relational Operator: %s\n", current_token.lexeme);
    match(TOK_GT);
    parse_Expr();
  }
}

void parse_Assign() {
  printf("Parsed Assignment ID: %s\n", current_token.lexeme);
  match(TOK_ID);
  match(TOK_ASSIGN);
  parse_Expr();
  match(TOK_SEMI);
}

void parse_Block() {
  match(TOK_LBRACE);
  printf("Entering Block...\n");
  parse_StmtList();
  match(TOK_RBRACE);
  printf("Exiting Block.\n");
}

void parse_IfStmt() {
  match(TOK_IF);
  match(TOK_LPAREN);
  parse_BoolExpr();
  match(TOK_RPAREN);
  parse_Block();
}

void parse_Stmt() {
  if (current_token.type == TOK_IF) {
    parse_IfStmt();
  } else if (current_token.type == TOK_ID) {
    parse_Assign();
  } else if (current_token.type == TOK_LBRACE) {
    parse_Block();
  } else {
    printf("Syntax Error: Unrecognized statement starting with '%s'\n",
           current_token.lexeme);
    exit(1);
  }
}

void parse_StmtList() {
  while (current_token.type != TOK_RBRACE && current_token.type != TOK_EOF) {
    parse_Stmt();
  }
}

// --- Main Execution ---
int main() {
  char user_input[256];

  printf("Enter a code snippet to parse (e.g., if ( x > 0 ) { y = x + 1 ; } "
         "):\n> ");
  if (fgets(user_input, sizeof(user_input), stdin) == NULL) {
    return 1;
  }

  // Initialize the pointer to the start of user input
  input_ptr = user_input;

  printf("\nStarting Parser...\n");
  advance(); // Load the first token from user input

  if (current_token.type != TOK_EOF) {
    parse_Stmt(); // Start parsing rules
  }

  if (current_token.type == TOK_EOF) {
    printf("Parsing Completed Successfully! The syntax is valid.\n");
  } else {
    printf("Syntax Error: Extraneous tokens at end of input.\n");
  }

  return 0;
}