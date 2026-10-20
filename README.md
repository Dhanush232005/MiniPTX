
# MiniPTX — Arithmetic-to-PTX Compiler

MiniPTX is an educational compiler written in C++17. It parses
integer arithmetic expressions and generates NVIDIA PTX assembly
for a small supported language.

## Features

- Lexical analysis and tokenization
- Recursive descent parser
- Abstract syntax tree (AST)
- Operator precedence and parentheses
- PTX instruction generation for integer arithmetic
- Error reporting for invalid syntax
- PowerShell tests for compiler output and invalid expressions

## Example

Input:

```text
2 + 3 * 4
```

MiniPTX builds an AST equivalent to:

```text
    +
   / \
  2   *
     / \
    3   4
```

It emits PTX instructions for multiplication followed by addition,
then stores the result through a kernel output pointer.

## Build on Windows

Open Developer PowerShell for Visual Studio in the project folder.

```powershell
cl /nologo /std:c++17 /EHsc /W4 /permissive- src\main.cpp /Fe:miniptx.exe
```

## Run

```powershell
.\miniptx.exe "2 + 3 * 4"
.\miniptx.exe "(2 + 3) * 4"
```

The program writes PTX assembly to standard output.

## Test

```powershell
powershell -ExecutionPolicy Bypass -File tests\run_tests.ps1
```

## Optional PTX validation

If the NVIDIA CUDA Toolkit and `ptxas` are installed:

```powershell
.\miniptx.exe "2 + 3 * 4" > tests\generated.ptx
ptxas -arch=sm_52 tests\generated.ptx -o tests\generated.cubin
```

## Architecture

1. Lexer: source characters become tokens.
2. Parser: tokens become an abstract syntax tree.
3. PTX emitter: AST operations become PTX instructions.

## Scope and limitations

This is a learning project, not a replacement for NVIDIA PTXAS.
It supports a small arithmetic language with integer constants,
unary minus, addition, subtraction, multiplication and parentheses.

It does not yet implement a general-purpose language, register
allocation, a GPU host launcher, a full optimization pipeline or
complete compiler debug information.

The emitted PTX must be validated with NVIDIA's assembler before
claiming assembler compatibility. Actual GPU execution requires a
host launcher and compatible hardware.

## Future improvements

- Constant folding and dead-code elimination
- Source-location tracking and diagnostics
- More comprehensive arithmetic and overflow tests
- GPU runtime execution
- Separate source modules and symbol resolution

## Technologies

C++17, Microsoft Visual C++, NVIDIA PTX ISA
