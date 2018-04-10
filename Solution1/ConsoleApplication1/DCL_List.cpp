#include "DCL_List.h"


void DCL_ListDestory(DCL_List* list)
{
    if (list)
    {
        if (DCL_ListEmpty(list))
        {
            DCL_List* list_ = list->next;
            while (list != list_)
            {
                DCL_List* next = list_->next;
                delete list_;
                list_ = next;
            }
        }
    }
}

bool DCL_ListEmpty(DCL_List* list)
{
    return NULL == list;
}

void DCL_ListInsertHead(DCL_List** list, DCL_List* node)
{
    if (list && node)
    {
        if (DCL_ListEmpty(*list))
        {
            node->next = node->prev = node;
            *list = node;
        }
        else
        {
            (*list)->prev->next = node;
            node->prev = (*list)->prev;
            node->next = *list;
            (*list)->prev = node;
            *list = node;
        }
    }
}

void DCL_ListInsertTail(DCL_List** list, DCL_List* node)
{
    if (list && node)
    {
        if (DCL_ListEmpty(*list))
        {
            node->next = node->prev = node;
            *list = node;
        }
        else
        {
            (*list)->prev->next = node;
            node->prev = (*list)->prev;
            node->next = *list;
            (*list)->prev = node;
        }
    }
}

DCL_List* DCL_ListFindNode(DCL_List* list, const int data)
{
    if (list)
    {
        if (!DCL_ListEmpty(list))
        {
            DCL_List* list_ = list->next;
            while (list_ != list)
            {
                if (data == list_->data)
                {
                    return list_;
                }

                list_ = list_->next;
            }
        }
    }

    return NULL;
}

void DCL_ListReverse(DCL_List** list)
{
    if (list)
    {
        if (!DCL_ListEmpty(*list))
        {
            DCL_List* list_ = *list;
            DCL_List* head = (*list)->prev;

            do
            {
                DCL_List* next = list_->next;
                list_->next = list_->prev;
                list_->prev = next;
                list_ = next;

            } while (list_ != *list);

            *list = head;
        }
    }
}

void DCL_ListTraver(DCL_List* list)
{
    if (list)
    {
        if (!DCL_ListEmpty(list))
        {
            DCL_List* list_ = list;

            do
            {
                printf("%d ", list_->data);
                list_ = list_->next;

            } while (list_ != list);
        }

        printf("\r\n");
    }
}
