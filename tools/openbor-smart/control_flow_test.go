package main

import "testing"

func TestParseIfElseAndElseIf(t *testing.T) {
	source := "void test(int health) { if (health > 50) health = 50; else if (health < 0) { health = 0; } else health = 1; }"
	functions, err := parseCSource(source)
	if err != nil { t.Fatalf("parseCSource failed: %v", err) }
	if len(functions) != 1 || len(functions[0].Body.Statements) != 1 { t.Fatalf("unexpected function body: %#v", functions) }
	first, ok := functions[0].Body.Statements[0].(*cIfStatement)
	if !ok { t.Fatalf("statement type = %T, want if", functions[0].Body.Statements[0]) }
	if first.Condition != "health > 50" { t.Errorf("if condition = %q", first.Condition) }
	if _, ok := first.Then.(*cExpressionStatement); !ok { t.Errorf("then branch type = %T", first.Then) }
	second, ok := first.Else.(*cIfStatement)
	if !ok { t.Fatalf("else branch type = %T, want else-if", first.Else) }
	if second.Condition != "health < 0" { t.Errorf("else-if condition = %q", second.Condition) }
	if _, ok := second.Then.(*cBlock); !ok { t.Errorf("else-if body type = %T, want block", second.Then) }
	if _, ok := second.Else.(*cExpressionStatement); !ok { t.Errorf("final else type = %T", second.Else) }
}

func TestParseWhileAndDoWhile(t *testing.T) {
	source := "void test() { while (alive && count < 3) { count++; } do count--; while (count > 0); }"
	functions, err := parseCSource(source)
	if err != nil { t.Fatalf("parseCSource failed: %v", err) }
	statements := functions[0].Body.Statements
	if len(statements) != 2 { t.Fatalf("got %d statements, want 2", len(statements)) }
	whileStmt, ok := statements[0].(*cWhileStatement)
	if !ok { t.Fatalf("first statement type = %T, want while", statements[0]) }
	if whileStmt.Condition != "alive && count < 3" { t.Errorf("while condition = %q", whileStmt.Condition) }
	if _, ok := whileStmt.Body.(*cBlock); !ok { t.Errorf("while body type = %T, want block", whileStmt.Body) }
	doStmt, ok := statements[1].(*cDoWhileStatement)
	if !ok { t.Fatalf("second statement type = %T, want do-while", statements[1]) }
	if doStmt.Condition != "count > 0" { t.Errorf("do-while condition = %q", doStmt.Condition) }
	if _, ok := doStmt.Body.(*cExpressionStatement); !ok { t.Errorf("do-while body type = %T", doStmt.Body) }
}

func TestParseForLoopAndLoopControl(t *testing.T) {
	source := "void test() { for (int i = 0; i < limit; i++) { if (skip(i)) continue; if (done(i)) break; } for (;;); }"
	functions, err := parseCSource(source)
	if err != nil { t.Fatalf("parseCSource failed: %v", err) }
	statements := functions[0].Body.Statements
	if len(statements) != 2 { t.Fatalf("got %d statements, want 2", len(statements)) }
	loop, ok := statements[0].(*cForStatement)
	if !ok { t.Fatalf("first statement type = %T, want for", statements[0]) }
	init, ok := loop.Initializer.(*cDeclarationStatement)
	if !ok { t.Fatalf("for initializer type = %T, want declaration", loop.Initializer) }
	if init.Declaration.Type != "int" || init.Declaration.Declarators[0].Name != "i" || init.Declaration.Declarators[0].Initializer != "0" {
		t.Errorf("unexpected for initializer: %#v", init.Declaration)
	}
	if loop.Condition != "i < limit" || loop.Post != "i++" { t.Errorf("for clauses = condition %q, post %q", loop.Condition, loop.Post) }
	body, ok := loop.Body.(*cBlock)
	if !ok || len(body.Statements) != 2 { t.Fatalf("unexpected for body: %#v", loop.Body) }
	firstIf, ok := body.Statements[0].(*cIfStatement)
	if !ok { t.Fatalf("first loop-body statement type = %T", body.Statements[0]) }
	if _, ok := firstIf.Then.(*cContinueStatement); !ok { t.Errorf("continue branch type = %T", firstIf.Then) }
	secondIf, ok := body.Statements[1].(*cIfStatement)
	if !ok { t.Fatalf("second loop-body statement type = %T", body.Statements[1]) }
	if _, ok := secondIf.Then.(*cBreakStatement); !ok { t.Errorf("break branch type = %T", secondIf.Then) }
	emptyLoop, ok := statements[1].(*cForStatement)
	if !ok { t.Fatalf("second statement type = %T, want for", statements[1]) }
	if emptyLoop.Initializer != nil || emptyLoop.Condition != "" || emptyLoop.Post != "" { t.Errorf("unexpected empty for clauses: %#v", emptyLoop) }
}

func TestParseRejectsMalformedControlFlow(t *testing.T) {
	for _, source := range []string{
		"void test() { if ready; }",
		"void test() { while (ready) }",
		"void test() { do work(); }",
		"void test() { for (int i = 0 i < 3; i++) work(); }",
	} {
		if _, err := parseCSource(source); err == nil {
			t.Errorf("expected parse error for %q", source)
		}
	}
}
