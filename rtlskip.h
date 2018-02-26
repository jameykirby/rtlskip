//
// rtlskip.h
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

#if !defined (_RTL_SKIPLIST_H)
#define _RTL_SKIPLIST_H

#include <ntddk.h>

#define RTL_SKIPLIST_MAX_LEVEL 32

// Build options.
#define RTL_SKIPLIST_INCLUDE_BACKWARD_LINK
// #define RTL_SKIPLIST_USE_RANDOMEX

// Forwarder for the function pointers.
typedef struct _RTL_SKIPLIST_TABLE RTL_SKIPLIST_TABLE, *PRTL_SKIPLIST_TABLE;

typedef
_IRQL_requires_same_
_Function_class_(RTL_SKIPLIST_COMPARE_ROUTINE)
RTL_GENERIC_COMPARE_RESULTS
NTAPI
RTL_SKIPLIST_COMPARE_ROUTINE
(
_In_ struct _RTL_SKIPLIST_TABLE *Table,
_In_ PVOID FirstStruct,
_In_ PVOID SecondStruct
);
typedef RTL_SKIPLIST_COMPARE_ROUTINE *PRTL_SKIPLIST_COMPARE_ROUTINE;

typedef
_IRQL_requires_same_
_Function_class_(RTL_SKIPLIST_ALLOCATE_ROUTINE)
__drv_allocatesMem(Mem)
PVOID
NTAPI
RTL_SKIPLIST_ALLOCATE_ROUTINE
(
_In_ struct _RTL_SKIPLIST_TABLE *Table,
_In_ CLONG ByteSize
);
typedef RTL_SKIPLIST_ALLOCATE_ROUTINE *PRTL_SKIPLIST_ALLOCATE_ROUTINE;

typedef
_IRQL_requires_same_
_Function_class_(RTL_SKIPLIST_FREE_ROUTINE)
VOID
NTAPI
RTL_SKIPLIST_FREE_ROUTINE
(
_In_ struct _RTL_SKIPLIST_TABLE *Table,
_In_ __drv_freesMem(Mem) _Post_invalid_ PVOID Buffer
);
typedef RTL_SKIPLIST_FREE_ROUTINE *PRTL_SKIPLIST_FREE_ROUTINE;

typedef struct _RTL_SKIPLIST_NODE {
    PVOID Data;
#if defined (RTL_SKIPLIST_INCLUDE_BACKWARD_LINK)
    struct _RTL_SKIPLIST_NODE *Backward;
#endif
    struct _RTL_SKIPLIST_NODE *Forward[1];
} RTL_SKIPLIST_NODE, *PRTL_SKIPLIST_NODE;

typedef struct _RTL_SKIPLIST_TABLE {
    struct _RTL_SKIPLIST_NODE *Head;
    LONG Level;
    ULONG Size;
#if defined (RTL_SKIPLIST_USE_RANDOMEX)
    ULONG Seed;
#else
    struct
    {
        ULONG x;
        ULONG y;
        ULONG z;
        ULONG c;
    } KissState;
#endif
    PVOID Context;
    PRTL_SKIPLIST_COMPARE_ROUTINE CompareRoutine;
    PRTL_SKIPLIST_ALLOCATE_ROUTINE AllocateRoutine;
    PRTL_SKIPLIST_FREE_ROUTINE FreeRoutine;
} RTL_SKIPLIST_TABLE, *PRTL_SKIPLIST_TABLE;

typedef struct _RTL_SKIPLIST_ITOR {
    struct _RTL_SLIPLIST_TABLE *Table;
    struct _RTL_SKIPLIST_NODE *Node;
} RTL_SKIPLIST_ITOR, *PRTL_SKIPLIST_ITOR;

PVOID
NTAPI
RtlInsertElementGenericTableSkiplist
(
_In_ PRTL_SKIPLIST_TABLE Table,
_In_reads_bytes_(BufferSize) PVOID Buffer,
_In_ CLONG BufferSize,
_Out_opt_ PBOOLEAN NewElement
);

BOOLEAN
NTAPI
RtlDeleteElementGenericTableSkiplist
(
_In_ PRTL_SKIPLIST_TABLE Table,
_In_ PVOID Buffer
);

_Must_inspect_result_
PVOID
NTAPI
RtlLookupElementGenericTableSkiplist
(
_In_ PRTL_SKIPLIST_TABLE Table,
_In_ PVOID Buffer
);

BOOLEAN
NTAPI
RtlIsGenericTableEmptySkiplist
(
_In_ PRTL_SKIPLIST_TABLE Table
);

LONGLONG
NTAPI
RtlNumberGenericTableElementsSkiplist
(
_In_ PRTL_SKIPLIST_TABLE Table
);

PVOID
NTAPI
RtlEnumerateGenericTableSkiplist
(
_In_ PRTL_SKIPLIST_TABLE Table,
_In_ PVOID *RestartKey
);

PVOID
NTAPI
RtlEnumerateGenericTableWithoutSplayingSkiplist
(
_In_ PRTL_SKIPLIST_TABLE Table,
_In_ PVOID *RestartKey
);

BOOLEAN
NTAPI
RtlInitializeGenericTableSkiplist
(
_Out_ PRTL_SKIPLIST_TABLE Table,
_In_ PRTL_SKIPLIST_COMPARE_ROUTINE CompareRoutine,
_In_ PRTL_SKIPLIST_ALLOCATE_ROUTINE AllocateRoutine,
_In_ PRTL_SKIPLIST_FREE_ROUTINE FreeRoutine,
_In_opt_ PVOID TableContext
);

// The follwoing functions still need implementing to round out
// interface parity with the WDK generic table interface.
//

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
);

VOID
NTAPI
RtlDeleteElementGenericTableSkiplistEx
(
_In_ PRTL_SKIPLIST_TABLE Table,
_In_ PVOID NodeOrParent
);

PVOID
NTAPI
RtlLookupElementGenericTableFullSkiplist
(
_In_ PRTL_SKIPLIST_TABLE Table,
_In_ PVOID Buffer,
_Out_ PVOID *NodeOrParent,
_Out_ TABLE_SEARCH_RESULT *SearchResult
);

PVOID
NTAPI
RtlLookupFirstMatchingElementGenericTableSkiplist
(
_In_ PRTL_SKIPLIST_TABLE Table,
_In_ PVOID Buffer,
_Out_ PVOID *RestartKey
);

PVOID
NTAPI
RtlGetElementGenericTableSkiplist
(
_In_ PRTL_SKIPLIST_TABLE Table,
_In_ ULONG I
);

#endif
