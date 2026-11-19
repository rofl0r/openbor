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
