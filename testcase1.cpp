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
void testcase1()
{
    // root must always outlife all children Dependency reactors
    requirecpp::DependencyReactor<TestCaseA> rootReact;
    std::thread t{[]()
    {
        try
        {
            requirecpp::DependencyReactor<TestCaseA, TestThreadType> depReact;
            // sync, blocking version
            auto user = depReact.require<UseSlowComponent>();
            user->use(); // -> Add Require decorator "Lazy" to block on first usage
            // tell requirecpp the thread has started and all require calls have been issued
            depReact.createComponent<TestThreadType>();
            // async version
            depReact.require<UseSlowComponent>([](const auto &user)
            {
                user->use();
            });

            std::cout << "cleanup TestcaseA" << std::endl;
        }
        catch (const std::exception &e)
        {
            std::cout << "Exception in Thread: " << e.what() << std::endl << std::endl;
        }
    }};
    try
    {
        // require thread to be started
        CreatorObject c{};
        auto thread = rootReact.require<TestThreadType>();
        thread.unlock();
        // require thread finish all require() calls
        std::cout << "require finish" << std::endl;
        rootReact.require<Finished<TestThreadType>>(); //< fails if not all require calls have been made,
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
