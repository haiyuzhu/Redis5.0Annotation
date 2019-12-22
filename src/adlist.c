/* adlist.c - A generic doubly linked list implementation
 *
 * Copyright (c) 2006-2010, Salvatore Sanfilippo <antirez at gmail dot com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Redis nor the names of its contributors may be used
 *     to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */


#include <stdlib.h>
#include "adlist.h"
#include "zmalloc.h"

/* Create a new list. The created list can be freed with
 * AlFreeList(), but private value of every node need to be freed
 * by the user before to call AlFreeList().
 *
 * On error, NULL is returned. Otherwise the pointer to the new list. */
// 创建双向链表的表头
list *listCreate(void)
{
    struct list *list;

    if ((list = zmalloc(sizeof(*list))) == NULL)
        return NULL;
    list->head = list->tail = NULL;
    list->len = 0;
    list->dup = NULL;
    list->free = NULL;
    list->match = NULL;
    return list;
}

/* Remove all the elements from the list without destroying the list itself. */
// 销毁双向链表中的所有节点
void listEmpty(list *list)
{
    unsigned long len;
    listNode *current, *next;

    current = list->head;
    len = list->len;
    while(len--) {
        next = current->next;
        if (list->free) list->free(current->value); //调用free函数指针释放当前节点value指向的空间
        zfree(current); // 释放当前节点的空间
        current = next;
    }
    list->head = list->tail = NULL; // 将链表的头、尾置NULL
    list->len = 0;
}

/* Free the whole list.
 *
 * This function can't fail. */
// 销毁整个双向链表
void listRelease(list *list)
{
    listEmpty(list);    // 销毁双向链表中的所有节点
    zfree(list);    // 销毁双向链表自己
}

/* Add a new node to the list, to head, containing the specified 'value'
 * pointer as value.
 *
 * On error, NULL is returned and no operation is performed (i.e. the
 * list remains unaltered).
 * On success the 'list' pointer you pass to the function is returned. */
// 向双向链表的头部添加一个节点，添加的节点的值为value指向的空间
list *listAddNodeHead(list *list, void *value)
{
    listNode *node;

    if ((node = zmalloc(sizeof(*node))) == NULL)    // 不能分配新节点的内存直接返回失败
        return NULL;
    node->value = value;
    if (list->len == 0) {   // 如果当前双向链表是空链表
        list->head = list->tail = node;
        node->prev = node->next = NULL;
    } else {    // 如果当前链表不是空链表，在链表头部插入一个节点
        node->prev = NULL;
        node->next = list->head;
        list->head->prev = node;
        list->head = node;  // 链表头节点指向刚插入的节点
    }
    list->len++;    // 成功插入，链表长度自增1
    return list;
}

/* Add a new node to the list, to tail, containing the specified 'value'
 * pointer as value.
 *
 * On error, NULL is returned and no operation is performed (i.e. the
 * list remains unaltered).
 * On success the 'list' pointer you pass to the function is returned. */
// 向链表的尾部插入一个节点，节点的值为value指向的空间
list *listAddNodeTail(list *list, void *value)
{
    listNode *node;

    if ((node = zmalloc(sizeof(*node))) == NULL)
        return NULL;
    node->value = value;
    if (list->len == 0) {   // 链表是空链表的情况
        list->head = list->tail = node;
        node->prev = node->next = NULL;
    } else {    // 链表不是空时，在尾部插入一个节点
        node->prev = list->tail;
        node->next = NULL;
        list->tail->next = node;
        list->tail = node;  // 链表尾部指向刚插入的节点
    }
    list->len++;
    return list;
}

// 在链表list中old_node节点处插入一个新的节点，新节点的值为value指向的空间
// after为0时是在old_node的前面插入，否则是在old_node的后面插入
list *listInsertNode(list *list, listNode *old_node, void *value, int after) {
    listNode *node;

    if ((node = zmalloc(sizeof(*node))) == NULL)
        return NULL;
    node->value = value;
    if (after) {    // after非0，在olde_node后面插入
        node->prev = old_node;
        node->next = old_node->next;    //  此处，old_node->next还没更新
        if (list->tail == old_node) {
            list->tail = node;
        }
    } else {    // 在old_node前面插入
        node->next = old_node;
        node->prev = old_node->prev;    // 此处，old_node->prev还没更新
        if (list->head == old_node) {
            list->head = node;
        }
    }
    if (node->prev != NULL) {
        node->prev->next = node;    // 更新old_node->next
    }
    if (node->next != NULL) {
        node->next->prev = node;    // 更新old_node->prev
    }
    list->len++;
    return list;
}

/* Remove the specified node from the specified list.
 * It's up to the caller to free the private value of the node.
 *
 * This function can't fail. */
// 从链表中删除指定节点
void listDelNode(list *list, listNode *node)
{
    if (node->prev)
        node->prev->next = node->next;  // 将node的前驱的next指向node的后继
    else
        list->head = node->next;
    if (node->next) // 将node的后继的prev指向node的前驱
        node->next->prev = node->prev;
    else
        list->tail = node->prev;
    if (list->free) list->free(node->value);
    zfree(node);
    list->len--;
}

/* Returns a list iterator 'iter'. After the initialization every
 * call to listNext() will return the next element of the list.
 *
 * This function can't fail. */
// 根据遍历的方向获取链表的迭代器
listIter *listGetIterator(list *list, int direction)
{
    listIter *iter;

    if ((iter = zmalloc(sizeof(*iter))) == NULL) return NULL;
    if (direction == AL_START_HEAD)
        iter->next = list->head;
    else
        iter->next = list->tail;
    iter->direction = direction;
    return iter;
}

/* Release the iterator memory */
void listReleaseIterator(listIter *iter) {
    zfree(iter);
}

/* Create an iterator in the list private iterator structure */
// 重新获取链表从头部开始的迭代器
void listRewind(list *list, listIter *li) {
    li->next = list->head;
    li->direction = AL_START_HEAD;
}

// 重新获取链表从尾部的迭代器
void listRewindTail(list *list, listIter *li) {
    li->next = list->tail;
    li->direction = AL_START_TAIL;
}

/* Return the next element of an iterator.
 * It's valid to remove the currently returned element using
 * listDelNode(), but not to remove other elements.
 *
 * The function returns a pointer to the next element of the list,
 * or NULL if there are no more elements, so the classical usage patter
 * is:
 *
 * iter = listGetIterator(list,<direction>);
 * while ((node = listNext(iter)) != NULL) {
 *     doSomethingWith(listNodeValue(node));
 * }
 *
 * */
// 获取迭代器的下一个元素
listNode *listNext(listIter *iter)
{
    listNode *current = iter->next;

    if (current != NULL) {
        if (iter->direction == AL_START_HEAD)
            iter->next = current->next;
        else
            iter->next = current->prev;
    }
    return current;
}

/* Duplicate the whole list. On out of memory NULL is returned.
 * On success a copy of the original list is returned.
 *
 * The 'Dup' method set with listSetDupMethod() function is used
 * to copy the node value. Otherwise the same pointer value of
 * the original node is used as value of the copied node.
 *
 * The original list both on success or error is never modified. */
// 深拷贝orig链表，创建链表，拷贝结点失败都返回NULL。
list *listDup(list *orig)
{
    list *copy;
    listIter iter;
    listNode *node;

    if ((copy = listCreate()) == NULL)
        return NULL;
    copy->dup = orig->dup;
    copy->free = orig->free;
    copy->match = orig->match;
    listRewind(orig, &iter);
    while((node = listNext(&iter)) != NULL) {
        void *value;
        // 如果定义了深拷贝函数，则用深拷贝函数来拷贝当前结点
        if (copy->dup) {
            value = copy->dup(node->value);
            if (value == NULL) {
                listRelease(copy);  // 如果有一个结点拷贝失败则需要将整个copy链表给销毁，因为可能是因为内存不足导致的拷贝失败。
                return NULL;
            }
        } else
            value = node->value;    // 如果未定义拷贝函数，则用浅拷贝，比如value就是存的一个int值
        if (listAddNodeTail(copy, value) == NULL) { // 将新增的结点添加到copy链表的尾部
            listRelease(copy);
            return NULL;
        }
    }
    return copy;
}

/* Search the list for a node matching a given key.
 * The match is performed using the 'match' method
 * set with listSetMatchMethod(). If no 'match' method
 * is set, the 'value' pointer of every node is directly
 * compared with the 'key' pointer.
 *
 * On success the first matching node pointer is returned
 * (search starts from head). If no matching node exists
 * NULL is returned. */
// 遍历整个链表，查询key对应的结点，找不到对应结点就是NULL
listNode *listSearchKey(list *list, void *key)
{
    listIter iter;
    listNode *node;

    listRewind(list, &iter);    //获取链表从头向尾的迭代器
    while((node = listNext(&iter)) != NULL) {
        if (list->match) {
            if (list->match(node->value, key)) {    // 如果定义了match函数，则用match函数判断当前结点是否是key对应的结点
                return node;
            }
        } else {
            if (key == node->value) {   // 如果没定义match函数，则直接将key跟value进行值比较
                return node;
            }
        }
    }
    return NULL;
}

/* Return the element at the specified zero-based index
 * where 0 is the head, 1 is the element next to head
 * and so on. Negative integers are used in order to count
 * from the tail, -1 is the last element, -2 the penultimate
 * and so on. If the index is out of range NULL is returned. */
// o(n)获取链表的第index个元素，从前往后是0,1,...,从后往前是-1,-2,...
listNode *listIndex(list *list, long index) {
    listNode *n;

    if (index < 0) {    // 从后往前，
        index = (-index)-1; // 计算从后往前的相对偏移
        n = list->tail;
        while(index-- && n) n = n->prev;
    } else {
        n = list->head;
        while(index-- && n) n = n->next;
    }
    return n;
}

/* Rotate the list removing the tail node and inserting it to the head. */
// 将链表的尾结点移动到链表的头部
void listRotate(list *list) {
    // 这里没调用listAddNodeHead，个人觉得是因为调用listAddNodeHead也需要将尾结点从链表断开，开辟新空间来存放结点，
    // 释放当前结点的空间。不高效！
    listNode *tail = list->tail;

    if (listLength(list) <= 1) return;

    /* Detach current tail */
    list->tail = tail->prev;
    list->tail->next = NULL;
    /* Move it as head */
    list->head->prev = tail;
    tail->prev = NULL;
    tail->next = list->head;
    list->head = tail;
}

/* Add all the elements of the list 'o' at the end of the
 * list 'l'. The list 'other' remains empty but otherwise valid. */
// 将链表o的所有结点从尾部添加到链表l上
void listJoin(list *l, list *o) {
    if (o->head)    // 设置o的头结点的前驱
        o->head->prev = l->tail;

    if (l->tail)    // 设置l的尾结点的后继
        l->tail->next = o->head;
    else
        l->head = o->head;

    if (o->tail) l->tail = o->tail; // 更新l的尾结点
    l->len += o->len;   // 更新l的长度

    /* Setup other as an empty list. */
    o->head = o->tail = NULL;
    o->len = 0;
}
