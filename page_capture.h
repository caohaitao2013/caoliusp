/*
* page_capture.cpp - capture a page
*
* Licensed under GPLv2 orlater
*
* Author:	KaiWen<wenkai1987@gmail.com>
* Date:		Mon Dec 16 15:55:09 CST 2013
*/
#ifndef PAGE_CAPTURE_H
#define PAGE_CAPTURE_H

#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>

class page_capture {
	typedef boost::shared_ptr<boost::asio::ip::tcp::socket> sock_ptr;
	typedef boost::shared_ptr<boost::asio::deadline_timer> timer_ptr;
public:
	page_capture(const std::string& host, const std::string& path, 
		unsigned int con_timeout_sec, unsigned int rw_timeout_ms);

	bool req_page();
	void get_page(std::string& page) const;
private:
	void resoved(const boost::system::error_code& e, 
		boost::asio::ip::tcp::resolver::iterator it);
	void stop(const boost::system::error_code& e);
	void connected(const boost::system::error_code& e, 
		boost::asio::ip::tcp::resolver::iterator it, sock_ptr sock);
	void writed(const boost::system::error_code& e, size_t len, sock_ptr sock);
	void readed_some(const boost::system::error_code& e, size_t len, 
		boost::shared_ptr<std::string> buf, sock_ptr sock);

	std::string m_host;
	std::string m_path;
	std::string m_page;
	unsigned int m_con_timeout_sec;
	unsigned int m_rw_timeout_ms;
	bool m_ret;
	boost::asio::io_service m_ios;
	boost::asio::deadline_timer m_timer;
};

#endif
