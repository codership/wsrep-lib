//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#ifndef TRREP_VIEW_HPP
#define TRREP_VIEW_HPP

#include <wsrep_api.h>

#include <vector>

namespace trrep
{
    class view
    {
    public:
        class member
        {
        public:
            member(const wsrep_member_info_t& member_info)
                : id_()
                , name_(member_info.name, WSREP_MEMBER_NAME_LEN)
                , incoming_(member_info.incoming, WSREP_INCOMING_LEN)
            {
                char uuid_str[WSREP_UUID_STR_LEN + 1];
                wsrep_uuid_print(&member_info.id, uuid_str, sizeof(uuid_str));
                id_ = uuid_str;
            }
            const std::string& id() const { return id_; }
            const std::string& name() const { return name_; }
            const std::string& incoming() const { return incoming_; }
        private:
            std::string id_;
            std::string name_;
            std::string incoming_;
        };

        view(const wsrep_view_info_t& view_info)
            : state_id_(view_info.state_id)
            , view_(view_info.view)
            , status_(view_info.status)
            , capabilities_(view_info.capabilities)
            , my_idx_(view_info.my_idx)
            , proto_ver_(view_info.proto_ver)
            , members_()
        {
            for (int i(0); i < view_info.memb_num; ++i)
            {
                members_.push_back(view_info.members[i]);
            }
        }


        wsrep_seqno_t id() const
        { return view_; }
        wsrep_view_status_t status() const
        { return status_; }
        int own_index() const
        { return my_idx_; }

        std::vector<member> members() const
        {
            std::vector<member> ret;
            for (std::vector<wsrep_member_info_t>::const_iterator i(members_.begin());
                 i != members_.end(); ++i)
            {
                ret.push_back(member(*i));
            }
            return ret;
        }
        //
        // Return true if the view is final
        //
        bool final() const
        {
            return (members_.empty() && my_idx_ == -1);
        }

    private:
        wsrep_gtid_t state_id_;
        wsrep_seqno_t view_;
        wsrep_view_status_t status_;
        wsrep_cap_t capabilities_;
        int my_idx_;
        int proto_ver_;
        std::vector<wsrep_member_info_t> members_;
    };
}

#endif // TRREP_VIEW
