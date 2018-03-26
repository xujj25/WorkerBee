//
// created by xujijun on 2018-03-19
//

#include <mutex.hpp>

namespace xjj {

    /*!
     * @brief 构造函数
     */
    Mutex::Mutex() {
        pthread_mutex_init(&m_mutex, nullptr);
    }

    /*!
     * @brief 析构函数
     */
    Mutex::~Mutex() {
        pthread_mutex_destroy(&m_mutex);
    }

    /*!
     * @brief 互斥量加锁
     * @return 成功与否
     */
    bool Mutex::lock() {
        return 0 == pthread_mutex_lock(&m_mutex);
    }

    /*!
     * @brief 互斥量解锁
     * @return 成功与否
     */
    bool Mutex::unlock() {
        return 0 == pthread_mutex_unlock(&m_mutex);
    }

    /*!
     * @brief 获取底层互斥量id的指针
     * @return 底层互斥量id的指针
     */
    pthread_mutex_t* Mutex::getMutex() {
        return &m_mutex;
    }


    /*!
     * @brief 构造函数
     * @param [in] mutex_ptr 用于初始化底层互斥量的互斥量指针
     */
    AutoLockMutex::AutoLockMutex(Mutex* mutex_ptr)
        : m_mutex_ptr(mutex_ptr) {
        m_mutex_ptr -> lock();  // 构造时自动上锁
    }

    /*!
     * @brief 析构函数
     */
    AutoLockMutex::~AutoLockMutex() {
        m_mutex_ptr -> unlock();  // 析构时自动解锁
    }

} // namespace xjj