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
// todo: simlify by replace some mutexes by synchronized_value, semaphore

// Rules for DependencyReactor: In the destructor, own pending require calls will throw.
// Destructor will wait for ComponentReferences to unlock

template<typename T>
struct ComponentHolder
{
    T* ptr;
    bool unavailable { false };
    std::shared_mutex mutex;
    // count calls to require, that are not yet satisfied. For destruction of the component, this must be zero.
    unsigned int awaited { 0 };
    // wait for component creation using this cv in require calls
    static std::condition_variable_any cv;
    // used to wait for finishing or aborting component usage, when Self destructs
    static std::condition_variable_any cvBack;
};

template<typename Context>
struct ContextAssociated
{
    /**
     * Container for Components. Components are owned by this container and only exist once here.
     * std::unique_ptr allows usage of incomplete types.
     */
//    template<typename T>
//    static T* components;

    template<typename T>
    static ComponentHolder<T> components;

//    template<typename T>
//    static std::shared_mutex componentReferenceMutex;
//    template<typename T>
//    static std::condition_variable_any componentReferenceCondition;
//    template<typename T>
//    static std::condition_variable_any componentReferenceConditionBack;

//    // flag to tell condition variable to stop waiting. Used when the required component is being decomposed
//    template<typename T>
//    static bool unavailable;
//    // number of blocking require calls for this component
//    template<typename T>
//    static unsigned int awaited;
    // this should only prevent threading problems but allow registerComponent to recurse
    // (in ctor of Component another Component can be registered)
    static std::recursive_mutex componentsMutex;

    static std::vector<std::function<void()>> checkDependencies;
    // not shared, because each must only be executed once
    static std::mutex checkDependenciesMutex;

    static std::vector<std::function<std::string()>> whatsMissing;
    static std::mutex whatsMissingMutex;

//    // NYI: not needed, if root context survives all children, advanced error detection
//    // Values indicating if the root/main context is beeing created/destroyed.
//    // this is needed to finally stop all pending blocked calls to require.
//    static bool s_exists;
//    static std::mutex s_existsMutex;

//    // NYI: not needed, if root context survives all children, advanced error detection
//    // stop all pending requires that have never been statisfied
//    // when closing root container
//    static std::vector<void()> s_notifyKill;
};

template<typename Context, typename T>
using Component = typename ContextAssociated<Context>::template components<T>;

template<typename Context>
template<typename T>
ComponentHolder<T> ContextAssociated<Context>::components;
//template<typename Context>
//template<typename T>
//T* ContextAssociated<Context>::components;
//template<typename Context>
//template<typename T>
//std::shared_mutex ContextAssociated<Context>::componentReferenceMutex;
//template<typename Context>
//template<typename T>
//std::condition_variable_any ContextAssociated<Context>::componentReferenceCondition;
//template<typename Context>
//template<typename T>
//std::condition_variable_any ContextAssociated<Context>::componentReferenceConditionBack;

//template<typename Context>
//template<typename T>
//bool ContextAssociated<Context>::unavailable {false};

//template<typename Context>
//template<typename T>
//unsigned int ContextAssociated<Context>::awaited {0};

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

//template<typename Context>
//bool ContextAssociated<Context>::s_exists;

//template<typename Context>
//std::mutex ContextAssociated<Context>::s_existsMutex;

//template<typename Context>
//std::vector<void()> ContextAssociated<Context>::s_notifyKill;

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
//template<typename T>
//class Reregisterable //: public T
//{
//public:
//    using type = T;
//    //operator T() const { return *this; }
//};
template<typename T>
class Existing //: public T
{
public:
    using type = T;
    //operator T() const { return *this; }
};

/// Decoration for require ///

// all calls to require have finished and no component is used
template<typename T>
class Finished //: public T
{
public:
    using type = T;
    //operator T() const { return *this; }
};

// all calls to require have returned, components may still be used
template<typename T>
class Assembled //: public T
{
public:
    using type = T;
    //operator T() const { return *this; }
};

// do not block and immediatly return a component, but block on first usage of the component
template<typename T>
class Lazy //: public T
{
public:
    using type = T;
    //operator T() const { return *this; }
};

// return a reference to a component or throw. This is basic ServiceLocator style
template<typename T>
class Unlocked //: public T
{
public:
    using type = T;
    //operator T() const { return *this; }
};

// every call will lock the component by overloading the -> operator.
// If the component was destroyed, the call will throw. This should not be used,
// calls to the component will be slow and it must be expected, that every call could throw
// This is discouraged for the same reasons as lock-free programming
template<typename T>
class LockFree //: public T
{
public:
    using type = T;
    //operator T() const { return *this; }
};

// immediately return with a component or nothing
// Not yet implemented
template<typename T>
class OptionalPeek //: public T
{
public:
    using type = T;
    //operator T() const { return *this; }
};

// Try getting a component and wait a maximum duration
// Not yet implemented
template<typename T, typename Duration, Duration duration_>
class Timed //: public T
{
public:
    using type = T;
    static constexpr Duration duration = duration_;
    //operator T() const { return *this; }
};

template <typename Context, typename Self>
class DependencyReactor;

template<typename Context, typename Self, typename T>
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
    {
        std::scoped_lock lk {DependencyReactor<Context, Self>::s_lockedComponentsMutex};
        DependencyReactor<Context, Self>::s_lockedComponents.insert(&m_lock);
    }
    ~ComponentReference()
    {
        unlock();
    }
    ComponentReference(const ComponentReference& other) = delete; // no copy
    ComponentReference(ComponentReference&& other) noexcept
        :m_t{other.m_t},
         m_lock{std::move(other.m_lock)}
    {};
    ComponentReference& operator=(const ComponentReference& other) = delete;  // no copy assign
    ComponentReference& operator=(ComponentReference&& other) noexcept = delete;  // no move assign
    void unlock()
    {
        std::scoped_lock lk {DependencyReactor<Context, Self>::s_lockedComponentsMutex};
        DependencyReactor<Context, Self>::s_lockedComponents.erase(&m_lock);
        m_lock.unlock();
    }
protected:
    void _lock()
    {
        m_lock.lock();
    }
};

template<typename Context, typename Self, typename T>
class ComponentReference<Context, Self, std::optional<std::reference_wrapper<T>>> : public std::optional<std::reference_wrapper<T>>
{};
template<typename Context, typename Self, typename T>
using ComponentOptionalReference = ComponentReference<Context, Self, std::optional<std::reference_wrapper<T>>>;

template<typename Context, typename Self, typename T>
class ComponentLazyReference
{
    std::optional<const ComponentReference<Context, Self, T>> m_t;
    void get()
    {
        if(!m_t.has_value())
            m_t.emplace(std::move(DependencyReactor<Context, Self>::template Get<T>::get()));
            //m_t = std::move(std::make_optional<ComponentReference<Context, Self, T>>(std::move(DependencyReactor<Context, Self>::template Get<T>::get())));
    }
public:
    using type = T;
    operator T() { get(); return m_t.value(); }
    operator T() const { get(); return m_t.value(); }
    T* operator->() { get(); return m_t.value().operator->(); }
    T* operator->() const { get(); return m_t.value().operator->();  }
    T& operator* () { get(); return m_t.value().operator*(); }
    T& operator* () const { get(); return m_t.value().operator*(); }
    ComponentLazyReference()
    {

    }
    ~ComponentLazyReference() = default;
    ComponentLazyReference(const ComponentLazyReference& other) = delete; // no copy
    ComponentLazyReference(ComponentLazyReference&& other) noexcept = delete; // no move
    ComponentLazyReference& operator=(const ComponentLazyReference& other) = delete;  // no copy assign
    ComponentLazyReference& operator=(ComponentLazyReference&& other) noexcept = delete;  // no move assign
    void unlock()
    {
        if(m_t.has_value()) m_t.unlock();
    }
};

template <typename Context, typename Self, typename T>
struct Requirement
{
    // lock mutex, happens in construction of ComponentReference or lazy access
    // mutex will be unlocked in conditionvariable.wait. Blocking
//        void markAwaiting();

//        void markSatisfied();
    std::shared_lock<std::shared_mutex> m_lock;
    // Pending requests: component not finished
    // all requests satisfied: component Dependencies satisfied/ Component Assembled
    // all ComonentReferences dropped: Component finished
    //how hand lock from guard to comp ref? How count pending/satisfied?
    // Guard Request while its pending (if non exist, Self is assembled)
    //template <typename Context, typename Self, typename T>
    struct RequestGuard
    {
        RequestGuard()
        {
            //std::cout << "Shared lock " << std::string{typeid(T).name()} << " in " << std::string{typeid(Context).name()} << " exists: " << Exists<T>::exists() << std::endl;
            // maintain a global register of component requests. If a context tears down with the component, all requests are aborted. TODO: not needed, call <T>.unavail = true and <T>.notify()
            auto &awaitedContext = Component<Context, T>::awaited;
            ++awaitedContext;
            // maintain a register of component requests per Self to know if Self is assembled.
            if(++DependencyReactor<Context, Self>::template s_awaited<T> != 1) return;
            DependencyReactor<Context, Self>::template s_pendingRequires.emplace(&ContextAssociated<Context>::template componentReferenceCondition<T>); // TODO: Move out of DependencyReactor class?
        }
        ~RequestGuard()
        {
            auto &awaitedContext = Component<Context, T>::awaited;
            --awaitedContext;
            if(--DependencyReactor<Context, Self>::template s_awaited<T> == 1)
            {
                DependencyReactor<Context, Self>::template s_pendingRequires.erase(&ContextAssociated<Context>::template componentReferenceCondition<T>);
                Component<Context, T>::cvBack.notify_all(); // notify clear
            }
        }
    };
    // Guard Component while it is used or requested and not yet available (if non exist, Self is finished)
    //template <typename Context, typename Self, typename T>
    struct UsageGuard
    {
        UsageGuard()
        {
            std::scoped_lock lk {DependencyReactor<Context, Self>::template s_components<T>::mutex};
            ++DependencyReactor<Context, Self>::template s_components<T>::refCount;
        }
        ~UsageGuard()
        {
            std::scoped_lock lk {DependencyReactor<Context, Self>::template s_components<T>::mutex};
            --DependencyReactor<Context, Self>::template s_components<T>::refCount;
        }
    };



    Requirement()
        :m_lock{Component<Context, T>::mutex, std::defer_lock}
    {
    }
    static bool satisfied()
    {
        std::cout << "in Wait " << std::string{typeid(T).name()} << " in " << std::string{typeid(Context).name()} << " exists: " << Exists<T>::exists() << " unav: " <<  ContextAssociated<Context>::template unavailable<T> << " awaited " << ContextAssociated<Context>::template awaited<T> << std::endl;
        return Exists<T>::exists() || ContextAssociated<Context>::template unavailable<T> || !s_exists;
    }
    void wait()
    {
        m_lock.lock();
        RequestGuard guard{};
        ContextAssociated<Context>::template componentReferenceCondition<T>.wait(m_lock, &Requirement::satisfied);
        const bool componentAvailable = ContextAssociated<Context>::template unavailable<T>; // store result before notifying clear
        const bool exists = s_exists;
        if(!componentAvailable) throw std::runtime_error{"Component was never registered (component shutdown)"};
        if(!exists) throw std::runtime_error{"Component was never registered (self shutdown)"};
    }
};


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
        std::unique_lock lk{s_mutex};
        if(s_exists) throw std::runtime_error{"Only one DependencyReactor per class allowed"};
        s_exists = true;
        //if constexpr(std::is_same<Context, Self>()) ContextAssociated<Context>::s_exists = true;
        lk.unlock();
        std::scoped_lock lkCheckDeps{ContextAssociated<Context>::checkDependenciesMutex};
        ContextAssociated<Context>::checkDependencies.emplace_back(std::bind(&DependencyReactor<Context, Self>::checkDependecies, this));

        std::scoped_lock lkMiss{ContextAssociated<Context>::whatsMissingMutex};
        ContextAssociated<Context>::whatsMissing.emplace_back(std::bind(&DependencyReactor<Context, Self>::whatsMissingLocal, this));
    }
    // 1) unset exists flag not signal "abort" to all require calls
    // 2) go through all own pending blocking require calls and let them throw
    ~DependencyReactor()
    {
        std::unique_lock lk{s_mutex};
        //std::unique_lock {ContextAssociated<Context>::componentsMutex};
        s_exists = false; // in clean, all blocking require() calls have to return
        //if constexpr(std::is_same<Context, Self>()) ContextAssociated<Context>::s_exists = false;
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
        std::scoped_lock lkCheckDeps{ContextAssociated<Context>::checkDependenciesMutex};
        for (auto&& checkDep : ContextAssociated<Context>::checkDependencies)
        {
            checkDep();
        }
    }

    template<typename T>
    static void clearComponent()
    {
        auto& component = ContextAssociated<Context>::template components<T>;
        std::unique_lock lkComp{ContextAssociated<Context>::template componentReferenceMutex<T>};
        // wait for all
        ContextAssociated<Context>::template unavailable<T> = true;
        std::cout << "before Notify " << std::string{typeid(T).name()} << " in " << std::string{typeid(Context).name()} << " exists: " << Exists<T>::exists() << " unav: " <<  ContextAssociated<Context>::template unavailable<T> << " awaited " << ContextAssociated<Context>::template awaited<T> << std::endl;
        ContextAssociated<Context>::template componentReferenceCondition<T>.notify_all();
        ContextAssociated<Context>::template componentReferenceConditionBack<T>.wait(lkComp, []()
        {
            return 0 == ContextAssociated<Context>::template awaited<T>;
        });
        std::cout << "after pingback Notify " << std::string{typeid(T).name()} << " in " << std::string{typeid(Context).name()} << " exists: " << Exists<T>::exists() << " unav: " <<  ContextAssociated<Context>::template unavailable<T> << " awaited " << ContextAssociated<Context>::template awaited<T> << std::endl;
        component = nullptr; // breakpoint here and MinGW causes race condition
        if constexpr(!is_specialization_of<Existing, T>())
        {
            std::unique_ptr<T> &ownedComp = DependencyReactor<Context, Self>::s_componentsOwned<T>;
            ownedComp.reset();
        }
        ContextAssociated<Context>::template unavailable<T> = false; // reregisterable
    }

    template<typename DecoratedT, typename ...Args>
    void registerComponent(const Args&& ...args)
    {
        using T = typename std::conditional<is_specialization_of<Existing, DecoratedT>::value, typename DecoratedT::type, DecoratedT>::type;
        std::unique_lock lk{s_mutex}; // guard s_lockdComponents
        //std::unique_lock lk2{ContextAssociated<Context>::componentsMutex};
        std::unique_lock lkComp{ContextAssociated<Context>::template componentReferenceMutex<T>};
        checkRegisterPreconditions<T>();
        std::unique_ptr<T> &ownedComp = DependencyReactor<Context, Self>::s_componentsOwned<T>;
        m_clear.emplace_back(&DependencyReactor<Context, Self>::clearComponent<DecoratedT>);
        auto& component = ContextAssociated<Context>::template components<T>;
        if constexpr(is_specialization_of<Existing, T>())
        {
            // use pointer to existing component
            std::tuple<Args...> a( args... );
            component = std::get<T>(&a);
        }
        else
        {
            // create a unique pointer and manage its lifecycle
            std::unique_ptr<T> &ownedComp = DependencyReactor<Context, Self>::s_componentsOwned<T>;
            ownedComp = std::make_unique<T>(args...);
            component = ownedComp.get();
        }
        lkComp.unlock();
        componentAdded<T>();
    }

    /**
     *  register compontent of type T. Args are forwarded to construct T
     */
//    template<typename T, typename ...Args>
//    void createComponent(const Args&& ...args)
//    {
//        std::unique_lock lk{s_mutex}; // guard s_lockdComponents
//        //std::unique_lock lk2{ContextAssociated<Context>::componentsMutex};
//        std::unique_lock lkComp{ContextAssociated<Context>::template componentReferenceMutex<T>};
//        checkRegisterPreconditions<T>();
//        std::unique_ptr<T> &ownedComp = DependencyReactor<Context, Self>::s_componentsOwned<T>;
//        auto& component = ContextAssociated<Context>::template components<T>;
//        m_clear.emplace_back(&DependencyReactor<Context, Self>::clearComponent<T>);
//        ownedComp = std::make_unique<T>(args...);
//        ContextAssociated<Context>::template components<T> = ownedComp.get();
//        lkComp.unlock();
//        componentAdded<T>();
//    }
//    template<typename T>
//    void registerExistingComponent(T& comp)
//    {
//        //std::unique_lock lk{s_mutex};
//        //std::unique_lock lk2{ContextAssociated<Context>::componentsMutex};
//        std::unique_lock lkComp{ContextAssociated<Context>::template componentReferenceMutex<T>};
//        checkRegisterPreconditions<T>();
//        auto& component = ContextAssociated<Context>::template components<T>;
//        m_clear.emplace_back([&component]()
//        {
//            // s_exists is now false, condition variable can terminate
//            std::unique_lock lkComp{ContextAssociated<Context>::template componentReferenceMutex<T>};
//            ContextAssociated<Context>::template unavailable<T> = true;
//            std::cout << "before Notify " << std::string{typeid(T).name()} << " in " << std::string{typeid(Context).name()} << " exists: " << Exists<T>::exists() << " unav: " <<  ContextAssociated<Context>::template unavailable<T> << " awaited " << ContextAssociated<Context>::template awaited<T> << std::endl;
//            ContextAssociated<Context>::template componentReferenceCondition<T>.notify_all();
//            ContextAssociated<Context>::template componentReferenceConditionBack<T>.wait(lkComp, []()
//            {
//                return 0 == ContextAssociated<Context>::template awaited<T>;
//            });
//            std::cout << "after pingback Notify " << std::string{typeid(T).name()} << " in " << std::string{typeid(Context).name()} << " exists: " << Exists<T>::exists() << " unav: " <<  ContextAssociated<Context>::template unavailable<T> << " awaited " << ContextAssociated<Context>::template awaited<T> << std::endl;
//            component = nullptr;
//            // let blocking calls to require wait for reregistering this component
//            ContextAssociated<Context>::template unavailable<T> = false; // reregisterable
//        });
//        component = &comp;
//        lkComp.unlock();
//        componentAdded<T>();
//    }


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
            std::shared_lock lk{DependencyReactor<Context, Dep>::s_lockedComponentsMutex};
            const bool fin = DependencyReactor<Context, Dep>::s_lockedComponents.empty();
            lk.unlock();
            return Exists<Dep>::exists() && fin && Exists<Tail...>::exists();
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

    // Acuire shared lock in ctor, unlocked.

    template <typename T>
    struct Get
    {
//        // fast, for internal usage, assume component exists, else throw
//        static ComponentReference<Context, Self, T> get()
//        {
//            std::shared_lock lkComp{ContextAssociated<Context>::template componentReferenceMutex<T>};
//            return {getUnlocked(), std::move(lkComp)};
//        }
//        static ComponentReference<Context, Self, T> getSync()
        // check existance in wait condition
        static ComponentReference<Context, Self, T> get()
        {
            //split this monster up, list of locks and waitconditions combined? Lock without get
            std::unique_lock lk{s_mutex}; // guard s_pendingRequires (could be tighter)
            static_assert(!std::is_same<Self, T>(), "Don't require Self");
            std::shared_lock lkComp{ContextAssociated<Context>::template componentReferenceMutex<T>};
            //std::cout << "Shared lock " << std::string{typeid(T).name()} << " in " << std::string{typeid(Context).name()} << " exists: " << Exists<T>::exists() << std::endl;
            ContextAssociated<Context>::template awaited<T>++;
            if(ContextAssociated<Context>::template awaited<T> == 1)
            {
                s_pendingRequires.emplace(&ContextAssociated<Context>::template componentReferenceCondition<T>);
                //s_pendingRequires.emplace(&ContextAssociated<Context>::template componentReferenceConditionBack<T>);
            }
            auto decrementAwait = [](void*){
                ContextAssociated<Context>::template awaited<T>--;
                if(ContextAssociated<Context>::template awaited<T> == 0)
                {
                    //s_pendingRequires.erase(&ContextAssociated<Context>::template componentReferenceConditionBack<T>);
                    s_pendingRequires.erase(&ContextAssociated<Context>::template componentReferenceCondition<T>);
                }
                ContextAssociated<Context>::template componentReferenceConditionBack<T>.notify_all(); // notify clear
            };
            std::unique_ptr<void, decltype(decrementAwait)> awaitGuard{(void*)1, decrementAwait };
            ContextAssociated<Context>::template componentReferenceCondition<T>.wait(lkComp, []
            {
                std::cout << "in Wait " << std::string{typeid(T).name()} << " in " << std::string{typeid(Context).name()} << " exists: " << Exists<T>::exists() << " unav: " <<  ContextAssociated<Context>::template unavailable<T> << " awaited " << ContextAssociated<Context>::template awaited<T> << std::endl;
                return Exists<T>::exists() || ContextAssociated<Context>::template unavailable<T> || !s_exists;
            });
            const bool componentAvailable = ContextAssociated<Context>::template unavailable<T>; // store result before notifying clear
            const bool exists = s_exists;
            awaitGuard.reset(); // decrement await
            if(componentAvailable) throw std::runtime_error{"Component was never registered (component shutdown)"};
            if(!exists) throw std::runtime_error{"Component was never registered (self shutdown)"};
            return {Get<Unlocked<T>>::get(), std::move(lkComp)};
        }
        template <typename DurationType>
        static ComponentReference<Context, Self, T> timedGet(const DurationType &dur)
        {
            std::shared_lock lkComp{ContextAssociated<Context>::template componentReferenceMutex<T>};
            if(!ContextAssociated<Context>::template componentReferenceCondition<T>.wait_for(lkComp, dur, []
            {
                return Exists<T>::exists() || ContextAssociated<Context>::template unavailable<T>;
            }))
            {
                throw std::runtime_error{"Component timeout"};
            }
            return {Get<Unlocked<T>>::get(), std::move(lkComp)};
        }
    };
    template <typename T>
    struct Get<Unlocked<T>>
    {
        // just throw if component does not exist
        static T& get()
        {
            T* &comp = ContextAssociated<Context>::template components<T>;
            if(nullptr == comp) throw std::runtime_error{"Component Unknown"};
            return *comp;
        }
    };
    // just remove Finished, independent of order
    template <typename T>
    struct Get<Finished<T>>
    {
        static auto get() { return Get<T>::get(); }
    };
    // If Unlocked is first, remove Finished. Other combination works automatically
    template <typename T>
    struct Get<Unlocked<Finished<T>>>
    {
        static T& get() { return Get<Unlocked<T>>::get(); }
    };
    template <typename T>
    struct Get<Lazy<T>>
    {
        static ComponentLazyReference<Context, Self, T> get()
        {
            return ComponentLazyReference<Context, Self, T>{};
        }
    };
    template <typename T>
    struct Get<Lazy<Finished<T>>>
    {
        static ComponentLazyReference<Context, Self, T> get()
        {
            return Get<Lazy<T>>::get();
        }
    };

    // Locked/Unlocked can be specified: unlocked is a reference. locked is not
    // callbacks can get a reference to a locked
    // while Finished is omitted in the callback, Unlocked is not. It can be deduced using auto, however, the lambda must dereference (use -> or *)
    template<typename T>
    static auto require() -> decltype(Get<T>::get())
    {
        // Todo: dont use blocking require/seyncGet in construction of createComponent
        //std::unique_lock lk2{ContextAssociated<Context>::componentsMutex};
        return Get<T>::get();
    }

    template<typename ...Deps, typename Callback>
    void require(const Callback &&callback)
    {
        //std::unique_lock lk{s_mutex};
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
                std::scoped_lock lkCheckDeps{ContextAssociated<Context>::checkDependenciesMutex};
                s_executeWith.emplace_back(std::make_unique<DependencyCallback<Context, Deps...>>(cb));
            }
//        }
    }

    static std::string whatsMissing()
    {
        std::scoped_lock lkMiss{ContextAssociated<Context>::whatsMissingMutex};
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
    // needed for destruction. Unsatisfied requests are discarded and throw to their blocking require() call.
    static std::unordered_set<std::condition_variable_any*> s_pendingRequires;

    struct ComponentReactorHolder
    {
        unsigned int awaited{0};
        unsigned int refCount{0};
        std::shared_mutex mMutex;
    };
    template<typename T>
    static ComponentReactorHolder s_components;
    static std::vector<CallbackEx> s_executeWith;
    static std::vector<CallbackEx> s_executeWhenFinished;
//    std::vector<std::function<bool()>> m_executeWith;
//    std::vector<std::function<bool()>> m_executeWhenFinished;

//    static site_t hash(const std::reference_wrapper<std::shared_lock<std::shared_mutex>>& o )
//    {

//    }
//    static std::shared_mutex s_lockedComponentsMutex;
//    static std::unordered_set<std::shared_lock<std::shared_mutex>*> s_lockedComponents;

    // There must only be one DependencyReactor for each combination of Context and Self
    // using static members DependencyReactor can manage itself in the application without
    // passing references.
    static bool s_exists;
    static std::mutex s_mutex;
    template<typename, typename> friend class DependencyReactor;
    template<typename, typename, typename> friend class ComponentReference;
    template<typename, typename, typename> friend class ComponentLazyReference;
};

template <typename Context, typename Self>
template<typename T>
std::unique_ptr<T> DependencyReactor<Context, Self>::s_componentsOwned;

//template <typename Context, typename Self>
//std::shared_mutex DependencyReactor<Context, Self>::s_lockedComponentsMutex;
//template <typename Context, typename Self>
//std::unordered_set<std::shared_lock<std::shared_mutex>*> DependencyReactor<Context, Self>::s_lockedComponents;

template <typename Context, typename Self>
bool DependencyReactor<Context, Self>::s_exists = false;

template <typename Context, typename Self>
std::mutex DependencyReactor<Context, Self>::s_mutex{};

template <typename Context, typename Self>
std::unordered_set<std::condition_variable_any*> DependencyReactor<Context, Self>::s_pendingRequires{};

template <typename Context, typename Self>
template<typename T>
typename DependencyReactor<Context, Self>::ComponentReactorHolder DependencyReactor<Context, Self>::s_components;
//template <typename Context, typename Self>
//template<typename T>
//unsigned int DependencyReactor<Context, Self>::s_awaited{0};

//template <typename Context, typename Self>
//template<typename T>
//unsigned int DependencyReactor<Context, Self>::s_refCount{0};

//template <typename Context, typename Self>
//template<typename T>
//std::shared_mutex DependencyReactor<Context, Self>::s_refCountMutex{0};

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
