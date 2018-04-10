#pragma once
#include <cstring>
#include <cstdio>



// µ•œÚ¡¥±Ì
// Singly linked list
struct SL_List
{
	SL_List* next;
	int data;
};



void SL_ListDestory(SL_List* list);

bool SL_ListEmpty(SL_List* list);

void SL_ListInsertHead(SL_List** list, SL_List* node);

void SL_ListInsertTail(SL_List** list, SL_List* node);

SL_List* SL_ListFindNode(SL_List* list, const int data);

void SL_ListReverse(SL_List** list);

void SL_ListTraver(SL_List* list);