SCRIPT_DIR=$(dirname "$0")
RUN_BIN=$SCRIPT_DIR/tests.exe
clang -g -Wall -Wextra -I/usr/lib/llvm-14/include $SCRIPT_DIR/tests.c -o $RUN_BIN -lpthread && $RUN_BIN
