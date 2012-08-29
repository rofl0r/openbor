/*
 * OpenBOR - http://www.LavaLit.com
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in OpenBOR root for details.
 *
 * Copyright (c) 2004 - 2011 OpenBOR Team
 */

#ifndef INTERPRETER_H
#define INTERPRETER_H
#include "depends.h"
#include "StackedSymbolTable.h"
#include "Instruction.h"
#include "Parser.h"
#include "SolidList.h"

typedef s32(*SCRIPTFUNCTION) (ScriptVariant ** varlist, ScriptVariant ** pretvar, int paramCount);

typedef struct Interpreter {
	StackedSymbolTable theSymbolTable;
	List *ptheFunctionList;	//external functionlist, to save some memory
	List theImportList;
	List theInstructionList;
	SolidList *theSolidListOfInstructionList;
	List paramList;
	Stack theDataStack;
	Stack theLabelStack;
	Parser theParser;
	pp_context theContext;

	Instruction **pCurrentInstruction;
	Instruction **pCurrentCall;
	Instruction **pReturnEntry;
	union {			// we have to use the index before solidifying the instruction list
		Instruction **pMainEntry;
		int mainEntryIndex;
	};
	int bHasImmediateCode;

	BOOL bCallCompleted;
	BOOL bMainCompleted;
} Interpreter;

void Interpreter_Init(Interpreter * pinterpreter, LPCSTR name, List * pflist);
void Interpreter_Clear(Interpreter * pinterpreter);
s32 Interpreter_ParseText(Interpreter * pinterpreter, LPSTR scriptText, ULONG startingLineNumber, LPCSTR path);
s32 Interpreter_PutValue(Interpreter * pinterpreter, LPCSTR variable, ScriptVariant * pValue, int refFlag);
s32 Interpreter_GetValue(Interpreter * pinterpreter, LPCSTR variable, ScriptVariant * pValue);
s32 Interpreter_GetValueByRef(Interpreter * pinterpreter, LPCSTR variable, ScriptVariant ** pValue);
s32 Interpreter_EvaluateImmediate(Interpreter * pinterpreter);
s32 Interpreter_EvaluateCall(Interpreter * pinterpreter);
s32 Interpreter_CompileInstructions(Interpreter * pinterpreter);
s32 Interpreter_Call(Interpreter * pinterpreter);
s32 Interpreter_EvalInstruction(Interpreter * pinterpreter);
void Interpreter_Reset(Interpreter * pinterpreter);
void Interpreter_ClearImports(Interpreter * pinterpreter);


#endif
