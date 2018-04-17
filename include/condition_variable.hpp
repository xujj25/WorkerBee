//
// created by xujijun on 2018-03-19
//

#ifndef _XJJ_CONDITION_VARIABLE_HPP
#define _XJJ_CONDITION_VARIABLE_HPP

#include <memory>
#include "mutex.hpp"

namespace xjj {
    /*!
     * @brief 条件变量类 \class
     */
    class ConditionVariable {
    public:

        /*!
         * @brief 构造函数
         */
        ConditionVariable();

        /*!
         * @brief 拷贝构造函数，设为delete，阻止拷贝
         */
        ConditionVariable(const ConditionVariable&) = delete;

        /*!
         * @brief 复制操作，设为delete，阻止赋值
         * @return ConditionVariable& 
         */
        ConditionVariable& operator=(const ConditionVariable&) = delete;

        /*!
         * @brief 析构函数
         */
        ~ConditionVariable();

        /*!
         * @brief 等候条件变量为真
         * @param [in] mutex_ptr 用于锁住条件变量的互斥量指针
         * @return 成功与否
         */
        bool wait(Mutex* mutex_ptr);

        /*!
         * @brief 定时等待条件变量为真
         * @param [in] mutex_ptr 用于锁住条件变量的互斥量指针
         * @param [in] seconds 等候时间长度，以秒为单位
         * @return 成功与否
         */
        bool timedWait(Mutex* mutex_ptr, long seconds);

        /*!
         * @brief 条件为真，唤醒一个等候条件变量变为真的线程
         * @return 成功与否
         */
        bool signal();

    private:

        /// 内部条件变量id
        pthread_cond_t m_cond;
    };
} // namespace xjj

#endif