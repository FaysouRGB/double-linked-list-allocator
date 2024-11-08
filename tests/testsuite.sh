#!/bin/sh

RED="\e[31m"
GREEN="\e[32m"
YELLOW="\e[33m"
DIM="\e[2m"
RESET="\e[m"

if [ ! -f "./libmalloc.so" ]; then
    printf "${RED}[TESTSUITE]${RESET} libmalloc.so not found."
    exit 1
fi

successful=0
total=0

run_test() {
    total=$((total + 1))
    LD_PRELOAD=./libmalloc.so "$@" > /dev/null 2>&1
    result=$?

    text=""
    color=""

    if [ $result -eq 0 ]; then
        text=" OK "
        color=$GREEN
        successful=$((successful + 1))
    else
        text="FAIL"
        color=$RED
    fi

    printf "│ ${color}%-4s${RESET} │ %-42s │\n" "$text" "$*"
}

# Run tests

printf "\n┌───────────────────────────────────────────────────┐\n"
printf "│ %-49s │\n" "Malloc Test Script"
printf "├──────┬────────────────────────────────────────────┤\n"
run_test factor 20 30 40 50 60 70 80 90
printf "├──────┼────────────────────────────────────────────┤\n"
run_test cat tests/testsuite.sh
printf "├──────┼────────────────────────────────────────────┤\n"
run_test ip a
printf "├──────┼────────────────────────────────────────────┤\n"
run_test ls
printf "├──────┼────────────────────────────────────────────┤\n"
run_test ls -la
printf "├──────┼────────────────────────────────────────────┤\n"
run_test find ../
printf "├──────┼────────────────────────────────────────────┤\n"
run_test tree ../
printf "├──────┼────────────────────────────────────────────┤\n"
run_test od ./libmalloc.so
printf "├──────┼────────────────────────────────────────────┤\n"
run_test git status
printf "├──────┼────────────────────────────────────────────┤\n"
run_test gimp --version
printf "├──────┼────────────────────────────────────────────┤\n"
run_test chromium --version
printf "├──────┼────────────────────────────────────────────┤\n"
run_test firefox --version

printf "├──────┴────────────────────────────────────────────┤\n"
printf "│ Total: %-2i / %2i Tests Successful                   │\n" "$successful" "$total"
printf "└───────────────────────────────────────────────────┘\n\n"
