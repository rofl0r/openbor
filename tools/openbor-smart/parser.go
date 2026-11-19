package main

import (
	"fmt"
	"strings"
)

type cParser struct {
	source string
	tokens []token
	pos    int
}

type cFunction struct {
	ReturnType string
	Name       string
	Params     []cParameter
	Body       *cBlock
	Line       int
}

type cParameter struct {
	Type       string
	Name       string
	Pointer    int
	ArraySizes []string
}

type cDeclarator struct {
	Name       string
	Pointer    int
	ArraySizes []string
	Initializer string
}

type cDeclaration struct {
	Type        string
	Declarators []cDeclarator
	Line        int
}

type cStatement interface{ isCStatement() }

type cBlock struct{ Statements []cStatement }
func (*cBlock) isCStatement() {}

type cDeclarationStatement struct{ Declaration cDeclaration }
func (*cDeclarationStatement) isCStatement() {}

type cReturnStatement struct{ Expression string }
func (*cReturnStatement) isCStatement() {}

type cExpressionStatement struct{ Expression string }
func (*cExpressionStatement) isCStatement() {}

type cIfStatement struct {
	Condition string
	Then      cStatement
	Else      cStatement
}
func (*cIfStatement) isCStatement() {}

type cWhileStatement struct {
	Condition string
	Body      cStatement
}
func (*cWhileStatement) isCStatement() {}

type cDoWhileStatement struct {
	Body      cStatement
	Condition string
}
func (*cDoWhileStatement) isCStatement() {}

type cForStatement struct {
	Initializer cStatement
	Condition   string
	Post        string
	Body        cStatement
}
func (*cForStatement) isCStatement() {}

type cBreakStatement struct{}
func (*cBreakStatement) isCStatement() {}

type cContinueStatement struct{}
func (*cContinueStatement) isCStatement() {}

type cEmptyStatement struct{}
func (*cEmptyStatement) isCStatement() {}

var cTypeWords = map[string]bool{
	"void": true, "char": true, "short": true, "int": true, "long": true,
	"float": true, "double": true, "signed": true, "unsigned": true,
	"const": true, "volatile": true, "static": true, "extern": true,
	"register": true, "struct": true, "union": true, "enum": true,
}

func parseCSource(source string) ([]cFunction, error) {
	all, err := lex(source)
	if err != nil {
		return nil, err
	}
	tokens := make([]token, 0, len(all))
	for _, tok := range all {
		if tok.kind != tokenWhitespace && tok.kind != tokenComment {
			tokens = append(tokens, tok)
		}
	}
	p := &cParser{source: source, tokens: tokens}
	var functions []cFunction
	for !p.eof() {
		fn, err := p.parseFunction()
		if err != nil {
			return nil, err
		}
		functions = append(functions, fn)
	}
	return functions, nil
}

func (p *cParser) parseFunction() (cFunction, error) {
	start := p.current()
	line := 1
	if start != nil { line = start.line }
	returnType, err := p.parseType()
	if err != nil { return cFunction{}, err }
	pointer := 0
	for p.accept("*") { pointer++ }
	name := p.current()
	if name == nil || name.kind != tokenIdentifier {
		return cFunction{}, p.errorHere("expected function name")
	}
	p.pos++
	if !p.accept("(") { return cFunction{}, p.errorHere("expected '(' after function name") }
	params, err := p.parseParameters()
	if err != nil { return cFunction{}, err }
	if !p.accept("{") { return cFunction{}, p.errorHere("expected function body") }
	body, err := p.parseBlockAfterOpen()
	if err != nil { return cFunction{}, err }
	if pointer > 0 { returnType += strings.Repeat("*", pointer) }
	return cFunction{ReturnType: returnType, Name: name.text, Params: params, Body: body, Line: line}, nil
}

func (p *cParser) parseType() (string, error) {
	var parts []string
	for p.current() != nil && cTypeWords[p.current().text] {
		word := p.current().text
		parts = append(parts, word)
		p.pos++
		if word == "struct" || word == "union" || word == "enum" {
			if p.current() != nil && p.current().kind == tokenIdentifier {
				parts = append(parts, p.current().text)
				p.pos++
			}
		}
	}
	if len(parts) == 0 {
		return "", p.errorHere("expected C type")
	}
	return strings.Join(parts, " "), nil
}

func (p *cParser) parseParameters() ([]cParameter, error) {
	var params []cParameter
	if p.accept(")") { return params, nil }
	if p.currentText() == "void" && p.peekText(1) == ")" {
		p.pos += 2
		return params, nil
	}
	for {
		typ, err := p.parseType()
		if err != nil { return nil, err }
		pointers := 0
		for p.accept("*") { pointers++ }
		name := ""
		if p.current() != nil && p.current().kind == tokenIdentifier {
			name = p.current().text
			p.pos++
		}
		arrays, err := p.parseArraySuffixes()
		if err != nil { return nil, err }
		params = append(params, cParameter{Type: typ, Name: name, Pointer: pointers, ArraySizes: arrays})
		if p.accept(")") { return params, nil }
		if !p.accept(",") { return nil, p.errorHere("expected ',' or ')' in parameter list") }
	}
}

func (p *cParser) parseBlockAfterOpen() (*cBlock, error) {
	block := &cBlock{}
	for {
		if p.eof() {
			return nil, p.errorHere("unterminated block")
		}
		if p.accept("}") {
			return block, nil
		}
		stmt, err := p.parseStatement()
		if err != nil {
			return nil, err
		}
		block.Statements = append(block.Statements, stmt)
	}
}

func (p *cParser) parseStatement() (cStatement, error) {
	switch p.currentText() {
	case ";":
		p.pos++
		return &cEmptyStatement{}, nil
	case "{":
		p.pos++
		return p.parseBlockAfterOpen()
	case "if":
		return p.parseIfStatement()
	case "while":
		return p.parseWhileStatement()
	case "do":
		return p.parseDoWhileStatement()
	case "for":
		return p.parseForStatement()
	case "return":
		p.pos++
		expr, err := p.collectUntil(";")
		if err != nil { return nil, err }
		if !p.accept(";") { return nil, p.errorHere("expected ';' after return") }
		return &cReturnStatement{Expression: expr}, nil
	case "break":
		p.pos++
		if !p.accept(";") { return nil, p.errorHere("expected ';' after break") }
		return &cBreakStatement{}, nil
	case "continue":
		p.pos++
		if !p.accept(";") { return nil, p.errorHere("expected ';' after continue") }
		return &cContinueStatement{}, nil
	}
	if p.current() != nil && cTypeWords[p.current().text] {
		decl, err := p.parseDeclaration()
		if err != nil { return nil, err }
		return &cDeclarationStatement{Declaration: decl}, nil
	}
	expr, err := p.collectUntil(";")
	if err != nil { return nil, err }
	if expr == "" { return nil, p.errorHere("expected statement") }
	if !p.accept(";") { return nil, p.errorHere("expected ';' after expression") }
	return &cExpressionStatement{Expression: expr}, nil
}

func (p *cParser) parseIfStatement() (cStatement, error) {
	p.pos++
	condition, err := p.parseParenthesizedExpression("if")
	if err != nil { return nil, err }
	thenStmt, err := p.parseStatement()
	if err != nil { return nil, err }
	stmt := &cIfStatement{Condition: condition, Then: thenStmt}
	if p.accept("else") {
		stmt.Else, err = p.parseStatement()
		if err != nil { return nil, err }
	}
	return stmt, nil
}

func (p *cParser) parseWhileStatement() (cStatement, error) {
	p.pos++
	condition, err := p.parseParenthesizedExpression("while")
	if err != nil { return nil, err }
	body, err := p.parseStatement()
	if err != nil { return nil, err }
	return &cWhileStatement{Condition: condition, Body: body}, nil
}

func (p *cParser) parseDoWhileStatement() (cStatement, error) {
	p.pos++
	body, err := p.parseStatement()
	if err != nil { return nil, err }
	if !p.accept("while") { return nil, p.errorHere("expected 'while' after do body") }
	condition, err := p.parseParenthesizedExpression("do-while")
	if err != nil { return nil, err }
	if !p.accept(";") { return nil, p.errorHere("expected ';' after do-while") }
	return &cDoWhileStatement{Body: body, Condition: condition}, nil
}

func (p *cParser) parseForStatement() (cStatement, error) {
	p.pos++
	if !p.accept("(") { return nil, p.errorHere("expected '(' after for") }
	stmt := &cForStatement{}
	if p.currentText() != ";" {
		if p.current() != nil && cTypeWords[p.current().text] {
			decl, err := p.parseDeclaration()
			if err != nil { return nil, err }
			stmt.Initializer = &cDeclarationStatement{Declaration: decl}
		} else {
			init, err := p.collectUntil(";")
			if err != nil { return nil, err }
			if !p.accept(";") { return nil, p.errorHere("expected ';' after for initializer") }
			if init != "" { stmt.Initializer = &cExpressionStatement{Expression: init} }
		}
	} else {
		p.pos++
	}
	condition, err := p.collectUntil(";")
	if err != nil { return nil, err }
	stmt.Condition = condition
	if !p.accept(";") { return nil, p.errorHere("expected ';' after for condition") }
	post, err := p.collectUntil(")")
	if err != nil { return nil, err }
	stmt.Post = post
	if !p.accept(")") { return nil, p.errorHere("expected ')' after for clauses") }
	stmt.Body, err = p.parseStatement()
	if err != nil { return nil, err }
	return stmt, nil
}

func (p *cParser) parseParenthesizedExpression(context string) (string, error) {
	if !p.accept("(") { return "", p.errorHere("expected '(' after " + context) }
	expr, err := p.collectUntil(")")
	if err != nil { return "", err }
	if !p.accept(")") { return "", p.errorHere("expected ')' after " + context + " condition") }
	if expr == "" { return "", p.errorHere("empty " + context + " condition") }
	return expr, nil
}

func (p *cParser) parseDeclaration() (cDeclaration, error) {
	line := p.current().line
	typ, err := p.parseType()
	if err != nil { return cDeclaration{}, err }
	decl := cDeclaration{Type: typ, Line: line}
	for {
		pointers := 0
		for p.accept("*") { pointers++ }
		name := p.current()
		if name == nil || name.kind != tokenIdentifier {
			return cDeclaration{}, p.errorHere("expected variable name")
		}
		p.pos++
		arrays, err := p.parseArraySuffixes()
		if err != nil { return cDeclaration{}, err }
		init := ""
		if p.accept("=") {
			init, err = p.collectUntil(",", ";")
			if err != nil { return cDeclaration{}, err }
			if init == "" { return cDeclaration{}, p.errorHere("expected initializer expression") }
		}
		decl.Declarators = append(decl.Declarators, cDeclarator{
			Name: name.text, Pointer: pointers, ArraySizes: arrays, Initializer: init,
		})
		if p.accept(";") { return decl, nil }
		if !p.accept(",") { return cDeclaration{}, p.errorHere("expected ',' or ';' in declaration") }
	}
}

func (p *cParser) parseArraySuffixes() ([]string, error) {
	var sizes []string
	for p.accept("[") {
		size, err := p.collectUntil("]")
		if err != nil { return nil, err }
		if !p.accept("]") { return nil, p.errorHere("expected ']' in array declarator") }
		sizes = append(sizes, size)
	}
	return sizes, nil
}

func (p *cParser) collectUntil(stops ...string) (string, error) {
	start := p.pos
	round, square, curly := 0, 0, 0
	for !p.eof() {
		t := p.currentText()
		if round == 0 && square == 0 && curly == 0 {
			for _, stop := range stops {
				if t == stop {
					return p.sourceSlice(start, p.pos), nil
				}
			}
		}
		switch t {
		case "(": round++
		case ")": round--; if round < 0 { return "", p.errorHere("unmatched ')'") }
		case "[": square++
		case "]": square--; if square < 0 { return "", p.errorHere("unmatched ']'") }
		case "{": curly++
		case "}": curly--; if curly < 0 { return "", p.errorHere("unmatched '}'") }
		}
		p.pos++
	}
	return "", p.errorHere("unexpected end of input")
}

func (p *cParser) sourceSlice(from, to int) string {
	if from >= to { return "" }
	start := p.tokens[from].offset
	last := p.tokens[to-1]
	end := last.offset + len(last.text)
	return strings.TrimSpace(p.source[start:end])
}

func (p *cParser) current() *token {
	if p.pos >= len(p.tokens) { return nil }
	return &p.tokens[p.pos]
}
func (p *cParser) currentText() string {
	if t := p.current(); t != nil { return t.text }
	return ""
}
func (p *cParser) peekText(distance int) string {
	i := p.pos + distance
	if i >= len(p.tokens) { return "" }
	return p.tokens[i].text
}
func (p *cParser) accept(text string) bool {
	if p.currentText() != text { return false }
	p.pos++
	return true
}
func (p *cParser) eof() bool { return p.pos >= len(p.tokens) }
func (p *cParser) errorHere(message string) error {
	if t := p.current(); t != nil {
		return fmt.Errorf("line %d, column %d: %s", t.line, t.column, message)
	}
	return fmt.Errorf("end of input: %s", message)
}
