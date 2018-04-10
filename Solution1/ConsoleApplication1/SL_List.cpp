#include "SL_List.h"


void SL_ListDestory(SL_List* list)
{
    while (list)
    {
        SL_List* next = list->next;
        delete list;
        list = next;
    }
}

bool SL_ListEmpty(SL_List* list)
{
    return NULL == list;
}

void SL_ListInsertHead(SL_List** list, SL_List* node)
{
    if (list && node)
    {
        if (SL_ListEmpty(*list))
        {
            *list = node;
        }
        else
        {
            node->next = *list;
            *list = node;
        }
    }
}

void SL_ListInsertTail(SL_List** list, SL_List* node)
{
    if (list && node)
    {
        if (SL_ListEmpty(*list))
        {
            *list = node;
        }
        else
        {
            SL_List* list_ = *list;
            while (list_->next)
            {
                list_ = list_->next;
            }

            list_->next = node;
        }
    }
}

SL_List* SL_ListFindNode(SL_List* list, const int data)
{
    SL_List* list_ = list;

    while (list_)
    {
        if (data == list_->data)
        {
            return list_;
        }

        list_ = list_->next;
    }

    return NULL;
}

void SL_ListReverse(SL_List** list)
{
    if (list)
    {
        if (!SL_ListEmpty(*list))
        {
            SL_List* tail = *list;
            SL_List* list_ = *list;

            while (list_)
            {
                SL_List* next = list_->next;
                SL_ListInsertHead(list, list_);
                list_ = next;
            }

            tail->next = NULL;
        }
    }
}

void SL_ListTraver(SL_List* list)
{
    SL_List* list_ = list;

    while (list_)
    {
        printf("%d ", list_->data);
        list_ = list_->next;
    }

    printf("\r\n");
}
