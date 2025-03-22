https://github.com/FaultyPine/binmerge

Idea: would be cool to not need an intermediate source file for game engines. Some engines use xml, or yaml, or whatever to represent assets, 
which then get "Compiled" for actual use in the runtime/shipping client. It would be a reasonable speedup to not need to parse those intermediate file formats, 
and instead just have the binary files themselves be the authoring AND shipping file format.

The main argument against this is source control. You can't do arbitrary merges/diffs of opaque binary file formats. 
If this was solved, I can't really think of any other big downsides to just using the binary file formats, and would speed up/simplify engine loading a fair bit.

To facilitate this idea, I want to make a tool that can merge binary file formats for version control systems. 
To do that, we need some info about the file format. This info is typically already represented in code through structure definitions. 
Those structure definitions are available through debugging information emitted by compilers. 
The idea here is to take in data about the structure of a file format from the code itself (example lib that uses the same idea: https://github.com/jam1garner/binrw) 
and use that data to properly do merge/diffs of arbitrary binary file formats given their internal structure.
Potential other avenue to get the format data - clang plugin.

Work todo:

proper thought path: we are merging entire File format layouts. 
Before i was thinking of merging multiple copies of the same structure layout. THis isn't the case.
The base might have 3 fields, the local might have removed one, and the remote might have added and changed one.
All of these changes should be properly reconciled.

merge/diff hardcoded binary file format
- merging a single field modification
- merging a single field addition
- merging a single field removal
- merging field reordering
- merging combinations of the above
hardcoded format -> merge/diff based on some sort of "file format spec"
derive the "file format spec" from program debug info

another possible (much simpler) approach would be to have the merge granularity be at a "struct" level, rather than field level
so two changes to the *same depth* struct cannot be merged.
but if you had
struct Data
{
    int y;
    struct SubData
    {
        int x;
    }
    SubData subdata;
}
Someone can make changes to "y" and to data inside "subdata" independently without affecting each other. Merges would always be correct with this approach.


I'm realizing now that merging as i'm doing it right now might be wasted effort.
Instead of reading in the structure info for base/both revisions, then doing the diff on those structures internally,
I could just do a normal textual diff on the formats (since i'd likely be reading them in through textual representations anyway)
then take that resulting merged text which represents the structure of the merged file format, and somehow
convert the base format binary data to the merged one. <- idk how that would work...

Brainstorming - how would that work?
if i have binary data for the base revision, and the base revision's file format, and a merged file format,
how would i produce the merged binary data?
for reordered, removed, and modified fields, this isn't hard.
Iterate through merged fields, if the field exists, or did exist under a different name in the base revision, copy the binary data in.
For newly added fields..... idk....
Since they didn't exist in the base revision there's no data to copy over to the merged version. I guess in this case we just look at the size of
the new field and fill it with 0s?
Or wait no... maybe we *would* actually have *both* binary data as well as the file format representation for all revisions.
If that's the case, then this entire program becomes way way way simpler to implement!


so the new flow of this idea would be:
- get format representations and binary blobs for all 3 revisions (base/local/remote)
- do a normal textual 3 way merge on the format representations (kdiff3? could use any normal merge tool...)
- get the resulting textual merged format representation
- transform the base binary data to the final merged binary data using the binary data from the other 2 revisions

more specifically:
clang plugin generates textual representation of file formats.
We put those representations somewhere that's easy to access later - i.e. in some appdata folder or something by default,
or users can override it with an env variable or explicit path.
Then, when merging, if the file extension is one we care about (users might have to manually set that part up?)
the files being "merged" would in theory be the binary files themselves, so we already have those.
We would then need some way of getting the base and both revision format representations somehow... 





perforce integration:
%1 source (theirs)
%2 destination (yours)
%b base
%r result


Resources:

"Bill Ritcher's excellent paper "A Trustworthy 3-Way Merge" talks about some of the common gotchas with three way merging and clever solutions to them that commercial SCM packages have used."
https://www.guiffy.com/SureMergeWP.html


There's a formal analysis of the diff3 algorithm, with pseudocode, in this paper: http://www.cis.upenn.edu/~bcpierce/papers/diff3-short.pdf
It is titled "A Formal Investigation of Diff3" and written by Sanjeev Khanna, Keshav Kunal, and Benjamin C. Pierce from Yahoo.


https://github.com/jmacd/xdelta/tree/release3_1_apl/xdelta3
https://meta-diff.sourceforge.net/
https://github.com/jam1garner/binrw // example of a good simple user-facing api. Just define your struct and that's it.
https://homes.cs.washington.edu/~mernst/pubs/merge-evaluation-ase2024.pdf
https://github.com/GumTreeDiff/gumtree


