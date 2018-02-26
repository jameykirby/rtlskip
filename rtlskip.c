//
// rtlskip.c
//
// Native NT implementation of a skip list - O(log n).
//
// This is free and unencumbered software released into the public domain.
//
// Anyone is free to copy, modify, publish, use, compile, sell, or
// distribute this software, either in source code form or as a compiled
// binary, for any purpose, commercial or non - commercial, and by any
// means.
// 
// In jurisdictions that recognize copyright laws, the author or authors
// of this software dedicate any and all copyright interest in the
// software to the public domain. We make this dedication for the benefit
// of the public at large and to the detriment of our heirs and
// successors. We intend this dedication to be an overt act of
// relinquishment in perpetuity of all present and future rights to this
// software under copyright law.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
// IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
// OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.
//
// For more information, please refer to <http://unlicense.org/>
// 
// Acknowledgments:
//
// Skip Lists: A Probabilistic Alternative to Balanced Trees - William Pugh
// ftp://ftp.cs.umd.edu/pub/skipLists/skiplists.pdf (Original Paper)
// http://cglab.ca/~morin/teaching/5408/refs/p90b.pdf (A Skip List Cookbook)
//

#include "rtlskip.h"

#if defined (RTL_SKIPLIST_USE_RANDOMEX)
#define RTL_SKIPLIST_DEFAULT_SEED 31337UL
#endif

// Generates a random number on the interval [0,0xffffffff]
static ULONG sl_rand
(
_In_ PRTL_SKIPLIST_TABLE Table
)
{
    ASSERT(Table != NULL);

#if defined (RTL_SKIPLIST_USE_RANDOMEX)
    return RtlRandomEx(&Table->Seed);
#else
    ULONGLONG t;

    Table->KissState.x = 314527869 * Table->KissState.x + 1234567;
    Table->KissState.y ^= Table->KissState.y << 5;
    Table->KissState.y ^= Table->KissState.y >> 7;
    Table->KissState.y ^= Table->KissState.y << 22;
    t = 4294584393ULL * Table->KissState.z + Table->KissState.c;
    Table->KissState.c = t >> 32;
    Table->KissState.z = (ULONG)t;

    return Table->KissState.x + Table->KissState.y + Table->KissState.z;
#endif
}

// Probabilistic random level generator.
static LONG sl_rlevel
(
_In_ PRTL_SKIPLIST_TABLE Table
)
{
    ASSERT(Table != NULL);

    LONG level = 1;
    while (sl_rand(Table) < (2147483647 / 2) && level < RTL_SKIPLIST_MAX_LEVEL)
        level++;
    return level;
}

static PRTL_SKIPLIST_NODE _locate
(
_In_ PRTL_SKIPLIST_TABLE Table,
_In_ PVOID Buffer,
_Out_ BOOLEAN *Found,
_Inout_opt_ PRTL_SKIPLIST_NODE *Update
)
{
    ASSERT(Table != NULL);

    PRTL_SKIPLIST_NODE x = Table->Head;
    *Found = FALSE;

    for (LONG i = Table->Level; i >= 0; i--)
    {
        while (x->Forward[i] != NULL)
        {
            RTL_GENERIC_COMPARE_RESULTS result = Table->CompareRoutine
                (
                Table,
                Buffer,
                x->Forward[i]->Data
                );

            if (result == GenericGreaterThan)
                x = x->Forward[i];
            else
            {
                // One downside to the Pugh algorithm is that it requires
                // two calls to the compare routine; one for traversal
                // and one for the actual compare for equality. We can
                // optimize out this double compare by doing the test
                // for equality in the same pass as traversal.
                if (result == GenericEqual)
                    *Found = TRUE;

                break;
            }
        }

        if (ARGUMENT_PRESENT(Update))
            Update[i] = x;
    }

    return x;
}

_Must_inspect_result_
PVOID
NTAPI
RtlLookupElementGenericTableSkiplist
(
_In_ PRTL_SKIPLIST_TABLE Table,
_In_ PVOID Buffer
)
{
    ASSERT(Table != NULL);

    BOOLEAN found;
    PRTL_SKIPLIST_NODE x = _locate(Table, Buffer, &found, NULL)->Forward[0];
    return (found == TRUE) ? x->Data : NULL;
}

PVOID
NTAPI
RtlInsertElementGenericTableSkiplist
(
_In_ PRTL_SKIPLIST_TABLE Table,
_In_reads_bytes_(BufferSize) PVOID Buffer,
_In_ CLONG BufferSize,
_Out_opt_ PBOOLEAN NewElement
)
{
    ASSERT(Table != NULL);

    if (ARGUMENT_PRESENT(NewElement))
        *NewElement = FALSE;

    BOOLEAN found;
    PRTL_SKIPLIST_NODE update[RTL_SKIPLIST_MAX_LEVEL + 1];
    PRTL_SKIPLIST_NODE x = _locate(Table, Buffer, &found, update)->Forward[0];

    if (found == TRUE)
        return x->Data;

    // This is what makes the whole thing work.
    LONG level = sl_rlevel(Table);

    x = Table->AllocateRoutine
        (
        Table,
        sizeof(RTL_SKIPLIST_NODE) + (level * sizeof(PRTL_SKIPLIST_NODE))
        );

    if (x == NULL)
        return NULL;

    PVOID data = Table->AllocateRoutine(Table, BufferSize);

    if (data == NULL)
    {
        Table->FreeRoutine(Table, x);
        return NULL;
    }

    if (ARGUMENT_PRESENT(NewElement))
        *NewElement = TRUE;

    RtlCopyMemory(data, Buffer, BufferSize);
    x->Data = data;
    Table->Size++;

    if (level > Table->Level)
    {
        for (LONG i = Table->Level + 1; i <= level; i++)
            update[i] = Table->Head;

        Table->Level = level;
    }

#if defined (RTL_SKIPLIST_INCLUDE_BACKWARD_LINK)
    x->Backward = update[0];

    if (update[0]->Forward[0] != NULL)
        update[0]->Forward[0]->Backward = x;
#endif

    for (INT i = 0; i <= level; i++)
    {   
        x->Forward[i] = update[i]->Forward[i];
        update[i]->Forward[i] = x;
    }

    return data;
}

BOOLEAN
NTAPI
RtlDeleteElementGenericTableSkiplist
(
_In_ PRTL_SKIPLIST_TABLE Table,
_In_ PVOID Buffer
)
{
    ASSERT(Table != NULL);

    BOOLEAN found;
    PRTL_SKIPLIST_NODE update[RTL_SKIPLIST_MAX_LEVEL + 1];
    PRTL_SKIPLIST_NODE x = _locate(Table, Buffer, &found, update)->Forward[0];

    if (found == FALSE)
        return FALSE;

    for (LONG i = 0; i < Table->Level; i++)
    {
        if (update[i]->Forward[i] != x)
            break;

        update[i]->Forward[i] = x->Forward[i];
    }

#if defined (RTL_SKIPLIST_INCLUDE_BACKWARD_LINK)
    if (x->Backward != NULL)
        x->Backward->Forward[0] = x->Forward[0];

    if (x->Forward[0] != NULL)
        x->Forward[0]->Backward = x->Backward;
#endif

    Table->FreeRoutine(Table, x->Data);
    Table->FreeRoutine(Table, x);
    --Table->Size;

    while (Table->Level > 0)
    {
        if (Table->Head->Forward[Table->Level - 1] != NULL)
            break;

        --Table->Level;
    }

    return TRUE;
}

BOOLEAN
NTAPI
RtlIsGenericTableEmptySkiplist
(
_In_ PRTL_SKIPLIST_TABLE Table
)
{
    ASSERT(Table != NULL);

    return ((Table->Size) ? (FALSE) : (TRUE));
}

LONGLONG
NTAPI
RtlNumberGenericTableElementsSkiplist
(
_In_ PRTL_SKIPLIST_TABLE Table
)
{
    ASSERT(Table != NULL);

    return Table->Size;
}

PVOID
NTAPI
RtlEnumerateGenericTableSkiplist
(
_In_ PRTL_SKIPLIST_TABLE Table,
_In_ PVOID *RestartKey
)
{
    ASSERT(Table != NULL);

    PRTL_SKIPLIST_NODE x;

    if (ARGUMENT_PRESENT(RestartKey))
    {
        PRTL_SKIPLIST_NODE restart = (PRTL_SKIPLIST_NODE)*RestartKey;
        *RestartKey = x = restart->Forward[0];
    }
    else
        x = Table->Head->Forward[0];

    return (x ? x->Data : NULL);
}

PVOID
NTAPI
RtlEnumerateGenericTableWithoutSplayingSkiplist
(
_In_ PRTL_SKIPLIST_TABLE Table,
_In_ PVOID *RestartKey
)
{
    ASSERT(Table != NULL);

    PRTL_SKIPLIST_NODE x;

    if (ARGUMENT_PRESENT(RestartKey))
    {
        PRTL_SKIPLIST_NODE restart = (PRTL_SKIPLIST_NODE)*RestartKey;
        *RestartKey = x = restart->Forward[0];
    }
    else
        x = Table->Head->Forward[0];

    return (x ? x->Data : NULL);
}

_Must_inspect_result_
BOOLEAN
NTAPI
RtlInitializeGenericTableSkiplist
(
_Out_ PRTL_SKIPLIST_TABLE Table,
_In_ PRTL_SKIPLIST_COMPARE_ROUTINE CompareRoutine,
_In_ PRTL_SKIPLIST_ALLOCATE_ROUTINE AllocateRoutine,
_In_ PRTL_SKIPLIST_FREE_ROUTINE FreeRoutine,
_In_opt_ PVOID TableContext
)
{
    // Seed the random number generator.
#if defined (RTL_SKIPLIST_USE_RANDOMEX)
    Table->Seed = RTL_SKIPLIST_DEFAULT_SEED;
#else
    Table->KissState.x = 123456789;
    Table->KissState.y = 987654321;
    Table->KissState.z = 43219876;
    Table->KissState.c = 6543217;
#endif

    PRTL_SKIPLIST_NODE head = AllocateRoutine
        (
        Table,
        sizeof(RTL_SKIPLIST_NODE) + (RTL_SKIPLIST_MAX_LEVEL * sizeof(PRTL_SKIPLIST_NODE))
        );

    if (head == NULL)
    {   // Oops!
        KdPrint
            ((
            "%s: %s(%d) Unable to allocate head node.\n",
            __FILE__,
            __FUNCTION__,
            __LINE__
            ));

        return FALSE;
    }

    head->Data = NULL;

    for (LONG i = 0; i <= RTL_SKIPLIST_MAX_LEVEL; i++)
        head->Forward[i] = NULL;

    Table->Head = head;
    Table->Level = 0;
    Table->Size = 0;
    Table->Context = TableContext;
    Table->CompareRoutine = CompareRoutine;
    Table->AllocateRoutine = AllocateRoutine;
    Table->FreeRoutine = FreeRoutine;

    return TRUE;
}

/*
PVOID
NTAPI
RtlInsertElementGenericTableFullSkiplist
(
_In_ PRTL_SKIPLIST_TABLE Table,
_In_reads_bytes_(BufferSize) PVOID Buffer,
_In_ CLONG BufferSize,
_Out_opt_ PBOOLEAN NewElement,
_In_ PVOID NodeOrParent,
_In_ TABLE_SEARCH_RESULT SearchResult
)
{
}

VOID
NTAPI
RtlDeleteElementGenericTableSkiplistEx
(
_In_ PRTL_SKIPLIST_TABLE Table,
_In_ PVOID NodeOrParent
)
{
}

PVOID
NTAPI
RtlLookupElementGenericTableFullSkiplist
(
_In_ PRTL_SKIPLIST_TABLE Table,
_In_ PVOID Buffer,
_Out_ PVOID *NodeOrParent,
_Out_ TABLE_SEARCH_RESULT *SearchResult
)
{
}

PVOID
NTAPI
RtlLookupFirstMatchingElementGenericTableSkiplist
(
_In_ PRTL_SKIPLIST_TABLE Table,
_In_ PVOID Buffer,
_Out_ PVOID *RestartKey
)
{
}

PVOID
NTAPI
RtlGetElementGenericTableSkiplist
(
_In_ PRTL_SKIPLIST_TABLE Table,
_In_ ULONG I
)
{
}

*/
