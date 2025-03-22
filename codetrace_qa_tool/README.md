Problem: 
When engineers make foundational changes to systems, it is often difficult for QA to "test" these changes.
The general sentiment is often "just continue testing everything".
Idea:
A program that takes, as input, a changelist/diff (from a VCS) and can show the user which lines of the new code 
they have "hit" during the run of a target program.

Imagine an engineer adds some new code, that does 1 of 3 things depending on user settings. 
The engineer submits this code, and QA is notified of the submission, and has the version control diff of the engineer's changes.
The QA tester pulls the new changed build, opens this tool and points the tool at the change diff.
The tool then starts the program, and shows a window that tells the QA tester "0%". Meaning they have not ran any of the code that the engineer changed yet. As the tester runs the program, it hits 1 of the 3 potential codepaths the engineer added. The tool would then show "33%" codepath coverage. It would reflect this in a view of the vcs diff by graying out the
parts of the code that the tester already ran. The tester could look at the other code that hasn't been run, and
potentially try to read the code to figure out how to get those pieces to run. Or, engineers could mark up their changes somehow (maybe like in the same tool they use for code review), and the tester could see those engineer notes on each of the changed parts that tell the tester what each part means and how to "hit" it (what settings to use, etc).


The backend tech behind this could be one of 2 ideas i've had:

- IPT (intel processor trace). 
    - Pros: very low and *constant* runtime overhead, completely software agnostic, no instrumentation needed
    - Cons: requires intel cpu, undocumented kernel driver, feels very "unsupported" in windows, potentially complex implementation needing ETW sideband events and a proper correlation library integrated into libipt

- DynamoRIO dynamic instrumentation
    - Pros: hardware agnostic, full control over all code (no 3rd party driver reliance)
    - Cons: Requires understanding of source code/symbols at some level - to instrument every changed branch. Bigger diffs may impact runtime performance