package main

import "testing"

func TestConvertAnimationTextPreservesAtCmdLines(t *testing.T) {
	in := "anim idle\n" +
		"\t@cmd changeentityproperty getlocalvar(\"self\") \"velocity\" 0 1 0\n" +
		"@cmd shoot \"cape with space\" 21 13 0\n" +
		"@cmd clearL\n" +
		"@cmd clearL()\n"
	got, err := ConvertAnimationText(in)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if got != in {
		t.Fatalf("animation text was modified\ngot:  %q\nwant: %q", got, in)
	}
}

func TestOutputPathForInput(t *testing.T) {
	cases := []struct {
		input string
		want  string
	}{
		{input: "game/data/scripts/boss.c", want: "game/data/scripts/boss.lua"},
		{input: "game/data/chars/player/player.txt", want: "game/data/chars/player/player.lua"},
		{input: "script", want: "script.lua"},
		{input: "boss.C", want: "boss.lua"},
	}
	for _, tc := range cases {
		if got := outputPathForInput(tc.input); got != tc.want {
			t.Errorf("outputPathForInput(%q) = %q, want %q", tc.input, got, tc.want)
		}
	}
}
