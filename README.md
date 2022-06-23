# lann

lann is an embeddable, dynamically typed programming language that focuses on its ease of use and simplicity, while minimizing RAM usage to a bare minimum (lann can run on devices with as little as 384 bytes of RAM, probably less with some limitations). This, plus its minimal standard library requirements, allows it to run on almost every single computer and microcontroller out there, ranging from small ATtinys to modern Linux computers.

We (or rather, I) are slowly trying to add more safety protections to lann, so eventually it could be used anywhere without any safety concerns regarding memory access with pointers and buffer overflowing.

Note that currently lann is a project fully maintained by a single person, so slow progress is expected.

## Building

lann is meant to be an embeddable library, composed of a source file(`lann.c`) and a header(`include/lann.h`), though it is bundled by default with a simple REPL and runner program. In order to build it, you may run `sh build.sh`.

After running that, an executable called `lann` should have showed up in the current directory.

## Testing and running

To enter the REPL prompt, run the program without any arguments: `./lann`. You should be greeted with a welcome message and a prompt like `> `. Then, you can start writing expressions and pressing enter to evaluate them. Note that expressions must be complete, you cannot leave an open begin-end block or parentheses. To exit, press Ctrl+C or call `exit()` with 0 or any return value as an argument.

To run a lann script stored in a file, run the program with that file as an argument: `./lann test.ln`. The program will immediately run and exit automatically when done.

## 

## Licensing

lann is licensed with the MIT license, check `LICENSE` for details.
