#!/bin/bash

# arg 1 is the ELF file matching the core dump.
ELF_FILE=$1 

# arg2 is the core dump file (in Zephyr core dump format)
DMP_FILE=$2

#Make sure this path is correct
GDB='/opt/arm-gnu-toolchain-13.3.rel1-x86_64-arm-none-eabi/bin/arm-none-eabi-gdb'

# Verifying GDB path
if [ ! -f $GDB ]; then
    echo "GDB path not valid! Please update in zdump.sh"
	read -p "Press any key to exit ..."
	exit -1
fi

# Directory of this script. Below paths are relative to that...
APP_DIR="`dirname "$0"`"

ZEPHYR_COREDUMP_TOOL="$APP_DIR/coredump/coredump_gdbserver.py"

if [ ! -f $ZEPHYR_COREDUMP_TOOL ]; then
    echo "ZEPHYR_COREDUMP_TOOL path not valid! Please update in zdump.sh"
	read -p "Press any key to exit ..."
	exit -1
fi

GDB_COMMAND_FILE="$APP_DIR/gdb-commands.txt"

if [ ! -f $GDB_COMMAND_FILE ]; then
    echo "GDB_COMMAND_FILE path not valid! Please update in zdump.sh"
	read -p "Press any key to exit ..."
	exit -1
fi

# Ensuring python3 is installed
PYCHECK="$(python3 -V)" 
if [[ $PYCHECK =~ "Python 3" ]];
then
  echo "python3 detected."
else
 echo "Please install python3 and try again."
 read -p "Press any key to exit ..."
 exit -1
fi 

# if the Core Dump path contains a backslash, it is probably a windows path (Dispatcher on Windows, this script in WSL)
if [[ $DMP_FILE =~ "\\" ]];
then
  DMP_FILE="$(wslpath -a $DMP_FILE)" 
fi 

echo "ELF file: $ELF_FILE"
echo "Core Dump file: $DMP_FILE"

function usageAndExit {
    echo
	echo "zdump"
    echo "Shows core dumps in the Zephyr core dump format using gdb."
    echo "The gdb commands are specified in gdb-commands.txt. By default, this shows a stack trace and the registers." 	
	echo "Intended for Percepio DevAlert. Configure the Dispatcher tool to start this script."
	echo "Learn more about Zephyr core dumps at https://docs.zephyrproject.org/latest/services/debugging/coredump.html"
	echo "Learn more about Percepio DevAlert at https://percepio.com/devalert"
	echo
    echo "Usage:"
    echo "    $0 <ELF file> <Core Dump file>"
	echo "    If Dispatcher runs on Windows, use ' ' on Dispatcher variables, e.g. '${file}'."
    echo 
    echo "Copyright Percepio AB, 2024"    
	echo
    exit 1
}

[[ -z $ELF_FILE ]] && echo "No ELF file specified" && usageAndExit
[[ -z $DMP_FILE ]] && echo "No Core Dump file specified" && usageAndExit

echo "Starting GDB server..."
python3 $ZEPHYR_COREDUMP_TOOL $ELF_FILE $DMP_FILE &

echo "Starting GDB client..."
$GDB $ELF_FILE -x $GDB_COMMAND_FILE

read -p "Press any key to exit ..."