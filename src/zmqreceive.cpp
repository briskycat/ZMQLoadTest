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

    auto receiveFunc = [&topic](zmq::context_t& context, const std::string& address, const int txRate) {
        try {
            auto log = spdlog::get("console");

            log->info() << "address = " << address;
            log->info() << "socket rxRate = " << txRate;

            zmq::socket_t socket(context, ZMQ_SUB);

            const int rate = txRate; // kilobits per second
            socket.setsockopt(ZMQ_RATE, &rate, sizeof(rate)); // The ZMQ_RATE option shall set the maximum send or receive data rate for multicast transports such as zmq_pgm(7) using the specified socket.

            socket.connect(address.c_str());

            socket.setsockopt(ZMQ_SUBSCRIBE, topic.c_str(), topic.length());

            std::chrono::steady_clock::time_point timeOfLastMessage = std::chrono::steady_clock::now(), start = std::chrono::steady_clock::now();
            uint64_t totalBytesReceived = 0;
            while (context != nullptr) {
                zmq::message_t mcMsg;
                if (!socket.recv(&mcMsg)) {
                    log->trace() << "Timeout";
                }
                else {
                    std::chrono::microseconds elapsedMicroseconds = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - timeOfLastMessage);
                    if(!elapsedMicroseconds.count())
                        elapsedMicroseconds = std::chrono::microseconds(1);

                    std::chrono::microseconds totalElapsedMicroseconds = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start);
                    if(!totalElapsedMicroseconds.count())
                        totalElapsedMicroseconds = std::chrono::microseconds(1);

                    totalBytesReceived += mcMsg.size();

                    log->trace() << "received a multicast message with " << mcMsg.size() << " bytes";
                    log->trace() << "current rate = " << (mcMsg.size() * 8ULL * 1000ULL) / elapsedMicroseconds.count() << " kilobits per second";
                    log->trace() << "avarage rate = " << (totalBytesReceived * 8ULL * 1000ULL) / totalElapsedMicroseconds.count() << " kilobits per second";
                    timeOfLastMessage = std::chrono::steady_clock::now();
                }
            }
        }
        catch(zmq::error_t& e) {
            if (e.num() != ETERM /* If not normal termination */) {
                throw;
            }
        }
    };

    auto log = spdlog::stdout_logger_mt("console");
    log->set_level(spdlog::level::trace);

    // Install a signal handler
    std::signal(SIGINT, signal_handler);

    try {
        std::string multicastAddress;
        int rxRate, workerThreads;

        po::options_description desc("Usage", 1024, 512);
        desc.add_options()
                ("help,h", "produce help message")
                ("multicast-address,m", po::value<std::string>(&multicastAddress)->default_value("epgm://239.192.2.3:5556"), "address for outgoing multicast data")
                ("upload-rate,R", po::value<int>(&rxRate)->default_value(8400), "maximum data rate in kilobits per second")
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
        auto receiveAsync = std::async(std::launch::async, std::bind(receiveFunc, std::ref(context), multicastAddress, rxRate));

        {
            std::cout << "Running. Press ^C (Control + C) to stop..." << std::endl;

            std::unique_lock<std::mutex> lk(gAccess);
            gCVQuit.wait(lk, [] {return gQuit; });

            std::cout << "Exiting..." << std::endl;
        }

        context.close();
        receiveAsync.get();
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
