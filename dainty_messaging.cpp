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

#include "dainty_messaging_message.h"
#include "dainty_messaging_messenger.h"
#include "dainty_messaging.h"

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

namespace messenger
{
  t_visibility_name to_name(t_visibility) {
    return {};
  }

  t_messenger::t_messenger(x_messenger) {
  }

  t_messenger::~t_messenger() {
  }

  t_messenger::operator t_validity() const {
    return VALID;
  }

  t_fd t_messenger::get_fd() const {
    return t_fd{0};
  }

  t_key t_messenger::get_key() const {
    return t_key{0};
  }

  t_name t_messenger::get_name() const {
    return {};
  }

  t_bool t_messenger::get_params(r_params) const {
    return false;
  }

  t_bool t_messenger::update_visibility(t_visibility) {
    return false;
  }

  t_bool t_messenger::update_alive_period(t_multiple_of_100ms) {
    return false;
  }

  t_bool t_messenger::post_message(R_key, r_message) const {
    return false;
  }

  t_bool t_messenger::wait_message(r_messages) const {
    return false;
  }

  t_bool t_messenger::check_message(r_messages) const {
    return false;
  }

  t_bool t_messenger::start_timer(R_timer_params) {
    return false;
  }

  t_bool t_messenger::stop_timer() {
    return false;
  }

  t_bool t_messenger::query_timer(r_timer_params) const {
    return false;
  }

  t_bool t_messenger::add_to_group(R_password, R_name, t_prio, t_user) {
    return false;
  }

  t_bool t_messenger::remove_from_group(R_password, R_name, p_user) {
    return false;
  }

  t_bool t_messenger::is_in_group(R_name, p_user) const {
    return false;
  }

  t_bool t_messenger::get_groups(r_group_list) const {
    return false;
  }

  t_bool t_messenger::add_monitor(R_name, t_prio, t_user) {
    return false;
  }

  t_bool t_messenger::remove_monitor(R_name, p_user) {
    return false;
  }

  t_key t_messenger::is_monitored(R_name, p_user) const {
    return t_key{0};
  }

  t_bool t_messenger::get_monitored(r_monitor_list) const {
    return false;
  }
}

///////////////////////////////////////////////////////////////////////////////

  t_visibility_name to_name(t_visibility) {
    return {};
  }

///////////////////////////////////////////////////////////////////////////////
}
}
