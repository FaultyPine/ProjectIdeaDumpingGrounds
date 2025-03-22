A filepath compression scheme

concept stolen from guerrilla game's data pipelines talk where they do this


a path might look like

"Folder1/Subfolder/file.txt"
but if we know the number of files/folders in each folder, we can compress this path representation with variable-length bit packing

imagine the folder structure:

```
Folder1 -
--------- Subfolder
                    |
                    ------- Subsubfolder
                    ------- file.txt
--------- Subfolder2
                    |
                    ------ anotherfile.txt
--------- somefile.txt
Folder2 - 
```

Folder1 contains 3 things: (Subfolder, Subfolder2, and somefile.txt)
Subfolder contains 2 things: (subsubfolder, file.txt)
Subfolder2 contains 1 thing. (anotherfile.txt)
Subsubfolder contains 0 things.

In this case, the representation of 
"Folder1/Subfolder/file.txt"
Can be compressed like so: 0 00 1   with only 4 bits!

In this example, we assume Folder1 and Folder2 reside in some "current relative directory", so we can index that whole directory with only 1 bit
Then, inside Folder1, there are 3 items, which can be represented by 2 bits.
Then, inside Subfolder, there are another 2 items, which is represented with another bit.

Using variable-length bit packing like this, we can compress paths a lot!

Though, one consideration/gotcha is that this path data is easily invalidated when another folder/file is added.
