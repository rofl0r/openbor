package main

import "testing"

func TestLexPreservesSourceAndClassifiesTokens(t *testing.T) {
	source := "void test() {\n  // comment\n  float x = 1.25e-2;\n  char *s = \"a\\\"b\";\n  self->health += 1;\n}"
	got, err := lex(source)
	if err != nil {
		t.Fatalf("lex failed: %v", err)
	}
	var rebuilt string
	for _, tok := range got {
		rebuilt += tok.text
	}
	if rebuilt != source {
		t.Fatalf("lexer did not preserve source\ngot:  %q\nwant: %q", rebuilt, source)
	}
	assertToken := func(text string, kind tokenKind) {
		t.Helper()
		for _, tok := range got {
			if tok.text == text && tok.kind == kind {
				return
			}
		}
		t.Errorf("missing token %q with kind %v", text, kind)
	}
	assertToken("void", tokenIdentifier)
	assertToken("// comment", tokenComment)
	assertToken("1.25e-2", tokenNumber)
	assertToken("\"a\\\"b\"", tokenString)
	assertToken("->", tokenOperator)
	assertToken("+=", tokenOperator)
}

func TestLexReportsUnterminatedLiterals(t *testing.T) {
	for _, source := range []string{"/* no end", "\"no end", "'x"} {
		if _, err := lex(source); err == nil {
			t.Errorf("expected an error for %q", source)
		}
	}
}

func TestLexTracksLineAndColumn(t *testing.T) {
	got, err := lex("a\r\nb")
	if err != nil {
		t.Fatal(err)
	}
	for _, tok := range got {
		if tok.text == "b" && (tok.line != 2 || tok.column != 1) {
			t.Fatalf("b location = %d:%d, want 2:1", tok.line, tok.column)
		}
	}
}

func TestLexNumericLiteralForms(t *testing.T) {
	cases := []string{
		"0x2A",
		"0x2AUL",
		"0777U",
		"42LL",
		"1.5f",
		".25",
		"1e-3",
		"0x1.fp+2",
		"0X.8P-1f",
	}
	for _, literal := range cases {
		t.Run(literal, func(t *testing.T) {
			got, err := lex(literal)
			if err != nil {
				t.Fatalf("lex failed: %v", err)
			}
			if len(got) != 1 || got[0].kind != tokenNumber || got[0].text != literal {
				t.Fatalf("tokens = %#v, want one number token %q", got, literal)
			}
		})
	}
}

func TestLexNumericLiteralBoundaries(t *testing.T) {
	cases := []struct {
		source string
		want   []string
	}{
		{"1+2", []string{"1", "+", "2"}},
		{"1e-3-2", []string{"1e-3", "-", "2"}},
		{"0xFFU+1", []string{"0xFFU", "+", "1"}},
		{".5*2", []string{".5", "*", "2"}},
		{"42ULL, name", []string{"42ULL", ",", " ", "name"}},
	}
	for _, tc := range cases {
		t.Run(tc.source, func(t *testing.T) {
			got, err := lex(tc.source)
			if err != nil {
				t.Fatalf("lex failed: %v", err)
			}
			if len(got) != len(tc.want) {
				t.Fatalf("token count = %d, want %d: %#v", len(got), len(tc.want), got)
			}
			for i, want := range tc.want {
				if got[i].text != want {
					t.Errorf("token %d = %q, want %q", i, got[i].text, want)
				}
			}
		})
	}
}
