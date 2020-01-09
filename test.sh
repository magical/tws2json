set -eu

pass=1
for file in tests/*.tws; do
    json=${file%.tws}.json
    ./tws2json "$file" >"$json.output"
    if ! diff -u "$json.golden" "$json.output"; then
        pass=0
    fi
done

if [[ "$pass" = 1 ]]; then
    echo PASS
else
    exit 1
fi
