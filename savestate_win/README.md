


uses GetWriteWatch to implement program-agnostic savestates


on state save:
- reset write watch
- record all open file handles - specifically recording a mapping between the handle and the path of the file
    specifically thinking of case where you have a file handle open, save, then the handle closes and you load that save
    in that case, the handle would be invalid. But we could introduce our own indirection by hooking file operations
    and making sure that old, invalid handles are re-opened with the correct path that we saved
    https://www.geoffchappell.com/studies/windows/km/ntoskrnl/api/ex/sysinfo/handle.htm
- might need to do something similar to ^ for stuff like network sockets, threads, etc
- ignoring vram (for now)


on state load:
- get written pages...
    (thinking of case where you have allocated memory, then save, then the mem is freed and you load the state)
    look through all those pages and if current page is unmapped/protected, need to re-reserve it and copy the old memory back in
- restore old file handles
- restore old thread states
- restore old network handles



the gpu makes all this much more complex. Would need another system to handle all the memory/state there. Honestly I don't know if it's even feasible to do this kind of thing with a gpu.

