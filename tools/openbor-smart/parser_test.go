package main

import "testing"

func TestParseCFunctionsAndParameters(t *testing.T) {
	source := "void test(int player, const char *name, float values[]) { return; }"
	functions, err := parseCSource(source)
	if err != nil { t.Fatalf("parseCSource failed: %v", err) }
	if len(functions) != 1 { t.Fatalf("got %d functions, want 1", len(functions)) }
	fn := functions[0]
	if fn.Name != "test" || fn.ReturnType != "void" { t.Fatalf("unexpected function: %#v", fn) }
	if len(fn.Params) != 3 { t.Fatalf("got %d parameters, want 3", len(fn.Params)) }
	if fn.Params[0].Type != "int" || fn.Params[0].Name != "player" { t.Errorf("unexpected first parameter: %#v", fn.Params[0]) }
	if fn.Params[1].Type != "const char" || fn.Params[1].Name != "name" || fn.Params[1].Pointer != 1 {
		t.Errorf("unexpected pointer parameter: %#v", fn.Params[1])
	}
	if fn.Params[2].Type != "float" || fn.Params[2].Name != "values" || len(fn.Params[2].ArraySizes) != 1 || fn.Params[2].ArraySizes[0] != "" {
		t.Errorf("unexpected array parameter: %#v", fn.Params[2])
	}
	if len(fn.Body.Statements) != 1 {
		t.Fatalf("got %d body statements, want 1", len(fn.Body.Statements))
	}
	if ret, ok := fn.Body.Statements[0].(*cReturnStatement); !ok || ret.Expression != "" {
		t.Fatalf("unexpected return statement: %#v", fn.Body.Statements[0])
	}
}

func TestParseMultipleDeclaratorsPointersAndArrays(t *testing.T) {
	source := "void test() { int a = 1, *b, values[4], matrix[2][3]; }"
	functions, err := parseCSource(source)
	if err != nil { t.Fatalf("parseCSource failed: %v", err) }
	declStmt, ok := functions[0].Body.Statements[0].(*cDeclarationStatement)
	if !ok { t.Fatalf("statement type = %T, want declaration", functions[0].Body.Statements[0]) }
	decl := declStmt.Declaration
	if decl.Type != "int" || len(decl.Declarators) != 4 {
		t.Fatalf("unexpected declaration: %#v", decl)
	}
	if d := decl.Declarators[0]; d.Name != "a" || d.Initializer != "1" {
		t.Errorf("unexpected initialized declarator: %#v", d)
	}
	if d := decl.Declarators[1]; d.Name != "b" || d.Pointer != 1 {
		t.Errorf("unexpected pointer declarator: %#v", d)
	}
	if d := decl.Declarators[2]; d.Name != "values" || len(d.ArraySizes) != 1 || d.ArraySizes[0] != "4" {
		t.Errorf("unexpected array declarator: %#v", d)
	}
	if d := decl.Declarators[3]; d.Name != "matrix" || len(d.ArraySizes) != 2 || d.ArraySizes[0] != "2" || d.ArraySizes[1] != "3" {
		t.Errorf("unexpected multidimensional array declarator: %#v", d)
	}
}

func TestParsePreservesExpressionsAndNestedBlocks(t *testing.T) {
	source := "int test(int x) { if (x > 0) { update(x, 2); } return x + 1; }"
	functions, err := parseCSource(source)
	if err != nil { t.Fatalf("parseCSource failed: %v", err) }
	fn := functions[0]
	if len(fn.Body.Statements) != 2 { t.Fatalf("got %d statements, want 2", len(fn.Body.Statements)) }
	expr, ok := fn.Body.Statements[0].(*cExpressionStatement)
	if !ok || expr.Expression != "if (x > 0) { update(x, 2); }" {
		t.Fatalf("unexpected preserved control-flow statement: %#v", fn.Body.Statements[0])
	}
	ret, ok := fn.Body.Statements[1].(*cReturnStatement)
	if !ok || ret.Expression != "x + 1" { t.Fatalf("unexpected return statement: %#v", fn.Body.Statements[1]) }
}

func TestParseReportsMalformedFunctionAndDeclaration(t *testing.T) {
	for _, source := range []string{
		"void test( { }",
		"void test() { int = 2; }",
		"void test() { int values[2; }",
	} {
		if _, err := parseCSource(source); err == nil {
			t.Errorf("expected parse error for %q", source)
		}
	}
}
