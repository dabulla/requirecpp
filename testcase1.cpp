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

class TestThreadType {};
void testcase1()
{
    // root must always outlife all children Dependency reactors
    requirecpp::DependencyReactor<TestCaseA> depReact;
    std::thread t{[]()
    {
        try
        {
            requirecpp::DependencyReactor<TestCaseA, TestThreadType> depReact;
            // What can happen?
            // 1) require is called before createComponent
            // 2) require is called after createComponent
            // 3) require is called after destruction of component with container<Context> already destroyed
            auto user = depReact.require<UseSlowComponent>();
            user->use();
//            depReact.require<UseSlowComponent>([](auto &user)
//            {
//                user->use();
//            });
        }
        catch (const std::exception &e)
        {
            std::cout << "Exception in Thread: " << e.what() << std::endl << std::endl;
        }
    }};
    try
    {
        std::this_thread::sleep_for(10ms);
        depReact.createComponent<RegisterSlowComponent>();
        depReact.createComponent<UseSlowComponent>();
        std::cout << "Dest" << std::endl;
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
