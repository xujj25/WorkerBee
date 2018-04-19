//
// created by xujijun on 2018-03-19
//

#ifndef _XJJ_MUTEX_HPP
#define _XJJ_MUTEX_HPP

#include <pthread.h>
#include <memory>

namespace xjj {
    /*!
     * @brief 互斥量类 \class
     */
    class Mutex {
    public:

        /*!
         * @brief 构造函数
         */
        Mutex();

        /*!
         * @brief 拷贝构造函数，设为delete，阻止拷贝
         */
        Mutex(const Mutex&) = delete;

        /*!
         * @brief 赋值操作，设为delete，阻止赋值
         * @return Mutex& 
         */
        Mutex& operator=(const Mutex&) = delete;

        /*!
         * @brief 析构函数
         */
        ~Mutex();

        /*!
         * @brief 互斥量加锁
         * @return 成功与否
         */
        bool lock();

        /*!
         * @brief 互斥量解锁
         * @return 成功与否
         */
        bool unlock();

        /*!
         * @brief 获取底层互斥量id的指针
         * @return 底层互斥量id的指针
         */
        pthread_mutex_t* getMutex();

    private:

        /// 底层互斥量id
        pthread_mutex_t m_mutex;
    };

    /*!
     * @brief 自动加解锁互斥量类 \class
     * 底层数据类型为Mutex*，在构造函数中就进行加锁，
     * 在析构函数中进行解锁
     */
    class AutoLockMutex {
    public:

        /*!
         * @brief 构造函数
         * @param [in] mutex_ptr 用于初始化底层互斥量的互斥量指针
         */
        explicit AutoLockMutex(Mutex* mutex_ptr);

        /*!
         * @brief 拷贝构造函数，设为delete，阻止拷贝
         */
        AutoLockMutex(const AutoLockMutex&) = delete;

        /*!
         * @brief 赋值操作，设为delete，阻止赋值
         * @return AutoLockMutex& 
         */
        AutoLockMutex& operator=(const AutoLockMutex&) = delete;

        /*!
         * @brief 析构函数
         */
        ~AutoLockMutex();

    private:

        /// 底层互斥量类指针
        Mutex* m_mutex_ptr;
    };
} // namespace xjj

#endif