package main

import (
	"fmt"
	"strings"
)

type tokenKind uint8

const (
	tokenWhitespace tokenKind = iota
	tokenComment
	tokenIdentifier
	tokenNumber
	tokenString
	tokenCharacter
	tokenOperator
	tokenPunctuation
)

type token struct {
	kind   tokenKind
	text   string
	offset int
	line   int
	column int
}

func lex(source string) ([]token, error) {
	var tokens []token
	for i, line, column := 0, 1, 1; i < len(source); {
		start, startLine, startColumn := i, line, column
		kind := tokenPunctuation
		c := source[i]

		switch {
		case isSpace(c):
			kind = tokenWhitespace
			for i < len(source) && isSpace(source[i]) {
				i, line, column = advance(source, i, line, column)
			}

		case c == '/' && i+1 < len(source) && source[i+1] == '/':
			kind = tokenComment
			for i < len(source) && source[i] != '\n' && source[i] != '\r' {
				i, line, column = advance(source, i, line, column)
			}

		case c == '/' && i+1 < len(source) && source[i+1] == '*':
			kind = tokenComment
			i, line, column = advance(source, i, line, column)
			i, line, column = advance(source, i, line, column)
			closed := false
			for i < len(source) {
				if source[i] == '*' && i+1 < len(source) && source[i+1] == '/' {
					i, line, column = advance(source, i, line, column)
					i, line, column = advance(source, i, line, column)
					closed = true
					break
				}
				i, line, column = advance(source, i, line, column)
			}
			if !closed {
				return nil, fmt.Errorf("line %d, column %d: unterminated block comment", startLine, startColumn)
			}

		case c == '"' || c == '\'':
			if c == '"' {
				kind = tokenString
			} else {
				kind = tokenCharacter
			}
			quote := c
			i, line, column = advance(source, i, line, column)
			closed := false
			for i < len(source) {
				if source[i] == '\\' {
					i, line, column = advance(source, i, line, column)
					if i < len(source) {
						i, line, column = advance(source, i, line, column)
					}
					continue
				}
				if source[i] == quote {
					i, line, column = advance(source, i, line, column)
					closed = true
					break
				}
				if source[i] == '\n' || source[i] == '\r' {
					return nil, fmt.Errorf("line %d, column %d: newline in quoted literal", startLine, startColumn)
				}
				i, line, column = advance(source, i, line, column)
			}
			if !closed {
				return nil, fmt.Errorf("line %d, column %d: unterminated quoted literal", startLine, startColumn)
			}

		case isIdentifierStart(c):
			kind = tokenIdentifier
			for i < len(source) && isIdentifierPart(source[i]) {
				i, line, column = advance(source, i, line, column)
			}

		case isDigit(c) || c == '.' && i+1 < len(source) && isDigit(source[i+1]):
			kind = tokenNumber
			i, line, column = scanNumber(source, i, line, column)

		default:
			if op, ok := matchOperator(source[i:]); ok {
				kind = tokenOperator
				for range op {
					i, line, column = advance(source, i, line, column)
				}
			} else {
				i, line, column = advance(source, i, line, column)
				if strings.ContainsRune("(){}[];,.:?@", rune(c)) {
					kind = tokenPunctuation
				} else {
					kind = tokenOperator
				}
			}
		}

		tokens = append(tokens, token{
			kind: kind, text: source[start:i], offset: start,
			line: startLine, column: startColumn,
		})
	}
	return tokens, nil
}

func scanNumber(source string, i, line, column int) (int, int, int) {
	if i+1 < len(source) && source[i] == '0' && (source[i+1] == 'x' || source[i+1] == 'X') {
		i, line, column = advance(source, i, line, column)
		i, line, column = advance(source, i, line, column)
		for i < len(source) && (isHexDigit(source[i]) || source[i] == '_') {
			i, line, column = advance(source, i, line, column)
		}
		if i < len(source) && source[i] == '.' {
			i, line, column = advance(source, i, line, column)
			for i < len(source) && (isHexDigit(source[i]) || source[i] == '_') {
				i, line, column = advance(source, i, line, column)
			}
		}
		if i < len(source) && (source[i] == 'p' || source[i] == 'P') {
			i, line, column = advance(source, i, line, column)
			if i < len(source) && (source[i] == '+' || source[i] == '-') {
				i, line, column = advance(source, i, line, column)
			}
			for i < len(source) && isDigit(source[i]) {
				i, line, column = advance(source, i, line, column)
			}
		}
		for i < len(source) && strings.ContainsRune("uUlLfF", rune(source[i])) {
			i, line, column = advance(source, i, line, column)
		}
		return i, line, column
	}

	for i < len(source) && (isDigit(source[i]) || source[i] == '_') {
		i, line, column = advance(source, i, line, column)
	}
	if i < len(source) && source[i] == '.' {
		i, line, column = advance(source, i, line, column)
		for i < len(source) && (isDigit(source[i]) || source[i] == '_') {
			i, line, column = advance(source, i, line, column)
		}
	}
	if i < len(source) && (source[i] == 'e' || source[i] == 'E') {
		i, line, column = advance(source, i, line, column)
		if i < len(source) && (source[i] == '+' || source[i] == '-') {
			i, line, column = advance(source, i, line, column)
		}
		for i < len(source) && isDigit(source[i]) {
			i, line, column = advance(source, i, line, column)
		}
	}
	for i < len(source) && strings.ContainsRune("uUlLfF", rune(source[i])) {
		i, line, column = advance(source, i, line, column)
	}
	return i, line, column
}

func matchOperator(source string) (string, bool) {
	for _, op := range []string{
		"<<=", ">>=", "->", "++", "--", "&&", "||", "==", "!=", "<=", ">=",
		"+=", "-=", "*=", "/=", "%=", "&=", "|=", "^=", "<<", ">>",
	} {
		if strings.HasPrefix(source, op) {
			return op, true
		}
	}
	if strings.ContainsRune("+-*/%&|^!~=<>", rune(source[0])) {
		return source[:1], true
	}
	return "", false
}

func advance(source string, i, line, column int) (int, int, int) {
	if source[i] == '\r' {
		i++
		if i < len(source) && source[i] == '\n' {
			i++
		}
		return i, line + 1, 1
	}
	if source[i] == '\n' {
		return i + 1, line + 1, 1
	}
	return i + 1, line, column + 1
}

func isSpace(c byte) bool {
	return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f'
}

func isDigit(c byte) bool { return c >= '0' && c <= '9' }
func isHexDigit(c byte) bool {
	return isDigit(c) || c >= 'a' && c <= 'f' || c >= 'A' && c <= 'F'
}
func isIdentifierStart(c byte) bool {
	return c == '_' || c >= 'a' && c <= 'z' || c >= 'A' && c <= 'Z'
}
func isIdentifierPart(c byte) bool {
	return isIdentifierStart(c) || isDigit(c)
}
