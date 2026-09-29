#include "../tools/ProcessRunner.hpp"
#include <fstream>
#include <iostream>
#include <atomic>

int main(int argc,char** argv) {
    if(argc==2 && std::string(argv[1])=="--sleep") {
        std::this_thread::sleep_for(std::chrono::seconds(60)); return 99;
    }
    if(argc==2 && std::string(argv[1])=="--ok") { std::cout << "child completed"; return 0; }
    const auto executable=std::filesystem::absolute(argv[0]);
    const auto out=std::filesystem::temp_directory_path()/"nextgen-process-test-output.txt";
    const auto start=std::chrono::steady_clock::now();
    const auto stalled=nextgen::tools::RunProcess(executable,{"--sleep"},out,std::chrono::milliseconds(200));
    if(!stalled.timedOut || std::chrono::steady_clock::now()-start>std::chrono::seconds(5)) return 1;
    const auto next=nextgen::tools::RunProcess(executable,{"--ok"},out,std::chrono::seconds(5));
    std::ifstream in(out); const std::string text((std::istreambuf_iterator<char>(in)),{}); in.close();
    std::filesystem::remove(out);
    if(next.timedOut || next.exitCode!=0 || text!="child completed") return 1;
    std::atomic<int> failures=0;
    {
        std::vector<std::jthread> workers;
        for(int worker=0;worker<4;++worker)workers.emplace_back([&,worker] {
            const auto log=std::filesystem::temp_directory_path()/("nextgen-process-parallel-"+std::to_string(worker)+".txt");
            for(int run=0;run<8;++run) {
                const auto result=nextgen::tools::RunProcess(executable,{"--ok"},log,std::chrono::seconds(5));
                if(result.exitCode!=0||result.timedOut)++failures;
                std::ifstream in(log);const std::string output((std::istreambuf_iterator<char>(in)),{});in.close();
                if(output!="child completed")++failures;
            }
            std::filesystem::remove(log);
        });
    }
    if(failures) {std::cerr<<"Parallel process isolation failed\n";return 1;}
    std::cout << "Hard timeout and subsequent process verified\n";
}
