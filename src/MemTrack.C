/*
Copyright (c) 2002, 2008 Curtis Bartley
All rights reserved.
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

- Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.

- Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in the
documentation and/or other materials provided with the
distribution.

- Neither the name of Curtis Bartley nor the names of any other
contributors may be used to endorse or promote products derived
from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
OF THE POSSIBILITY OF SUCH DAMAGE.
*/


#ifdef DEBUG_MEMTRACK
#include <MemTrack.h>

#include <Base.h>
#include <typeinfo>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <algorithm>

#undef new    // IMPORTANT!

     /* ------------------------------------------------------------ */
     /* -------------------- namespace MemTrack -------------------- */
     /* ------------------------------------------------------------ */

namespace MemTrack
{

    /* ------------------------------------------------------------ */
    /* --------------------- class BlockHeader -------------------- */
    /* ------------------------------------------------------------ */

    class BlockHeader
    {
    private:    // static member variables
        static BlockHeader *ourFirstNode;
    
    private:    // member variables
        BlockHeader *myPrevNode;
        BlockHeader *myNextNode;
        size_t myRequestedSize;
        char const *myFilename;
        int myLineNum;
        char const *myTypeName;

    public:     // members
        BlockHeader(size_t requestedSize);
        ~BlockHeader();
        
        size_t GetRequestedSize() const { return myRequestedSize; }
        char const *GetFilename() const { return myFilename; }
        int GetLineNum() const { return myLineNum; }
        char const *GetTypeName() const { return myTypeName; }
        
        void Stamp(char const *filename, int lineNum, char const *typeName);
        
        static void AddNode(BlockHeader *node);
        static void RemoveNode(BlockHeader *node);
        static size_t CountBlocks();
        static void GetBlocks(BlockHeader **blockHeaderPP);
        static bool TypeGreaterThan(BlockHeader *header1, BlockHeader *header2);
    };

    /* ---------------------------------------- BlockHeader static member variables */

    BlockHeader *BlockHeader::ourFirstNode = NULL;

    /* ---------------------------------------- BlockHeader constructor */

    BlockHeader::BlockHeader(size_t requestedSize)
    {
        myPrevNode = NULL;
        myNextNode = NULL;
        myRequestedSize = requestedSize;
        myFilename = "[unknown]";
        myLineNum = 0;
        myTypeName = "[unknown]";
    }

    /* ---------------------------------------- BlockHeader destructor */

    BlockHeader::~BlockHeader()
    {
    }
        
    /* ---------------------------------------- BlockHeader Stamp */

    void BlockHeader::Stamp(char const *filename, int lineNum, char const *typeName)
    {
        myFilename = filename;
        myLineNum = lineNum;
        myTypeName = typeName;
    }

    /* ---------------------------------------- BlockHeader AddNode */

    void BlockHeader::AddNode(BlockHeader *node)
    {
        assert(node != NULL);
        assert(node->myPrevNode == NULL);
        assert(node->myNextNode == NULL);

        // If we have at least one node in the list ...        
        if (ourFirstNode != NULL)
            {
                // ... make the new node the first node's predecessor.
                assert(ourFirstNode->myPrevNode == NULL);
                ourFirstNode->myPrevNode = node;
            }

        // Make the first node the new node's succesor.
        node->myNextNode = ourFirstNode;

        // Make the new node the first node.
        ourFirstNode = node;
    }

    /* ---------------------------------------- BlockHeader RemoveNode */

    void BlockHeader::RemoveNode(BlockHeader *node)
    {
        assert(node != NULL);
        assert(ourFirstNode != NULL);

        // If target node is the first node in the list...
        if (ourFirstNode == node)
            {
                // ... make the target node's successor the first node.
                assert(ourFirstNode->myPrevNode == NULL);
                ourFirstNode = node->myNextNode;
            }
        
        // Link target node's predecessor, if any, to its successor.
        if (node->myPrevNode != NULL)
            {
                node->myPrevNode->myNextNode = node->myNextNode;
            }
        
        // Link target node's successor, if any, to its predecessor.
        if (node->myNextNode != NULL)
            {
                node->myNextNode->myPrevNode = node->myPrevNode;
            }

        // Clear target node's previous and next pointers.
        node->myPrevNode = NULL;
        node->myNextNode = NULL;
    }

    /* ---------------------------------------- BlockHeader CountBlocks */

    size_t BlockHeader::CountBlocks()
    {
        size_t count = 0;
        BlockHeader *currNode = ourFirstNode;
        while (currNode != NULL)
            {
                count++;
                currNode = currNode->myNextNode;
            }
        return count;
    }

    /* ---------------------------------------- BlockHeader GetBlocks */

    void BlockHeader::GetBlocks(BlockHeader **blockHeaderPP)
    {
        BlockHeader *currNode = ourFirstNode;
        while (currNode != NULL)
            {
                *blockHeaderPP = currNode;
                blockHeaderPP++;
                currNode = currNode->myNextNode;
            }
    }

    /* ---------------------------------------- BlockHeader TypeGreaterThan */

    bool BlockHeader::TypeGreaterThan(BlockHeader *header1, BlockHeader *header2)
    {
        return (strcmp(header1->myTypeName, header2->myTypeName) > 0);
    }

    /* ------------------------------------------------------------ */
    /* ---------------------- class Signature --------------------- */
    /* ------------------------------------------------------------ */

    class Signature
    {
    private:    // constants
        static const unsigned int SIGNATURE1 = 0xCAFEBABE;
        static const unsigned int SIGNATURE2 = 0xFACEFACE;
        
    private:    // member variables
        unsigned int mySignature1;
        unsigned int mySignature2;
            
    public:        // construction/destruction
        Signature() : mySignature1(SIGNATURE1), mySignature2(SIGNATURE2) {};
        ~Signature() { mySignature1 = 0; mySignature2 = 0; }
            
    public:        // static member functions
        static bool IsValidSignature(const Signature *pProspectiveSignature)
        {
                try
                    {
                        if (pProspectiveSignature->mySignature1 != SIGNATURE1) return false;
                        if (pProspectiveSignature->mySignature2 != SIGNATURE2) return false;
                        return true;
                    }
                catch (...)
                    {
                        return false;
                    }
        }
    };

    /* ------------------------------------------------------------ */
    /* -------------------- address conversion -------------------- */
    /* ------------------------------------------------------------ */

    /* We divide the memory blocks we allocate into two "chunks", the
     * "prolog chunk" where we store information about the allocation,
     * and the "user chunk" which we return to the caller to use.
     */

    /* ---------------------------------------- alignment */

    const size_t ALIGNMENT = 4;

    /* If "value" (a memory size or offset) falls on an alignment boundary,
     * then just return it.  Otherwise return the smallest number larger
     * than "value" that falls on an alignment boundary.
     */    

#define PAD_TO_ALIGNMENT_BOUNDARY(value) \
    ((value) + ((ALIGNMENT - ((value) % ALIGNMENT)) % ALIGNMENT))

    /* ---------------------------------------- chunk structs */
    
    /* We declare incomplete structures for each chunk, just to 
     * provide type safety.
     */

    struct PrologChunk;
    struct UserChunk;

    /* ---------------------------------------- chunk sizes and offsets */

    const size_t SIZE_BlockHeader = PAD_TO_ALIGNMENT_BOUNDARY(sizeof(BlockHeader));
    const size_t SIZE_Signature = PAD_TO_ALIGNMENT_BOUNDARY(sizeof(Signature));

    const size_t OFFSET_BlockHeader = 0;
    const size_t OFFSET_Signature = OFFSET_BlockHeader + SIZE_BlockHeader;
    const size_t OFFSET_UserChunk = OFFSET_Signature + SIZE_Signature;
    
    const size_t SIZE_PrologChunk = OFFSET_UserChunk;

    /* ---------------------------------------- GetUserAddress */

    static UserChunk *GetUserAddress(PrologChunk *pProlog)
    {
        char *pchProlog = reinterpret_cast<char *>(pProlog);
        char *pchUser = pchProlog + OFFSET_UserChunk;
        UserChunk *pUser = reinterpret_cast<UserChunk *>(pchUser);
        return pUser;
    }
    /* ---------------------------------------- GetPrologAddress */

    static PrologChunk *GetPrologAddress(UserChunk *pUser)
    {
        char *pchUser = reinterpret_cast<char *>(pUser);
        char *pchProlog = pchUser - OFFSET_UserChunk;
        PrologChunk *pProlog = reinterpret_cast<PrologChunk *>(pchProlog);
        return pProlog;
    }

    /* ---------------------------------------- GetHeaderAddress */

    static BlockHeader *GetHeaderAddress(PrologChunk *pProlog)
    {
        char *pchProlog = reinterpret_cast<char *>(pProlog);
        char *pchHeader = pchProlog + OFFSET_BlockHeader;
        BlockHeader *pHeader = reinterpret_cast<BlockHeader *>(pchHeader);
        return pHeader;
    }

    /* ---------------------------------------- GetSignatureAddress */

    static Signature *GetSignatureAddress(PrologChunk *pProlog)
    {
        char *pchProlog = reinterpret_cast<char *>(pProlog);
        char *pchSignature = pchProlog + OFFSET_Signature;
        Signature *pSignature = reinterpret_cast<Signature *>(pchSignature);
        return pSignature;
    }

    /* ------------------------------------------------------------ */
    /* -------------- memory allocation and stamping -------------- */
    /* ------------------------------------------------------------ */

    /* ---------------------------------------- TrackMalloc */
    
    void *TrackMalloc(size_t size)
    {
        // Allocate the memory, including space for the prolog.
        PrologChunk *pProlog = (PrologChunk *)malloc(SIZE_PrologChunk + size);
        
        // If the allocation failed, then return NULL.
        if (pProlog == NULL) return NULL;
                
        // Use placement new to construct the block header in place.
        BlockHeader *pBlockHeader = new (pProlog) BlockHeader(size);
        
        // Link the block header into the list of extant block headers.
        BlockHeader::AddNode(pBlockHeader);
        
        // Use placement new to construct the signature in place.
        Signature *pSignature = new (GetSignatureAddress(pProlog)) Signature;
        
        // Get the offset to the user chunk and return it.
        UserChunk *pUser = GetUserAddress(pProlog);
        
        return pUser;
    }

    /* ---------------------------------------- TrackFree */
    
    void TrackFree(void *p)
    {
        // It's perfectly valid for "p" to be null; return if it is.
        if (p == NULL) return;
    
        // Get the prolog address for this memory block.
        UserChunk *pUser = reinterpret_cast<UserChunk *>(p);    
        PrologChunk *pProlog = GetPrologAddress(pUser);
       
        // Check the signature, and if it's invalid, return immediately.
        Signature *pSignature = GetSignatureAddress(pProlog);
        if (!Signature::IsValidSignature(pSignature)) return;
        
        // Destroy the signature.
        pSignature->~Signature();
        pSignature = NULL;

        // Unlink the block header from the list and destroy it.
        BlockHeader *pBlockHeader = GetHeaderAddress(pProlog);
        BlockHeader::RemoveNode(pBlockHeader);
        pBlockHeader->~BlockHeader();
        pBlockHeader = NULL;

        // Free the memory block.    
        free(pProlog);
    }

    /* ---------------------------------------- TrackStamp */

    void TrackStamp(void *p, const MemStamp &stamp, char const *typeName)
    {
        // Get the header and signature address for this pointer.
        UserChunk *pUser = reinterpret_cast<UserChunk *>(p);
        PrologChunk *pProlog = GetPrologAddress(pUser);
        BlockHeader *pHeader = GetHeaderAddress(pProlog);
        Signature *pSignature = GetSignatureAddress(pProlog);

        // If the signature is not valid, then return immediately.
        if (!Signature::IsValidSignature(pSignature)) return;

        // "Stamp" the information onto the header.
        pHeader->Stamp(stamp.filename, stamp.lineNum, typeName);
    }

    /* ---------------------------------------- TrackDumpBlocks */

    void TrackDumpBlocks()
    {
        // Get an array of pointers to all extant blocks.
        size_t numBlocks = BlockHeader::CountBlocks();
        BlockHeader **ppBlockHeader =
            (BlockHeader **)calloc(numBlocks, sizeof(*ppBlockHeader));
        BlockHeader::GetBlocks(ppBlockHeader);

        // Dump information about the memory blocks.
        printf("\n");
        printf("=====================\n");
        printf("Current Memory Blocks\n");
        printf("=====================\n");
        printf("\n");
        for (size_t i = 0; i < numBlocks; i++)
            {
                BlockHeader *pBlockHeader = ppBlockHeader[i];
                char const *typeName = pBlockHeader->GetTypeName();
                size_t size = pBlockHeader->GetRequestedSize();
                char const *fileName = pBlockHeader->GetFilename();
                int lineNum = pBlockHeader->GetLineNum();
                printf("*** #%-6d %5d bytes %-50s\n", i, size, typeName);
                printf("... %s:%d\n", fileName, lineNum);
            }

        // Clean up.
        free(ppBlockHeader);
    }

    /* ---------------------------------------- struct MemDigest */

    struct MemDigest
    {
        char const *typeName;
        int blockCount;
        size_t totalSize;

        static bool TotalSizeGreaterThan(const MemDigest &md1, const MemDigest &md2)
        { return md1.totalSize > md2.totalSize; }
    };


    /* ---------------------------------------- SummarizeMemoryUsageForType */

    static void SummarizeMemoryUsageForType(
                                            MemDigest *pMemDigest,
                                            BlockHeader **ppBlockHeader,
                                            size_t startPost,
        size_t endPost
                                            )
    {
        pMemDigest->typeName = ppBlockHeader[startPost]->GetTypeName();
        pMemDigest->blockCount = 0;
        pMemDigest->totalSize = 0;
        for (size_t i = startPost; i < endPost; i++)
            {
                pMemDigest->blockCount++;
                pMemDigest->totalSize += ppBlockHeader[i]->GetRequestedSize();
                assert(strcmp(ppBlockHeader[i]->GetTypeName(), pMemDigest->typeName) == 0);
            }
    }

    /* ---------------------------------------- TrackListMemoryUsage */

    void TrackListMemoryUsage(double sigPrint)
    {
        // If there are no allocated blocks, then return now.
        size_t numBlocks = BlockHeader::CountBlocks();
        if (numBlocks == 0) return;

        // Get an array of pointers to all extant blocks.
        BlockHeader **ppBlockHeader =
            (BlockHeader **)calloc(numBlocks, sizeof(*ppBlockHeader));
        BlockHeader::GetBlocks(ppBlockHeader);

        // Sort the blocks by type name.
        std::sort(
                  ppBlockHeader,
                  ppBlockHeader + numBlocks,
                  BlockHeader::TypeGreaterThan
                  );

        // Find out how many unique types we have.
        size_t numUniqueTypes = 1;
        for (size_t i = 1; i < numBlocks; i++)
            {
                char const *prevTypeName = ppBlockHeader[i - 1]->GetTypeName();
                char const *currTypeName = ppBlockHeader[i]->GetTypeName();
                if (strcmp(prevTypeName, currTypeName) != 0) numUniqueTypes++;
            }

        // Create an array of "digests" summarizing memory usage by type.
        size_t startPost = 0;
        size_t uniqueTypeIndex = 0;
        MemDigest *pMemDigestArray =
            (MemDigest *)calloc(numUniqueTypes, sizeof(*pMemDigestArray));
        for (size_t i = 1; i <= numBlocks; i++)    // yes, less than or *equal* to
            {
                char const *prevTypeName = ppBlockHeader[i - 1]->GetTypeName();
                char const *currTypeName = (i < numBlocks) ? ppBlockHeader[i]->GetTypeName() : "";
                if (strcmp(prevTypeName, currTypeName) != 0)
                    {
                        size_t endPost = i;
                        SummarizeMemoryUsageForType(
                                                    pMemDigestArray + uniqueTypeIndex,
                                                    ppBlockHeader,
                                                    startPost,
                    endPost
                                                    );
                        startPost = endPost;
                        uniqueTypeIndex++;
                    }
            }
        assert(uniqueTypeIndex = numUniqueTypes);

        // Sort the digests by total memory usage.
        std::sort(
                  pMemDigestArray,
                  pMemDigestArray + numUniqueTypes,
                  MemDigest::TotalSizeGreaterThan
                  );

        // Compute the grand total memory usage.
        size_t grandTotalNumBlocks = 0;
        size_t grandTotalSize = 0;
        for (size_t i = 0; i < numUniqueTypes; i++)
            {
                grandTotalNumBlocks += pMemDigestArray[i].blockCount;
                grandTotalSize += pMemDigestArray[i].totalSize;
            }

        // Dump the memory usage statistics.
        printf("%-50s%5s  %5s %7s %s \n", "allocated type", "blocks", "", "bytes", "");
        printf("%-50s%5s  %5s %7s %s \n", "--------------", "------", "", "-----", "");

        for (size_t i = 0; i < numUniqueTypes; i++)
            {
                MemDigest *pMD = pMemDigestArray + i;
                size_t blockCount = pMD->blockCount;
                double blockCountPct = 100.0 * blockCount / grandTotalNumBlocks;
                size_t totalSize = pMD->totalSize;
                double totalSizePct = 100.0 * totalSize / grandTotalSize;

                if (totalSizePct >= sigPrint){
                    printf(
                           "%-50s %5d %5.1f%% %7d %5.1f%%\n",
                           pMD->typeName,
                           blockCount,
                           blockCountPct,
                           totalSize,
                           totalSizePct
                           );
                }
            }
        printf("%-50s %5s %5s  %7s %s \n", "--------", "-----", "", "-------", "");
        printf("%-50s %5d %5s  %7d %s \n", "[totals]", grandTotalNumBlocks, "", grandTotalSize, "");

        // Clean up.
        free(ppBlockHeader);
        free(pMemDigestArray);
    }

}    // namespace MemTrack
/* ------------------------------------------------------------ */
/* ---------------------- new and delete ---------------------- */
/* ------------------------------------------------------------ */

/* ---------------------------------------- operator new */

void *operator new(size_t size)
{
    void *p = MemTrack::TrackMalloc(size);
    if (p == NULL) {
        throw std::bad_alloc();
    }
    return p;
}

/* ---------------------------------------- operator delete */

void operator delete(void *p)
{
    MemTrack::TrackFree(p);
}

/* ---------------------------------------- operator new[] */

void *operator new[](size_t size)
{
    void *p = MemTrack::TrackMalloc(size);
    if (p == NULL){
        throw std::bad_alloc();
    } 
    return p;
}

/* ---------------------------------------- operator delete[] */

void operator delete[](void *p)
{
    MemTrack::TrackFree(p);
}
#endif // DEBUG_MEMTRACK

