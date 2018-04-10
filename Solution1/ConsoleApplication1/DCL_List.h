#pragma once

#include <cstring>
#include <cstdio>


// 双向循环链表
// Doubly Circular linked list
struct DCL_List
{
    DCL_List* prev;
    DCL_List* next;
    int data;
};


void DCL_ListDestory(DCL_List* list);

bool DCL_ListEmpty(DCL_List* list);

void DCL_ListInsertHead(DCL_List** list, DCL_List* node);

void DCL_ListInsertTail(DCL_List** list, DCL_List* node);

DCL_List* DCL_ListFindNode(DCL_List* list, const int data);

void DCL_ListReverse(DCL_List** list);

void DCL_ListTraver(DCL_List* list);

