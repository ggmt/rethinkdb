#ifndef __FSM_BTREE_HPP__
#define __FSM_BTREE_HPP__

#include <assert.h>
#include "message_hub.hpp"
#include "buffer_cache/callbacks.hpp"
#include "worker_pool.hpp"

template <class config_t>
class btree_fsm : public cpu_message_t,
                  public block_available_callback<config_t>,
                  public transaction_begin_callback<config_t>,
                  public transaction_commit_callback<config_t>
{
public:
    typedef typename config_t::cache_t cache_t;
    typedef typename config_t::btree_fsm_t btree_fsm_t;
    //typedef typename config_t::worker_t worker_t;
    typedef typename cache_t::transaction_t transaction_t;
    typedef typename cache_t::buf_t buf_t;
    //typedef void (worker_t::*on_complete_t)(btree_fsm_t *btree_fsm);
    typedef void (*on_complete_t)(btree_fsm_t* btree_fsm);
public:
    enum transition_result_t {
        transition_incomplete,
        transition_ok,
        transition_complete
    };

public:
    btree_fsm(btree_key *_key)
        : cpu_message_t(cpu_message_t::mt_btree),
          transaction(NULL),
          cache(NULL),   // Will be set when we arrive at the core we are operating on
          on_complete(NULL), noreply(false)
        {
        keycpy(&key, _key);
    }
    virtual ~btree_fsm() {}

    /* TODO: This function will be called many times per each
     * operation (once for blocks that are kept in memory, and once
     * for each AIO request). In addition, for a btree of btrees, it
     * will get called once for the inner btree per each block. We
     * might want to consider hardcoding a switch statement instead of
     * using a virtual function, and testing the performance
     * difference. */

    // If event is null, we're initiating the first transition
    // (i.e. do_transition is getting called for the first time in
    // this fsm instance). After this, do_transition should only get
    // called on disk events (when a node has been read from disk).
    virtual transition_result_t do_transition(event_t *event) = 0;

    // Return true if the state machine is in a completed state
    virtual bool is_finished() = 0;

    virtual void on_block_available(buf_t *buf);
    virtual void on_txn_begin(transaction_t *txn);
    virtual void on_txn_commit(transaction_t *txn);

protected:
    block_id_t get_root_id(void *superblock_buf);

public:
    union {
        char key_memory[MAX_KEY_SIZE+sizeof(btree_key)];
        btree_key key;
    };
    transaction_t *transaction;
    cache_t *cache;
    on_complete_t on_complete;
    bool noreply;
};

#include "btree/fsm.tcc"

#endif // __FSM_BTREE_HPP__
