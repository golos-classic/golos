#include <boost/test/unit_test.hpp>

#include "worker_fixture.hpp"
#include "helpers.hpp"

#include <golos/plugins/worker_api/worker_api_plugin.hpp>

using namespace golos;
using namespace golos::protocol;
using namespace golos::plugins::worker_api;

struct worker_api_fixture : public golos::chain::worker_fixture {
    worker_api_fixture() : golos::chain::worker_fixture() {
        database_fixture::initialize<worker_api_plugin>();
        open_database();
        startup();
    }
};

BOOST_FIXTURE_TEST_SUITE(worker_api_plugin_techspec, worker_api_fixture)

BOOST_AUTO_TEST_CASE(worker_techspec_create) {
    BOOST_TEST_MESSAGE("Testing: worker_techspec_create");

    ACTORS((alice)(bob))
    generate_block();

    signed_transaction tx;

    const auto& wtmo_idx = db->get_index<worker_techspec_metadata_index, by_post>();

    comment_create("bob", bob_private_key, "bob-techspec", "", "bob-techspec");

    vote_operation vop;
    vop.voter = "alice";
    vop.author = "bob";
    vop.permlink = "bob-techspec";
    vop.weight = STEEMIT_100_PERCENT;
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, vop));
    generate_block();

    worker_techspec_operation op;
    op.author = "bob";
    op.permlink = "bob-techspec";
    op.specification_cost = ASSET_GOLOS(6);
    op.development_cost = ASSET_GOLOS(60);
    op.payments_interval = 60*60*24*2;
    op.payments_count = 2;
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, op));
    generate_block();

    BOOST_TEST_MESSAGE("-- Checking metadata creating");

    const auto& wto_post = db->get_comment("bob", string("bob-techspec"));
    auto wtmo_itr = wtmo_idx.find(wto_post.id);
    BOOST_CHECK(wtmo_itr != wtmo_idx.end());

    BOOST_TEST_MESSAGE("-- Checking metadata has NOT filled field modified");

    BOOST_CHECK_EQUAL(wtmo_itr->modified, fc::time_point_sec::min());

    BOOST_TEST_MESSAGE("-- Checking metadata has field net_rshares filled from post");

    BOOST_CHECK_EQUAL(wtmo_itr->net_rshares, wto_post.net_rshares);

    validate_database();
}

BOOST_AUTO_TEST_CASE(worker_techspec_modify) {
    BOOST_TEST_MESSAGE("Testing: worker_techspec_modify");

    ACTORS((alice)(bob))
    generate_block();

    signed_transaction tx;

    const auto& wtmo_idx = db->get_index<worker_techspec_metadata_index, by_post>();

    comment_create("bob", bob_private_key, "bob-techspec", "", "bob-techspec");

    worker_techspec_operation op;
    op.author = "bob";
    op.permlink = "bob-techspec";
    op.specification_cost = ASSET_GOLOS(6);
    op.development_cost = ASSET_GOLOS(60);
    op.payments_interval = 60*60*24*2;
    op.payments_count = 2;
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, op));
    generate_block();

    const auto& wto_post = db->get_comment("bob", string("bob-techspec"));
    auto wtmo_itr = wtmo_idx.find(wto_post.id);
    BOOST_CHECK(wtmo_itr != wtmo_idx.end());
    BOOST_CHECK_EQUAL(wtmo_itr->modified, fc::time_point_sec::min());

    BOOST_TEST_MESSAGE("-- Modifying worker techspec");

    auto now = db->head_block_time();

    op.payments_count = 3;
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, op));
    generate_block();

    wtmo_itr = wtmo_idx.find(wto_post.id);

    BOOST_TEST_MESSAGE("-- Checking metadata has updated field modified");

    BOOST_CHECK_EQUAL(wtmo_itr->modified, now);

    validate_database();
}

BOOST_AUTO_TEST_CASE(worker_techspec_vote) {
    BOOST_TEST_MESSAGE("Testing: worker_techspec_vote");

    ACTORS((alice)(bob))
    generate_block();

    signed_transaction tx;

    const auto& wtmo_idx = db->get_index<worker_techspec_metadata_index, by_post>();

    comment_create("bob", bob_private_key, "bob-techspec", "", "bob-techspec");

    worker_techspec_operation wtop;
    wtop.author = "bob";
    wtop.permlink = "bob-techspec";
    wtop.specification_cost = ASSET_GOLOS(6);
    wtop.development_cost = ASSET_GOLOS(60);
    wtop.payments_interval = 60*60*24*2;
    wtop.payments_count = 2;
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, wtop));
    generate_block();

    const auto& wto_post = db->get_comment("bob", string("bob-techspec"));
    auto wtmo_itr = wtmo_idx.find(wto_post.id);
    BOOST_CHECK(wtmo_itr != wtmo_idx.end());
    BOOST_CHECK_EQUAL(wtmo_itr->net_rshares, 0);

    BOOST_TEST_MESSAGE("-- Voting worker techspec post");

    vote_operation vop;
    vop.voter = "alice";
    vop.author = "bob";
    vop.permlink = "bob-techspec";
    vop.weight = STEEMIT_100_PERCENT;
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, vop));
    generate_block();

    const auto& wto_post_voted = db->get_comment("bob", string("bob-techspec"));
    wtmo_itr = wtmo_idx.find(wto_post_voted.id);
    BOOST_CHECK(wtmo_itr != wtmo_idx.end());

    BOOST_TEST_MESSAGE("-- Checking metadata has updated field net_rshares from post");

    BOOST_CHECK_EQUAL(wtmo_itr->net_rshares, wto_post_voted.net_rshares);

    validate_database();
}

BOOST_AUTO_TEST_CASE(worker_techspec_delete) {
    BOOST_TEST_MESSAGE("Testing: worker_techspec_delete");

    ACTORS((alice)(bob))
    auto private_key = create_approvers(0, 1);
    generate_block();

    signed_transaction tx;

    const auto& wtmo_idx = db->get_index<worker_techspec_metadata_index, by_post>();

    BOOST_TEST_MESSAGE("-- Creating techspec without approves");

    comment_create("bob", bob_private_key, "bob-techspec", "", "bob-techspec");

    worker_techspec_operation wtop;
    wtop.author = "bob";
    wtop.permlink = "bob-techspec";
    wtop.specification_cost = ASSET_GOLOS(6);
    wtop.development_cost = ASSET_GOLOS(60);
    wtop.payments_interval = 60*60*24*2;
    wtop.payments_count = 2;
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, wtop));
    generate_block();

    const auto& wto_post = db->get_comment("bob", string("bob-techspec"));
    auto wtmo_itr = wtmo_idx.find(wto_post.id);
    BOOST_CHECK(wtmo_itr != wtmo_idx.end());

    BOOST_TEST_MESSAGE("-- Deleting it");

    worker_techspec_delete_operation op;
    op.author = "bob";
    op.permlink = "bob-techspec";
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, op));
    generate_block();

    BOOST_TEST_MESSAGE("-- Checking worker techspec metadata object is deleted");

    wtmo_itr = wtmo_idx.find(wto_post.id);
    BOOST_CHECK(wtmo_itr == wtmo_idx.end());

    BOOST_TEST_MESSAGE("-- Creating techspec with 1 approve");

    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, wtop));
    generate_block();

    generate_blocks(STEEMIT_MAX_WITNESSES); // Enough for approvers to reach TOP-19 and not leave it

    worker_techspec_approve_operation wtaop;
    wtaop.approver = "approver0";
    wtaop.author = "bob";
    wtaop.permlink = "bob-techspec";
    wtaop.state = worker_techspec_approve_state::approve;
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, private_key, wtaop));
    generate_block();

    BOOST_TEST_MESSAGE("-- Deleting it");

    op.author = "bob";
    op.permlink = "bob-techspec";
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, op));
    generate_block();

    BOOST_TEST_MESSAGE("-- Checking worker techspec metadata object exists");

    wtmo_itr = wtmo_idx.find(wto_post.id);
    BOOST_CHECK(wtmo_itr != wtmo_idx.end());

    validate_database();
}

BOOST_AUTO_TEST_CASE(worker_techspec_approve) {
    BOOST_TEST_MESSAGE("Testing: worker_techspec_approve");

    ACTORS((alice)(bob)(carol)(dave))
    auto approver_key = create_approvers(0, STEEMIT_MAJOR_VOTED_WITNESSES);
    generate_block();

    signed_transaction tx;

    const auto& wtmo_idx = db->get_index<worker_techspec_metadata_index, by_post>();

    comment_create("bob", bob_private_key, "bob-techspec", "", "bob-techspec");

    worker_techspec_operation wtop;
    wtop.author = "bob";
    wtop.permlink = "bob-techspec";
    wtop.specification_cost = ASSET_GOLOS(6);
    wtop.development_cost = ASSET_GOLOS(60);
    wtop.payments_interval = 60*60*24*2;
    wtop.payments_count = 2;
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, wtop));
    generate_block();

    BOOST_TEST_MESSAGE("-- Checking approves are 0 before approving");

    const auto& wto_post = db->get_comment("bob", string("bob-techspec"));
    auto wtmo_itr = wtmo_idx.find(wto_post.id);
    BOOST_CHECK(wtmo_itr != wtmo_idx.end());
    BOOST_CHECK_EQUAL(wtmo_itr->approves, 0);
    BOOST_CHECK_EQUAL(wtmo_itr->disapproves, 0);

    generate_blocks(STEEMIT_MAX_WITNESSES); // Enough for approvers to reach TOP-19 and not leave it

    BOOST_TEST_MESSAGE("-- Approving worker techspec (after abstain)");

    worker_techspec_approve_operation op;
    op.approver = "approver0";
    op.author = "bob";
    op.permlink = "bob-techspec";
    op.state = worker_techspec_approve_state::approve;
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, approver_key, op));

    wtmo_itr = wtmo_idx.find(wto_post.id);
    BOOST_CHECK_EQUAL(wtmo_itr->approves, 1);
    BOOST_CHECK_EQUAL(wtmo_itr->disapproves, 0);

    BOOST_TEST_MESSAGE("-- Disapproving worker techspec (after approve)");

    op.state = worker_techspec_approve_state::disapprove;
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, approver_key, op));

    wtmo_itr = wtmo_idx.find(wto_post.id);
    BOOST_CHECK_EQUAL(wtmo_itr->approves, 0);
    BOOST_CHECK_EQUAL(wtmo_itr->disapproves, 1);

    BOOST_TEST_MESSAGE("-- Abstaining worker techspec (after disapprove)");

    op.state = worker_techspec_approve_state::abstain;
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, approver_key, op));

    wtmo_itr = wtmo_idx.find(wto_post.id);
    BOOST_CHECK_EQUAL(wtmo_itr->approves, 0);
    BOOST_CHECK_EQUAL(wtmo_itr->disapproves, 0);

    generate_block();

    BOOST_TEST_MESSAGE("-- Disapproving worker techspec (after abstain)");

    op.state = worker_techspec_approve_state::disapprove;
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, approver_key, op));

    wtmo_itr = wtmo_idx.find(wto_post.id);
    BOOST_CHECK_EQUAL(wtmo_itr->approves, 0);
    BOOST_CHECK_EQUAL(wtmo_itr->disapproves, 1);

    BOOST_TEST_MESSAGE("-- Approving worker techspec (after disapprove)");

    op.state = worker_techspec_approve_state::approve;
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, approver_key, op));

    wtmo_itr = wtmo_idx.find(wto_post.id);
    BOOST_CHECK_EQUAL(wtmo_itr->approves, 1);
    BOOST_CHECK_EQUAL(wtmo_itr->disapproves, 0);

    BOOST_TEST_MESSAGE("-- Abstaining worker techspec (after approve)");

    op.state = worker_techspec_approve_state::abstain;
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, approver_key, op));

    wtmo_itr = wtmo_idx.find(wto_post.id);
    BOOST_CHECK_EQUAL(wtmo_itr->approves, 0);
    BOOST_CHECK_EQUAL(wtmo_itr->disapproves, 0);

    {
        BOOST_TEST_MESSAGE("-- Approving techspec with preset worker and checking metadata fields");

        comment_create("dave", dave_private_key, "dave-techspec", "", "dave-techspec");

        wtop.author = "dave";
        wtop.permlink = "dave-techspec";
        wtop.worker = "dave";
        BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, dave_private_key, wtop));
        generate_block();

        auto now = approve_techspec_final("dave", "dave-techspec", approver_key);

        wtmo_itr = wtmo_idx.find(db->get_comment("dave", string("dave-techspec")).id);
        BOOST_CHECK_EQUAL(wtmo_itr->work_beginning_time, now);
        BOOST_CHECK_EQUAL(wtmo_itr->payment_beginning_time, fc::time_point_sec::min());
    }

    validate_database();
}

BOOST_AUTO_TEST_CASE(worker_techspec_approve_premade) {
    BOOST_TEST_MESSAGE("Testing: worker_techspec_approve_premade");

    ACTORS((alice)(bob))
    generate_block();

    signed_transaction tx;

    const auto& wtmo_idx = db->get_index<worker_techspec_metadata_index, by_post>();

    worker_techspec_operation wtop;
    wtop.author = "alice";
    wtop.permlink = "alice-premade";
    wtop.specification_cost = ASSET_GOLOS(6);
    wtop.development_cost = ASSET_GOLOS(60);
    wtop.payments_interval = 60*60*24;
    wtop.payments_count = 40;
    wtop.worker = "bob";
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, wtop));
    generate_block();

    BOOST_TEST_MESSAGE("-- Approving techspec and checking metadata fields");

    auto approver_key = create_approvers(0, STEEMIT_MAJOR_VOTED_WITNESSES);
    generate_blocks(STEEMIT_MAX_WITNESSES);
    auto now = approve_techspec_final("alice", "alice-premade", approver_key);

    auto wtmo_itr = wtmo_idx.find(db->get_comment("alice", string("alice-premade")).id);
    BOOST_CHECK_EQUAL(wtmo_itr->work_beginning_time, fc::time_point_sec::min());
    BOOST_CHECK_EQUAL(wtmo_itr->payment_beginning_time, now + wtop.payments_interval);
}

BOOST_AUTO_TEST_CASE(worker_assign) {
    BOOST_TEST_MESSAGE("Testing: worker_assign");

    ACTORS((alice)(bob))
    auto private_key = create_approvers(0, STEEMIT_MAJOR_VOTED_WITNESSES);
    generate_block();

    signed_transaction tx;

    const auto& wtmo_idx = db->get_index<worker_techspec_metadata_index, by_post>();

    comment_create("bob", bob_private_key, "bob-techspec", "", "bob-techspec");

    worker_techspec_operation wtop;
    wtop.author = "bob";
    wtop.permlink = "bob-techspec";
    wtop.specification_cost = ASSET_GOLOS(6);
    wtop.development_cost = ASSET_GOLOS(60);
    wtop.payments_interval = 60*60*24*2;
    wtop.payments_count = 2;
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, wtop));

    generate_block();

    BOOST_TEST_MESSAGE("-- Approving worker techspec by witnesses");

    generate_blocks(STEEMIT_MAX_WITNESSES); // Enough for approvers to reach TOP-19 and not leave it

    for (auto i = 0; i < STEEMIT_MAJOR_VOTED_WITNESSES; ++i) {
        const auto name = "approver" + std::to_string(i);
        worker_techspec_approve_operation wtaop;
        wtaop.approver = name;
        wtaop.author = "bob";
        wtaop.permlink = "bob-techspec";
        wtaop.state = worker_techspec_approve_state::approve;
        BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, private_key, wtaop));
    }

    const auto& wto_post = db->get_comment("bob", string("bob-techspec"));
    auto wtmo_itr = wtmo_idx.find(wto_post.id);
    BOOST_CHECK_EQUAL(wtmo_itr->work_beginning_time, time_point_sec::min());
    BOOST_CHECK_EQUAL(wtmo_itr->consumption_per_day, db->get_dynamic_global_properties().worker_consumption_per_day);

    BOOST_TEST_MESSAGE("-- Assigning worker");

    auto now = db->head_block_time();

    worker_assign_operation op;
    op.assigner = "bob";
    op.worker_techspec_author = "bob";
    op.worker_techspec_permlink = "bob-techspec";
    op.worker = "alice";
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, op));

    wtmo_itr = wtmo_idx.find(wto_post.id);

    BOOST_TEST_MESSAGE("-- Checking work beginning time is updated on assign");

    BOOST_CHECK_EQUAL(wtmo_itr->work_beginning_time, now);

    BOOST_TEST_MESSAGE("-- Unassigning worker");

    op.worker = "";
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, op));

    wtmo_itr = wtmo_idx.find(wto_post.id);

    BOOST_TEST_MESSAGE("-- Checking work beginning time is set to zero when unassign");

    BOOST_CHECK_EQUAL(wtmo_itr->work_beginning_time, time_point_sec::min());

    validate_database();
}

BOOST_AUTO_TEST_SUITE_END()
