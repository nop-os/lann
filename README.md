# lann

<img align="left" width="140" height="140" src="https://raw.githubusercontent.com/nop-os/lann/master/lann.png">

lann is an embeddable, dynamically typed programming language that focuses on
its ease of use and simplicity, while minimizing RAM usage to a bare minimum.
This, plus its minimal standard library requirements, allows it to run on almost
every single computer and microcontroller out there, ranging from small ATtinys
to modern Linux computers.

## Features

- Working completely on less than 1 KiB of RAM and even on microcontrollers.
- Custom heap allocator and stack for safe and sandboxed memory access.
- Fast and simple direct interpreter with no intermediate representations.
- Fully embeddable, with no external dependencies and 100% written in C.
- Error handling with custom error types and built-in checks for OOB, etc.
- Function built-ins kept to a minimum and selectable, even printf is optional.
- Support for package installation to `/usr/share/lann` or equivalent.

## Example

```lann
printf("hello, []!\n", "world")

func pow block begin
  let base get(args, 0)
  let exp get(args, 1)
  
  if exp = 0 begin
    give 1
  end else if exp < 0 begin
    give 1 / pow(base, -exp)
  end else begin
    give pow(base, exp - 1) * base
  end
end

printf("2 ** 14 = []\n", pow(2, 14))
```

## Building and installing

To build lann on linux, run the shell script `build.sh`. This will generate two
binaries in `build/`, where you can run them from there, or you can install it
to `/usr/bin` by running the shell script `install.sh` as root.

## Running

To run a lann script, run the lann binary with the script path as its first
argument: `lann examples/bench.ln`. To enter the lann REPL prompt, run the lann
binary with no arguments.

Note that running it with no arguments will actually load the script at the path
`/usr/share/lann/repl.ln`, so this could be replaced with a customized version
if desired.

## Embedding

To embed lann into your own C project, copy `lann.c` and `include/lann.h` from
within the `lann` folder into the desired one. You may want to also copy fully
or partially `funcs.c` to provide implementations for standard I/O functions
and more.

You can check `main.c` inside the `lann` folder for reference.

## To-Do list

- Implement a fixed-point division algorithm instead of approximating it.
- Check OOBs properly in allocated memory regions.

## Licensing

lann is licensed with the MIT license, check `LICENSE` for details.
