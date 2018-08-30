/******************************************************************************

 MIT License

 Copyright (c) 2018 kieme, frits.germs@gmx.net

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all
 copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 SOFTWARE.

******************************************************************************/

#include "dainty_container_ptr.h"
#include "dainty_os_threading.h"
#include "dainty_os_clock.h"
#include "dainty_mt_event_dispatcher.h"
#include "dainty_mt_waitable_chained_queue.h"
#include "dainty_mt_command.h"
#include "dainty_mt_thread.h"
#include "dainty_messaging_message.h"
#include "dainty_messaging_messenger.h"
#include "dainty_messaging.h"

using namespace dainty::container;
using namespace dainty::named;
using namespace dainty::mt;
using namespace dainty::os;

using string::FMT;
using dainty::container::any::t_any;
using dainty::os::threading::t_mutex_lock;
using dainty::os::clock::t_time;
using dainty::mt::thread::t_thread;
using dainty::mt::event_dispatcher::t_dispatcher;
using dainty::mt::event_dispatcher::t_event_logic;
using dainty::mt::event_dispatcher::t_action;
using dainty::mt::event_dispatcher::CONTINUE;
using dainty::mt::event_dispatcher::QUIT_EVENT_LOOP;
using dainty::mt::event_dispatcher::RD;
using dainty::messaging::messenger::t_multiple_of_100ms;

using t_thd_err                = t_thread::t_logic::t_err;
using t_cmd_err                = command::t_processor::t_logic::t_err;
using t_cmd_client             = command::t_client;
using t_cmd_processor          = command::t_processor;
using t_cmd                    = command::t_command;
using t_any_user               = any::t_user;
using t_que_chain              = waitable_chained_queue::t_chain;
using t_que_client             = waitable_chained_queue::t_client;
using t_que_processor          = waitable_chained_queue::t_processor;
using t_messenger_id           = dainty::messaging::messenger::t_id;
using R_messenger_id           = dainty::messaging::messenger::R_id;
using r_messenger_messages     = dainty::messaging::messenger::r_messages;
using r_messenger_timer_params = dainty::messaging::messenger::r_timer_params;
using R_messenger_timer_params = dainty::messaging::messenger::R_timer_params;
using r_messenger_monitor_list = dainty::messaging::messenger::r_monitor_list;

namespace dainty
{
namespace messaging
{

namespace message
{
///////////////////////////////////////////////////////////////////////////////

  t_message::t_message() : buf_{t_n{10}} {
  }

  t_message::t_message(t_n n) : buf_{n} {
  }

  t_message::t_message(x_bytebuf buf) : buf_{std::move(buf)} {
  }

  t_message& t_message::operator=(x_bytebuf buf) {
    return *this;
  }

  t_bool t_message::set(const t_id&, t_uint32 length, t_uint16 cnt) {
    return false;
  }

  t_bool t_message::get(t_id&      id,
                        t_uint32&  length,
                        t_key&     dst,
                        t_key&     src,
                        t_uint16&  cnt) {
    return false;
  }

  t_message::operator t_bool() const {
    return false;
  }

  t_message::operator t_validity () const {
    return VALID;
  }

  t_bool t_message::is_empty() const {
    return false;
  }

  t_n t_message::get_capacity() const {
    return t_n{0};
  }

  p_byte t_message::data() {
    return nullptr;
  }

  P_byte t_message::data() const {
    return nullptr;
  }

  P_byte t_message::cdata() const {
    return nullptr;
  }

  t_bytebuf t_message::release() {
    return {t_n{0}};
  }

  t_bytebuf t_message::clone() const {
    return {t_n{0}};
  }

  t_messenger_key read_dst(R_message) {
    return t_messenger_key{0};
  }

  t_messenger_key read_src(R_message) {
    return t_messenger_key{0};
  }

  t_message_id read_id (R_message) {
    return {t_message_domain{0}, t_message_user{0L}, t_message_version{1}};
  }

  t_n read_len(R_message) {
     return t_n{0};
  }

///////////////////////////////////////////////////////////////////////////////

  t_notify_message::t_notify_message() {
  }

  t_bool t_notify_message::get(t_messenger_state&,
                               t_messenger_name&,
                               t_messenger_key&,
                               t_messenger_prio&,
                               t_messenger_user&) {
    return false;
  }

///////////////////////////////////////////////////////////////////////////////

  t_timeout_message::t_timeout_message() {
  }

  t_bool t_timeout_message::get(t_bool& periodic,
               t_multiple_of_100ms&,
               t_messenger_key&,
               t_messenger_prio&,
               t_messenger_user&) {
    return false;
  }

///////////////////////////////////////////////////////////////////////////////

  t_bool t_message_fail::get(reason_t&, t_message& send_message) {
    return false;
  }

///////////////////////////////////////////////////////////////////////////////

  t_alive_message::t_alive_message() {
  }

  t_bool t_alive_message::get() {
    return false;
  }
}

///////////////////////////////////////////////////////////////////////////////

  struct t_item_ {
    message::t_bytebuf buf;
    t_item_(message::t_bytebuf&& _buf) : buf{std::move(_buf)} {
    }
    t_item_()                                     = delete;
    t_item_(const message::t_bytebuf&)            = delete;
    t_item_& operator=(const message::t_bytebuf&) = delete;
    t_item_& operator=(message::t_bytebuf&&)      = delete;
  };
  using R_item_ = t_prefix<t_item_>::R_;

///////////////////////////////////////////////////////////////////////////////

  struct t_update_params_cmd_ : t_cmd {
    constexpr static command::t_id cmd_id = 1;
    R_params params;

    inline
    t_update_params_cmd_(R_params _params) : t_cmd{cmd_id}, params(_params) {
    };
  };
  using r_update_params_cmd_ = t_prefix<t_update_params_cmd_>::r_;

  struct t_fetch_params_cmd_ : t_cmd {
    constexpr static command::t_id cmd_id = 2;
    r_params params;

    inline
    t_fetch_params_cmd_(r_params _params) : t_cmd{cmd_id}, params(_params) {
    };
  };
  using r_fetch_params_cmd_ = t_prefix<t_fetch_params_cmd_>::r_;

  struct t_make_messenger_cmd_ : t_cmd {
    constexpr static command::t_id cmd_id = 3;
    R_messenger_name name;
    R_messenger_create_params params;

    inline
    t_make_messenger_cmd_(R_messenger_name _name,
                          R_messenger_create_params _params)
      : t_cmd{cmd_id}, name(_name), params(_params) {
    }
  };
  using r_make_messenger_cmd_ = t_prefix<t_make_messenger_cmd_>::r_;

  struct t_destroy_messenger_cmd_ : t_cmd {
    constexpr static command::t_id cmd_id = 4;
    R_messenger_id id;

    inline
    t_destroy_messenger_cmd_(R_messenger_id _id) : t_cmd{cmd_id}, id(_id) {
    }
  };
  using r_destroy_messenger_cmd_ = t_prefix<t_destroy_messenger_cmd_>::r_;

  struct t_fetch_messenger_cmd_ : t_cmd {
    constexpr static command::t_id cmd_id = 5;
    R_messenger_name   name;
    r_messenger_params params;

    inline
    t_fetch_messenger_cmd_(R_messenger_name _name, r_messenger_params _params)
      : t_cmd{cmd_id}, name{_name}, params{_params} {
    }
  };
  using r_fetch_messenger_cmd_ = t_prefix<t_fetch_messenger_cmd_>::r_;

  struct t_fetch_messenger_info_cmd_ : t_cmd {
    constexpr static command::t_id cmd_id = 6;
    R_messenger_name name;
    r_messenger_info info;
    t_bool           clearstats;

    inline
    t_fetch_messenger_info_cmd_(R_messenger_name _name,
                                r_messenger_info _info, t_bool _clearstats)
      : t_cmd{cmd_id}, name{_name}, info{_info}, clearstats{_clearstats} {
    }
  };
  using r_fetch_messenger_info_cmd_ = t_prefix<t_fetch_messenger_info_cmd_>::r_;

  struct t_fetch_messengers_cmd_ : t_cmd {
    constexpr static command::t_id cmd_id = 7;
    r_messenger_infos infos;
    t_bool            clearstats;

    inline
    t_fetch_messengers_cmd_(r_messenger_infos _infos, t_bool _clearstats)
      : t_cmd{cmd_id}, infos{_infos}, clearstats{_clearstats} {
    }
  };
  using r_fetch_messengers_cmd_ = t_prefix<t_fetch_messengers_cmd_>::r_;

  struct t_create_group_cmd_ : t_cmd {
    constexpr static command::t_id cmd_id = 8;
    R_password             password;
    R_messenger_name       name;
    t_messenger_visibility visibility;

    inline
    t_create_group_cmd_(R_password             _password,
                        R_messenger_name       _name,
                        t_messenger_visibility _visibility)
      : t_cmd{cmd_id}, password{_password}, name{_name},
        visibility{_visibility} {
    }
  };
  using r_create_group_cmd_ = t_prefix<t_create_group_cmd_>::r_;

  struct t_destroy_group_cmd_ : t_cmd {
    constexpr static command::t_id cmd_id = 9;
    R_password       password;
    R_messenger_name name;

    inline
    t_destroy_group_cmd_(R_password _password, R_messenger_name _name)
      : t_cmd{cmd_id}, password{_password}, name{_name} {
    }
  };
  using r_destroy_group_cmd_ = t_prefix<t_destroy_group_cmd_>::r_;

  struct t_fetch_group_cmd_ : t_cmd {
    constexpr static command::t_id cmd_id = 10;
    R_messenger_name       name;
    r_messenger_visibility visibility;
    p_messenger_group_list group_list;

    inline
    t_fetch_group_cmd_(R_messenger_name       _name,
                       r_messenger_visibility _visibility,
                       p_messenger_group_list _group_list)
      : t_cmd{cmd_id}, name{_name}, visibility{_visibility},
        group_list{_group_list} {
    }
  };
  using r_fetch_group_cmd_ = t_prefix<t_fetch_group_cmd_>::r_;

  struct t_add_messenger_to_group_cmd_ : t_cmd {
    constexpr static command::t_id cmd_id = 11;
    R_messenger_password password;
    R_messenger_name     name;
    R_messenger_name     group;
    t_messenger_prio     prio;
    t_messenger_user     user;

    t_add_messenger_to_group_cmd_(R_messenger_password _password,
                                  R_messenger_name _name,
                                  R_messenger_name _group,
                                  t_messenger_prio _prio,
                                  t_messenger_user _user)
      : t_cmd{cmd_id}, password{_password}, name{_name}, group{_group},
        prio{_prio}, user{_user} {
    }
  };
  using r_add_messenger_to_group_cmd_ =
    t_prefix<t_add_messenger_to_group_cmd_>::r_;

  struct t_remove_messenger_from_group_cmd_ : t_cmd {
    constexpr static command::t_id cmd_id = 12;
    R_messenger_password password;
    R_messenger_name     name;
    R_messenger_name     group;
    p_messenger_user     user;

    inline
    t_remove_messenger_from_group_cmd_(R_messenger_password _password,
                                       R_messenger_name _name,
                                       R_messenger_name _group,
                                       p_messenger_user _user)
      : t_cmd{cmd_id}, password{_password}, name{_name}, group{_group},
        user{user} {
    }
  };
  using r_remove_messenger_from_group_cmd_ =
    t_prefix<t_remove_messenger_from_group_cmd_>::r_;

  struct t_is_messenger_in_group_cmd_ : t_cmd {
    constexpr static command::t_id cmd_id = 13;
    R_messenger_name name;
    R_messenger_name group;
    p_messenger_user user;

    inline
    t_is_messenger_in_group_cmd_(R_messenger_name _name,
                                 R_messenger_name _group,
                                 p_messenger_user _user)
      : t_cmd{cmd_id}, name{_name}, group{_group}, user{user} {
    }
  };
  using r_is_messenger_in_group_cmd_ =
    t_prefix<t_is_messenger_in_group_cmd_>::r_;

  struct t_fetch_messenger_groups_cmd_ : t_cmd {
    constexpr static command::t_id cmd_id = 14;
    R_messenger_name       name;
    r_messenger_group_list group_list;

    inline
    t_fetch_messenger_groups_cmd_(R_messenger_name       _name,
                                  r_messenger_group_list _group_list)
      : t_cmd{cmd_id}, name{_name}, group_list{_group_list} {
    }
  };
  using r_fetch_messenger_cmd_ = t_prefix<t_fetch_messenger_cmd_>::r_;

  struct t_who_is_cmd_ : t_cmd {
    constexpr static command::t_id cmd_id = 15;
    R_messenger_key  key;
    r_messenger_name name;
    t_bool*          group;
    t_bool*          local;

    inline
    t_who_is_cmd_(R_messenger_key _key, r_messenger_name _name,
                  t_bool* _group, t_bool* _local)
      : t_cmd{cmd_id}, key{_key}, name{_name}, group{_group}, local{_local} {
    }
  };
  using r_who_is_cmd_ = t_prefix<t_who_is_cmd_>::r_;

  struct t_get_name_cmd_ : t_cmd {
    constexpr static command::t_id cmd_id = 16;
    R_messenger_id   id;
    t_messenger_name name;

    inline
    t_get_name_cmd_(R_messenger_id _id) : t_cmd{cmd_id}, id{_id} {
    }
  };
  using r_get_name_cmd_ = t_prefix<t_get_name_cmd_>::r_;

  struct t_get_params_cmd_ : t_cmd {
    constexpr static command::t_id cmd_id = 17;
    R_messenger_id     id;
    r_messenger_params params;

    inline
    t_get_params_cmd_(R_messenger_id _id, r_messenger_params _params)
      : t_cmd{cmd_id}, id{_id}, params{_params} {
    }
  };
  using r_get_params_cmd_ = t_prefix<t_get_params_cmd_>::r_;

  struct t_update_visibility_cmd_ : t_cmd {
    constexpr static command::t_id cmd_id = 18;
    R_messenger_id         id;
    t_messenger_visibility visibility;

    inline
    t_update_visibility_cmd_(R_messenger_id         _id,
                             t_messenger_visibility _visibility)
      : t_cmd{cmd_id}, id{_id}, visibility{_visibility} {
    }
  };
  using r_update_visibility_cmd_ = t_prefix<t_update_visibility_cmd_>::r_;

  struct t_update_alive_period_cmd_ : t_cmd {
    constexpr static command::t_id cmd_id = 19;
    R_messenger_id      id;
    t_multiple_of_100ms multiple;

    inline
    t_update_alive_period_cmd_(R_messenger_id      _id,
                               t_multiple_of_100ms _multiple)
      : t_cmd{cmd_id}, id{_id}, multiple{_multiple} {
    }
  };
  using r_update_alive_period_cmd_ = t_prefix<t_update_alive_period_cmd_>::r_;

  struct t_start_timer_cmd_ : t_cmd {
    constexpr static command::t_id cmd_id = 20;
    R_messenger_id           id;
    R_messenger_timer_params params;

    inline
    t_start_timer_cmd_(R_messenger_id _id, R_messenger_timer_params _params)
      : t_cmd{cmd_id}, id{_id}, params{_params} {
    }
  };
  using r_start_timer_cmd_ = t_prefix<t_start_timer_cmd_>::r_;

  struct t_stop_timer_cmd_ : t_cmd {
    constexpr static command::t_id cmd_id = 21;
    R_messenger_id id;

    inline
    t_stop_timer_cmd_(R_messenger_id _id) : t_cmd{cmd_id}, id{_id} {
    }
  };
  using r_stop_timer_cmd_ = t_prefix<t_stop_timer_cmd_>::r_;

  struct t_query_timer_cmd_ : t_cmd {
    constexpr static command::t_id cmd_id = 22;
    R_messenger_id           id;
    r_messenger_timer_params params;

    inline
    t_query_timer_cmd_(R_messenger_id _id, r_messenger_timer_params _params)
      : t_cmd{cmd_id}, id{_id}, params{_params} {
    }
  };
  using r_query_timer_cmd_ = t_prefix<t_query_timer_cmd_>::r_;

  struct t_add_to_group_cmd_ : t_cmd {
    constexpr static command::t_id cmd_id = 23;
    R_messenger_id       id;
    R_messenger_password password;
    R_messenger_name     name;
    t_messenger_prio     prio;
    t_messenger_user     user;

    inline
    t_add_to_group_cmd_(R_messenger_id       _id,
                        R_messenger_password _password,
                        R_messenger_name     _name,
                        t_messenger_prio     _prio,
                        t_messenger_user     _user)
     : t_cmd{cmd_id}, id{_id}, password{_password}, name{_name}, prio{_prio},
       user{_user} {
    }
  };
  using r_add_to_group_cmd_ = t_prefix<t_add_to_group_cmd_>::r_;

  struct t_remove_from_group_cmd_ : t_cmd {
    constexpr static command::t_id cmd_id = 24;
    R_messenger_id       id;
    R_messenger_password password;
    R_messenger_name     group;
    p_messenger_user     user;

    inline
    t_remove_from_group_cmd_(R_messenger_id       _id,
                             R_messenger_password _password,
                             R_messenger_name     _group,
                             p_messenger_user     _user)
     : t_cmd{cmd_id}, id{_id}, password{_password}, group{_group}, user{_user} {
    }
  };
  using r_remove_from_group_cmd_ = t_prefix<t_remove_from_group_cmd_>::r_;

  struct t_is_in_group_cmd_ : t_cmd {
    constexpr static command::t_id cmd_id = 25;
    R_messenger_id   id;
    R_messenger_name group;
    p_messenger_user user;

    inline
    t_is_in_group_cmd_(R_messenger_id   _id,
                       R_messenger_name _group,
                       p_messenger_user _user)
      : t_cmd{cmd_id}, id{_id}, group{_group}, user{_user} {
    }
  };
  using r_is_in_group_cmd_ = t_prefix<t_is_in_group_cmd_>::r_;

  struct t_get_groups_cmd_ : t_cmd {
    constexpr static command::t_id cmd_id = 26;
    R_messenger_id id;
    r_messenger_group_list group_list;

    inline
    t_get_groups_cmd_(R_messenger_id _id, r_messenger_group_list _group_list)
      : t_cmd{cmd_id}, id{_id}, group_list{_group_list} {
    }
  };
  using r_get_groups_cmd_ = t_prefix<t_get_groups_cmd_>::r_;

  struct t_add_monitor_cmd_ : t_cmd {
    constexpr static command::t_id cmd_id = 27;
    R_messenger_id   id;
    R_messenger_name group;
    t_messenger_prio prio;
    t_messenger_user user;

    inline
    t_add_monitor_cmd_(R_messenger_id   _id,
                       R_messenger_name _group,
                       t_messenger_prio _prio,
                       t_messenger_user _user)
      : t_cmd{cmd_id}, id{_id}, group{_group}, prio{_prio}, user{_user} {
    }
  };
  using r_add_monitor_cmd_ = t_prefix<t_add_monitor_cmd_>::r_;

  struct t_remove_monitor_cmd_ : t_cmd {
    constexpr static command::t_id cmd_id = 28;
    R_messenger_id   id;
    R_messenger_name name;
    p_messenger_user user;

    inline
    t_remove_monitor_cmd_(R_messenger_id   _id,
                          R_messenger_name _name,
                          p_messenger_user _user)
      : t_cmd{cmd_id}, id{_id}, name{_name}, user{_user} {
    }
  };
  using r_remove_monitor_cmd_ = t_prefix<t_remove_monitor_cmd_>::r_;

  struct t_is_monitored_cmd_ : t_cmd {
    constexpr static command::t_id cmd_id = 29;
    t_messenger_key  key = t_messenger_key{0};
    R_messenger_id   id;
    R_messenger_name name;
    p_messenger_user user;

    inline
    t_is_monitored_cmd_(R_messenger_id   _id,
                        R_messenger_name _name,
                        p_messenger_user _user)
      : t_cmd{cmd_id}, id{_id}, name{_name}, user{_user} {
    }
  };
  using r_is_monitored_cmd_ = t_prefix<t_is_monitored_cmd_>::r_;

  struct t_get_monitored_cmd_ : t_cmd {
    constexpr static command::t_id cmd_id = 30;
    R_messenger_id           id;
    r_messenger_monitor_list monitor_list;

    inline
    t_get_monitored_cmd_(R_messenger_id           _id,
                         r_messenger_monitor_list _monitor_list)
      : t_cmd{cmd_id}, id{_id}, monitor_list{_monitor_list} {
    }
  };
  using r_get_monitored_cmd_ = t_prefix<t_get_monitored_cmd_>::r_;

  struct t_clean_death_cmd_ : t_cmd {
    constexpr static command::t_id cmd_id = 31;

    inline
    t_clean_death_cmd_() : t_cmd{cmd_id} {
    };
  };
  using r_clean_death_cmd_ = t_prefix<t_clean_death_cmd_>::r_;

///////////////////////////////////////////////////////////////////////////////

  class t_data_ {
  public:
    t_params params;

    t_data_(R_params _params) : params(_params) {
    }
  };

///////////////////////////////////////////////////////////////////////////////

  class t_logic_ : public t_thread::t_logic,
                   public t_cmd_processor::t_logic,
                   public t_que_processor::t_logic,
                   public t_dispatcher::t_logic {
  public:
    t_logic_(err::t_err err, R_params params)
      : data_         {params},
        cmd_processor_{err},
        que_processor_{err, data_.params.queuesize},
        dispatcher_   {err, {t_n{2}, "epoll_service"}} {
    }

    t_cmd_client make_cmd_client() {
      return cmd_processor_.make_client(command::t_user{0L});
    }

    t_que_client make_que_client() {
      return que_processor_.make_client(waitable_chained_queue::t_user{0L});
    }

    virtual t_validity update(t_thd_err err,
                              r_pthread_attr) noexcept override {
      printf("thread update - before thread is created\n");
      return VALID;
    }

    virtual t_validity prepare(t_thd_err err) noexcept override {
      printf("thread prepare - after thread is created\n");
      return VALID;
    }

    virtual p_void run() noexcept override {
      printf("thread run - its main loop\n");

      err::t_err err;
      {
        t_cmd_proxy_ cmd_proxy{err, action_, cmd_processor_, *this};
        dispatcher_.add_event (err, {cmd_processor_.get_fd(), RD}, &cmd_proxy);

        t_que_proxy_ que_proxy{err, action_, que_processor_, *this};
        dispatcher_.add_event (err, {que_processor_.get_fd(), RD}, &que_proxy);

        dispatcher_.event_loop(err, this);
      }

      if (err) {
        err.print();
        err.clear();
      }

      return nullptr;
    }

    virtual t_void may_reorder_events (r_event_infos infos) override {
      //printf("events received = %lu\n", infos.size());
    }

    virtual t_void notify_event_remove(r_event_info) override {
      printf("remove event which should never happen!!\n");
      // not required
    }

    virtual t_quit notify_timeout(t_usec) override {
      printf("timeout that should never happen!!\n");
      return true; // not required
    }

    virtual t_quit notify_error(t_errn)  override {
      printf("error in event handling!!\n");
      return true; // die
    }

    t_void process_item(waitable_chained_queue::t_entry& entry) {
      auto& item = entry.any.ref<t_item_>();
    }

    t_void process_chain(t_chain& chain) {
      for (auto item = chain.head; item; item = item->next())
        process_item(item->ref());
    }

    virtual t_void async_process(t_chain chain) noexcept override {
      printf("recv a trace\n");
      process_chain(chain);
    }

    virtual t_void async_process(t_user, p_command cmd) noexcept override {
      printf("thread async command - none is used at this point\n");
      // not used
    }

    t_void process(err::t_err err, r_update_params_cmd_ cmd) noexcept {
      //XXX
    }

    t_void process(err::t_err err, r_fetch_params_cmd_ cmd) noexcept {
      //XXX
    }

    t_void process(err::t_err err, r_make_messenger_cmd_ cmd) noexcept {
    }

    t_void process(err::t_err err, r_destroy_messenger_cmd_ cmd) noexcept {
    }

    t_void process(err::t_err err, r_fetch_messenger_cmd_ cmd) noexcept {
    }

    t_void process(err::t_err err, r_fetch_messenger_info_cmd_ cmd) noexcept {
    }

    t_void process(err::t_err err, r_fetch_messengers_cmd_ cmd) noexcept {
    }

    t_void process(err::t_err err, r_create_group_cmd_ cmd) noexcept {
    }

    t_void process(err::t_err err, r_destroy_group_cmd_ cmd) noexcept {
    }

    t_void process(err::t_err err, r_fetch_group_cmd_ cmd) noexcept {
    }

    t_void process(err::t_err err, r_add_messenger_to_group_cmd_ cmd) noexcept {
    }

    t_void process(err::t_err err,
                   r_remove_messenger_from_group_cmd_ cmd) noexcept {
    }

    t_void process(err::t_err err, r_is_messenger_in_group_cmd_ cmd) noexcept {
    }

    t_void process(err::t_err err, r_fetch_messenger_groups_cmd_ cmd) noexcept {
    }

    t_void process(err::t_err err, r_who_is_cmd_ cmd) noexcept {
    }

    t_void process(err::t_err err, r_get_name_cmd_ cmd) noexcept {
    }

    t_void process(err::t_err err, r_get_params_cmd_ cmd) noexcept {
    }

    t_void process(err::t_err err, r_update_visibility_cmd_ cmd) noexcept {
    }

    t_void process(err::t_err err, r_update_alive_period_cmd_ cmd) noexcept {
    }

    t_void process(err::t_err err, r_start_timer_cmd_ cmd) noexcept {
    }

    t_void process(err::t_err err, r_stop_timer_cmd_ cmd) noexcept {
    }

    t_void process(err::t_err err, r_query_timer_cmd_ cmd) noexcept {
    }

    t_void process(err::t_err err, r_add_to_group_cmd_ cmd) noexcept {
    }

    t_void process(err::t_err err, r_remove_from_group_cmd_ cmd) noexcept {
    }

    t_void process(err::t_err err, r_is_in_group_cmd_ cmd) noexcept {
    }

    t_void process(err::t_err err, r_get_groups_cmd_ cmd) noexcept {
    }

    t_void process(err::t_err err, r_add_monitor_cmd_ cmd) noexcept {
    }

    t_void process(err::t_err err, r_remove_monitor_cmd_ cmd) noexcept {
    }

    t_void process(err::t_err err, r_is_monitored_cmd_ cmd) noexcept {
    }

    t_void process(err::t_err err, r_get_monitored_cmd_ cmd) noexcept {
    }

    t_void process(err::t_err err, r_clean_death_cmd_ cmd) noexcept {
      printf("thread clean_death_cmd received\n");
      action_.cmd = QUIT_EVENT_LOOP;
    }

    virtual t_void process(t_cmd_err err, t_user,
                           r_command cmd) noexcept override {
      ERR_GUARD(err) {
        switch (cmd.id) {
          case t_update_params_cmd_::cmd_id:
            process(err, static_cast<r_update_params_cmd_>(cmd));
            break;
          case t_fetch_params_cmd_::cmd_id:
            process(err, static_cast<r_fetch_params_cmd_>(cmd));
            break;
          case t_make_messenger_cmd_::cmd_id:
            process(err, static_cast<r_make_messenger_cmd_>(cmd));
            break;
          case t_destroy_messenger_cmd_::cmd_id:
            process(err, static_cast<r_destroy_messenger_cmd_>(cmd));
            break;
          case t_fetch_messenger_cmd_::cmd_id:
            process(err, static_cast<r_fetch_messenger_cmd_>(cmd));
            break;
          case t_fetch_messenger_info_cmd_::cmd_id:
            process(err, static_cast<r_fetch_messenger_info_cmd_>(cmd));
            break;
          case t_fetch_messengers_cmd_::cmd_id:
            process(err, static_cast<r_fetch_messengers_cmd_>(cmd));
            break;
          case t_create_group_cmd_::cmd_id:
            process(err, static_cast<r_create_group_cmd_>(cmd));
            break;
          case t_destroy_group_cmd_::cmd_id:
            process(err, static_cast<r_destroy_group_cmd_>(cmd));
            break;
          case t_fetch_group_cmd_::cmd_id:
            process(err, static_cast<r_fetch_group_cmd_>(cmd));
            break;
          case t_add_messenger_to_group_cmd_::cmd_id:
            process(err, static_cast<r_add_messenger_to_group_cmd_>(cmd));
            break;
          case t_clean_death_cmd_::cmd_id:
            process(err, static_cast<r_clean_death_cmd_>(cmd));
            break;
          default:
            // made a mess
            // XXX- 16
            break;
        }
      }
    }




    t_void process(err::t_err err,
                   r_remove_messenger_from_group_cmd_ cmd) noexcept {
    }

    t_void process(err::t_err err, r_is_messenger_in_group_cmd_ cmd) noexcept {
    }

    t_void process(err::t_err err, r_fetch_messenger_groups_cmd_ cmd) noexcept {
    }

    t_void process(err::t_err err, r_who_is_cmd_ cmd) noexcept {
    }

    t_void process(err::t_err err, r_get_name_cmd_ cmd) noexcept {
    }

    t_void process(err::t_err err, r_get_params_cmd_ cmd) noexcept {
    }

    t_void process(err::t_err err, r_update_visibility_cmd_ cmd) noexcept {
    }

    t_void process(err::t_err err, r_update_alive_period_cmd_ cmd) noexcept {
    }

    t_void process(err::t_err err, r_start_timer_cmd_ cmd) noexcept {
    }

    t_void process(err::t_err err, r_stop_timer_cmd_ cmd) noexcept {
    }

    t_void process(err::t_err err, r_query_timer_cmd_ cmd) noexcept {
    }

    t_void process(err::t_err err, r_add_to_group_cmd_ cmd) noexcept {
    }

    t_void process(err::t_err err, r_remove_from_group_cmd_ cmd) noexcept {
    }

    t_void process(err::t_err err, r_is_in_group_cmd_ cmd) noexcept {
    }

    t_void process(err::t_err err, r_get_groups_cmd_ cmd) noexcept {
    }

    t_void process(err::t_err err, r_add_monitor_cmd_ cmd) noexcept {
    }

    t_void process(err::t_err err, r_remove_monitor_cmd_ cmd) noexcept {
    }

    t_void process(err::t_err err, r_is_monitored_cmd_ cmd) noexcept {
    }

    t_void process(err::t_err err, r_get_monitored_cmd_ cmd) noexcept {
    }

///////////////////////////////////////////////////////////////////////////////

  private:
    class t_cmd_proxy_ : public t_event_logic {
    public:
      t_cmd_proxy_(err::t_err& err, t_action& action,
                   t_cmd_processor& processor, t_cmd_processor::t_logic& logic)
        : err_(err), action_(action), processor_(processor), logic_{logic} {
      }

      virtual t_name get_name() const override {
        return {"cmd logic"};
      }

      virtual t_action notify_event(r_event_params params) override {
        action_.cmd = CONTINUE;
        processor_.process(err_, logic_);
        return action_;
      }

    private:
      err::t_err&               err_;
      t_action&                 action_;
      t_cmd_processor&          processor_;
      t_cmd_processor::t_logic& logic_;
    };

    class t_que_proxy_ : public t_event_logic {
    public:
      t_que_proxy_(err::t_err& err, t_action& action,
                   t_que_processor& processor, t_que_processor::t_logic& logic)
        : err_(err), action_(action), processor_(processor), logic_{logic} {
      }

      virtual t_name get_name() const override {
        return {"queue logic"};
      }

      virtual t_action notify_event(r_event_params params) override {
        action_.cmd = CONTINUE;
        processor_.process_available(err_, logic_);
        return action_;
      }

    private:
      err::t_err&               err_;
      t_action&                 action_;
      t_que_processor&          processor_;
      t_que_processor::t_logic& logic_;
    };

    t_action        action_;
    t_data_         data_;
    t_cmd_processor cmd_processor_;
    t_que_processor que_processor_;
    t_dispatcher    dispatcher_;
  };

///////////////////////////////////////////////////////////////////////////////

  class t_messaging_ {
  public:
    using r_err = t_prefix<t_err>::r_;

    t_messaging_(r_err err, R_params params)
      : logic_     {err, params},
        cmd_client_{logic_.make_cmd_client()},
        que_client_{logic_.make_que_client()},
        thread_    {err, P_cstr{"messaging"}, &logic_, false} {
    }

    t_validity update(r_err err, R_params params) {
      t_update_params_cmd_ cmd(params);
      return cmd_client_.request(err, cmd);
    }

    t_void fetch(r_err err, r_params params) {
      t_fetch_params_cmd_ cmd(params);
      cmd_client_.request(err, cmd);
    }

    t_messenger_id create_messenger(r_err err, R_messenger_name name,
                                    R_messenger_create_params params) {
      //XXX -1
      return t_messenger_id{};
    }

    t_void destroy_messenger(R_messenger_id id) {
      //XXX -2
    }

    t_bool fetch_messenger(r_err err, R_messenger_name name,
                                      r_messenger_params params) {
      //XXX -3
      return false;
    }

    t_bool fetch_messenger(r_err err, R_messenger_name name,
                                      r_messenger_info info,
                                      t_bool clearstats) {
      //XXX -4
      return false;
    }

    t_bool fetch_messengers(r_err err, r_messenger_infos infos,
                                       t_bool clearstats) {
      //XXX -5
      return false;
    }

    t_bool create_group(r_err err, R_password password,
                                   R_messenger_name,
                                   t_messenger_visibility visibility) {
      //XXX -6
      return false;
    }

    t_bool destroy_group(r_err err, R_password password,
                                    R_messenger_name name) {
      //XXX -7
      return false;
    }

    t_bool fetch_group(r_err err, R_messenger_name name,
                                  r_messenger_visibility visibility,
                                  p_messenger_group_list group_list) {
      //XXX -8
      return false;
    }

    t_bool add_messenger_to_group(r_err err, R_messenger_password password,
                                             R_messenger_name name,
                                             R_messenger_name group,
                                             t_messenger_prio prio,
                                             t_messenger_user user) {
      //XXX -9
      return false;
    }

    t_bool remove_messenger_from_group(r_err err, R_messenger_password password,
                                                  R_messenger_name name,
                                                  R_messenger_name group,
                                                  p_messenger_user user) {
      //XXX -10
      return false;
    }

    t_bool is_messenger_in_group(r_err err, R_messenger_name name,
                                            R_messenger_name group,
                                            p_messenger_user user) {
      //XXX -11
      return false;
    }

    t_bool fetch_messenger_groups(r_err err, R_messenger_name name,
                                             r_messenger_group_list group_list) {
      //XXX -12
      return false;
    }

    t_bool who_is(r_err err, R_messenger_key key, r_messenger_name name,
                             t_bool* group, t_bool* local) {
      //XXX -13
      return false;
    }

    t_bool post_message(r_err err, R_messenger_key key, r_message message) {
      //XXX -14
      return false;
    }

    t_messenger_name get_name(r_err err, R_messenger_id id) const {
      //XXX -15
      return {};
    }

    t_bool get_params(r_err err, R_messenger_id id,
                      r_messenger_params params) const {
      //XXX -16
      return false;
    }

    t_bool update_visibility(r_err err, R_messenger_id id,
                                        t_messenger_visibility visibility) {
      //XXX -17
      return false;
    }

    t_bool update_alive_period(r_err err, R_messenger_id id,
                                          t_multiple_of_100ms) {
      //XXX -18
      return false;
    }

    t_bool post_message(r_err err, R_messenger_id id, R_messenger_key,
                                   r_message) const {
      //XXX -19
      return false;
    }

    t_bool wait_message(r_err err, R_messenger_id id,
                                   r_messenger_messages messages) const {
      //XXX -20
      return false;
    }

    t_bool check_message(r_err err, R_messenger_id id,
                                    r_messenger_messages messages) const {
      //XXX -21
      return false;
    }

    t_bool start_timer(r_err err, R_messenger_id id,
                                  R_messenger_timer_params params) {
      //XXX -22
      return false;
    }

    t_bool stop_timer(r_err err, R_messenger_id id) {
      //XXX -23
      return false;
    }

    t_bool query_timer(r_err err, R_messenger_id id,
                                  r_messenger_timer_params params) const {
      //XXX -24
      return false;
    }

    t_bool add_to_group(r_err err, R_messenger_id id,
                                   R_messenger_password password,
                                   R_messenger_name name,
                                   t_messenger_prio prio,
                                   t_messenger_user user) {
      //XXX -25
      return false;
    }

    t_bool remove_from_group(r_err err, R_messenger_id id,
                                        R_messenger_password password,
                                        R_messenger_name group,
                                        p_messenger_user user) {
      //XXX -26
      return false;
    }

    t_bool is_in_group(r_err err, R_messenger_id id,
                                  R_messenger_name group,
                                  p_messenger_user user) const {
      //XXX -27
      return false;
    }

    t_bool get_groups(r_err err, R_messenger_id id,
                                 r_messenger_group_list group_list) const {
      //XXX -28
      return false;
    }

    t_bool add_monitor(r_err err, R_messenger_id id,
                       R_messenger_name group, t_messenger_prio prio,
                       t_messenger_user user) {
      //XXX -29
      return false;
    }

    t_bool remove_monitor(r_err err, R_messenger_id id,
                                     R_messenger_name name,
                                     p_messenger_user user) {
      //XXX -30
      return false;
    }

    t_messenger_key is_monitored(r_err err, R_messenger_id id,
                                            R_messenger_name name,
                                            p_messenger_user user) const {
      //XXX -31
      return t_messenger_key{0};
    }

    t_bool get_monitored(r_err err, R_messenger_id id,
                         r_messenger_monitor_list monitor_list) const {
      //XXX -32
      return false;
    }

    t_validity clean_death() {
      t_clean_death_cmd_ cmd;
      return cmd_client_.request(cmd);
    }

  private:
    t_logic_     logic_;
    t_cmd_client cmd_client_;
    t_que_client que_client_;
    t_thread     thread_;
  };
  using p_messaging_ = t_prefix<t_messaging_>::p_;

///////////////////////////////////////////////////////////////////////////////

  p_messaging_ mr_ = nullptr; // atomic or shared_ptr

///////////////////////////////////////////////////////////////////////////////

namespace messenger
{
  t_visibility_name to_name(t_visibility) {
    // XXX
    return {};
  }

  t_messenger mk_(R_id id) {
    return t_messenger{id};
  }

  t_messenger::t_messenger(x_messenger messenger) : id_{messenger.id_} {
    set(messenger.id_.fd)  = -1;
    set(messenger.id_.key) = 0;
  }

  t_messenger::~t_messenger() {
    if (mr_)
      mr_->destroy_messenger(id_);
  }

  t_messenger::operator t_validity() const {
    return get(id_.fd) != -1 ? VALID : INVALID;
  }

  t_fd t_messenger::get_fd() const {
    return id_.fd;
  }

  t_key t_messenger::get_key() const {
    return id_.key;
  }

  t_name t_messenger::get_name(t_err err) const {
    ERR_GUARD(err) {
      if (mr_)
        return mr_->get_name(err, id_);
      err = err::E_XXX;
    }
    return {};
  }

  t_bool t_messenger::get_params(t_err err, r_params params) const {
    ERR_GUARD(err) {
      if (mr_)
        return mr_->get_params(err, id_, params);
      err = err::E_XXX;
    }
    return false;
  }

  t_bool t_messenger::update_visibility(t_err err, t_visibility visibility) {
    ERR_GUARD(err) {
      if (mr_)
        return mr_->update_visibility(err, id_, visibility);
      err = err::E_XXX;
    }
    return false;
  }

  t_bool t_messenger::update_alive_period(t_err err,
                                          t_multiple_of_100ms multiple) {
    ERR_GUARD(err) {
      if (mr_)
        return mr_->update_alive_period(err, id_, multiple);
      err = err::E_XXX;
    }
    return false;
  }

  t_bool t_messenger::post_message(t_err err, R_key key,
                                   r_message message) const {
    ERR_GUARD(err) {
      if (mr_)
        return mr_->post_message(err, id_, key, message);
      err = err::E_XXX;
    }
    return false;
  }

  t_bool t_messenger::wait_message(t_err err, r_messages messages) const {
    ERR_GUARD(err) {
      if (mr_)
        return mr_->wait_message(err, id_, messages);
      err = err::E_XXX;
    }
    return false;
  }

  t_bool t_messenger::check_message(t_err err, r_messages messages) const {
    ERR_GUARD(err) {
      if (mr_)
        return mr_->check_message(err, id_, messages);
      err = err::E_XXX;
    }
    return false;
  }

  t_bool t_messenger::start_timer(t_err err, R_timer_params params) {
    ERR_GUARD(err) {
      if (mr_)
        return mr_->start_timer(err, id_, params);
      err = err::E_XXX;
    }
    return false;
  }

  t_bool t_messenger::stop_timer(t_err err) {
    ERR_GUARD(err) {
      if (mr_)
        return mr_->stop_timer(err, id_);
      err = err::E_XXX;
    }
    return false;
  }

  t_bool t_messenger::query_timer(t_err err, r_timer_params params) const {
    ERR_GUARD(err) {
      if (mr_)
        return mr_->query_timer(err, id_, params);
      err = err::E_XXX;
    }
    return false;
  }

  t_bool t_messenger::add_to_group(t_err err, R_password password,
                                   R_name name, t_prio prio, t_user user) {
    ERR_GUARD(err) {
      if (mr_)
        return mr_->add_to_group(err, id_, password, name, prio, user);
      err = err::E_XXX;
    }
    return false;
  }

  t_bool t_messenger::remove_from_group(t_err err, R_password password,
                                        R_name name, p_user user) {
    ERR_GUARD(err) {
      if (mr_)
        return mr_->remove_from_group(err, id_, password, name, user);
      err = err::E_XXX;
    }
    return false;
  }

  t_bool t_messenger::is_in_group(t_err err, R_name name, p_user user) const {
    ERR_GUARD(err) {
      if (mr_)
        return mr_->is_in_group(err, id_, name, user);
      err = err::E_XXX;
    }
    return false;
  }

  t_bool t_messenger::get_groups(t_err err, r_group_list group_list) const {
    ERR_GUARD(err) {
      if (mr_)
        return mr_->get_groups(err, id_, group_list);
      err = err::E_XXX;
    }
    return false;
  }

  t_bool t_messenger::add_monitor(t_err err, R_name name, t_prio prio,
                                  t_user user) {
    ERR_GUARD(err) {
      if (mr_)
        return mr_->add_monitor(err, id_, name, prio, user);
      err = err::E_XXX;
    }
    return false;
  }

  t_bool t_messenger::remove_monitor(t_err err, R_name name, p_user user) {
    ERR_GUARD(err) {
      if (mr_)
        return mr_->remove_monitor(err, id_, name, user);
      err = err::E_XXX;
    }
    return false;
  }

  t_key t_messenger::is_monitored(t_err err, R_name name, p_user user) const {
    ERR_GUARD(err) {
      if (mr_)
        return mr_->is_monitored(err, id_, name, user);
      err = err::E_XXX;
    }
    return t_key{0};
  }

  t_bool t_messenger::get_monitored(t_err err,
                                    r_monitor_list monitor_list) const {
    ERR_GUARD(err) {
      if (mr_)
        return mr_->get_monitored(err, id_, monitor_list);
      err = err::E_XXX;
    }
    return false;
  }
}

///////////////////////////////////////////////////////////////////////////////

  t_visibility_name to_name(t_visibility visibility) {
    // XXX
    return {};
  }

  t_messenger_create_params default_messenger_create_params() {
    // XXX
    return {};
  }

  t_validity start(t_err err, P_params params) {
    ERR_GUARD(err) {
      static t_mutex_lock lock(err);
      <% auto scope = lock.make_locked_scope(err);
        if (scope == VALID) {
          if (!mr_) {
            mr_ = new t_messaging_{err, params ? *params : t_params{}};
            if (err)
              delete named::reset(mr_);
          }
          return VALID;
        }
      %>
    }
    return INVALID;
  }

  t_void destroy() {
    if (mr_) {
      mr_->clean_death();
      delete named::reset(mr_);
    }
  }

  t_bool is_running() {
    if (mr_)
      return true;
    return false;
  }

  t_validity update(t_err err, R_params params) {
    ERR_GUARD(err) {
      if (mr_)
        return mr_->update(err, params);
      err = err::E_XXX;
    }
    return INVALID;
  }

  t_void fetch(t_err err, r_params params) {
    ERR_GUARD(err) {
      if (mr_)
        mr_->fetch(err, params);
      err = err::E_XXX;
    }
  }

  t_messenger create_messenger(t_err err, R_messenger_name name,
                                          R_messenger_create_params params) {
    ERR_GUARD(err) {
      if (mr_)
        return messenger::mk_(mr_->create_messenger(err, name, params));
      err = err::E_XXX;
    }
    return messenger::mk_(t_messenger_id{});
  }

  t_bool fetch_messenger(t_err err, R_messenger_name name,
                                    r_messenger_params params) {
    ERR_GUARD(err) {
      if (mr_)
        return mr_->fetch_messenger(err, name, params);
      err = err::E_XXX;
    }
    return false;
  }

  t_bool fetch_messenger(t_err err, R_messenger_name name,
                                    r_messenger_info info, t_bool clearstats) {
    ERR_GUARD(err) {
      if (mr_)
        return mr_->fetch_messenger(err, name, info, clearstats);
      err = err::E_XXX;
    }
    return false;
  }

  t_bool fetch_messengers(t_err err, r_messenger_infos infos,
                                     t_bool clearstats) {
    ERR_GUARD(err) {
      if (mr_)
        return mr_->fetch_messengers(err, infos, clearstats);
      err = err::E_XXX;
    }
    return false;
  }

  t_bool create_group(t_err err, R_password password, R_messenger_name name,
                                 t_messenger_visibility visibility) {
    ERR_GUARD(err) {
      if (mr_)
        return mr_->create_group(err, password, name, visibility);
      err = err::E_XXX;
    }
    return false;
  }

  t_bool destroy_group(t_err err, R_password password, R_messenger_name name) {
    ERR_GUARD(err) {
      if (mr_)
        return mr_->destroy_group(err, password, name);
      err = err::E_XXX;
    }
    return false;
  }

  t_bool fetch_group(t_err err, R_messenger_name name,
                                r_messenger_visibility visibility,
                                p_messenger_group_list group_list) {
    ERR_GUARD(err) {
      if (mr_)
        return mr_->fetch_group(err, name, visibility, group_list);
      err = err::E_XXX;
    }
    return false;
  }

  t_bool add_messenger_to_group(t_err err, R_messenger_password password,
                                           R_messenger_name name,
                                           R_messenger_name group,
                                           t_messenger_prio prio,
                                           t_messenger_user user) {
    ERR_GUARD(err) {
      if (mr_)
        return mr_->add_messenger_to_group(err, password, name, group, prio,
                                                user);
      err = err::E_XXX;
    }
    return false;
  }

  t_bool remove_messenger_from_group(t_err err, R_messenger_password password,
                                                R_messenger_name name,
                                                R_messenger_name group,
                                                p_messenger_user user) {
    ERR_GUARD(err) {
      if (mr_)
        return mr_->remove_messenger_from_group(err, password, name, group,
                                                user);
      err = err::E_XXX;
    }
    return false;
  }

  t_bool is_messenger_in_group(t_err err, R_messenger_name name,
                                          R_messenger_name group,
                                          p_messenger_user user) {
    ERR_GUARD(err) {
      if (mr_)
        return mr_->is_messenger_in_group(err, name, group, user);
      err = err::E_XXX;
    }
    return false;
  }

  t_bool fetch_messenger_groups(t_err err, R_messenger_name name,
                                           r_messenger_group_list group_list) {
    ERR_GUARD(err) {
      if (mr_)
        return mr_->fetch_messenger_groups(err, name, group_list);
      err = err::E_XXX;
    }
    return false;
  }

  t_bool who_is(t_err err, R_messenger_key key, r_messenger_name name,
                t_bool* group, t_bool* local) {
    ERR_GUARD(err) {
      if (mr_)
        return mr_->who_is(err, key, name, group, local);
      err = err::E_XXX;
    }
    return false;
  }

  t_bool post_message(t_err err, R_messenger_key key, r_message message) {
    ERR_GUARD(err) {
      if (mr_)
        return mr_->post_message(err, key, message);
      err = err::E_XXX;
    }
    return false;
  }

///////////////////////////////////////////////////////////////////////////////

  struct automatic_start_ {
    automatic_start_() {
      t_err err;
      if (start(err) == INVALID) {
        // what to do
        // XXX - 11
      }
    }

    ~automatic_start_() {
      destroy();
    }
  };

  automatic_start_ start_;

///////////////////////////////////////////////////////////////////////////////
}
}
