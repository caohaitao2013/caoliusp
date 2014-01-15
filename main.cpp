/*
* page_capture.cpp - capture a page
*
* Licensed under GPLv2 orlater
*
* Author:	KaiWen<wenkai1987@gmail.com>
* Date:		Mon Dec 16 15:55:09 CST 2013
*/
#include <iostream>
#include <fstream>
#include <boost/atomic.hpp>
#include <boost/thread.hpp>
#include <boost/foreach.hpp>
#include <boost/program_options.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/xpressive/xpressive.hpp>
#include <boost/date_time/gregorian/gregorian.hpp>
#include "page_capture.h"

namespace bs = boost::system;
namespace bx = boost::xpressive;
namespace bg = boost::gregorian;
namespace bp = boost::program_options;

static const char* cl_host = "cl.or.gs";
static const char* home_url = "thread0806.php?fid=16&search=60&page=";

bool verbose;				// show verbose message
int threads;				// threads num
bg::date ge_day;			// the date which page shoule newer or equal than
bg::date le_day;			// the date which page shoule older or equal than
boost::atomic<int> name_c;		// pic name index
unsigned int con_timeout_sec;		// tcp connect timeout seconds
unsigned int rw_timeout_ms;		// tcp read/write timeout millisecond
int limit;				// picture count limit
bool exit_flag = false;			// tell main thread exit

bx::sregex node_reg = bx::sregex::compile("<tr align=\"center\" class=\"tr3 t_one\" "
	"onMouseOver=\"this.className='tr3 t_two'\" onMouseOut=\"this.className='tr3 t_one'\">.*?</tr>");
bx::sregex date_reg = bx::sregex::compile("<div class=\"f10\">(.*?)</div>");
bx::sregex uri_reg = bx::sregex::compile("href=\"(.*?)\"");
bx::sregex pic_reg = bx::sregex::compile("input type='image' src='(.*?)'");
bx::sregex remote_pic_reg = bx::sregex::compile("http://(.*?)/(.*(\\..*))");

void get_pic(const std::vector<std::string>& uri_list, int n) {
	BOOST_FOREACH(const std::string& uri, uri_list) {
		std::string page;
		page_capture pcap(cl_host, uri, con_timeout_sec, rw_timeout_ms);
		pcap.req_page();
		pcap.get_page(page);

		int t[] = {1};
		bx::sregex_token_iterator it(page.begin(), page.end(), pic_reg, t);
		bx::sregex_token_iterator end;

		int i = 0;
		for (; it != end; ++it) {
			bx::smatch what;
			if (!bx::regex_match(*it, what, remote_pic_reg))
				continue;

			std::cout << "get picture: " << *it << " #" << n << "." << ++i << std::endl;

			std::string pic;
			pic.reserve(64 * 1024);
			page_capture pcap(what[1], what[2], con_timeout_sec, rw_timeout_ms);
			pcap.req_page();
			pcap.get_page(pic);

			boost::iterator_range<std::string::iterator> rng = boost::find_first(pic, "\r\n\r\n");
			if (rng) {
				std::string name("./pic/");
				name += boost::lexical_cast<std::string>(++name_c);
				name += what[3];
				std::fstream file(name.c_str(), std::ios::out);
				file.write(&pic[0] + std::distance(pic.begin(), rng.end()), pic.size());
			}

			if (limit > 0 && name_c >= limit) {
				exit_flag = true;
				return ;
			}
		}
	}
}

void parse(const std::string& node, std::vector<std::string>& uri) {
	bx::smatch what;
	if (bx::regex_search(node, what, date_reg)) {
		if (verbose) {
			for (size_t i = 0; i < what.size(); i++)
				std::cout << "regex-match#" << i << " " << what[i] << std::endl;
		}

		if (what[1] != "Top-marks") {
			bg::date d = bg::from_string(what[1]);
			if (ge_day <= d && d <= le_day) {
				bx::smatch uri_match;
				if (bx::regex_search(node, uri_match, uri_reg))
					uri.push_back(uri_match[1]);
			}
		}
	} else {
		if (verbose)
			std::cout << "didn't find date" << std::endl;
	}
}

void parse_latest_page(const std::string& page) {
	bx::sregex_token_iterator it(page.begin(), page.end(), node_reg);
	bx::sregex_token_iterator end;
	std::vector<std::string> uri;

	for (; it != end; ++it) {
		if (verbose) {
			std::cout << "---------------------" << std::endl;
			std::cout << *it << std::endl;
		}

		parse(*it, uri);
	}

	std::cout << "--------today's latest post (total " << uri.size() << " posts)--------" << std::endl;
	for (std::vector<std::string>::iterator it = uri.begin(); it != uri.end(); ++it)
		std::cout << *it << std::endl;

	std::cout << "\n--------------------\n";
	boost::thread_group tgrp;
	int uri_per_thread = uri.size() / threads;
	for (int i = 0; i < threads; i++) {
		std::vector<std::string>::iterator beg, end;
		beg = uri.begin();

		if (i == threads - 1) {
			std::advance(beg, i * uri_per_thread);
			end = uri.end();
		} else {
			std::advance(beg, i * uri_per_thread);
			end = beg;
			std::advance(end, uri_per_thread);
		}

		tgrp.create_thread(boost::bind(&get_pic, std::vector<std::string>(beg, end), i));
	}

	tgrp.join_all();
}

int main(int argc, char* argv[]) {
	bp::options_description desc("Allowed options");
	desc.add_options()
		("help", "display this message")
		("verbose,v", "show verbose message")
		("thread,t", bp::value<int>(&threads)->default_value(1), "set multi-thread num")
		("fdate,n", bp::value<std::string>(), "capture post newer than the date(default today)")
		("tdate,o", bp::value<std::string>(), "capture post older than the date(default today)")
		("cont,c", bp::value<unsigned int>(&con_timeout_sec)->default_value(10), "tcp connect timeout seconds")
		("rwt,r", bp::value<unsigned int>(&rw_timeout_ms)->default_value(2000), "tcp read/write timeout milleseconds")
		("limit,l", bp::value<int>(&limit)->default_value(-1), "picture limit count");

	bp::variables_map vm;
	bp::store(bp::parse_command_line(argc, argv, desc), vm);
	bp::notify(vm);

	if (vm.count("help")) {
		std::cout << desc << "\n";
		return 0;
	}

	verbose = vm.count("verbose") ? true : false;
	ge_day = vm.count("fdate") ? bg::from_string(vm["fdate"].as<std::string>()) : bg::day_clock::local_day();
	le_day = vm.count("tdate") ? bg::from_string(vm["tdate"].as<std::string>()) : bg::day_clock::local_day();

	std::string page;
	int n = 1;
	while (!exit_flag) {
		std::string path = home_url + boost::lexical_cast<std::string>(n);
		page_capture pcap(cl_host, path, con_timeout_sec, rw_timeout_ms);	// :-)
		pcap.req_page();
		pcap.get_page(page);

		if (verbose)
			std::cout << "page:\n" << page << std::endl;

		parse_latest_page(page);
		page.clear();
	}

	return 0;
}
