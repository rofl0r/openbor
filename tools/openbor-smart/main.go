package main

import (
	"flag"
	"fmt"
	"io/fs"
	"os"
	"path/filepath"
	"strings"
)

// ConvertAnimationText deliberately preserves animation text verbatim.
// Legacy @cmd syntax is interpreted by the OpenBOR Lua engine at load time.
func ConvertAnimationText(input string) (string, error) {
	return input, nil
}

func outputPathForInput(inputPath string) string {
	ext := filepath.Ext(inputPath)
	if ext == "" { return inputPath + ".lua" }
	return strings.TrimSuffix(inputPath, ext) + ".lua"
}

func convertFile(inputPath, outputPath string, force bool) error {
	data, err := os.ReadFile(inputPath)
	if err != nil { return fmt.Errorf("read %s: %w", inputPath, err) }
	converted, err := ConvertCSource(string(data))
	if err != nil { return fmt.Errorf("convert %s: %w", inputPath, err) }
	if !force {
		if _, err := os.Stat(outputPath); err == nil {
			return fmt.Errorf("refusing to overwrite existing file %s (use -force)", outputPath)
		} else if !os.IsNotExist(err) {
			return fmt.Errorf("check output %s: %w", outputPath, err)
		}
	}
	if err := os.MkdirAll(filepath.Dir(outputPath), 0o755); err != nil {
		return fmt.Errorf("create output directory for %s: %w", outputPath, err)
	}
	if err := os.WriteFile(outputPath, []byte(converted), 0o644); err != nil {
		return fmt.Errorf("write %s: %w", outputPath, err)
	}
	return nil
}

func convertPath(inputPath, outputDir string, recursive, force bool) error {
	info, err := os.Stat(inputPath)
	if err != nil { return fmt.Errorf("stat input: %w", err) }
	if !info.IsDir() {
		if !strings.EqualFold(filepath.Ext(inputPath), ".c") {
			return fmt.Errorf("input must be a .c file or a directory containing .c files")
		}
		output := outputPathForInput(inputPath)
		if outputDir != "" { output = filepath.Join(outputDir, filepath.Base(output)) }
		if err := convertFile(inputPath, output, force); err != nil { return err }
		fmt.Printf("Converted %s -> %s\n", inputPath, output)
		return nil
	}

	root, err := filepath.Abs(inputPath)
	if err != nil { return err }
	var files []string
	err = filepath.WalkDir(root, func(path string, entry fs.DirEntry, walkErr error) error {
		if walkErr != nil { return walkErr }
		if path == root { return nil }
		rel, err := filepath.Rel(root, path)
		if err != nil { return err }
		if entry.IsDir() {
			if !recursive && strings.Contains(rel, string(filepath.Separator)) { return filepath.SkipDir }
			if !recursive { return filepath.SkipDir }
			return nil
		}
		if strings.EqualFold(filepath.Ext(entry.Name()), ".c") { files = append(files, path) }
		return nil
	})
	if err != nil { return fmt.Errorf("scan input directory: %w", err) }
	if len(files) == 0 { return fmt.Errorf("no .c files found in %s", inputPath) }

	converted, failed := 0, 0
	var failures []string
	for _, file := range files {
		output := outputPathForInput(file)
		if outputDir != "" {
			rel, err := filepath.Rel(root, file)
			if err != nil { return err }
			output = filepath.Join(outputDir, outputPathForInput(rel))
		}
		if err := convertFile(file, output, force); err != nil {
			failed++
			failures = append(failures, err.Error())
			fmt.Fprintln(os.Stderr, "FAILED:", err)
			continue
		}
		converted++
		fmt.Printf("Converted %s -> %s\n", file, output)
	}
	fmt.Printf("Batch complete: %d converted, %d failed\n", converted, failed)
	if failed > 0 { return fmt.Errorf("%d file(s) failed conversion", failed) }
	return nil
}

func run() error {
	inputPath := flag.String("in", "", "input .c file or directory")
	outputDir := flag.String("out", "", "optional output directory (preserves subdirectories for folder input)")
	recursive := flag.Bool("recursive", false, "include .c files in subdirectories")
	force := flag.Bool("force", false, "overwrite existing .lua output files")
	flag.Parse()
	if *inputPath == "" {
		return fmt.Errorf("usage: openbor-smart -in path/to/script.c | -in path/to/scripts -recursive")
	}
	return convertPath(*inputPath, *outputDir, *recursive, *force)
}

func main() {
	if err := run(); err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}
}
