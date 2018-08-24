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

#ifndef _DAINTY_MESSAGING_H_
#define _DAINTY_MESSAGING_H_

#include <vector>
#include "dainty_messaging_messenger.h"

namespace dainty
{
namespace messaging
{
  using t_messenger_params        = messenger::t_params;
  using t_messenger_visibility    = messenger::t_visibility;
  using t_messenger_create_params = messenger::t_create_params;

///////////////////////////////////////////////////////////////////////////////

  enum t_visibility {
    VISIBILITY_OFF,
    VISIBILITY_LOCAL,
    VISIBILITY_SLAVE,
    VISIBILITY_MASTER
  };

  enum  t_name_tag_ { };
  using t_name = named::string::t_string<t_name_tag_, 16>;
  using R_name = named::t_prefix<t_name>::R_;

///////////////////////////////////////////////////////////////////////////////

  class t_params {
  public:
    t_visibility visibility;
    t_name       name;

    params_t() : visibility(VISIBILITY_OFF) {
    }

    params_t(t_visibility _visibility) : visibility(_visibility) {
    }

    params_t(t_visibility visibility, R_name name)
      : visibility_(visibility), name_(name) {
    }
  };

///////////////////////////////////////////////////////////////////////////////

  class t_messenger_stats {
  public:
    // XXX
    t_void reset() {
    }
  };

  class t_messenger_info {
  public:
    t_messenger_key_t  key;
    t_messenger_name_t name;
    t_messenger_params params;
    t_messenger_stats  stats;
    t_messenger_info() : key_(0) {
    }
  };

  using t_messenger_infos = std::vector<t_messenger_info>;

///////////////////////////////////////////////////////////////////////////////

  t_bool is_running();

  void_t update_params(R_params);
  void_t fetch_params (r_params);

///////////////////////////////////////////////////////////////////////////////

  t_messenger_create_params default_messenger_create_params();

///////////////////////////////////////////////////////////////////////////////

  t_messenger create_messenger(t_err,
                               R_messenger_name,
                               R_messenger_create_params
                                 = default_messenger_create_params());

  t_bool fetch_messenger(R_messenger_name, r_messenger_params);
  t_bool fetch_messenger(R_messenger_name, r_messenger_info,
                         t_bool clearstats = false);

  t_bool fetch_messengers(r_messenger_infos, t_bool clearstats = false);

///////////////////////////////////////////////////////////////////////////////

  // group - individual, chained, chained_revert
  t_bool create_group(R_password, R_messenger_name, t_messenger_visibility);
  t_bool destroy_group(R_password, R_messenger_name);
  t_bool fetch_group(R_messenger_name, r_messenger_visibility,
                     p_messenger_group_list = nullptr);

  t_bool add_messenger_to_group(R_password,
                                R_messenger_name name,
                                R_messenger_name group,
                                t_messenger_prio = t_messenger_prio(0),
                                t_messenger_user = t_messenger_user());
  t_bool remove_messenger_from_group(R_password,
                                     R_messenger_name name,
                                     R_messenger_name group,
                                     p_messenger_user = nullptr);
  t_bool is_messenger_in_group (R_messenger_name name,
                                R_messenger_name group,
                                p_messenger_user = nullptr);
  t_bool fetch_messenger_groups(R_messenger_name name,
                                r_messenger_group_list);

///////////////////////////////////////////////////////////////////////////////

  t_bool who_is(R_messenger_key, r_messenger_name,
                t_bool* group = nullptr, t_bool* local = nullptr);

///////////////////////////////////////////////////////////////////////////////

  t_bool post_message(R_messenger_key, r_message);

///////////////////////////////////////////////////////////////////////////////
}
}

#endif
