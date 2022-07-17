# lann

lann is an embeddable, dynamically typed programming language that focuses on its ease of use and simplicity, while minimizing RAM usage to a bare minimum (as of its latest version, lann can run on devices with as little as 768 bytes of RAM, probably less with some limitations). This, plus its minimal standard library requirements, allows it to run on almost every single computer and microcontroller out there, ranging from small ATtinys to modern Linux computers.

We (or rather, I) are slowly trying to add more safety protections to lann, so eventually it could be used anywhere without any safety concerns regarding memory access with pointers and buffer overflowing.

Note that currently lann is a project fully maintained by a single person, so slow progress is expected.

## Features

- Minimal RAM usage (only 768 bytes minimum).
- Fast and simple interpreter (no intermediate representations, code is ran as it's being lexed and parsed).
- Fully embeddable, minimal standard library requirements and 100% written in C.
- Built-in and custom heap allocator and stack for safe and sandboxed memory access.
- Function built-ins kept to a minimum (things like printf() and get_text() are optional).
- Extensively documented syntax and usage (see `lann.txt`, WIP).

## Example

```lann
let count: 0

while count < 100: begin
  printf("[]\n", count)
  let count: count + 1
end

array test: 6
str_copy(test, "world")

printf("hello, []!\n", test)

func pow: block begin
  let base: get(args, 0)
  let exp: get(args, 1)
  
  if exp = 0: give 1
  else if exp < 0: give 1 / pow(base, -exp)
  else: give pow(base, exp - 1) * base
end

printf("2 ** 14 = []\n", pow(2, 14))
```

## Building

lann is meant to be an embeddable library, composed of a source file(`lann.c`) and a header(`include/lann.h`), though it is bundled by default with a simple REPL and runner program. In order to build it, you may run `sh build.sh`.

After running that, an executable called `lann` should have showed up in the current directory.

## Testing and running

To enter the REPL prompt, run the program without any arguments: `./lann`. You should be greeted with a welcome message and a prompt like `> `. Then, you can start writing expressions and pressing enter to evaluate them. Note that expressions must be complete, you cannot leave an open begin-end block or parentheses. To exit, press Ctrl+C or call `exit()` with 0 or any return value as an argument.

To run a lann script stored in a file, run the program with that file as an argument: `./lann test.ln`. The program will immediately run and exit automatically when done.

## Embedding

Embedding lann into an existing project is as easy as copying the source and header files, `lann.c` and `include/lann.h`, and compiling them alongside the project.

## To-Do list

Here are some things you can help with (if you want, of course):

- Make division work with bigger numbers (and without using floats).
- Use strncpy and strncmp in str_copy and str_test so we don't get out of bounds accesses.
- Check out of bounds strings properly instead of just the first byte.
- Macros with macro-end blocks.

## Licensing

lann is licensed with the MIT license, check `LICENSE` for details.
