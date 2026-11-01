#include <condition_variable>
#include <iostream>
#include <memory>
#include <mutex>
#include <numeric>
#include <string>
#include <thread>
#include <chrono>
#include <vector>

class Message {
public:
    int id;
    std::string text;
    std::vector<int> numbers;
    Message(int i, std::string t, std::vector<int> n)
        : id(i), text(std::move(t)), numbers(std::move(n)) {}
    int sum() const { return std::accumulate(numbers.begin(), numbers.end(), 0); }
};

class Monitor {
    std::mutex m;
    std::condition_variable cv;
    std::unique_ptr<Message> data;
    bool stopped = false;
public:
   
    bool provide(std::unique_ptr<Message> msg) {
        std::unique_lock<std::mutex> lk(m);
        cv.wait(lk, [&]{ return !data || stopped; });
        if (stopped) return false;
        std::cout << "provided #" << msg->id << ": " << msg->text << std::endl;
        data = std::move(msg);
        cv.notify_one();
        return true;
    }

    
    std::unique_ptr<Message> consume() {
        std::unique_lock<std::mutex> lk(m);
        cv.wait(lk, [&]{ return data || stopped; });
        if (!data) return nullptr;                 
        std::cout << "consumed #" << data->id << ": " << data->text
                  << ", sum = " << data->sum() << std::endl;
        auto res = std::move(data);
        cv.notify_one();
        return res;
    }
    void stop() {
        { std::lock_guard<std::mutex> lk(m); stopped = true; }
        cv.notify_all();                           
    }
};

int main() {
    Monitor mon;
    std::thread producer([&]{
        for (int i = 1; ; ++i) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            std::vector<int> nums(i);
            std::iota(nums.begin(), nums.end(), 1);
            if (!mon.provide(std::make_unique<Message>(i, "event " + std::to_string(i), nums)))
                break;
        }
        std::cout << "producer stopped" << std::endl;
    });
    std::thread consumer([&]{
        while (auto msg = mon.consume()) {
          
        }
        std::cout << "consumer stopped" << std::endl;
    });

    std::cout << " press Enter to stop \n";
    std::cin.get();
    mon.stop();
    producer.join();
    consumer.join();
}
