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

#include "mh/megahal.h"

#include <string>
#include <fstream> // DEBUG:
#include <sstream>
#include <algorithm>

#include <thread>

#include <skivvy/logrep.h>
#include <skivvy/stl.h>
#include <skivvy/str.h>

namespace skivvy { namespace ircbot {

PLUGIN_INFO("artibot", "Artibot AI", "0.1");
IRC_BOT_PLUGIN(ArtibotIrcBotPlugin);

using namespace skivvy;
using namespace skivvy::types;
using namespace skivvy::utils;
using namespace skivvy::string;

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

std::iostream& ArtibotIrcBotPlugin::post(std::iostream& io, const str& data)
{
	for(const net::cookie_pair& c: cookies)
	{
		bug("c: " << c.second.key << " = " << c.second.val);
	}
	bug("Cookie: " << net::get_cookie_header(cookies, ".pandorabots.com"));

	io << "POST /pandora/talk?botid=9400cc047e36a720 HTTP/1.1\r\n";
	io << "Host: www.pandorabots.com\r\n";
	io << "User-Agent: " << bot.get_name() << " v" << bot.get_version() << " " << bot.nick << "\r\n";
	io << "Accept: */*\r\n";
	io << "Connection: keep-alive\r\n";
	io << "Referer: http://www.pandorabots.com/pandora/talk?botid=9400cc047e36a720\r\n";

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
	io << "GET /pandora/talk?botid=9400cc047e36a720 HTTP/1.1\r\n";
	io << "Host: www.pandorabots.com\r\n";
	io << "User-Agent: " << bot.get_name() << " v" << bot.get_version() << " " << bot.nick << "\r\n";
	io << "Accept: */*\r\n";
	io << "Connection: keep-alive\r\n";
	io << "\r\n";
	io << std::flush;
	return io;
}

// TODO use logrep's extract_delimited_text instead of this
std::string extract_form_data(const str& html)
{
	str formdata;

	str::size_type pos = 0;
	str::size_type end = 0;
	static const str::size_type npos = str::npos;
	str bdelim = R"(<input type="HIDDEN" name=")";
	str edelim = R"(")";

	if((pos = html.find(bdelim, 0)) != npos
	&& (end = html.find(edelim, (pos = pos + bdelim.size()))) != npos)
	{
		formdata = html.substr(pos, end - pos);

		bdelim = R"(value=")";
		edelim = R"(")";

		if((pos = html.find(bdelim, end)) != npos
		&& (end = html.find(edelim, (pos = pos + bdelim.size()))) != npos)
			formdata += "=" + html.substr(pos, end - pos);
	}

	//bug("formdata: " << formdata);
	return formdata;
}


static str extract_reply_text(const str& html, const str& nick)
{
	str reply;

	str::size_type pos = 0;
	str::size_type end = 0;
	static const str::size_type npos = str::npos;

	str bdelim = R"(<b>9jawap robot:</b>)";
	str edelim = R"(<br>)";

	if((pos = html.find(bdelim, 0)) != npos
	&& (end = html.find(edelim, (pos = pos + bdelim.size()))) != npos)
	{
		reply = html.substr(pos, end - pos);
	}

	pos = reply.find("9jawap Chatbuddie");
	if(pos != str::npos)
		reply.replace(pos, 17, nick);

	//bug("reply: " << reply);
	return reply;
}

static std::istream& getsentence(std::istream& is, str& s)
{
	static str const punct = ".?!";
	s.clear();
	char c;
	while(is.get(c) && punct.find(c) == str::npos)
	{
		s += c;
	}
	if(is && std::ispunct(c)) s += c;
	else s.clear();

	// must begin with uppercase letter
	if(!s.empty() && !std::isupper(s[0]))
		s.clear();

	// must not contain abbreviation
//	static str_vec abbrs;
//	if(abbrs.empty())
//	{
//		abbrs.push_back("Jr.");
//		abbrs.push_back("Sr.");
//	}
//	for(const str& abbr: abbrs)
//		if(!s.empty() && s.find(abbr) != str::npos)
//			s.clear();


	return is;
}

static str read_wikipedia(str_vec& v)
{
	str title;
	net::socketstream ss;

	str UA = "SkivvyBot/0.1 (+http://sookee.dyndns.org/skivvy) (mailto:oaskivvy@gmail.com)";

	log("Request random title");

	ss.open("en.wikipedia.org", 80);
	ss << "HEAD /wiki/Special:Random HTTP/1.1\r\n";
	ss << "Host: en.wikipedia.org\r\n";
	ss << "User-Agent: " << UA << "\r\n";
	ss << "Accept: */*\r\n";
	ss << "\r\n" << std::flush;

	net::header_map headers;
	net::read_http_headers(ss, headers);
	net::header_citer i = headers.find("location");

	if(i != headers.cend() && i->second.size() > 29)
	{
		title = i->second.substr(29);
		std::string params = "format=xml&action=query&redirects&titles=" + i->second.substr(29);

		std::replace(title.begin(), title.end(), '_', ' ');

		log("Request pageid.");

		ss.open("en.wikipedia.org", 80);
		ss << "GET /w/api.php?" << params << " HTTP/1.1\r\n";
		ss << "Host: en.wikipedia.org\r\n";
		ss << "User-Agent: " << UA << "\r\n";
		ss << "Accept: */*\r\n";
		ss << "\r\n" << std::flush;

		headers.clear();
		net::read_http_headers(ss, headers);

		std::ostringstream oss;
		for(char c; ss.get(c); oss.put(c));

		str xml = oss.str();
		str::size_type pos;
		if((pos = xml.find("pageid=\"")) != str::npos && xml.size() > (pos + 8))
		{
			std::istringstream iss(xml.substr(pos + 8));
			str pageid;
			std::getline(iss, pageid, '"');

			log("Request Page: " << pageid);

			params = "action=parse&format=xml&prop=text&pageid=" + pageid;

			ss.open("en.wikipedia.org", 80);
			ss << "GET /w/api.php?" << params << " HTTP/1.1\r\n";
			ss << "Host: en.wikipedia.org\r\n";
			ss << "User-Agent: " << UA << "\r\n";
			ss << "Accept: */*\r\n";
			ss << "Accept-Language: en,en-us\r\n";
			ss << "Accept-Charset: ISO-8859-1,utf-8\r\n";
			ss << "Cache-Control: max-age=0\r\n";
			ss << "\r\n" << std::flush;

			headers.clear();
			net::read_http_headers(ss, headers);

			std::ostringstream oss;
			for(char c; ss.get(c); oss.put(c));

			str xml = oss.str();
			str html = net::html_to_text(xml);

			size_t pos = 0;
			std::string p;
			while((pos = extract_delimited_text(html, "<p>", "</p>", p, pos)) != str::npos)
			{
				std::cout << pos << '\n';
				str line = net::html_to_text(p);

				// now extract sentences
				std::istringstream iss(line);
				while(getsentence(iss, line))
				{
					//bug("AI: Sentence: " << line);
					if(line.size() > 30 && line.size() < 100)
					{
						v.push_back(line);
						bug("AI: Learning: " << line);
					}
				}
			}
		}
	}
	return title;
}

str ArtibotIrcBotPlugin::read()
{
	bug_func();
	str title;
	str_vec lines;
	if(!(title = read_wikipedia(lines)).empty())
		for(const str& line: lines)
		{
			bug("MH: Reading: " << line);
			bug("Reply: " << mh(line));
		}
	return title;
}

void ArtibotIrcBotPlugin::ai_read(const message& msg)
{
	if(bot.get(AI) != AI_MEGAHAL)
	{
		bot.fc_reply(msg, "I don't like reading, please talk to me instead.");
		return;
	}
	bot.fc_reply(msg, "I just read about " + read());
}

void ArtibotIrcBotPlugin::reader()
{
	size_t delay = bot.get(AUTOREAD, AUTOREAD_DEFAULT);
//	std::istringstream iss(bot.get(AUTOREAD, AUTOREAD_DEFAULT));
//	iss >> delay;

	log("Artibot: Reading delay: " << (delay / 60) << " minutes");

	size_t count;
	while(!done)
	{
		log("READ WIKIPEDIA: " << read());
		count = delay;
		while(!done && --count != 0)
			std::this_thread::sleep_for(std::chrono::seconds(1));
	}
}

//-----------------------------------------------------
//Artibot AI: ai()
//-----------------------------------------------------
//text: you tierd Skivvy
//say: botcust2=f73f2fe40be32f19&message=you%20tierd%20Skivvy
//c: botcust2 = f73f2fe40be32f19
//Cookie: Cookie: botcust2=f73f2fe40be32f19
//2012-01-21 16:40:30: BAD RESPONSE:
//2012-01-21 16:40:30: ERROR reading headers.

//text: I care about robots that make sense!!!!!!!!!!!!!!!!!
//say: botcust2=d36167980bec7543&message=I%20care%20about%20robots%20that%20make%20sense%21%21%21%21%21%21%21%21%21%21%21%21%21%21%21%21%21
//c: botcust2 = d36167980bec7543
//Cookie: Cookie: botcust2=d36167980bec7543
//2012-01-22 11:13:41: STREAM ERROR:
//2012-01-22 11:13:41:           is: 0
//2012-01-22 11:13:41: ERROR reading headers.


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
		test.replace(pos, bot.nick.size(), "9jawap Chatbuddie");

	str say = formdata + "&message=" + net::urlencode(test);
	bug("say: " << say);

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
	formdata = extract_form_data(html);

	return extract_reply_text(html, bot.nick);
}

static std::string random_excuse(const str& word)
{
	static const str_vec excuses =
	{
		"Don't you think that * is not a very nice word?"
		, "Please be more polite."
		, "I won't answer if you are going to use bad words."
		, "Maybe you should wash your mouth out with soap?"
		, "I would prefer that you talked nicely if you don't mind."
		, "You shouldn't be using words like that."
		, "Mind your language please!"
		, "Would your mother let you use words like that?"
		, "If you talk to me nicely then I might reply."
	};

	str excuse = excuses[rand_int(0, excuses.size() - 1)];
	str::size_type pos;
	while((pos = excuse.find("*")) != str::npos)
		excuse.replace(pos, 1, word);

	return excuse;
}

str ArtibotIrcBotPlugin::mh(const str& text)
{
	bug("-----------------------------------------------------");
	bug(get_name() << ": mh()");
	bug("text: " << text);
	bug("-----------------------------------------------------");

	static size_t count = 0; // for regular brain saving

	if(!(++count % 10)) mh::megahal_cleanup();

	std::ostringstream oss;
	char* reply = mh::megahal_do_reply(text.c_str(), 0);
	mh::megahal_output(reply, oss);

	return oss.str();
}

str ArtibotIrcBotPlugin::mh(const message& msg)
{
	str text = msg.get_user_params();
	str word;
	std::istringstream iss(text);
	while(iss >> word)
	{
		bug("CHECK WORD: " << word);
		for(const str& match: banned_words)
			if(bot.wild_match(match, word))
			{
				offender_mtx.lock();
				++offender_map[msg.from];

				// Save offender map
				std::ofstream ofs(bot.getf(OFFENDER_FILE, OFFENDER_FILE_DEFAULT));
				for(const str_siz_pair& p: offender_map)
					ofs << p.first << '\n' << p.second << '\n';
				ofs.close();
				offender_mtx.unlock();

				if(offender_map[msg.from] > 3)
				{
					log("Adding to ignores: " << msg.from);
					offends.push_back(msg.from);
				}
				log("Warning to: " << msg.from);
				return random_excuse(word);
			}
	}
	return mh(text);
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
		if(offender_map[user] > 3) offends.push_back(user);
	}

	if(bot.get(AI) == AI_MEGAHAL)
	{
		mh::megahal_setnoprompt();
		mh::megahal_setnowrap();
		mh::megahal_setdirectory(const_cast<char*>("."));
		mh::megahal_setnobanner();
		mh::megahal_initialize();

		str word;
		std::ifstream ifs(bot.getf(BANNED_WORD_FILE, BANNED_WORD_FILE_DEFAULT));
		while(std::getline(ifs, word) && !trim(word).empty())
			banned_words.push_back(word);
	}
	else
	{
		net::socketstream io;
		io.open("www.pandorabots.com", 80);

		if(!get(io))
		{
			log("ERROR getting data.");
			return false;
		}

		net::header_map headers;
		if(!net::read_http_headers(io, headers))
		{
			log("ERROR reading headers.");
			return false;
		}

		cookies.clear();
		if(!net::read_http_cookies(io, headers, cookies))
		{
			log("ERROR reading cookies.");
			return false;
		}

		str html;
		if(!net::read_http_response_data(io, headers, html))
		{
			log("ERROR reading response data.");
			return false;
		}

		formdata = extract_form_data(html);
	}

	bot.add_monitor(*this);

	if(bot.get(AI) == AI_MEGAHAL && !bot.get(AUTOREAD).empty())
		reader_func = std::async(std::launch::async, [&]{ reader(); });

	add
	(action(
		"!ai_read"
		, "!ai_read Tell the AI to go read a random page from Wikipedia."
		, [&](const message& msg){ ai_read(msg); }
	));
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

	bug("Megahal cleanup.");
	if(bot.get(AI) == AI_MEGAHAL)
		mh::megahal_cleanup();
	bug("Wait on thread death.");
	if(reader_func.valid())
		if(reader_func.wait_for(std::chrono::seconds(10)) == std::future_status::ready)
			reader_func.get();
}

// INTERFACE: IrcBotMonitor

void ArtibotIrcBotPlugin::event(const message& msg)
{
	if(msg.cmd != "PRIVMSG")
		return;

	for(const str& s: ignores)
		if(msg.from.find(s) != str::npos)
			return;

	str to = lowercase(msg.text);//get_user_cmd();
	str nick = lowercase(bot.nick);
	if(bot.get(RESPOND) == RESPOND_CASUAL ? to.find(nick) != str::npos : to.find(nick) == 0)
	{
		for(const str& s: offends)
			if(msg.from.find(s) != str::npos)
			{
				bot.fc_reply(msg, msg.get_nick() + ": I am not speaking to you today.");
				return;
			}

		str response = msg.get_nick() + ": ";
		if(bot.get(AI) == AI_MEGAHAL)
			response += mh(msg);
		else
			response += ai(msg.get_user_params());
		bot.fc_reply(msg, response);
	}

	if(msg.text.size() > 8 && msg.text.find("\001ACTION ") == 0)
	{
		bug_msg(msg);
		std::string action;
		extract_delimited_text(msg.text, "\001ACTION ", "\001", action);
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
			const str_set& nicks = bot.nicks[msg.params];
			bug("msg.params: " << msg.params);

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
		if(pc < siz(rand_int(0, 99)))
			return;

		bug_var(acts.size());
		auto a = acts.begin();
		std::advance(a, rand_int(0, acts.size() - 1));
		if(a != acts.cend())
			ai_random_acts(*a, msg.params); // to test
	}
}

void ArtibotIrcBotPlugin::ai_random_acts(str action, const str& channel)
{
	bug_func();
	bug_var(action);
	bug_var(channel);
	str_vec chans(bot.chans.cbegin(), bot.chans.cend());

//	if(!acts.empty()/* && rand_int(0, 1)*/)
	{
		str_vec nicks;
		for(const str& nick: bot.nicks[channel])
			if(nick != bot.nick)
				nicks.push_back(nick);

		if(!nicks.empty()) // TODO PM has no nicks what to  do abotu wildcards?
			for(size_t pos = 0; (pos = action.find("*", pos)) != str::npos; ++pos)
				if(!(pos && action[pos - 1] == '\\'))
				{
					size_t n = rand_int(0, nicks.size() - 1);
					action.replace(pos, 1, nicks[n]);
					nicks.erase(nicks.begin() + n);
					if(nicks.empty())
						nicks.assign(bot.nicks[channel].cbegin(), bot.nicks[channel].cend());
				}

		size_t pos = 0;
		while((pos = action.find("\\*")) != str::npos)
			action.replace(pos, 2, "*");
		std::async(std::launch::async, [&,channel,action]
		{
			std::this_thread::sleep_for(std::chrono::seconds(rand_int(1, 8)));
			irc->me(channel, action);
		});
	}
}

}} // sookee::ircbot
