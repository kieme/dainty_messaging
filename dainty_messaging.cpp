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
#include "dainty_mt_event_dispatcher.h"
#include "dainty_mt_waitable_chained_queue.h"
#include "dainty_mt_command.h"
#include "dainty_mt_thread.h"
#include "dainty_os_threading.h"
#include "dainty_os_clock.h"
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

using t_thd_err       = t_thread::t_logic::t_err;
using t_cmd_err       = command::t_processor::t_logic::t_err;
using t_cmd_client    = command::t_client;
using t_cmd_processor = command::t_processor;
using t_cmd           = command::t_command;
using t_any_user      = any::t_user;
using t_que_chain     = waitable_chained_queue::t_chain;
using t_que_client    = waitable_chained_queue::t_client;
using t_que_processor = waitable_chained_queue::t_processor;

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

  t_message& t_message::operator=(x_bytebuf) {
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

  //XXX

  struct t_clean_death_cmd_ : t_cmd {
    constexpr static command::t_id cmd_id = 25;

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

    messenger::t_id create_messenger(r_err err, R_messenger_name name,
                                     R_messenger_create_params params) {
    }

    t_bool fetch_messenger(r_err err, R_messenger_name name,
                           r_messenger_params params) {
    }

    t_bool fetch_messenger(r_err err, R_messenger_name name,
                           r_messenger_info info, t_bool clearstats) {
    }

    t_bool fetch_messengers(r_err err, r_messenger_infos infos,
                            t_bool clearstats) {
    }

    t_bool create_group(r_err err, R_password password,
                        R_messenger_name, t_messenger_visibility visibility) {
    }

    t_bool destroy_group(r_err err, R_password password,
                         R_messenger_name name) {
    }

    t_bool fetch_group(r_err err, R_messenger_name name,
                       r_messenger_visibility visibility,
                       p_messenger_group_list group_list) {
    }

    t_bool add_messenger_to_group(r_err err, R_messenger_password password,
                                  R_messenger_name name,
                                  R_messenger_name group,
                                  t_messenger_prio prio,
                                  t_messenger_user user) {
    }

    t_bool remove_messenger_from_group(r_err err,
                                       R_messenger_password password,
                                       R_messenger_name name,
                                       R_messenger_name group,
                                       p_messenger_user user) {
    }

    t_bool is_messenger_in_group(r_err err, R_messenger_name name,
                                 R_messenger_name group,
                                 p_messenger_user user) {
    }

    t_bool fetch_messenger_groups(r_err err, R_messenger_name name,
                                  r_messenger_group_list group_list) {
    }

    t_bool who_is(r_err err, R_messenger_key key, r_messenger_name name,
                  t_bool* group, t_bool* local) {
    }

    t_bool post_message(r_err err, R_messenger_key key, r_message message) {
    }

    t_name get_name(r_err err, messenger::R_id id) const {
    }

    t_bool get_params(r_err err, messenger::R_id id, r_params params) const {
    }

    t_bool update_visibility(r_err err, messenger::R_id id,
                             t_visibility visibility) {
    }

    t_bool update_alive_period(r_err err, messenger::R_id id,
                               t_multiple_of_100ms) {
    }

    t_bool post_message(r_err err, messenger::R_id id, R_key, r_message) const {
    }

    t_bool wait_message(r_err err, messenger::R_id id, r_messages) const {
    }

    t_bool check_message(r_err err, messenger::R_id id, r_messages) const {
    }

    t_bool start_timer(r_err err, messenger::R_id id, R_timer_params) {
    }

    t_bool stop_timer(r_err err, messenger::R_id id) {
    }

    t_bool query_timer(r_err err, messenger::R_id id, r_timer_params) const {
    }

    t_bool add_to_group(r_err err, messenger::R_id id, R_password, R_name, t_prio,
                        t_user) {
    }

    t_bool remove_from_group(r_err err, messenger::R_id id, R_password, R_name,
                             p_user) {
    }

    t_bool is_in_group(r_err err, messenger::R_id id, R_name, p_user) const {
    }

    t_bool get_groups(r_err err, messenger::R_id id, r_group_list) const {
    }

    t_bool add_monitor(r_err err, messenger::R_id id, R_name, t_prio, t_user) {
    }

    t_bool remove_monitor(r_err err, messenger::R_id id, R_name, p_user) {
    }

    t_key is_monitored(r_err err, messenger::R_id id, R_name, p_user) const {
    }

    t_bool get_monitored(r_err err, messenger::R_id id, r_monitor_list) const {
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

  t_messenger::t_messenger(x_messenger) {
    // XXX
  }

  t_messenger::~t_messenger() {
    // XXX
  }

  t_messenger::operator t_validity() const {
    // XXX
    return VALID;
  }

  t_fd t_messenger::get_fd() const {
    // XXX
    return t_fd{0};
  }

  t_key t_messenger::get_key() const {
    // XXX
    return t_key{0};
  }

  t_name t_messenger::get_name(t_err) const {
    // XXX
    return {};
  }

  t_bool t_messenger::get_params(t_err, r_params) const {
    // XXX
    return false;
  }

  t_bool t_messenger::update_visibility(t_err, t_visibility) {
    // XXX
    return false;
  }

  t_bool t_messenger::update_alive_period(t_err, t_multiple_of_100ms) {
    // XXX
    return false;
  }

  t_bool t_messenger::post_message(t_err, R_key, r_message) const {
    // XXX
    return false;
  }

  t_bool t_messenger::wait_message(t_err, r_messages) const {
    // XXX
    return false;
  }

  t_bool t_messenger::check_message(t_err, r_messages) const {
    // XXX
    return false;
  }

  t_bool t_messenger::start_timer(t_err, R_timer_params) {
    // XXX
    return false;
  }

  t_bool t_messenger::stop_timer(t_err) {
    // XXX
    return false;
  }

  t_bool t_messenger::query_timer(t_err, r_timer_params) const {
    // XXX
    return false;
  }

  t_bool t_messenger::add_to_group(t_err, R_password, R_name, t_prio, t_user) {
    // XXX
    return false;
  }

  t_bool t_messenger::remove_from_group(t_err, R_password, R_name, p_user) {
    // XXX
    return false;
  }

  t_bool t_messenger::is_in_group(t_err, R_name, p_user) const {
    // XXX
    return false;
  }

  t_bool t_messenger::get_groups(t_err, r_group_list) const {
    // XXX
    return false;
  }

  t_bool t_messenger::add_monitor(t_err, R_name, t_prio, t_user) {
    // XXX
    return false;
  }

  t_bool t_messenger::remove_monitor(t_err, R_name, p_user) {
    // XXX
    return false;
  }

  t_key t_messenger::is_monitored(t_err, R_name, p_user) const {
    // XXX
    return t_key{0};
  }

  t_bool t_messenger::get_monitored(t_err, r_monitor_list) const {
    // XXX
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
    // XXX
  }

  t_bool fetch_messenger(t_err err, R_messenger_name name,
                         r_messenger_params params) {
    // XXX
    return false;
  }

  t_bool fetch_messenger(t_err err, R_messenger_name name,
                         r_messenger_info info, t_bool clearstats) {
    // XXX
    return false;
  }

  t_bool fetch_messengers(t_err err, r_messenger_infos infos,
                          t_bool clearstats) {
    // XXX
    return false;
  }

  t_bool create_group(t_err err, R_password password, R_messenger_name name,
                      t_messenger_visibility) {
    // XXX
    return false;
  }

  t_bool destroy_group(t_err err, R_password password, R_messenger_name name) {
    // XXX
    return false;
  }

  t_bool fetch_group(t_err err, R_messenger_name name,
                     r_messenger_visibility visibility,
                     p_messenger_group_list group_list) {
    // XXX
    return false;
  }

  t_bool add_messenger_to_group(t_err err, R_messenger_password password,
                                R_messenger_name name,
                                R_messenger_name group,
                                t_messenger_prio prio,
                                t_messenger_user user) {
    // XXX
    return false;
  }

  t_bool remove_messenger_from_group(t_err err, R_messenger_password password,
                                     R_messenger_name name,
                                     R_messenger_name group,
                                     p_messenger_user user) {
    // XXX
    return false;
  }

  t_bool is_messenger_in_group (t_err err, R_messenger_name name,
                                R_messenger_name group,
                                p_messenger_user user) {
    // XXX
    return false;
  }

  t_bool fetch_messenger_groups(t_err err, R_messenger_name name,
                                r_messenger_group_list group_list) {
    // XXX
    return false;
  }

  t_bool who_is(t_err err, R_messenger_key key, r_messenger_name name,
                t_bool* group, t_bool* local) {
    ERR_GUARD(err) {
      if (mr_)
        return mr_->who_is(err, key, name, group, local);
    }
    return false;
  }

  t_bool post_message(t_err err, R_messenger_key key, r_message message) {
    // XXX
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
