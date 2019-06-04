/*
	Author: Arne-Tobias Rak
	TU Darmstadt
*/
#include <iostream>
#include <sstream>
#include <unistd.h>
#include "httplib.h"

std::thread* networkTraceThread;
const size_t networkTraceSampleDurMs = 250;
bool runTrace = false;
bool resetTrace = false;
std::map<std::string, std::map<size_t, size_t>> netTraces;


void printHelp()
{
	std::cout <<
		"bw [bytes/s]  - set bandwidth\n" <<
		"trace [path]  - run network trace\n" <<
		"quit          - close server\n";
	std::cout << std::endl;
}

void networkTrace(const std::map<size_t, size_t>& trace)
{
	while (runTrace)
	{
		resetTrace = false;
		for (auto& pair : trace)
		{
			httplib::bandwidth = pair.second;
			std::this_thread::sleep_for(std::chrono::milliseconds(networkTraceSampleDurMs));
			if (!runTrace || resetTrace)
				break;
		}
	}
}

void stopNetworkTrace()
{
	runTrace = false;
	if (networkTraceThread && networkTraceThread->joinable())
		networkTraceThread->join();
}

void startNetworkTrace(const std::string& path)
{
	stopNetworkTrace();

	if (netTraces.find(path) == netTraces.end())
	{
		std::ifstream traceFile(path);
		std::map<size_t, size_t> netTrace;
		int timestamp;
		while (!traceFile.eof())
		{
			traceFile >> timestamp;
			size_t add = 1500 * (1000.0 / networkTraceSampleDurMs);
			int ts = timestamp / networkTraceSampleDurMs * networkTraceSampleDurMs;
			if (netTrace.find(ts) != netTrace.end())
				netTrace[ts] += add;
			else
				netTrace[ts] = add;
		}
		netTraces[path] = netTrace;
	}

	runTrace = true;
	networkTraceThread = new std::thread(networkTrace, netTraces.at(path));
	networkTraceThread->detach();
}

void processCommand(const std::string& cmd)
{
	std::istringstream ss(cmd);
	std::string basecmd;
	ss >> basecmd;

	if (basecmd == "quit")
		exit(-1);
	else if (basecmd == "bw")
	{
		size_t bw;
		ss >> bw;
		stopNetworkTrace();
		httplib::bandwidth = bw;
	}
	else if (basecmd == "trace")
	{
		std::string path;
		ss >> path;
		startNetworkTrace(path);
	}
	else
		printHelp();
}

int main(int argc, char* argv[])
{
	using namespace httplib;

        int opt = 0;
	int port = 80; // Set to default port 80 if no port is specified at command line
	char *hostname = NULL;
        enum { CHARACTER_MODE, WORD_MODE, LINE_MODE } mode = CHARACTER_MODE;

	if (argc < 2)
	{
		std::cout << "Start with www directory path as argument." << std::endl;
		return -1;
	}

        while ((opt = getopt(argc, argv, "p:h:?")) != -1) {
        	switch (opt) {
		case 'p': 
			// std::cout << "-p" << optarg << std::endl;
			port = atoi(optarg);
			break;
        	case 'h': 
			hostname = optarg;
			// std::cout << "-h" << optarg << std::endl;
			// printf(“port: %s\n”, optarg);
                        break;
		case '?':
			if (optopt == 'p')
          			fprintf (stderr, "Option -%c requires an argument.\n", optopt);
			else if (optopt == 'h')
          			fprintf (stderr, "Option -%c requires an argument.\n", optopt);
        		else if (isprint (optopt))
          			fprintf (stderr, "Unknown option `-%c'.\n", optopt);
        		else
          			fprintf (stderr, "Unknown option character `\\x%x'.\n", optopt);
        		return 1;
        	default:
            		fprintf(stderr, "Usage: %s [-p port] [-h hostname] www_dir_path\n", argv[0]);
            		exit(EXIT_FAILURE);
        	}
    	}

        std::cout << argv[argc-1] << std::endl;
	Server sv;
	sv.set_base_dir(argv[argc-1]);
	sv.set_host_name(hostname);
        sv.set_port(port);
	sv.Get("/cntrl", [](const Request& req, Response& res) {
		std::string cntrlContent;
		cntrlContent.resize(httplib::bandwidth / 10 + 1, 'c');
		res.set_content(cntrlContent, "text/plain");
	});

	sv.Get(R"(/trace/([^\s]+))", [&](const Request& req, Response& res) {
		std::string path = req.matches[1];
		startNetworkTrace(path);
		res.set_content("ok", "text/plain");
	});

	sv.Get(R"(/bw/(\d+))", [&](const Request& req, Response& res) {
		int bw = std::stoi(req.matches[1]);
		stopNetworkTrace();
		httplib::bandwidth = bw;
		res.set_content("ok", "text/plain");
	});

	sv.Get("/tracereset", [&](const Request& req, Response& res) {
		resetTrace = true;
		res.set_content("ok", "text/plain");
	});

	std::cout << "Starting Web Server on port " << port << ", root directory " << argv[argc-1] << std::endl;
	std::cout << "Listening on port " << port << std::endl;
	// std::thread([&sv]() { sv.listen("localhost", 80); }).detach();
	std::thread([&sv]() { sv.listen(); }).detach();
	// std::thread([&sv]() { sv.listen(hostname, 7777); }).detach();

	while (true)
	{
		std::cout << "> ";
		std::string input;
		std::getline(std::cin, input);
		processCommand(input);
	}

	return 0;
}
