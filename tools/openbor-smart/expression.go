package main

import (
	"fmt"
	"strings"
)

// cExpr is the syntax tree used by the expression parser. Source spelling is
// retained on leaf nodes so a later Lua generator can make deliberate,
// compatibility-aware translations.
type cExpr interface{ isCExpr() }

type cIdentifierExpr struct{ Name string }
func (cIdentifierExpr) isCExpr() {}

type cLiteralExpr struct {
	Kind tokenKind
	Text string
}
func (cLiteralExpr) isCExpr() {}

type cCastExpr struct {\n\tType  string\n\tValue cExpr\n}\n\nfunc (cCastExpr) isCExpr() {}\n\ntype cSizeofTypeExpr struct{ Type string }\n\nfunc (cSizeofTypeExpr) isCExpr() {}\n\ntype cUnaryExpr struct {
	Operator string
	Operand  cExpr
	Postfix  bool
}
func (cUnaryExpr) isCExpr() {}

type cBinaryExpr struct {
	Operator string
	Left     cExpr
	Right    cExpr
}
func (cBinaryExpr) isCExpr() {}

type cConditionalExpr struct {
	Condition cExpr
	Then      cExpr
	Else      cExpr
}
func (cConditionalExpr) isCExpr() {}

type cCallExpr struct {
	Callee    cExpr
	Arguments []cExpr
}
func (cCallExpr) isCExpr() {}

type cIndexExpr struct {
	Value cExpr
	Index cExpr
}
func (cIndexExpr) isCExpr() {}

type cMemberExpr struct {
	Value  cExpr
	Member string
	Pointer bool
}
func (cMemberExpr) isCExpr() {}

type cCommaExpr struct{ Values []cExpr }
func (cCommaExpr) isCExpr() {}

type cExpressionParser struct {
	tokens []token
	pos    int
}

func parseCExpression(source string) (cExpr, error) {
	all, err := lex(source)
	if err != nil { return nil, err }
	tokens := make([]token, 0, len(all))
	for _, tok := range all {
		if tok.kind != tokenWhitespace && tok.kind != tokenComment { tokens = append(tokens, tok) }
	}
	if len(tokens) == 0 { return nil, fmt.Errorf("empty expression") }
	p := &cExpressionParser{tokens: tokens}
	expr, err := p.parseExpression(1)
	if err != nil { return nil, err }
	if p.current() != nil {
		return nil, p.errorHere("unexpected token " + p.current().text)
	}
	return expr, nil
}

func (p *cExpressionParser) parseExpression(minPrecedence int) (cExpr, error) {
	left, err := p.parsePrefix()
	if err != nil { return nil, err }
	left, err = p.parsePostfix(left)
	if err != nil { return nil, err }

	for {
		op := p.currentText()
		if op == "?" {
			const conditionalPrecedence = 3
			if conditionalPrecedence < minPrecedence { break }
			p.pos++
			thenExpr, err := p.parseExpression(1)
			if err != nil { return nil, err }
			if !p.accept(":") { return nil, p.errorHere("expected ':' in conditional expression") }
			elseExpr, err := p.parseExpression(conditionalPrecedence)
			if err != nil { return nil, err }
			left = cConditionalExpr{Condition:left, Then:thenExpr, Else:elseExpr}
			continue
		}

		precedence, rightAssociative, ok := cBinaryPrecedence(op)
		if !ok || precedence < minPrecedence { break }
		p.pos++
		nextMin := precedence + 1
		if rightAssociative { nextMin = precedence }
		right, err := p.parseExpression(nextMin)
		if err != nil { return nil, err }
		if op == "," {
			if comma, ok := left.(cCommaExpr); ok {
				left = cCommaExpr{Values: append(comma.Values, right)}
			} else {
				left = cCommaExpr{Values: []cExpr{left, right}}
			}
		} else {
			left = cBinaryExpr{Operator:op, Left:left, Right:right}
		}
	}
	return left, nil
}

func (p *cExpressionParser) parsePrefix() (cExpr, error) {
	t := p.current()
	if t == nil { return nil, p.errorHere("expected expression") }
	switch t.kind {
	case tokenIdentifier:
		p.pos++
		if t.text == "sizeof" && p.currentText() == "(" {
			if typ, end, ok := p.typeNameAt(p.pos + 1); ok && end < len(p.tokens) && p.tokens[end].text == ")" {
				p.pos = end + 1
				return cSizeofTypeExpr{Type:typ}, nil
			}
			p.pos++
			inner, err := p.parseExpression(1)
			if err != nil { return nil, err }
			if !p.accept(")") { return nil, p.errorHere("expected ')' after sizeof") }
			return cUnaryExpr{Operator:"sizeof", Operand:inner}, nil
		}
		return cIdentifierExpr{Name:t.text}, nil
	case tokenNumber, tokenString, tokenCharacter:
		p.pos++
		return cLiteralExpr{Kind:t.kind, Text:t.text}, nil
	}

	if isPrefixOperator(t.text) {
		p.pos++
		operand, err := p.parseExpression(14)
		if err != nil { return nil, err }
		return cUnaryExpr{Operator:t.text, Operand:operand}, nil
	}
	if p.currentText() == "(" {
		if typ, end, ok := p.typeNameAt(p.pos + 1); ok && end < len(p.tokens) && p.tokens[end].text == ")" {
			p.pos = end + 1
			value, err := p.parseExpression(14)
			if err != nil { return nil, err }
			return cCastExpr{Type:typ, Value:value}, nil
		}
		p.pos++
		expr, err := p.parseExpression(1)
		if err != nil { return nil, err }
		if !p.accept(")") { return nil, p.errorHere("expected ')' in grouped expression") }
		return expr, nil
	}
	return nil, p.errorHere("expected expression")
}


// typeNameAt recognises the built-in type-name subset supported by this
// parser. It intentionally does not guess typedef names.
func (p *cExpressionParser) typeNameAt(start int) (string, int, bool) {
	if start >= len(p.tokens) || !cTypeWords[p.tokens[start].text] { return "", start, false }
	parts := []string{}
	i := start
	for i < len(p.tokens) && cTypeWords[p.tokens[i].text] {
		word := p.tokens[i].text
		parts = append(parts, word)
		i++
		if (word == "struct" || word == "union" || word == "enum") && i < len(p.tokens) && p.tokens[i].kind == tokenIdentifier {
			parts = append(parts, p.tokens[i].text)
			i++
		}
	}
	for i < len(p.tokens) && p.tokens[i].text == "*" {
		parts = append(parts, "*")
		i++
	}
	return strings.Join(parts, " "), i, true
}

func (p *cExpressionParser) parsePostfix(expr cExpr) (cExpr, error) {
	for {
		switch p.currentText() {
		case "(":
			p.pos++
			var args []cExpr
			if !p.accept(")") {
				for {
					arg, err := p.parseExpression(2) // comma separates arguments
					if err != nil { return nil, err }
					args = append(args, arg)
					if p.accept(")") { break }
					if !p.accept(",") { return nil, p.errorHere("expected ',' or ')' in call") }
				}
			}
			expr = cCallExpr{Callee:expr, Arguments:args}
		case "[":
			p.pos++
			index, err := p.parseExpression(1)
			if err != nil { return nil, err }
			if !p.accept("]") { return nil, p.errorHere("expected ']' after array index") }
			expr = cIndexExpr{Value:expr, Index:index}
		case ".", "->":
			pointer := p.currentText() == "->"
			p.pos++
			member := p.current()
			if member == nil || member.kind != tokenIdentifier {
				return nil, p.errorHere("expected member name")
			}
			p.pos++
			expr = cMemberExpr{Value:expr, Member:member.text, Pointer:pointer}
		case "++", "--":
			op := p.currentText()
			p.pos++
			expr = cUnaryExpr{Operator:op, Operand:expr, Postfix:true}
		default:
			return expr, nil
		}
	}
}

func cBinaryPrecedence(op string) (int, bool, bool) {
	switch op {
	case ",":
		return 1, false, true
	case "=", "+=", "-=", "*=", "/=", "%=", "<<=", ">>=", "&=", "^=", "|=":
		return 2, true, true
	case "||":
		return 4, false, true
	case "&&":
		return 5, false, true
	case "|":
		return 6, false, true
	case "^":
		return 7, false, true
	case "&":
		return 8, false, true
	case "==", "!=":
		return 9, false, true
	case "<", "<=", ">", ">=":
		return 10, false, true
	case "<<", ">>":
		return 11, false, true
	case "+", "-":
		return 12, false, true
	case "*", "/", "%":
		return 13, false, true
	}
	return 0, false, false
}

func isPrefixOperator(op string) bool {
	switch op {
	case "+", "-", "!", "~", "*", "&", "++", "--":
		return true
	}
	return false
}

func (p *cExpressionParser) current() *token {
	if p.pos >= len(p.tokens) { return nil }
	return &p.tokens[p.pos]
}
func (p *cExpressionParser) currentText() string {
	if t := p.current(); t != nil { return t.text }
	return ""
}
func (p *cExpressionParser) accept(text string) bool {
	if p.currentText() != text { return false }
	p.pos++
	return true
}
func (p *cExpressionParser) errorHere(message string) error {
	if t := p.current(); t != nil {
		return fmt.Errorf("line %d, column %d: %s", t.line, t.column, message)
	}
	return fmt.Errorf("end of expression: %s", message)
}

// cExprShape provides a compact structural representation for tests and
// diagnostics without coupling the AST to Lua output formatting.
func cExprShape(expr cExpr) string {
	switch e := expr.(type) {
	case cIdentifierExpr:
		return e.Name
	case cLiteralExpr:
		return e.Text
	case cUnaryExpr:
		if e.Postfix { return cExprShape(e.Operand) + e.Operator }
		return e.Operator + cExprShape(e.Operand)
	case cBinaryExpr:
		return "(" + cExprShape(e.Left) + " " + e.Operator + " " + cExprShape(e.Right) + ")"
	case cConditionalExpr:
		return "(" + cExprShape(e.Condition) + " ? " + cExprShape(e.Then) + " : " + cExprShape(e.Else) + ")"
	case cCallExpr:
		parts := make([]string, len(e.Arguments))
		for i, arg := range e.Arguments { parts[i] = cExprShape(arg) }
		return cExprShape(e.Callee) + "(" + strings.Join(parts, ", ") + ")"
	case cIndexExpr:
		return cExprShape(e.Value) + "[" + cExprShape(e.Index) + "]"
	case cMemberExpr:
		op := "."
		if e.Pointer { op = "->" }
		return cExprShape(e.Value) + op + e.Member
	case cCommaExpr:
		parts := make([]string, len(e.Values))
		for i, value := range e.Values { parts[i] = cExprShape(value) }
		return "(" + strings.Join(parts, ", ") + ")"
	}
	return fmt.Sprintf("<%T>", expr)
}
