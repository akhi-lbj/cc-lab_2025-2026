import os

# Paths
week_5_path = "c:/Akhil/BITSian/BITS/Junior/6th Sem/CC/Lab/Lab_Assignment/Week_5/week_5.c"
week_6_path = "c:/Akhil/BITSian/BITS/Junior/6th Sem/CC/Lab/Lab_Assignment/Week_6/week_6.c"

with open(week_5_path, "r", encoding="utf-8") as f:
    lines = f.readlines()

# Find the end of parse_StmtList
end_line = 0
for i, line in enumerate(lines):
    if "ASTNode* parse_StmtList(void) {" in line:
        # Include this function
        for j in range(i, len(lines)):
            if "return list;" in lines[j] and j > i:
                end_line = j + 2
                break
        break

code = "".join(lines[:end_line])

tac_code = """
/* ========================================================================
 * PART 5 - 3-ADDRESS CODE GENERATION (TAC)
 * ======================================================================== */
typedef struct TAC {
    char op[16];
    char arg1[64];
    char arg2[64];
    char result[64];
    struct TAC* next;
} TAC;

TAC* tac_head = NULL;
TAC* tac_tail = NULL;
int temp_counter = 1;
int label_counter = 1;

char* new_temp() {
    char* temp = (char*)malloc(16);
    sprintf(temp, "t%d", temp_counter++);
    return temp;
}

char* new_label() {
    char* label = (char*)malloc(16);
    sprintf(label, "L%d", label_counter++);
    return label;
}

void emit(const char* op, const char* arg1, const char* arg2, const char* result) {
    TAC* instr = (TAC*)malloc(sizeof(TAC));
    strcpy(instr->op, op ? op : "");
    strcpy(instr->arg1, arg1 ? arg1 : "");
    strcpy(instr->arg2, arg2 ? arg2 : "");
    strcpy(instr->result, result ? result : "");
    instr->next = NULL;
    if (!tac_head) tac_head = tac_tail = instr;
    else { tac_tail->next = instr; tac_tail = instr; }
}

void print_tac() {
    printf("\\n========================================\\n");
    printf("GENERATED THREE-ADDRESS CODE (TAC)\\n");
    printf("========================================\\n\\n");
    TAC* curr = tac_head;
    while (curr) {
        if (strcmp(curr->op, "label") == 0) {
            printf("%s:\\n", curr->result);
        } else if (strcmp(curr->op, "goto") == 0) {
            printf("  goto %s\\n", curr->result);
        } else if (strcmp(curr->op, "if") == 0) {
            printf("  if %s goto %s\\n", curr->arg1, curr->result);
        } else if (strcmp(curr->op, "print") == 0) {
            printf("  print %s\\n", curr->arg1);
        } else if (strcmp(curr->op, "=") == 0) {
            printf("  %s = %s\\n", curr->result, curr->arg1);
        } else if (strcmp(curr->op, "!") == 0 || strcmp(curr->op, "uminus") == 0) {
            printf("  %s = %s %s\\n", curr->result, curr->op, curr->arg1);
        } else {
            printf("  %s = %s %s %s\\n", curr->result, curr->arg1, curr->op, curr->arg2);
        }
        curr = curr->next;
    }
    printf("\\n========================================\\n");
    printf("TAC Generation Complete!\\n");
    printf("========================================\\n");
}

char* generate_tac(ASTNode* n);
void generate_bool_tac(ASTNode* n, const char* true_label, const char* false_label);

void generate_bool_tac(ASTNode* n, const char* true_label, const char* false_label) {
    if (!n) return;
    
    if (n->kind == NODE_BINARY_OP && (n->op == TOK_AND || n->op == TOK_OR)) {
        if (n->op == TOK_AND) {
            char* mid_label = new_label();
            generate_bool_tac(n->children[0], mid_label, false_label);
            emit("label", "", "", mid_label);
            generate_bool_tac(n->children[1], true_label, false_label);
            free(mid_label);
        } else if (n->op == TOK_OR) {
            char* mid_label = new_label();
            generate_bool_tac(n->children[0], true_label, mid_label);
            emit("label", "", "", mid_label);
            generate_bool_tac(n->children[1], true_label, false_label);
            free(mid_label);
        }
    } else if (n->kind == NODE_UNARY_OP && n->op == TOK_NOT) {
        generate_bool_tac(n->children[0], false_label, true_label);
    } else if (n->kind == NODE_BINARY_OP && (n->op == TOK_LT || n->op == TOK_GT || n->op == TOK_LE || n->op == TOK_GE || n->op == TOK_EQ || n->op == TOK_NEQ)) {
        char* arg1 = generate_tac(n->children[0]);
        char* arg2 = generate_tac(n->children[1]);
        char op_str[10];
        switch (n->op) {
            case TOK_LT: strcpy(op_str, "<"); break;
            case TOK_GT: strcpy(op_str, ">"); break;
            case TOK_LE: strcpy(op_str, "<="); break;
            case TOK_GE: strcpy(op_str, ">="); break;
            case TOK_EQ: strcpy(op_str, "=="); break;
            case TOK_NEQ: strcpy(op_str, "!="); break;
            default: strcpy(op_str, "???"); break;
        }
        
        char cond_str[128];
        sprintf(cond_str, "%s %s %s", arg1, op_str, arg2);
        emit("if", cond_str, "", true_label);
        emit("goto", "", "", false_label);
        
        if(arg1) free(arg1);
        if(arg2) free(arg2);
    } else {
        char* arg1 = generate_tac(n);
        if(arg1) {
            char cond_str[128];
            sprintf(cond_str, "%s == true", arg1);
            emit("if", cond_str, "", true_label);
            emit("goto", "", "", false_label);
            free(arg1);
        }
    }
}

char* generate_tac(ASTNode* n) {
    if (!n) return NULL;
    
    switch (n->kind) {
        case NODE_LITERAL:
        case NODE_VAR_REF: {
            char* res = strdup(n->lexeme);
            return res;
        }
        case NODE_BINARY_OP: {
            if (n->op == TOK_AND || n->op == TOK_OR || n->op == TOK_LT || n->op == TOK_GT || n->op == TOK_LE || n->op == TOK_GE || n->op == TOK_EQ || n->op == TOK_NEQ) {
                char* true_label = new_label();
                char* false_label = new_label();
                char* end_label = new_label();
                char* res = new_temp();
                generate_bool_tac(n, true_label, false_label);
                emit("label", "", "", true_label);
                emit("=", "true", "", res);
                emit("goto", "", "", end_label);
                emit("label", "", "", false_label);
                emit("=", "false", "", res);
                emit("label", "", "", end_label);
                free(true_label); free(false_label); free(end_label);
                return res;
            }

            char* arg1 = generate_tac(n->children[0]);
            char* arg2 = generate_tac(n->children[1]);
            char* res = new_temp();
            
            char op_str[10];
            switch (n->op) {
                case TOK_PLUS: strcpy(op_str, "+"); break;
                case TOK_MINUS: strcpy(op_str, "-"); break;
                case TOK_MUL: strcpy(op_str, "*"); break;
                case TOK_DIV: strcpy(op_str, "/"); break;
                case TOK_MOD: strcpy(op_str, "%"); break;
                default: strcpy(op_str, "op"); break;
            }
            
            emit(op_str, arg1, arg2, res);
            if(arg1) free(arg1); 
            if(arg2) free(arg2);
            return res;
        }
        case NODE_UNARY_OP: {
            if (n->op == TOK_NOT) {
                char* true_label = new_label();
                char* false_label = new_label();
                char* end_label = new_label();
                char* res = new_temp();
                generate_bool_tac(n, true_label, false_label);
                emit("label", "", "", true_label);
                emit("=", "true", "", res);
                emit("goto", "", "", end_label);
                emit("label", "", "", false_label);
                emit("=", "false", "", res);
                emit("label", "", "", end_label);
                free(true_label); free(false_label); free(end_label);
                return res;
            }
            char* arg1 = generate_tac(n->children[0]);
            char* res = new_temp();
            if (n->op == TOK_MINUS) {
                emit("uminus", arg1, "", res);
            }
            if(arg1) free(arg1);
            return res;
        }
        case NODE_ASSIGN: {
            char* expr_res = generate_tac(n->children[0]);
            if(expr_res) {
                emit("=", expr_res, "", n->lexeme);
                free(expr_res);
            }
            return NULL;
        }
        case NODE_IF: {
            char* true_label = new_label();
            char* false_label = new_label();
            char* exit_label = new_label();
            
            generate_bool_tac(n->children[0], true_label, false_label);
            
            emit("label", "", "", true_label);
            generate_tac(n->children[1]); 
            
            if (n->child_count == 3) { 
                emit("goto", "", "", exit_label);
                emit("label", "", "", false_label);
                generate_tac(n->children[2]); 
                emit("label", "", "", exit_label);
            } else {
                emit("label", "", "", false_label);
            }
            
            free(true_label); free(false_label); free(exit_label);
            return NULL;
        }
        case NODE_WHILE: {
            char* start_label = new_label();
            char* true_label = new_label();
            char* false_label = new_label();
            
            emit("label", "", "", start_label);
            generate_bool_tac(n->children[0], true_label, false_label);
            
            emit("label", "", "", true_label);
            generate_tac(n->children[1]);
            emit("goto", "", "", start_label);
            
            emit("label", "", "", false_label);
            
            free(start_label); free(true_label); free(false_label);
            return NULL;
        }
        case NODE_PRINT: {
            char* expr_res = generate_tac(n->children[0]);
            if(expr_res) {
                emit("print", expr_res, "", "");
                free(expr_res);
            }
            return NULL;
        }
        case NODE_STMT_LIST:
        case NODE_BLOCK: {
            for (int i = 0; i < n->child_count; i++) {
                generate_tac(n->children[i]);
            }
            return NULL;
        }
        default:
            for (int i = 0; i < n->child_count; i++) {
                generate_tac(n->children[i]);
            }
            return NULL;
    }
}

int main(int argc, char **argv) {
    char source[16384] = {0};

    printf("========================================\\n");
    printf("     TAC GENERATOR - Question 4        \\n");
    printf("========================================\\n\\n");
    
    if (argc > 1) {
        FILE *fp = fopen(argv[1], "r");
        if (!fp) { printf("Could not open file: %s\\n", argv[1]); return 1; }
        size_t len = fread(source, 1, sizeof(source)-1, fp);
        source[len] = '\\0';
        fclose(fp);
    } else {
        printf("Enter source code (end with Ctrl+D / Ctrl+Z):\\n");
        int i = 0; int ch;
        while ((ch = getchar()) != EOF && i < (int)sizeof(source)-1)
            source[i++] = (char)ch;
        source[i] = '\\0';
    }
    if (strlen(source) == 0) { printf("No input.\\n"); return 0; }

    printf("\\n----------------------------------------\\n");
    printf("INPUT PROGRAM:\\n");
    printf("----------------------------------------\\n");
    printf("%s\\n", source);

    input_ptr = source;
    current_line = 1;
    advance();

    ASTNode* root = parse_StmtList();

    if (current_token.type == TOK_EOF) {
        generate_tac(root);
        print_tac();
    } else {
        printf("\\n *** Syntax Error: Extraneous tokens after program\\n");
        return 1;
    }

    return 0;
}
"""

with open(week_6_path, "w", encoding="utf-8") as f:
    f.write(code + tac_code)

print("week_6.c generated successfully!")
