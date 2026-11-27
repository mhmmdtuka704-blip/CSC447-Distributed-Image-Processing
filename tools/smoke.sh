#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")/.."

temporary=$(mktemp -d)
trap 'rm -r -- "$temporary"' EXIT
launcher=(mpirun --map-by slot --bind-to none)
if (( EUID == 0 )); then launcher+=(--allow-run-as-root); fi

make -j
python3 tools/prepare.py "$temporary/input" --sizes 4
manifest="$temporary/input/batch-4.txt"
./build/sequential "$manifest" "$temporary/serial" 161 103 18
for ranks in 2 4 8; do
    "${launcher[@]}" -np "$ranks" ./build/distributed "$manifest" "$temporary/mpi$ranks" 161 103 18
    python3 tools/verify.py "$temporary/serial" "$temporary/mpi$ranks" 4 161 103
done

# Two entries for the same file are separate jobs. A missing job leaves its ID vacant.
printf '%s\n' images/input-001.png missing.png images/input-001.png > "$temporary/input/fail.txt"
set +e
"${launcher[@]}" -np 4 ./build/distributed "$temporary/input/fail.txt" "$temporary/partial" 1 1 -255 \
    > "$temporary/fail.log" 2>&1
status=$?
set -e
[[ "$status" == 1 ]]
grep -qF 'Job 2' "$temporary/fail.log"
grep -qF 'jobs=3 completed=2 ranks=4' "$temporary/fail.log"
[[ $(find "$temporary/partial" -maxdepth 1 -type f | wc -l) -eq 2 ]]
[[ -f "$temporary/partial/job-00001.png" && -f "$temporary/partial/job-00003.png" ]]
cmp "$temporary/partial/job-00001.png" "$temporary/partial/job-00003.png"
printf 'PASS: missing input reported, two distinct IDs saved, workers stopped cleanly\n'
