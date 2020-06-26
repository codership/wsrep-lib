/*
 * Copyright (C) 2020 Codership Oy <info@codership.com>
 *
 * This file is part of wsrep-lib.
 *
 * Wsrep-lib is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * Wsrep-lib is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with wsrep-lib.  If not, see <https://www.gnu.org/licenses/>.
 */

/** @file db_tls.cpp
 *
 * This file demonstrates the use of TLS service. It does not implement
 * real encryption, but may manipulate stream bytes for testing purposes.
 */

#include "db_tls.hpp"

#include "wsrep/logger.hpp"

#include <unistd.h> // read()
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h> // send()
#include <cassert>
#include <cerrno>
#include <cstring>

#include <mutex>
#include <string>

namespace
{
    class db_stream : public wsrep::tls_stream
    {
    public:
        db_stream(int fd, int mode)
            : fd_(fd)
            , state_(s_initialized)
            , last_error_()
            , mode_(mode)
            , stats_()
            , is_blocking_()
        {
            int val(fcntl(fd_, F_GETFL, 0));
            is_blocking_ = not (val & O_NONBLOCK);
        }
        struct stats
        {
            size_t bytes_read{0};
            size_t bytes_written{0};
        };

        /*
         *    in            idle --|
         *     |-> ch -|      ^    | -> want_read  --|
         *     |-> sh -| ---- |--> | -> want_write --|
         *                    |----------------------|
         */
        enum state
        {
            s_initialized,
            s_client_handshake,
            s_server_handshake,
            s_idle,
            s_want_read,
            s_want_write
        };

        int get_error_number() const { return last_error_; }
        const void* get_error_category() const
        {
            return reinterpret_cast<const void*>(1);
        }

        static char* get_error_message(int value, const void*)
        {
            return ::strerror(value);
        }

        enum wsrep::tls_service::status client_handshake();

        enum wsrep::tls_service::status server_handshake();

        wsrep::tls_service::op_result read(void*, size_t);

        wsrep::tls_service::op_result write(const void*, size_t);

        enum state state() const { return state_; }

        int fd() const { return fd_; }
        void inc_reads(size_t val) { stats_.bytes_read += val; }
        void inc_writes(size_t val) { stats_.bytes_written += val; }
        const stats& get_stats() const { return stats_; }
    private:
        enum wsrep::tls_service::status handle_handshake_read(const char* expect);
        size_t determine_read_count(size_t max_count)
        {
            if (is_blocking_ || mode_ < 2) return max_count;
            else if (::rand() % 100 == 0) return std::min(size_t(42), max_count);
            else return max_count;
        }
        size_t determine_write_count(size_t count)
        {
            if (is_blocking_ || mode_ < 2) return count;
            else if (::rand() % 100 == 0) return std::min(size_t(43), count);
            else return count;
        }

        ssize_t do_read(void* buf, size_t max_count)
        {
            if (is_blocking_ || mode_ < 3 )
                return ::read(fd_, buf, max_count);
            else if (::rand() % 1000 == 0) return EINTR;
            else return ::read(fd_, buf, max_count);
        }

        ssize_t do_write(const void* buf, size_t count)
        {
            if (is_blocking_ || mode_ < 3)
                return ::send(fd_, buf, count, MSG_NOSIGNAL);
            else if (::rand() % 1000 == 0) return EINTR;
            else return ::send(fd_, buf, count, MSG_NOSIGNAL);
        }

        wsrep::tls_service::op_result map_success(ssize_t result)
        {
            if (is_blocking_ || mode_ < 2)
            {
                return wsrep::tls_service::op_result{
                    wsrep::tls_service::success, size_t(result)};
            }
            else if (::rand() % 1000 == 0)
            {
                wsrep::log_info() << "Success want extra read";
                state_ = s_want_read;
                return wsrep::tls_service::op_result{
                    wsrep::tls_service::want_read, size_t(result)};
            }
            else if (::rand() % 1000 == 0)
            {
                wsrep::log_info() << "Success want extra write";
                state_ = s_want_write;
                return wsrep::tls_service::op_result{
                    wsrep::tls_service::want_write, size_t(result)};
            }
            else
            {
                return wsrep::tls_service::op_result{
                    wsrep::tls_service::success, size_t(result)};
            }
        }

        wsrep::tls_service::op_result map_result(ssize_t result)
        {
            if (result > 0)
            {
                return map_success(result);
            }
            else if (result == 0)
            {
                return wsrep::tls_service::op_result{
                    wsrep::tls_service::eof, 0};
            }
            else if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                return wsrep::tls_service::op_result{
                    wsrep::tls_service::want_read, 0};
            }
            else
            {
                last_error_ = errno;
                return wsrep::tls_service::op_result{
                    wsrep::tls_service::error, 0};
            }
        }

        void clear_error() { last_error_ = 0; }

        int fd_;
        enum state state_;
        int last_error_;
        // Operation mode:
        // 1 - simulate handshake exchange
        // 2 - simulate errors and short reads
        int mode_;
        stats stats_;
        bool is_blocking_;
    };

    enum wsrep::tls_service::status db_stream::client_handshake()
    {
        clear_error();
        enum wsrep::tls_service::status ret;
        assert(state_ == s_initialized ||
               state_ == s_client_handshake ||
               state_ == s_want_write);
        if (state_ == s_initialized)
        {
            (void)::send(fd_, "clie", 4, MSG_NOSIGNAL);
            ret = wsrep::tls_service::want_read;
            state_ = s_client_handshake;
            wsrep::log_info() << this << " client handshake sent";
            stats_.bytes_written += 4;
            if (not is_blocking_) return ret;
        }

        if (state_ == s_client_handshake)
        {
            if ((ret = handle_handshake_read("serv")) ==
                wsrep::tls_service::success)
            {
                state_ = s_want_write;
                ret = wsrep::tls_service::want_write;
            }
            if (not is_blocking_) return ret;
        }

        if (state_ == s_want_write)
        {
            state_ = s_idle;
            ret = wsrep::tls_service::success;
            if (not is_blocking_) return ret;
        }

        if (not is_blocking_)
        {
            last_error_ = EPROTO;
            ret = wsrep::tls_service::error;
        }
        return ret;
    }



    enum wsrep::tls_service::status db_stream::server_handshake()
    {
        enum wsrep::tls_service::status ret;
        assert(state_ == s_initialized ||
               state_ == s_server_handshake ||
               state_ == s_want_write);

        if (state_ == s_initialized)
        {
            ::send(fd_, "serv", 4, MSG_NOSIGNAL);
            ret = wsrep::tls_service::want_read;
            state_ = s_server_handshake;
            stats_.bytes_written += 4;
            if (not is_blocking_) return ret;
        }

        if (state_ == s_server_handshake)
        {
            if ((ret = handle_handshake_read("clie")) ==
                wsrep::tls_service::success)
            {
                state_ = s_want_write;
                ret = wsrep::tls_service::want_write;
            }
            if (not is_blocking_) return ret;
        }

        if (state_ == s_want_write)
        {
            state_ = s_idle;
            ret = wsrep::tls_service::success;
            if (not is_blocking_) return ret;
        }

        if (not is_blocking_)
        {
            last_error_ = EPROTO;
            ret = wsrep::tls_service::error;
        }
        return ret;
    }

    enum wsrep::tls_service::status db_stream::handle_handshake_read(
        const char* expect)
    {
        assert(::strlen(expect) >= 4);
        char buf[4] = { };
        ssize_t read_result(::read(fd_, buf, sizeof(buf)));
        if (read_result > 0) stats_.bytes_read += size_t(read_result);
        enum wsrep::tls_service::status ret;
        if (read_result == -1 &&
            (errno == EWOULDBLOCK || errno == EAGAIN))
        {
            ret = wsrep::tls_service::want_read;
        }
        else if (read_result == 0)
        {
            ret = wsrep::tls_service::eof;
        }
        else if (read_result != 4 || ::memcmp(buf, expect, 4))
        {
            last_error_ = EPROTO;
            ret = wsrep::tls_service::error;
        }
        else
        {
            wsrep::log_info() << "Handshake success: " << std::string(buf, 4);
            ret = wsrep::tls_service::success;
        }
        return ret;
    }

    wsrep::tls_service::op_result db_stream::read(void* buf, size_t max_count)
    {
        clear_error();
        if (state_ == s_want_read)
        {
            state_ = s_idle;
            if (max_count == 0)
                return wsrep::tls_service::op_result{
                    wsrep::tls_service::success, 0};
        }
        max_count = determine_read_count(max_count);
        ssize_t read_result(do_read(buf, max_count));
        if (read_result > 0)
        {
            inc_reads(size_t(read_result));
        }
        return map_result(read_result);
    }

    wsrep::tls_service::op_result db_stream::write(
        const void* buf, size_t count)
    {
        clear_error();
        if (state_ == s_want_write)
        {
            state_ = s_idle;
            if (count == 0)
                return wsrep::tls_service::op_result{
                    wsrep::tls_service::success, 0};
        }
        count = determine_write_count(count);
        ssize_t write_result(do_write(buf, count));
        if (write_result > 0)
        {
            inc_writes(size_t(write_result));
        }
        return map_result(write_result);
    }
}


static db_stream::stats global_stats;
std::mutex global_stats_lock;
static int global_mode;

static void merge_to_global_stats(const db_stream::stats& stats)
{
    std::lock_guard<std::mutex> lock(global_stats_lock);
    global_stats.bytes_read += stats.bytes_read;
    global_stats.bytes_written += stats.bytes_written;
}

wsrep::tls_stream* db::tls::create_tls_stream(int fd) WSREP_NOEXCEPT
{
    auto ret(new db_stream(fd, global_mode));
    wsrep::log_debug() << "New DB stream: " << ret;
    return ret;
}

void db::tls::destroy(wsrep::tls_stream* stream) WSREP_NOEXCEPT
{
    auto dbs(static_cast<db_stream*>(stream));
    merge_to_global_stats(dbs->get_stats());
    wsrep::log_debug() << "Stream destroy: " << dbs->get_stats().bytes_read
                       << " " << dbs->get_stats().bytes_written;
    wsrep::log_debug() << "Stream destroy" << dbs;
    delete dbs;
}

int db::tls::get_error_number(const wsrep::tls_stream* stream)
    const WSREP_NOEXCEPT
{
    return static_cast<const db_stream*>(stream)->get_error_number();
}

const void* db::tls::get_error_category(const wsrep::tls_stream* stream)
    const WSREP_NOEXCEPT
{
    return static_cast<const db_stream*>(stream)->get_error_category();
}

const char* db::tls::get_error_message(const wsrep::tls_stream*,
                                       int value, const void* category)
    const WSREP_NOEXCEPT
{
    return db_stream::get_error_message(value, category);
}

enum wsrep::tls_service::status
db::tls::client_handshake(wsrep::tls_stream* stream) WSREP_NOEXCEPT
{
    return static_cast<db_stream*>(stream)->client_handshake();
}

enum wsrep::tls_service::status
db::tls::server_handshake(wsrep::tls_stream* stream) WSREP_NOEXCEPT
{
    return static_cast<db_stream*>(stream)->server_handshake();
}

wsrep::tls_service::op_result db::tls::read(
    wsrep::tls_stream* stream,
    void* buf, size_t max_count) WSREP_NOEXCEPT
{
    return static_cast<db_stream*>(stream)->read(buf, max_count);
}

wsrep::tls_service::op_result db::tls::write(
    wsrep::tls_stream* stream,
    const void* buf, size_t count) WSREP_NOEXCEPT
{
    return static_cast<db_stream*>(stream)->write(buf, count);
}

wsrep::tls_service::status
db::tls::shutdown(wsrep::tls_stream*) WSREP_NOEXCEPT
{
    // @todo error simulation
    return wsrep::tls_service::success;
}


void db::tls::init(int mode)
{
    global_mode = mode;
}

std::string db::tls::stats()
{
    std::ostringstream oss;
    oss << "Transport stats:\n"
        << "  bytes_read: " << global_stats.bytes_read << "\n"
        << "  bytes_written: " << global_stats.bytes_written << "\n";
    return oss.str();
}
