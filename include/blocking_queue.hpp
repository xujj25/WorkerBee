//
// created by xujijun on 2018-03-19
//

#ifndef _XJJ_BLOCKING_QUEUE_HPP
#define _XJJ_BLOCKING_QUEUE_HPP

#include <queue>
#include <climits>
#include "condition_variable.hpp"

namespace xjj {
    /*!
     * @brief 阻塞队列模板类 \class
     * 主要的实现维护一个queue，使用条件变量和互斥量来进行队空和队满时的阻塞
     * @tparam T 模板参数类型
     */
    template <typename T>
    class BlockingQueue {
    public:
        /// 队列长度数据类型
        typedef size_t size_type;

        /// 队列长度默认上限
        static const size_type DefaultMaxLen = static_cast<size_type>(ULLONG_MAX);

        /*!
         * @brief 构造函数
         * @param [in] max_len 队列最大长度，默认为size_type可表示的最大值
         */
        explicit BlockingQueue(size_type max_len = DefaultMaxLen)
                : m_max_len(max_len) {}

        /*!
         * @brief 入队函数
         * @param [in] element 入队对象const引用
         */
        void push(const T& element) {
            {  // 使用自动加锁互斥量
                AutoLockMutex autoLockMutex(&m_mutex);
                // 当队列已超队长上限，则等待队列未满条件
                while (m_queue.size() >= m_max_len) {
                    m_not_full_cond_var.wait(&m_mutex);
                }
                m_queue.push(element);
            }  // 离开作用域自动释放互斥量
            m_not_empty_cond_var.signal();  // 插入成功，队列非空条件为真，唤醒等候出队线程
        }

        /*!
         * @brief 出队函数
         * @param [out] element 队头对象引用
         */
        void pop(T& element) {
            {
                AutoLockMutex autoLockMutex(&m_mutex);
                // 当队列为空，则等待队列非空条件
                while (m_queue.empty()) {
                    m_not_empty_cond_var.wait(&m_mutex);
                }
                // 获取队头对象
                element = m_queue.front();
                m_queue.pop();  // 弹出队头对象
            }
            m_not_full_cond_var.signal();  // 出队成功，队长不超上限，唤醒等候入队线程
        }

        /*!
         * @brief 定时出队函数
         * @param [out] element 队头对象引用
         * @param [in] seconds 等候时间，以秒为单位
         * @return 出队操作成功与否
         */
        bool timedPop(T& element, long seconds) {
            bool can_pop = false;  // 出队是否成功状态
            {
                AutoLockMutex autoLockMutex(&m_mutex);
                if (m_queue.empty())  // 队列为空，等候seconds秒后队列是否非空
                    m_not_empty_cond_var.timedWait(&m_mutex, seconds);

                // 队列非空条件为真，或者超时，对队列是否非空进行判断
                if (!m_queue.empty()) {  // 队列非空，执行出队操作
                    can_pop = true;
                    element = m_queue.front();
                    m_queue.pop();
                }
            }
            m_not_full_cond_var.signal();
            return can_pop;
        }

        /*!
         * @brief 清空阻塞队列
         */
        void clear() {
            AutoLockMutex autoLockMutex(&m_mutex);
            while (!m_queue.empty())
                m_queue.pop();
        }

        /*!
         * 判断阻塞队列判空
         * @return 队列是否为空
         */
        bool empty() {
            AutoLockMutex autoLockMutex(&m_mutex);
            return m_queue.empty();
        }

        /*!
         * 获取阻塞队列长度
         * @return 阻塞队列长度
         */
        size_type size() {
            AutoLockMutex autoLockMutex(&m_mutex);
            return m_queue.size();
        }

    private:
        /// 队列互斥量
        Mutex m_mutex;

        /// 队长上限
        size_type m_max_len;

        /// 内部队列对象
        std::queue<T> m_queue;

        /// 队长不超上限 条件变量
        ConditionVariable m_not_full_cond_var;

        /// 队列非空 条件变量
        ConditionVariable m_not_empty_cond_var;
    };
} // namespace xjj

#endif