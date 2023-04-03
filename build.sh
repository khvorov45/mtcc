SCRIPT_DIR=$(dirname "$0")
RUN_BIN=$SCRIPT_DIR/build.exe
clang -g -Wall -Wextra -I/usr/lib/llvm-14/include $SCRIPT_DIR/build.c -o $RUN_BIN -lpthread -lclang && $RUN_BIN
