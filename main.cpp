#include <iostream>
#include <thread>
#include "DependencyReactor.hpp"

using namespace requirecpp;
using namespace std::chrono_literals;

// The only reason why this must actually be a well defined type is typeid(Self) for debugging
class App {};

// Test two independent DependecyReactors
class BasicTest {};

class PlayerNameLabel
{
public:
    std::string getName()
    {
        return "Superhans";
    }
};

class PrintingComponent
{
    requirecpp::DependencyReactor<App, PrintingComponent> m_dr;
    std::string m_prefix;
public:
    PrintingComponent(const std::string &prefix)
        :m_prefix{prefix}
    {
        auto printVisitor = [this](auto &o)
        {
            std::cout << m_prefix << " Registered: " << typeid (o).name() << " (" << *o << ")" << std::endl;
        };
        // print Components of type int, double and string as soon as
        // they are registered or directly, if they exist
        m_dr.require<int>(printVisitor);
        m_dr.require<std::string>(printVisitor);
        m_dr.require<double>(printVisitor);
    }
    void print(const std::string &message)
    {
        std::cout << m_prefix << " Message: " << message << std::endl;
    }
};

class Player
{
    requirecpp::DependencyReactor<App, Player> m_dr;
public:
    PlayerNameLabel m_nameLabel;
    Player()
    {
        m_dr.registerExistingComponent(m_nameLabel);
        m_dr.require<PrintingComponent>([](auto &printer)
        {
            printer->print("Player is using printer");
        });
    }
};

class Chat
{
    requirecpp::DependencyReactor<App, Chat> m_dr;
public:
    Chat()
    {
        m_dr.require<requirecpp::Unlocked<PrintingComponent>, PlayerNameLabel>([](auto &printer, auto &label)
        {
            printer.print(label->getName() + " entered the room");
        });
    }
};

class End{};

class TestCaseA {};

class SlowComponent
{
public:
    void slowOperation()
    {
        std::cout << "slow operation started" << std::endl;
        std::this_thread::sleep_for(20ms);
        std::cout << "slow operation finished" << std::endl;
    }
};
class UseSlowComponent
{
    requirecpp::DependencyReactor<TestCaseA, UseSlowComponent> m_dr;
public:
    void use()
    {
        m_dr.require<SlowComponent>([](auto &comp)
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
void test()
{
    std::cout << "thread started" << std::endl;
    std::thread t{[]()
    {
        requirecpp::DependencyReactor<TestCaseA, TestThreadType> depReact;
        auto user = depReact.require<UseSlowComponent>();
        user->use();
//        depReact.require<UseSlowComponent>([](auto &user)
//        {
//            user->use();
//        });
    }};
    std::this_thread::sleep_for(10ms);
    {
        requirecpp::DependencyReactor<TestCaseA> depReact;
        depReact.createComponent<RegisterSlowComponent>();
        depReact.createComponent<UseSlowComponent>();
    }
    if(t.joinable())
    {
        t.join();
        std::cout << "thread finished" << std::endl;
    }
    std::cout << "End of test1" << std::endl;
}

int main()
{
    try {
        std::cout << "Start Test1" << std::endl;
        test();
        test();
        test();
        test();
        test();
        test();
        test();
        return 0;
        std::cout << "Start DependencyReactor" << std::endl;
        requirecpp::DependencyReactor<App> depReact;

        // when all dependecies for PrintingComponent, PlayerNameLabel, ... are satisfied, print a message
        depReact.require<PrintingComponent, PlayerNameLabel, Chat, double, int, End>(
                    [](const auto &...) { std::cout << "All dependencies have been satisfied" << std::endl; });

        // create PrintingComponent with a prefix.
        depReact.createComponent<PrintingComponent>(std::string{"printer ->"});

        // create a chat and print when all its dependecies are satisfied
        depReact.require<requirecpp::Finished<Chat>>([](const auto &...) { std::cout << "Chat finished!" << std::endl; });
        depReact.createComponent<Chat>();
        // Chat is not yet finished (it depends on PlayerNameLabel, which is not yet available). Print information about dependencies
        std::cout << "State:\n" << requirecpp::DependencyReactor<App>::whatsMissing() << std::endl;

        // finally register Player, which will register PlayerNameLabel
        depReact.createComponent<Player>();

        std::cout << "Name of Player: " << depReact.require<PlayerNameLabel>()->getName() << std::endl;
        std::cout << "Name of Player: " << depReact.require<Unlocked<PlayerNameLabel>>().getName() << std::endl;

        depReact.require<Unlocked<int>>([](int &){ std::cout << "Should never be called" << std::endl;});

        requirecpp::DependencyReactor<BasicTest> depReactBasic;
        // register a callback for int
        depReactBasic.require<Unlocked<int>>([](auto &i){ std::cout << "Recognized an int " << i << " and increment it." << std::endl; i++;});
        depReactBasic.createComponent<int>(333);
        depReactBasic.require<Unlocked<int>>([](auto &i){ std::cout << "After increment: " << i << std::endl;});
        std::cout << "Register double dependency" << std::endl;
        depReactBasic.require<Unlocked<int>, Unlocked<double>>([](int &i, double &d){ std::cout << "Having int and double " << i << " " << d << std::endl;});
        std::cout << "Register double -> " << depReactBasic.exists<double>() << std::endl;
        depReactBasic.createComponent<double>(2.0);
        std::cout << "Does double exist? -> " << depReactBasic.exists<double>() << std::endl;

        depReact.createComponent<End>();
    }
    catch(const std::exception &e)
    {
        std::cout << "Exception: " << e.what() << std::endl;
    }
}
