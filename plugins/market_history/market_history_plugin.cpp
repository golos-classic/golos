#include <golos/plugins/market_history/market_history_plugin.hpp>
#include <golos/plugins/json_rpc/api_helper.hpp>

#include <golos/chain/index.hpp>
#include <golos/chain/operation_notification.hpp>
#include <golos/chain/steem_objects.hpp>
#include <golos/chain/account_object.hpp>

#include <golos/protocol/exceptions.hpp>

#include <boost/algorithm/string.hpp>


namespace golos {
    namespace plugins {
        namespace market_history {

            using golos::protocol::fill_order_operation;
            using golos::chain::operation_notification;


            class market_history_plugin::market_history_plugin_impl {
            public:
                market_history_plugin_impl(market_history_plugin &plugin)
                        : _my(plugin),
                          _db(appbase::app().get_plugin<golos::plugins::chain::plugin>().db()){
                }

                ~market_history_plugin_impl() {
                }

                market_pair get_market_pair(asset asset1, asset asset2) const;
                market_pair get_market_pair(const market_str_pair& pair) const;

                market_ticker get_ticker(const market_pair& pair) const;
                market_volume get_volume(const market_pair& pair) const;
                order_book get_order_book(const market_pair& pair, uint32_t limit) const;
                order_book_extended get_order_book_extended(const market_pair& pair, uint32_t limit) const;
                vector<market_trade> get_trade_history(const market_pair& pair, time_point_sec start, time_point_sec end, uint32_t limit) const;
                vector<market_trade> get_recent_trades(const market_pair& pair, uint32_t limit) const;
                vector<bucket_object> get_market_history(const market_pair& pair, uint32_t bucket_seconds, time_point_sec start, time_point_sec end) const;
                flat_set<uint32_t> get_market_history_buckets() const;
                std::vector<limit_order> get_open_orders(const market_pair& pair, const std::string& owner) const;


                void update_market_histories(const golos::chain::operation_notification &o);

                golos::chain::database &database() const {
                    return _db;
                }

                market_history_plugin &_my;
                flat_set<uint32_t> _tracked_buckets = flat_set<uint32_t>  {15, 60, 300, 3600, 86400 };

                int32_t _maximum_history_per_bucket_size = 1000;

                golos::chain::database &_db;
            };

            void market_history_plugin::market_history_plugin_impl::update_market_histories(const operation_notification &o) {
                if (o.op.which() ==
                    operation::tag<fill_order_operation>::value) {
                    fill_order_operation op = o.op.get<fill_order_operation>();

                    auto &db = database();
                    const auto& bucket_idx = db.get_index<bucket_index, by_bucket>();

                    db.create<order_history_object>([&](auto& ho) {
                        ho.time = db.head_block_time();
                        ho.op = op;
                    });

                    if (!_maximum_history_per_bucket_size) {
                        return;
                    }
                    if (!_tracked_buckets.size()) {
                        return;
                    }

                    asset_symbol_type sym1;
                    asset_symbol_type sym2;
                    std::tie(sym1, sym2) = get_market_pair(op.open_pays, op.current_pays);

                    for (auto bucket : _tracked_buckets) {
                        auto cutoff = db.head_block_time() - fc::seconds(
                                bucket * _maximum_history_per_bucket_size);

                        auto open = fc::time_point_sec(
                                (db.head_block_time().sec_since_epoch() /
                                 bucket) * bucket);
                        auto seconds = bucket;

                        auto itr = bucket_idx.find(std::make_tuple(sym1, sym2, seconds, open));
                        if (itr == bucket_idx.end()) {
                            db.create<bucket_object>([&](auto& b) {
                                b.sym1 = sym1;
                                b.sym2 = sym2;

                                b.open = open;
                                b.seconds = bucket;

                                if (op.open_pays.symbol == sym1) {
                                    b.high_asset1 = op.open_pays.amount;
                                    b.high_asset2 = op.current_pays.amount;
                                    b.low_asset1 = op.open_pays.amount;
                                    b.low_asset2 = op.current_pays.amount;
                                    b.open_asset1 = op.open_pays.amount;
                                    b.open_asset2 = op.current_pays.amount;
                                    b.close_asset1 = op.open_pays.amount;
                                    b.close_asset2 = op.current_pays.amount;
                                    b.asset1_volume = op.open_pays.amount;
                                    b.asset2_volume = op.current_pays.amount;
                                } else {
                                    b.high_asset1 = op.current_pays.amount;
                                    b.high_asset2 = op.open_pays.amount;
                                    b.low_asset1 = op.current_pays.amount;
                                    b.low_asset2 = op.open_pays.amount;
                                    b.open_asset1 = op.current_pays.amount;
                                    b.open_asset2 = op.open_pays.amount;
                                    b.close_asset1 = op.current_pays.amount;
                                    b.close_asset2 = op.open_pays.amount;
                                    b.asset1_volume = op.current_pays.amount;
                                    b.asset2_volume = op.open_pays.amount;
                                }
                            });
                        } else {
                            db.modify(*itr, [&](auto& b) {
                                if (op.open_pays.symbol == sym1) {
                                    b.asset1_volume += op.open_pays.amount;
                                    b.asset2_volume += op.current_pays.amount;
                                    b.close_asset1 = op.open_pays.amount;
                                    b.close_asset2 = op.current_pays.amount;

                                    if (b.high() <
                                        price(op.current_pays, op.open_pays)) {
                                        b.high_asset1 = op.open_pays.amount;
                                        b.high_asset2 = op.current_pays.amount;
                                    }

                                    if (b.low() >
                                        price(op.current_pays, op.open_pays)) {
                                        b.low_asset1 = op.open_pays.amount;
                                        b.low_asset2 = op.current_pays.amount;
                                    }
                                } else {
                                    b.asset1_volume += op.current_pays.amount;
                                    b.asset2_volume += op.open_pays.amount;
                                    b.close_asset1 = op.current_pays.amount;
                                    b.close_asset2 = op.open_pays.amount;

                                    if (b.high() <
                                        price(op.open_pays, op.current_pays)) {
                                        b.high_asset1 = op.current_pays.amount;
                                        b.high_asset2 = op.open_pays.amount;
                                    }

                                    if (b.low() >
                                        price(op.open_pays, op.current_pays)) {
                                        b.low_asset1 = op.current_pays.amount;
                                        b.low_asset2 = op.open_pays.amount;
                                    }
                                }
                            });

                            if (_maximum_history_per_bucket_size > 0) {
                                open = fc::time_point_sec();
                                itr = bucket_idx.lower_bound(std::make_tuple(sym1, sym2, seconds, open));

                                while (itr->sym1 == sym1 && itr->sym2 == sym2 &&
                                       itr->seconds == seconds &&
                                       itr->open < cutoff) {
                                    auto old_itr = itr;
                                    ++itr;
                                    db.remove(*old_itr);
                                }
                            }
                        }
                    }
                }
            }

            market_pair market_history_plugin::market_history_plugin_impl::get_market_pair(asset asset1, asset asset2) const {
                auto sym1 = asset1.symbol;
                auto sym2 = asset2.symbol;
                if (sym2 == STEEM_SYMBOL
                    || (asset2.symbol_name() < asset1.symbol_name() && sym1 != STEEM_SYMBOL)) {
                    std::swap(sym1, sym2);
                }
                return market_pair(sym1, sym2);
            }

            market_pair market_history_plugin::market_history_plugin_impl::get_market_pair(const market_str_pair& pair) const {
                GOLOS_CHECK_PARAM(pair, {
                    GOLOS_CHECK_VALUE(pair.first.size() >= 3 && pair.first.size() <= 14, "pair.first must be between 3 and 14");
                    GOLOS_CHECK_VALUE(pair.second.size() >= 3 && pair.second.size() <= 14, "pair.second must be between 3 and 14");
                });
                auto sym_from_str = [&](std::string str) {
                    boost::to_upper(str);
                    asset_symbol_type sym = 0;
                    if (str == "GOLOS") {
                        sym = STEEM_SYMBOL;
                    } else if (str == "GBG") {
                        sym = SBD_SYMBOL;
                    } else {
                        sym = _db.get_asset(str).symbol();
                    }
                    return sym;
                };
                return get_market_pair(
                    asset(0, sym_from_str(pair.first)),
                    asset(0, sym_from_str(pair.second))
                );
            }

            market_ticker market_history_plugin::market_history_plugin_impl::get_ticker(const market_pair& pair) const {
                const auto& bucket_idx = _db.get_index<bucket_index, by_bucket>();
                auto itr = bucket_idx.lower_bound(std::make_tuple(pair.first, pair.second, 86400, database().head_block_time() - 86400));

                market_ticker result;

                if (itr != bucket_idx.end()) {
                    auto open = (asset(itr->open_asset2, pair.second) /
                                 asset(itr->open_asset1, pair.first)).to_real();
                    result.latest = (asset(itr->close_asset2, pair.second) /
                                     asset(itr->close_asset1, pair.first)).to_real();
                    result.percent_change =
                            ((result.latest - open) / open) * 100;
                } else {
                    result.latest = 0;
                    result.percent_change = 0;
                }

                auto orders = get_order_book(pair, 1);
                if (orders.bids.size()) {
                    result.highest_bid = orders.bids[0].price;
                }
                if (orders.asks.size()) {
                    result.lowest_ask = orders.asks[0].price;
                }

                auto volume = get_volume(pair);
                result.asset1_volume = volume.asset1_volume;
                result.asset2_volume = volume.asset2_volume;

                return result;
            }

            market_volume market_history_plugin::market_history_plugin_impl::get_volume(const market_pair& pair) const {
                const auto& bucket_idx = _db.get_index<bucket_index, by_bucket>();
                auto itr = bucket_idx.lower_bound(std::make_tuple(pair.first, pair.second, 0, database().head_block_time() - 86400));
                market_volume result;
                result.asset1_volume = asset(0, pair.first);
                result.asset2_volume = asset(0, pair.second);

                uint32_t bucket_size = itr->seconds;
                while (itr != bucket_idx.end() &&
                        itr->sym1 == pair.first && itr->sym2 == pair.second &&
                        itr->seconds == bucket_size) {
                    result.asset1_volume.amount += itr->asset1_volume;
                    result.asset2_volume.amount += itr->asset2_volume;

                    ++itr;
                }

                return result;
            }

            order_book market_history_plugin::market_history_plugin_impl::get_order_book(const market_pair& pair, uint32_t limit) const {
                const auto& order_idx = _db.get_index<golos::chain::limit_order_index, golos::chain::by_price>();
                auto itr = order_idx.lower_bound(price::max(pair.second, pair.first));

                order_book result;

                while (itr != order_idx.end() &&
                       itr->sell_price.base.symbol == pair.second &&
                       itr->sell_price.quote.symbol == pair.first &&
                       result.bids.size() < limit) {
                    order cur;
                    cur.price = itr->sell_price.base.to_real() / itr->sell_price.quote.to_real();
                    cur.asset1 = (asset(itr->for_sale, pair.second) * itr->sell_price).amount;
                    cur.asset2 = itr->for_sale;
                    result.bids.push_back(cur);
                    ++itr;
                }

                itr = order_idx.lower_bound(price::max(pair.first, pair.second));

                while (itr != order_idx.end() &&
                       itr->sell_price.base.symbol == pair.first &&
                       itr->sell_price.quote.symbol == pair.second &&
                       result.asks.size() < limit) {
                    order cur;
                    cur.price = itr->sell_price.quote.to_real() / itr->sell_price.base.to_real();
                    cur.asset1 = itr->for_sale;
                    cur.asset2 = (asset(itr->for_sale, pair.first) * itr->sell_price).amount;
                    result.asks.push_back(cur);
                    ++itr;
                }

                return result;
            }

            order_book_extended market_history_plugin::market_history_plugin_impl::get_order_book_extended(const market_pair& pair, uint32_t limit) const {
                order_book_extended result;

                auto max_sell = price::max(pair.second, pair.first);
                auto max_buy = price::max(pair.first, pair.second);
                const auto& limit_price_idx = _db.get_index<golos::chain::limit_order_index, golos::chain::by_price>();
                auto sell_itr = limit_price_idx.lower_bound(max_sell);
                auto buy_itr = limit_price_idx.lower_bound(max_buy);
                auto end = limit_price_idx.end();

                while (sell_itr != end &&
                       sell_itr->sell_price.base.symbol == pair.second &&
                       sell_itr->sell_price.quote.symbol == pair.first &&
                       result.bids.size() < limit) {
                    auto itr = sell_itr;
                    order_extended cur;
                    cur.order_price = itr->sell_price;
                    cur.real_price = (cur.order_price).to_real();
                    cur.asset2 = itr->for_sale;
                    cur.asset1 = (asset(itr->for_sale, pair.second) * cur.order_price).amount;
                    cur.created = itr->created;
                    result.bids.push_back(cur);
                    ++sell_itr;
                }
                while (buy_itr != end &&
                       buy_itr->sell_price.base.symbol == pair.first &&
                       buy_itr->sell_price.quote.symbol == pair.second &&
                       result.asks.size() < limit) {
                    auto itr = buy_itr;
                    order_extended cur;
                    cur.order_price = itr->sell_price;
                    cur.real_price = (~cur.order_price).to_real();
                    cur.asset1 = itr->for_sale;
                    cur.asset2 = (asset(itr->for_sale, pair.first) * cur.order_price).amount;
                    cur.created = itr->created;
                    result.asks.push_back(cur);
                    ++buy_itr;
                }

                return result;
            }


            vector<market_trade> market_history_plugin::market_history_plugin_impl::get_trade_history(
                    const market_pair& pair, time_point_sec start, time_point_sec end, uint32_t limit) const {
                const auto& bucket_idx = _db.get_index<order_history_index, by_time>();
                auto itr = bucket_idx.lower_bound(start);

                std::vector<market_trade> result;

                while (itr != bucket_idx.end() && itr->time <= end &&
                       result.size() < limit) {
                    if (!(itr->op.open_pays.symbol == pair.first && itr->op.current_pays.symbol == pair.second)
                        && !(itr->op.open_pays.symbol == pair.second && itr->op.current_pays.symbol == pair.first)) {
                        ++itr;
                        continue;
                    }
                    market_trade trade;
                    trade.date = itr->time;
                    trade.current_pays = itr->op.current_pays;
                    trade.open_pays = itr->op.open_pays;
                    result.push_back(trade);
                    ++itr;
                }

                return result;
            }

            vector<market_trade> market_history_plugin::market_history_plugin_impl::get_recent_trades(const market_pair& pair, uint32_t limit) const {
                const auto& order_idx = _db.get_index<order_history_index, by_time>();
                auto itr = order_idx.rbegin();

                vector<market_trade> result;

                while (itr != order_idx.rend() && result.size() < limit) {
                    if (!(itr->op.open_pays.symbol == pair.first && itr->op.current_pays.symbol == pair.second)
                        && !(itr->op.open_pays.symbol == pair.second && itr->op.current_pays.symbol == pair.first)) {
                        ++itr;
                        continue;
                    }
                    market_trade trade;
                    trade.date = itr->time;
                    trade.current_pays = itr->op.current_pays;
                    trade.open_pays = itr->op.open_pays;
                    result.push_back(trade);
                    ++itr;
                }

                return result;
            }

            vector<bucket_object> market_history_plugin::market_history_plugin_impl::get_market_history(
                    const market_pair& pair, uint32_t bucket_seconds, time_point_sec start, time_point_sec end) const {
                const auto& bucket_idx = _db.get_index<bucket_index, by_bucket>();
                auto itr = bucket_idx.lower_bound(std::make_tuple(pair.first, pair.second, bucket_seconds, start));

                std::vector<bucket_object> result;

                while (itr != bucket_idx.end() &&
                       itr->sym1 == pair.first && itr->sym2 == pair.second &&
                       itr->seconds == bucket_seconds && itr->open < end) {
                    result.push_back(*itr);

                    ++itr;
                }

                return result;
            }

            flat_set<uint32_t> market_history_plugin::market_history_plugin_impl::get_market_history_buckets() const {
                return appbase::app().get_plugin<market_history_plugin>().get_tracked_buckets();
            }

            std::vector<limit_order> market_history_plugin::market_history_plugin_impl::get_open_orders(const market_pair& pair, const string& owner) const {
                return _db.with_weak_read_lock([&]() {
                    std::vector<limit_order> result;
                    const auto& idx = _db.get_index<golos::chain::limit_order_index, golos::chain::by_account>();
                    auto itr = idx.lower_bound(owner);
                    while (itr != idx.end() && itr->seller == owner) {
                        if (itr->sell_price.base.symbol == pair.first && itr->sell_price.quote.symbol == pair.second) {
                            result.push_back(*itr);
                            result.back().real_price = (~result.back().sell_price).to_real();
                        } else if (itr->sell_price.base.symbol == pair.second && itr->sell_price.quote.symbol == pair.first) {
                            result.push_back(*itr);
                            result.back().real_price = (result.back().sell_price).to_real();
                        }
                        ++itr;
                    }
                    return result;
                });
            }

            market_history_plugin::market_history_plugin() {
            }

            market_history_plugin::~market_history_plugin() {
            }

            void market_history_plugin::set_program_options(
                    boost::program_options::options_description &cli,
                    boost::program_options::options_description &cfg
            ) {
                cfg.add_options()
                        ("market-history-bucket-size",
                         boost::program_options::value<string>()->default_value("[15,60,300,3600,86400]"),
                         "Track market history by grouping orders into buckets of equal size measured in seconds specified as a JSON array of numbers")
                        ("market-history-buckets-per-size",
                         boost::program_options::value<uint32_t>()->default_value(5760),
                         "How far back in time to track history for each bucket size, measured in the number of buckets (default: 5760)");
            }

            void market_history_plugin::plugin_initialize(const boost::program_options::variables_map &options) {
                try {
                    ilog("market_history plugin: plugin_initialize() begin");
                    _my.reset(new market_history_plugin_impl(*this));
                    golos::chain::database& db = _my->database();

                    db.post_apply_operation.connect(
                            [&](const golos::chain::operation_notification &o) { _my->update_market_histories(o); });
                    golos::chain::add_plugin_index<bucket_index>(db);
                    golos::chain::add_plugin_index<order_history_index>(db);

                    if (options.count("bucket-size")) {
                        std::string buckets = options["bucket-size"].as<string>();
                        _my->_tracked_buckets = fc::json::from_string(buckets).as<flat_set<uint32_t>>();
                    }
                    if (options.count("history-per-size")) {
                        _my->_maximum_history_per_bucket_size = options["history-per-size"].as<uint32_t>();
                    }

                    wlog("bucket-size ${b}", ("b", _my->_tracked_buckets));
                    wlog("history-per-size ${h}", ("h", _my->_maximum_history_per_bucket_size));

                    ilog("market_history plugin: plugin_initialize() end");
                    JSON_RPC_REGISTER_API ( name() ) ;
                } FC_CAPTURE_AND_RETHROW()
            }

            void market_history_plugin::plugin_startup() {
                ilog("market_history plugin: plugin_startup() begin");

                ilog("market_history plugin: plugin_startup() end");
            }

            void market_history_plugin::plugin_shutdown() {
                ilog("market_history plugin: plugin_shutdown() begin");

                ilog("market_history plugin: plugin_shutdown() end");
            }

            flat_set<uint32_t> market_history_plugin::get_tracked_buckets() const {
                return _my->_tracked_buckets;
            }

            uint32_t market_history_plugin::get_max_history_per_bucket() const {
                return _my->_maximum_history_per_bucket_size;
            }


            // Api Defines

            DEFINE_API(market_history_plugin, get_ticker) {
                PLUGIN_API_VALIDATE_ARGS(
                    (market_str_pair, pair, market_str_pair("GOLOS", "GBG"))
                );
                auto &db = _my->database();
                return db.with_weak_read_lock([&]() {
                    return _my->get_ticker(_my->get_market_pair(pair));
                });
            }

            DEFINE_API(market_history_plugin, get_volume) {
                PLUGIN_API_VALIDATE_ARGS(
                    (market_str_pair, pair, market_str_pair("GOLOS", "GBG"))
                );
                auto &db = _my->database();
                return db.with_weak_read_lock([&]() {
                    return _my->get_volume(_my->get_market_pair(pair));
                });
            }

            DEFINE_API(market_history_plugin, get_order_book) {
                PLUGIN_API_VALIDATE_ARGS(
                    (uint32_t, limit)
                    (market_str_pair, pair, market_str_pair("GOLOS", "GBG"))
                );
                GOLOS_CHECK_LIMIT_PARAM(limit, 500);

                auto &db = _my->database();
                return db.with_weak_read_lock([&]() {
                    return _my->get_order_book(_my->get_market_pair(pair), limit);
                });
            }

            DEFINE_API(market_history_plugin, get_order_book_extended) {
                PLUGIN_API_VALIDATE_ARGS(
                    (uint32_t, limit)
                    (market_str_pair, pair, market_str_pair("GOLOS", "GBG"))
                );
                GOLOS_CHECK_LIMIT_PARAM(limit, 1000);

                auto &db = _my->database();
                return db.with_weak_read_lock([&]() {
                    return _my->get_order_book_extended(_my->get_market_pair(pair), limit);
                });
            }


            DEFINE_API(market_history_plugin, get_trade_history) {
                PLUGIN_API_VALIDATE_ARGS(
                    (time_point_sec, start)
                    (time_point_sec, end)
                    (uint32_t,       limit)
                    (market_str_pair, pair, market_str_pair("GOLOS", "GBG"))
                );
                GOLOS_CHECK_LIMIT_PARAM(limit, 1000);

                auto &db = _my->database();
                return db.with_weak_read_lock([&]() {
                    return _my->get_trade_history(_my->get_market_pair(pair), start, end, limit);
                });
            }

            DEFINE_API(market_history_plugin, get_recent_trades) {
                PLUGIN_API_VALIDATE_ARGS(
                    (uint32_t, limit)
                    (market_str_pair, pair, market_str_pair("GOLOS", "GBG"))
                );
                GOLOS_CHECK_LIMIT_PARAM(limit, 1000);

                auto &db = _my->database();
                return db.with_weak_read_lock([&]() {
                    return _my->get_recent_trades(_my->get_market_pair(pair), limit);
                });
            }

            DEFINE_API(market_history_plugin, get_market_history) {
                PLUGIN_API_VALIDATE_ARGS(
                    (uint32_t,       bucket_seconds)
                    (time_point_sec, start)
                    (time_point_sec, end)
                    (market_str_pair,    pair, market_str_pair("GOLOS", "GBG"))
                );
                auto &db = _my->database();
                return db.with_weak_read_lock([&]() {
                    return _my->get_market_history(_my->get_market_pair(pair), bucket_seconds, start, end);
                });
            }

            DEFINE_API(market_history_plugin, get_market_history_buckets) {
                PLUGIN_API_VALIDATE_ARGS();
                auto &db = _my->database();
                return db.with_weak_read_lock([&]() {
                    return _my->get_market_history_buckets();
                });
            }

            DEFINE_API(market_history_plugin, get_open_orders) {
                PLUGIN_API_VALIDATE_ARGS(
                    (string, account)
                    (market_str_pair, pair, market_str_pair("GOLOS", "GBG"))
                );
                auto &db = _my->database();
                return db.with_weak_read_lock([&]() {
                    return _my->get_open_orders(_my->get_market_pair(pair), account);
                });
            }

        }
    }
} // golos::plugins::market_history
