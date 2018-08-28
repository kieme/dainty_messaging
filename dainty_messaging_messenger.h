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

#ifndef _DAINTY_MESSAGING_MESSENGER_H_
#define _DAINTY_MESSAGING_MESSENGER_H_

#include <map>
#include "dainty_named.h"
#include "dainty_os_fdbased.h"
#include "dainty_named_string.h"
#include "dainty_container_list.h"
#include "dainty_messaging_err.h"
#include "dainty_messaging_message.h"

namespace dainty
{
namespace messaging
{
namespace messenger
{
  using named::t_bool;
  using named::t_n_;
  using named::t_n;
  using named::t_validity;
  using named::VALID;
  using named::INVALID;
  using os::t_fd;
  using err::t_err;
  using message::t_message;
  using message::r_message;
  using message::t_multiple_of_100ms;

  using t_user     = message::t_messenger_user;
  using t_key      = message::t_messenger_key;
  using t_name     = message::t_messenger_name;
  using t_prio     = message::t_messenger_prio;
  using R_key      = named::t_prefix<t_key>::R_;
  using r_name     = named::t_prefix<t_name>::r_;
  using R_name     = named::t_prefix<t_name>::R_;
  using p_user     = named::t_prefix<t_user>::p_;
  using t_messages = container::list::t_list<t_message>;
  using r_messages = named::t_prefix<t_messages>::r_;

///////////////////////////////////////////////////////////////////////////////

  enum  t_password_tag_ { };
  using t_password = named::string::t_string<t_password_tag_, 16>;
  using R_password = named::t_prefix<t_password>::R_;

///////////////////////////////////////////////////////////////////////////////

  enum t_visibility {
    VISIBILITY_OFF,
    VISIBILITY_PROCESS,
    VISIBILITY_NODE,
    VISIBILITY_SYSTEM
  };

  enum  t_visibility_name_tag_ { };
  using t_visibility_name = named::string::t_string<t_visibility_name_tag_, 14>;

  t_visibility_name to_name(t_visibility);

///////////////////////////////////////////////////////////////////////////////

  class t_timer_params {
  public:
    t_multiple_of_100ms factor;
    t_bool              periodic;
    t_prio              prio;
    t_user              user;

    t_timer_params(t_multiple_of_100ms _factor   = t_multiple_of_100ms(0),
                   t_bool              _periodic = false,
                   t_prio              _prio     = t_prio(0),
                   t_user              _user     = t_user(0L))
      : factor(_factor), periodic(_periodic), prio(_prio), user(_user) {
    }
  };
  using r_timer_params = named::t_prefix<t_timer_params>::r_;
  using R_timer_params = named::t_prefix<t_timer_params>::R_;

///////////////////////////////////////////////////////////////////////////////

  class t_group {
  public:
    t_name     name;
    t_password password;
    t_prio     prio;
    t_user     user;

    t_group() : prio(0), user(0L) {
    }

    t_group(R_name _name, R_password _password, t_prio _prio, t_user _user)
      : name(_name), password(_password), prio(_prio), user(_user) {
    }
  };
  using R_group = named::t_prefix<t_group>::R_;

  using t_group_list = std::map<t_name, t_group>;
  using r_group_list = named::t_prefix<t_group_list>::r_;
  using R_group_list = named::t_prefix<t_group_list>::R_;
  using p_group_list = named::t_prefix<t_group_list>::p_;

///////////////////////////////////////////////////////////////////////////////

  class t_monitor {
  public:
    t_name name;
    t_prio prio;
    t_user user;
    t_key  key;

    t_monitor() : prio{0}, user{0L}, key{0} {
    }

    t_monitor(R_name _name, t_prio _prio, t_user _user, t_key _key)
      : name{_name}, prio{_prio}, user{_user}, key{_key} {
    }
  };

  using t_monitor_list = std::map<t_name, t_monitor>;
  using r_monitor_list = named::t_prefix<t_monitor_list>::r_;
  using R_monitor_list = named::t_prefix<t_monitor_list>::R_;
  using p_monitor_list = named::t_prefix<t_monitor_list>::p_;

///////////////////////////////////////////////////////////////////////////////

  class t_create_params {
  public:
    t_visibility        visibility;
    t_multiple_of_100ms alive_factor;
    t_timer_params      timer_params;

    t_create_params() : visibility(VISIBILITY_PROCESS), alive_factor(0) {
    }

    t_create_params(t_visibility        _visibility,
                    t_multiple_of_100ms _alive_factor = t_multiple_of_100ms(0),
                    R_timer_params      _timer_params = t_timer_params())
      : visibility(_visibility), alive_factor(_alive_factor),
        timer_params(_timer_params) {
    }
  };
  using R_create_params = named::t_prefix<t_create_params>::R_;

///////////////////////////////////////////////////////////////////////////////

  class t_params {
  public:
    // queue size
    t_visibility        visibility;
    t_multiple_of_100ms alive_factor;
    t_timer_params      timer_params;
    t_group_list        group_list;
    t_monitor_list      monitor_list;

    t_params(t_visibility        _visibility   = VISIBILITY_PROCESS,
             t_multiple_of_100ms _alive_factor = t_multiple_of_100ms(0),
             R_timer_params      _timer_params = t_timer_params())
      : visibility(_visibility), alive_factor(_alive_factor),
        timer_params(_timer_params) {
    }

    t_params(t_visibility        _visibility,
             t_multiple_of_100ms _alive_factor,
             R_timer_params      _timer_params,
             R_group_list        _group_list,
             R_monitor_list      _monitor_list)
      : visibility(_visibility), alive_factor(_alive_factor),
        timer_params(_timer_params), group_list(_group_list),
        monitor_list(_monitor_list) {
    }
  };
  using r_params = named::t_prefix<t_params>::r_;
  using R_params = named::t_prefix<t_params>::R_;

///////////////////////////////////////////////////////////////////////////////

  class t_id {
  public:
    t_key key;
    t_fd  fd;

    t_id() : key(0), fd(-1) {
    }

    t_id(R_key _key, t_fd _fd) : key(_key), fd(_fd) {
    }

    operator t_validity() const {
      return get(key) && get(fd) != -1 ? VALID : INVALID;
    }
  };
  using R_id = named::t_prefix<t_id>::R_;

///////////////////////////////////////////////////////////////////////////////

  class t_messenger;
  using r_messenger = named::t_prefix<t_messenger>::r_;
  using R_messenger = named::t_prefix<t_messenger>::R_;
  using x_messenger = named::t_prefix<t_messenger>::x_;

  class t_messenger {
  public:
     t_messenger(x_messenger);
     t_messenger(R_messenger)           = delete;
     r_messenger operator=(R_messenger) = delete;
     r_messenger operator=(x_messenger) = delete;
    ~t_messenger();

    operator t_validity() const;

    t_fd   get_fd    () const;
    t_key  get_key   () const;
    t_name get_name  (t_err) const;
    t_bool get_params(t_err, r_params) const;

    t_bool update_visibility  (t_err, t_visibility);
    t_bool update_alive_period(t_err, t_multiple_of_100ms);

    t_bool post_message (t_err, R_key, r_message) const;
    t_bool wait_message (t_err, r_messages) const;
    t_bool check_message(t_err, r_messages) const;

    t_bool start_timer(t_err, R_timer_params);
    t_bool stop_timer (t_err);
    t_bool query_timer(t_err, r_timer_params) const;

    t_bool add_to_group     (t_err, R_password, R_name, t_prio = t_prio(0),
                                    t_user = t_user());
    t_bool remove_from_group(t_err, R_password, R_name, p_user = nullptr);
    t_bool is_in_group      (t_err, R_name, p_user = nullptr) const;
    t_bool get_groups       (t_err, r_group_list) const;

    t_bool add_monitor   (t_err, R_name, t_prio = t_prio(0), t_user = t_user());
    t_bool remove_monitor(t_err, R_name, p_user = nullptr);
    t_key  is_monitored  (t_err, R_name, p_user = nullptr) const;
    t_bool get_monitored (t_err, r_monitor_list) const;

  private:
    friend t_messenger create_messenger(t_err, R_name, R_create_params);
    t_messenger(R_id id) : id_(id) {
    }

    const t_id id_;
  };

///////////////////////////////////////////////////////////////////////////////
}
}
}

#endif
