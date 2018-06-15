//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#ifndef WSREP_DB_SIMULATOR_HPP
#define WSREP_DB_SIMULATOR_HPP

#include <memory>
#include <chrono>
#include <unordered_set>

#include "db_params.hpp"

namespace db
{
    class server;

    class simulator
    {
    public:
        simulator(const params& params)
            : mutex_()
            , params_(params)
            , servers_()
            , clients_start_()
            , clients_stop_()
            , stats_()
        { }

        void start();
        void stop();
        void donate_sst(server&,
                        const std::string& req, const wsrep::gtid& gtid, bool);
        const db::params& params() const
        { return params_; }
        std::string stats() const;
    private:
        std::string server_port(size_t i) const
        {
            std::ostringstream os;
            os << (10000 + (i + 1)*10);
            return os.str();
        }
        std::string build_cluster_address() const;

        wsrep::default_mutex mutex_;
        const db::params& params_;
        std::map<size_t, std::unique_ptr<server>> servers_;
        std::chrono::time_point<std::chrono::steady_clock> clients_start_;
        std::chrono::time_point<std::chrono::steady_clock> clients_stop_;
    public:
        struct stats
        {
            long long commits;
            long long aborts;
            long long replays;
            stats()
                : commits(0)
                , aborts(0)
                , replays(0)
            { }
        } stats_;
    };
}
#endif // WSRE_DB_SIMULATOR_HPP
