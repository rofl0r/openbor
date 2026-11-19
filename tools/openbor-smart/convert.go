package main

import (
	"fmt"
	"strconv"
	"strings"
)

type luaWriter struct {
	out strings.Builder
	indent int
}

func (w *luaWriter) line(s string) {
	w.out.WriteString(strings.Repeat("\t", w.indent))
	w.out.WriteString(s)
	w.out.WriteByte('\n')
}

func ConvertCSource(source string) (string, error) {
	functions, err := parseCSource(source)
	if err != nil { return "", fmt.Errorf("parse C source: %w", err) }
	if len(functions) == 0 { return "", fmt.Errorf("no functions found") }
	w := &luaWriter{}
	w.line("local function __openbor_smart_truth(value)")
	w.indent++
	w.line("return value ~= nil and value ~= false and value ~= 0")
	w.indent--
	w.line("end")
	w.out.WriteByte('\n')
	for i, fn := range functions {
		if i > 0 { w.out.WriteByte('\n') }
		if err := w.function(fn); err != nil {
			return "", fmt.Errorf("function %s (line %d): %w", fn.Name, fn.Line, err)
		}
	}
	return w.out.String(), nil
}

func (w *luaWriter) function(fn cFunction) error {
	params := make([]string, 0, len(fn.Params))
	for i, p := range fn.Params {
		if p.Name == "" { return fmt.Errorf("parameter %d has no name; unnamed parameters are not supported", i+1) }
		params = append(params, p.Name)
	}
	w.line("function " + fn.Name + "(" + strings.Join(params, ", ") + ")")
	w.indent++
	if err := w.block(fn.Body); err != nil { return err }
	w.indent--
	w.line("end")
	return nil
}

func (w *luaWriter) block(block *cBlock) error {
	for _, stmt := range block.Statements {
		if err := w.statement(stmt); err != nil { return err }
	}
	return nil
}

func (w *luaWriter) statement(stmt cStatement) error {
	switch s := stmt.(type) {
	case *cBlock:
		w.line("do")
		w.indent++
		if err := w.block(s); err != nil { return err }
		w.indent--
		w.line("end")
	case *cDeclarationStatement:
		for _, d := range s.Declaration.Declarators {
			if d.Pointer > 0 { return fmt.Errorf("pointer variable %q is not supported by Lua conversion", d.Name) }
			if len(d.ArraySizes) > 0 {
				if d.Initializer != "" { return fmt.Errorf("array initializer for %q is not supported", d.Name) }
				w.line("local " + d.Name + " = {}")
				continue
			}
			value := "nil"
			if d.Initializer != "" {
				expr, err := luaExpression(d.Initializer)
				if err != nil { return fmt.Errorf("initializer for %s: %w", d.Name, err) }
				value = expr
			}
			w.line("local " + d.Name + " = " + value)
		}
	case *cReturnStatement:
		if strings.TrimSpace(s.Expression) == "" { w.line("return"); return nil }
		expr, err := luaExpression(s.Expression)
		if err != nil { return fmt.Errorf("return expression: %w", err) }
		w.line("return " + expr)
	case *cExpressionStatement:
		if strings.TrimSpace(s.Expression) == "" { return nil }
		expr, err := luaStatementExpression(s.Expression)
		if err != nil { return fmt.Errorf("expression statement %q: %w", s.Expression, err) }
		w.line(expr)
	case *cIfStatement:
		condition, err := luaExpression(s.Condition)
		if err != nil { return fmt.Errorf("if condition: %w", err) }
		w.line("if __openbor_smart_truth(" + condition + ") then")
		w.indent++
		if err := w.statement(s.Then); err != nil { return err }
		w.indent--
		if s.Else == nil {
			w.line("end")
		} else {
			w.line("else")
			w.indent++
			if err := w.statement(s.Else); err != nil { return err }
			w.indent--
			w.line("end")
		}
	case *cWhileStatement:
		if containsContinue(s.Body) { return fmt.Errorf("continue inside while-loop is not supported yet") }
		condition, err := luaExpression(s.Condition)
		if err != nil { return fmt.Errorf("while condition: %w", err) }
		w.line("while __openbor_smart_truth(" + condition + ") do")
		w.indent++
		if err := w.statement(s.Body); err != nil { return err }
		w.indent--
		w.line("end")
	case *cDoWhileStatement:
		condition, err := luaExpression(s.Condition)
		if err != nil { return fmt.Errorf("do-while condition: %w", err) }
		if containsContinue(s.Body) { return fmt.Errorf("continue inside do-while is not supported yet") }
		w.line("repeat")
		w.indent++
		if err := w.statement(s.Body); err != nil { return err }
		w.indent--
		w.line("until not __openbor_smart_truth(" + condition + ")")
	case *cForStatement:
		if containsContinue(s.Body) { return fmt.Errorf("continue inside for-loop is not supported yet") }
		w.line("do")
		w.indent++
		if s.Initializer != nil {
			if err := w.statement(s.Initializer); err != nil { return err }
		}
		condition := "true"
		if strings.TrimSpace(s.Condition) != "" {
			var err error
			condition, err = luaExpression(s.Condition)
			if err != nil { return fmt.Errorf("for condition: %w", err) }
		}
		w.line("while __openbor_smart_truth(" + condition + ") do")
		w.indent++
		if err := w.statement(s.Body); err != nil { return err }
		if strings.TrimSpace(s.Post) != "" {
			post, err := luaStatementExpression(s.Post)
			if err != nil { return fmt.Errorf("for post expression: %w", err) }
			w.line(post)
		}
		w.indent--
		w.line("end")
		w.indent--
		w.line("end")
	case *cSwitchStatement:
		value, err := luaExpression(s.Expression)
		if err != nil { return fmt.Errorf("switch expression: %w", err) }
		w.line("repeat")
		w.indent++
		for i, item := range s.Cases {
			if item.Default && i != len(s.Cases)-1 { return fmt.Errorf("default label must be last; C switch fallthrough is not supported") }
			if len(item.Statements) == 0 { return fmt.Errorf("empty switch case may fall through; explicit case bodies are required") }
			body := item.Statements
			last := body[len(body)-1]
			if _, ok := last.(*cBreakStatement); ok {
				body = body[:len(body)-1]
			} else if _, ok := last.(*cReturnStatement); !ok {
				if i != len(s.Cases)-1 { return fmt.Errorf("switch case %q can fall through; add break or return", item.Value) }
			}
			if item.Default && i == 0 {
				w.line("if true then")
			} else if item.Default {
				w.line("else")
			} else if i == 0 {
				caseValue, err := luaExpression(item.Value)
				if err != nil { return fmt.Errorf("case label %q: %w", item.Value, err) }
				w.line("if " + value + " == " + caseValue + " then")
			} else {
				caseValue, err := luaExpression(item.Value)
				if err != nil { return fmt.Errorf("case label %q: %w", item.Value, err) }
				w.line("elseif " + value + " == " + caseValue + " then")
			}
			w.indent++
			for _, child := range body {
				if err := w.statement(child); err != nil { return err }
			}
			w.indent--
		}
		w.line("end")
		w.indent--
		w.line("until true")
	case *cBreakStatement:
		w.line("break")
	case *cContinueStatement:
		return fmt.Errorf("continue is not supported outside loops; inside loops it requires loop-specific rewriting")
	case *cEmptyStatement:
		return nil
	default:
		return fmt.Errorf("unsupported statement node %T", stmt)
	}
	return nil
}

func containsContinue(stmt cStatement) bool {
	switch s := stmt.(type) {
	case *cContinueStatement:
		return true
	case *cBlock:
		for _, child := range s.Statements { if containsContinue(child) { return true } }
	case *cIfStatement:
		return containsContinue(s.Then) || s.Else != nil && containsContinue(s.Else)
	case *cWhileStatement:
		return containsContinue(s.Body)
	case *cDoWhileStatement:
		return containsContinue(s.Body)
	case *cForStatement:
		return containsContinue(s.Body)
	case *cSwitchStatement:
		for _, item := range s.Cases { for _, child := range item.Statements { if containsContinue(child) { return true } } }
	}
	return false
}

func luaStatementExpression(source string) (string, error) {
	expr, err := parseCExpression(source)
	if err != nil { return "", err }
	switch e := expr.(type) {
	case cUnaryExpr:
		if e.Operator == "++" || e.Operator == "--" {
			name, err := luaLValue(e.Operand)
			if err != nil { return "", err }
			op := "+"
			if e.Operator == "--" { op = "-" }
			return name + " = " + name + " " + op + " 1", nil
		}
	case cBinaryExpr:
		if strings.HasSuffix(e.Operator, "=") && e.Operator != "==" && e.Operator != "!=" && e.Operator != "<=" && e.Operator != ">=" {
			left, err := luaLValue(e.Left)
			if err != nil { return "", err }
			right, err := luaExprNode(e.Right)
			if err != nil { return "", err }
			op := strings.TrimSuffix(e.Operator, "=")
			if op == "" { return left + " = " + right, nil }
			return left + " = " + left + " " + op + " (" + right + ")", nil
		}
	}
	return luaExprNode(expr)
}

func luaLValue(expr cExpr) (string, error) {
	switch e := expr.(type) {
	case cIdentifierExpr:
		return e.Name, nil
	case cMemberExpr:
		base, err := luaExprNode(e.Value); if err != nil { return "", err }
		return base + "." + e.Member, nil
	case cIndexExpr:
		base, err := luaExprNode(e.Value); if err != nil { return "", err }
		index, err := luaExprNode(e.Index); if err != nil { return "", err }
		return base + "[(" + index + ") + 1]", nil
	default:
		return "", fmt.Errorf("assignment target %T is not a supported variable", expr)
	}
}

func luaExpression(source string) (string, error) {
	expr, err := parseCExpression(source)
	if err != nil { return "", err }
	return luaExprNode(expr)
}

func luaExprNode(expr cExpr) (string, error) {
	switch e := expr.(type) {
	case cIdentifierExpr:
		switch e.Name { case "NULL": return "nil", nil; case "true", "false", "nil": return e.Name, nil }
		return e.Name, nil
	case cLiteralExpr:
		switch e.Kind {
		case tokenNumber:
			return normalizeNumber(e.Text), nil
		case tokenString:
			return e.Text, nil
		case tokenCharacter:
			value, err := strconv.Unquote(e.Text)
			if err != nil { return "", fmt.Errorf("invalid character literal %s: %w", e.Text, err) }
			return strconv.Quote(value), nil
		}
	case cCastExpr:
		if strings.Contains(e.Type, "*") { return "", fmt.Errorf("pointer casts are not supported") }
		return luaExprNode(e.Value)
	case cSizeofTypeExpr:
		return "", fmt.Errorf("sizeof(type) has no direct Lua equivalent")
	case cUnaryExpr:
		if e.Postfix { return "", fmt.Errorf("postfix %s inside an expression is not supported; use it as a standalone statement", e.Operator) }
		switch e.Operator {
		case "!":
			value, err := luaExprNode(e.Operand); if err != nil { return "", err }
			return "(not " + value + ")", nil
		case "+", "-", "~":
			value, err := luaExprNode(e.Operand); if err != nil { return "", err }
			return "(" + e.Operator + value + ")", nil
		case "sizeof":
			return "", fmt.Errorf("sizeof(expression) has no direct Lua equivalent")
		default:
			return "", fmt.Errorf("C unary operator %q has no supported Lua equivalent", e.Operator)
		}
	case cBinaryExpr:
		left, err := luaExprNode(e.Left); if err != nil { return "", err }
		right, err := luaExprNode(e.Right); if err != nil { return "", err }
		op := e.Operator
		switch op { case "&&": op = "and"; case "||": op = "or"; case "!=": op = "~=" }
		if strings.HasSuffix(op, "=") && op != "==" && op != "!=" && op != "~=" && op != "<=" && op != ">=" {
			return "", fmt.Errorf("compound assignment %q is only supported as a standalone statement", op)
		}
		return "(" + left + " " + op + " " + right + ")", nil
	case cConditionalExpr:
		condition, err := luaExprNode(e.Condition); if err != nil { return "", err }
		thenValue, err := luaExprNode(e.Then); if err != nil { return "", err }
		elseValue, err := luaExprNode(e.Else); if err != nil { return "", err }
		return "(function() if __openbor_smart_truth(" + condition + ") then return " + thenValue + " else return " + elseValue + " end end)()", nil
	case cCallExpr:
		callee, err := luaExprNode(e.Callee); if err != nil { return "", err }
		args := make([]string, len(e.Arguments))
		for i, arg := range e.Arguments { args[i], err = luaExprNode(arg); if err != nil { return "", err } }
		return callee + "(" + strings.Join(args, ", ") + ")", nil
	case cIndexExpr:
		base, err := luaExprNode(e.Value); if err != nil { return "", err }
		index, err := luaExprNode(e.Index); if err != nil { return "", err }
		return base + "[(" + index + ") + 1]", nil
	case cMemberExpr:
		base, err := luaExprNode(e.Value); if err != nil { return "", err }
		return base + "." + e.Member, nil
	case cCommaExpr:
		return "", fmt.Errorf("comma operator is not supported in Lua expressions")
	}
	return "", fmt.Errorf("unsupported expression node %T", expr)
}

func normalizeNumber(text string) string {
	lower := strings.ToLower(text)
	for len(lower) > 0 && strings.ContainsRune("ulf", rune(lower[len(lower)-1])) {
		lower = lower[:len(lower)-1]
	}
	if strings.HasPrefix(lower, "0") && len(lower) > 1 && lower[1] >= '0' && lower[1] <= '7' && !strings.ContainsAny(lower, ".eEpPxX") {
		if value, err := strconv.ParseInt(lower, 8, 64); err == nil { return strconv.FormatInt(value, 10) }
	}
	return lower
}
