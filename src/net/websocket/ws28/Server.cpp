#include "Server.h"
#include "logger.hpp"
#ifndef _WIN32
#include <signal.h>
#endif

namespace ws28{

Server::Server(uv_loop_t *loop) : m_pLoop(loop)
                                , ssl_enable_(false)
{
	m_fnCheckConnection = [](Client*, HTTPRequest &req) -> bool {
		auto host = req.headers.Get("host");
		if(!host) return true; // No host header, default to accept
		
		auto origin = req.headers.Get("origin");
		if(!origin) return true;

        if (!(origin == host)) {
		    log_warnf("server check origin:%s, host:%s",
		        std::string(origin.value()).c_str(),
				std::string(host.value()).c_str());
		}

		return true;
		//return origin == host;
	};
}

Server::Server(uv_loop_t *loop,
            const std::string& key_file,
			const std::string cert_file):m_pLoop(loop)
			                        , ssl_enable_(true)
									, key_file_(key_file)
									, cert_file_(cert_file)
                            
{
	m_fnCheckConnection = [](Client*, HTTPRequest &req) -> bool {
		auto host = req.headers.Get("host");
		if(!host) return true; // No host header, default to accept
		
		auto origin = req.headers.Get("origin");
		if(!origin) return true;

        if (!(origin == host)) {
		    log_warnf("server check origin:%s, host:%s",
		        std::string(origin.value()).c_str(),
				std::string(host.value()).c_str());
		}

		return true;
		//return origin == host;
	};
}

bool Server::Listen(int port, bool ipv4Only){
	if(m_Server) return false;
	
#ifndef _WIN32
	signal(SIGPIPE, SIG_IGN);
#endif
	
	auto server = SocketHandle{new uv_tcp_t};
	uv_tcp_init_ex(m_pLoop, server.get(), ipv4Only ? AF_INET : AF_INET6);
	server->data = this;
	
	struct sockaddr_storage addr;
	
	if(ipv4Only){
		uv_ip4_addr("0.0.0.0", port, (struct sockaddr_in*) &addr);
	}else{
		uv_ip6_addr("::0", port, (struct sockaddr_in6*) &addr);
	}
	
	uv_tcp_nodelay(server.get(), (int) true);
	
	// Enable SO_REUSEPORT
#ifndef _WIN32
	uv_os_fd_t fd;
	int r = uv_fileno((uv_handle_t*) server.get(), &fd);
	(void) r;
	assert(r == 0);
	int optval = 1;
	setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));
#endif
	
	if(uv_tcp_bind(server.get(), (struct sockaddr*) &addr, 0) != 0){
		return false;
	}
	
	if(uv_listen((uv_stream_t*) server.get(), 512, [](uv_stream_t* server, int status){
		((Server*) server->data)->OnConnection(server, status);
	}) != 0){
		return false;
	}
	
	m_Server = std::move(server);
	return true;
}

void Server::StopListening(){
	// Just in case we have more logic in the future
	if(!m_Server) return;
	
	m_Server.reset();
}

void Server::DestroyClients(){
	// Clients will erase themselves from this vector
	while(!m_Clients.empty()){
		m_Clients.back()->Destroy();
	}
}

Server::~Server(){
	StopListening();
	DestroyClients();
}

void Server::OnConnection(uv_stream_t* server, int status){
	if(status < 0) return;
	
	SocketHandle socket{new uv_tcp_t};
	uv_tcp_init(m_pLoop, socket.get());
	
	socket->data = nullptr;

	if(uv_accept(server, (uv_stream_t*) socket.get()) == 0){
		Client* client = nullptr;
		if (ssl_enable_) {
			client = new Client(this, std::move(socket), key_file_, cert_file_);
		} else {
			client = new Client(this, std::move(socket));
		}
		
		m_Clients.emplace_back(client);
		
		// If for whatever reason uv_tcp_getpeername failed (happens... somehow?)
		if(client->GetIP()[0] == '\0') client->Destroy();
	}
}

std::unique_ptr<Client> Server::NotifyClientPreDestroyed(Client *client){
	for(auto it = m_Clients.begin(); it != m_Clients.end(); ++it){
		if(it->get() == client){
			std::unique_ptr<Client> r = std::move(*it);
			*it = std::move(m_Clients.back());
			m_Clients.pop_back();
			return r;
		}
	}
	
	assert(false);
	return {};
}

}
