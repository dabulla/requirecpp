#ifndef DEPENDENCYREACTOR_HPP
#define DEPENDENCYREACTOR_HPP

#include <chrono>
#include <condition_variable>
#include <deque>
#include <functional>
#include <optional>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <unordered_set>
#include <utility>
#include <vector>
#include <iostream>

namespace
{


template<typename Context>
struct ContextAssociated
{
    /**
     * Container for Components. Components are owned by this container and only exist once here.
     * std::unique_ptr allows usage of incomplete types.
     */
    template<typename T>
    static T* components;

    template<typename T>
    static std::shared_mutex componentReferenceMutex;
    template<typename T>
    static std::condition_variable_any componentReferenceCondition;
    template<typename T>
    static std::condition_variable_any componentReferenceConditionBack;

    // flag to tell condition variable to stop waiting. Used when the required component is being decomposed
    template<typename T>
    static bool unavailable;
    // number of blocking require calls for this component
    template<typename T>
    static unsigned int awaited;
    // this should only prevent threading problems but allow registerComponent to recurse
    // (in ctor of Component another Component can be registered)
    static std::recursive_mutex componentsMutex;

    static std::vector<std::function<void()>> checkDependencies;
    // not shared, because each must only be executed once
    static std::mutex checkDependenciesMutex;

    static std::vector<std::function<std::string()>> whatsMissing;
    static std::mutex whatsMissingMutex;
};

template<typename Context>
template<typename T>
T* ContextAssociated<Context>::components;
template<typename Context>
template<typename T>
std::shared_mutex ContextAssociated<Context>::componentReferenceMutex;
template<typename Context>
template<typename T>
std::condition_variable_any ContextAssociated<Context>::componentReferenceCondition;
template<typename Context>
template<typename T>
std::condition_variable_any ContextAssociated<Context>::componentReferenceConditionBack;

template<typename Context>
template<typename T>
bool ContextAssociated<Context>::unavailable {false};

template<typename Context>
template<typename T>
unsigned int ContextAssociated<Context>::awaited {0};

template<typename Context>
std::recursive_mutex ContextAssociated<Context>::componentsMutex;

template<typename Context>
std::vector<std::function<void()>> ContextAssociated<Context>::checkDependencies;
template<typename Context>
std::mutex ContextAssociated<Context>::checkDependenciesMutex;

template<typename Context>
std::vector<std::function<std::string()>> ContextAssociated<Context>::whatsMissing;
template<typename Context>
std::mutex ContextAssociated<Context>::whatsMissingMutex;

//template<typename Self>
//struct SelfAssociated
//{
//    template<typename T>
//    static std::unique_ptr<T> componentsOwned;
//};
}

namespace requirecpp {

class DependencyCallbackBase : public std::function<bool()>
{
    public:
    using function::function;
    virtual std::unordered_set<std::string> whatsMissing() const
    {
        return std::unordered_set<std::string>{"Error"};
    }
};

template <typename Context, typename ...Deps>
class DependencyCallback : public DependencyCallbackBase
{
public:
    using DependencyCallbackBase::DependencyCallbackBase;

    std::unordered_set<std::string> whatsMissing() const override
    {
        return _whatsMissing<Deps...>();
    }

    template<typename Dep>
    std::unordered_set<std::string> _whatsMissing() const
    {
        std::unordered_set<std::string> ret{typeid(Dep).name()};
        return ret;
    }

    // exists should be private, is not guarded
    template<typename Dep, typename Dep2, typename ...Args>
    std::unordered_set<std::string> _whatsMissing() const
    {
        auto set1 = _whatsMissing<Dep2, Args...>();
        const auto set2 = _whatsMissing<Dep2, Args...>();
        set1.insert(set2.cbegin(), set2.cend());
        return set1;
    }
};

/// Decorations for create/register ///

/**
 * Allows components to be unregistered and registered again.
 * An owned component is unregistered when the owning DependecyReactor destructs
 * Others can require the component during destruction. If it is a reregisterable
 * component, calls to require will wait. If it is not a reregisterable component,
 * the call to require will fail and throw.
 * If the application hangs on exit due to
 */
template<typename T>
class Reregisterable //: public T
{
public:
    using type = T;
    //operator T() const { return *this; }
};

/// Decoration for require ///

template<typename T>
class Finished //: public T
{
public:
    using type = T;
    //operator T() const { return *this; }
};

template<typename T>
class Unlocked //: public T
{
public:
    using type = T;
    //operator T() const { return *this; }
};

// Not yet implemented
template<typename T>
class OptionalPeek //: public T
{
public:
    using type = T;
    //operator T() const { return *this; }
};
// Not yet implemented
template<typename T, typename Duration, Duration duration_>
class Timed //: public T
{
public:
    using type = T;
    static constexpr Duration duration = duration_;
    //operator T() const { return *this; }
};

template<typename Context, typename T>
class ComponentReference
{
    T& m_t;
    std::shared_lock<std::shared_mutex> m_lock;
public:
    using type = T;
    operator T() { return m_t; }
    operator T() const { return m_t; }
    T* operator->() { return &m_t; }
    T* operator->() const { return &m_t;  }
    T& operator* () { return m_t; }
    T& operator* () const { return m_t; }
    ComponentReference(T& t, std::shared_lock<std::shared_mutex>&& lock)
        :m_t{t}
        ,m_lock{std::move(lock)}
    {}
    ComponentReference(const ComponentReference& other) = delete; // no copy
    ComponentReference(ComponentReference&& other) noexcept = delete; // no move
    ComponentReference& operator=(const ComponentReference& other) = delete;  // no copy assign
    ComponentReference& operator=(ComponentReference&& other) noexcept = delete;  // no move assign
};

template<typename Context, typename T>
class ComponentReference<Context, std::optional<std::reference_wrapper<T>>> : public std::optional<std::reference_wrapper<T>>
{};
template<typename Context, typename T>
using ComponentOptionalReference = ComponentReference<Context, std::optional<std::reference_wrapper<T>>>;

// use this to disable debugging information
//template <typename Context, typename ...Deps>
//using CallbackImpl = std::function<bool()>

//template <typename Context, typename ...Deps>
//using CallbackEx = std::function<bool()>

template <typename Context, typename ...Deps>
using CallbackImpl = DependencyCallback<Context, Deps...>;

using CallbackEx = std::unique_ptr<DependencyCallbackBase>;


// Context can be T
// Components have a member DependencyReactor<Themself>
// With that interface executeWith<Dep1, Dep2, Dep3>(lambda) can be used.
// As soon, as all lambdas with dependencies have been executed, onFinished<T>(lambda) is invoked,
// this could happend directly after registerComponent, if all dependecies are met or if there are no dependencies.
// Whenever a component is registered, the executeWith-lambdas that have all dependencies satisfied,
// are executed. After that onFinished is checked an might be invoked.
// executeWith<Dep1, Dep2, Dep3>(lambda) can be used with executeWith<RequireFinished<Dep1>, Dep2, Dep3>(lambda), this must not introduce circular dependencies.
template <typename Context, typename Self = Context>
class DependencyReactor
{
public:
    void checkDependecies()
    {
        auto end1 = std::remove_if(s_executeWith.begin(), s_executeWith.end(), [](auto& cb){ return (*cb)();});
        s_executeWith.erase(end1, s_executeWith.end());
        auto end2 = std::remove_if(s_executeWhenFinished.begin(), s_executeWhenFinished.end(), [](auto& cb){ return (*cb)();});
        s_executeWhenFinished.erase(end2, s_executeWhenFinished.end());
    }
    DependencyReactor()
    {
        std::unique_lock staticLock{s_mutex};
        if(s_exists) throw std::runtime_error{"Only one DependencyReactor per class allowed"};
        s_exists = true;
        staticLock.unlock();
        std::scoped_lock {ContextAssociated<Context>::checkDependenciesMutex};
        ContextAssociated<Context>::checkDependencies.emplace_back(std::bind(&DependencyReactor<Context, Self>::checkDependecies, this));

        std::scoped_lock {ContextAssociated<Context>::whatsMissingMutex};
        ContextAssociated<Context>::whatsMissing.emplace_back(std::bind(&DependencyReactor<Context, Self>::whatsMissingLocal, this));
    }
    ~DependencyReactor()
    {
        std::unique_lock {s_mutex};
        //std::unique_lock {ContextAssociated<Context>::componentsMutex};
        s_exists = false; // in clean, all blocking require() calls have to return
        for (auto& cv : s_pendingRequires)
        {
            std::cout << "aborted require" << std::endl;
            cv->notify_all();
        }
        s_pendingRequires.clear();
        for (auto&& clear : m_clear)
        {
            clear();
        }
    }
    DependencyReactor(const DependencyReactor& _other) = delete;
    DependencyReactor& operator=(const DependencyReactor& _other) = delete;

    template < template <typename...> class Template, typename T >
    struct is_specialization_of : std::false_type {};

    template < template <typename...> class Template, typename... Args >
    struct is_specialization_of< Template, Template<Args...> > : std::true_type {};

    template<typename T>
    void checkRegisterPreconditions()
    {
        static_assert(!std::is_same<Context, T>(), "Context cannot be registered");
        static_assert(!is_specialization_of<Finished, T>(), "Do not decorate createComponent with Finished");
        static_assert(!is_specialization_of<Unlocked, T>(), "Do not decorate createComponent with Unlocked");

        if(Exists<T>::exists())
        {
            throw std::runtime_error{"Called registerCompontent twice"};
        }
        //std::cout << "register " << std::string(typeid(T).name()) << " in " << std::string(typeid(Context).name()) << " exists: " << Exists<T>::exists() << std::endl;
    }
    template<typename T>
    void componentAdded()
    {
        //std::cout << "Notify " << std::string(typeid(T).name()) << " in " << std::string(typeid(Context).name()) << " exists: " << Exists<T>::exists() << std::endl;
        ContextAssociated<Context>::template componentReferenceCondition<T>.notify_all();
        std::scoped_lock depsLock{ContextAssociated<Context>::checkDependenciesMutex};
        for (auto&& checkDep : ContextAssociated<Context>::checkDependencies)
        {
            checkDep();
        }
    }

    /**
     *  register compontent of type T. Args are forwarded to construct T
     */
    template<typename T, typename ...Args>
    void createComponent(const Args&& ...args)
    {
        //std::unique_lock{s_mutex};
        //std::unique_lock   {ContextAssociated<Context>::componentsMutex};
        std::unique_lock lk{ContextAssociated<Context>::template componentReferenceMutex<T>};
        checkRegisterPreconditions<T>();
        std::unique_ptr<T> &ownedComp = DependencyReactor<Context, Self>::s_componentsOwned<T>;
        auto& component = ContextAssociated<Context>::template components<T>;
        m_clear.emplace_back([&ownedComp, &component]() // < todo: make this use static vars, find way to fix race condition, find better synchronozation that with condition_variable ping/pong
        {
            std::unique_lock lk{ContextAssociated<Context>::template componentReferenceMutex<T>};
            ContextAssociated<Context>::template unavailable<T> = true;
            std::cout << "before Notify " << std::string{typeid(T).name()} << " in " << std::string{typeid(Context).name()} << " exists: " << Exists<T>::exists() << " unav: " <<  ContextAssociated<Context>::template unavailable<T> << " awaited " << ContextAssociated<Context>::template awaited<T> << std::endl;
            ContextAssociated<Context>::template componentReferenceCondition<T>.notify_all();
            ContextAssociated<Context>::template componentReferenceConditionBack<T>.wait(lk, []()
            {
                return 0 == ContextAssociated<Context>::template awaited<T>;
            });
            std::cout << "after pingback Notify " << std::string{typeid(T).name()} << " in " << std::string{typeid(Context).name()} << " exists: " << Exists<T>::exists() << " unav: " <<  ContextAssociated<Context>::template unavailable<T> << " awaited " << ContextAssociated<Context>::template awaited<T> << std::endl;
            component = nullptr; // breakpoint here and MinGW causes race condition
            ownedComp.reset();
            ContextAssociated<Context>::template unavailable<T> = false;
        });
        ownedComp = std::make_unique<T>(args...);
        ContextAssociated<Context>::template components<T> = ownedComp.get();
        lk.unlock();
        componentAdded<T>();
    }
    template<typename T>
    void registerExistingComponent(T& comp)
    {
        //std::unique_lock{s_mutex};
        //std::unique_lock   {ContextAssociated<Context>::componentsMutex};
        std::unique_lock lk{ContextAssociated<Context>::template componentReferenceMutex<T>};
        checkRegisterPreconditions<T>();
        auto& component = ContextAssociated<Context>::template components<T>;
        m_clear.emplace_back([&component]()
        {
            // s_exists is now false, condition variable can terminate
            std::unique_lock lk{ContextAssociated<Context>::template componentReferenceMutex<T>};
            ContextAssociated<Context>::template unavailable<T> = true;
            std::cout << "before Notify " << std::string{typeid(T).name()} << " in " << std::string{typeid(Context).name()} << " exists: " << Exists<T>::exists() << " unav: " <<  ContextAssociated<Context>::template unavailable<T> << " awaited " << ContextAssociated<Context>::template awaited<T> << std::endl;
            ContextAssociated<Context>::template componentReferenceCondition<T>.notify_all();
            ContextAssociated<Context>::template componentReferenceConditionBack<T>.wait(lk, []()
            {
                return 0 == ContextAssociated<Context>::template awaited<T>;
            });
            std::cout << "after pingback Notify " << std::string{typeid(T).name()} << " in " << std::string{typeid(Context).name()} << " exists: " << Exists<T>::exists() << " unav: " <<  ContextAssociated<Context>::template unavailable<T> << " awaited " << ContextAssociated<Context>::template awaited<T> << std::endl;
            component = nullptr;
            // let blocking calls to require wait for reregistering this component
            ContextAssociated<Context>::template unavailable<T> = false;
        });
        component = &comp;
        lk.unlock();
        componentAdded<T>();
    }


    template <typename ...Tail>
    struct Exists
    {
        static bool exists() { return true; }
    };
    template <typename Dep, typename ...Tail>
    struct Exists<Finished<Dep>, Tail...>
    {
        static bool exists()
        {
            return Exists<Dep>::exists() && DependencyReactor<Context, Dep>::s_executeWith.empty() && Exists<Tail...>::exists();
        }
    };
    // exists never locks
    template <typename Dep, typename ...Tail>
    struct Exists<Unlocked<Dep>, Tail...>
    {
        static bool exists()
        {
            return Exists<Dep, Tail...>::exists();
        }
    };
    template <typename Dep, typename ...Tail>
    struct Exists<Dep, Tail...>
    {
        static bool exists()
        {
            return nullptr != ContextAssociated<Context>::template components<Dep> && Exists<Tail...>::exists();
        }
    };

    // exists should be private, is not guarded
    template<typename ...Args>
    bool exists()
    {
        return Exists<Args...>::exists();
    }


    template <typename T>
    struct Get
    {
        // fast, for internal usage, assume component exists, else throw
        static ComponentReference<Context, T> get()
        {
            std::shared_lock lk{ContextAssociated<Context>::template componentReferenceMutex<T>};
            return {getUnlocked(), std::move(lk)};
        }
        // check existance in wait condition
        static ComponentReference<Context, T> getSync()
        {
            std::shared_lock lk{ContextAssociated<Context>::template componentReferenceMutex<T>};
            //std::cout << "Shared lock " << std::string{typeid(T).name()} << " in " << std::string{typeid(Context).name()} << " exists: " << Exists<T>::exists() << std::endl;
            ContextAssociated<Context>::template awaited<T>++;
            if(ContextAssociated<Context>::template awaited<T> == 1)
            {
                s_pendingRequires.emplace(&ContextAssociated<Context>::template componentReferenceCondition<T>);
                s_pendingRequires.emplace(&ContextAssociated<Context>::template componentReferenceConditionBack<T>);
            }
            auto decrementAwait = [](void*){
                ContextAssociated<Context>::template awaited<T>--;
                if(ContextAssociated<Context>::template awaited<T> == 0)
                {
                    s_pendingRequires.erase(&ContextAssociated<Context>::template componentReferenceConditionBack<T>);
                    s_pendingRequires.erase(&ContextAssociated<Context>::template componentReferenceCondition<T>);
                }
                ContextAssociated<Context>::template componentReferenceConditionBack<T>.notify_all(); // notify clear
            };
            std::unique_ptr<void, decltype(decrementAwait)> awaitGuard{(void*)1, decrementAwait };
            ContextAssociated<Context>::template componentReferenceCondition<T>.wait(lk, []
            {
                std::cout << "in Wait " << std::string{typeid(T).name()} << " in " << std::string{typeid(Context).name()} << " exists: " << Exists<T>::exists() << " unav: " <<  ContextAssociated<Context>::template unavailable<T> << " awaited " << ContextAssociated<Context>::template awaited<T> << std::endl;
                return Exists<T>::exists() || ContextAssociated<Context>::template unavailable<T> || !s_exists;
            });
            const bool componentAvailable = ContextAssociated<Context>::template unavailable<T>; // store result before notifying clear
            const bool exists = s_exists;
            awaitGuard.reset(); // decrement await
            if(componentAvailable) throw std::runtime_error{"Component was never registered (component shutdown)"};
            if(!exists) throw std::runtime_error{"Component was never registered (self shutdown)"};
            return {getUnlocked(), std::move(lk)};
        }
        template <typename DurationType>
        static ComponentReference<Context, T> timedGet(const DurationType &dur)
        {
            std::shared_lock lk{ContextAssociated<Context>::template componentReferenceMutex<T>};
            if(!ContextAssociated<Context>::template componentReferenceCondition<T>.wait_for(lk, dur, []
            {
                return Exists<T>::exists() || ContextAssociated<Context>::template unavailable<T>;
            }))
            {
                throw std::runtime_error{"Component timeout"};
            }
            return {getUnlocked(), std::move(lk)};
        }
        // just throw if component does not exist
        static T& getUnlocked()
        {
            T* &comp = ContextAssociated<Context>::template components<T>;
            if(nullptr == comp) throw std::runtime_error{"Component Unknown"};
            return *comp;
        }
    };
    template <typename T>
    struct Get<Unlocked<T>>
    {
        static T& get() { return Get<T>::getUnlocked(); }
        // sync not supported yet for unlocked
        static T& getSync() { return Get<T>::getUnlocked(); }
        // timed not supported yet for unlocked and should not compile
        //template <typename DurationType>
        //static auto timedGet(const DurationType &dur) { return Get<T>::timedGet(const DurationType &dur); }
        static T& getUnlocked() { return Get<T>::getUnlocked(); }
    };
    template <typename T>
    struct Get<Finished<T>>
    {
        static auto get() { return Get<T>::get(); }
        static auto getSync() { return Get<T>::getSync(); }
        template <typename DurationType>
        static auto timedGet(const DurationType &dur) { return Get<T>::timedGet(dur); }

        static T& getUnlocked() { return Get<T>::getUnlocked(); }
    };

    // Locked/Unlocked can be specified: unlocked is a reference. locked is not
    // callbacks can get a reference to a locked
    // while Finished is omitted in the callback, Unlocked is not. It can be deduced using auto, however, the lambda must dereference (use -> or *)
    template<typename T>
    static auto require() -> decltype(Get<T>::get())
    {
        //std::unique_lock {ContextAssociated<Context>::componentsMutex};
        return Get<T>::getSync();
    }

    template<typename ...Deps, typename Callback>
    void require(const Callback &&callback)
    {
        //std::unique_lock{s_mutex};
        _require<Deps...>(std::forward<const Callback &&>(callback));
    }

    template<typename ...Deps, typename Callback>
    void _require(const Callback &&callback)
    {
//        const auto locks = std::make_tuple(std::unique_lock{ContextAssociated<Context>::template componentReferenceMutex<Deps>, std::try_to_lock}...);
//        //std::unique_lock contextLock{ContextAssociated<Context>::componentReferenceMutex<Deps>, std::try_to_lock}...;
//        const bool ownsAllLock = std::apply([](const auto&... lock){return (lock.owns_lock() && ...);}, locks);
//        if( ownsAllLock && exists<Deps...>())
//        {
//            // avoid copying callback with this extra if
//            // if lock could not be acquired, this might be executed in the ctor of a component.
//            // in this case, execution is delayed until ctor finished.
//            ContextAssociated<Context>::template components<Deps>, locks
//            callback(require<Deps>()...);
//        }
//        else
//        {
            auto cb = [this, callback]()
            {
                if(!exists<Deps...>()) return false;
                callback(require<Deps>()...);
                return true;
            };
            if(!cb())
            {
                std::scoped_lock contextLock{ContextAssociated<Context>::checkDependenciesMutex};
                s_executeWith.emplace_back(std::make_unique<DependencyCallback<Context, Deps...>>(cb));
            }
//        }
    }

    static std::string whatsMissing()
    {
        std::scoped_lock {ContextAssociated<Context>::whatsMissingMutex};
        std::string missing{" "};
        for (auto&& notif : ContextAssociated<Context>::whatsMissing)
        {
            missing += notif();
            missing += "\n ";
        }
        if(!missing.empty())
        {
            missing.pop_back();
            missing.pop_back();
        }
        return missing;
    }
    std::string whatsMissingLocal() const
    {
        std::unordered_set<std::string> missing;
        for (const CallbackEx& exec : s_executeWith)
        {
            const auto other = exec->whatsMissing();
            missing.insert(other.cbegin(), other.cend());
        }
        std::string missingConcat;
        for (const auto &miss : missing)
        {
            missingConcat += "\"" + miss + "\"";
            missingConcat += ", ";
        }
        if(!missingConcat.empty())
        {
            missingConcat.pop_back();
            missingConcat.pop_back();
        }

        std::unordered_set<std::string> unfin;
        for (const CallbackEx& exec : s_executeWhenFinished)
        {
            const auto other = exec->whatsMissing();
            unfin.insert(other.cbegin(), other.cend());
        }
        std::string unfinishedConcat;
        for (const auto &u : unfin)
        {
            unfinishedConcat += "\"" + u + "\"";
            unfinishedConcat += ", ";
        }
        if(!unfinishedConcat.empty())
        {
            unfinishedConcat.pop_back();
            unfinishedConcat.pop_back();
        }
        return std::string{"\""} + typeid(Self).name() + "\" needs: " +  missingConcat + "; Not yet finished is: " + unfinishedConcat + ";";
    }

private:
    template<typename T>
    static std::unique_ptr<T> s_componentsOwned;

    std::vector<std::function<void()>> m_clear;
    static std::unordered_set<std::condition_variable_any*> s_pendingRequires;
    static std::vector<CallbackEx> s_executeWith;
    static std::vector<CallbackEx> s_executeWhenFinished;
//    std::vector<std::function<bool()>> m_executeWith;
//    std::vector<std::function<bool()>> m_executeWhenFinished;

    // There must only be one DependencyReactor for each combination of Context and Self
    // using static members DependencyReactor can manage itself in the application without
    // passing references.
    static bool s_exists;
    static std::mutex s_mutex;
    template<typename, typename> friend class DependencyReactor;
};

template <typename Context, typename Self>
template<typename T>
std::unique_ptr<T> DependencyReactor<Context, Self>::s_componentsOwned;

template <typename Context, typename Self>
bool DependencyReactor<Context, Self>::s_exists = false;

template <typename Context, typename Self>
std::mutex DependencyReactor<Context, Self>::s_mutex{};

template <typename Context, typename Self>
std::unordered_set<std::condition_variable_any*> DependencyReactor<Context, Self>::s_pendingRequires{};

template <typename Context, typename Self>
std::vector<CallbackEx> DependencyReactor<Context, Self>::s_executeWith{};

template <typename Context, typename Self>
std::vector<CallbackEx> DependencyReactor<Context, Self>::s_executeWhenFinished{};
//template <typename Context, typename Self>
//template<typename T>
//T* DependencyReactor<Context, Self>::components = nullptr;

//template <typename Context, typename Self>
//template<typename T>
//std::unique_ptr<T> DependencyReactor<Context, Self>::componentsOwned{};

}

#endif // DEPENDENCYREACTOR_HPP
