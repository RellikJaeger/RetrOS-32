/**
 * @file lex.c
 * @author Joe Bayer (joexbayer)
 * @brief Lexer for the minmial C interpreter
 * @version 0.1
 * @date 2023-05-14
 * @see https://github.com/rswier/c4
 * @see https://github.com/lotabout/write-a-C-interpreter
 * @copyright Copyright (c) 2023
 * 
 */

#ifdef __cplusplus
extern "C" {
#endif

/* 128 symbols */
#define LEX_MAX_SYMBOLS 128

#include "lex.h"
#include "vm.h"
//#include "interpreter.h"

struct identifier {
    int token;
    int hash;
    char * name;
    int class;
    int type;
    int value;
    int Bclass;
    int Btype;
    int Bvalue;
};

struct lexer {
    int token;
    int token_val;

    struct identifier symbols[LEX_MAX_SYMBOLS];
    struct identifier* current_id;
    struct identifier* main;

    int basetype;    // the type of a declaration, make it global for convenience
    int expr_type;   // the type of an expression

    // function frame
    //
    // 0: arg 1
    // 1: arg 2
    // 2: arg 3
    // 3: return address
    // 4: old bp pointer  <- lexer_context.index_of_bp
    // 5: local var 1
    // 6: local var 2
    int index_of_bp; // index of bp pointer on stack

} lexer_context;

char *src;
int line = 1;
char* data;
int* text;

int error = 0;
int error_line = 0;

const char lex_errors[22][35] = {
    "All good.", "expected token",
    "unexpected token EOF of expression",
    "bad function call", "undefined variable",
    "bad dereference", "bad address of", "bad lvalue of pre-increment",
    "bad expression", "bad lvalue in assignment", "missing colon in conditional",
    "bad value in increment", "pointer type expected", "compiler error", "bad parameter declaration",
    "duplicate parameter declaration", "bad local declaration", "duplicate local declaration",
    "bad enum identifier", "bad enum initializer", "bad global declaration", "duplicate global declaration"
};

char* lex_get_error()
{
    return lex_errors[error];
}

int lex_get_error_line()
{
    return error_line;
}

void next();

void match(int tk) {
    if (lexer_context.token == tk) {
        next();
        return;
    }
    //twritef("%d: expected token: %c\n", line, tk);
    if(error == 0) error = 1;
    if(error_line == 0) error_line =line;
}

void expression(int level) {
    //DEBUG_PRINT("expression\n");
    // expressions have various format.
    // but majorly can be divided into two parts: unit and operator
    // for example `(char) *a[10] = (int *) func(b > 0 ? 10 : 20);
    // `a[10]` is an unit while `*` is an operator.
    // `func(...)` in total is an unit.
    // so we should first parse those unit and unary operators
    // and then the binary ones
    //
    // also the expression can be in the following types:
    //
    // 1. unit_unary ::= unit | unit unary_op | unary_op unit
    // 2. expr ::= unit_unary (bin_op unit_unary ...)

    // unit_unary()
    struct identifier* id;
    int tmp;
    int *addr;
    {
        if (!lexer_context.token) {
            //twritef("%d: unexpected token EOF of expression\n", line);
            if(error == 0) error = 2;
            if(error_line == 0) error_line =line;
        }
        if (lexer_context.token == Num) {
            match(Num);
            //DEBUG_PRINT("number %d\n", lexer_context.token_val);

            // emit code
            *++text = IMM;
            *++text = lexer_context.token_val;
            lexer_context.expr_type = INT;
        }
        else if (lexer_context.token == '"') {
            // continous string "abc" "abc"
            // emit code
            *++text = IMM;
            *++text = lexer_context.token_val;

            ////DEBUG_PRINT("expression string %s\n", lexer_context.token_val);

            match('"');
            // store the rest strings
            while (lexer_context.token == '"') {
                match('"');
            }

            // append the end of string character '\0', all the data are default
            // to 0, so just move data one position forward.
            data = (char *)(((int)data + sizeof(int)) & (-sizeof(int)));
            lexer_context.expr_type = PTR;
        } else if (lexer_context.token == Sizeof) {
            //DEBUG_PRINT("Sizeof\n");
            // sizeof is actually an unary operator
            // now only `sizeof(int)`, `sizeof(char)` and `sizeof(*...)` are
            // supported.
            match(Sizeof);
            match('(');
            lexer_context.expr_type = INT;

            if (lexer_context.token == Int) {
                match(Int);
            } else if (lexer_context.token == Char) {
                match(Char);
                lexer_context.expr_type = CHAR;
            }

            while (lexer_context.token == Mul) {
                match(Mul);
                lexer_context.expr_type = lexer_context.expr_type + PTR;
            }

            match(')');

            // emit code
            *++text = IMM;
            *++text = (lexer_context.expr_type == CHAR) ? sizeof(char) : sizeof(int);

            lexer_context.expr_type = INT;
        } else if (lexer_context.token == Id) {
            // there are several type when occurs to Id
            // but this is unit, so it can only be
            // 1. function call
            // 2. Enum variable
            // 3. global/local variable
            match(Id);


            id = lexer_context.current_id;
            //DEBUG_PRINT("Context identifier: %s\n", id->name);

            if (lexer_context.token == '(') {
                // function call
                match('(');

                // pass in arguments
                tmp = 0; // number of arguments
                while (lexer_context.token != ')') {
                    expression(Assign);
                    *++text = PUSH;
                    tmp ++;

                    if (lexer_context.token == ',') {
                        match(',');
                    }

                }
                match(')');

                //DEBUG_PRINT("New function\n");

                // emit code
                if (id->class == Sys) {
                    // system functions
                    *++text = id->value;
                }
                else if (id->class == Fun) {
                    // function call
                    *++text = CALL;
                    *++text = id->value;
                }
                else {
                    //twritef("%d: bad function call\n", line);
                    if(error == 0) error = 3;
                    if(error_line == 0) error_line =line;
                }

                // clean the stack for arguments
                if (tmp > 0) {
                    *++text = ADJ;
                    *++text = tmp;
                }
                lexer_context.expr_type = id->type;
            }
            else if (id->class == Num) {
                // enum variable
                *++text = IMM;
                *++text = id->value;
                lexer_context.expr_type = INT;
            }
            else {
                // variable
                if (id->class == Loc) {
                    *++text = LEA;
                    *++text = lexer_context.index_of_bp - (int)id->value;
                }
                else if (id->class == Glo) {
                    *++text = IMM;
                    *++text = id->value;
                }
                else {
                    //twritef("%d: undefined variable\n", line);
                    if(error == 0) error = 4;
                    if(error_line == 0) error_line =line;
                }

                // emit code, default behaviour is to load the value of the
                // address which is stored in `ax`
                lexer_context.expr_type = id->type;
                *++text = (lexer_context.expr_type == CHAR) ? LC : LI;
            }
        }
        else if (lexer_context.token == '(') {
            // cast or parenthesis
            match('(');
            if (lexer_context.token == Int || lexer_context.token == Char) {
                tmp = (lexer_context.token == Char) ? CHAR : INT; // cast type
                match(lexer_context.token);
                while (lexer_context.token == Mul) {
                    match(Mul);
                    tmp = tmp + PTR;
                }

                match(')');

                expression(Inc); // cast has precedence as Inc(++)

                lexer_context.expr_type  = tmp;
            } else {
                // normal parenthesis
                expression(Assign);
                match(')');
            }
        }
        else if (lexer_context.token == Mul) {
            // dereference *<addr>
            match(Mul);
            expression(Inc); // dereference has the same precedence as Inc(++)

            if (lexer_context.expr_type >= PTR) {
                lexer_context.expr_type = lexer_context.expr_type - PTR;
            } else {
                //twritef("%d: bad dereference\n", line);
                if(error == 0) error = 5;
                if(error_line == 0) error_line =line;
            }

            *++text = (lexer_context.expr_type == CHAR) ? LC : LI;
        }
        else if (lexer_context.token == And) {
            // get the address of
            match(And);
            expression(Inc); // get the address of
            if (*text == LC || *text == LI) {
                text --;
            } else {
                //twritef("%d: bad address of\n", line);
                if(error == 0) error = 6;
                if(error_line == 0) error_line =line;
            }

            lexer_context.expr_type = lexer_context.expr_type + PTR;
        }
        else if (lexer_context.token == '!') {
            // not
            match('!');
            expression(Inc);

            // emit code, use <expr> == 0
            *++text = PUSH;
            *++text = IMM;
            *++text = 0;
            *++text = EQ;

            lexer_context.expr_type = INT;
        }
        else if (lexer_context.token == '~') {
            // bitwise not
            match('~');
            expression(Inc);

            // emit code, use <expr> XOR -1
            *++text = PUSH;
            *++text = IMM;
            *++text = -1;
            *++text = XOR;

            lexer_context.expr_type = INT;
        }
        else if (lexer_context.token == Add) {
            // +var, do nothing
            match(Add);
            expression(Inc);

            lexer_context.expr_type = INT;
        }
        else if (lexer_context.token == Sub) {
            // -var
            match(Sub);

            if (lexer_context.token == Num) {
                *++text = IMM;
                *++text = -lexer_context.token_val;
                match(Num);
            } else {

                *++text = IMM;
                *++text = -1;
                *++text = PUSH;
                expression(Inc);
                *++text = MUL;
            }

            lexer_context.expr_type = INT;
        }
        else if (lexer_context.token == Inc || lexer_context.token == Dec) {
            tmp = lexer_context.token;
            match(lexer_context.token);
            expression(Inc);
            if (*text == LC) {
                *text = PUSH;  // to duplicate the address
                *++text = LC;
            } else if (*text == LI) {
                *text = PUSH;
                *++text = LI;
            } else {
                //twritef("%d: bad lvalue of pre-increment\n", line);
                if(error == 0) error = 7;
                if(error_line == 0) error_line =line;
            }
            *++text = PUSH;
            *++text = IMM;
            *++text = (lexer_context.expr_type > PTR) ? sizeof(int) : sizeof(char);
            *++text = (tmp == Inc) ? ADD : SUB;
            *++text = (lexer_context.expr_type == CHAR) ? SC : SI;
        }
        else {
            //twritef("%d: bad expression\n", line);
            if(error == 0) error = 8;
            if(error_line == 0) error_line =line;
        }
    }

    // binary operator and postfix operators.
    {
        while (lexer_context.token >= level) {
            // handle according to current operator's precedence
            tmp = lexer_context.expr_type;
            if (lexer_context.token == Assign) {
                // var = expr;
                match(Assign);
                if (*text == LC || *text == LI) {
                    *text = PUSH; // save the lvalue's pointer
                } else {
                    //twritef("%d: bad lvalue in assignment\n", line);
                    if(error == 0) error = 9;
                    if(error_line == 0) error_line =line;
                }
                expression(Assign);

                lexer_context.expr_type = tmp;
                *++text = (lexer_context.expr_type == CHAR) ? SC : SI;
            }
            else if (lexer_context.token == Cond) {
                // expr ? a : b;
                match(Cond);
                *++text = JZ;
                addr = ++text;
                expression(Assign);
                if (lexer_context.token == ':') {
                    match(':');
                } else {
                    //twritef("%d: missing colon in conditional\n", line);
                    if(error == 0) error = 10;
                    if(error_line == 0) error_line =line;
                }
                *addr = (int)(text + 3);
                *++text = JMP;
                addr = ++text;
                expression(Cond);
                *addr = (int)(text + 1);
            }
            else if (lexer_context.token == Lor) {
                // logic or
                match(Lor);
                *++text = JNZ;
                addr = ++text;
                expression(Lan);
                *addr = (int)(text + 1);
                lexer_context.expr_type = INT;
            }
            else if (lexer_context.token == Lan) {
                // logic and
                match(Lan);
                *++text = JZ;
                addr = ++text;
                expression(Or);
                *addr = (int)(text + 1);
                lexer_context.expr_type = INT;
            }
            else if (lexer_context.token == Or) {
                // bitwise or
                match(Or);
                *++text = PUSH;
                expression(Xor);
                *++text = OR;
                lexer_context.expr_type = INT;
            }
            else if (lexer_context.token == Xor) {
                // bitwise xor
                match(Xor);
                *++text = PUSH;
                expression(And);
                *++text = XOR;
                lexer_context.expr_type = INT;
            }
            else if (lexer_context.token == And) {
                // bitwise and
                match(And);
                *++text = PUSH;
                expression(Eq);
                *++text = AND;
                lexer_context.expr_type = INT;
            }
            else if (lexer_context.token == Eq) {
                // equal ==
                match(Eq);
                *++text = PUSH;
                expression(Ne);
                *++text = EQ;
                lexer_context.expr_type = INT;
            }
            else if (lexer_context.token == Ne) {
                // not equal !=
                match(Ne);
                *++text = PUSH;
                expression(Lt);
                *++text = NE;
                lexer_context.expr_type = INT;
            }
            else if (lexer_context.token == Lt) {
                // less than
                match(Lt);
                *++text = PUSH;
                expression(Shl);
                *++text = LT;
                lexer_context.expr_type = INT;
            }
            else if (lexer_context.token == Gt) {
                // greater than
                match(Gt);
                *++text = PUSH;
                expression(Shl);
                *++text = GT;
                lexer_context.expr_type = INT;
            }
            else if (lexer_context.token == Le) {
                // less than or equal to
                match(Le);
                *++text = PUSH;
                expression(Shl);
                *++text = LE;
                lexer_context.expr_type = INT;
            }
            else if (lexer_context.token == Ge) {
                // greater than or equal to
                match(Ge);
                *++text = PUSH;
                expression(Shl);
                *++text = GE;
                lexer_context.expr_type = INT;
            }
            else if (lexer_context.token == Shl) {
                // shift left
                match(Shl);
                *++text = PUSH;
                expression(Add);
                *++text = SHL;
                lexer_context.expr_type = INT;
            }
            else if (lexer_context.token == Shr) {
                // shift right
                match(Shr);
                *++text = PUSH;
                expression(Add);
                *++text = SHR;
                lexer_context.expr_type = INT;
            }
            else if (lexer_context.token == Add) {
                // add
                match(Add);
                *++text = PUSH;
                expression(Mul);

                lexer_context.expr_type = tmp;
                if (lexer_context.expr_type > PTR) {
                    // pointer type, and not `char *`
                    *++text = PUSH;
                    *++text = IMM;
                    *++text = sizeof(int);
                    *++text = MUL;
                }
                *++text = ADD;
            }
            else if (lexer_context.token == Sub) {
                // sub
                match(Sub);
                *++text = PUSH;
                expression(Mul);
                if (tmp > PTR && tmp == lexer_context.expr_type) {
                    // pointer subtraction
                    *++text = SUB;
                    *++text = PUSH;
                    *++text = IMM;
                    *++text = sizeof(int);
                    *++text = DIV;
                    lexer_context.expr_type = INT;
                } else if (tmp > PTR) {
                    // pointer movement
                    *++text = PUSH;
                    *++text = IMM;
                    *++text = sizeof(int);
                    *++text = MUL;
                    *++text = SUB;
                    lexer_context.expr_type = tmp;
                } else {
                    // numeral subtraction
                    *++text = SUB;
                    lexer_context.expr_type = tmp;
                }
            }
            else if (lexer_context.token == Mul) {
                // multiply
                match(Mul);
                *++text = PUSH;
                expression(Inc);
                *++text = MUL;
                lexer_context.expr_type = tmp;
            }
            else if (lexer_context.token == Div) {
                // divide
                match(Div);
                *++text = PUSH;
                expression(Inc);
                *++text = DIV;
                lexer_context.expr_type = tmp;
            }
            else if (lexer_context.token == Mod) {
                // Modulo
                match(Mod);
                *++text = PUSH;
                expression(Inc);
                *++text = MOD;
                lexer_context.expr_type = tmp;
            }
            else if (lexer_context.token == Inc || lexer_context.token == Dec) {
                // postfix inc(++) and dec(--)
                // we will increase the value to the variable and decrease it
                // on `ax` to get its original value.
                if (*text == LI) {
                    *text = PUSH;
                    *++text = LI;
                }
                else if (*text == LC) {
                    *text = PUSH;
                    *++text = LC;
                }
                else {
                    //twritef("%d: bad value in increment\n", line);
                    if(error == 0) error = 11;
                    if(error_line == 0) error_line =line;
                }

                *++text = PUSH;
                *++text = IMM;
                *++text = (lexer_context.expr_type > PTR) ? sizeof(int) : sizeof(char);
                *++text = (lexer_context.token == Inc) ? ADD : SUB;
                *++text = (lexer_context.expr_type == CHAR) ? SC : SI;
                *++text = PUSH;
                *++text = IMM;
                *++text = (lexer_context.expr_type > PTR) ? sizeof(int) : sizeof(char);
                *++text = (lexer_context.token == Inc) ? SUB : ADD;
                match(lexer_context.token);
            }
            else if (lexer_context.token == Brak) {
                // array access var[xx]
                match(Brak);
                *++text = PUSH;
                expression(Assign);
                match(']');

                if (tmp > PTR) {
                    // pointer, `not char *`
                    *++text = PUSH;
                    *++text = IMM;
                    *++text = sizeof(int);
                    *++text = MUL;
                }
                else if (tmp < PTR) {
                    //twritef("%d: pointer type expected\n", line);
                    if(error == 0) error = 12;
                    if(error_line == 0) error_line =line;
                }
                lexer_context.expr_type = tmp - PTR;
                *++text = ADD;
                *++text = (lexer_context.expr_type == CHAR) ? LC : LI;
            }
            else {
                //twritef("%d: compiler error, token = %d\n", line, lexer_context.token);
                if(error == 0) error = 13;
                if(error_line == 0) error_line =line;
            }
        }
    }
}


void next() {
    char *last_pos;
    int hash;

    while (lexer_context.token = *src) {
        ++src;
        //DEBUG_PRINT("Token %c\n", lexer_context.token);

        // parse lexer_context.token here
        if (lexer_context.token == '\n') {
            ++line;
            //DEBUG_PRINT("new line\n");
            continue;
        }
        else if (lexer_context.token == '#') {
            // skip macro, because we will not support it
            
            //DEBUG_PRINT("skip # macro\n");
            while (*src != 0 && *src != '\n') {
                src++;
            }
        }
        else if ((lexer_context.token >= 'a' && lexer_context.token <= 'z') || (lexer_context.token >= 'A' && lexer_context.token <= 'Z') || (lexer_context.token == '_')) {

            //DEBUG_PRINT("Parsing new identifier\n");
            // parse identifier
            last_pos = src - 1;
            hash = lexer_context.token;

            while ((*src >= 'a' && *src <= 'z') || (*src >= 'A' && *src <= 'Z') || (*src >= '0' && *src <= '9') || (*src == '_')) {
                hash = hash * 147 + *src;
                src++;
            }
            // look for existing identifier, linear search
            lexer_context.current_id = &lexer_context.symbols[0];
            while (lexer_context.current_id->token) {
                if (lexer_context.current_id->hash == hash && !memcmp((char *)lexer_context.current_id->name, last_pos, src - last_pos)) {
                    //found one, return
                    lexer_context.token = lexer_context.current_id->token;
                    ////DEBUG_PRINT("Found identifier %s\n", lexer_context.current_id->name);
                    return;
                }
                lexer_context.current_id += 1;
            }



            // store new ID
            lexer_context.current_id->name = last_pos;
            lexer_context.current_id->hash = hash;
            lexer_context.token = lexer_context.current_id->token = Id;
            ////DEBUG_PRINT("new identifier %s\n", lexer_context.current_id->name);
            return;
        }
        else if (lexer_context.token >= '0' && lexer_context.token <= '9') {
            //DEBUG_PRINT("Parsing new number\n");
            // parse number, three kinds: dec(123) hex(0x123) oct(017)
            lexer_context.token_val = lexer_context.token - '0';
            if (lexer_context.token_val > 0) {
                // dec, starts with [1-9]
                while (*src >= '0' && *src <= '9') {
                    lexer_context.token_val = lexer_context.token_val*10 + *src++ - '0';
                }
            } else {
                // starts with 0
                if (*src == 'x' || *src == 'X') {
                    //hex
                    lexer_context.token = *++src;
                    while ((lexer_context.token >= '0' && lexer_context.token <= '9') || (lexer_context.token >= 'a' && lexer_context.token <= 'f') || (lexer_context.token >= 'A' && lexer_context.token <= 'F')) {
                        lexer_context.token_val = lexer_context.token_val * 16 + (lexer_context.token & 15) + (lexer_context.token >= 'A' ? 9 : 0);
                        lexer_context.token = *++src;
                    }
                } else {
                    // oct
                    while (*src >= '0' && *src <= '7') {
                        lexer_context.token_val = lexer_context.token_val*8 + *src++ - '0';
                    }
                }
            }

            lexer_context.token = Num;
            return;
        }
        else if (lexer_context.token == '"' || lexer_context.token == '\'') {
            //DEBUG_PRINT("Parsing string literal\n");
            // parse string literal, currently, the only supported escape
            // character is '\n', store the string literal into data.
            last_pos = data;
            while (*src != 0 && *src != lexer_context.token) {
                lexer_context.token_val = *src++;
                if (lexer_context.token_val == '\\') {
                    // escape character
                    lexer_context.token_val = *src++;
                    if (lexer_context.token_val == 'n') {
                        lexer_context.token_val = '\n';
                    }
                }

                if (lexer_context.token == '"') {
                    *data++ = lexer_context.token_val;
                }
            }

            src++;
            // if it is a single character, return Num lexer_context.token
            if (lexer_context.token == '"') {
                lexer_context.token_val = (int)last_pos;
            } else {
                lexer_context.token = Num;
            }

            return;
        }
        else if (lexer_context.token == '/') {
            if (*src == '/') {
                //DEBUG_PRINT("Skipping comment...\n");
                // skip comments
                while (*src != 0 && *src != '\n') {
                    ++src;
                }
            } else {
                // divide operator
                lexer_context.token = Div;
                return;
            }
        }
        else if (lexer_context.token == '=') {
            // parse '==' and '='
            if (*src == '=') {
                src ++;
                lexer_context.token = Eq;
            } else {
                lexer_context.token = Assign;
            }
            return;
        }
        else if (lexer_context.token == '+') {
            // parse '+' and '++'
            if (*src == '+') {
                src ++;
                lexer_context.token = Inc;
            } else {
                lexer_context.token = Add;
            }
            return;
        }
        else if (lexer_context.token == '-') {
            // parse '-' and '--'
            if (*src == '-') {
                src ++;
                lexer_context.token = Dec;
            } else {
                lexer_context.token = Sub;
            }
            return;
        }
        else if (lexer_context.token == '!') {
            // parse '!='
            if (*src == '=') {
                src++;
                lexer_context.token = Ne;
            }
            return;
        }
        else if (lexer_context.token == '<') {
            // parse '<=', '<<' or '<'
            if (*src == '=') {
                src ++;
                lexer_context.token = Le;
            } else if (*src == '<') {
                src ++;
                lexer_context.token = Shl;
            } else {
                lexer_context.token = Lt;
            }
            return;
        }
        else if (lexer_context.token == '>') {
            // parse '>=', '>>' or '>'
            if (*src == '=') {
                src ++;
                lexer_context.token = Ge;
            } else if (*src == '>') {
                src ++;
                lexer_context.token = Shr;
            } else {
                lexer_context.token = Gt;
            }
            return;
        }
        else if (lexer_context.token == '|') {
            // parse '|' or '||'
            if (*src == '|') {
                src ++;
                lexer_context.token = Lor;
            } else {
                lexer_context.token = Or;
            }
            return;
        }
        else if (lexer_context.token == '&') {
            // parse '&' and '&&'
            if (*src == '&') {
                src ++;
                lexer_context.token = Lan;
            } else {
                lexer_context.token = And;
            }
            return;
        }
        else if (lexer_context.token == '^') {
            lexer_context.token = Xor;
            return;
        }
        else if (lexer_context.token == '%') {
            lexer_context.token = Mod;
            return;
        }
        else if (lexer_context.token == '*') {
            lexer_context.token = Mul;
            return;
        }
        else if (lexer_context.token == '[') {
            lexer_context.token = Brak;
            return;
        }
        else if (lexer_context.token == '?') {
            lexer_context.token = Cond;
            return;
        }
        else if (lexer_context.token == '~' || lexer_context.token == ';' || lexer_context.token == '{' || lexer_context.token == '}' || lexer_context.token == '(' || lexer_context.token == ')' || lexer_context.token == ']' || lexer_context.token == ',' || lexer_context.token == ':') {
            // directly return the character as lexer_context.token;
            return;
        }
    }
    return;
}

void statement() {
    //DEBUG_PRINT("statement\n");
    // there are 6 kinds of statements here:
    // 1. if (...) <statement> [else <statement>]
    // 2. while (...) <statement>
    // 3. { <statement> }
    // 4. return xxx;
    // 5. <empty statement>;
    // 6. expression; (expression end with semicolon)

    int *a, *b; // bess for branch control

    if (lexer_context.token == If) {
        // if (...) <statement> [else <statement>]
        //
        //   if (...)           <cond>
        //                      JZ a
        //     <statement>      <statement>
        //   else:              JMP b
        // a:                 a:
        //     <statement>      <statement>
        // b:                 b:
        //
        //
        match(If);
        match('(');
        //DEBUG_PRINT("IF expression\n");
        expression(Assign);  // parse condition
        match(')');

        // emit code for if
        *++text = JZ;
        b = ++text;

        //DEBUG_PRINT("IF statement\n");
        statement();         // parse statement
        if (lexer_context.token == Else) { // parse else
            match(Else);

            // emit code for JMP B
            *b = (int)(text + 3);
            *++text = JMP;
            b = ++text;

            //DEBUG_PRINT("if else statementn");
            statement();
        }

        *b = (int)(text + 1);
    }
    else if (lexer_context.token == While) {
        //DEBUG_PRINT("while\n");
        //
        // a:                     a:
        //    while (<cond>)        <cond>
        //                          JZ b
        //     <statement>          <statement>
        //                          JMP a
        // b:                     b:
        match(While);

        a = text + 1;

        match('(');
        expression(Assign);
        match(')');

        *++text = JZ;
        b = ++text;

        statement();

        *++text = JMP;
        *++text = (int)a;
        *b = (int)(text + 1);
    }
    else if (lexer_context.token == '{') {
        // { <statement> ... }
        match('{');

        while (lexer_context.token != '}') {
            //DEBUG_PRINT("Statement Body");
            statement();
        }

        match('}');
    }
    else if (lexer_context.token == Return) {
        // return [expression];
        match(Return);

        if (lexer_context.token != ';') {
            expression(Assign);
        }

        match(';');

        // emit code for return
        *++text = LEV;
    }
    else if (lexer_context.token == ';') {
        // empty statement
        match(';');
    }
    else {
        // a = b; or function_call();
        expression(Assign);
        match(';');
    }
}

void function_parameter() {

    //DEBUG_PRINT("function_parameter\n");
    int type;
    int params;
    params = 0;
    while (lexer_context.token != ')') {
        // int name, ...
        type = INT;
        if (lexer_context.token == Int) {
            match(Int);
        } else if (lexer_context.token == Char) {
            type = CHAR;
            match(Char);
        }

        // pointer type
        while (lexer_context.token == Mul) {
            match(Mul);
            type = type + PTR;
        }

        // parameter name
        if (lexer_context.token != Id) {
            //twritef("%d: bad parameter declaration\n", line);
            if(error == 0) error = 14;
            if(error_line == 0) error_line =line;
        }
        if (lexer_context.current_id->class == Loc) {
            //twritef("%d: duplicate parameter declaration\n", line);
            if(error == 0) error = 15;
            if(error_line == 0) error_line =line;
        }

        match(Id);
        // store the local variable
        //DEBUG_PRINT("store the local variable\n");
        lexer_context.current_id->Bclass = lexer_context.current_id->class; lexer_context.current_id->class  = Loc;
        lexer_context.current_id->Btype  = lexer_context.current_id->type;  lexer_context.current_id->type   = type;
        lexer_context.current_id->Bvalue = lexer_context.current_id->value; lexer_context.current_id->value  = params++;   // index of current parameter

        if (lexer_context.token == ',') {
            match(',');
        }
    }
    lexer_context.index_of_bp = params+1;
}

void function_body() {
    //DEBUG_PRINT("function_body\n");
    // type func_name (...) {...}
    //                   -->|   |<--

    // ... {
    // 1. local declarations
    // 2. statements
    // }

    int pos_local; // position of local variables on the stack.
    int type;
    pos_local = lexer_context.index_of_bp;

    while (lexer_context.token == Int || lexer_context.token == Char) {
        // local variable declaration, just like global ones.
        lexer_context.basetype = (lexer_context.token == Int) ? INT : CHAR;

        match(lexer_context.token);
        while (lexer_context.token != ';') {
            type = lexer_context.basetype;
            while (lexer_context.token == Mul) {
                match(Mul);
                type = type + PTR;
            }

            if (lexer_context.token != Id) {
                // invalid declaration
                //twritef("%d: bad local declaration\n", line);
                if(error == 0) error = 16;
                if(error_line == 0) error_line =line;
            }
            if (lexer_context.current_id->class == Loc) {
                // identifier exists
                //twritef("%d: duplicate local declaration\n", line);
                if(error == 0) error = 17;
                if(error_line == 0) error_line =line;
            }
            match(Id);


            //DEBUG_PRINT("store the local variable\n");
            // store the local variable
            lexer_context.current_id->Bclass = lexer_context.current_id->class; lexer_context.current_id->class  = Loc;
            lexer_context.current_id->Btype  = lexer_context.current_id->type;  lexer_context.current_id->type   = type;
            lexer_context.current_id->Bvalue = lexer_context.current_id->value; lexer_context.current_id->value  = ++pos_local;   // index of current parameter

            if (lexer_context.token == ',') {
                match(',');
            }
        }
        match(';');
    }

    //DEBUG_PRINT("save the stack size for local variables %x\n", text);
    // save the stack size for local variables
    *++text = ENT;
    *++text = pos_local - lexer_context.index_of_bp;

    // statements
    //DEBUG_PRINT("Function: statements\n");
    while (lexer_context.token != '}') {
        statement();
    }

    // emit code for leaving the sub function
    *++text = LEV;
}

void function_declaration() {
    // type func_name (...) {...}
    //               | this part
    //DEBUG_PRINT("function_declaration...\n");

    match('(');
    function_parameter();
    match(')');
    match('{');
    function_body();
    //match('}');

    // unwind local variable declarations for all local variables.
    lexer_context.current_id = lexer_context.symbols;
    while (lexer_context.current_id->token) {
        if (lexer_context.current_id->class == Loc) {
            lexer_context.current_id->class = lexer_context.current_id->Bclass;
            lexer_context.current_id->type  = lexer_context.current_id->Btype;
            lexer_context.current_id->value = lexer_context.current_id->Bvalue;
        }
        lexer_context.current_id = lexer_context.current_id + 1;
    }
}

void enum_declaration() {
    // parse enum [id] { a = 1, b = 3, ...}
    int i;
    i = 0;
    while (lexer_context.token != '}') {
        if (lexer_context.token != Id) {
            //twritef("%d: bad enum identifier %d\n", line, lexer_context.token);
            if(error == 0) error = 18;
            if(error_line == 0) error_line =line;
        }
        next();
        if (lexer_context.token == Assign) {
            // like {a=10}
            next();
            if (lexer_context.token != Num) {
                //twritef("%d: bad enum initializer\n", line);
                if(error == 0) error = 19;
                if(error_line == 0) error_line =line;
            }
            i = lexer_context.token_val;
            next();
        }

        lexer_context.current_id->class = Num;
        lexer_context.current_id->type = INT;
        lexer_context.current_id->value = i++;

        if (lexer_context.token == ',') {
            next();
        }
    }
}

void global_declaration() {
    // global_declaration ::= enum_decl | variable_decl | function_decl
    //
    // enum_decl ::= 'enum' [id] '{' id ['=' 'num'] {',' id ['=' 'num'} '}'
    //
    // variable_decl ::= type {'*'} id { ',' {'*'} id } ';'
    //
    // function_decl ::= type {'*'} id '(' parameter_decl ')' '{' body_decl '}'


    //DEBUG_PRINT("global_declaration\n");

    int type; // tmp, actual type for variable
    int i; // tmp

    lexer_context.basetype = INT;

    // parse enum, this should be treated alone.
    if (lexer_context.token == Enum) {
        // enum [id] { a = 10, b = 20, ... }
        match(Enum);
        if (lexer_context.token != '{') {
            match(Id); // skip the [id] part
        }
        if (lexer_context.token == '{') {
            // parse the assign part
            match('{');
            enum_declaration();
            match('}');
        }

        //DEBUG_PRINT("enum parsed\n");

        match(';');
        return;
    }

    // parse type information
    if (lexer_context.token == Int) {
        match(Int);
        //DEBUG_PRINT("int matched\n");
    }
    else if (lexer_context.token == Char) {
        match(Char);
        lexer_context.basetype = CHAR;
        //DEBUG_PRINT("char matched\n");
    }

    // parse the comma seperated variable declaration.
    while (lexer_context.token != ';' && lexer_context.token != '}') {
        type = lexer_context.basetype;
        // parse pointer type, note that there may exist `int ****x;`
        while (lexer_context.token == Mul) {
            match(Mul);
            type = type + PTR;
            //DEBUG_PRINT("ptr type\n");
        }

        if (lexer_context.token != Id) {
            // invalid declaration
            //twritef("%d: bad global declaration\n", line);
            if(error == 0) error = 20;
            if(error_line == 0) error_line =line;
        }
        if (lexer_context.current_id->class) {
            // identifier exists
            //twritef("%d: duplicate global declaration %s\n", line, lexer_context.current_id->name);
            if(error == 0) error = 21;
            if(error_line == 0) error_line =line;
        }
        match(Id);
        lexer_context.current_id->type = type;
        //DEBUG_PRINT("current type %d\n", type);

        if (lexer_context.token == '(') {
            lexer_context.current_id->class = Fun;
            lexer_context.current_id->value = (text + 1); // the memory address of function
            //DEBUG_PRINT("function_declaration\n");
            function_declaration();
        } else {
            // variable declaration
            //DEBUG_PRINT("variable declaration\n");
            lexer_context.current_id->class = Glo; // global variable
            lexer_context.current_id->value = (int)data; // assign memory address
            data = data + sizeof(int);
        }

        if (lexer_context.token == ',') {
            match(',');
        }
    }
    next();
}


void lex_init()
{
    memset((char*)lexer_context.symbols, 0, LEX_MAX_SYMBOLS*sizeof(struct identifier));

    src = "char else enum if int return sizeof while "
          "open read close printf malloc memset memcmp exit free void main";

     // add keywords to symbol table
    int i = Char;
    while (i <= While) {
        next();
        lexer_context.current_id->token = i++;
    }

    // add library to symbol table
    i = OPEN;
    while (i <= FREE) { 
        next();
        lexer_context.current_id->class = Sys;
        lexer_context.current_id->type = INT;
        lexer_context.current_id->value = i++;
    }

    next(); lexer_context.current_id->token = Char; // handle void type
    next(); lexer_context.main = lexer_context.current_id; // keep track of main
    //DEBUG_PRINT("Added important default lexer_context.tokens and system lexer_context.tokens\n");
}

void* program(int* _text, char* _data, char* _str)
{
    error = 0;
    error_line = 0;
    line = 0;
    lex_init();
    // get next lexer_context.token
    text = _text;
    data = _data;
    src = _str;

    next();
    while (lexer_context.token > 0) {
        global_declaration();
    }

    //dbgprintf("Section sizes: Text: %d. Data: %d\n", text - _text, data - _data);

    return error ? 0 : (void*)lexer_context.main->value;
}


#ifdef __cplusplus
}
#endif
