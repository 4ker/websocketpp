/*
 * Copyright (c) 2011, Peter Thorson. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the WebSocket++ Project nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" 
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 * ARE DISCLAIMED. IN NO EVENT SHALL PETER THORSON BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 */

#ifndef WEBSOCKETPP_ROLE_SERVER_HPP
#define WEBSOCKETPP_ROLE_SERVER_HPP

#include "../processors/hybi.hpp"
#include "../processors/hybi_legacy.hpp"
#include "../rng/blank_rng.hpp"

#include <boost/algorithm/string.hpp>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/scoped_ptr.hpp>

#include <iostream>
#include <stdexcept>

namespace websocketpp {

// Forward declarations
template <typename T> struct endpoint_traits;
    
namespace role {

template <class endpoint>
class server {
public:
	// Connection specific details
	template <typename connection_type>
	class connection {
	public:
		typedef connection<connection_type> type;
		typedef endpoint endpoint_type;
		
		// Valid always
        int get_version() const {
			return m_version;
		}
		std::string get_request_header(const std::string& key) const {
			return m_request.header(key);
		}
		std::string get_origin() const {
			return m_origin;
		}
		
		// Information about the requested URI
		// valid only after URIs are loaded
		// TODO: check m_uri for NULLness
		bool get_secure() const {
			return m_uri->get_secure();
		}
		std::string get_host() const {
			return m_uri->get_host();
		}
		std::string get_resource() const {
			return m_uri->get_resource();
		}
		uint16_t get_port() const {
			return m_uri->get_port();
		}
		
		// Valid for CONNECTING state
		void add_response_header(const std::string& key, const std::string& value) {
			m_response.add_header(key,value);
		}
		void replace_response_header(const std::string& key, const std::string& value) {
			m_response.replace_header(key,value);
		}
		void remove_response_header(const std::string& key) {
			m_response.remove_header(key);
		}
		
		const std::vector<std::string>& get_subprotocols() const {
			return m_requested_subprotocols;
		}
		const std::vector<std::string>& get_extensions() const {
			return m_requested_extensions;
		}
		void select_subprotocol(const std::string& value);
		void select_extension(const std::string& value);
		
		// Valid if get_version() returns -1 (ie this is an http connection)
		void set_body(const std::string& value);
		
		int32_t rand() {
            return 0;
        }
		
		// should this exist?
		boost::asio::io_service& get_io_service() {
			return m_endpoint.get_io_service();
		}
	protected:
		connection(endpoint& e)
        : m_endpoint(e),
          m_connection(static_cast< connection_type& >(*this)),
          m_version(-1),
          m_uri() {}
		
		// initializes the websocket connection
		void async_init();
		void handle_read_request(const boost::system::error_code& error,
								 std::size_t bytes_transferred);
		void write_response();
		void handle_write_response(const boost::system::error_code& error);
		
		void log_open_result();
	private:
		endpoint&					m_endpoint;
		connection_type&			m_connection;
		
		int							m_version;
		uri_ptr						m_uri;
		std::string					m_origin;
		std::vector<std::string>	m_requested_subprotocols;
		std::vector<std::string>	m_requested_extensions;
		std::string					m_subprotocol;
		std::vector<std::string>	m_extensions;
		
		http::parser::request		m_request;
		http::parser::response		m_response;
		blank_rng					m_rng;
	};
	
	// types
	typedef server<endpoint> type;
	typedef endpoint endpoint_type;
	
	typedef typename endpoint_traits<endpoint>::connection_ptr connection_ptr;
	typedef typename endpoint_traits<endpoint>::handler_ptr handler_ptr;
    
	// handler interface callback base class
    class handler_interface {
	public:
		virtual void validate(connection_ptr connection) {};
		virtual void on_open(connection_ptr connection) {};
		virtual void on_close(connection_ptr connection) {};
		virtual void on_fail(connection_ptr connection) {}
		
		virtual void on_message(connection_ptr connection,message::data_ptr) {};
		
		virtual bool on_ping(connection_ptr connection,std::string) {return true;}
		virtual void on_pong(connection_ptr connection,std::string) {}
		virtual void http(connection_ptr connection) {}
	};
	
	server(boost::asio::io_service& m) 
	 : m_ws_endpoint(static_cast< endpoint_type& >(*this)),
	   m_io_service(m),
	   m_endpoint(),
	   m_acceptor(m),
       m_timer(m,boost::posix_time::seconds(0)) {}
	
    void listen(uint16_t port);
protected:
	bool is_server() {
		return true;
	}
private:
	// start_accept creates a new connection and begins an async_accept on it
	void start_accept();
	
	// handle_accept will begin the connection's read/write loop and then reset
	// the server to accept a new connection. Errors returned by async_accept
	// are logged and ingored.
	void handle_accept(connection_ptr con, 
                       const boost::system::error_code& error);
	
	endpoint_type&					m_ws_endpoint;
	boost::asio::io_service&		m_io_service;
	boost::asio::ip::tcp::endpoint	m_endpoint;
	boost::asio::ip::tcp::acceptor	m_acceptor;
    
    boost::asio::deadline_timer     m_timer;
};

// server<endpoint> Implimentation
template <class endpoint>
void server<endpoint>::listen(uint16_t port) {
    m_endpoint.port(port);
    m_acceptor.open(m_endpoint.protocol());
    m_acceptor.set_option(boost::asio::socket_base::reuse_address(true));
    m_acceptor.bind(m_endpoint);
    m_acceptor.listen(200000);
    
    this->start_accept();
    
    m_ws_endpoint.alog().at(log::alevel::DEVEL) << "role::server listening on port " << port << log::endl;
    
    m_io_service.run();
}

template <class endpoint>
void server<endpoint>::start_accept() {
    connection_ptr con = m_ws_endpoint.create_connection();
    
    m_acceptor.async_accept(
        con->get_raw_socket(),
        boost::bind(
            &type::handle_accept,
            this,
            con,
            boost::asio::placeholders::error
        )
    );
}

// handle_accept will begin the connection's read/write loop and then reset
// the server to accept a new connection. Errors returned by async_accept
// are logged and ingored.
template <class endpoint>
void server<endpoint>::handle_accept(connection_ptr con, 
                                     const boost::system::error_code& error)
{
    if (error) {
		if (error == boost::system::errc::too_many_files_open) {
			m_ws_endpoint.elog().at(log::elevel::ERROR) 
				<< "async_accept returned error: " << error 
				<< " (too many files open)" << log::endl;
            m_timer.expires_from_now(boost::posix_time::milliseconds(1000));
            m_timer.async_wait(boost::bind(&type::start_accept,this));
            return;
		} else {
			m_ws_endpoint.elog().at(log::elevel::ERROR) 
				<< "async_accept returned error: " << error 
				<< " (unknown)" << log::endl;
		}
    } else {
        con->start();
    }
    
    this->start_accept();
}
    
// server<endpoint>::connection<connnection_type> Implimentation

template <class endpoint>
template <class connection_type>
void server<endpoint>::connection<connection_type>::select_subprotocol(
                                                    const std::string& value)
{
    std::vector<std::string>::iterator it;
    
    it = std::find(m_requested_subprotocols.begin(),
                   m_requested_subprotocols.end(),
                   value);
    
    if (value != "" && it == m_requested_subprotocols.end()) {
        throw std::invalid_argument("Attempted to choose a subprotocol not proposed by the client");
    }
    
    m_subprotocol = value;
}

template <class endpoint>
template <class connection_type>
void server<endpoint>::connection<connection_type>::select_extension(
                                                    const std::string& value)
{
    if (value == "") {
        return;
    }
    
    std::vector<std::string>::iterator it;
    
    it = std::find(m_requested_extensions.begin(),
                   m_requested_extensions.end(),
                   value);
    
    if (it == m_requested_extensions.end()) {
        throw std::invalid_argument("Attempted to choose an extension not proposed by the client");
    }
    
    m_extensions.push_back(value);
}

// Valid if get_version() returns -1 (ie this is an http connection)
template <class endpoint>
template <class connection_type>
void server<endpoint>::connection<connection_type>::set_body(
                                                    const std::string& value)
{
    if (m_connection.m_version != -1) {
        // TODO: throw exception
        throw std::invalid_argument("set_body called from invalid state");
    }
    
    m_response.set_body(value);
}
    
    
template <class endpoint>
template <class connection_type>
void server<endpoint>::connection<connection_type>::async_init() {
    boost::asio::async_read_until(
        m_connection.get_socket(),
        m_connection.buffer(),
        "\r\n\r\n",
        boost::bind(
            &type::handle_read_request,
            m_connection.shared_from_this(), // shared from this?
            boost::asio::placeholders::error,
            boost::asio::placeholders::bytes_transferred
        )
    );
}

template <class endpoint>
template <class connection_type>
void server<endpoint>::connection<connection_type>::handle_read_request(
    const boost::system::error_code& error, std::size_t bytes_transferred)
{
    if (error) {
        // log error
        m_endpoint.elog().at(log::elevel::ERROR) << "Error reading HTTP request. code: " << error << log::endl;
        m_connection.terminate(false);
        return;
    }
    
    try {
        std::istream request(&m_connection.buffer());
        
        if (!m_request.parse_complete(request)) {
            // not a valid HTTP request/response
            throw http::exception("Recieved invalid HTTP Request",http::status_code::BAD_REQUEST);
        }
        
        m_endpoint.alog().at(log::alevel::DEBUG_HANDSHAKE) << m_request.raw() << log::endl;
        
        std::string h = m_request.header("Upgrade");
        if (boost::ifind_first(h,"websocket")) {
            h = m_request.header("Sec-WebSocket-Version");
            if (h == "") {
                // websocket upgrade is present but version is not.
                // assume hybi00
                m_version = 0;
            } else {
                m_version = atoi(h.c_str());
                if (m_version == 0) {
                    throw(http::exception("Unable to determine connection version",http::status_code::BAD_REQUEST));
                }
            }
            
            // create a websocket processor
            if (m_version == 0) {
                //m_response.add_header("Sec-WebSocket-Version","13, 8, 7");
                
                char foo[9];
                foo[8] = 0;
                
                request.get(foo,9);
                
                if (request.gcount() != 8) {
                    throw http::exception("Missing Key3",http::status_code::BAD_REQUEST);
                }
                m_request.add_header("Sec-WebSocket-Key3",std::string(foo));
                
                //throw(http::exception("Unsupported WebSocket version",http::status_code::BAD_REQUEST));
                m_connection.m_processor = processor::ptr(new processor::hybi_legacy<connection_type>(m_connection));
            } else if (m_version == 7 ||
                       m_version == 8 ||
                       m_version == 13) {
                m_connection.m_processor = processor::ptr(new processor::hybi<connection_type>(m_connection));
            } else {
                m_response.add_header("Sec-WebSocket-Version","13, 8, 7");
                
                throw(http::exception("Unsupported WebSocket version",http::status_code::BAD_REQUEST));
            }
            
            m_connection.m_processor->validate_handshake(m_request);
            m_origin = m_connection.m_processor->get_origin(m_request);
            m_uri = m_connection.m_processor->get_uri(m_request);
            
            m_endpoint.get_handler()->validate(m_connection.shared_from_this());
            
            m_response.set_status(http::status_code::SWITCHING_PROTOCOLS);
        } else {
            // continue as HTTP?
            m_endpoint.get_handler()->http(m_connection.shared_from_this());
            
            // should there be a more encapsulated http processor here?
            m_origin = m_request.header("Origin");
            
            // Set URI
            std::string h = m_request.header("Host");
            
            size_t found = h.find(":");
            if (found == std::string::npos) {
                // TODO: this makes the assumption that WS and HTTP
                // default ports are the same.
                m_uri.reset(new uri(m_endpoint.is_secure(),h,m_request.uri()));
            } else {
                m_uri.reset(new uri(m_endpoint.is_secure(),
                                    h.substr(0,found),
                                    h.substr(found+1),
                                    m_request.uri()));
            }
            
            m_response.set_status(http::status_code::OK);
        }
    } catch (const http::exception& e) {
        m_endpoint.elog().at(log::elevel::ERROR) << e.what() << log::endl;
        m_response.set_status(e.m_error_code,e.m_error_msg);
        m_response.set_body(e.m_body);
    } catch (const uri_exception& e) {
        // there was some error building the uri
        m_endpoint.elog().at(log::elevel::ERROR) << e.what() << log::endl;
        m_response.set_status(http::status_code::BAD_REQUEST);
    }
    
    write_response();
}

template <class endpoint>
template <class connection_type>
void server<endpoint>::connection<connection_type>::write_response() {
    m_response.set_version("HTTP/1.1");
    
    if (m_response.get_status_code() == http::status_code::SWITCHING_PROTOCOLS) {
        // websocket response
        m_connection.m_processor->handshake_response(m_request,m_response);
        
        if (m_subprotocol != "") {
            m_response.replace_header("Sec-WebSocket-Protocol",m_subprotocol);
        }
        
        // TODO: return negotiated extensions
    } else {
        // TODO: HTTP response
    }
    
    m_response.replace_header("Server","WebSocket++/2011-11-18");
    
    std::string raw = m_response.raw();
    
    // Hack for legacy HyBi
    if (m_version == 0) {
        raw += boost::dynamic_pointer_cast<processor::hybi_legacy<connection_type> >(m_connection.m_processor)->get_key3();
    }
    
    m_endpoint.alog().at(log::alevel::DEBUG_HANDSHAKE) << raw << log::endl;
    
    boost::asio::async_write(
        m_connection.get_socket(),
        boost::asio::buffer(raw),
        boost::bind(
            &type::handle_write_response,
            m_connection.shared_from_this(),
            boost::asio::placeholders::error
        )
    );
}

template <class endpoint>
template <class connection_type>
void server<endpoint>::connection<connection_type>::handle_write_response(
    const boost::system::error_code& error)
{
    // TODO: handshake timer
    
    if (error) {
        m_endpoint.elog().at(log::elevel::ERROR) << "Network error writing handshake respons. code: " << error << log::endl;
        
        m_connection.terminate(false);
        return;
    }
    
    log_open_result();
    
    if (m_response.get_status_code() != http::status_code::SWITCHING_PROTOCOLS) {
        if (m_version == -1) {
            // if this was not a websocket connection, we have written 
            // the expected response and the connection can be closed.
        } else {
            // this was a websocket connection that ended in an error
            m_endpoint.elog().at(log::elevel::ERROR) 
            << "Handshake ended with HTTP error: " 
            << m_response.get_status_code() << " " 
            << m_response.get_status_msg() << log::endl;
        }
        m_connection.terminate(true);
        return;
    }
    
    m_connection.m_state = session::state::OPEN;
    
    m_endpoint.get_handler()->on_open(m_connection.shared_from_this());
    
    m_connection.handle_read_frame(boost::system::error_code());
}

template <class endpoint>
template <class connection_type>
void server<endpoint>::connection<connection_type>::log_open_result() {
    std::stringstream version;
    version << "v" << m_version << " ";
    
	std::string remote;
	boost::system::error_code ec;
	boost::asio::ip::tcp::endpoint ep = m_connection.get_raw_socket().remote_endpoint(ec);
	if (ec) {
		// An error occurred.
		//remote = "Unknown";
		//ignore?
		m_endpoint.elog().at(log::elevel::WARN) << "Error getting remote endpoint. code: " << ec << log::endl;
	} else {
		
	}
	
	
    m_endpoint.alog().at(log::alevel::CONNECT) << (m_version == -1 ? "HTTP" : "WebSocket") << " Connection "
    << ep << " "
    << (m_version == -1 ? "" : version.str())
    << (get_request_header("User-Agent") == "" ? "NULL" : get_request_header("User-Agent")) 
    << " " << m_uri->get_resource() << " " << m_response.get_status_code() 
    << log::endl;
}
    
} // namespace role	
} // namespace websocketpp

#endif // WEBSOCKETPP_ROLE_SERVER_HPP