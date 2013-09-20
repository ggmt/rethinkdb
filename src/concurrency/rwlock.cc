#include "concurrency/rwlock.hpp"

rwlock_acq_t::rwlock_acq_t(rwlock_t *lock,
                           rwlock_access_t access)
    : lock_(lock), access_(access) {
    lock_->add_acq(this);
}

rwlock_acq_t::~rwlock_acq_t() {
    lock_->remove_acq(this);
}

signal_t *rwlock_acq_t::rwlock_read_signal() {
    return &read_cond_;
}

signal_t *rwlock_acq_t::rwlock_write_signal() {
    return &write_cond_;
}

rwlock_t::rwlock_t() { }

rwlock_t::~rwlock_t() {
    guarantee(acqs_.empty(), "An rwlock_t was destroyed while locks are held.");
}

void rwlock_t::add_acq(rwlock_acq_t *acq) {
    ASSERT_NO_CORO_WAITING;
    acqs_.push_back(acq);

    pulse_chain(acq);
}

void rwlock_t::remove_acq(rwlock_acq_t *acq) {
    ASSERT_NO_CORO_WAITING;
    rwlock_acq_t *next = acqs_.next(acq);
    acqs_.remove(acq);
    if (next != NULL) {
        pulse_chain(next);
    }
}

void rwlock_t::pulse_chain(rwlock_acq_t *const acq) {
    // This function pulses acq and subsequent nodes in the right places, if the
    // conditions are right -- a node gets pulsed for read access if the previous
    // node is pulsed and has read access, or if there's no previous node.
    rwlock_acq_t *const prev = acqs_.prev(acq);

    // Write node at beginning of chain gets write access.
    if (prev == NULL && acq->access_ != rwlock_access_t::read) {
        acq->read_cond_.pulse_if_not_already_pulsed();
        acq->write_cond_.pulse_if_not_already_pulsed();
        return;
    }

    // If the previous node has read access, subsequent nodes should have read
    // access, up to and including a write node.
    if (prev == NULL || (prev->access_ == rwlock_access_t::read
                         && prev->read_cond_.is_pulsed())) {
        // Don't re-pulse chains that have already been pulsed -- it would be
        // idempotent.
        if (acq->read_cond_.is_pulsed()) {
            return;
        }

        // Now pulse the nodes until we have pulsed a write node.
        rwlock_acq_t *node = acq;
        for (;;) {
            node->read_cond_.pulse();
            if (node->access_ != rwlock_access_t::read) {
                return;
            }
            node = acqs_.next(node);
            if (node == NULL) {
                return;
            }
        }
    }
}
