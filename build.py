#!/usr/bin/env python3

import os
import math
import shutil
import difflib
import subprocess
import sys
import contextlib
from pathlib import Path

@contextlib.contextmanager
def cd(new_dir):
    previous_dir = os.getcwd()
    os.chdir(new_dir)
    try:
        yield
    finally:
        os.chdir(previous_dir)

class StackState:
    BATCH_ONLY = 0
    SH_ONLY = 1
    IF_STATEMENT = 2

    def __init__(self, const, indent):
        self.const = const
        self.indent = indent

stack = []

def get_filter_state():
    for i in stack:
        if (i.const == StackState.BATCH_ONLY or i.const == StackState.SH_ONLY): return i

def silent_system_stdout(command):
    p = subprocess.Popen(command, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    return p.stdout.read()

def silent_system_call(command):
    p = subprocess.Popen(command, shell=True, stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT)
    return p.returncode

def system_stdout(command):
    p = subprocess.Popen(command, shell=True)
    return p.returncode

def which(arg):
    return shutil.which(arg)

def exists(arg):
    return os.path.exists(arg)
        
def rm(arg):
    try:
        shutil.rmtree(arg)
        return 1
    except OSError as e:
        print(e)
        return 0

def cp(src, dest):
    try:
        shutil.copy(src, dest)
        return 1
    except OSError as e:
        print(e)
        return 0

def mkdir(arg):
    return os.mkdir(arg)

def arg(i):
    if argc > i:
        return argv[i]

def get_line_indent(line):
    for i in range(len(line)):
        if line[i] not in (" ", "\t"):
            return i // 4 # python convention is 4 spaces?

def print_error(msg):
    print(f"\x1b[31m{msg}\x1b[0m")

def print_success(msg):
    print(f"\x1b[32m{msg}\x1b[0m")

preprocessed_code = []

script_dir = os.path.dirname(os.path.realpath(__file__)).replace("\\", "/")
sh_shells = ("/bin/zsh", "/bin/bash")

def print_numbered_lines(code):
    lines = code.split("\n")
    number_space = int(math.log10(len(lines))) + 1
    for line_i in range(len(lines)):
        print(f"\x1b[33m{line_i + 1:<{number_space}}\x1b[0m {lines[line_i]}")

argv = [script for script in sys.argv[1:] if script != "-c" and script != "--code"]
argc = len(argv)

script = Path(__file__).with_name("build.psi")
if not script.is_file():
    print_error("[PSI]: build.psi not found! Please insert this file in the same directory as build.py");
    exit()

with open(script, "r", encoding="utf-8") as f:
	for line in f:
		command = line.strip()
		if command == '"""bat':
			stack.append(StackState(StackState.BATCH_ONLY, get_line_indent(line)))
		elif command == '"""sh':
			stack.append(StackState(StackState.SH_ONLY, get_line_indent(line)))
		elif command == '"""':
			if (stack):
				stack.pop()
			else:
				print("Missing block starting statement '\"\"\"(shell)'!")
				break
		elif command and command[0] == "#":
			pass
		else:
			filter_state = get_filter_state()
			if (os.name == "posix" and os.getenv("SHELL") in sh_shells and filter_state and filter_state.const == StackState.SH_ONLY or
			os.name == "nt" and filter_state and filter_state.const == StackState.BATCH_ONLY):
				left_trail = "    " * filter_state.indent
				preprocessed_code.append(f"{left_trail}if error_code := os.system(f'{command}'): raise Exception(error_code)\n")
			elif not filter_state or filter_state and filter_state.const not in (StackState.SH_ONLY, StackState.BATCH_ONLY):
				preprocessed_code.append(line)

	code = "".join(preprocessed_code)
	if "--code" in sys.argv or "-c" in sys.argv:
		print_numbered_lines(code)
	else:
		exec(code)

