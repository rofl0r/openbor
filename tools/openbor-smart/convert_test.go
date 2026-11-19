package main

import (
	"os"
	"path/filepath"
	"strings"
	"testing"
)

func TestConvertCSourceFunctionsAndControlFlow(t *testing.T) {
	source := `void test(int player) {
	int health = 100;
	if (health > 0 && player != 0) {
		health -= 10;
	} else {
		health = 0;
	}
	for (int i = 0; i < 3; i++) {
		health = health + i;
	}
	return;
}`
	got, err := ConvertCSource(source)
	if err != nil { t.Fatalf("ConvertCSource failed: %v", err) }
	for _, want := range []string{
		"function test(player)",
		"local health = 100",
		"if __openbor_smart_truth(((health > 0) and (player ~= 0))) then",
		"health = health - (10)",
		"local i = 0",
		"while __openbor_smart_truth((i < 3)) do",
		"i = i + 1",
		"return",
	} {
		if !strings.Contains(got, want) { t.Errorf("converted Lua missing %q:\n%s", want, got) }
	}
}

func TestConvertCSourceTranslatesLegacyArrayIndexing(t *testing.T) {
	source := `void test() {
	int values[4];
	values[0] = 10;
	values[2] = values[0] + 1;
}`
	got, err := ConvertCSource(source)
	if err != nil { t.Fatalf("ConvertCSource failed: %v", err) }
	for _, want := range []string{"local values = {}", "values[(0) + 1] = 10", "values[(2) + 1] = (values[(0) + 1] + 1)"} {
		if !strings.Contains(got, want) { t.Errorf("converted Lua missing %q:\n%s", want, got) }
	}
}

func TestConvertCSourceTernaryAndDoWhile(t *testing.T) {
	source := `int choose(int value) {
	do { value--; } while (value > 10);
	return value > 0 ? value : 0;
}`
	got, err := ConvertCSource(source)
	if err != nil { t.Fatalf("ConvertCSource failed: %v", err) }
	if !strings.Contains(got, "until not __openbor_smart_truth((value > 10))") { t.Errorf("do-while conversion missing:\n%s", got) }
	if !strings.Contains(got, "if __openbor_smart_truth((value > 0)) then return value else return 0 end") { t.Errorf("ternary conversion missing:\n%s", got) }
}

func TestConvertCSourceRejectsUnsupportedPointerOperations(t *testing.T) {
	for _, source := range []string{
		`void test() { int *pointer; }`,
		`void test() { int value = sizeof(int); }`,
		`void test() { int value = *pointer; }`,
	} {
		if _, err := ConvertCSource(source); err == nil { t.Errorf("expected unsupported construct error for %q", source) }
	}
}

func TestConvertPathBatchRecursiveAndSafeOverwrite(t *testing.T) {
	root := t.TempDir()
	out := filepath.Join(t.TempDir(), "converted")
	if err := os.MkdirAll(filepath.Join(root, "nested"), 0o755); err != nil { t.Fatal(err) }
	if err := os.WriteFile(filepath.Join(root, "one.c"), []byte("void one() { return; }"), 0o644); err != nil { t.Fatal(err) }
	if err := os.WriteFile(filepath.Join(root, "nested", "two.c"), []byte("void two() { return; }"), 0o644); err != nil { t.Fatal(err) }
	if err := convertPath(root, out, true, false); err != nil { t.Fatalf("recursive batch failed: %v", err) }
	for _, path := range []string{filepath.Join(out, "one.lua"), filepath.Join(out, "nested", "two.lua")} {
		if _, err := os.Stat(path); err != nil { t.Errorf("expected output %s: %v", path, err) }
	}
	if err := convertPath(root, out, true, false); err == nil { t.Fatal("expected existing outputs to be protected") }
	if err := convertPath(root, out, true, true); err != nil { t.Fatalf("forced batch conversion failed: %v", err) }
}

func TestConvertCSourceSwitchCases(t *testing.T) {
	source := `void test(int state) {
	switch (state) {
	case 1: activate(); break;
	case 2: return;
	default: reset(); break;
	}
}`
	got, err := ConvertCSource(source)
	if err != nil { t.Fatalf("ConvertCSource failed: %v", err) }
	for _, want := range []string{
		"repeat",
		"if state == 1 then",
		"elseif state == 2 then",
		"else",
		"reset()",
		"until true",
	} {
		if !strings.Contains(got, want) { t.Errorf("converted Lua missing %q:\\n%s", want, got) }
	}
}

func TestConvertCSourceRejectsSwitchFallthrough(t *testing.T) {
	source := `void test(int state) {
	switch (state) { case 1: activate(); case 2: reset(); break; }
}`
	if _, err := ConvertCSource(source); err == nil { t.Fatal("expected switch fallthrough error") }
}
