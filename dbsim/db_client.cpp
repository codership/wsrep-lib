//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#include "db_client.hpp"
#include "db_server.hpp"

#include "wsrep/logger.hpp"

db::client::client(db::server& server,
                   wsrep::client_id client_id,
                   enum wsrep::client_state::mode mode,
                   const db::params& params)
    : mutex_()
    , cond_()
    , params_(params)
    , server_(server)
    , server_state_(server.server_state())
    , client_state_(mutex_, cond_, this, server_state_, client_service_, client_id, mode)
    , client_service_(client_state_)
    , se_trx_(server.storage_engine())
    , stats_()
{ }

void db::client::start()
{
    client_state_.open(client_state_.id());
    for (size_t i(0); i < params_.n_transactions; ++i)
    {
        run_one_transaction();
        report_progress(i + 1);
    }
    client_state_.close();
    client_state_.cleanup();
}

bool db::client::bf_abort(wsrep::seqno seqno)
{
    return client_state_.bf_abort(seqno);
}

////////////////////////////////////////////////////////////////////////////////
//                              Private                                       //
////////////////////////////////////////////////////////////////////////////////

template <class F>
int db::client::client_command(F f)
{
    int err(client_state_.before_command());
    // wsrep::log_debug() << "before_command: " << err;
    // If err != 0, transaction was BF aborted while client idle
    if (err == 0)
    {
        err = client_state_.before_statement();
        if (err == 0)
        {
            err = f();
        }
        client_state_.after_statement();
    }
    client_state_.after_command_before_result();
    if (client_state_.current_error())
    {
        // wsrep::log_info() << "Current error";
        assert(client_state_.transaction().state() ==
               wsrep::transaction::s_aborted);
        err = 1;
    }
    client_state_.after_command_after_result();
    // wsrep::log_info() << "client_command(): " << err;
    return err;
}

void db::client::run_one_transaction()
{
    client_state_.reset_error();
    int err = client_command(
        [&]()
        {
            // wsrep::log_debug() << "Start transaction";
            err = client_state_.start_transaction(
                server_.next_transaction_id());
            assert(err == 0);
            se_trx_.start(this);
            return err;
        });

    const wsrep::transaction& transaction(
        client_state_.transaction());

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
            unsigned long long client_key(client_state_.id().get());
            key.append_key_part(&client_key, sizeof(client_key));
            key.append_key_part(&data, sizeof(data));
            err = client_state_.append_key(key);
            err = err || client_state_.append_data(
                wsrep::const_buffer(os.str().c_str(),
                                    os.str().size()));
            return err;
        });

    err = err || client_command(
        [&]()
        {
            // wsrep::log_debug() << "Commit";
            assert(err == 0);
            if (client_state_.do_2pc())
            {
                err = err || client_state_.before_prepare();
                err = err || client_state_.after_prepare();
            }
            err = err || client_state_.before_commit();
            if (err == 0) se_trx_.commit();
            err = err || client_state_.ordered_commit();
            err = err || client_state_.after_commit();
            if (err)
            {
                client_state_.before_rollback();
                se_trx_.rollback();
                client_state_.after_rollback();
            }
            return err;
        });

    assert(err ||
           transaction.state() == wsrep::transaction::s_aborted ||
           transaction.state() == wsrep::transaction::s_committed);
    assert(se_trx_.active() == false);
    assert(transaction.active() == false);

    switch (transaction.state())
    {
    case wsrep::transaction::s_committed:
        ++stats_.commits;
        break;
    case wsrep::transaction::s_aborted:
        ++stats_.rollbacks;
        break;
    default:
        assert(0);
    }
}

void db::client::report_progress(size_t i) const
{
    if ((i % 1000) == 0)
    {
        wsrep::log_info() << "client: " << client_state_.id().get()
                          << " transactions: " << i
                          << " " << 100*double(i)/params_.n_transactions << "%";
    }
}
