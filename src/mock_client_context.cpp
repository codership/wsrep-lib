//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#include "trrep/transaction_context.hpp"
#include "mock_client_context.hpp"


int trrep::mock_client_context::apply(
    const trrep::data& data __attribute__((unused)))

{
    assert(transaction_.state() == trrep::transaction_context::s_executing);
    return (fail_next_applying_ ? 1 : 0);
}

int trrep::mock_client_context::commit()
{
    int ret(0);
    if (do_2pc())
    {
        if (transaction_.before_prepare())
        {
            ret = 1;
        }
        else if (transaction_.after_prepare())
        {
            ret = 1;
        }
    }
    if (ret == 0 &&
        (transaction_.before_commit() ||
         transaction_.ordered_commit() ||
         transaction_.after_commit()))
    {
        ret = 1;
    }
    return ret;
}

int trrep::mock_client_context::rollback()
{
    int ret(0);
    if (transaction_.before_rollback())
    {
        ret = 1;
    }
    else if (transaction_.after_rollback())
    {
        ret = 1;
    }
    return ret;
}
