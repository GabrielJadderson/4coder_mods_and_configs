
#include "../4coder_token.h"
#include "../4coder_base_types.h"
#include "../4coder_types.h"
#include "4coder_gabriel_c_ast.h"

//#define HashPrime 317
#define HashPrime 13331
typedef struct SYMBOL
{
    u64 NameSize;
    char* name;
    void* value;
    SYMBOL* next;
} SYMBOL;

typedef struct SymbolTable
{
    SYMBOL* table[HashPrime];
    struct SymbolTable* next;
} SymbolTable;

u64 GabrielHashString(char* str)
{
    u64 length = (u64)(strlen(str));
    if (!length) return 0;
	u64 value = 0;
    for (u64 i = 0; i < length - 1; i++)
    {
        value += str[i];
        value = value << 1;
    }
    value += str[length - 1];
    return value;
}
SymbolTable* InitSymbolTable()
{
    SymbolTable* st = (SymbolTable*)calloc(1, sizeof(struct SymbolTable));
    return st;
}

SymbolTable* ScopeSymbolTable(SymbolTable* t)
{
    SymbolTable* st = InitSymbolTable();
    st->next = t;
    return st;
}

SYMBOL* PutSymbol(SymbolTable* t, char* name, u64 size, void* value)
{
    if (t)
    {
        int index = GabrielHashString(name) % HashPrime;
        SYMBOL* s = (SYMBOL*)calloc(1, sizeof(struct SYMBOL));
        s->name = (char*)calloc(size + 1, sizeof(char));
        memcpy_s(s->name, size, name, size);
        s->NameSize = size;
        s->value = value;
        SYMBOL* temp = t->table[index];
        if (!temp)
        {
            t->table[index] = s;
            return s;
        }
        while (temp)
        {
            if (strcmp(temp->name, name) == 0)
            {
                temp->value = value;
                return temp;
            }
            if (!temp->next)
            {
                temp->next = s;
                return s;
            }
            temp = temp->next;
        }
    }
    return 0;
}

SYMBOL* GetSymbol(SymbolTable* t, char* name)
{
    if (t)
    {
        int index = GabrielHashString(name) % HashPrime;
        SYMBOL* s = t->table[index];
        SymbolTable* hashtable = t;
        while (hashtable != NULL)
        {
            s = hashtable->table[index];
            while (s != NULL)
            {
                if (strcmp(s->name, name) == 0)
                {
                    return s;
                }
                s = s->next;
            }
            hashtable = hashtable->next;
        }
    }
    return NULL;
}








//~













//~


















static SymbolTable* GlobalSymbolTable = NULL;
static b32 GabrielParserInitialized = false;


static FColor FUNCTION_HIGHLIGHT_COLOR = fcolor_argb(0.686f, 0.929f, 0.215f, 1.0f);

static FColor STRUCT_HIGHLIGHT_COLOR = fcolor_argb(0, 0.694f, 1.0f, 1.0f);
static FColor UNION_HIGHLIGHT_COLOR = fcolor_argb(0, 0.694f, 1.0f, 1.0f);
static FColor ENUM_HIGHLIGHT_COLOR = fcolor_argb(0, 0.694f, 1.0f, 1.0f);
//static FColor FUNCTION_PARAM_HIGHLIGHT_COLOR = fcolor_argb(0.2f, 1.0f, 0.925f, 1.0f);
static FColor VARIABLE_HIGHLIGHT_COLOR = fcolor_argb(0.2f, 1.0f, 0.925f, 1.0f);
//static FColor VARIABLE_HIGHLIGHT_COLOR = fcolor_argb(0.976f, 1.0f, 0.2f, 1.0f);
static FColor FUNCTION_PARAM_HIGHLIGHT_COLOR = fcolor_argb(0.976f, 1.0f, 0.2f, 1.0f);
static FColor MEMBER_VARIABLE_HIGHTLIGHT_COLOR = fcolor_argb(1.0f, 0.18f, 1.0f, 1.0f);
static FColor MACRO_VARIABLE_HIGHTLIGHT_COLOR = fcolor_argb(0.807f, 0.305f, 0.850f, 1.0f);

static FColor ENUM_MEMBER_VARIABLE_HIGHTLIGHT_COLOR = fcolor_argb(0.765f, 0.847f, 0.596f, 1.0f);
static FColor STRUCT_MEMBER_VARIABLE_HIGHTLIGHT_COLOR = fcolor_argb(0.549f, 1.0f, 0.596f, 1.0f);
static FColor UNION_MEMBER_VARIABLE_HIGHTLIGHT_COLOR = fcolor_argb(0.549f, 1.0f, 0.596f, 1.0f);

//holds allocated strings.
struct parser_string_buffer 
{
    char* Name;
    u64 Length;
};

struct parser_type 
{
    //void, identifier, .... 
};

struct gabriel_parser_context 
{
    i64 StartIndex;
    i64 EndIndex;
    
    b32 IsVariable;
    
    b32 IsFunction;
    b32 IsFunctionParameter;
    
    b32 IsUnion;
    b32 IsStruct;
    b32 IsEnum;
    
    b32 IsReferenceToSomeRecord;
    
    b32 IsMemberVariable;

    b32 IsMacro;

    b32 IsTrueType;

    b32 IsStructMember;
    b32 IsEnumMember;
    b32 IsUnionMember;

    b32 IsFunctionLocalVariable;
    
    //the name of the identifier whom this context belongs to.
    parser_string_buffer* IdentifierName;
    
    parser_type* Type;
};

struct gabriel_parser_state
{
    u64 NestCounter; //DO NOT RESET

    b32 IsFunction;
    b32 IsInsideFunction;
	
    b32 IsInsideStruct;
    b32 IsInsideEnum;
    b32 IsInsideUnion;
    b32 IsInsideDeclaration; // = { 0, 0, 0};

    //resetables
    //
    //storage class specifier
    b32 IsTypedef;
    b32 IsExtern;
    b32 IsStatic;

    //type specifier
    b32 HasTypeSpecifier;
    b32 IsStruct;
    b32 IsEnum;
    b32 IsUnion;
	
    Token* PreviousIdentifier;
};

static gabriel_parser_state global_gabriel_parser_state = { 0 };

static void
gabriel_parser_derive_truetype_context_from_prescope(gabriel_parser_context* Context, Token_Array* Tokens, i64 Index)
{
    //given the index of a open scope { looks back and determines which type we are.
	
    //keep going back until we have some context.
    i64 LookBackIndex = Index;
    while ((LookBackIndex - 1) > 0)
    {
        Token LookBackToken = Tokens->tokens[--LookBackIndex];
        if (LookBackToken.kind == TokenBaseKind_EOF) { break; }
    	else if (LookBackToken.kind == TokenBaseKind_Whitespace) { continue; }
    	else if (LookBackToken.kind == TokenBaseKind_LexError) { break; }
    	else if (LookBackToken.kind == TokenBaseKind_LiteralInteger) { continue; }
        else if (LookBackToken.kind == TokenBaseKind_LiteralFloat) { continue; }
        else if (LookBackToken.kind == TokenBaseKind_LiteralString) { continue; }

        else if (LookBackToken.kind == TokenBaseKind_ScopeOpen)
        {
            break; 
        }
    	
        else if (LookBackToken.kind == TokenBaseKind_ParentheticalClose)
        {
            Context->IsVariable;
            return;
        }
    	else if (LookBackToken.kind == TokenBaseKind_ParentheticalOpen)
        {
            if (LookBackToken.sub_kind == TokenCppKind_ParenOp)
            {
                break;
            }
        }
        else if (LookBackToken.kind == TokenBaseKind_Comment) { continue; }
    	else if (LookBackToken.kind == TokenBaseKind_ScopeClose)
    	{
            break; //failed?
    	}
    	else if (LookBackToken.kind == TokenBaseKind_StatementClose)
        {
            continue;
        }
    	else if (LookBackToken.kind == TokenBaseKind_Preprocessor) {}
    	else if (LookBackToken.kind == TokenBaseKind_Operator)
        {
            break; //FAIL, We've reached too far too the left to make any sense.
        }
        else if (LookBackToken.kind == TokenBaseKind_Keyword)
        {
            if (LookBackToken.sub_kind == TokenCppKind_Struct)
            {
                Context->IsTrueType = true;
                Context->IsStruct = true;
                return;
            }
        	else if (LookBackToken.sub_kind == TokenCppKind_Enum)
			{
                Context->IsTrueType = true;
                Context->IsEnum = true;
                return;
			}
            else if (LookBackToken.sub_kind == TokenCppKind_Union)
            {
                Context->IsTrueType = true;
                Context->IsUnion = true;
                return;
            }
            break;
    	}
    	else if (LookBackToken.kind == TokenBaseKind_Identifier)
    	{
            continue;
    		//TODO(Gabriel): try to lookup variable.
    	}
    }
}

static void
gabriel_parser_crawl_back_to_nearest_truetype(gabriel_parser_context* Context, Token_Array* Tokens, i64 Index)
{
    //given a identifier index in the middle of some comma's 
	//NOTE(Gabriel): figures out if we're inside a function parameter list or enum declaration list or struct

	
	
    //keep going back until we have some context.
    i64 LookBackIndex = Index;
    while ((LookBackIndex - 1) > 0)
    {
        Token LookBackToken = Tokens->tokens[--LookBackIndex];
        if (LookBackToken.kind == TokenBaseKind_EOF) { break; } 
        else if (LookBackToken.kind == TokenBaseKind_Whitespace) { continue; }
    	else if (LookBackToken.kind == TokenBaseKind_LexError) { break; }
    	else if (LookBackToken.kind == TokenBaseKind_LiteralInteger) { break; }
    	else if (LookBackToken.kind == TokenBaseKind_LiteralFloat) { break; }
    	else if (LookBackToken.kind == TokenBaseKind_LiteralString) { break; }

        else if (LookBackToken.kind == TokenBaseKind_ScopeOpen)
        {
            gabriel_parser_derive_truetype_context_from_prescope(Context, Tokens, LookBackIndex);
            if (Context->IsTrueType)
            {
                if (Context->IsStruct)
                {
	                Context->IsTrueType = false;
                	Context->IsStruct = false;
                    Context->IsStructMember = true;
                    return;
                }
            	else if (Context->IsEnum)
            	{
                    Context->IsTrueType = false;
                    Context->IsEnum = false;
                    Context->IsEnumMember= true;
                    return;
            	}
                else if (Context->IsUnion)
                {
                    Context->IsTrueType = false;
                    Context->IsUnion = false;
                    Context->IsUnionMember= true;
                    return;
                }
            }
        
        }
    	else if (LookBackToken.kind == TokenBaseKind_ParentheticalClose) { break; }
    	else if (LookBackToken.kind == TokenBaseKind_ParentheticalOpen)
        {
            if (LookBackToken.sub_kind == TokenCppKind_ParenOp)
            {
                Context->IsFunctionParameter = true;
                return;
            }
            break;
        }
        else if (LookBackToken.kind == TokenBaseKind_Comment) { continue;  }
        else if (LookBackToken.kind == TokenBaseKind_ScopeClose) { break; }
    	else if (LookBackToken.kind == TokenBaseKind_StatementClose)
        {
            if (LookBackToken.sub_kind == TokenCppKind_Comma)
            {
                continue;
            } else
            {
                //Context->IsMemberVariable = true;
                //return;
                continue;
            }
            break;
        }
        else if (LookBackToken.kind == TokenBaseKind_Preprocessor) { continue;  }
    	else if (LookBackToken.kind == TokenBaseKind_Operator)
    	{
            continue;
    	}
    	else if (LookBackToken.kind == TokenBaseKind_Keyword)
    	{
            continue;
    	}
    	else if (LookBackToken.kind == TokenBaseKind_Identifier) { continue; }
    }
}

static String_Const_u8
gabriel_parser_extract_token_string(Application_Links* app, Buffer_ID buffer, Token* token)
{
    Range_i64 token_range = { token->pos, token->pos + token->size, };
    u8 token_buffer[256] = { 0 };
    buffer_read_range(app, buffer, token_range, token_buffer);
    String_Const_u8 token_string = { token_buffer, (u64)(token_range.end - token_range.start) };
    return token_string;
}

static gabriel_parser_context
gabriel_parser_build_some_context_from_surroundings(Application_Links* app,
                                                    Buffer_ID buffer, 
                                                    Token_Array* Tokens, i64 StartIndex, SymbolTable* ReferenceSymbolTable)
{
    gabriel_parser_context Context = {0};
	

    Range_i64 token_range = { Tokens->tokens[StartIndex].pos, Tokens->tokens[StartIndex].pos + Tokens->tokens[StartIndex].size, };
    u8 token_buffer[256] = { 0 };
    buffer_read_range(app, buffer, token_range, token_buffer);
    String_Const_u8 token_string = { token_buffer, (u64)(token_range.end - token_range.start) };
    //String_Const_u8 token_string = gabriel_parser_extract_token_string(app, buffer, &Tokens->tokens[StartIndex]);

    ////first lookup.
    SYMBOL* symbol = NULL;
	if ((symbol = GetSymbol(ReferenceSymbolTable, (char*)token_string.str)) != 0)
	{
        //gabriel_parser_context Ctx = *(gabriel_parser_context*)symbol->value;
        //if (Ctx.IsTrueType)
        //{
        //    return Ctx;
        //}
		
        return *(gabriel_parser_context*)symbol->value;
		//TODO(Gabriel):  Fix all of this bad stuff.
	}

	if (global_gabriel_parser_state.IsInsideEnum)
	{
        Context.IsEnumMember = true;
        Context.IsStructMember = false;
        Context.IsUnionMember = false;
        goto FuncEnd;
    } 
    /*
    else if (global_gabriel_parser_state.IsInsideStruct)
    {
        Context.IsStructMember = true;
        Context.IsEnumMember = false;
        Context.IsUnionMember = false;
        goto FuncEnd;
    } else if (global_gabriel_parser_state.IsInsideUnion) 
    {
        Context.IsStructMember = false;
        Context.IsEnumMember = false;
        Context.IsUnionMember = true;
        goto FuncEnd;
    } 
    */
    else if (global_gabriel_parser_state.IsEnum && 
        global_gabriel_parser_state.NestCounter == 0 && 
        !global_gabriel_parser_state.PreviousIdentifier &&
        global_gabriel_parser_state.HasTypeSpecifier)
    {
        Context.IsEnum = true;
        goto FuncEnd;
    } else if (global_gabriel_parser_state.IsStruct &&
        global_gabriel_parser_state.NestCounter == 0 &&
        !global_gabriel_parser_state.PreviousIdentifier &&
        global_gabriel_parser_state.HasTypeSpecifier)
    {
        Context.IsStruct = true;
        goto FuncEnd;
    } else if (global_gabriel_parser_state.IsUnion &&
        global_gabriel_parser_state.NestCounter == 0 &&
        !global_gabriel_parser_state.PreviousIdentifier &&
        global_gabriel_parser_state.HasTypeSpecifier)
    {
        Context.IsUnion = true;
        goto FuncEnd;
    }


    b32 AloneInSentence = false;
    b32 FirstParameterInFunction = false;
    b32 CommaToTheLeft = false;
    b32 IdentifierToTheLeft = false;
    b32 BracketToTheLeft = false;
    b32 FoundReturnOrGotoStatmentToTheLeft = false;
    b32 FoundStarToTheLeft = false;

    i64 LookBackIndex = StartIndex;
    while((LookBackIndex - 1) > 0)
    {
        Token token = Tokens->tokens[--LookBackIndex];
        if (token.kind == TokenBaseKind_EOF) { break; }
        else if (token.kind == TokenBaseKind_Whitespace) { continue; }
        else if (token.kind == TokenBaseKind_LexError) { break; }
        else if (token.kind == TokenBaseKind_LiteralInteger) { break; }
        else if (token.kind == TokenBaseKind_LiteralFloat) { break; }
        else if (token.kind == TokenBaseKind_LiteralString) { break; }
        else if (token.kind == TokenBaseKind_ScopeOpen)
        {
        	if (global_gabriel_parser_state.NestCounter > 0)
        	{
                gabriel_parser_derive_truetype_context_from_prescope(&Context, Tokens, LookBackIndex);
                if (Context.IsTrueType)
                {
                    if (Context.IsStruct)
                    {
                        Context.IsTrueType = false;
                        Context.IsStruct = false;
                        Context.IsStructMember = true;
                        goto FuncEnd;
                    } else if (Context.IsEnum)
                    {
                        Context.IsTrueType = false;
                        Context.IsEnum = false;
                        Context.IsEnumMember = true;
                        goto FuncEnd;
                    } else if (Context.IsUnion)
                    {
                        Context.IsTrueType = false;
                        Context.IsUnion = false;
                        Context.IsUnionMember = true;
                        goto FuncEnd;
                    }
                } else  
                {
                    AloneInSentence = true;
                }
        	}
        	
            /*
            gabriel_parser_derive_truetype_context_from_prescope(&Context, Tokens, LookBackIndex);
            if (Context.IsTrueType)
            {
                if (Context.IsStruct)
                {
                    Context.IsTrueType = false;
                    Context.IsStruct = false;
                    Context.IsStructMember = true;
                    goto FuncEnd;
                } else if (Context.IsEnum)
                {
                    Context.IsTrueType = false;
                    Context.IsEnum = false;
                    Context.IsEnumMember = true;
                    goto FuncEnd;
                } else if (Context.IsUnion)
                {
                    Context.IsTrueType = false;
                    Context.IsUnion = false;
                    Context.IsUnionMember = true;
                    goto FuncEnd;
                }
            }
        	*/
        	
        }

        else if (token.kind == TokenBaseKind_ParentheticalClose) { break; }
        else if (token.kind == TokenBaseKind_ParentheticalOpen)
        {
        	if (token.sub_kind == TokenCppKind_ParenOp)
        	{
                AloneInSentence = true;
                FirstParameterInFunction = true;
            } else if (token.sub_kind == TokenCppKind_BrackOp) 
            {
                BracketToTheLeft = true;
            }
	        break;
        }
    	
        else if (token.kind == TokenBaseKind_Comment) { AloneInSentence = true; }
        else if (token.kind == TokenBaseKind_ScopeClose) { AloneInSentence = true; }
        else if (token.kind == TokenBaseKind_StatementClose)
        {
        	if (token.sub_kind == TokenCppKind_Comma)
        	{
                gabriel_parser_crawl_back_to_nearest_truetype(&Context, Tokens, LookBackIndex);
        		if (Context.IsTrueType || Context.IsFunctionParameter || Context.IsEnumMember || Context.IsStructMember
                    || Context.IsUnionMember)
        		{
                    goto FuncEnd;
        		}
        		
                CommaToTheLeft = true;
                break;
        	}
	        AloneInSentence = true;
        }

        else if (token.kind == TokenBaseKind_Preprocessor)
        {
            Context.IsMacro = true;
            goto FuncEnd;
        }
    	
        else if (token.kind == TokenBaseKind_Operator)
        {
            if (token.sub_kind == TokenCppKind_Comma)
            {
                Context.IsFunctionParameter = true;
                CommaToTheLeft = true;
                break;
            } else if (token.sub_kind == TokenCppKind_Dot)
            {
                Context.IsMemberVariable = true;
                Context.EndIndex = LookBackIndex;
                goto FuncEnd;
            } else if (token.sub_kind == TokenCppKind_Star)
            {
                FoundStarToTheLeft = true;
                break;
            } else if (token.sub_kind == TokenCppKind_EqEq)
            {
                if (AloneInSentence) 
                {
                    Context.IsVariable = true;
                    goto FuncEnd;
                } 
            } else
            {
                Context.IsVariable = true;
                goto FuncEnd;
            }
        } 
        else if (token.kind == TokenBaseKind_Keyword)
        {
            if (token.sub_kind == TokenCppKind_Struct)
            {
                Context.IsTrueType = true;
                Context.IsStruct = true;
                goto FuncEnd;
            } else if (token.sub_kind == TokenCppKind_Enum)
            {
                Context.IsTrueType = true;
                Context.IsEnum = true;
                goto FuncEnd;
            } else if (token.sub_kind == TokenCppKind_Union)
            {
                Context.IsTrueType = true;
                Context.IsUnion = true;
                goto FuncEnd;
            }

            //| VOID | CHAR | SHORT | INT | LONG | FLOAT | DOUBLE | SIGNED | UNSIGNED |
        	else if (token.sub_kind == TokenCppKind_Void || 
                token.sub_kind == TokenCppKind_Char || 
                token.sub_kind == TokenCppKind_Short ||
                token.sub_kind == TokenCppKind_Int || 
                token.sub_kind == TokenCppKind_Long ||
                token.sub_kind == TokenCppKind_Float|| 
                token.sub_kind == TokenCppKind_Double || 
                token.sub_kind == TokenCppKind_Signed || 
                token.sub_kind == TokenCppKind_Unsigned)
            {
                Context.IsTrueType = true;
                Context.IsVariable = true;
                goto FuncEnd;
            } else if (token.sub_kind == TokenCppKind_Return || token.sub_kind == TokenCppKind_Goto) 
            {
                FoundReturnOrGotoStatmentToTheLeft = true;
                break;
            }

        	if (FoundStarToTheLeft)
        	{
                Context.IsVariable = true;
                goto FuncEnd;
        	}

        	
            break;
        } 
        else if (token.kind == TokenBaseKind_Identifier)
        {
            if (AloneInSentence)
            {
                Context.IsReferenceToSomeRecord = true;
                goto FuncEnd;
            }

            IdentifierToTheLeft = true;
            break;
        	

        } 

    }



	//RIGHT

    i64 RightEndSpan = StartIndex;
    while ((RightEndSpan + 1) < Tokens->count)
    {
        Token token = Tokens->tokens[++RightEndSpan];
    	
    	if (token.kind == TokenBaseKind_EOF) { break; } else if (token.kind == TokenBaseKind_Whitespace) { continue; } else if (token.kind == TokenBaseKind_LexError) { break; } else if (token.kind == TokenBaseKind_Comment) { break; } else if (token.kind == TokenBaseKind_Preprocessor) { break; } else if (token.kind == TokenBaseKind_LiteralInteger) { break; } else if (token.kind == TokenBaseKind_LiteralFloat) { break; } else if (token.kind == TokenBaseKind_LiteralString) { break; } else if (token.kind == TokenBaseKind_ScopeOpen) { break; } else if (token.kind == TokenBaseKind_ScopeClose) { break; }

    	else if (token.kind == TokenBaseKind_ParentheticalClose)
        {
            if (token.sub_kind == TokenCppKind_BrackCl) 
            {
                if (BracketToTheLeft) 
                {
                    Context.IsVariable = true;
                    goto FuncEnd;
                }
            }
            if (CommaToTheLeft)
            {
                Context.IsVariable = true;
                goto FuncEnd;
            }
            if (FirstParameterInFunction)
            {
                Context.IsVariable = true;
                goto FuncEnd;
            }
            Context.IsFunctionParameter = true;
            goto FuncEnd;
        }
    	else if (token.kind == TokenBaseKind_ParentheticalOpen)
    	{
		  	if (token.sub_kind == TokenCppKind_ParenOp)
            {
                Context.IsFunction = true;
                global_gabriel_parser_state.IsInsideFunction = true;
                goto FuncEnd;
            } else if (token.sub_kind == TokenCppKind_BrackOp)  
            {
                if (AloneInSentence)
                {
                    Context.IsVariable = true;
                    goto FuncEnd;
                }
            }
    		
            break;
    	}

    	else if (token.kind == TokenBaseKind_StatementClose) 
        {
            if (token.sub_kind == TokenCppKind_Semicolon) 
            {
                if (FoundReturnOrGotoStatmentToTheLeft || FoundStarToTheLeft) 
                {
                    Context.IsVariable = true;
                    goto FuncEnd;
                }
            }
            gabriel_parser_crawl_back_to_nearest_truetype(&Context, Tokens, RightEndSpan);
            if (Context.IsTrueType)
            {
                if (Context.IsStruct)
                {
                    Context.IsTrueType = false;
                    Context.IsStruct = false;
                    Context.IsStructMember = true;
                    goto FuncEnd;
                } else if (Context.IsEnum)
                {
                    Context.IsTrueType = false;
                    Context.IsEnum = false;
                    Context.IsEnumMember = true;
                    goto FuncEnd;
                } else if (Context.IsUnion)
                {
                    Context.IsTrueType = false;
                    Context.IsUnion = false;
                    Context.IsUnionMember = true;
                    goto FuncEnd;
                }
            } else if (Context.IsFunctionParameter)
            {
                goto FuncEnd;
            } else
            {
                break; //NOTE(Gabriel): maybe we should make this a variable in here?
            }
    		//if (token.sub_kind == TokenCppKind_Comma)
    		//{
            //    Context.IsVariable = true;
            //    goto FuncEnd;
    		//}
    		//
            //Context.IsVariable = true;
            //goto FuncEnd;
        }


        else if (token.kind == TokenBaseKind_Operator)
        {
        	if (IdentifierToTheLeft || AloneInSentence || BracketToTheLeft || FoundStarToTheLeft)
        	{
                Context.IsVariable = true;
                goto FuncEnd;
        	}
        	
            if (token.sub_kind == TokenCppKind_Comma)
            {
            	if (CommaToTheLeft)
            	{
                    Context.IsVariable = true;
                    Context.IsFunctionParameter = false;
                    goto FuncEnd;
            	}
				Context.IsFunctionParameter = true;
                goto FuncEnd;
            } else if (token.sub_kind == TokenCppKind_Star)
            {
            	
                Context.IsReferenceToSomeRecord = true;
                goto FuncEnd;

            	
            } else if (token.sub_kind == TokenCppKind_Arrow)
            {
                //TODDO(Gabriel): look up that record and assign color?
                Context.IsVariable = true;
                Context.EndIndex = RightEndSpan;
                goto FuncEnd;
            } else if (token.sub_kind == TokenCppKind_ParenOp)
            {
                //TODDO(Gabriel): look up that record and assign color?
                //Context.IsFunction = true;
                //goto FuncEnd;
                break;
            }
        } else if (token.kind == TokenBaseKind_Keyword)
        {
            break;
        } else if (token.kind == TokenBaseKind_Identifier)
        {
            
            
            i64 LookAheadIndex = RightEndSpan;
            while ((LookAheadIndex + 1) < Tokens->count)
            {
                Token LookAheadToken = Tokens->tokens[++LookAheadIndex];
                if (LookAheadToken.kind == TokenBaseKind_EOF) { break; }
            	else if (LookAheadToken.kind == TokenBaseKind_Whitespace) { continue; }
            	else if (LookAheadToken.kind == TokenBaseKind_LexError) { break; }
            	else if (LookAheadToken.kind == TokenBaseKind_Comment) { break; }
            	else if (LookAheadToken.kind == TokenBaseKind_Preprocessor) { break; }
            	else if (LookAheadToken.kind == TokenBaseKind_LiteralInteger) { break; }
            	else if (LookAheadToken.kind == TokenBaseKind_LiteralFloat) { break; }
            	else if (LookAheadToken.kind == TokenBaseKind_LiteralString) { break; }
            	else if (LookAheadToken.kind == TokenBaseKind_ScopeOpen) { break; }
            	else if (LookAheadToken.kind == TokenBaseKind_ScopeClose) { break; }

            	
				else if (LookAheadToken.kind == TokenBaseKind_ParentheticalOpen)
                {
                    Context.IsFunction = true;
                    global_gabriel_parser_state.IsInsideFunction = true;
                    goto FuncEnd;
                } else if (LookAheadToken.kind == TokenBaseKind_Operator)
                {

                    if (LookAheadToken.sub_kind == TokenCppKind_Comma)
                    {
                        Context.IsFunctionParameter = true;
                        Context.EndIndex = LookAheadIndex;
                        goto FuncEnd;
                    } else if (LookAheadToken.sub_kind == TokenCppKind_ParenOp)
                    {
                        Context.IsReferenceToSomeRecord = true;
                        Context.EndIndex = LookAheadIndex;
                        goto FuncEnd;
                    } else
                    {
                        Context.IsReferenceToSomeRecord = true;
                        Context.EndIndex = LookAheadIndex;
                        goto FuncEnd;
                    }

                	

                }
            }


            Context.IsReferenceToSomeRecord = true;

            break;


        }

    }



FuncEnd:


	if (Context.IsMacro || Context.IsFunction || Context.IsUnion || Context.IsStruct || Context.IsEnum || Context.IsEnumMember)
	{
        gabriel_parser_context* PtrContext = (gabriel_parser_context*)calloc(1, sizeof(gabriel_parser_context));
        *PtrContext = Context;
        PutSymbol(ReferenceSymbolTable, (char*)token_string.str, token_string.size, PtrContext);
	}
    
    return Context;
}



function void
gabriel_paint_from_context(Application_Links* app, Text_Layout_ID text_layout_id, gabriel_parser_context* Context, Token* token)
{
    if (Context->IsStruct || Context->IsEnum || Context->IsUnion || Context->IsFunctionParameter || Context->IsVariable ||
    Context->IsReferenceToSomeRecord || Context->IsMemberVariable || Context->IsMacro || Context->IsStructMember ||
    Context->IsEnumMember || Context->IsUnionMember)
    {
        Range_i64 range = { 0 };
        range.start = token->pos;
        range.end = token->pos + token->size;
        // NOTE(Skytrias): use your own colorscheme her via fcolor_id(defcolor_*)
        // NOTE(Skytrias): or set the color you'd like to use globally like i do
        if (Context->IsStruct)
        {
            paint_text_color(app, text_layout_id, range, fcolor_resolve(STRUCT_HIGHLIGHT_COLOR));
        } else if (Context->IsEnum)
        {
            paint_text_color(app, text_layout_id, range, fcolor_resolve(ENUM_HIGHLIGHT_COLOR));
        }

        else if (Context->IsUnion)
        {
            paint_text_color(app, text_layout_id, range, fcolor_resolve(UNION_HIGHLIGHT_COLOR));
        } else if (Context->IsFunctionParameter)
        {
            paint_text_color(app, text_layout_id, range, fcolor_resolve(FUNCTION_PARAM_HIGHLIGHT_COLOR));
        } else if (Context->IsVariable)
        {
            paint_text_color(app, text_layout_id, range, fcolor_resolve(VARIABLE_HIGHLIGHT_COLOR));
        } else if (Context->IsReferenceToSomeRecord)
        {
            paint_text_color(app, text_layout_id, range, fcolor_resolve(STRUCT_HIGHLIGHT_COLOR));
        } else if (Context->IsMemberVariable)
        {
            paint_text_color(app, text_layout_id, range, fcolor_resolve(MEMBER_VARIABLE_HIGHTLIGHT_COLOR));
        } else if (Context->IsMacro)
        {
            paint_text_color(app, text_layout_id, range, fcolor_resolve(MACRO_VARIABLE_HIGHTLIGHT_COLOR));
        } else if (Context->IsEnumMember)
        {
            paint_text_color(app, text_layout_id, range, fcolor_resolve(ENUM_MEMBER_VARIABLE_HIGHTLIGHT_COLOR));
        } else if (Context->IsStructMember)
        {
            paint_text_color(app, text_layout_id, range, fcolor_resolve(STRUCT_MEMBER_VARIABLE_HIGHTLIGHT_COLOR));
        } else if (Context->IsUnionMember)
        {
            paint_text_color(app, text_layout_id, range, fcolor_resolve(UNION_MEMBER_VARIABLE_HIGHTLIGHT_COLOR));
        }
    }
}




function void
gabriel_parse_tokens(Application_Links* app, Buffer_ID buffer, Text_Layout_ID text_layout_id)
{
    if (!GabrielParserInitialized)
    {
        GlobalSymbolTable = InitSymbolTable();
        global_gabriel_parser_state = { 0 };
        GabrielParserInitialized = true;
    }

    Token PreviousToken = {0};

    Token_Array array = get_token_array_from_buffer(app, buffer);
    if (array.tokens != 0)
    {
        Range_i64 visible_range = text_layout_get_visible_range(app, text_layout_id);
        i64 first_index = token_index_from_pos(&array, visible_range.first);
        Token_Iterator_Array it = token_iterator_index(0, &array, first_index);
        for (;;)
        {
            Token* token = token_it_read(&it);
            if (token->pos >= visible_range.one_past_last)
            {
                break;
            }
            
        	if (token->kind == TokenBaseKind_Whitespace) { }
            else if (token->kind == TokenBaseKind_EOF) { break; }
        	else if (token->kind == TokenBaseKind_LexError) { break; }
        	else if (token->kind == TokenBaseKind_Comment) {  }
            else if (token->kind == TokenBaseKind_Preprocessor) {  }
        	else if (token->kind == TokenBaseKind_LiteralInteger) {  }
        	else if (token->kind == TokenBaseKind_LiteralFloat) {  }
        	else if (token->kind == TokenBaseKind_LiteralString) {  }
        	
        	else if (token->kind == TokenBaseKind_ScopeOpen)
        	{
        		if (token->sub_kind == TokenCppKind_BraceOp)
        		{
                    global_gabriel_parser_state.NestCounter++;

        			if (global_gabriel_parser_state.HasTypeSpecifier)
        			{
						if (global_gabriel_parser_state.IsStruct)
                        {
							global_gabriel_parser_state.IsInsideStruct = true;
                        }
						else if (global_gabriel_parser_state.IsEnum)
                        {
							global_gabriel_parser_state.IsInsideEnum = true;
                        }
                        else if (global_gabriel_parser_state.IsUnion)
                        {
							global_gabriel_parser_state.IsInsideUnion = true;
                        }
        			} else
        			{
        				if (global_gabriel_parser_state.IsFunction)
        				{
                            global_gabriel_parser_state.IsInsideFunction = true;
        				}
        			}


        			
        		} else
        		{
        		}
        	}
        	else if (token->kind == TokenBaseKind_ScopeClose) 
            {
                if (token->sub_kind == TokenCppKind_BraceCl)
                {
                	if (global_gabriel_parser_state.NestCounter == 0)
                    {
	                    
                    } else if ((global_gabriel_parser_state.NestCounter - 1) == 0)
                    {
                        global_gabriel_parser_state = { 0 };
                    } else
                    {
                        global_gabriel_parser_state.NestCounter--;

                        if (global_gabriel_parser_state.IsInsideFunction) 
                        {
                            global_gabriel_parser_state.IsEnum = false;
                            global_gabriel_parser_state.IsStruct = false;
                            global_gabriel_parser_state.IsUnion = false;
                            global_gabriel_parser_state.HasTypeSpecifier = false;
                            global_gabriel_parser_state.IsTypedef = false;
                            global_gabriel_parser_state.IsExtern = false;
                            global_gabriel_parser_state.IsStatic = false;
                            global_gabriel_parser_state.IsInsideDeclaration = false;
                            global_gabriel_parser_state.IsInsideEnum = false;
                            global_gabriel_parser_state.IsInsideStruct = false;
                            global_gabriel_parser_state.IsInsideUnion = false;
                        }
                    }
                	
                }
            }


            else if (token->kind == TokenBaseKind_ParentheticalOpen)
            {
            }
        	else if (token->kind == TokenBaseKind_Operator)
            {

            	if (token->sub_kind == TokenCppKind_Comma)
                {
                }
            	else if (token->sub_kind == TokenCppKind_ParenOp)
                {
                }
            	else
                {
                }

            }
			else if (token->kind == TokenBaseKind_Keyword)
			{
				
                //storage_class_specifier
				if (token->sub_kind == TokenCppKind_Typedef)
				{
					//Typedef
                    global_gabriel_parser_state.IsTypedef = true;
				}
				else if (token->sub_kind == TokenCppKind_Extern)
				{
                    global_gabriel_parser_state.IsExtern = true;
				}
                else if (token->sub_kind == TokenCppKind_Static)
                {
                    global_gabriel_parser_state.IsStatic = true;
                }


                //type_specifier | VOID | CHAR | SHORT | INT | LONG | FLOAT | DOUBLE | SIGNED | UNSIGNED | BOOL
                else if (token->sub_kind == TokenCppKind_Void || 
                    token->sub_kind == TokenCppKind_Char || 
                    token->sub_kind == TokenCppKind_Short || 
                    token->sub_kind == TokenCppKind_Int || 
                    token->sub_kind == TokenCppKind_Long ||
                    token->sub_kind == TokenCppKind_Float ||
                    token->sub_kind == TokenCppKind_Double ||
                    token->sub_kind == TokenCppKind_Signed || 
                    token->sub_kind == TokenCppKind_Unsigned ||
                    token->sub_kind == TokenCppKind_Bool)
                {
                    global_gabriel_parser_state.HasTypeSpecifier = true;
                }

                //type_specifier | Struct | Enum | Union | 
                else if (token->sub_kind == TokenCppKind_Struct)
                {
                    global_gabriel_parser_state.HasTypeSpecifier = true;
                    global_gabriel_parser_state.IsStruct = true;
                }
				else if (token->sub_kind == TokenCppKind_Enum)
				{
                    global_gabriel_parser_state.HasTypeSpecifier = true;
                    global_gabriel_parser_state.IsEnum = true;
				}
                else if (token->sub_kind == TokenCppKind_Union)
                {
                    global_gabriel_parser_state.HasTypeSpecifier = true;
                    global_gabriel_parser_state.IsUnion = true;
                }
			}

	        else if (token->kind == TokenBaseKind_StatementClose)
			{
	        	if (token->sub_kind == TokenCppKind_Comma)
	        	{
	        		
	        	} else
	        	{
	        		if (global_gabriel_parser_state.NestCounter == 0)
	        		{
                        global_gabriel_parser_state = { 0 };
                    } else if (global_gabriel_parser_state.NestCounter >= 1)
                    {

                    }
	        	}
	        }

        	
            else if (token->kind == TokenBaseKind_Identifier)
            {
                i64 IdentifierIndex = token_it_index(&it);
                gabriel_parser_context Context = gabriel_parser_build_some_context_from_surroundings(app, buffer, &array, 
                    IdentifierIndex, GlobalSymbolTable);
            	
                global_gabriel_parser_state.PreviousIdentifier = token;
                gabriel_paint_from_context(app, text_layout_id, &Context, token);

            }

            if (!token_it_inc_all(&it))
            {
                break;
            } else
            {
                PreviousToken = *token;
            }
        }
    }
}

//Hook this between "Color Parens" and "Line highlight" in RenderBuffer
//
function void
gabriel_start(Application_Links *app, Buffer_ID buffer, Text_Layout_ID text_layout_id) {

    gabriel_parse_tokens(app, buffer, text_layout_id);
	/*
	if (!GabrielParserInitialized)
	{
        GlobalSymbolTable = InitSymbolTable();
        GabrielParserInitialized = true;
	}


	Token_Array array = get_token_array_from_buffer(app, buffer);
    if (array.tokens != 0){
        Range_i64 visible_range = text_layout_get_visible_range(app, text_layout_id);
        i64 first_index = token_index_from_pos(&array, visible_range.first);
        Token_Iterator_Array it = token_iterator_index(0, &array, first_index);
        for (;;){
            Token *token = token_it_read(&it);
            if (token->pos >= visible_range.one_past_last){
                break;
            } 
            
            gabriel_parser_context* Context;
            
            // search for default text, count up the size
            if (token->kind == TokenBaseKind_Identifier) {
                
                i64 NextIndex = token_it_index(&it);
                 Context = gabriel_parser_build_some_context_from_surroundings(app, buffer,
                                                                        &array,
                                                                        NextIndex, GlobalSymbolTable);
                
                

                 
            } 
            

            
            if (!token_it_inc_all(&it)){
                break;
            }
        }
    }
    */
}
