#include "page_capture.h"
#include <boost/bind.hpp>
#include <boost/make_shared.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

namespace ba = boost::asio;
namespace bp = boost::posix_time;

page_capture::page_capture(const std::string& host, const std::string& path, 
	unsigned int timeout_sec) : 
	m_host(host),
	m_path(path),
	m_timeout_sec(timeout_sec),
	m_ret(false),
	m_timer(m_ios, bp::seconds(timeout_sec))
{
}

bool page_capture::req_page() {
	ba::ip::tcp::resolver res(m_ios);
	ba::ip::tcp::resolver::query qry(m_host, "80");

	res.async_resolve(qry, boost::bind(&page_capture::resoved, this, _1, _2));
	m_timer.async_wait(boost::bind(&page_capture::stop, this, _1));

	m_ios.run();
	return m_ret;
}

void page_capture::resoved(const boost::system::error_code& e, 
	ba::ip::tcp::resolver::iterator it)
{
	if (e)
		return ;

	m_timer.expires_from_now(bp::seconds(m_timeout_sec));
	m_timer.async_wait(boost::bind(&page_capture::stop, this, _1));

	sock_ptr sock = boost::make_shared<ba::ip::tcp::socket>(boost::ref(m_ios));
	ba::async_connect(*sock, it, boost::bind(&page_capture::connected, this, _1, _2, sock));

	ba::deadline_timer t(m_ios, bp::seconds(m_timeout_sec));
}

void page_capture::stop(const boost::system::error_code& e) {
	if (!e) {
		std::cout << "timeout: " << m_host << "/" << m_path << std::endl;
		m_ios.stop();
	}
}

void page_capture::connected(const boost::system::error_code& e, 
	ba::ip::tcp::resolver::iterator it, sock_ptr sock)
{
	if (e)
		return ;

	m_timer.expires_from_now(bp::milliseconds(FAST_PAGE_TIMEOUT_MS));
	m_timer.async_wait(boost::bind(&page_capture::stop, this, _1));

	std::string uri("GET /");
	uri += m_path;
	uri += " HTTP/1.1\r\n";
	uri += "Host: ";
	uri += m_host;
	uri += "\r\n";
	uri += "User-Agent: Chome\r\n\r\n";

	ba::async_write(*sock, ba::buffer(uri), boost::bind(&page_capture::writed, this, _1, _2, sock));
}

void page_capture::writed(const boost::system::error_code& e, size_t len, sock_ptr sock) {
	if (e)
		return ;

	m_timer.expires_from_now(bp::milliseconds(FAST_PAGE_TIMEOUT_MS));
	m_timer.async_wait(boost::bind(&page_capture::stop, this, _1));

	boost::shared_ptr<std::string> buf(boost::make_shared<std::string>());
	buf->resize(64 * 1024);
	sock->async_read_some(ba::buffer(&(*buf)[0], buf->size()), boost::bind(&page_capture::readed_some, this, _1, _2, buf, sock));
}

void page_capture::readed_some(const boost::system::error_code& e, size_t len,
	boost::shared_ptr<std::string> buf, sock_ptr sock)
{
	if (e) {
		m_timer.cancel();
		return ;
	}

	m_timer.expires_from_now(bp::milliseconds(FAST_PAGE_TIMEOUT_MS));
	m_timer.async_wait(boost::bind(&page_capture::stop, this, _1));

	m_page.append(*buf, 0, len);
	sock->async_read_some(ba::buffer(&(*buf)[0], buf->size()), boost::bind(&page_capture::readed_some, this, _1, _2, buf, sock));
}

void page_capture::get_page(std::string& page) const {
	page = m_page;
}
