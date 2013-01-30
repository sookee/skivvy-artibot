#pragma once
#ifndef _SOOKEE_IRCBOT_ARTIBOT_H_
#define _SOOKEE_IRCBOT_ARTIBOT_H_
/*
 * ircbot-artibot.h
 *
 *  Created on: 05 Aug 2011
 *      Author: oaskivvy@gmail.com
 */

/*-----------------------------------------------------------------.
| Copyright (C) 2011 SooKee oaskivvy@gmail.com               |
'------------------------------------------------------------------'

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
02110-1301, USA.

http://www.gnu.org/licenses/gpl-2.0.html

'-----------------------------------------------------------------*/

// Artificial Intelligence Plugin

#include <skivvy/ircbot.h>
#include <skivvy/network.h>

#include <mutex>
#include <future>

namespace skivvy { namespace ircbot {

class ArtibotIrcBotPlugin _final_
: public BasicIrcBotPlugin
, public IrcBotMonitor
{
public:

private:

	str_vec ignores; // complete ignore
	str_vec offends; // get warning

	// Session & form data
	std::string formdata;
	net::cookie_jar cookies;
//	std::map<std::string, net::cookie> cookies;

	str_vec banned_words;
	str_siz_map offender_map;

	std::iostream& get(std::iostream& io);
	std::iostream& post(std::iostream& io, const std::string& data);

	bool done;
	std::future<void> reader_func;

	std::mutex offender_mtx;
	std::mutex random_acts_mtx;

	str read();
	void reader();
	void ai_read(const message& msg);
	void ai_random_acts(str action, const str& channel);

	std::string ai(const std::string& text);
	std::string mh(const str& text);
	std::string mh(const message& msg);

public:
	ArtibotIrcBotPlugin(IrcBot& bot);
	virtual ~ArtibotIrcBotPlugin();

	// INTERFACE: BasicIrcBotPlugin

	virtual bool initialize();

	// INTERFACE: IrcBotPlugin

	virtual std::string get_id() const;
	virtual std::string get_name() const;
	virtual std::string get_version() const;
	virtual void exit();

	// INTERFACE: IrcBotMonitor

	virtual void event(const message& msg);
};

}} // sookee::ircbot

#endif // _SOOKEE_IRCBOT_ARTIBOT_H_
