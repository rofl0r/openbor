package main

import "testing"

func TestParseCExpressionCasts(t *testing.T) {
	for _, tc := range []struct{ source, want string }{
		{"(int)value", "(int)value"},
		{"(float *)(pointer + offset)", "(float *)(pointer + offset)"},
		{"(unsigned long)health + 1", "((unsigned long)health + 1)"},
		{"sizeof(int)", "sizeof(int)"},
		{"sizeof(const char *)", "sizeof(const char *)"},
		{"sizeof(value + 1)", "sizeof(value + 1)"},
	} {
		expr, err := parseCExpression(tc.source)
		if err != nil { t.Errorf("%s: parse failed: %v", tc.source, err); continue }
		if got := cExprShape(expr); got != tc.want {
			t.Errorf("%s: shape = %s, want %s", tc.source, got, tc.want)
		}
	}
}

func TestParseCExpressionDoesNotTreatGroupingAsCast(t *testing.T) {
	expr, err := parseCExpression("(value + offset) * scale")
	if err != nil { t.Fatalf("parse failed: %v", err) }
	if got, want := cExprShape(expr), "((value + offset) * scale)"; got != want {
		t.Fatalf("shape = %s, want %s", got, want)
	}
}

func TestParseCExpressionPointerAndTaggedTypeCasts(t *testing.T) {
	for _, tc := range []struct{ source, want string }{
		{"(int **)pointer", "(int * *)pointer"},
		{"(const unsigned char *)buffer", "(const unsigned char *)buffer"},
		{"(struct Entity *)value", "(struct Entity *)value"},
		{"sizeof(void *)", "sizeof(void *)"},
		{"sizeof(struct Entity **)", "sizeof(struct Entity * *)"},
	} {
		expr, err := parseCExpression(tc.source)
		if err != nil {
			t.Errorf("%s: parse failed: %v", tc.source, err)
			continue
		}
		if got := cExprShape(expr); got != tc.want {
			t.Errorf("%s: shape = %s, want %s", tc.source, got, tc.want)
		}
	}
}

func TestParseCExpressionSizeofAndCastPrecedence(t *testing.T) {
	for _, tc := range []struct{ source, want string }{
		{"sizeof value", "sizeof(value)"},
		{"sizeof *pointer", "sizeof(*pointer)"},
		{"sizeof pointer->health", "sizeof(pointer->health)"},
		{"sizeof(value + offset) * scale", "(sizeof(value + offset) * scale)"},
		{"(int)*pointer + offset", "((int)*pointer + offset)"},
		{"(float)(health + bonus) / scale", "((float)(health + bonus) / scale)"},
		{"(unsigned long)value >> 2", "((unsigned long)value >> 2)"},
	} {
		expr, err := parseCExpression(tc.source)
		if err != nil {
			t.Errorf("%s: parse failed: %v", tc.source, err)
			continue
		}
		if got := cExprShape(expr); got != tc.want {
			t.Errorf("%s: shape = %s, want %s", tc.source, got, tc.want)
		}
	}
}

func TestParseCExpressionSizeofAST(t *testing.T) {
	// sizeof(type) must remain distinct from sizeof(expression), and the
	// unparenthesized form consumes exactly one unary expression.
	expr, err := parseCExpression("sizeof(unsigned int *)")
	if err != nil {
		t.Fatalf("sizeof type: parse failed: %v", err)
	}
	if got, ok := expr.(cSizeofTypeExpr); !ok || got.Type != "unsigned int *" {
		t.Fatalf("sizeof type: unexpected AST %#v", expr)
	}

	for _, tc := range []struct {
		source string
		operator string
		postfix bool
	}{
		{"sizeof value", "", false},
		{"sizeof *pointer", "*", false},
		{"sizeof pointer->health", "", false},
		{"sizeof value++", "++", true},
	} {
		expr, err = parseCExpression(tc.source)
		if err != nil {
			t.Errorf("%s: parse failed: %v", tc.source, err)
			continue
		}
		sizeofExpr, ok := expr.(cUnaryExpr)
		if !ok || sizeofExpr.Operator != "sizeof" {
			t.Errorf("%s: expected sizeof expression AST, got %#v", tc.source, expr)
			continue
		}
		if tc.operator != "" {
			operand, ok := sizeofExpr.Operand.(cUnaryExpr)
			if !ok || operand.Operator != tc.operator || operand.Postfix != tc.postfix {
				t.Errorf("%s: unexpected sizeof operand AST %#v", tc.source, sizeofExpr.Operand)
			}
		}
	}

	expr, err = parseCExpression("sizeof value + offset")
	if err != nil {
		t.Fatalf("sizeof unary operand with addition: parse failed: %v", err)
	}
	binary, ok := expr.(cBinaryExpr)
	if !ok || binary.Operator != "+" {
		t.Fatalf("sizeof value + offset: expected addition outside sizeof, got %#v", expr)
	}
	if sizeofExpr, ok := binary.Left.(cUnaryExpr); !ok || sizeofExpr.Operator != "sizeof" {
		t.Fatalf("sizeof value + offset: expected sizeof on left operand, got %#v", binary.Left)
	}
}
