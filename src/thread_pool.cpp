//
// created by xujijun on 2018-03-19
//

#include <algorithm>
#include <iostream>
#include <utility>
#include "thread_pool.hpp"

namespace xjj {

    /*!
     * @brief 线程池最大线程数目，用户可自行修改
     */
    const ThreadPool::thread_num_type ThreadPool::MaxThreadNum = 9; // set upper bound for 4-core CPU

    /*!
     * @brief 线程池类构造函数
     * @param [in] thread_num 指定线程总数
     * @param [in] overload 是否允许过载，即等待任务数与工作任务数超过线程总数
     */
    ThreadPool::ThreadPool(thread_num_type thread_num, bool overload)
            : m_thread_num(static_cast<thread_num_type>(std::min(MaxThreadNum, thread_num))),
              m_overload(overload),
              m_running(false) {
        // 创建内部线程对象，但底层线程对象未创建
        for (thread_num_type i = 0; i < m_thread_num; i++) {
            auto thread_ptr =
                    std::make_shared<Thread>(
                            &m_waiting_queue,
                            &m_working_queue,
                            &m_finished_queue,
                             true);  // 默认在线程池发出终止指令时，选择继续执行等待队列中的任务
            m_thread_ptr_set.push_back(thread_ptr);
        }
    }

    /*!
     * @brief 析构函数：调用terminate终止线程池
     */
    ThreadPool::~ThreadPool() {
        terminate(true);  // 默认在线程池发出终止指令时，选择继续执行等待队列中的任务
    }

    /*!
     * @brief 启动线程池
     */
    void ThreadPool::start() {
        m_running = true;
        for (auto& thread_ptr : m_thread_ptr_set)
            thread_ptr -> start();
    }

    /*!
     * @brief 往线程池添加任务
     * @param [in] function 任务可执行对象
     * @param [in] task_id 任务id
     * @return 添加成功与否
     */
    bool ThreadPool::addTask(std::function<void()> function, int32_t task_id) {
        if (!m_running) // 线程池未在运行，此时添加任务会抛异常
            throw std::runtime_error("thread pool is not running.");

        // 在非过载模式下，判断是否过载，以过载则不继续添加任务
        if (!m_overload && m_waiting_queue.size() + m_working_queue.size() >= m_thread_num)
            return false;
        // 添加任务
        m_waiting_queue.push(Task(std::move(function), task_id));
        return true;
    }

    /*!
     * @brief 终止线程池
     * @param [in] wait_finish 线程池发出终止指令时，是否选择继续执行等待队列中的任务，默认为true
     */
    void ThreadPool::terminate(bool wait_finish) {
        if (!m_running)  // 未在运行则无需终止
            return;
        m_running = false;

        for (auto& thread_ptr : m_thread_ptr_set) {
            thread_ptr -> terminate(wait_finish);  // 对所有线程发出终止指令
        }

        for (auto& thread_ptr : m_thread_ptr_set) {
            thread_ptr -> join();  // 将所有线程置于分离状态
        }

        m_waiting_queue.clear();  // 清空等待队列
        m_finished_queue.clear();  // 清空完成队列
        m_thread_ptr_set.clear();  // 清空线程指针数组
    }

    /*!
     * @brief 获取一个已经完成的任务的id
     * @return 获取到的id（已完成任务队列为空时返回-1）
     */
    int32_t ThreadPool::getFinishedTaskId() {
        if (!m_finished_queue.empty()) {  // 完成队列非空时才可执行
            int32_t res;
            m_finished_queue.pop(res);  // 从等待队列队头获取一个id
            return res;
        }
        return -1;
    }

    /*!
     * @brief 创建线程时调用的函数，用于包裹Thread对象的run方法，设为static函数以限制只能在本文件内使用
     * @param [in] thread_ptr 调用线程指针
     * @return
     */
    static void *threadFunction(void* thread_ptr) {
        reinterpret_cast<ThreadPool::Thread*>(thread_ptr) -> run();
        return nullptr;
    }

    /*!
     * @brief 构造函数
     * @param [in,out] waiting_queue_ptr 等待队列指针
     * @param [in,out] working_queue_ptr 工作队列指针
     * @param [in,out] finished_queue_ptr 任务完成队列指针
     * @param [in] wait_finish 线程池发出终止指令时，是否选择继续执行等待队列中的任务，默认为true
     */
    ThreadPool::Thread::Thread(BlockingQueue<Task> *waiting_queue_ptr,
                               BlockingQueue<int> *working_queue_ptr,
                               BlockingQueue<int32_t> *finished_queue_ptr,
                               bool wait_finish)
            : m_waiting_queue_ptr(waiting_queue_ptr),
              m_working_queue_ptr(working_queue_ptr),
              m_finished_queue_ptr(finished_queue_ptr),
              m_wait_finish(wait_finish),
              m_running(false),
              m_thread_id(0) {}

    /*!
     * @brief 析构函数
     */
    ThreadPool::Thread::~Thread() {}

    /*!
     * @brief 启动线程
     * @return 创建线程是否成功
     */
    bool ThreadPool::Thread::start() {
        m_running = true;  // 将状态置为运行
        return 0 == pthread_create(&m_thread_id, nullptr, threadFunction, this);
    }

    /*!
     * @brief 执行线程工作
     */
    void ThreadPool::Thread::run() {
        // 循环获取等待队列中的任务
        while (true) {
            if (!m_running) {  // 判断是否在运行状态
                if (!m_wait_finish ||  // 不需要等待所有挂起任务被执行完
                        (m_wait_finish && m_waiting_queue_ptr -> empty())) // 需要等待挂起任务执行完，但是已经没有挂起任务
                    break;
            }

            Task task;

            // 定时获取一个等待队列队头任务，超时1秒返回
            if (!m_waiting_queue_ptr -> timedPop(task, 1))
                continue;

            // 工作队列中的线程数+1
            m_working_queue_ptr -> push(1);

            // 执行获取到的任务
            task.m_function();

            int i;

            // 工作队列中的线程数-1
            m_working_queue_ptr -> pop(i);

            if (task.m_task_id >= 0)
                m_finished_queue_ptr -> push(task.m_task_id);  // 将id非负的任务id加入完成队列
        }
    }

    /*!
     * @brief 将线程置于分离状态，等待指定线程终止
     * @return 操作是否成功
     */
    bool ThreadPool::Thread::join() {
        return 0 == pthread_join(m_thread_id, nullptr);
    }

    /*!
     * @brief 线程退出
     */
    void ThreadPool::Thread::exit() {
        pthread_exit(nullptr);
    }

    /*!
     * @brief 终止线程
     * @param [in] wait_finish 线程池发出终止指令时，是否选择继续执行等待队列中的任务
     */
    void ThreadPool::Thread::terminate(bool wait_finish) {
        m_running = false;  // 状态更改为停止运行
        m_wait_finish = wait_finish;  // 设置是否选择继续执行等待队列中的任务
    }

    /*!
     * @brief 任务类构造函数
     * @param [in] function 任务可执行对象，默认为nullptr
     * @param [in] task_id 任务id，默认为-1
     */
    ThreadPool::Task::Task(std::function<void()> function, int32_t task_id)
            : m_function(std::move(function)),
              m_task_id(task_id) {}

} // namespace xjj
