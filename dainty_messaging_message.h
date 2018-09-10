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

#ifndef _DAINTY_MESSAGING_MESSAGE_H_
#define _DAINTY_MESSAGING_MESSAGE_H_

#include <utility>
#include "dainty_named.h"
#include "dainty_named_string.h"
#include "dainty_container_bytebuf.h"

namespace dainty
{
namespace messaging
{
namespace message
{
///////////////////////////////////////////////////////////////////////////////

  using named::t_bool;
  using named::t_n_;
  using named::t_n;
  using named::t_validity;
  using named::VALID;
  using named::INVALID;

  using named::t_uint16;
  using named::t_explicit;
  using named::t_prefix;
  using named::string::t_string;

  using container::bytebuf::p_byte;
  using container::bytebuf::P_byte;

  enum  t_bytebuf_tag_ { };
  using t_bytebuf = container::bytebuf::t_bytebuf<t_bytebuf_tag_, 0>;
  using x_bytebuf = named::t_prefix<t_bytebuf>::x_;

  enum t_message_type {
    MSG_NOTIFY,
    MSG_TIMEOUT,
    MSG_ALIVE,
    MSG_FAIL
  };

  enum t_messenger_state {
    STATE_AVAILABLE,
    STATE_AVAILABLE_UNRESPONSIVE,
    STATE_UNAVAILABLE,
  };

///////////////////////////////////////////////////////////////////////////////

  enum  t_messenger_user_tag_ {};
  using t_messenger_user = named::t_user<t_messenger_user_tag_>;

  enum  t_messenger_key_tag_ {};
  using t_messenger_key_ = named::t_uint64;
  using t_messenger_key  = t_explicit<t_messenger_key_, t_messenger_key_tag_>;

  enum  t_messenger_name_tag_ {};
  using t_messenger_name = t_string<t_messenger_name_tag_, 32>;

  enum  t_messenger_prio_tag_ {};
  using t_messenger_prio_ = named::t_uint16;
  using t_messenger_prio  = t_explicit<t_messenger_prio_,
                                       t_messenger_prio_tag_>;

  enum  t_message_domain_tag_ {};
  using t_message_domain_ = named::t_uchar;
  using t_message_domain  = t_explicit<t_message_domain_,
                                       t_message_domain_tag_>;

  enum  t_message_user_tag_ {};
  using t_message_user_ = named::t_char;
  using t_message_user  = t_explicit<t_message_user_, t_message_user_tag_>;

  enum  t_message_version_tag_ {};
  using t_message_version_ = named::t_uchar;
  using t_message_version  = t_explicit<t_message_version_,
                                        t_message_version_tag_>;

  enum  t_multiple_of_100ms_tag_ {};
  using t_multiple_of_100ms = named::t_multiple<100, t_multiple_of_100ms_tag_>;

///////////////////////////////////////////////////////////////////////////////

  class t_message_id {
  public:
    using t_domain  = t_message_domain;
    using t_user    = t_message_user;
    using t_version = t_message_version;

    inline t_message_id(t_domain _domain, t_user _user, t_version _version)
      : domain(_domain), user(_user), version(_version) {
    }

    t_domain  domain;
    t_user    user;
    t_version version;
  };
  using r_message_id = t_prefix<t_message_id>::r_;
  using R_message_id = t_prefix<t_message_id>::R_;

///////////////////////////////////////////////////////////////////////////////

  class t_message;
  using r_message = named::t_prefix<t_message>::r_;
  using x_message = named::t_prefix<t_message>::x_;
  using R_message = named::t_prefix<t_message>::R_;

  class t_message {
  public:
    using t_key     = t_messenger_key;
    using t_version = t_message_version;
    using r_id      = r_message_id;
    using R_id      = R_message_id;
    using t_user    = t_message_user;
    using t_domain  = t_message_domain;

    t_message();
    t_message(t_n);
    t_message(x_message);
    t_message(R_message);

    r_message operator=(x_message);
    r_message operator=(R_message);

    t_bool set(R_id, t_n len, t_uint16 cnt = 0);
    t_bool get(r_id       id,
               t_n&       len,
               t_key&     dst,
               t_key&     src,
               t_uint16&  cnt);

    operator t_bool    () const;
    operator t_validity() const;

    t_bool  is_empty    () const;
    t_n     get_capacity() const;

    p_byte  data();
    P_byte  data() const;
    P_byte cdata() const;

    t_message clone() const;

  private:
    t_bytebuf buf_;
  };

  t_messenger_key read_dst(R_message);
  t_messenger_key read_src(R_message);
  t_message_id    read_id (R_message);
  t_n             read_len(R_message);

///////////////////////////////////////////////////////////////////////////////

  class t_notify_message;
  using r_notify_message = named::t_prefix<t_notify_message>::r_;
  using x_notify_message = named::t_prefix<t_notify_message>::x_;

  class t_notify_message : public t_message {
  public:
    t_notify_message();

    inline t_notify_message(x_message msg) : t_message(std::move(msg)) {
    }

    inline t_notify_message(x_notify_message msg) : t_message(std::move(msg)) {
    }

    inline r_notify_message operator=(x_notify_message msg) {
      t_message::operator=(std::move(msg));
      return *this;
    }

    t_bool get(t_messenger_state&,
               t_messenger_name&,
               t_messenger_key&,
               t_messenger_prio&,
               t_messenger_user&);
  };

///////////////////////////////////////////////////////////////////////////////

  class t_timeout_message;
  using r_timeout_message = named::t_prefix<t_timeout_message>::r_;
  using x_timeout_message = named::t_prefix<t_timeout_message>::x_;

  class t_timeout_message : public t_message {
  public:
    t_timeout_message();

    inline t_timeout_message(x_message msg) : t_message(std::move(msg)) {
    }

    inline
    t_timeout_message(x_timeout_message msg) : t_message(std::move(msg)) {
    }

    inline r_timeout_message operator=(x_timeout_message msg) {
      t_message::operator=(std::move(msg));
      return *this;
    }

    t_bool get(t_bool& periodic,
               t_multiple_of_100ms&,
               t_messenger_key&,
               t_messenger_prio&,
               t_messenger_user&);
  };

///////////////////////////////////////////////////////////////////////////////

  class t_fail_message;
  using r_fail_message = named::t_prefix<t_fail_message>::r_;
  using x_fail_message = named::t_prefix<t_fail_message>::x_;

  class t_fail_message : public t_message {
  public:
    enum reason_t {
      reason_messenger_noexist = 1
    };

    inline t_fail_message(t_n n) : t_message(n) {
    }

    inline t_fail_message(x_message msg) : t_message(std::move(msg)) {
    }

    inline t_fail_message(x_fail_message msg) : t_message(std::move(msg)) {
    }

    inline r_fail_message operator=(x_fail_message msg) {
      t_message::operator=(std::move(msg));
      return *this;
    }

    t_bool get(reason_t&, r_message send_message);
  };

///////////////////////////////////////////////////////////////////////////////

  class t_alive_message;
  using r_alive_message = named::t_prefix<t_alive_message>::r_;
  using x_alive_message = named::t_prefix<t_alive_message>::x_;

  class t_alive_message : public t_message {
  public:
    t_alive_message();

    inline t_alive_message(x_message msg) : t_message(std::move(msg)) {
    }

    inline t_alive_message(x_alive_message msg) : t_message(std::move(msg)) {
    }

    inline r_alive_message operator=(x_alive_message msg) {
      t_message::operator=(std::move(msg));
      return *this;
    }

    t_bool get(); // not sure what to pass yet
  };

///////////////////////////////////////////////////////////////////////////////
}
}
}

#endif
