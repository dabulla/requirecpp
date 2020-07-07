#include <iostream>
#include "DependencyReactor.hpp"

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
    iocdr::DependencyReactor<App, PrintingComponent> m_dr;
    std::string m_prefix;
public:
    PrintingComponent(const std::string &prefix)
        :m_prefix{prefix}
    {
        auto printVisitor = [this](auto &o)
        {
            std::cout << m_prefix << " Registered: " << typeid (o).name() << " (" << o << ")" << std::endl;
        };
        // print Components of type int, double and string as soon as
        // they are registered or directly, if they exist
        m_dr.executeWith<int>(printVisitor);
        m_dr.executeWith<std::string>(printVisitor);
        m_dr.executeWith<double>(printVisitor);
    }
    void print(const std::string &message)
    {
        std::cout << m_prefix << " Message: " << message << std::endl;
    }
};

class Player
{
    iocdr::DependencyReactor<App, Player> m_dr;
public:
    PlayerNameLabel m_nameLabel;
    Player()
    {
        m_dr.registerExistingComponent(m_nameLabel);
        m_dr.executeWith<PrintingComponent>([](auto &printer)
        {
            printer.print("Player is using printer");
        });
    }
};

class Chat
{
    iocdr::DependencyReactor<App, Chat> m_dr;
public:
    Chat()
    {
        m_dr.executeWith<PrintingComponent, PlayerNameLabel>([](auto &printer, auto &label)
        {
            printer.print(label.getName() + " entered the room");
        });
    }
};

class End{};

int main()
{
    try {
        std::cout << "Start DependencyReactor" << std::endl;
        iocdr::DependencyReactor<App> depReact;

        // when all dependecies for PrintingComponent, PlayerNameLabel, ... are satisfied, print a message
        depReact.executeWhenFinished<PrintingComponent, PlayerNameLabel, Chat, double, int, End>(
                    [](const auto &...) { std::cout << "All dependencies have been satisfied" << std::endl; });

        // create PrintingComponent with a prefix.
        depReact.createComponent<PrintingComponent>(std::string{"printer ->"});

        // create a chat and print when all its dependecies are satisfied
        depReact.executeWhenFinished<Chat>([](const auto &...) { std::cout << "Chat finished!" << std::endl; });
        depReact.createComponent<Chat>();
        // Chat is not yet finished (it depends on PlayerNameLabel, which is not yet available). Print information about dependencies
        std::cout << "State:\n" << iocdr::DependencyReactor<App>::whatsMissing() << std::endl;

        // finally register Player, which will register PlayerNameLabel
        depReact.createComponent<Player>();

        std::cout << "Name of Player: " << depReact.get<PlayerNameLabel>().getName() << std::endl;

        depReact.executeWith<int>([](int &){ std::cout << "Should never be called" << std::endl;});

        iocdr::DependencyReactor<BasicTest> depReactBasic;
        // register a callback for int
        depReactBasic.executeWith<int>([](int &i){ std::cout << "Recognized an int " << i << " and increment it." << std::endl; i++;});
        depReactBasic.createComponent<int>(333);
        depReactBasic.executeWith<int>([](auto &i){ std::cout << "After increment: " << i << std::endl;});
        std::cout << "Register double dependency" << std::endl;
        depReactBasic.executeWith<int, double>([](int &i, double &d){ std::cout << "Having int and double " << i << " " << d << std::endl;});
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
