#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>

int token;				// current token
char *src, *old_src;	// pointer to source code string
int poolsize;			// default size of test/data/stack
int line;				// line number

int 
	*text,				// text segment
	*old_text,			// for dump text segment
	*stack;				// stack
char *data;				// data segment

/////////////////////////////////////////////////////////////////
int *pc, *bp, *sp, ax, cycle; //virtual machine registers

//instructions
enum {
	LEA, IMM, JMP, CALL, JZ, JNZ, ENT, ADJ, LEV, LI, LC, SI, SC, PUSH,
	OR, XOR, AND, EQ, NE, LT, GT, LE, GE, SHL, SHR, ADD, SUB, MUL, DIV, MOD,
	OPEN, READ, CLOS, PRTF, MALC, MSET, MCMP, EXIT
};

///////////////////////////////////////////////////////////////////
//token and classes (operators last and in precedence order
enum {
	Num = 128, Fun, Sys, Glo, Loc, Id,
	Char, Else, Enum, If, Int, Return, Sizeof, While,
	Assign, Cond, Lor, Lan, Or, Xor, And, Eq, Ne, Lt, Gt, Le, Ge,
	Shl, Shr, Add, Sub, Mul, Div, Mod, Inc, Dec, Brak
};

//struct identifier{
//	int token;
//	int hash;
//	char *name;
//	int class;
//	int type;
//	int value;
//	int Bclass;
//	int Btype;
//	int Bvalue;
//}

int token_val;		// value of current token (mainly for number)
int *current_id,	// current parsed ID
	*symbols;		// symbol table

// fields of identifier
enum{
	Token, Hash, Name, Type, Class, Value, BType, BClass,BValue, IdSize
};

///////////////////////////////////////////////////////////////////
//types of variable/function
enum {
	CHAR, INT, PTR
};
int *idmain;			// the 'main' function

int basetype; 			// the type of a declaration, make it global for convenience
int expr_type;			// the type of an expression
///////////////////////////////////////////////////////////////////

int index_of_bp;// index of bp pointer on stack

void next(){
	//token = *src++;

	char *last_pos;
	int hash;

	while(token == *src){
		++src;
		//parse token here

		if(token == '\n'){
			++line;
		}
		else if(token == '#'){
			//skip macro, because we will not support it
			while(*src != 0 && *src != '\n'){
				++src;
			}
		}
		//identifier
		else if((token >= 'a' && token <= 'z') || (token >= 'A' && token <= 'Z') || (token == '_')){
			// parse identifier
			last_pos = src - 1;
			hash = token;

			while((*src >= 'a' && *src <= 'z') || (*src >= 'A' && *src <= 'Z') || (*src >= '0' && *src <= '9') || (*src == '_')){
				hash = hash * 147 + *src;
				++src;
			}

			// look for existing identifier, linear search
			current_id = symbols;
			while(current_id[Token]){
				if(current_id[Hash] == hash && !memcmp((char*)current_id[Name], last_pos, src - last_pos)){
					//found one, return
					token = current_id[Token];
					return;
				}
				current_id = current_id + IdSize;
			}

			//store new ID
			current_id[Name] = (int)last_pos;
			current_id[Hash] = hash;
			token = current_id[Token] = Id;
			return;
		}
		//number
		else if(token >= '0' && token <= '9'){
			// parse number, there kinds:dec(123) hex(0x123) oct(017)
			token_val = token - '0';
			if(token_val >0){
				//dec, starts with [1~9]
				while(*src >= '0' && *src <= '9'){
					token_val = token_val * 10 + *src++ - '0';
				}
			} else {
				//start with number 0
				if(*src == 'x' || *src == 'X'){
					//hex
					token = *++src;
					while((token >= '0' && token <= '9') || (token >= 'a' && token <= 'f') || (token >= 'A' && token <= 'F')){
						// ASCII 'a'=0x61  'A'=0x41, so we can use (token &15)
						token_val = token_val * 16 + (token & 15) + (token >= 'A' ? 9:0);
						token = *++src;
					}
				} else {
					//oct
					while(*src >= '0' && *src <= '7'){
						token_val = token_val * 8 + *src++ - '0';
					}
				}
			}
			token = Num;
			return;
		}
		//string
		else if(token == '"' || token == '\''){
			// parse string literal, currently, the only supported escape
			// character is '\n', store the string literal into data
			last_pos = data;
			while(*src != 0 && *src != token){
				token_val = *src++;
				if(token_val == '\\'){
					// escape character
					token_val = *src++;
					if(token_val == 'n'){
						token_val = '\n';
					}
				}
				if(token == '"'){
					*data++ = token_val;
				}
			}
			++src;
			// if it is a single character, return Num token
			if(token == '"'){
				token_val = (int)last_pos;
			} else {
				token = Num;
			}
			return;
		}
		//comments
		//not support /* comment */

		else if (token == '/'){
			if (*src == '/'){
				//skip comments
				while(*src != 0 && *src != '\n'){
					++src;
				}
			} else {
				// divide operator
				token = Div;
				return;
			}
		}
		/////////////////////////////////////////////////////////////////
		else if(token == '='){
			// parse '==' and '='
			if(*src == '='){
				++src;
				token = Eq;
			} else {
				token = Assign;
			}
			return;
		}
		else if(token == '+'){
			//parse '+' and '++'
			if(*src == '+'){
				++src;
				token = Inc;
			} else {
				token = Add;
			}
			return;
		}
		else if(token == '-'){
			//parse '-' and '--'
			if(*src == '-'){
				++src;
				token = Dec;
			} else {
				token = Sub;
			}
			return ;
		}
		else if(token == '<'){
			//parse '<=', '<<' or '<'
			if(*src == '='){
				++src;
				token = Le;
			} 
			else if(*src == '<'){
				++src;
				token = Shl;
			} else {
				token = Lt;
			}
			return;
		}
		else if(token == '>'){
			//parse '>=','>>','>'
			if(*src == '='){
				++src;
				token = Ge;
			}
			else if(*src == '>'){
				++src;
				token = Shr;
			} else {
				token = Gt;
			}
			return;
		}
		else if (token == '|'){
			// parse '|' or '||'
			if(*src == '|'){
				++src;
				token = Lor;
			} else {
				token = Or;
			}
			return ;
		}
		else if(token == '&'){
			// parse '&' and '&&'
			if(*src == '&'){
				++src;
				token = Lan;
			} else {
				token =And;
			}
			return;
		}
		else if(token == '^'){
			token = Xor;
			return;
		}
		else if(token == '%'){
			token = Mod;
			return;
		}
		else if(token == '*'){
			token = Mul;
			return;
		}
		else if(token == '['){
			token = Brak;
			return;
		}
		else if(token == '?'){
			token = Cond;
			return;
		}
		else if (token == '~' || token == ';' || token == '{' || token == '}' || token == '(' || token == ')' || token == ']' || token == ',' || token == ':'){
			// directly return the character as token;
			return;
		}

	}

	return;
}

void match(int tk){
	if(token == tk){
		next();
	} else {
		printf("%d: expected token: %d\n", line, tk);
		exit(-1);
	}
}

void expression(int level){
	int *id;
	int tmp;
	int *addr;
	if(!token){
		printf("%d: unexpected token EOF of expresstion\n", line);
		exit(-1);
	}
	if(token == Num){
		match(Num);
		//emit code
		*++text = IMM;
		*++text = token_val;
		expr_type = INT;
	}
	else if(token == '"'){
		// continout string "abc" "def" = "abcdef"

		//emit code
		*++text = IMM;
		*++text = token_val;
		match('"');
		// store the rest string
		while(token == '"'){
			match('"');
		}
		// just move data one position forward.
		data = (char*)(((int)data + sizeof(int)) & (-sizeof(int)));
		expr_type = PTR;
	}
	else if(token == Sizeof){
		// only 'sizeof(int)', 'sizeof(char)' and 'sizeof(*...)' are supported.
		match(Sizeof);
		match('(');
		expr_type = INT;

		if(token == Int){
			match(Int);
		}
		else if(token == Char){
			match(Char);
			expr_type = CHAR;
		}

		while(token == Mul){
			match(Mul);
			expr_type = expr_type + PTR;
		}
		match(')');

		//emit code
		*++text = IMM;
		*++text = (expr_type == CHAR) ? sizeof(char) : sizeof(int);

		expr_type = INT;
	}
	else if(token == Id){
		// 1. function call 2. Enum variable 3. global/local variable
		match(Id);
		id = current_id;
		if(token == '('){
			// function call
			match('(');
			tmp = 0; //number of arguments
			while(token != ')'){
				expression(Assign);
				*++text = PUSH;
				tmp++;
				if(token == ','){
					match(',');
				}
			}
			match(')');

			//emit code
			if(id[Class] == Sys){
				// system functions
				*++text = id[Value];
			}
			else if(id[Class] == Fun){
				// function call
				*++text = CALL;
				*++text = id[Value];
			}
			else {
				printf("%d: bad function call\n", line);
				exit(-1);
			}

			//clean the stack for arguments
			if(tmp > 0){
				*++text = ADJ;
				*++text = tmp;
			}
			expr_type = id[Type];
		}
		else if(id[Class] == Num){
			//enum variable
			*++text = IMM;
			*++text = id[Value];
			expr_type = INT;
		}
		else{
			//variable
			if(id[Class] == Loc){
				*++text = LEA;
				*++text = index_of_bp - id[Value];
			}
			else if(id[Class] == Glo){
				*++text = IMM;
				*++text = id[Value];
			}
			else{
				printf("%d: undefined variable\n", line);
				exit(-1);
			}
			// emit code, default behaviour is to load the value of the 
			// address which is stored in 'ax'
			expr_type = id[Type];
			*++text = (expr_type == Char) ? LC : LI;
		}
	}
	else if(token == '('){
		match('(');
		if(token == Int || token == Char){
			tmp = (token == Char) ? CHAR : INT;
			match(token);
			while(token == Mul){
				match(Mul);
				tmp = tmp + PTR;
			}
			match(')');

			expression(Inc); 	// cast has precedence as Inc(++)

			expr_type = tmp;
		}
		else{
			//normal parenthesis
			expression(Assign);
			match(')');
		}
	}
	else if(token == Mul){
		// dereference *<addr>
		match(Mul);
		expression(Inc); // dereference has the same precedence as Inc(++)

		if(expr_type >= PTR){
			expr_type = expr_type - PTR;
		}
		else{
			printf("%d: bad dereference\n", line);
			exit(-1);
		}
		*++text = (expr_type == CHAR) ? LC : LI;
	}
	else if(token == And){
		// get the address of
		match(And);
		expression(Inc);	// get the address of
		if(*text == LC || *text == LI){
			--text;
		}
		else{
			printf("%d: bad address of \n", line);
			exit(-1);
		}
		expr_type = expr_type + PTR;
	}
	else if(token == '!'){
		match('!');
		expression(Inc);

		// emit code, use <expr> == 0
		*++text = PUSH;
		*++text = IMM;
		*++text = 0;
		*++text = EQ;

		expr_type = INT;
	}
	else if(token == '~'){
		//bitwise not
		match('~');
		expression(Inc);

		*++text = PUSH;
		*++text = IMM;
		*++text = -1;
		*++text = XOR;

		expr_type = INT;
	}
	else if(token == Add){
		match(Add);
		expression(Inc);

		expr_type = INT;
	}
	else if(token == Sub){
		match(Sub);
		if(token == Num){
			*++text = IMM;
			*++text = -token_val;
		}
		else {
			*++text = IMM;
			*++text = -1;
			*++text = PUSH;
			expression(Inc);
			*++text =MUL;
		}
		expr_type = INT;
	}
	else if(token == Inc || token == Dec){
		tmp = token;
		match(token);
		expression(Inc);
		if(*text == LC){
			*text = PUSH;	// to duplicate the address
			*++text = LI;
		}
		else {
			printf("%d: bad lvalue of pre-increment\n", line);
			exit(-1);
		}
		*++text = PUSH;
		*++text = IMM;
		
		*++text = (expr_type > PTR) ? sizeof(int) : sizeof(char);
		*++text = (tmp == Inc) ? ADD : SUB;
		*++text = (expr_type == CHAR) ? SC : SI;
	}
	else{
		printf("%d: bad expression\n", line);
		exit(-1);
	}
	
	// binary operator and postfix operations.
	while(token >= level){
		//handle according to current operator's precedence
		tmp = expr_type;
		if(token = Assign){
			// var = expr;
			match(Assign);
			if(*text == LC || *text == LI){
				*text = PUSH; //save the lvalue's pointer
			} else {
				printf("%d: bad lvalue in assignment\n", line);
				exit(-1);
			}
			expression(Assign);

			expr_type = tmp;
			*++text = (expr_type == CHAR) ? SC : SI;
		}
		else if (token == Cond){
			// expr ? a : b;
			match(Cond);
			*++text = JZ;
			addr = ++text;
			expression(Assign);
			if(token == ':'){
				match(':');
			} else {
				printf("%d: missing colon in conditional\n", line);
				exit(-1);
			}
			*addr = (int)(text + 3);
			*++text = JMP;
			addr = ++text;
			expression(Cond);
			*addr = (int)(text + 1);
		}
		else if (token = Lor){
			// logic or
			match(Lor);
			*++text = JNZ;
			addr = ++text;
			expression(Lan);
			*addr = (int)(text + 1);
			expr_type = INT;
		}
		else if (token == Lan){
			//logic and
			match(Lan):
			*++text = JZ;
			expression(Or);
			addr = (int)(text + 1);
			expr_type = INT;
		}
		else if (token == Or){
			//bitwise or
			match(Or);
			*++text = PUSH;
			expression(Xor);
			*++text = OR;
			expr_type = INT;
		}
//////////.................................///////////////
	}
}

void statement(){
	int *a, *b; // for branch control
	
	if(token == If){
		match(If);
		match('(');
		expression(Assign); // parse condition
		match(')');

		*++text = JZ;
		b = ++text;
		statement(); // parse statement
		if(token == Else){
			match(Else);

			// emit code for JMP B
			
			*b = (int)(text + 3);
			*++text = JMP;
			b = ++text;

			statement();
		}
		*b = (int)(text + 1);
	}
	else if (token == While){
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
	else if(token == Return){
		// return <expression>;
		match(Return);
		if(token != ';'){
			expression(Assign);
		}
		match(';');
		//emit code for return 
		*++text = LEV;
	}
	else if(token == '{'){
		match('{');
		while(token != '}'){
			statement();
		}
		match('}');
	}
	else if(token == ';'){
		match(';');
	} else{
		// a = b; or function_call();
		expression(Assign);
		match(';');
	}
}

void function_parameter(){
	int type;
	int params;
	params = 0;
	while(token != ')'){
		type = Int;
		if(token == Int){
			match(Int);
		} 
		else if(token == Char){
			type = Char;
			match(Char);
		}
		//pointer type
		while(token == Mul){
			match(Mul);
			type = type + PTR;
		}
		//parameter name
		if(token != Id){
			printf("%d: bad parameter declaration\n", line);
			exit(-1);
		}
		if(current_id[Class] == Loc){
			printf("%d: duplicate parameter declaration\n", line);
			exit(-1);
		}
		match(Id);
		current_id[BClass] = current_id[Class]; current_id[Class] = Loc;
		current_id[BType] = current_id[Type]; current_id[Type] = type;
		current_id[BValue] = current_id[Value]; current_id[Value] = params++;
		if(token == ','){
			match(',');
		}
	}
	index_of_bp = params + 1;
}

void function_body(){
	//type func_name (...) {...}
	//                     |<->|
	//{
	//	1.local declaration
	//	2.statements
	//}
	int pos_local;
	int type;
	pos_local = index_of_bp;

	while(token == Int || token == Char){
		//local variable declaration, just like global ones.
		basetype = (token == Int)?INT:CHAR;
		match(token);
		while(token != ';'){
			match(Mul);
			type = type + PTR;
		}
		if(token != Id){
			printf("%d: bad local declaration\n", line);
			exit(-1);
		}
		if(current_id[Class] == Loc){
			printf("%d: duplicate local declaration\n", line);
			exit(-1);
		}
		match(Id);
		current_id[BClass] = current_id[Class]; current_id[Class] = Loc;
		current_id[BType] = current_id[Type]; current_id[Type] = type;
		current_id[BValue] = current_id[Value]; current_id[Value] = ++pos_local;
		if(token == ','){
			match(',');
		}
	}
	match(';');
	//save the stack size for local variables
	*++text = ENT;
	*++text = pos_local - index_of_bp;
	//statements
	while(token != '}'){
		statement();
	}
	//emit code for leaving the sub function
	*++text = LEV;
}

void function_declaraion(){
	//type func_name (...) {...}
	//               |<->|
	match('(');
	function_parameter();
	match(')');
	function_body();
	//match('}');
	
	current_id = symbols;
	while(current_id[Token]){
		if(current_id[Class] == Loc){
			current_id[Class] = current_id[BClass];
			current_id[Type] = current_id[BType];
			current_id[Value] = current_id[BValue];
		}
		current_id = current_id + IdSize;
	}
}

void program(){
	next();				// get next token
	while(token > 0){
		printf("token is : %c\n", token);
		next(-1);
	}
}

int eval(){
	int op, *tmp;
	while(1){
		op = *pc++;

		if(op == IMM){ax = *pc++;}
		else if(op == LC){ax = *(char*)ax;}
		else if(op == LI){ax = *(int*)ax;}
		else if(op == SC){ax = *(char*)*sp++ = ax;}
		else if(op == SI){ax = *(int*)*sp++ = ax;}
		else if(op == PUSH){*--sp = ax;}
		else if(op == JMP){pc = (int*)*pc;}
		else if(op == JZ){pc = ax?pc+1:(int*)*pc;}
		else if(op == JNZ){pc = ax?(int*)*pc:pc+1;}
		else if(op == CALL){*--sp = (int)(pc+1); pc = (int*)*pc;}
		//else if (op == RET){pc = (int*)*sp++;}
		else if(op == ENT){*--sp = (int)bp; bp = sp; sp = sp - *pc++;}
		else if(op == ADJ){sp = sp + *pc++;}
		else if(op == LEV){sp = bp; bp = (int*)*sp++; pc = (int*)*sp++;}
		else if(op == LEA){ax = (int)(bp + *pc++);}

		else if(op == OR) ax = *sp++ | ax;
		else if(op == XOR) ax = *sp++ ^ ax;
		else if(op == AND) ax = *sp++ & ax;
		else if(op == EQ) ax = *sp++ == ax;
		else if(op == NE) ax = *sp++ != ax;
		else if(op == LT) ax = *sp++ < ax;
		else if(op == GT) ax = *sp++ > ax;
		else if(op == LE) ax = *sp++ <= ax;
		else if(op == GE) ax = *sp++ >= ax;
		else if(op == SHL) ax = *sp++ << ax;
		else if(op == SHR) ax = *sp++ >> ax;
		else if(op == ADD) ax = *sp++ + ax;
		else if(op == SUB) ax = *sp++ - ax;
		else if(op == MUL) ax = *sp++ * ax;
		else if(op == DIV) ax = *sp++ / ax;
		else if(op == MOD) ax = *sp++ % ax;

		else if(op == EXIT){printf("exit(%d)", *sp);return *sp;}
		else if(op == OPEN){ax = open((char*)sp[1],sp[0]);}
		else if(op == CLOS){ax = close(*sp);}
		else if(op == READ){ax = read(sp[2], (char*)sp[1], sp[0]);}
		else if(op == PRTF){tmp = sp + pc[1];ax = printf((char*)tmp[-1], tmp[-2], tmp[-3], tmp[-4], tmp[-5], tmp[-6]);}
		else if(op == MALC){ax = (int)malloc(*sp);}
		else if(op == MSET){ax = (int)memset((char*)sp[2], sp[1], sp[0]);}
		else if(op == MCMP){ax = memcmp((char*)sp[2], (char*)sp[1], sp[0]);}
		else{
			printf("unknow instruction:%d\n", op);
			return -1;
		}
	}
}

int main(int argc, char **argv){
	int i , fd;
	argc--;
	argv++;

	poolsize = 256 * 1024; //arbitrary size
/*
	if((fd = open(*argv,0)) < 0){
		printf("could not open(%s)", *argv);
		return -1;
	}
	
	if(!(src = old_src = malloc(poolsize))){
		printf("could not malloc (%d) for source area\n",poolsize);
		return -1;
	}
	
	if((i = read(fd, src, poolsize-1))<=0){
		printf("read() returned %d\n",i);
		return -1;
	}
	
	src[i]=0;				//add EOF character
	close(fd);
*/
	//////////////////////////////////////////////////////////////////
	if(!(text = old_text = malloc(poolsize))){
		printf("could not malloc (%d) for text area \n",poolsize);
		return -1;
	}
	if(!(data = malloc(poolsize))){
		printf("could not malloc (%d) for data area\n",poolsize);
	}
	if(!(stack = malloc(poolsize))){
		printf("could not malloc (%d) for data area\n",poolsize);
		return -1;
	}
	memset(text, 0, poolsize);
	memset(data, 0, poolsize);
	memset(stack, 0, poolsize);

	bp = sp = (int*)((int)stack + poolsize);
	ax = 0;
	/*
	//test
	i=0;
	text[i++] = IMM;
	text[i++] = 10;
	text[i++] = PUSH;
	text[i++] = IMM;
	text[i++] = 20;
	text[i++] = ADD;
	text[i++] = PUSH;
	text[i++] = EXIT;
	pc = text;
	*/
	//////////////////////////////////////////////////////////////////
	//keyword init
	src = "char else enum if int return sizeof while "
		  "open read close printf malloc memset memcmp exit void main";
	//add keywords to symbol table
	i = Char;
	while(i <= While){
		next();
		current_id[Token] = i++;
	}
	//add library to symbol table
	i = OPEN;
	while(i <= EXIT){
		next();
		current_id[Class] = Sys;
		current_id[Type] = INT;
		current_id[Value] = i++;
	}
	next();
	current_id[Token] = Char; 		// handle void type
	next();
	idmain = current_id;			// keep track of main

	//////////////////////////////////////////////////////////////////
//	program();
	return eval();
}
