/*
 * =====================================================================================
 *
 *       Filename:  test_uc_coroutine.cpp
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2017年01月11日 11时37分00秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  anonymalias (anonym_alias@163.com), 
 *   Organization:  
 *
 * =====================================================================================
 */

#include "uc_coroutine_pool.h"

#include <stdio.h>

UCCoroutine * coro = NULL;

int working(int arg)
{
    printf("%s:%d\n", __FUNCTION__, arg);

    printf("%s:before yield|task_id:%u\n", __FUNCTION__, coro->GetTaskID());
    coro->Yield_S();
    printf("%s:after yield|task_id:%u\n", __FUNCTION__, coro->GetTaskID());
}

void test_1()
{
    printf("%s:start\n", __FUNCTION__);

    UCCoroutinePool coro_pool;

    for(int i = 1; i <= 10; ++i)
    {
        coro = coro_pool.Post(std::bind(working, i));
        coro->Resume_P();

        printf("\n%s:before resume|task_id:%u\n", __FUNCTION__, coro->GetTaskID());
        coro->Resume_P();
        printf("%s:after resume|task_id:%u\n", __FUNCTION__, coro->GetTaskID());
    }

    printf("%s:end\n", __FUNCTION__);
}

void test_2()
{
    printf("%s:start\n", __FUNCTION__);

    UCCoroutinePool coro_pool;
    std::list<UCCoroutine *> running_coro_list;

    for(int i = 1; i <= 10; ++i)
    {
        coro = coro_pool.Post(std::bind(working, i));
        coro->Resume_P();
        running_coro_list.push_back(coro);
    }

    for(int i = 1; i <= 10; ++i)
    {
        coro = running_coro_list.front();
        running_coro_list.pop_front();

        printf("\n%s:before resume|task_id:%u\n", __FUNCTION__, coro->GetTaskID());
        coro->Resume_P();
        printf("%s:after resume|task_id:%u\n", __FUNCTION__, coro->GetTaskID());
    }

    printf("%s:end\n", __FUNCTION__);
}

void test_3()
{
    printf("%s:start\n", __FUNCTION__);

    UCCoroutinePool coro_pool(1024, 512, 1024000, true);
    std::list<UCCoroutine *> running_coro_list;

    for(int i = 1; i <= 10; ++i)
    {
        coro = coro_pool.Post(std::bind(working, i));
        coro->Resume_S();
        running_coro_list.push_back(coro);
    }

    for(int i = 1; i <= 10; ++i)
    {
        coro = running_coro_list.front();
        running_coro_list.pop_front();

        printf("\n%s:before resume|task_id:%u\n", __FUNCTION__, coro->GetTaskID());
        coro->Resume_S();
        printf("%s:after resume|task_id:%u\n", __FUNCTION__, coro->GetTaskID());
    }

    printf("%s:end\n", __FUNCTION__);
}

int main()
{
    test_3();
}
