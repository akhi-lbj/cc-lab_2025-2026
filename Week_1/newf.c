#include <ctype.h>
#include <stdio.h>
#include <string.h>
#define MAX 100

char keywords[][10] = {"int", "float", "if", "else", "while", "print"};
int num_keywords = 6;

int isKeyword(char *str) {
  for (int i = 0; i < num_keywords; i++) {
    if (strcmp(str, keywords[i]) == 0)
      return 1;
  }
  return 0;
}

/// @brief 
/// @param fp 
void lexicalAnalyzer(FILE *fp) {
  int ch, next;
  char buffer[MAX];
  int i;
  int line = 1; // track current line number

  while ((ch = fgetc(fp)) != EOF) {
    if (ch == '\n')
      line++;

    if (isspace(ch))
      continue;

    /* IDENTIFIERS & KEYWORDS */
    if (isalpha(ch) || ch == '_') {
      i = 0;
      buffer[i++] = ch;
      
      while ((isalnum(ch = fgetc(fp)) || ch == '_') && i < MAX - 1) {
        buffer[i++] = ch;
      }
      buffer[i] = '\0'; // Fixed null terminator
      ungetc(ch, fp);

      if (isKeyword(buffer))
        printf("<KEYWORD, %s>\n", buffer); 
      else
        printf("<IDENTIFIER, %s>\n", buffer);
    }

    /* NUMBERS */
    else if (isdigit(ch)) {
      i = 0;
      int dotCount = 0;
      buffer[i++] = ch;
      
      while ((isdigit(ch = fgetc(fp)) || ch == '.') && i < MAX - 1) {
        if (ch == '.')
          dotCount++;
        buffer[i++] = ch;
      }
      buffer[i] = '\0'; 
      ungetc(ch, fp);

      if (dotCount == 0)
        printf("<INTEGER, %s>\n", buffer); 
      else if (dotCount == 1)
        printf("<FLOAT, %s>\n", buffer);
      else
        printf("<LEXICAL ERROR: Invalid number %s at line %d>\n", buffer, line);
    }

    /* OPERATORS */
    else {
      switch (ch) {
      case '+':
      case '-':
      case '*':
      case '/':
      case '%':
        printf("<ARITHMETIC_OP, %c>\n", ch); 
        break;
      case '<':
      case '>':
      case '=':
      case '!':
        next = fgetc(fp);
        if (next == '=') {
          printf("<RELATIONAL_OP, %c=>\n", ch); 
        } else {
          ungetc(next, fp);
          if (ch == '=')
            printf("<ASSIGNMENT_OP, =>\n");
          else if (ch == '!')
            printf("<LOGICAL_OP, !>\n");
          else
            printf("<RELATIONAL_OP, %c>\n", ch);
        }
        break;
      case '&':
        if ((next = fgetc(fp)) == '&')
          printf("<LOGICAL_OP, &&>\n"); 
        else {
          printf("<LEXICAL ERROR: & at line %d>\n", line);
          ungetc(next, fp);
        }
        break;
      case '|':
        if ((next = fgetc(fp)) == '|')
          printf("<LOGICAL_OP, ||>\n"); 
        else {
          printf("<LEXICAL ERROR: | at line %d>\n", line);
          ungetc(next, fp);
        }
        break;
      case '(':
      case ')':
      case '{':
      case '}':
      case ';':
      case ',':
        printf("<DELIMITER, %c>\n", ch); 
        break;
      default:
        printf("<LEXICAL ERROR: Unknown symbol %c at line %d>\n", ch, line);
      }
    }
  }
}

int main() {
  FILE *fp = fopen("input.txt", "r");
  if (fp == NULL) {
    printf("Error opening file\n"); 
    return 1;
  }
  lexicalAnalyzer(fp);
  fclose(fp);
  return 0;
}