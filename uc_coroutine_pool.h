/*
 * =====================================================================================
 *
 *       Filename:  uc_coroutine_pool.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  03/23/2017 07:29:16 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  YOUR NAME (), 
 *   Organization:  
 *
 * =====================================================================================
 */

#include "uc_coroutine.h"

#include <list>
#include <unordered_map>

class UCCoroutinePool
{
public:
    typedef std::function<void(void)> task_t;
    struct TaskCoro
    {
        task_t task;
        UCCoroutine * coro;
    };

public:
    
    UCCoroutinePool(uint32_t max_coro = 1024, uint32_t max_free_coro = 512, 
            uint32_t stack_size = 102400, bool is_shared_model = false) : 
        m_max_coro(max_coro), m_max_free_coro(max_free_coro), m_stack_size(stack_size)
    {
        if(is_shared_model)
        {
            m_shared_buf_ptr = new char[m_stack_size];
        }

        for(uint32_t i = 0; i < max_free_coro; ++i)
        {
            UCCoroutine * coro = NULL;
            if(!is_shared_model)
                coro = new UCCoroutine(
                        std::bind(&UCCoroutinePool::Execute, this, std::placeholders::_1), 
                        m_stack_size
                        );
            else
                coro = new UCCoroutine(
                        std::bind(&UCCoroutinePool::Execute, this, std::placeholders::_1),
                        m_shared_buf_ptr, m_stack_size
                        );

            m_free_coro_list.push_back(coro);
        }

    }

    uint32_t GetTaskID()
    {
        static uint32_t task_id = 0;
        return ++task_id == 0 ? ++task_id : task_id;
    }

    void Execute(uint32_t task_id)
    {
        printf("%s:%u\n", __FUNCTION__, task_id);

        if(!m_coro_map.count(task_id))
        {
            return;
        }

        m_coro_map[task_id].task();
        m_free_coro_list.push_back(m_coro_map[task_id].coro);
    }

    UCCoroutine * Post(task_t cb)
    {
        printf("\n%s:\n", __FUNCTION__);

        UCCoroutine * coro = NULL;

        if(!m_free_coro_list.empty())
        {
            coro = m_free_coro_list.back();
            m_free_coro_list.pop_back();

            coro->Init();
        }
        else
        {
            if(m_coro_map.size() >= m_max_coro)
            {
                printf("coroutine num is up to %lu >= m_max_coro:%u\n", m_coro_map.size(), m_max_coro);
                return NULL;
            }

            coro = new UCCoroutine(
                    std::bind(&UCCoroutinePool::Execute, this, std::placeholders::_1), 
                    m_stack_size);
            coro->Init();
        }

        uint32_t task_id = GetTaskID();
        m_coro_map[task_id] = {cb, coro};
        coro->SetTaskID(task_id);

        return coro;
    }


private:
    uint32_t m_max_coro;
    uint32_t m_max_free_coro;
    uint32_t m_stack_size;
    char * m_shared_buf_ptr;

    std::list<UCCoroutine *> m_free_coro_list;
    std::unordered_map<uint32_t, TaskCoro> m_coro_map;

};
