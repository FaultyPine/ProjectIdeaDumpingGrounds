#include <iostream>
#include <string>
#include <vector>
#include <cmath>
#include <windows.h>
#include <stdlib.h>
#include <sstream>

// Helper function to find the minimum number of bits to represent a number
int minBits(int num) {
    return (num == 0) ? 1 : static_cast<int>(std::log2(num)) + 1;
}

// Function to count files and subdirectories recursively in a directory
int countItemsInDirectory(const std::string& directory) {
    WIN32_FIND_DATA findFileData;
    HANDLE hFind = FindFirstFile((directory + "\\*").c_str(), &findFileData);
    
    int itemCount = 0;

    if (hFind == INVALID_HANDLE_VALUE) {
        // Return 0 if the directory cannot be opened
        return 0;
    }

    do {
        // Skip the "." and ".." directories
        if (findFileData.cFileName[0] == '.') continue;

        // Check if the item is a directory or file
        //std::string itemPath = directory + "\\" + findFileData.cFileName;
        //if (findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            // It's a directory, count it
            //itemCount++;
        //} else {
            // It's a file, count it
            itemCount++;
        //}
    } while (FindNextFile(hFind, &findFileData) != 0);

    FindClose(hFind);
    return itemCount;
}



std::vector<bool> compressPath(const std::string& rootFolder, const std::string& targetPath) {
    // Split the target path into components (e.g., "Folder1\\Subfolder1\\file.txt")
    std::stringstream ss(targetPath);
    std::string segment;
    std::vector<std::string> targetParts;

    while (std::getline(ss, segment, '\\')) {
        targetParts.push_back(segment);
    }

    std::vector<bool> compressedPath;

    // Start from the root folder
    std::string currentPath = rootFolder;
    
    for (const std::string& part : targetParts) {

        int itemCount = countItemsInDirectory(currentPath);
        int bitsRequired = minBits(itemCount);

        // Add the necessary bits to the compressed path for this part
        // We need to know the position of 'part' within its parent folder
        WIN32_FIND_DATA findFileData;
        HANDLE hFind = FindFirstFile((currentPath + "\\*").c_str(), &findFileData);
        int index = 0;

        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                // Skip "." and ".." directories
                if (findFileData.cFileName[0] == '.') continue;
                
                // Check if the current part matches
                if (findFileData.cFileName == part) {
                    break;
                }
                index++; // Move to the next item in the directory
            } while (FindNextFile(hFind, &findFileData) != 0);

            // Close the handle after iteration
            FindClose(hFind);
        }

        // Encode the index using the minimum number of bits required
        for (int i = bitsRequired - 1; i >= 0; --i) {
            compressedPath.push_back((index >> i) & 1);
        }

        // Build the current path up to the next folder/file
        currentPath += "\\" + part;
    }

    return compressedPath;
}

std::vector<uint8_t> compressToBytes(const std::vector<bool>& compressedPath) {
    // Calculate the number of bytes required (round up to account for leftover bits)
    size_t numBytes = (compressedPath.size() + 7) / 8;  // Equivalent to ceil(compressedPath.size() / 8.0)
    
    // Pre-allocate the vector to avoid reallocations
    std::vector<uint8_t> compressedBytes;
    compressedBytes.reserve(numBytes); // Reserve enough space

    uint8_t byte = 0;
    int bitCount = 0;

    for (bool bit : compressedPath) {
        // Set the bit in the byte
        if (bit) {
            byte |= (1 << (7 - bitCount)); // Set bit at the correct position
        }
        
        bitCount++;

        // Every 8 bits, push the byte to the vector and reset the counter
        if (bitCount == 8) {
            compressedBytes.push_back(byte);
            byte = 0;
            bitCount = 0;
        }
    }

    // If there are leftover bits (less than 8 bits), we need to push the remaining byte
    if (bitCount > 0) {
        compressedBytes.push_back(byte);
    }

    return compressedBytes;
}

struct DirectoryTreeNode
{
    char nodeName[50];
    static constexpr size_t nodeNameSize() { return sizeof(nodeName); }
    inline bool Valid() const { return nodeName[0] != 0; }
};
struct DirectoryTreeStructure
{
    DirectoryTreeNode node = {};
    std::vector<DirectoryTreeStructure> children = {};
    inline bool Valid() const { return node.Valid(); }
};

struct SerializedDirectoryTreeStructure
{
    char rootFolder[MAX_PATH];
    DirectoryTreeStructure tree;
    bool Valid() { return rootFolder[0] != 0 && tree.node.Valid(); }
};

std::string getLastPathComponent(const std::string& path) {
    // Find the last occurrence of the path separator ('/' or '\').
    size_t pos = path.find_last_of("/\\");

    // If no separator is found, return the whole path (it might be just a filename).
    if (pos == std::string::npos) {
        return path;
    }

    // If the last character is a separator, remove it.
    if (pos == path.length() - 1) {
        std::string temp_path = path;
        temp_path.pop_back();
        pos = temp_path.find_last_of("/\\");
        if (pos == std::string::npos){
            return "";
        }
    }

    // Extract the substring after the last separator.
    return path.substr(pos + 1);
}

DirectoryTreeStructure generateFilesystemTree(const std::string& rootFolder)
{
    DirectoryTreeStructure result = {};
    std::string lastComponent = getLastPathComponent(rootFolder);
    if (lastComponent.size() > DirectoryTreeNode::nodeNameSize())
    {
        std::cout << "root folder component name too long\n" << rootFolder << "\n";
        exit(1);
    }
    memcpy(result.node.nodeName, lastComponent.data(), lastComponent.size());

    // Add the necessary bits to the compressed path for this part
    // We need to know the position of 'part' within its parent folder
    WIN32_FIND_DATA findFileData;
    HANDLE hFind = FindFirstFile((rootFolder + "\\*").c_str(), &findFileData);

    if (hFind != INVALID_HANDLE_VALUE) 
    {
        do 
        {
            // Skip "." and ".." directories
            if (findFileData.cFileName[0] == '.') continue;

            DirectoryTreeStructure thisiter = {};

            if (findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) 
            {
                std::string itemPath = rootFolder + "\\" + findFileData.cFileName;
                thisiter = generateFilesystemTree(itemPath);
            }
            else
            {
                const char* ss = findFileData.cFileName;
                size_t size = strlen(ss);
                // size_t size = wcstombs(NULL, ss, 0) + 1;
                if (size > DirectoryTreeNode::nodeNameSize())
                 {
                    std::cout << "file item name too long\n" << findFileData.cFileName << "\n";
                    exit(1);
                }
                // wcstombs(thisiter.node.nodeName, ss, size);
                memcpy(thisiter.node.nodeName, ss, size);

            }    

            result.children.push_back(thisiter);
        } while (FindNextFile(hFind, &findFileData) != 0);

        // Close the handle after iteration
        FindClose(hFind);
    }


    return result;
}
SerializedDirectoryTreeStructure generateSerializedFilesystemTree(const std::string& rootFolder)
{
    SerializedDirectoryTreeStructure result = {};
    memset(result.rootFolder, 0, MAX_PATH);
    if (rootFolder.size() < MAX_PATH)
    {
        memcpy(result.rootFolder, rootFolder.data(), rootFolder.size());
        result.tree = generateFilesystemTree(rootFolder);
    }
    return result;
}
// returns memory that the caller now owns
void* serializeCompressedDirectoryTree(const SerializedDirectoryTreeStructure& tree, size_t* outBufferSize = nullptr)
{
    auto moveBufCursor = [](std::vector<uint8_t>& buf, size_t amtToMove) -> void*
    {
        void* start = buf.data() + buf.size();
        for (size_t i = 0; i < amtToMove; i++)
        {
            buf.push_back(0);
        }
        return start;
    };
    #define ADD_TO_BUF(varptr, size) {\
        void* dst = moveBufCursor(outBuffer, size); \
        memcpy(dst, varptr, size);}

    std::vector<uint8_t> outBuffer = {};
    outBuffer.reserve(200);

    uint32_t rootfoldercstrsize = strnlen(tree.rootFolder, MAX_PATH)+1;
    ADD_TO_BUF(tree.rootFolder, rootfoldercstrsize);

    std::vector<const DirectoryTreeStructure*> structures = {};
    structures.push_back(&tree.tree);
    const DirectoryTreeStructure* structure = structures[0];
    // Serialized format looks like:
    // root folder name CString
    // Node name cstring
    // node number of children (64bit)
    //  child1 node name cstring
    //  child1 number of children
    //  child2 node name cstring
    //  child2 number of children
    //      potentially child2's child1 name
    //      potentially child2's child1 num children
    //      .......
    while (structure->Valid())
    {
        const DirectoryTreeNode* node = &structure->node;
        std::cout << "Serializing " << node->nodeName << "\n";
        size_t nodenamelen = strnlen(node->nodeName, node->nodeNameSize())+1;
        ADD_TO_BUF(node->nodeName, nodenamelen);

        size_t numChildren = structure->children.size();
        ADD_TO_BUF(&numChildren, sizeof(numChildren));

        for (size_t i = 0; i < numChildren; i++)
        {
            structures.push_back(&structure->children[i]);           
        }

        if (!structures.empty())
        {
            structures.erase(structures.begin());
            structure = structures.front();
        } else { break; }
    }

    void* result = malloc(outBuffer.size());
    if (result) memcpy(result, outBuffer.data(), outBuffer.size());
    if (outBufferSize) *outBufferSize = outBuffer.size();
    return result;
}

// TODO:
SerializedDirectoryTreeStructure deserializeDirectoryTreeFromBuffer(void* buffer)
{

}

std::string decompressPath(const std::vector<uint8_t> compressedPath)
{
    
}

int main() {
    // Root folder path (change this to your desired directory)
    std::string rootFolder = "C:\\Dev\\ProjectIdeaDumpingGrounds\\pathpacker\\testdir";
    // Path to the specific item inside the root folder (e.g., "Folder1\\Subfolder1\\file.txt")
    std::string targetPath = "Folder1\\Subfolder1\\test2.txt";
    
    SerializedDirectoryTreeStructure filesystemTree = generateSerializedFilesystemTree(rootFolder);

    // Compress the path structure into a vector of bits to the target item
    std::vector<bool> compressed = compressPath(rootFolder, targetPath);
    
    // Output the compressed path in binary format
    std::cout << "Compressed Path (in binary): ";
    for (bool bit : compressed) {
        std::cout << bit;
    }
    std::cout << std::endl;
    
    std::vector<uint8_t> pathCompressedBuffer = compressToBytes(compressed);

    std::cout << "Compressed path from " << targetPath.size() << " bytes, to " << pathCompressedBuffer.size() << " bytes. " << (float)pathCompressedBuffer.size() / (float)targetPath.size() << " compression ratio\n";

    size_t filesystemMetadataSize = 0;
    void* serializedBuffer = serializeCompressedDirectoryTree(filesystemTree, &filesystemMetadataSize);

    std::cout << "Filesystem metadata structure size: " << filesystemMetadataSize << " bytes\n";

    return 0;
}
