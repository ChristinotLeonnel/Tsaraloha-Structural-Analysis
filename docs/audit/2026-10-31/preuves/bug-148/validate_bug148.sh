#!/usr/bin/env bash
# Validation complète de la correction de BUG-148, sans fenêtre, après compilation Release.
SP="/c/Users/TSARAL~1/AppData/Local/Temp/claude/d--Tsaraloha-Platforme-TSA/84fdaea9-ba6e-422a-80ce-1f048ba5db29/scratchpad"
WIN=$(cygpath -w "$SP")
cd "$SP" || exit 1

echo "=== suites"
python scripts/run_hidden_desktop.py "$WIN\\b148-tests.console.txt" "D:\\Tsaraloha Platforme\\TSA" python "tools/ci/run_test_suites.py" \
    --exe "build-windows-validation/TSA_TestSuite.exe" --out "$WIN\\b148-tests" --available opensees,gmsh,ifcopenshell,display --timeout 600
grep -E "^Suites|ÉCHOUÉE|NON EXÉCUTÉE" "$SP/b148-tests.console.txt" | cut -c1-120
grep -E "Tests réussis" "$SP/b148-tests/report.md"

echo "=== harnais"
python scripts/make_harness_cmd.py "D:/Tsaraloha Platforme/TSA/build-windows-validation" "$WIN\\harness" > /dev/null
python -c "
import subprocess
r = subprocess.run(['cmd', '/c', r'$WIN\\harness\\build.cmd'], stdin=subprocess.DEVNULL, capture_output=True, creationflags=0x08000000)
print('harnais code', r.returncode)
"

echo "=== empreinte des exemples"
python scripts/dump_all.py "$WIN\\harness\\harness.exe" "$WIN\\b148-dump" > "$SP/b148-dump.log" 2>&1
python scripts/compare_dumps.py dump-apres b148-dump | tee "$SP/b148-dump-compare.txt" | tail -12

echo "=== bancs"
for b in official:"D:\\Tsaraloha Platforme\\TSA\\tests\\benchmarks" random:"$WIN\\cases-random" hinge:"$WIN\\cases-hinge" more:"$WIN\\cases-more" nl:"$WIN\\cases-nl" units:"$WIN\\cases-units"; do
    name=${b%%:*}; cases=${b#*:}
    python scripts/hidden_bench.py "$cases" "$WIN\\b148-bench-$name" "$WIN\\b148-bench-$name-work" "$WIN\\b148-bench-$name.console.txt" > /dev/null
    echo "$name : $(tail -1 "$SP/b148-bench-$name.console.txt" | sed 's/ — rapport.*//')"
done

echo "=== comparaison des bancs (avant correction -> après)"
for n in official random hinge more nl units; do
    echo "== $n"
    python scripts/compare_bench.py apres-bench-$n/latest.json b148-bench-$n/latest.json | cut -c1-170 | head -12
done
echo "=== fin de validation"
