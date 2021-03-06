#pragma once

#include <golos/protocol/operations.hpp>
#include <golos/chain/steem_object_types.hpp>
#include <golos/plugins/operation_history/history_object.hpp>

namespace golos { namespace plugins { namespace operation_history {

    struct applied_operation final {
        applied_operation();

        applied_operation(const operation_object&);

        golos::protocol::transaction_id_type trx_id;
        uint32_t block = 0;
        uint32_t trx_in_block = 0;
        uint16_t op_in_trx = 0;
        uint64_t virtual_op = 0;
        fc::time_point_sec timestamp;
        golos::protocol::operation op;
        string json_metadata;
    };

    struct donate_meta final {
        account_name_type referrer;
        asset referrer_interest = asset(0, STEEM_SYMBOL);
    };

} } } // golos::plugins::operation_history

FC_REFLECT(
    (golos::plugins::operation_history::applied_operation),
    (trx_id)(block)(trx_in_block)(op_in_trx)(virtual_op)(timestamp)(op)(json_metadata))

FC_REFLECT(
    (golos::plugins::operation_history::donate_meta),
    (referrer)(referrer_interest))
