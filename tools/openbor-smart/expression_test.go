package main

import "testing"

func TestParseCExpressionOperatorPrecedence(t *testing.T) {
	expr, err := parseCExpression("a + b * c << 2 == d && ready || fallback")
	if err != nil { t.Fatalf("parseCExpression failed: %v", err) }
	want := "(((((a + (b * c)) << 2) == d) && ready) || fallback)"
	if got := cExprShape(expr); got != want {
		t.Fatalf("expression shape = %s, want %s", got, want)
	}
}

func TestParseCExpressionPointerAndIncrementCombinations(t *testing.T) {
	for _, tc := range []struct{ source, want string }{
		{"*pointer++", "*pointer++"},
		{"(*pointer)++", "*pointer++"},
		{"++*pointer", "++*pointer"},
		{"--entity->health", "--entity->health"},
		{"&items[index++]", "&items[index++]"},
		{"*(*pointer + offset)", "*(*pointer + offset)"},
		{"entity->health += ++damage", "(entity->health += ++damage)"},
		{"array[--index]", "array[--index]"},
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

	// In C, *pointer++ means *(pointer++), while (*pointer)++ increments
	// the pointed-to value. Their printed shapes are similar, so verify
	// the AST structure as well as the diagnostic shape.
	expr, err := parseCExpression("*pointer++")
	if err != nil {
		t.Fatalf("*pointer++: parse failed: %v", err)
	}
	deref, ok := expr.(cUnaryExpr)
	if !ok || deref.Operator != "*" {
		t.Fatalf("*pointer++: expected outer dereference, got %#v", expr)
	}
	postfix, ok := deref.Operand.(cUnaryExpr)
	if !ok || !postfix.Postfix || postfix.Operator != "++" {
		t.Fatalf("*pointer++: expected postfix increment of pointer, got %#v", deref.Operand)
	}

	expr, err = parseCExpression("(*pointer)++")
	if err != nil {
		t.Fatalf("(*pointer)++: parse failed: %v", err)
	}
	postfix, ok = expr.(cUnaryExpr)
	if !ok || !postfix.Postfix || postfix.Operator != "++" {
		t.Fatalf("(*pointer)++: expected postfix increment, got %#v", expr)
	}
	deref, ok = postfix.Operand.(cUnaryExpr)
	if !ok || deref.Operator != "*" {
		t.Fatalf("(*pointer)++: expected increment operand to be dereference, got %#v", postfix.Operand)
	}
}

func TestParseCExpressionAssignmentAssociativity(t *testing.T) {
	expr, err := parseCExpression("a = b += c * 2")
	if err != nil { t.Fatalf("parseCExpression failed: %v", err) }
	want := "(a = (b += (c * 2)))"
	if got := cExprShape(expr); got != want { t.Fatalf("expression shape = %s, want %s", got, want) }
}

func TestParseCExpressionCallsAndNestedArguments(t *testing.T) {
	expr, err := parseCExpression("spawn(name, getentityproperty(self, \"health\"), clamp(x, 0, max))")
	if err != nil { t.Fatalf("parseCExpression failed: %v", err) }
	want := "spawn(name, getentityproperty(self, \"health\"), clamp(x, 0, max))"
	if got := cExprShape(expr); got != want { t.Fatalf("expression shape = %s, want %s", got, want) }
	call, ok := expr.(cCallExpr)
	if !ok || len(call.Arguments) != 3 { t.Fatalf("unexpected call AST: %#v", expr) }
}

func TestParseCExpressionArraysPointersAndMembers(t *testing.T) {
	expr, err := parseCExpression("entity->animation->frame[index + 1].sprite")
	if err != nil { t.Fatalf("parseCExpression failed: %v", err) }
	want := "entity->animation->frame[(index + 1)].sprite"
	if got := cExprShape(expr); got != want { t.Fatalf("expression shape = %s, want %s", got, want) }

	expr, err = parseCExpression("*items[index]++")
	if err != nil { t.Fatalf("parseCExpression failed: %v", err) }
	want = "*items[index]++"
	if got := cExprShape(expr); got != want { t.Fatalf("expression shape = %s, want %s", got, want) }
}


func TestParseCExpressionPointerArithmeticAndPostfixChains(t *testing.T) {
	for _, tc := range []struct{ source, want string }{
		{"*(base + index)", "*(base + index)"},
		{"&entity->health", "&entity->health"},
		{"matrix[row][column]", "matrix[row][column]"},
		{"(pointer + offset)->health", "(pointer + offset)->health"},
		{"(*get_entity())->health", "(*get_entity())->health"},
		{"items[index].animation.frame", "items[index].animation.frame"},
		{"pointer++->field", "pointer++->field"},
		{"*items[index + offset]", "*items[(index + offset)]"},
		{"*(pointer + offset)++", "*(pointer + offset)++"},
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


func TestParseCExpressionUnaryAndConditional(t *testing.T) {
	expr, err := parseCExpression("!dead ? ++health : health - damage")
	if err != nil { t.Fatalf("parseCExpression failed: %v", err) }
	want := "(!dead ? ++health : (health - damage))"
	if got := cExprShape(expr); got != want { t.Fatalf("expression shape = %s, want %s", got, want) }
}

func TestParseCExpressionCommaAndPostfix(t *testing.T) {
	expr, err := parseCExpression("(x++, y--, ++z)")
	if err != nil { t.Fatalf("parseCExpression failed: %v", err) }
	want := "(x++, y--, ++z)"
	if got := cExprShape(expr); got != want { t.Fatalf("expression shape = %s, want %s", got, want) }
}

func TestParseCExpressionRejectsMalformedInput(t *testing.T) {
	for _, source := range []string{
		"a +",
		"call(a,)",
		"items[",
		"entity->",
		"a ? b",
		"(a + b",
		"a b",
	} {
		if _, err := parseCExpression(source); err == nil {
			t.Errorf("expected parse error for %q", source)
		}
	}
}

func TestParseCExpressionBitwiseModuloAndAssociativity(t *testing.T) {
	for _, tc := range []struct{ source, want string }{
		{"health % 3 + bonus", "((health % 3) + bonus)"},
		{"flags & FLAG_HIDDEN | FLAG_FALLING", "((flags & FLAG_HIDDEN) | FLAG_FALLING)"},
		{"mask ^ bits & other", "(mask ^ (bits & other))"},
		{"health >= 0 && alive != 0", "((health >= 0) && (alive != 0))"},
		{"index << 2 + offset", "(index << (2 + offset))"},
		{"score - damage - bonus", "((score - damage) - bonus)"},
		{"health *= scale + bonus", "(health *= (scale + bonus))"},
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
