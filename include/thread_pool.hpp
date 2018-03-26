//
// created by xujijun on 2018-03-19
//

#ifndef _XJJ_THREAD_POOL_HPP
#define _XJJ_THREAD_POOL_HPP

#include <vector>
#include <functional>
#include "blocking_queue.hpp"

namespace xjj {
    /*!
     * @brief 线程池类 \class
     * 主要流程：
     * 1. 构造线程池
     * 2. 使用start启动线程池
     * 3. 往线程池中加入任务，多个线程争抢执行
     * 4. 使用terminate终止线程池
     */
    class ThreadPool {
    private:
        /*!
         * @brief 任务类 \struct
         */
        struct Task {
            /// 任务可执行对象
            std::function<void()> m_function;

            /// 任务id
            int32_t m_task_id;

            /*!
             * @brief 任务类构造函数
             * @param [in] function 任务可执行对象，默认为nullptr
             * @param [in] task_id 任务id，默认为-1
             */
            explicit Task(std::function<void()> function = nullptr, int32_t task_id = -1);
        };
    public:
        /*!
         * @brief 内部线程类 \class
         */
        class Thread {
        public:
            /*!
             * @brief 构造函数
             * @param [in,out] waiting_queue_ptr 等待队列指针
             * @param [in,out] working_queue_ptr 工作队列指针
             * @param [in,out] finished_queue_ptr 任务完成队列指针
             * @param [in] wait_finish 线程池发出终止指令时，是否选择继续执行等待队列中的任务，默认为true
             */
            Thread(BlockingQueue<Task> *waiting_queue_ptr,
                   BlockingQueue<int> *working_queue_ptr,
                   BlockingQueue<int32_t> *finished_queue_ptr,
                   bool wait_finish = true);

            /*!
             * @brief 析构函数
             */
            ~Thread();

            /*!
             * @brief 执行线程工作
             */
            void run();

            /*!
             * @brief 启动线程
             * @return 创建线程是否成功
             */
            bool start();

            /*!
             * @brief 终止线程
             * @param [in] wait_finish 线程池发出终止指令时，是否选择继续执行等待队列中的任务
             */
            void terminate(bool wait_finish);

            /*!
             * @brief 将线程置于分离状态，等待指定线程终止
             * @return 操作是否成功
             */
            bool join();

            /*!
             * @brief 线程退出
             */
            void exit();

        private:
            /// 线程id
            pthread_t m_thread_id;

            /// 线程是否处于运行状态
            bool m_running;

            /// 线程池发出终止指令时，是否选择继续执行等待队列中的任务
            bool m_wait_finish;

            /// 等待队列指针
            BlockingQueue<Task> *m_waiting_queue_ptr;

            /// 工作队列指针
            BlockingQueue<int> *m_working_queue_ptr;

            /// 任务完成队列指针
            BlockingQueue<int32_t > *m_finished_queue_ptr;
        };

        /*!
         * @brief 线程数目数据类型
         */
        typedef size_t thread_num_type;

        /*!
         * @brief 线程池类构造函数
         * @param [in] thread_num 指定线程总数
         * @param [in] overload 是否允许过载，即等待任务数与工作任务数超过线程总数
         */
        explicit ThreadPool(thread_num_type thread_num = 5, bool overload = true);

        /*!
         * @brief 析构函数：调用terminate终止线程池
         */
        ~ThreadPool();

        /*!
         * @brief 启动线程池
         */
        void start();

        /*!
         * @brief 往线程池添加任务
         * @param [in] function 任务可执行对象
         * @param [in] task_id 任务id
         * @return 添加成功与否
         */
        bool addTask(std::function<void()> function, int32_t task_id);

        /*!
         * @brief 获取一个已经完成的任务的id
         * @return 获取到的id（已完成任务队列为空时返回-1）
         */
        int32_t getFinishedTaskId();

        /*!
         * @brief 终止线程池
         * @param [in] wait_finish 线程池发出终止指令时，是否选择继续执行等待队列中的任务，默认为true
         */
        void terminate(bool wait_finish = true);

    private:

        /// 线程池是否处于运行状态
        bool m_running;

        /// 是否允许过载，即等待任务数与工作任务数超过线程总数
        bool m_overload;

        /// 线程数目
        thread_num_type m_thread_num;

        /// 线程指针数组
        std::vector<std::shared_ptr<Thread>> m_thread_ptr_set;

        /// 任务等待队列：存放任务
        BlockingQueue<Task> m_waiting_queue;

        /// 工作队列：只需记录工作中的任务的数目
        BlockingQueue<int> m_working_queue;

        /// 任务完成队列：存放已经完成的任务id
        BlockingQueue<int32_t> m_finished_queue;

        /// 线程池最大线程数目
        static const thread_num_type MaxThreadNum;
    };
} // namespace xjj

#endif