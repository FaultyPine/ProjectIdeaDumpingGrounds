Idk if this is already how some debuggers do this, or if this is a novel idea


How I imagine today's slow terrible debuggers do conditional breakpoints is to trap normally, then 
evaluate the user's expression with a necessarily slow expression parser/pseudo-compiler and execute that using the running process's memory in the evaluation.

A faster way to do it:
you aren't just limited to "conditions" in conditional breakpoints, you can fully just execute code
Basically a dumber inline Live++

You set a breakpoint on a line, then can write ANY code there
You get the compilation info from the running process like Live++ (very complicated...)
then inserts a jump in the running process to this new tiny program.
Notably, this wouldn't be a function call, just a straight jump so your code gets
access to all stack/whatever as if the code you wrote was directly inline in the original code.

That way, you could interface with your entire program in ""conditional breakpoints"" (not limited to whatever expression
evaluator the debugger implements), and you get fully native performance when evaluating a conditional that decides if it should trap the program or not

