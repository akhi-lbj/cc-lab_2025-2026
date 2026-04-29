#include <stdio.h>
#include <ctype.h>
#include <string.h>

#define MAX 100

char keywords[][10] = {"int","float","if","else","while","print"};
int num_keywords = 6;

int isKeyword(char *str){
    for(int i=0;i<num_keywords;i++){
        if(strcmp(str,keywords[i])==0)
            return 1;
    }
    return 0;
}

void lexicalAnalyzer(FILE *fp){

    char ch,next;
    char buffer[MAX];
    int i;

    while((ch=fgetc(fp))!=EOF){

        if(isspace(ch))
            continue;

        /* IDENTIFIER OR KEYWORD */

        if(isalpha(ch) || ch=='_'){
            i=0;

            while(isalnum(ch) || ch=='_'){
                if(i<MAX-1)
                    buffer[i++]=ch;
                ch=fgetc(fp);
            }

            buffer[i]='\0';
            fseek(fp,-1,SEEK_CUR);

            if(isKeyword(buffer))
                printf("Keyword: %s\n",buffer);
            else
                printf("Identifier: %s\n",buffer);
        }

        /* NUMBERS */

        else if(isdigit(ch)){

            i=0;
            int dotCount=0;

            while(isdigit(ch) || ch=='.'){

                if(ch=='.')
                    dotCount++;

                if(i<MAX-1)
                    buffer[i++]=ch;

                ch=fgetc(fp);
            }

            buffer[i]='\0';
            fseek(fp,-1,SEEK_CUR);

            if(dotCount>1)
                printf("Invalid Float: %s\n",buffer);

            else if(dotCount==1)
                printf("Float value: %s\n",buffer);

            else
                printf("Integer value: %s\n",buffer);
        }

        /* OPERATORS */

        else{

            switch(ch){

                case '+':
                case '-':
                case '*':
                case '/':
                case '%':
                    printf("Arithmetic Operator: %c\n",ch);
                    break;

                case '<':
                case '>':
                case '=':
                case '!':
                    next=fgetc(fp);

                    if(next=='='){
                        printf("Relational Operator: %c=\n",ch);
                    }
                    else{
                        fseek(fp,-1,SEEK_CUR);

                        if(ch=='=')
                            printf("Assignment Operator: =\n");
                        else if(ch=='!')
                            printf("Logical Operator: !\n");
                        else
                            printf("Relational Operator: %c\n",ch);
                    }
                    break;

                case '&':
                    next=fgetc(fp);
                    if(next=='&')
                        printf("Logical Operator: &&\n");
                    else{
                        printf("Invalid Operator: &\n");
                        fseek(fp,-1,SEEK_CUR);
                    }
                    break;

                case '|':
                    next=fgetc(fp);
                    if(next=='|')
                        printf("Logical Operator: ||\n");
                    else{
                        printf("Invalid Operator: |\n");
                        fseek(fp,-1,SEEK_CUR);
                    }
                    break;

                case '(':
                case ')':
                case '{':
                case '}':
                case ';':
                    printf("Delimiter: %c\n",ch);
                    break;

                default:
                    printf("Lexical Error: Unknown character %c\n",ch);
            }
        }
    }
}

int main(){

    FILE *fp=fopen("input.txt","r");

    if(fp==NULL){
        printf("File not found\n");
        return 1;
    }

    lexicalAnalyzer(fp);

    fclose(fp);
}