/*
 * ircbot-artibot.cpp
 *
 *  Created on: 07 Aug 2011
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

#include <skivvy/plugin-artibot.h>

#include <string>
#include <fstream>
#include <sstream>
#include <algorithm>

#include <thread>
#include <array>
#include <vector>

#include <skivvy/network.h>
#include <skivvy/logrep.h>
#include <skivvy/utils.h>
#include <skivvy/stl.h>
#include <skivvy/str.h>

#include <pcrecpp.h>

namespace skivvy { namespace artibot {

PLUGIN_INFO("artibot", "Artibot AI", "1.0-beta");
IRC_BOT_PLUGIN(ArtibotIrcBotPlugin);

using namespace skivvy;
using namespace skivvy::types;
using namespace skivvy::utils;
using namespace skivvy::ircbot;
using namespace skivvy::string;
using namespace pcrecpp;

const str AI = "artibot.ai";
const str AI_MEGAHAL = "megahal";
const str AUTOREAD = "artibot.autoread";
const siz AUTOREAD_DEFAULT = 60 * 60 * 24; // daily
const str OFFENDER_FILE = "artibot.offender_file";
const str OFFENDER_FILE_DEFAULT = "artibot-offenders.txt";
const str BANNED_WORD_FILE = "artibot.banned_word_file";
const str BANNED_WORD_FILE_DEFAULT = "airtibot-banned-words.txt";
const str RESPOND = "artibot.respond";
const str RESPOND_DIRECT = "direct";
const str RESPOND_CASUAL = "casual";
const str RESPOND_DEFAULT = RESPOND_DIRECT;
const str RANDOM_ACTS_FILE = "artibot.random_acts_file";
const str RANDOM_ACTS_FILE_DEFAULT = "artibot-random-acts.txt";
const str RANDOM_ACTS_TRIGER = "artibot.random_acts_trigger";
const siz RANDOM_ACTS_TRIGER_DEFAULT = 50;
const str RA_DELAY = "artibot.random_acts_delay";
const siz RA_DELAY_DEFAULT = 10; // seconds

const str PANDORA_ID = "artibot.id";
const str PANDORA_FORM_PCRE = "artibot.form.pcre";
const str PANDORA_REPLY_PCRE = "artibot.reply.pcre";
const str PANDORA_NAME = "artibot.name";

std::iostream& ArtibotIrcBotPlugin::post(std::iostream& io, const str& data)
{
	for(const net::cookie_pair& c: cookies)
	{
		bug("c: " << c.second.key << " = " << c.second.val);
	}
	bug("Cookie: " << net::get_cookie_header(cookies, ".pandorabots.com"));

	str id = bot.get(PANDORA_ID);

	io << "POST /pandora/talk?botid=" + id + " HTTP/1.1\r\n";
	io << "Host: www.pandorabots.com\r\n";
	io << "User-Agent: " << bot.get_name() << " v" << bot.get_version() << " " << bot.nick << "\r\n";
	io << "Accept: */*\r\n";
	io << "Connection: keep-alive\r\n";
	io << "Referer: http://www.pandorabots.com/pandora/talk?botid=" + id + "\r\n";

	io << net::get_cookie_header(cookies, ".pandorabots.com") << "\r\n";

	io << "Content-Type: application/x-www-form-urlencoded\r\n";
	io << "Content-Length: " << data.size() << "\r\n";
	io << "\r\n";
	io << data;
	io << std::flush;
	return io;
}

std::iostream& ArtibotIrcBotPlugin::get(std::iostream& io)
{
	str id = bot.get(PANDORA_ID);

	io << "GET /pandora/talk?botid=" + id + " HTTP/1.1\r\n";
	io << "Host: www.pandorabots.com\r\n";
	io << "User-Agent: " << bot.get_name() << " v" << bot.get_version() << " " << bot.nick << "\r\n";
	io << "Accept: */*\r\n";
	io << "Connection: keep-alive\r\n";
	io << "\r\n";
	io << std::flush;
	return io;
}

//std::string extract_form_data(const str& html)
//{
//	str formdata;
//
//	RE(R"x(<input type="HIDDEN" name="([^"]*)")x").PartialMatch(html, &formdata);
//
//	return formdata;
//}

//static str extract_reply_text(const str& html, const str& nick)
//{
//	str reply;
//
//	RE(R"x(<b>9jawap robot:</b>([^<]*)<)x").PartialMatch(html, &reply);
//
//	siz pos = reply.find("9jawap Chatbuddie");
//	if(pos != str::npos)
//		reply.replace(pos, 17, nick);
//
//	pos = reply.find("Chat girl");
//	if(pos != str::npos)
//		reply.replace(pos, 9, nick);
//
//	//bug("reply: " << reply);
//	return trim(reply);
//}

str ArtibotIrcBotPlugin::ai(const str& text)
{
	bug("-----------------------------------------------------");
	bug(get_name() << ": ai()");
	bug("-----------------------------------------------------");
	bug("text: " << text);

	str test = lowercase(text);
	size_t pos = test.find(lowercase(bot.nick));
	test = text;
	if(pos != str::npos)
		test.replace(pos, bot.nick.size(), bot.get(PANDORA_NAME));

	str say = formdata + "&message=" + net::urlencode(test);
	bug_var(say);

	net::socketstream io;
	io.open("www.pandorabots.com", 80);

	if(!post(io, say))
	{
		log("ERROR posting data.");
		return "I feel somewhat disconnected.";
	}

	net::header_map headers;
	if(!net::read_http_headers(io, headers))
	{
		log("ERROR reading headers.");
		return "My head hurts.";
	}

	//cookies.clear();
	if(!net::read_http_cookies(io, headers, cookies))
	{
		log("ERROR reading cookies.");
		return "I need more cookies.";
	}

	str html;
	if(!net::read_http_response_data(io, headers, html))
	{
		log("ERROR reading response data.");
		return "I wish I knew how to respond.";
	}

	// Form data
//	formdata = extract_form_data(html);
	str data = html;
	for(const str& pcre: bot.get_vec(PANDORA_FORM_PCRE))
		RE(pcre).PartialMatch(data, &data);
	formdata = data;

	data = html;
	for(const str& pcre: bot.get_vec(PANDORA_REPLY_PCRE))
		RE(pcre).PartialMatch(data, &data);

	str reply = data;

	for(const str& name: bot.get_vec(PANDORA_NAME))
	{
		siz pos = reply.find(name);
		if(pos != str::npos)
			reply.replace(pos, name.size(), bot.nick);
	}

	trim(reply);

	return reply;
}

ArtibotIrcBotPlugin::ArtibotIrcBotPlugin(IrcBot& bot)
: BasicIrcBotPlugin(bot), done(false)
{
}

ArtibotIrcBotPlugin::~ArtibotIrcBotPlugin() {}

// INTERFACE: BasicIrcBotPlugin

bool ArtibotIrcBotPlugin::initialize()
{
	offends = bot.get_vec("artibot.offend");
	ignores = bot.get_vec("artibot.ignore");

	// Load in banned word offenders
	std::ifstream ifs(bot.getf(OFFENDER_FILE, OFFENDER_FILE_DEFAULT));
	std::string user;
	size_t times;
	while(std::getline(ifs, user) && ifs >> times >> std::ws)
	{
		offender_map[user] = times;
		if(times > 3)
			offends.push_back(user);
	}

	log("Initializing PANDORA");
	net::socketstream io;
	io.open("www.pandorabots.com", 80);

	if(!get(io))
	{
		log("ERROR getting data.");
		return true;
	}

	net::header_map headers;
	if(!net::read_http_headers(io, headers))
	{
		log("ERROR reading headers.");
		return true;
	}

	cookies.clear();
	if(!net::read_http_cookies(io, headers, cookies))
	{
		log("ERROR reading cookies.");
		return true;
	}

	str html;
	if(!net::read_http_response_data(io, headers, html))
	{
		log("ERROR reading response data.");
		return true;
	}

	str data = html;
	for(const str& pcre: bot.get_vec(PANDORA_FORM_PCRE))
		RE(pcre).PartialMatch(data, &data);
	formdata = data;

	bot.add_monitor(*this);

	return true;
}

// INTERFACE: IrcBotPlugin

str ArtibotIrcBotPlugin::get_id() const { return ID; }
str ArtibotIrcBotPlugin::get_name() const { return NAME; }
str ArtibotIrcBotPlugin::get_version() const { return VERSION; }

void ArtibotIrcBotPlugin::exit()
{
	bug_func();
	done = true;

	// Save banned word offenders
	bug("Saving banned word offenders.");
	std::ofstream ofs(bot.getf(OFFENDER_FILE, OFFENDER_FILE_DEFAULT));
	for(const str_siz_pair& p: offender_map)
		ofs << p.first << '\n' << p.second << '\n';
	ofs.close();

	bug("Wait on thread death.");
	if(reader_func.valid())
		if(reader_func.wait_for(std::chrono::seconds(10)) == std::future_status::ready)
			reader_func.get();
}

// INTERFACE: IrcBotMonitor

void ArtibotIrcBotPlugin::event(const message& msg)
{
	if(msg.command != "PRIVMSG")
		return;
	BUG_COMMAND(msg);

	str userhost = msg.get_userhost();
	for(const str& s: ignores)
		if(userhost.find(s) != str::npos)
			return;

	str text = msg.get_trailing();

	str to = lowercase(text);//get_user_cmd();
	str nick = lowercase(bot.nick);
	if(bot.get(RESPOND) == RESPOND_CASUAL ? to.find(nick) != str::npos : to.find(nick) == 0)
	{
		for(const str& s: offends)
			if(userhost.find(s) != str::npos)
			{
				bot.fc_reply(msg, msg.get_nickname() + ": I am not speaking to you today.");
				return;
			}

		str response = msg.get_nickname() + ": " + ai(msg.get_user_params());
		bot.fc_reply(msg, response);
	}

	if(text.size() > 8 && text.find("\001ACTION ") == 0)
	{
		bug_msg(msg);
		std::string action;
		extract_delimited_text(text, "\001ACTION ", "\001", action);
		bug("ACTION DETECTED: " << action);

		bool allow_action = true;

		str word;
		siss iss(action);
		while(iss >> word)
			for(const str& match: banned_words)
				if(bot.wild_match(match, word))
				{
					log("BANNED WORD found in action: " << action << " - rejecting");
					allow_action = false;
					break;
				}

		str_set acts;
		lock_guard lock(random_acts_mtx);
		std::ifstream ifs(bot.getf(RANDOM_ACTS_FILE, RANDOM_ACTS_FILE_DEFAULT));
		str act;
		while(std::getline(ifs, act))
			acts.insert(act);
		ifs.close();

		if(allow_action)
		{
			size_t pos = 0;

			bug_var(msg.get_chan());
			const str_set& botnicks = bot.nicks[msg.get_chan()];

			// Sort nicks largest first to ensure whole nicks match
			// rather than sub-nicks
			struct gt
			{
				bool operator()(const str& s1, const str& s2)
				{
					if(s1.size() == s2.size())
						return s1 < s2;
					return s1.size() > s2.size();
				}
			};

			std::set<str, gt> nicks(botnicks.begin(), botnicks.end());

			for(pos = 0; (pos = action.find("*", pos)) != str::npos; pos += 2)
				action.replace(pos, 1, "\\*");

			bug_var(action);

			for(const str& nick: nicks)
				for(pos = 0; (pos = action.find(nick, pos)) != str::npos; ++pos)
					action.replace(pos, nick.size(), "*");

			bug_var(action);

			acts.insert(action);
			std::ofstream ofs(bot.getf(RANDOM_ACTS_FILE, RANDOM_ACTS_FILE_DEFAULT));
			for(const str& act: acts)
				ofs << act << '\n';
			ofs.close();
		}
		if(acts.empty())
			return;

		siz pc = bot.get(RANDOM_ACTS_TRIGER, RANDOM_ACTS_TRIGER_DEFAULT);
		siz ri = siz(rand_int(0, 99));

		bug_var(pc);
		bug_var(ri);

		if(pc <= ri)
			return;

		bug_var(acts.size());
		auto a = acts.begin();
		std::advance(a, rand_int(0, acts.size() - 1));
		if(a != acts.cend())
			ai_random_acts(*a, msg.get_chan()); // to test
	}
}

void ArtibotIrcBotPlugin::ai_random_acts(str action, const str& chan)
{
	bug_func();
	bug_var(action);
	bug_var(chan);
	str_vec nicks(bot.nicks[chan].begin(), bot.nicks[chan].end());

	action = wild_replace(action, nicks);

	// gender

	struct gender
	{
		str m;
		str f;
		gender(const str& m, const str& f): m(m), f(f) {}
	};

	// pronoun map
	static const str_vec m = {"boy", "he", "his", "himself"};
	static const str_vec f = {"girl", "she", "her", "herself"};

	bool male = bot.get("artibot.gender", "male") != "female";

	const str_vec& og = male ? f : m; // old gender
	const str_vec& ng = male ? m : f; // new gender

	str gaction, sep, word;
	siss iss(action);
	while(iss >> word)
	{
		for(siz i = 0; i < og.size(); ++i)
			if(lowercase(word) == lowercase(og[i]))
				word = ng[i];
		gaction += sep + word;
		sep = " ";
	}
	action = gaction;

	std::async(std::launch::async, [&,chan,action]
	{
		std::this_thread::sleep_for(std::chrono::seconds(rand_int(1, bot.get(RA_DELAY, RA_DELAY_DEFAULT))));
		irc->me(chan, action);
	});
}

}} // sookee::artibot
