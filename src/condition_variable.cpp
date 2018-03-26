//
// created by xujijun on 2018-03-19
//

#include <condition_variable.hpp>
#include <ctime>

namespace xjj {

    /*!
     * @brief 构造函数
     */
    ConditionVariable::ConditionVariable() {
        pthread_cond_init(&m_cond, nullptr);
    }

    /*!
     * @brief 析构函数
     */
    ConditionVariable::~ConditionVariable() {
        pthread_cond_destroy(&m_cond);
    }

    /*!
     * @brief 等候条件变量为真
     * @param [in] mutex_ptr 用于锁住条件变量的互斥量指针
     * @return 成功与否
     */
    bool ConditionVariable::wait(Mutex* mutex_ptr) {
        return 0 == pthread_cond_wait(&m_cond, mutex_ptr -> getMutex());
    }

    /*!
     * @brief 条件为真，唤醒一个等候条件变量变为真的线程
     * @return 成功与否
     */
    bool ConditionVariable::signal() {
        return 0 == pthread_cond_signal(&m_cond);
    }

    /*!
     * @brief 定时等待条件变量为真
     * @param [in] mutex_ptr 用于锁住条件变量的互斥量指针
     * @param [in] seconds 等候时间长度，以秒为单位
     * @return 成功与否
     */
    bool ConditionVariable::timedWait(Mutex *mutex_ptr, long seconds) {
        timespec now;
        clock_gettime(CLOCK_REALTIME, &now);  // 获取当前绝对时间
        now.tv_sec += seconds;  // 将当前时间加上等候的时间，作为等候条件变量变为真的时间
        return 0 == pthread_cond_timedwait(&m_cond, mutex_ptr -> getMutex(), &now);
    }

} // namespace xjj