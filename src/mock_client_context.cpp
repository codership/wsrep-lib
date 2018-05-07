//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#include "trrep/transaction_context.hpp"
#include "mock_client_context.hpp"


int trrep::mock_client_context::apply(
    trrep::transaction_context& transaction_context __attribute__((unused)),
    const trrep::data& data __attribute__((unused)))

{
    assert(transaction_context.state() == trrep::transaction_context::s_executing);
    return (fail_next_applying_ ? 1 : 0);
}

int trrep::mock_client_context::commit(
    trrep::transaction_context& transaction_context)
{
    int ret(0);
    if (do_2pc())
    {
        if (transaction_context.before_prepare())
        {
            ret = 1;
        }
        else if (transaction_context.after_prepare())
        {
            ret = 1;
        }
    }
    if (ret == 0 &&
        (transaction_context.before_commit() ||
         transaction_context.ordered_commit() ||
         transaction_context.after_commit()))
    {
        ret = 1;
    }
    return ret;
}

int trrep::mock_client_context::rollback(
    trrep::transaction_context& transaction_context)
{
    int ret(0);
    if (transaction_context.before_rollback())
    {
        ret = 1;
    }
    else if (transaction_context.after_rollback())
    {
        ret = 1;
    }
    return ret;
}
