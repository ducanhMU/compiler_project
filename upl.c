#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <stdarg.h>

#define MAX_TOKEN_LEN 100
#define MAX_STMTS 100
#define MAX_ERRORS 100
#define MAX_TOKENS 2000
#define MAX_SYMBOLS 100

typedef enum {
    TOK_BEGIN, TOK_END, TOK_IF, TOK_THEN, TOK_ELSE, TOK_DO, TOK_WHILE, TOK_FOR,
    TOK_PRINT, TOK_INT, TOK_BOOL, TOK_TRUE, TOK_FALSE, TOK_ID, TOK_NUM, TOK_EQ,
    TOK_GT, TOK_GTE, TOK_PLUS, TOK_MUL, TOK_ASSIGN, TOK_LPAREN, TOK_RPAREN,
    TOK_LBRACE, TOK_RBRACE, TOK_SEMICOLON, TOK_EOF, TOK_ERROR
} TokenType;

typedef struct {
    TokenType type;
    char text[MAX_TOKEN_LEN];
    int line;
} Token;

typedef struct Node {
    char *label;
    struct Node **children;
    int num_children;
} Node;

typedef struct {
    int line;
    char message[256];
} Error;

typedef struct {
    Token *tokens;
    int capacity;
    int count;
} TokenList;

typedef struct {
    char *name;
    char *type; // "int" or "bool"
    int line;
} Symbol;

typedef struct {
    Symbol *symbols;
    int count;
} SymbolTable;

TokenList token_list;
SymbolTable symbol_table;
Token current_token;
FILE *file;
int line = 1;
Error errors[MAX_ERRORS];
int error_count = 0;
int token_index = -1;
int last_error_line = 0; // Track the line of the last error

// Function prototypes
void init_token_list(void);
void free_token_list(void);
void add_token(TokenType type, const char *text, int line);
void next_token(void);
void tokenize_file(void);
void add_error(int line, const char *format, ...);
void print_errors(void);
Node* make_node(const char *label, int num_children, ...);
void free_tree(Node *node);
void print_tree(Node *node, int depth);
Node* parse_prog(void);
Node* parse_stmts(void);
Node* parse_stmt(void);
Node* parse_if_stmt(void);
Node* parse_if_then(void);
Node* parse_else_opt(void);
Node* parse_do_while_stmt(void);
Node* parse_print_stmt(void);
Node* parse_decl_stmt(void);
Node* parse_type(void);
Node* parse_init_decl(int *line, const char *type);
Node* parse_assign_stmt(void);
Node* parse_for_stmt(void);
Node* parse_expr(void);
Node* parse_eq_expr(void);
Node* parse_rel_expr(void);
Node* parse_add_expr(void);
Node* parse_mul_expr(void);
Node* parse_prim_expr(void);
Node* parse_lit(void);
void skip_to_sync(void);
void init_symbol_table(void);
void free_symbol_table(void);
void add_symbol(const char *name, const char *type, int line);
int is_variable_declared(const char *name);

// Initialize token list
void init_token_list() {
    token_list.capacity = MAX_TOKENS;
    token_list.count = 0;
    token_list.tokens = malloc(sizeof(Token) * token_list.capacity);
}

// Free token list
void free_token_list() {
    free(token_list.tokens);
    token_list.count = 0;
    token_list.capacity = 0;
}

// Add token to list
void add_token(TokenType type, const char *text, int line) {
    if (token_list.count >= token_list.capacity) {
        token_list.capacity *= 2;
        token_list.tokens = realloc(token_list.tokens, sizeof(Token) * token_list.capacity);
    }
    token_list.tokens[token_list.count].type = type;
    strncpy(token_list.tokens[token_list.count].text, text, MAX_TOKEN_LEN - 1);
    token_list.tokens[token_list.count].text[MAX_TOKEN_LEN - 1] = '\0';
    token_list.tokens[token_list.count].line = line;
    token_list.count++;
}

// Initialize symbol table
void init_symbol_table() {
    symbol_table.count = 0;
    symbol_table.symbols = malloc(sizeof(Symbol) * MAX_SYMBOLS);
}

// Free symbol table
void free_symbol_table() {
    for (int i = 0; i < symbol_table.count; i++) {
        free(symbol_table.symbols[i].name);
        free(symbol_table.symbols[i].type);
    }
    free(symbol_table.symbols);
}

// Add symbol to table
void add_symbol(const char *name, const char *type, int line) {
    if (symbol_table.count >= MAX_SYMBOLS) {
        add_error(line, "Too many variables declared");
        return;
    }
    for (int i = 0; i < symbol_table.count; i++) {
        if (strcmp(symbol_table.symbols[i].name, name) == 0) {
            add_error(line, "Variable %s already declared", name);
            return;
        }
    }
    symbol_table.symbols[symbol_table.count].name = strdup(name);
    symbol_table.symbols[symbol_table.count].type = strdup(type);
    symbol_table.symbols[symbol_table.count].line = line;
    symbol_table.count++;
}

// Check if variable is declared
int is_variable_declared(const char *name) {
    for (int i = 0; i < symbol_table.count; i++) {
        if (strcmp(symbol_table.symbols[i].name, name) == 0) {
            return 1;
        }
    }
    return 0;
}

void next_token() {
    if (token_index + 1 < token_list.count) {
        token_index++;
        current_token = token_list.tokens[token_index];
    } else {
        current_token.type = TOK_EOF;
        current_token.line = line;
        current_token.text[0] = '\0';
    }
}

void tokenize_file() {
    int c;
    char text[MAX_TOKEN_LEN];
    while ((c = fgetc(file)) != EOF) {
        if (c == '\n') { line++; continue; }
        if (isspace(c)) continue;
        if (c == '/') {
            int next = fgetc(file);
            if (next == '/') {
                while ((c = fgetc(file)) != '\n' && c != EOF);
                if (c == '\n') line++;
                continue;
            } else if (next == '*') {
                int prev = 0;
                while ((c = fgetc(file)) != EOF) {
                    if (prev == '*' && c == '/') break;
                    if (c == '\n') line++;
                    prev = c;
                }
                if (c == EOF) add_error(line, "Unterminated block comment");
                continue;
            } else {
                ungetc(next, file);
                text[0] = c; text[1] = '\0';
                add_token(TOK_ERROR, text, line);
                add_error(line, "Unsupported operator: %c", c);
                continue;
            }
        }
        if (isalpha(c)) {
            int i = 0;
            text[i++] = c;
            while ((c = fgetc(file)) != EOF && isalnum(c)) {
                if (i < MAX_TOKEN_LEN - 1) text[i++] = c;
            }
            text[i] = '\0';
            ungetc(c, file);
            if (strcmp(text, "begin") == 0) add_token(TOK_BEGIN, text, line);
            else if (strcmp(text, "end") == 0) add_token(TOK_END, text, line);
            else if (strcmp(text, "if") == 0) add_token(TOK_IF, text, line);
            else if (strcmp(text, "then") == 0) add_token(TOK_THEN, text, line);
            else if (strcmp(text, "else") == 0) add_token(TOK_ELSE, text, line);
            else if (strcmp(text, "do") == 0) add_token(TOK_DO, text, line);
            else if (strcmp(text, "while") == 0) add_token(TOK_WHILE, text, line);
            else if (strcmp(text, "for") == 0) add_token(TOK_FOR, text, line);
            else if (strcmp(text, "print") == 0) add_token(TOK_PRINT, text, line);
            else if (strcmp(text, "int") == 0) add_token(TOK_INT, text, line);
            else if (strcmp(text, "bool") == 0) add_token(TOK_BOOL, text, line);
            else if (strcmp(text, "true") == 0) add_token(TOK_TRUE, text, line);
            else if (strcmp(text, "false") == 0) add_token(TOK_FALSE, text, line);
            else {
		int valid = 1, hasNum = 0;
		if (!text[0] || !isalpha(text[0])) valid =  0;
   		 for (int j = 1; text[j]; j++) {
       			 if (isdigit(text[j])) hasNum = 1;
       			 else if (!isalpha(text[j])) valid =  0;
	         	 else if (hasNum) valid =  0;
   		 }
                if (valid) {
                    add_token(TOK_ID, text, line);
                } else {
                    add_token(TOK_ERROR, text, line);
                    add_error(line, "Invalid identifier: %s", text);
                }
            }
            continue;
        }
        if (isdigit(c)) {
            int i = 0;
            text[i++] = c;
            while ((c = fgetc(file)) != EOF && isdigit(c)) {
                if (i < MAX_TOKEN_LEN - 1) text[i++] = c;
            }
            text[i] = '\0';
            ungetc(c, file);
            add_token(TOK_NUM, text, line);
            continue;
        }
        if (c == '=') {
            int next = fgetc(file);
            if (next == '=') {
                text[0] = '='; text[1] = '='; text[2] = '\0';
                add_token(TOK_EQ, text, line);
            } else {
                ungetc(next, file);
                text[0] = '='; text[1] = '\0';
                add_token(TOK_ASSIGN, text, line);
            }
            continue;
        }
        if (c == '>') {
            int next = fgetc(file);
            if (next == '=') {
                text[0] = '>'; text[1] = '='; text[2] = '\0';
                add_token(TOK_GTE, text, line);
            } else {
                ungetc(next, file);
                text[0] = '>'; text[1] = '\0';
                add_token(TOK_GT, text, line);
            }
            continue;
        }
        if (c == '+') {
            text[0] = '+'; text[1] = '\0';
            add_token(TOK_PLUS, text, line);
            continue;
        }
        if (c == '*') {
            text[0] = '*'; text[1] = '\0';
            add_token(TOK_MUL, text, line);
            continue;
        }
        if (c == '(') {
            text[0] = '('; text[1] = '\0';
            add_token(TOK_LPAREN, text, line);
            continue;
        }
        if (c == ')') {
            text[0] = ')'; text[1] = '\0';
            add_token(TOK_RPAREN, text, line);
            continue;
        }
        if (c == '{') {
            text[0] = '{'; text[1] = '\0';
            add_token(TOK_LBRACE, text, line);
            continue;
        }
        if (c == '}') {
            text[0] = '}'; text[1] = '\0';
            add_token(TOK_RBRACE, text, line);
            continue;
        }
        if (c == ';') {
            text[0] = ';'; text[1] = '\0';
            add_token(TOK_SEMICOLON, text, line);
            continue;
        }
        if (c == '<') {
            text[0] = '<'; text[1] = '\0';
            add_token(TOK_ERROR, text, line);
            add_error(line, "Unsupported operator: %c", c);
            continue;
        }
        text[0] = c; text[1] = '\0';
        add_token(TOK_ERROR, text, line);
        add_error(line, "Unsupported operator: %c", c);
    }
    add_token(TOK_EOF, "", line);
}

void add_error(int line, const char *format, ...) {
    if (error_count >= MAX_ERRORS) return;
    if (line == last_error_line) return; // Skip additional errors on the same line
    char message[256];
    va_list args;
    va_start(args, format);
    vsnprintf(message, 256, format, args);
    va_end(args);
    // Avoid duplicate errors with the same message on the same line
    for (int i = 0; i < error_count; i++) {
        if (errors[i].line == line && strcmp(errors[i].message, message) == 0) {
            return;
        }
    }
    errors[error_count].line = line;
    strncpy(errors[error_count].message, message, 256);
    error_count++;
    last_error_line = line; // Update the last error line
}

void print_errors() {
    int printed_lines[MAX_ERRORS] = {0}; // Track printed line numbers
    for (int i = 0; i < error_count; i++) {
        int line = errors[i].line;
        // Print only the first error for each line
        if (!printed_lines[line]) {
            printf("- Error at line %d: %s\n", line, errors[i].message);
            printed_lines[line] = 1;
        }
    }
}

Node* make_node(const char *label, int num_children, ...) {
    Node *node = malloc(sizeof(Node));
    node->label = strdup(label);
    node->num_children = num_children;
    node->children = malloc(sizeof(Node*) * num_children);
    va_list args;
    va_start(args, num_children);
    for (int i = 0; i < num_children; i++) {
        node->children[i] = va_arg(args, Node*);
    }
    va_end(args);
    return node;
}

void free_tree(Node *node) {
    if (!node) return;
    for (int i = 0; i < node->num_children; i++) {
        free_tree(node->children[i]);
    }
    free(node->label);
    free(node->children);
    free(node);
}

void print_tree(Node *node, int depth) {
    if (!node) return;
    for (int i = 0; i < depth; i++) printf("  ");
    printf("%s\n", node->label);
    for (int i = 0; i < node->num_children; i++) {
        print_tree(node->children[i], depth + 1);
    }
}

void skip_to_sync() {
    int current_line = current_token.line;
    while (token_index < token_list.count && token_list.tokens[token_index].line == current_line) {
        token_index++;
    }
    if (token_index < token_list.count) {
        current_token = token_list.tokens[token_index];
    } else {
        current_token.type = TOK_EOF;
        current_token.line = line;
        current_token.text[0] = '\0';
    }
}

Node* parse_prog() {
    if (current_token.type != TOK_BEGIN) {
        add_error(current_token.line, "Expected 'begin'");
        skip_to_sync();
        return NULL;
    }
    next_token();
    Node *stmts = parse_stmts();
    if (current_token.type != TOK_END) {
        if (current_token.line != last_error_line) {
            add_error(current_token.line, "Expected 'end'");
        }
        skip_to_sync();
        free_tree(stmts);
        return NULL;
    }
    next_token();
    return make_node("Prog", 1, stmts ? stmts : make_node("Stmts", 0));
}

Node* parse_stmts() {
    Node *stmt_list[MAX_STMTS];
    int stmt_count = 0;
    while (current_token.type != TOK_END && current_token.type != TOK_RBRACE && current_token.type != TOK_EOF) {
        if (stmt_count >= MAX_STMTS) {
            if (current_token.line != last_error_line) {
                add_error(current_token.line, "Too many statements");
            }
            skip_to_sync();
            break;
        }
        Node *stmt = parse_stmt();
        if (stmt) {
            stmt_list[stmt_count++] = stmt;
        } else {
            if (current_token.line != last_error_line) {
                skip_to_sync();
            }
        }
    }
    if (stmt_count == 0) return NULL;
    Node *node = malloc(sizeof(Node));
    node->label = strdup("Stmts");
    node->num_children = stmt_count;
    node->children = malloc(sizeof(Node*) * stmt_count);
    for (int i = 0; i < stmt_count; i++) {
        node->children[i] = stmt_list[i];
    }
    return node;
}

Node* parse_stmt() {
    // Reset last_error_line for a new statement
    if (current_token.line != last_error_line) {
        last_error_line = 0;
    }
    if (current_token.type == TOK_IF) return parse_if_stmt();
    else if (current_token.type == TOK_DO) return parse_do_while_stmt();
    else if (current_token.type == TOK_PRINT) return parse_print_stmt();
    else if (current_token.type == TOK_INT || current_token.type == TOK_BOOL) return parse_decl_stmt();
    else if (current_token.type == TOK_FOR) return parse_for_stmt();
    else if (current_token.type == TOK_ID) {
        // Peek at the next token to distinguish assignment from invalid declaration
        Token next = token_index + 1 < token_list.count ? token_list.tokens[token_index + 1] : current_token;
        if (next.type == TOK_ASSIGN) {
            return parse_assign_stmt();
        } else {
            if (current_token.line != last_error_line) {
                add_error(current_token.line, "Expected 'int' or 'bool' for declaration or '=' for assignment");
            }
            skip_to_sync();
            return NULL;
        }
    }
    else {
        if (current_token.line != last_error_line) {
            add_error(current_token.line, "Expected 'int', 'bool', identifier, or statement keyword");
        }
        skip_to_sync();
        return NULL;
    }
}

Node* parse_if_stmt() {
    Node *if_then = parse_if_then();
    if (!if_then) return NULL;
    Node *else_opt = parse_else_opt();
    return make_node("IfStmt", 2, if_then, else_opt);
}

Node* parse_if_then() {
    if (current_token.type != TOK_IF) {
        if (current_token.line != last_error_line) {
            add_error(current_token.line, "Expected 'if'");
        }
        skip_to_sync();
        return NULL;
    }
    int if_line = current_token.line;
    next_token();
    if (current_token.type != TOK_LPAREN) {
        if (current_token.line != last_error_line) {
            add_error(current_token.line, "Expected '('");
        }
        skip_to_sync();
        return NULL;
    }
    next_token();
    Node *expr = parse_expr();
    if (!expr) {
        skip_to_sync();
        return NULL;
    }
    if (current_token.type != TOK_RPAREN) {
        if (current_token.line != last_error_line) {
            add_error(current_token.line, "Expected ')'");
        }
        skip_to_sync();
        free_tree(expr);
        return NULL;
    }
    next_token();
    if (current_token.type != TOK_THEN) {
        if (current_token.line != last_error_line) {
            add_error(current_token.line, "Expected 'then'");
        }
        skip_to_sync();
        free_tree(expr);
        return NULL;
    }
    next_token();
    if (current_token.type != TOK_LBRACE) {
        if (current_token.line != last_error_line) {
            add_error(current_token.line, "Expected '{'");
        }
        skip_to_sync();
        free_tree(expr);
        return NULL;
    }
    next_token();
    Node *stmts = parse_stmts();
    if (current_token.type != TOK_RBRACE) {
        if (current_token.line != last_error_line) {
            add_error(current_token.line, "Expected '}'");
        }
        skip_to_sync();
        free_tree(expr);
        free_tree(stmts);
        return NULL;
    }
    next_token();
    return make_node("IfThen", 2, expr, stmts ? stmts : make_node("Stmts", 0));
}

Node* parse_else_opt() {
    if (current_token.type == TOK_ELSE) {
        next_token();
        if (current_token.type != TOK_LBRACE) {
            if (current_token.line != last_error_line) {
                add_error(current_token.line, "Expected '{'");
            }
            skip_to_sync();
            return NULL;
        }
        next_token();
        Node *stmts = parse_stmts();
        if (current_token.type != TOK_RBRACE) {
            if (current_token.line != last_error_line) {
                add_error(current_token.line, "Expected '}'");
            }
            skip_to_sync();
            free_tree(stmts);
            return NULL;
        }
        next_token();
        return make_node("ElseOpt", 1, stmts ? stmts : make_node("Stmts", 0));
    }
    return NULL;
}

Node* parse_do_while_stmt() {
    if (current_token.type != TOK_DO) {
        if (current_token.line != last_error_line) {
            add_error(current_token.line, "Expected 'do'");
        }
        skip_to_sync();
        return NULL;
    }
    next_token();
    if (current_token.type != TOK_LBRACE) {
        if (current_token.line != last_error_line) {
            add_error(current_token.line, "Expected '{'");
        }
        skip_to_sync();
        return NULL;
    }
    next_token();
    Node *stmts = parse_stmts();
    if (current_token.type != TOK_RBRACE) {
        if (current_token.line != last_error_line) {
            add_error(current_token.line, "Expected '}'");
        }
        skip_to_sync();
        free_tree(stmts);
        return NULL;
    }
    next_token();
    if (current_token.type != TOK_WHILE) {
        if (current_token.line != last_error_line) {
            add_error(current_token.line, "Expected 'while'");
        }
        skip_to_sync();
        free_tree(stmts);
        return NULL;
    }
    next_token();
    if (current_token.type != TOK_LPAREN) {
        if (current_token.line != last_error_line) {
            add_error(current_token.line, "Expected '('");
        }
        skip_to_sync();
        free_tree(stmts);
        return NULL;
    }
    next_token();
    Node *expr = parse_expr();
    if (!expr) {
        skip_to_sync();
        free_tree(stmts);
        return NULL;
    }
    if (current_token.type != TOK_RPAREN) {
        if (current_token.line != last_error_line) {
            add_error(current_token.line, "Expected ')'");
        }
        skip_to_sync();
        free_tree(stmts);
        free_tree(expr);
        return NULL;
    }
    next_token();
    if (current_token.type != TOK_SEMICOLON) {
        if (current_token.line != last_error_line) {
            add_error(current_token.line, "Expected ';'");
        }
        skip_to_sync();
        free_tree(stmts);
        free_tree(expr);
        return NULL;
    }
    next_token();
    return make_node("DoWhileStmt", 2, stmts ? stmts : make_node("Stmts", 0), expr);
}

Node* parse_print_stmt() {
    if (current_token.type != TOK_PRINT) {
        if (current_token.line != last_error_line) {
            add_error(current_token.line, "Expected 'print'");
        }
        skip_to_sync();
        return NULL;
    }
    next_token();
    if (current_token.type != TOK_LPAREN) {
        if (current_token.line != last_error_line) {
            add_error(current_token.line, "Expected '('");
        }
        skip_to_sync();
        return NULL;
    }
    next_token();
    Node *expr = parse_expr();
    if (!expr) {
        skip_to_sync();
        return NULL;
    }
    if (current_token.type != TOK_RPAREN) {
        if (current_token.line != last_error_line) {
            add_error(current_token.line, "Expected ')'");
        }
        skip_to_sync();
        free_tree(expr);
        return NULL;
    }
    next_token();
    if (current_token.type != TOK_SEMICOLON) {
        if (current_token.line != last_error_line) {
            add_error(current_token.line, "Expected ';'");
        }
        skip_to_sync();
        free_tree(expr);
        return NULL;
    }
    next_token();
    return make_node("PrintStmt", 1, expr);
}

Node* parse_decl_stmt() {
    Node *type = parse_type();
    if (!type) {
        skip_to_sync();
        return NULL;
    }
    int decl_line;
    const char *type_str = strcmp(type->label, "Type_int") == 0 ? "int" : "bool";
    Node *init_decl = parse_init_decl(&decl_line, type_str);
    if (!init_decl) {
        free_tree(type);
        skip_to_sync();
        return NULL;
    }
    if (current_token.type != TOK_SEMICOLON) {
        if (decl_line != last_error_line) {
            add_error(decl_line, "Expected ';'");
        }
        skip_to_sync();
        free_tree(type);
        free_tree(init_decl);
        return NULL;
    }
    next_token();
    return make_node("DeclStmt", 2, type, init_decl);
}

Node* parse_type() {
    if (current_token.type == TOK_INT) {
        next_token();
        return make_node("Type_int", 0);
    } else if (current_token.type == TOK_BOOL) {
        next_token();
        return make_node("Type_bool", 0);
    } else {
        if (current_token.line != last_error_line) {
            add_error(current_token.line, "Expected 'int' or 'bool'");
        }
        skip_to_sync();
        return NULL;
    }
}

Node* parse_init_decl(int *line, const char *type) {
    if (current_token.type != TOK_ID) {
        if (current_token.line != last_error_line) {
            add_error(current_token.line, "Expected identifier");
        }
        skip_to_sync();
        return NULL;
    }
    *line = current_token.line;
    char *id = strdup(current_token.text);
    next_token();
    if (current_token.type == TOK_ASSIGN) {
        next_token();
        Node *expr = parse_expr();
        if (!expr) {
            free(id);
            skip_to_sync();
            return NULL;
        }
        add_symbol(id, type, *line);
        return make_node("InitDecl", 2, make_node(id, 0), expr);
    }
    add_symbol(id, type, *line);
    return make_node("InitDecl", 1, make_node(id, 0));
}

Node* parse_assign_stmt() {
    if (current_token.type != TOK_ID) {
        if (current_token.line != last_error_line) {
            add_error(current_token.line, "Expected identifier");
        }
        skip_to_sync();
        return NULL;
    }
    char *id = strdup(current_token.text);
    int assign_line = current_token.line;
    if (!is_variable_declared(id)) {
        if (assign_line != last_error_line) {
            add_error(assign_line, "Undeclared variable: %s", id);
        }
        free(id);
        skip_to_sync();
        return NULL;
    }
    next_token();
    if (current_token.type != TOK_ASSIGN) {
        if (current_token.line != last_error_line) {
            add_error(current_token.line, "Expected '='");
        }
        free(id);
        skip_to_sync();
        return NULL;
    }
    next_token();
    Node *expr = parse_expr();
    if (!expr) {
        free(id);
        skip_to_sync();
        return NULL;
    }
    if (current_token.type != TOK_SEMICOLON) {
        if (current_token.line != last_error_line) {
            add_error(current_token.line, "Expected ';'");
        }
        free(id);
        free_tree(expr);
        skip_to_sync();
        return NULL;
    }
    next_token();
    return make_node("AssignStmt", 2, make_node(id, 0), expr);
}

Node* parse_for_stmt() {
    if (current_token.type != TOK_FOR) {
        if (current_token.line != last_error_line) {
            add_error(current_token.line, "Expected 'for'");
        }
        skip_to_sync();
        return NULL;
    }
    next_token();
    if (current_token.type != TOK_LPAREN) {
        if (current_token.line != last_error_line) {
            add_error(current_token.line, "Expected '('");
        }
        skip_to_sync();
        return NULL;
    }
    next_token();
    Node *init = NULL;
    if (current_token.type == TOK_INT || current_token.type == TOK_BOOL) {
        Node *type = parse_type();
        if (!type) {
            skip_to_sync();
            return NULL;
        }
        int decl_line;
        const char *type_str = strcmp(type->label, "Type_int") == 0 ? "int" : "bool";
        Node *init_decl = parse_init_decl(&decl_line, type_str);
        if (!init_decl) {
            free_tree(type);
            skip_to_sync();
            return NULL;
        }
        init = make_node("ForInit", 2, type, init_decl);
    } else if (current_token.type == TOK_ID) {
        char *id = strdup(current_token.text);
        int assign_line = current_token.line;
        if (!is_variable_declared(id)) {
            if (assign_line != last_error_line) {
                add_error(assign_line, "Undeclared variable: %s", id);
            }
            free(id);
            skip_to_sync();
            return NULL;
        }
        next_token();
        if (current_token.type != TOK_ASSIGN) {
            if (current_token.line != last_error_line) {
                add_error(current_token.line, "Expected '='");
            }
            free(id);
            skip_to_sync();
            return NULL;
        }
        next_token();
        Node *expr = parse_expr();
        if (!expr) {
            free(id);
            skip_to_sync();
            return NULL;
        }
        init = make_node("ForInit", 2, make_node(id, 0), expr);
    } else {
        if (current_token.line != last_error_line) {
            add_error(current_token.line, "Expected 'int', 'bool', or identifier for for-loop initialization");
        }
        skip_to_sync();
        return NULL;
    }
    if (current_token.type != TOK_SEMICOLON) {
        if (current_token.line != last_error_line) {
            add_error(current_token.line, "Expected ';' after for-loop initialization");
        }
        free_tree(init);
        skip_to_sync();
        return NULL;
    }
    next_token();
    Node *cond = parse_expr();
    if (!cond) {
        free_tree(init);
        skip_to_sync();
        return NULL;
    }
    if (current_token.type != TOK_SEMICOLON) {
        if (current_token.line != last_error_line) {
            add_error(current_token.line, "Expected ';' after for-loop condition");
        }
        free_tree(init);
        free_tree(cond);
        skip_to_sync();
        return NULL;
    }
    next_token();
    Node *update = NULL;
    if (current_token.type == TOK_ID) {
        char *id = strdup(current_token.text);
        int update_line = current_token.line;
        if (!is_variable_declared(id)) {
            if (update_line != last_error_line) {
                add_error(update_line, "Undeclared variable: %s", id);
            }
            free(id);
            free_tree(init);
            free_tree(cond);
            skip_to_sync();
            return NULL;
        }
        next_token();
        if (current_token.type != TOK_ASSIGN) {
            if (current_token.line != last_error_line) {
                add_error(current_token.line, "Expected '=' in for-loop update");
            }
            free(id);
            free_tree(init);
            free_tree(cond);
            skip_to_sync();
            return NULL;
        }
        next_token();
        Node *expr = parse_expr();
        if (!expr) {
            free(id);
            free_tree(init);
            free_tree(cond);
            skip_to_sync();
            return NULL;
        }
        update = make_node("Update", 2, make_node(id, 0), expr);
    } else {
        if (current_token.line != last_error_line) {
            add_error(current_token.line, "Expected identifier in for-loop update");
        }
        free_tree(init);
        free_tree(cond);
        skip_to_sync();
        return NULL;
    }
    if (current_token.type != TOK_RPAREN) {
        if (current_token.line != last_error_line) {
            add_error(current_token.line, "Expected ')' after for-loop update");
        }
        free_tree(init);
        free_tree(cond);
        free_tree(update);
        skip_to_sync();
        return NULL;
    }
    next_token();
    if (current_token.type != TOK_LBRACE) {
        if (current_token.line != last_error_line) {
            add_error(current_token.line, "Expected '{' for for-loop body");
        }
        free_tree(init);
        free_tree(cond);
        free_tree(update);
        skip_to_sync();
        return NULL;
    }
    next_token();
    Node *stmts = parse_stmts();
    if (current_token.type != TOK_RBRACE) {
        if (current_token.line != last_error_line) {
            add_error(current_token.line, "Expected '}' after for-loop body");
        }
        free_tree(init);
        free_tree(cond);
        free_tree(update);
        free_tree(stmts);
        skip_to_sync();
        return NULL;
    }
    next_token();
    return make_node("ForStmt", 4, init, cond, update, stmts ? stmts : make_node("Stmts", 0));
}

Node* parse_expr() {
    Node *eq = parse_eq_expr();
    return eq;
}

Node* parse_eq_expr() {
    Node *rel = parse_rel_expr();
    if (!rel) return NULL;
    while (current_token.type == TOK_EQ) {
        next_token();
        Node *rel2 = parse_rel_expr();
        if (!rel2) {
            free_tree(rel);
            skip_to_sync();
            return NULL;
        }
        rel = make_node("EqExpr", 2, rel, rel2);
    }
    return rel;
}

Node* parse_rel_expr() {
    Node *add = parse_add_expr();
    if (!add) return NULL;
    while (current_token.type == TOK_GT || current_token.type == TOK_GTE) {
        TokenType op = current_token.type;
        next_token();
        Node *add2 = parse_add_expr();
        if (!add2) {
            free_tree(add);
            skip_to_sync();
            return NULL;
        }
        add = make_node(op == TOK_GT ? "Gt" : "Gte", 2, add, add2);
    }
    return add;
}

Node* parse_add_expr() {
    Node *mul = parse_mul_expr();
    if (!mul) return NULL;
    while (current_token.type == TOK_PLUS) {
        next_token();
        Node *mul2 = parse_mul_expr();
        if (!mul2) {
            free_tree(mul);
            skip_to_sync();
            return NULL;
        }
        mul = make_node("AddExpr", 2, mul, mul2);
    }
    return mul;
}

Node* parse_mul_expr() {
    Node *prim = parse_prim_expr();
    if (!prim) return NULL;
    while (current_token.type == TOK_MUL) {
        next_token();
        Node *prim2 = parse_prim_expr();
        if (!prim2) {
            free_tree(prim);
            skip_to_sync();
            return NULL;
        }
        prim = make_node("MulExpr", 2, prim, prim2);
    }
    return prim;
}

Node* parse_prim_expr() {
    if (current_token.type == TOK_ID) {
        char *id = strdup(current_token.text);
        int expr_line = current_token.line;
        if (!is_variable_declared(id)) {
            if (expr_line != last_error_line) {
                add_error(expr_line, "Undeclared variable: %s", id);
            }
            free(id);
            skip_to_sync();
            return NULL;
        }
        next_token();
        return make_node("Id", 1, make_node(id, 0));
    } else if (current_token.type == TOK_NUM || current_token.type == TOK_TRUE || current_token.type == TOK_FALSE) {
        return parse_lit();
    } else if (current_token.type == TOK_LPAREN) {
        next_token();
        Node *expr = parse_expr();
        if (!expr) {
            skip_to_sync();
            return NULL;
        }
        if (current_token.type != TOK_RPAREN) {
            if (current_token.line != last_error_line) {
                add_error(current_token.line, "Expected ')'");
            }
            free_tree(expr);
            skip_to_sync();
            return NULL;
        }
        next_token();
        return expr;
    } else {
        if (current_token.line != last_error_line) {
            add_error(current_token.line, "Invalid primary expression");
        }
        skip_to_sync();
        return NULL;
    }
}

Node* parse_lit() {
    if (current_token.type == TOK_NUM) {
        char *num = strdup(current_token.text);
        next_token();
        return make_node("Num", 1, make_node(num, 0));
    } else if (current_token.type == TOK_TRUE) {
        next_token();
        return make_node("True", 0);
    } else if (current_token.type == TOK_FALSE) {
        next_token();
        return make_node("False", 0);
    }
    if (current_token.line != last_error_line) {
        add_error(current_token.line, "Expected literal");
    }
    skip_to_sync();
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <filename>\n", argv[0]);
        exit(1);
    }
    file = fopen(argv[1], "r");
    if (!file) {
        fprintf(stderr, "Could not open file %s\n", argv[1]);
        exit(1);
    }
    init_token_list();
    init_symbol_table();
    tokenize_file();
    fclose(file);
    token_index = -1;
    next_token();
    Node *root = parse_prog();
    if (error_count > 0 || !root || current_token.type != TOK_EOF) {
        printf("- source code has correct syntax: no\n");
        print_errors();
    } else {
        printf("- source code has correct syntax: yes\n");
        print_tree(root, 0);
    }
    free_tree(root);
    free_token_list();
    free_symbol_table();
    return error_count > 0 ? 1 : 0;
}
