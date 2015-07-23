#include <zmq.hpp>
#include <spdlog/spdlog.h>
#include <boost/program_options.hpp>
#include <csignal>
#include <iostream>
#include <mutex>
#include <condition_variable>
#include <future>
#include <chrono>

namespace po = boost::program_options;

std::mutex gAccess;
bool gQuit = false;
std::condition_variable gCVQuit;

static void signal_handler(int)
{
    {
        std::unique_lock<std::mutex> lk(gAccess);
        gQuit = true;
    }
    gCVQuit.notify_all();
}

int main(int argc, char* argv[])
{
    const std::string topic("ZMQ-Test");

    auto sendFunc = [&topic](zmq::context_t& context, const std::string& address, const size_t messageSize, const int txRate, const int ttl) {
        try {
            auto log = spdlog::get("console");

            // make real data TX rate 5% lower to allow for service data
            unsigned long long dataTxRate = static_cast<unsigned long long>(txRate) - static_cast<unsigned long long>(txRate) * 5ULL / 100ULL;

            log->info() << "address = " << address;
            log->info() << "messageSize = " << messageSize;
            log->info() << "socket txRate = " << txRate;
            log->info() << "data txRate = " << dataTxRate;
            log->info() << "ttl = " << ttl;

            zmq::socket_t socket(context, ZMQ_PUB);

            const int linger = 100;
            socket.setsockopt(ZMQ_LINGER, &linger, sizeof(linger));

            const int rate = txRate; // kilobits per second
            socket.setsockopt(ZMQ_RATE, &rate, sizeof(rate)); // The ZMQ_RATE option shall set the maximum send or receive data rate for multicast transports such as zmq_pgm(7) using the specified socket.

            const int maxHops = ttl;
            socket.setsockopt(ZMQ_MULTICAST_HOPS, &maxHops, sizeof(maxHops)); // Sets the time-to-live field in every multicast packet sent from this socket. The default is 1 which means that the multicast packets don't leave the local network.

            // const int recIvl = 10000;
            // socket.setsockopt (ZMQ_RECOVERY_IVL, &recIvl, sizeof(recIvl)); // The ZMQ_RECOVERY_IVL option shall set the recovery interval for multicast transports using the specified socket. The recovery interval determines the maximum time in milliseconds that a receiver can be absent from a multicast group before unrecoverable data loss will occur.

            socket.bind(address.c_str());

            std::vector<char> someData(messageSize);
            std::copy(topic.cbegin(), topic.cend(), someData.begin());

            std::chrono::steady_clock::time_point timeOfNextSend = std::chrono::steady_clock::now();
            while (context != nullptr) {

                std::this_thread::sleep_until(timeOfNextSend);

                auto delay = std::chrono::microseconds(static_cast<unsigned long long>(someData.size()) * 8LL * 1000LL / dataTxRate); // txRate is in kilobits per second or = in bits per millisecond = 0.001 bits per microsecond and so on
                timeOfNextSend = std::chrono::steady_clock::now() + (delay.count() ? delay : std::chrono::microseconds(1));

                if(socket.send(someData.data(), someData.size())) {
                    log->trace() << "sent a message with " << someData.size() << " random bytes";
                }

                log->trace() << "delay until next send = " << delay.count() << " microseconds";
            }
        }
        catch(zmq::error_t& e) {
            if (e.num() != ETERM /* If not normal termination */) {
                throw;
            }
        }
    };

    auto log =  spdlog::stdout_logger_mt("console");
    log->set_level(spdlog::level::trace);

    // Install a signal handler
    std::signal(SIGINT, signal_handler);

    try {
        std::string multicastAddress;
        size_t messageSize;
        int txRate, maxHops, workerThreads;

        po::options_description desc("Usage", 1024, 512);
        desc.add_options()
                ("help,h", "produce help message")
                ("multicast-address,m", po::value<std::string>(&multicastAddress)->default_value("epgm://239.192.2.3:5556"), "address for outgoing multicast data")
                ("message-size,S", po::value<size_t>(&messageSize)->default_value(1024 * 1024), "size of a message to multicast")
                ("upload-rate,R", po::value<int>(&txRate)->default_value(8400), "maximum data upload rate in kilobits per second")
                ("ttl,T", po::value<int>(&maxHops)->default_value(1), "multicast packet TTL (or maximum number of hops between networks)")
                ("worker-threads,W", po::value<int>(&workerThreads)->default_value(1), "number of I/O threads in ZMQ context")
                ;
        po::positional_options_description p;
        p.add("multicast-address", -1);

        po::variables_map vm;
        po::store(po::command_line_parser(argc, argv).options(desc).positional(p).run(), vm);

        if (vm.count("help")) {
            std::cout << desc << std::endl;
            return EXIT_FAILURE;
        }

        po::notify(vm);

        zmq::context_t context(workerThreads);
        auto sendAsync = std::async(std::launch::async, std::bind(sendFunc, std::ref(context), multicastAddress, messageSize, txRate, maxHops));

        {
            std::cout << "Running. Press ^C (Control + C) to stop..." << std::endl;

            std::unique_lock<std::mutex> lk(gAccess);
            gCVQuit.wait(lk, [] {return gQuit; });

            std::cout << "Exiting..." << std::endl;
        }

        context.close();
        sendAsync.get();
    }
    catch (std::exception& e) {
        log->critical() << "Error: " << e.what();
        return EXIT_FAILURE;
    }
    catch (...) {
        log->critical() << "Unknown error!";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
