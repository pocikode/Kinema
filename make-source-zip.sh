#!/bin/bash
set -euo pipefail

OUTPUT_FILE="kinema-source.zip"
WORKING_DIR="$PWD"
TMPDIR=$(mktemp -d)
trap 'rm -rf "$TMPDIR"' EXIT

echo "Copying tracked repo files..."
git ls-files -z | while IFS= read -r -d '' file; do
    case "$file" in
        .serena/*|tools/*) continue ;;
    esac
    target="$TMPDIR/$file"
    mkdir -p "$(dirname "$target")"
    cp -a "$file" "$target"
done


echo "Copying external/geni submodule contents..."
if [ -d "external/geni" ]; then
    mkdir -p "$TMPDIR/external/geni"
    rsync -a \
      --exclude='.git' --exclude='.git/*' \
      --exclude='.serena' --exclude='.serena/*' \
      --exclude='tools' --exclude='tools/*' \
      "$WORKING_DIR/external/geni/" "$TMPDIR/external/geni/"
else
    echo "Warning: external/geni not found; archive may be incomplete." >&2
fi

echo "Creating $OUTPUT_FILE..."
rm -f "$OUTPUT_FILE"
(cd "$TMPDIR" && zip -r -q "$WORKING_DIR/$OUTPUT_FILE" .)

echo "Done: $WORKING_DIR/$OUTPUT_FILE"
echo ""
echo "Archive contents summary:"
unzip -l "$WORKING_DIR/$OUTPUT_FILE" | tail -6
