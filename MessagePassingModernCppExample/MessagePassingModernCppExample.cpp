#ifdef _WIN32
#include <windows.h>
#else
#include <signal.h>
#include <unistd.h>
#endif

#include <atomic>
#include <chrono>
#include <functional>
#include <future>
#include <iostream>
#include <mutex>
#include <random>
#include <sstream>

#include "BlockingCollection.h"
#include "cpptime.h"

#include "ResourceManager.h"

std::atomic<bool> g_quit(false);

template <typename T>
std::future<T> Enqueue(code_machina::BlockingCollection<std::function<void(ResourceManager*)>>& queue,
    std::function<T(ResourceManager*)> method)
{
    std::shared_ptr<std::promise<T>> prom = std::make_shared<std::promise<T>>();
    std::future<T> fut = prom->get_future();

    queue.add([prom, method](ResourceManager* r) {
        prom->set_value(method(r));
    });

    return fut;
}

// In the C# version, we can't enqueue lambdas that have void as a return type
// but in C++ it's possible if we use template specialization
template <>
std::future<void> Enqueue(code_machina::BlockingCollection<std::function<void(ResourceManager*)>>& queue,
    std::function<void(ResourceManager*)> method)
{
    std::shared_ptr<std::promise<void>> prom = std::make_shared<std::promise<void>>();
    std::future<void> fut = prom->get_future();

    queue.add([prom, method](ResourceManager* r) {
        method(r);
        prom->set_value();
    });

    return fut;
}

#ifdef _WIN32
BOOL WINAPI consoleHandler(DWORD signal) {

    if (signal == CTRL_C_EVENT)
    {
        g_quit = true;
    }

    return TRUE;
}
#else
void consoleHandler(int)
{
    g_quit = true;
}
#endif

int main()
{
#ifdef _WIN32
    if (!SetConsoleCtrlHandler(consoleHandler, TRUE))
    {
        printf("\nERROR: Could not set control handler\n");
        return 1;
    }
#else
    signal (SIGINT, consoleHandler);
#endif

    std::random_device dev;
    std::mt19937 rng(dev());
    std::uniform_int_distribution<std::mt19937::result_type> dist100(0, 99); // distribution in range [0, 99]

    ResourceManager resource;
    code_machina::BlockingCollection<std::function<void(ResourceManager*)>> queue;
    
    // start the thread managing the resource
    std::thread resMgrThread([&queue, &resource]() {
        while (!queue.is_completed())
        {
            std::function<void(ResourceManager*)> action;

            // take will block if there is no data to be taken
            auto status = queue.take(action);

            if (status == code_machina::BlockingCollectionStatus::Ok)
            {
                try
                {
                    action(&resource);
                }
                catch (...)
                {
                    std::printf("[Error] Caught an exception in ThreadMain !\n");
                }
            }

            // Status can also return BlockingCollectionStatus::Completed meaning take was called 
            // on a completed collection. Some other thread can call complete_adding after we pass 
            // the is_completed check but before we call take. In this example, we can ignore that
            // status since the loop will break on the next iteration.
        }
    });

    ResourceStatus lastResourceStatus;
    std::mutex lastResourceStatusLock;
    resource.ResourceStatusUpdatedSignal.connect([&lastResourceStatusLock,&lastResourceStatus](ResourceStatus s)
    {
        std::stringstream ss;
        ss << std::this_thread::get_id();

        std::size_t threadId;
        ss >> threadId;

        std::time_t now = s.Timestamp;
        std::tm* ptm = std::localtime(&now);
        char buffer[32];
        std::strftime(buffer, 32, "%d/%m/%Y %H:%M:%S", ptm);

        printf("[Thread %zu] Resource status has been updated at %s: %d.\n",
            threadId,
            buffer,
            s.SomeValue);

        lastResourceStatusLock.lock();
        lastResourceStatus = s;
        lastResourceStatusLock.unlock();
    });

    // spawn one thread here that will communicate with the thread managing the resource using prog.Enqueue(...)
    std::thread webServerThread([&queue, &resource]() {
        std::printf("Starting web server thread !\n");

        std::stringstream ss;
        ss << std::this_thread::get_id();

        std::size_t threadId;
        ss >> threadId;

        while (!g_quit.load())
        {
            std::future<std::string> currentTaskFuture = Enqueue<std::string>(queue, [threadId](ResourceManager* r)
            {
                return r->SendData(threadId);
            });
            std::string res = currentTaskFuture.get();

            printf("[Thread %zu] Task result = %s\n", threadId, res.c_str());

            std::this_thread::sleep_for(std::chrono::milliseconds(250));
        }
    });

    CppTime::Timer refreshStatusTimer;
    auto refreshStatusTimerId = refreshStatusTimer.add(std::chrono::seconds(1),
        [&rng, &dist100, &queue](CppTime::timer_id)
        {
            std::future<void> currentTaskFuture = Enqueue<void>(queue, [&rng, &dist100](ResourceManager* r)
            {
                r->UpdateStatus(dist100(rng));
            });
        }, std::chrono::seconds(1));

    // and use the main thread too
    std::stringstream ss;
    ss << std::this_thread::get_id();

    std::size_t mainThreadId;
    ss >> mainThreadId;

    while (!g_quit.load())
    {
        std::future<std::string> currentTaskFuture = Enqueue<std::string>(queue, [mainThreadId](ResourceManager* r)
        {
            return r->SendData(mainThreadId);
        });
        std::string res = currentTaskFuture.get();

        printf("[Thread %zu] Task result = %s\n", mainThreadId, res.c_str());

        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    };

    webServerThread.join();

    refreshStatusTimer.remove(refreshStatusTimerId);

    queue.complete_adding();
    resMgrThread.join();
}
