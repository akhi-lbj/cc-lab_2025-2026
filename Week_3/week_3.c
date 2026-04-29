#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Token types
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
  char lexeme[20];
  int line;
} Token;

Token current_token;
char *input_ptr;
int current_line = 1;

const char* token_type_to_string(TokenType type) {
    switch (type) {
        case TOK_INT: return "int";
        case TOK_FLOAT: return "float";
        case TOK_IF: return "if";
        case TOK_ELSE: return "else";
        case TOK_WHILE: return "while";
        case TOK_PRINT: return "print";
        case TOK_LPAREN: return "'('";
        case TOK_RPAREN: return "')'";
        case TOK_LBRACE: return "'{'";
        case TOK_RBRACE: return "'}'";
        case TOK_ID: return "Identifier";
        case TOK_NUM: return "Number";
        case TOK_ASSIGN: return "'='";
        case TOK_PLUS: return "'+'";
        case TOK_MINUS: return "'-'";
        case TOK_MUL: return "'*'";
        case TOK_DIV: return "'/'";
        case TOK_MOD: return "'%'";
        case TOK_LT: return "'<'";
        case TOK_GT: return "'>'";
        case TOK_LE: return "'<='";
        case TOK_GE: return "'>='";
        case TOK_EQ: return "'=='";
        case TOK_NEQ: return "'!='";
        case TOK_AND: return "'&&'";
        case TOK_OR: return "'||'";
        case TOK_NOT: return "'!'";
        case TOK_SEMI: return "';'";
        case TOK_EOF: return "EOF";
        default: return "Unknown";
    }
}

// --- Abstract Syntax Tree Node ---
typedef struct Node {
    char name[50];
    struct Node* children[20];
    int num_children;
    int is_terminal;
    int width;
    int center_x;
} Node;

Node* create_node(const char* name, int is_terminal) {
    Node* n = (Node*)malloc(sizeof(Node));
    strcpy(n->name, name);
    n->num_children = 0;
    n->is_terminal = is_terminal;
    return n;
}

void add_child(Node* parent, Node* child) {
    if (child != NULL && parent->num_children < 20) {
        parent->children[parent->num_children++] = child;
    }
}

// --- Basic Lexical Analyzer ---
void advance() {
  while (isspace(*input_ptr)) {
      if (*input_ptr == '\n') {
          current_line++;
      }
      input_ptr++;
  }

  current_token.line = current_line;

  if (*input_ptr == '\0') {
    current_token.type = TOK_EOF;
    strcpy(current_token.lexeme, "EOF");
    return;
  }

  if (isalpha(*input_ptr)) {
    int i = 0;
    while (isalnum(*input_ptr) || *input_ptr == '_') {
      if (i < 19) current_token.lexeme[i++] = *input_ptr;
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
      if (i < 19) current_token.lexeme[i++] = *input_ptr;
      input_ptr++;
    }
    if (*input_ptr == '.') {
      if (i < 19) current_token.lexeme[i++] = *input_ptr;
      input_ptr++;
      while (isdigit(*input_ptr)) {
        if (i < 19) current_token.lexeme[i++] = *input_ptr;
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

// --- Syntax Analyzer (Parser) ---
Node* match(TokenType expected) {
  if (current_token.type == expected) {
    Node* n = create_node(current_token.lexeme, 1);
    if (expected == TOK_ID) {
      char buf[50];
      sprintf(buf, "ID(%s)", current_token.lexeme);
      strcpy(n->name, buf);
    } else if (expected == TOK_NUM) {
      char buf[50];
      sprintf(buf, "NUM(%s)", current_token.lexeme);
      strcpy(n->name, buf);
    }
    advance();
    return n;
  } else {
    printf("Syntax Error on line %d: Expected %s, but encountered '%s'\n", 
           current_token.line, token_type_to_string(expected), current_token.lexeme);
    exit(1);
  }
}

Node* parse_BoolExpr();
Node* parse_StmtList();
Node* parse_Stmt();
Node* parse_Block();

Node* parse_Factor() {
  Node* n = create_node("Factor", 0);
  if (current_token.type == TOK_ID || current_token.type == TOK_NUM) {
    Node* c = create_node(current_token.lexeme, 1);
    if (current_token.type == TOK_ID) {
      char buf[50];
      sprintf(buf, "ID(%s)", current_token.lexeme);
      strcpy(c->name, buf);
    } else {
      char buf[50];
      sprintf(buf, "NUM(%s)", current_token.lexeme);
      strcpy(c->name, buf);
    }
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
    printf("Syntax Error on line %d: Expected Identifier, Number, '(', or '!', but encountered '%s'\n", 
           current_token.line, current_token.lexeme);
    exit(1);
  }
  return n;
}

Node* parse_Term() {
  Node* left = parse_Factor();
  while (current_token.type == TOK_MUL || current_token.type == TOK_DIV || current_token.type == TOK_MOD) {
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

Node* parse_ArithExpr() {
  Node* left = parse_Term();
  while (current_token.type == TOK_PLUS || current_token.type == TOK_MINUS) {
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

Node* parse_RelExpr() {
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

Node* parse_EqExpr() {
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

Node* parse_AndExpr() {
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

Node* parse_BoolExpr() {
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

Node* parse_DeclStmt() {
  Node* n = create_node("DeclStmt", 0);
  if (current_token.type == TOK_INT || current_token.type == TOK_FLOAT) {
    add_child(n, match(current_token.type));
    add_child(n, match(TOK_ID));
    
    // Handle optional initialization
    if (current_token.type == TOK_ASSIGN) {
        add_child(n, match(TOK_ASSIGN));
        add_child(n, parse_BoolExpr());
    }
    
    add_child(n, match(TOK_SEMI));
  }
  return n;
}

Node* parse_AssignStmt() {
  Node* n = create_node("AssignStmt", 0);
  add_child(n, match(TOK_ID));
  add_child(n, match(TOK_ASSIGN));
  add_child(n, parse_BoolExpr());
  add_child(n, match(TOK_SEMI));
  return n;
}

Node* parse_Block() {
  Node* n = create_node("Block", 0);
  add_child(n, match(TOK_LBRACE));
  add_child(n, parse_StmtList());
  add_child(n, match(TOK_RBRACE));
  return n;
}

Node* parse_IfStmt() {
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

Node* parse_WhileStmt() {
  Node* n = create_node("WhileStmt", 0);
  add_child(n, match(TOK_WHILE));
  add_child(n, match(TOK_LPAREN));
  add_child(n, parse_BoolExpr());
  add_child(n, match(TOK_RPAREN));
  add_child(n, parse_Block());
  return n;
}

Node* parse_PrintStmt() {
  Node* n = create_node("PrintStmt", 0);
  add_child(n, match(TOK_PRINT));
  add_child(n, match(TOK_LPAREN));
  add_child(n, parse_BoolExpr());
  add_child(n, match(TOK_RPAREN));
  add_child(n, match(TOK_SEMI));
  return n;
}

Node* parse_Stmt() {
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
    printf("Syntax Error on line %d: Unrecognized statement starting with '%s'\n", 
           current_token.line, current_token.lexeme);
    exit(1);
  }
  return n;
}

Node* parse_StmtList() {
  Node* n = create_node("StmtList", 0);
  while (current_token.type != TOK_RBRACE && current_token.type != TOK_EOF) {
    add_child(n, parse_Stmt());
  }
  return n;
}

// --- Derivation and Tree Printers ---

#define MAX_ROWS 2000
#define MAX_COLS 2000
char grid[MAX_ROWS][MAX_COLS];
int max_row_used = 0;

void init_grid() {
    max_row_used = 0;
    for (int r=0; r<MAX_ROWS; r++) {
        for (int c=0; c<MAX_COLS; c++) {
            grid[r][c] = ' ';
        }
    }
}

int compute_width(Node* n) {
  int name_len = strlen(n->name);
  if (n->num_children == 0) {
    n->width = name_len;
    return n->width;
  }
  int total_children_width = 0;
  for (int i=0; i<n->num_children; i++) {
    total_children_width += compute_width(n->children[i]);
    if (i > 0) total_children_width += 3; // spacing
  }
  n->width = total_children_width > name_len ? total_children_width : name_len;
  return n->width;
}

void place_nodes(Node* n, int row, int col_start) {
    if (row > max_row_used) max_row_used = row;
    
    n->center_x = col_start + n->width / 2;
    int name_len = strlen(n->name);
    int name_start = n->center_x - name_len / 2;
    
    for (int i=0; i<name_len; i++) {
        if (name_start + i < MAX_COLS && name_start + i >= 0) {
            grid[row][name_start + i] = n->name[i];
        }
    }
    
    if (n->num_children > 0) {
        if (row + 1 > max_row_used) max_row_used = row + 1;
        int children_total_width = 0;
        for (int i=0; i<n->num_children; i++) {
            children_total_width += n->children[i]->width;
            if (i > 0) children_total_width += 3;
        }
        int offset = (n->width - children_total_width) / 2;
        int current_col = col_start + offset;
        
        for (int i=0; i < n->num_children; i++) {
            place_nodes(n->children[i], row + 2, current_col);
            
            // Connector mapping
            int child_center = n->children[i]->center_x;
            int px = n->center_x;
            if (child_center < px - 1) {
                int mx = (child_center + px) / 2;
                if (mx >= 0 && mx < MAX_COLS) grid[row+1][mx] = '/';
            } else if (child_center > px + 1) {
                int mx = (child_center + px) / 2;
                if (mx >= 0 && mx < MAX_COLS) grid[row+1][mx] = '\\';
            } else {
                if (px >= 0 && px < MAX_COLS) grid[row+1][px] = '|';
            }
            
            current_col += n->children[i]->width + 3;
        }
    }
}

void print_and_save_tree() {
    FILE* f = fopen("parse_tree.txt", "w");
    for (int r=0; r<=max_row_used; r++) {
        int last_char = MAX_COLS - 1;
        while (last_char >= 0 && grid[r][last_char] == ' ') last_char--;
        
        for (int c=0; c<=last_char; c++) {
            putchar(grid[r][c]);
            if (f) fputc(grid[r][c], f);
        }
        putchar('\n');
        if (f) fputc('\n', f);
    }
    if (f) fclose(f);
}

// --- Main Execution ---
int main(int argc, char** argv) {
  char user_input[8192];
  user_input[0] = '\0';
  
  if (argc > 1) {
      FILE* fp = fopen(argv[1], "r");
      if (!fp) {
          printf("Could not open file: %s\n", argv[1]);
          return 1;
      }
      size_t len = fread(user_input, 1, sizeof(user_input) - 1, fp);
      user_input[len] = '\0';
      fclose(fp);
  } else {
      char ch;
      int i = 0;
      printf("Enter a code snippet to parse (end with Ctrl+D or Ctrl+Z):\n");
      while ((ch = getchar()) != EOF && i < sizeof(user_input) - 1) {
        user_input[i++] = ch;
      }
      user_input[i] = '\0';
  }

  if (strlen(user_input) == 0) {
    return 0;
  }

  input_ptr = user_input;
  advance(); // Load the first token

  if (current_token.type != TOK_EOF) {
    Node* ast = parse_StmtList(); // Build AST
    
    if (current_token.type == TOK_EOF) {
      printf("\nParsing Completed Successfully! The syntax is valid.\n\n");
      printf("--- Parse Tree ---\n");
      init_grid();
      compute_width(ast);
      place_nodes(ast, 0, 0);
      print_and_save_tree();
      printf("\n[Parse Tree successfully saved to parse_tree.txt]\n");
    } else {
      printf("Syntax Error on line %d: Extraneous tokens at end of input. Encountered '%s'\n", 
             current_token.line, current_token.lexeme);
    }
  }

  return 0;
}
