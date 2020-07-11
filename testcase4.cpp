#include "testcases.h"
#include "requirecpp.hpp"
#include <iostream>
#include <thread>
#include <atomic>

using namespace requirecpp;
using namespace std::chrono_literals;

// Token for require context
class TestCaseA {};

class SlowComponent
{
    std::atomic<int> m_counter{0};
public:
    void slowOperation()
    {
        std::this_thread::sleep_for(100ms);
        ++m_counter;
    }
};

class UseSlowComponent
{
    requirecpp::DependencyReactor<TestCaseA, UseSlowComponent> m_dr;
public:
    UseSlowComponent()
    {
        m_dr.require<SlowComponent>([](const auto &comp)
        {
            comp->slowOperation();
        });
    }
    void use()
    {
        m_dr.require<SlowComponent>([](const auto &comp)
        {
            comp->slowOperation();
        });
    }
};

class RegisterSlowComponent
{
    requirecpp::DependencyReactor<TestCaseA, RegisterSlowComponent> m_dr;
public:
    RegisterSlowComponent()
    {
        m_dr.createComponent<SlowComponent>();
    }
};

class CreatorObject
{
    requirecpp::DependencyReactor<TestCaseA, CreatorObject> m_dr;
public:
    CreatorObject()
    {
        std::this_thread::sleep_for(10ms);
        m_dr.createComponent<RegisterSlowComponent>();
        m_dr.createComponent<UseSlowComponent>();
        std::cout << "Dest" << std::endl;
    }
};

class TestThreadType {};
void testcase4()
{
    // root must always outlife all children Dependency reactors
    requirecpp::DependencyReactor<TestCaseA> depReact;

    bool reqCalled = false;
    std::mutex mtx;
    std::condition_variable cv;
    std::thread t{[&cv, &mtx, &reqCalled]()
    {
        try
        {
            requirecpp::DependencyReactor<TestCaseA, TestThreadType> depReact;
            depReact.createComponent<TestThreadType>();
            {
                // async cases:
                // 1) require is called before createComponent
                // 2) require is called after createComponent
                // 3) require is called after destruction of component with container<Context> already destroyed

                // async version
                depReact.require<UseSlowComponent>([](const auto &user)
                {
                    user->use();
                });

                // sync, blocking version
                auto user = depReact.require<UseSlowComponent>();
                std::unique_lock lk{mtx};
                reqCalled = true;
                lk.unlock();
                cv.notify_one();
                user->use();
            }
            std::unique_lock lk{mtx};
            cv.wait(lk, [&reqCalled]{return !reqCalled;});
            std::cout << "cleanup TestcaseA" << std::endl;
        }
        catch (const std::exception &e)
        {
            std::cout << "Exception in Thread: " << e.what() << std::endl << std::endl;
        }
    }};
    try
    {
        depReact.require<TestThreadType>();
        CreatorObject c{};
        std::unique_lock lk{mtx};
        cv.wait(lk, [&reqCalled]{ return reqCalled;});
        std::cout << "waited" << std::endl;
        depReact.require<Finished<TestThreadType>>();
        reqCalled = false;
        cv.notify_one();
        std::cout << "require Finished" << std::endl;
    }
    catch (const std::exception &e)
    {
        std::cout << "Exception: " << e.what() << std::endl << std::endl;
    }
    if(t.joinable())
    {
        std::cout << "Join ";
        t.join();
    }
    std::cout << "Fin" << std::endl;
}
