#include <iostream>
#include <string>
#include <chrono>
#include <thread>

using namespace std;
void printProgressBar(int progress, int total, int length = 50) {
    float percentage = (float)progress / total;
    int filledLength = percentage * length;
    std::string bar(filledLength, '#');
    bar.append(length - filledLength, '-');
    std::cout << "\r[" << bar << "] " << int(percentage * 100.0) << "% Complete";
    std::cout.flush();
}

int main() {
    int total = 100;
    for (int i = 0; i <= total; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        printProgressBar(i, total);
    }
    std::cout << std::endl;
    return 0;
}
