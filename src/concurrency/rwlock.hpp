#ifndef CONCURRENCY_RWLOCK_HPP_
#define CONCURRENCY_RWLOCK_HPP_

#include "concurrency/cond_var.hpp"
#include "containers/intrusive_list.hpp"


// A read/write lock with non-blocking constructors.

class rwlock_t;

enum class rwlock_access_t {
    read,
    write,
};

class rwlock_acq_t : private intrusive_list_node_t<rwlock_acq_t> {
public:
    // Doesn't block.  Use rwlock_acquisition_signal to see when the lock has been
    // acquired.
    rwlock_acq_t(rwlock_t *lock,
                 rwlock_access_t access);
    ~rwlock_acq_t();

    // A signal that gets pulsed when read access is available.
    signal_t *rwlock_read_signal();
    // A signal that gets pulsed when write access is available (only applicable when
    // `access` is `write`.)
    signal_t *rwlock_write_signal();

    rwlock_access_t access() const { return access_; }

private:
    friend class rwlock_t;
    friend class intrusive_list_t<rwlock_acq_t>;

    // Pulsed for locks with write access.
    cond_t write_cond_;
    // Pulsed for locks with read or write access.
    cond_t read_cond_;
    rwlock_t *lock_;
    rwlock_access_t access_;

    DISABLE_COPYING(rwlock_acq_t);
};

class rwlock_t {
public:
    rwlock_t();
    ~rwlock_t();

private:
    // rwlock_acq_t doesn't access acqs_ directly.
    friend class rwlock_acq_t;

    void add_acq(rwlock_acq_t *acq);
    void remove_acq(rwlock_acq_t *acq);

    void pulse_chain(rwlock_acq_t *acq);

    intrusive_list_t<rwlock_acq_t> acqs_;

    DISABLE_COPYING(rwlock_t);
};

#endif  // CONCURRENCY_RWLOCK_HPP_
