https://medium.com/@bhackett_4326


==========================================

Idea:
Parse program's import table, insert shims over system calls to my own dll. 
Record output during recording, return recorded data during replay

## Threads:
The big problem is during replay. If I want to scrub and analyze my replay at some arbitrary point in time,
there is no way to guarantee that the state of other threads will be the same as it was when the program first ran.
I.E. I set some breakpoint on a function during a replay, we play the program until that point, then stop all threads.
The state of the other threads at that point will be totally non-deterministic in a bad way. They could be in the middle of doing some
operation that changes some other state that I want to look at. 

This ^ is the real problem to solve for this kind of idea. Everything else is possible. rr solved this by forcing single-threaded. WinDbg solved it by fully emulating everything, and so they have full data about what all threads were doing at all points in time.

Potential solution ideas: 
- introduce "Ordered locks". This would *actually* make the program threads deterministic
- Intel PT cycle-accurate timing packets. 
    Records "cycle accurate" timings for all threads. CPU overhead is minimal, memory bandwidth overhead may be large.
    Considering memory bandwidth may already (depending on the traced program ofc) be tight from recording syscall buffers, this might be rough. Maybe not though...
    Though all the ipt literature says cyc timing packets may cause bandwidth problems, but that's also including all the other packets it generates.
    If i'm *only* concerned about timing packets, maybe i can turn off the other ones? Or maybe the other ones would still be useful...
    Intel PT + syscall hooks might be the winning combo here...

RE ^ IPT timing packets: 
even if i have timing info, during replay I can't control thread scheduling... so how would this help..?
Mmm i've been thinking about "replay" as just running the program normally but with the logged trace being substituted in at syscall sites and stuff
Maybe *replays* need to be totally emulated. Would be basically building a pseudo-x64win-VM (at a certain point, it probably would be a literal vm)
QEMU? 

Other forms of non-determinism not caught by the above:
- stuff like rdtsc/rdtscp/cpuid, memory mapped files being read in from page faults, reading from shared memory across processes
for these: i'd expose an API that would be an "explicit shim" on those things. So instead of raw-calling the rdtsc intrinsic, you'd call the library's rdtsc func.
    - alternative, more involved approach - scan the executable .text for those specific instructions, and hook those instructions... somehow... (would need some builtin scratch space to do a trampoline to)

For mmap-ed stuff, idk.. maybe have a shim on memmapping it (MapViewOfFile/OpenFileMapping), and set up my own page fault handler, so when you read in a page, 
it hits my handler, we read the actual page in, then record that in the handler and pass it back to the user. 
For shared process memory ipc stuff, maybe another custom pagefault handler?
Both of these are probably really slow though, so maybe just an exposed API to the user for reading from these mapped buffers would be best here.


malloc non-deterministic pointers. 
- (at least for now...) ignore this! If your program depends on pointer values, you're crazy! Lol. 
    - for real though, some programs might use pointer values for stuff like random hashes. In those cases, another explicit recording API might be the solution
    - disabling ASLR for recording & replaying might be reasonable? Idk how that would impact stuff like game's anti-cheat...
    - hook malloc/VirtualAlloc/HeapAlloc and use an internal deterministic algorithm?
    - record malloc output and try to force-allocate (VirtualAlloc address param) at that virtual address during replays




long term:
- tag calls or even scopes to be excluded from recording. On replay, these excluded parts will have their real system calls called.
    - extend this with the concept of "readonly persistent files", where reads to some files or even network endpoints can be *assumed* to not change, 
        allowing us to not record that data, and during replay, to read in the file normally, reducing replay data size. Think game assets - huge file reads, rarely changes
    - could be useful for parts of your program you don't care about replaying that you know would work fine in a replay senario
- analysis of recorded sessions. This is where it gets interesting... https://pernos.co/
    - personal gush: i haven't really used pernosco, but holy moly if windows developers had access to that kind of tech with low-overhead recording like rr life would be so good





extra random thoughts:

points that got brought up as to why this concept might not work:
That the WinDbg team has access to windows source,
and yet they still chose emulation for their TTD implementation. I think this isn't because this method is not doable, but because
they wanted to implement it without needing source modification. I don't have that restriction.

