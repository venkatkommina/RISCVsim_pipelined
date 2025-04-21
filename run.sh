#!/bin/bash

if [ "$1" == "compile" ]; then
    echo "🔧 Compiling..."
    cd src && make
    echo "✅ Compiled."
elif [ "$1" == "test" ]; then
    if [ -z "$2" ]; then
        echo "❌ Please provide a test filename."
    else
        ./bin/myRISCVSim test/"$2"
    fi
else
    echo "Usage:"
    echo "  ./run.sh compile              # to compile"
    echo "  ./run.sh test test2-auipc.mc # to run a test file"
fi
