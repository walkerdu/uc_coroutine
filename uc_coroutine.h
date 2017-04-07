/*
 * =====================================================================================
 *
 *       Filename:  uc_coroutine.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2017年01月10日 18时11分34秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  anonymalias (anonym_alias@163.com), 
 *   Organization:  
 *
 * =====================================================================================
 */

#include <ucontext.h>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <iostream>

#include <functional>


class UCCoroutine
{
public:
    typedef std::function<void(uint32_t)> callback_t;

    friend class UCCoroutinePool;

public:
    UCCoroutine(callback_t callback, uint32_t _stack_size = 102400) : 
        cb(callback), private_stack_size(_stack_size), private_stack_sp(NULL), private_buf_size(0),
        share_stack_sp(NULL), share_stack_size(0)
    {//private stack model

        getcontext(&m_ctx);
        
        private_stack_sp = new char[private_stack_size];

        m_ctx.uc_stack.ss_sp = private_stack_sp; 
        m_ctx.uc_stack.ss_size = private_stack_size;
        m_ctx.uc_link = &m_main_ctx;

        //printf("%s:makecontext:stack%p\n", __FUNCTION__, m_ctx.uc_stack.ss_sp);

        //makecontext(&m_ctx, Run, 1, this);
    }

    UCCoroutine(callback_t callback, char * _stack_sp, uint32_t _stack_size) : 
        cb(callback), share_stack_sp(_stack_sp), share_stack_size(_stack_size),
        private_stack_sp(NULL), private_stack_size(0), private_buf_size(0)
    {//share stack model

        getcontext(&m_ctx);
        
        m_ctx.uc_stack.ss_sp = share_stack_sp;
        m_ctx.uc_stack.ss_size = share_stack_size;
        m_ctx.uc_link = &m_main_ctx;

        //printf("%s:makecontext:stack%p\n", __FUNCTION__, m_ctx.uc_stack.ss_sp);

        //makecontext(&m_ctx, Run, 1, this);
    }

    void Resume_P()
    {
        //setcontext(&m_ctx);
        swapcontext(&m_main_ctx, &m_ctx);
    }

    void Resume_S()
    {
        printf("[TRACE:%s:IN]\n", __FUNCTION__);

        if(private_stack_sp)
        {
            char * start_sp = share_stack_sp + share_stack_size - private_stack_size; 
            memmove(start_sp, private_stack_sp, private_stack_size);
        }

        swapcontext(&m_main_ctx, &m_ctx);

        printf("[TRACE:%s:OUT]\n", __FUNCTION__);
    }

    void Yield_P()
    {
        //getcontext(&m_ctx);
        swapcontext(&m_ctx, &m_main_ctx);
    }

    void Yield_S()
    {
        getcontext(&m_ctx);

        uint32_t share_stack_used = (uint64_t)share_stack_sp + share_stack_size  - m_ctx.uc_mcontext.gregs[REG_RSP];

        if(!private_stack_sp)
        {
            private_stack_size = share_stack_used;
            private_stack_sp = new char[private_stack_size];

            private_buf_size = private_stack_size;

            memmove(private_stack_sp, (void*)(m_ctx.uc_mcontext.gregs[REG_RSP]), private_stack_size);
        }
        else
        {//used share stack size, may change bigger or smaller than last yeild
            if(private_stack_size < share_stack_used)
            {
                if(private_buf_size < share_stack_used)
                {
                    delete private_stack_sp;

                    private_stack_sp = new char[share_stack_used];
                    private_buf_size = share_stack_used;
                }
    
                private_stack_size = share_stack_used;

                memmove(private_stack_sp, (void*)(m_ctx.uc_mcontext.gregs[REG_RSP]), private_stack_size);
            }
            else
            {
                private_stack_size = share_stack_used;

                memmove(private_stack_sp, (void *)(m_ctx.uc_mcontext.gregs[REG_RSP]), private_stack_size);
            }
        }

        //setcontext(&m_main_ctx);
        swapcontext(&m_ctx, &m_main_ctx);
    }

    uint32_t GetTaskID()
    {
        return task_id;
    }

private:
    void Init()
    {
        makecontext(&m_ctx, Run, 1, this);
    }

    void SetTaskID(uint32_t _task_id)
    {
        task_id = _task_id;
    }

private:
    UCCoroutine(const UCCoroutine & uc_context)
    {}
    UCCoroutine & operator=(const UCCoroutine & uc_context)
    {}

    static void Run(void * arg)
    {
        UCCoroutine * coro = (UCCoroutine *)arg;

        printf("%s:[coro:%p,task_id:%u]\n", __FUNCTION__, arg, coro->task_id);

        coro->cb(coro->task_id);
    }

private:
    ucontext_t m_ctx;
    ucontext_t m_main_ctx;
    callback_t cb;

    char * private_stack_sp;
    uint32_t private_stack_size;
    uint32_t private_buf_size;

    char * share_stack_sp;
    uint32_t share_stack_size;

    uint32_t task_id;
};


