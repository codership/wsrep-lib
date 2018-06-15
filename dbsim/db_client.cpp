//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#include "db_client.hpp"


template <class F>
int db::client::client_command(F f)
{
    int err(client_context_.before_command());
    // wsrep::log_debug() << "before_command: " << err;
    // If err != 0, transaction was BF aborted while client idle
    if (err == 0)
    {
        err = client_context_.before_statement();
        if (err == 0)
        {
            err = f();
        }
        client_context_.after_statement();
    }
    client_context_.after_command_before_result();
    if (client_context_.current_error())
    {
        // wsrep::log_info() << "Current error";
        assert(client_context_.transaction().state() ==
               wsrep::transaction_context::s_aborted);
        err = 1;
    }
    client_context_.after_command_after_result();
    // wsrep::log_info() << "client_command(): " << err;
    return err;
}

void db::client::run_one_transaction()
{
    client_context_.reset_error();
    int err = client_command(
        [&]()
        {
            // wsrep::log_debug() << "Start transaction";
            err = client_context_.start_transaction(
                server_context_.next_transaction_id());
            assert(err == 0);
            se_trx_.start(this);
            return err;
        });

    const wsrep::transaction_context& transaction(
        client_context_.transaction());

    err = err || client_command(
        [&]()
        {
            // wsrep::log_debug() << "Generate write set";
            assert(transaction.active());
            assert(err == 0);
            int data(std::rand() % params_.n_rows);
            std::ostringstream os;
            os << data;
            wsrep::key key(wsrep::key::exclusive);
            key.append_key_part("dbms", 4);
            unsigned long long client_key(client_context_.id().get());
            key.append_key_part(&client_key, sizeof(client_key));
            key.append_key_part(&data, sizeof(data));
            err = client_context_.append_key(key);
            err = err || client_context_.append_data(
                wsrep::const_buffer(os.str().c_str(),
                                    os.str().size()));
            return err;
        });

    err = err || client_command(
        [&]()
        {
            // wsrep::log_debug() << "Commit";
            assert(err == 0);
            if (client_context_.do_2pc())
            {
                err = err || client_context_.before_prepare();
                err = err || client_context_.after_prepare();
            }
            err = err || client_context_.before_commit();
            if (err == 0) se_trx_.commit();
            err = err || client_context_.ordered_commit();
            err = err || client_context_.after_commit();
            if (err)
            {
                client_context_.rollback();
            }
            return err;
        });

    assert(err ||
           transaction.state() == wsrep::transaction_context::s_aborted ||
           transaction.state() == wsrep::transaction_context::s_committed);
    assert(se_trx_.active() == false);
    assert(transaction.active() == false);

    switch (transaction.state())
    {
    case wsrep::transaction_context::s_committed:
        ++stats_.commits;
        break;
    case wsrep::transaction_context::s_aborted:
        ++stats_.rollbacks;
        break;
    default:
        assert(0);
    }
}

bool db::client::bf_abort(wsrep::seqno seqno)
{
    wsrep::unique_lock<wsrep::mutex> lock(mutex_);
    return client_context_.bf_abort(lock, seqno);
}
