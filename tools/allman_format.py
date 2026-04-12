"""
Convert K&R brace style to Allman style in C++ project source files.
Skips External/ directories.
"""
import os
import re

ROOT = os.path.dirname(os.path.abspath(__file__))
SOURCE_DIR = os.path.join(ROOT, "source")

SKIP_DIRS = {"External", "dearimgui"}
EXTENSIONS = {".cpp", ".h", ".hpp"}

# Characters that, when immediately before ' {', indicate an initializer
# (not a control/function brace that should move to next line)
INITIALIZER_PREV_CHARS = {'=', ',', '(', '[', '{', '\\'}


def allman_convert(text):
    lines = text.splitlines(keepends=True)
    result = []

    for line in lines:
        stripped = line.rstrip("\r\n")
        ending = line[len(stripped):]  # the original line ending

        # Only process lines that end with '{'
        if not stripped.endswith('{'):
            result.append(line)
            continue

        # Get the part before the trailing '{'
        before_brace = stripped[:-1].rstrip()

        # Skip pure '{' lines (already on their own line)
        if not before_brace.strip():
            result.append(line)
            continue

        # Skip if the content (ignoring leading whitespace) starts with '//'
        # i.e. this is a commented-out line
        if before_brace.lstrip().startswith('//'):
            result.append(line)
            continue

        # Skip preprocessor lines
        if before_brace.lstrip().startswith('#'):
            result.append(line)
            continue

        # Skip if last non-space char before '{' indicates an initializer
        last_char = before_brace[-1] if before_brace else ''
        if last_char in INITIALIZER_PREV_CHARS:
            result.append(line)
            continue

        # Detect the indentation of the current line
        indent = re.match(r'^(\s*)', stripped).group(1)

        # Emit the line content without '{', then '{' on its own line
        result.append(indent + before_brace.lstrip() + ending)
        result.append(indent + '{' + ending)

    return "".join(result)


def process_file(path):
    with open(path, "r", encoding="utf-8", errors="replace") as f:
        original = f.read()

    converted = allman_convert(original)

    if converted != original:
        with open(path, "w", encoding="utf-8", newline="") as f:
            f.write(converted)
        return True
    return False


def main():
    changed = 0
    skipped = 0
    for dirpath, dirnames, filenames in os.walk(SOURCE_DIR):
        # Skip External directories in-place
        dirnames[:] = [d for d in dirnames if d not in SKIP_DIRS]

        for fname in filenames:
            ext = os.path.splitext(fname)[1].lower()
            if ext not in EXTENSIONS:
                continue
            fpath = os.path.join(dirpath, fname)
            if process_file(fpath):
                rel = os.path.relpath(fpath, ROOT)
                print(f"  formatted: {rel}")
                changed += 1
            else:
                skipped += 1

    print(f"\nDone. {changed} files changed, {skipped} files unchanged.")


if __name__ == "__main__":
    main()
