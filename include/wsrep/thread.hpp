//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#include <pthread.h>

namespace wsrep
{
    class thread
    {
    public:
        class id
        {
        public:
            id() : thread_() { }
            explicit id(pthread_t thread) : thread_(thread) { }
        private:
            friend bool operator==(thread::id left, thread::id right)
            {
                return (pthread_equal(left.thread_, right.thread_));
            }
            pthread_t thread_;
        };

        thread()
            : id_(pthread_self())
        { }
    private:
        id id_;
    };

    namespace this_thread
    {
        static inline thread::id get_id() { return thread::id(pthread_self()); }
    }
};
