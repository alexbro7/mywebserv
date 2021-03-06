#include "server.hpp"

bool exit_status = false;

// class Config;

void launch_browser(int port)
{
	std::string test, o;
	std::cout << std::endl;
	std::cout << BLUE << "[⊛] => " << WHITE << "Want to open page on browser on first port? (y/n)";
	while (1)
	{
		std::cin >> test;
		if (test == "y")
		{
			// std::cout << "Opening page on port " << port << std::endl;
			if (MAC == 1)
				o = "open http://localhost:"; // --> mac
			else
				o = "xdg-open http://localhost:"; // --> linux
			o += intToStr(port);
			system(o.c_str());
			break;
		}
		if (test == "n")
		{
			break;
		}
	}
	system("clear");
	std::cout << RED << "   _      __    __   ____            \n  | | /| / /__ / /  / __/__ _____  __\n  | |/ |/ / -_) _ \\_\\ \\/ -_) __/ |/ /\n  |__/|__/\\__/_.__/___/\\__/_/  |___/ \n " << BLUE << "\n⎯⎯  jcluzet  ⎯  alebross ⎯  amanchon  ⎯⎯\n\n"
			  << RESET;
}

sockaddr_in ListenSocketAssign(int port, int *listen_sock, std::string ip)
{
	struct sockaddr_in address;

	if ((*listen_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		perror("In socket");
		std::cout << std::endl
				  << WHITE << "[" << getHour() << "] QUIT Web" << RED << "Serv" << RESET << std::endl;
		exit(EXIT_FAILURE);
	}
	fcntl(*listen_sock, F_SETFL, O_NONBLOCK);
	int reuse = 1;
	if (setsockopt(*listen_sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) != 0)
	{
		perror("set sockopt");
		exit(EXIT_FAILURE);
	}

	if (ip == "0.0.0.0")
		address.sin_addr.s_addr = INADDR_ANY;
	else if (ip == "127.0.0.1")
		address.sin_addr.s_addr = INADDR_LOOPBACK;
	else
		address.sin_addr.s_addr = inet_addr(ip.c_str()); // fonctionne aussi avec "0.0.0.0" et "127.0.0.1"	
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_family = AF_INET;
	address.sin_port = htons(port);

	memset(address.sin_zero, '\0', sizeof address.sin_zero);

	if (bind(*listen_sock, (struct sockaddr *)&address, sizeof(address)) < 0)
	{
			// std::cout << RED << "[⊛] => " << WHITE << "PORT " << port << " Already in use." << RESET << std::endl;
		std::cout << WHITE << "[" << getHour() << "] QUIT Web" << RED << "Serv" << WHITE << " : " << RESET << "Port already in use" << std::endl;
		exit(EXIT_FAILURE);
		// while (bind(*listen_sock, (struct sockaddr *)&address, sizeof(address)) < 0)
		// {
		// 	port++;
		// 	address.sin_port = htons(port);
		// }
		// std::cout << GREEN << "[⊛] => " << WHITE << "We have change the port number to " << GREEN << port << RESET << std::endl;
	}
	if (listen(*listen_sock, 10) < 0)
	{
		perror("In listen");
		std::cout << std::endl
				  << WHITE << "[" << getHour() << "] QUIT Web" << RED << "Serv" << RESET << std::endl;

		exit(EXIT_FAILURE);
	}
	return (address);
}

int build_fd_set(int *listen_sock, Config *conf, fd_set *read_fds, fd_set *write_fds, fd_set *except_fds)
{
	size_t i;
	size_t j;
	int high_sock = -1;
	(void)except_fds;

	FD_ZERO(read_fds);
	for (j = 0; j < conf->server.size(); ++j)
	{
		if (listen_sock[j] != -1)
			FD_SET(listen_sock[j], read_fds);
		for (i = 0; i < conf->server[j].client.size(); ++i)
		{
			if (conf->server[j].client[i].socket != -1)
				FD_SET(conf->server[j].client[i].socket, read_fds);
			if (conf->server[j].client[i].pipe_cgi_out[0] != -1)
				FD_SET(conf->server[j].client[i].pipe_cgi_out[0], read_fds);
			if (conf->server[j].client[i].fd_file != -1)
				FD_SET(conf->server[j].client[i].fd_file, read_fds);
		}
	}
	FD_ZERO(write_fds);
	for (j = 0; j < conf->server.size(); ++j)
	{
		for (i = 0; i < conf->server[j].client.size(); ++i)
		{
			if (conf->server[j].client[i].socket != -1)
				FD_SET(conf->server[j].client[i].socket, write_fds);
			if (conf->server[j].client[i].pipe_cgi_in[1] != -1)
				FD_SET(conf->server[j].client[i].pipe_cgi_in[1], write_fds);
		}
	}

	for (size_t i = 0; i < conf->server.size(); ++i)
	{
		if (listen_sock[i] > high_sock)
			high_sock = listen_sock[i];
	}
	for (size_t j = 0; j < conf->server.size(); j++)
	{
		for (size_t i = 0; i < conf->server[j].client.size(); i++)
		{
			if (high_sock < conf->server[j].client[i].socket)
				high_sock = conf->server[j].client[i].socket;
			if (high_sock < conf->server[j].client[i].pipe_cgi_in[1])
				high_sock = conf->server[j].client[i].pipe_cgi_in[1];
			if (high_sock < conf->server[j].client[i].pipe_cgi_out[0])
				high_sock = conf->server[j].client[i].pipe_cgi_out[0];
			if (high_sock < conf->server[j].client[i].fd_file)
				high_sock = conf->server[j].client[i].fd_file;
		}
	}
	return (high_sock);
}

void WriteResponse(Config *conf, Client *client, size_t j, size_t i)
{
	int valwrite;

	if (client->response->writing == false)
	{
		client->response->makeResponse();
		// std::cout << client->request->get_header("Accept") << ">>> " << client->response->c_type() << "<<<" << "?" << std::endl;
		if (client->request->get_header("Accept").find(client->response->c_type()) == std::string::npos && (client->response->getstat() == 0 || client->response->getstat() == 200) // error 406 not acceptable
		 && client->request->get_header("Accept") != "" && client->request->get_header("Accept").find("*/*") == std::string::npos)
		{
			// std::cout << client->request->get_header("Accept") << "?" << std::endl;
			client->response->clear();
			client->response->setStatus(406);
			client->fd_file = client->response->openFile();
			return;
		}
		client->response->transfer = client->response->get_response();
		client->response->writing = true;
	}
	if (client->response->transfer.length() < BUFFER_SIZE * 10)
	{
		valwrite = write(client->socket, client->response->transfer.c_str(), client->response->transfer.length());
		client->response->writing = false;
		// std::cout << client->response->transfer << std::endl;
	}
	else
	{
		valwrite = write(client->socket, client->response->transfer.c_str(), BUFFER_SIZE * 10);
		client->response->transfer = client->response->transfer.erase(0, BUFFER_SIZE * 10);
	}
	if (valwrite < 0)
	{
		if(CONNEXION_LOG == 1)
			std::cout << RED << "[⊛ DISCONNECT] => " << RESET << inet_ntoa(client->sockaddr.sin_addr) << WHITE << ":" << RESET << ntohs(client->sockaddr.sin_port) << RED << "    ⊛ " << WHITE << "PORT: " << RED << conf->server[j].port << RESET << std::endl;
		close(client->socket);
		conf->server[j].client.erase(conf->server[j].client.begin() + i);
		i--;
		return;
	}
	else if (valwrite == 0)
	{
	} // ???
	// std::cout << valwrite << " " << client->response->get_response().length();
	if (client->response->writing == false && (client->response->getstat() == 400 || client->response->getstat() == 500 || client->request->get_header("Connection") == "close"))
	{
		if(CONNEXION_LOG == 1)
			std::cout << RED << "[⊛ DISCONNECT] => " << RESET << inet_ntoa(client->sockaddr.sin_addr) << WHITE << ":" << RESET << ntohs(client->sockaddr.sin_port) << RED << "    ⊛ " << WHITE << "PORT: " << RED << conf->server[j].port << RESET << std::endl;
		close(client->socket);
		conf->server[j].client.erase(conf->server[j].client.begin() + i);
		i--;
	}
	else if (client->response->writing == false)
	{
		if(LOG == 1)
			output_log(client->response->getstat(), client->response->get_fpath());
		if (conf->get_debug() == true)
			output_debug(client->request->get_request(), client->response->get_response()); //PAS REFAIRE GETHEADER
		client->response->clear();
		client->request->clear();
	}
	return;
}

void ReadFile(Client *client)
{
	int valread;
	char data[BUFFER_SIZE + 1];

	if ((valread = read(client->fd_file, data, BUFFER_SIZE)) < 0)
	{
		client->fd_file = -1;
		client->response->setStatus(500);
		// std::cout << "!!!!!!!!!!!!!!!!1" << std::endl;
		client->fd_file = client->response->openFile();
		client->response->transfer = "";
	}
	else if (valread == 0)
	{
		// std::cout << "HERE" << client->fd_file << std::endl;
		close(client->fd_file);
		client->fd_file = -1;
	}
	else
	{
		data[valread] = '\0';
		client->response->transfer += std::string(data, valread);
	}
	return;
}

void ReadCGI(Client *client)
{
	int valread;
	char data[BUFFER_SIZE + 1];

	if ((valread = read(client->pipe_cgi_out[0], data, BUFFER_SIZE)) < 0)
	{
		client->response->setStatus(500);
		// std::cout << "!!!!!!!!!!!!!!!!2" << std::endl;
		client->fd_file = client->response->openFile();
		close(client->pipe_cgi_out[0]);
		client->pipe_cgi_out[1] = -1;
		client->pipe_cgi_out[0] = -1;
		client->response->transfer = "";
	}
	else if (valread == 0)
	{
		close(client->pipe_cgi_out[0]);
		client->pipe_cgi_out[1] = -1;
		client->pipe_cgi_out[0] = -1;
		client->response->setStatus(200);
		if (client->response->transfer.length() > 0)
        	std::cout << GREEN << "[⊛ CGI]        => " << WHITE << client->response->transfer.substr(0, 20) + "....." << RESET << std::endl;
		else
        	std::cout << GREEN << "[⊛ CGI]        => " << RED << "NOT VALID CGI-BIN" << RESET << std::endl;

	}
	else
	{
		data[valread] = '\0';
		client->response->transfer += std::string(data, valread);
	}
	return;
}

void WriteCGI(Client *client)
{
	int valwrite;

	valwrite = write(client->pipe_cgi_in[1], client->request->get_body().c_str(), client->request->get_body().length());
	if (valwrite < 0)
	{
		client->response->setStatus(500);
		// std::cout << "!!!!!!!!!!!!!!!!3" << std::endl;
		client->fd_file = client->response->openFile();
		close(client->pipe_cgi_in[1]);
		client->pipe_cgi_in[1] = -1;
		client->pipe_cgi_in[0] = -1;
		close(client->pipe_cgi_out[0]);
		client->pipe_cgi_out[1] = -1;
		client->pipe_cgi_out[0] = -1;
		client->response->transfer = "";
	}
	else if (valwrite == 0 || valwrite == static_cast<int>(client->request->get_body().length()))
	{
		close(client->pipe_cgi_in[1]);
		client->pipe_cgi_in[1] = -1;
		client->pipe_cgi_in[0] = -1;
	}
	return;
}

void ReadRequest(Config *conf, Client *client, size_t j, size_t i)
{
	int valread;
	char data[BUFFER_SIZE + 1];

	if ((valread = read(client->socket, data, BUFFER_SIZE)) < 0)
	{
		if(CONNEXION_LOG == 1)
			std::cout << RED << "[⊛ DISCONNECT] => " << RESET << inet_ntoa(client->sockaddr.sin_addr) << WHITE << ":" << RESET << ntohs(client->sockaddr.sin_port) << RED << "    ⊛ " << WHITE << "PORT: " << RED << conf->server[j].port << RESET << std::endl;
		close(client->socket);
		conf->server[j].client.erase(conf->server[j].client.begin() + i);
		i--;
		return;
	}
	else if (valread == 0)
	{
		if(CONNEXION_LOG == 1)
			std::cout << RED << "[⊛ DISCONNECT] => " << RESET << inet_ntoa(client->sockaddr.sin_addr) << WHITE << ":" << RESET << ntohs(client->sockaddr.sin_port) << RED << "    ⊛ " << WHITE << "PORT: " << RED << conf->server[j].port << RESET << std::endl;
		close(client->socket);
		conf->server[j].client.erase(conf->server[j].client.begin() + i);
		i--;
		return;
	}
	else
	{
		data[valread] = '\0';
		client->request->add(data);

		if (client->request->ready())
		{
			Server*	conf_local;
			std::string	location;

			//std::cout << conf->server[j].ip << " " << conf->server[j].port << std::endl;
			location = apply_location(client->request->get_path(), &conf->server[j], &conf_local);
			client->response->setConf(conf_local);
			if (conf->server[j].root != conf_local->root)
			{
				client->request->set_path(client->request->get_path().erase(1, location.length()));
				if (client->request->get_path().compare(0, 2, "//"))
					client->request->set_path(client->request->get_path().erase(1, 1));	
			}
			if ((client->request->get_method() == "POST" && !conf_local->methods[1]) || (client->request->get_method() == "GET" && !conf_local->methods[0]) || (client->request->get_method() == "DELETE" && !conf_local->methods[2])) // check error 405 Method not allowed
			{
				client->response->setStatus(405);
				client->fd_file = client->response->openFile();
				return;
			}
			if (client->request->get_header("Host") != conf->server[j].ip + ":" + conf->server[j].port)
			{
				for (size_t i = 0; i < conf_local->server_name.size(); i++)
				{
					if (conf_local->server_name[i] == client->request->get_header("Host")
						|| conf_local->server_name[i] + ":" + conf->server[j].port == client->request->get_header("Host"))
						break ;
					else if (i + 1 == conf_local->server_name.size())
					{
						if (conf->server[j].ip == "0.0.0.0" && client->request->get_header("Host").find(":") != std::string::npos
							&& client->request->get_header("Host").find(":") == client->request->get_header("Host").find_last_of(":")
							&& client->request->get_header("Host").substr(client->request->get_header("Host").find(":") + 1, client->request->get_header("Host").length()) == conf->server[j].port)
							break ;
						client->response->setStatus(400);				
						client->fd_file = client->response->openFile();
						return ;
					}
				}
			}
			if (is_cgi(client->request, conf_local) == true)
			{
        		if(ft_atoi(conf_local->client_max_body_size.c_str()) < client->request->get_body().length()) // check max body size error 413
				{
					client->response->setStatus(413);
					client->fd_file = client->response->openFile();
					return;
				}
				if (client->request->get_method() == "POST" && client->request->get_header("Content-Type").find(";") != std::string::npos && client->request->get_header("Content-Type").substr(0, client->request->get_header("Content-Type").find(";")) == "multipart/form-data")
				{
					client->response->setStatus(201);
				}
				treat_cgi(conf_local, client);
			}
			else
				client->fd_file = client->response->treatRequest();	
		}
	}


	return;
}

void NewClients(int *listen_sock, Config *conf, fd_set *read_fds)
{
	int new_socket;
	struct sockaddr_in address;
	int addrlen = sizeof(address);

	for (size_t j = 0; j < conf->server.size(); j++)
	{
		if (conf->server[j].client.size() < CO_MAX && FD_ISSET(listen_sock[j], read_fds))
		{
			if ((new_socket = accept(listen_sock[j], (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0)
			{
				// pas grave (?)
				return;
			}
			fcntl(new_socket, F_SETFL, O_NONBLOCK);
			conf->server[j].client.push_back(Client(new_socket, address));
			if(CONNEXION_LOG == 1)
				std::cout << GREEN << "[⊛ CONNECT]    => " << RESET << inet_ntoa(address.sin_addr) << WHITE << ":" << RESET << ntohs(address.sin_port) << RED << "    ⊛ " << WHITE << "PORT: " << GREEN << conf->server[j].port << RESET << std::endl;
		}
	}
	return;
}

int run_server(Config conf)
{
	Client *client;
	int high_sock;
	fd_set read_fds;
	fd_set write_fds;
	int listen_sock[conf.server.size()];
	for (size_t i = 0; i < conf.server.size(); i++)
		ListenSocketAssign(atoi(conf.server[i].port.c_str()), &listen_sock[i], conf.server[i].ip);

	while (1)
	{
		high_sock = build_fd_set(&listen_sock[0], &conf, &read_fds, &write_fds, NULL);

		int activity = select(high_sock + 1, &read_fds, &write_fds, NULL, NULL);
		if (exit_status == true) // ??
			return (0);
		if (activity < 0)
		{
			perror("select error");
			std::cout << std::endl
					  << WHITE << "[" << getHour() << "] QUIT Web" << RED << "Serv" << RESET << std::endl;
			exit(-1);
		}

		NewClients(&listen_sock[0], &conf, &read_fds);

		for (size_t j = 0; j < conf.server.size(); j++)
		{
			for (size_t i = 0; i < conf.server[j].client.size(); i++)
			{
				client = &conf.server[j].client[i];
				if (client->request->ready() == false && FD_ISSET(client->socket, &read_fds)) // read client
				{
					ReadRequest(&conf, client, j, i);
				}
				else if (client->request->ready() == true && client->request->get_method() == "POST" && client->pipe_cgi_in[1] != -1 && FD_ISSET(client->pipe_cgi_in[1], &write_fds)) // write cgi
				{
					WriteCGI(client);

				}
				else if (client->request->ready() == true && is_cgi(client->request, client->response->get_conf()) && client->pipe_cgi_in[1] == -1 && client->pipe_cgi_out[0] != -1 && FD_ISSET(client->pipe_cgi_out[0], &read_fds)) // read cgi
				{
					ReadCGI(client);
				}
				else if (client->request->ready() == true // && is_cgi(client->request) == false //&& client->request->get_method() == "GET"
						 && client->fd_file != -1 && FD_ISSET(client->fd_file, &read_fds))
				{
					ReadFile(client);

				}
				else if (client->request->ready() == true && client->pipe_cgi_out[0] == -1 && client->fd_file == -1 && FD_ISSET(client->socket, &write_fds)) // write client
				{
					WriteResponse(&conf, client, j, i);
				}
			}
		}
	}
	return 0;
}

void quit_sig(int sig)
{
	if (SIGINT == sig)
		exit_status = true;
	std::cout << std::endl
			  << WHITE << "[" << getHour() << "] QUIT Web" << RED << "Serv" << RESET << std::endl;
}