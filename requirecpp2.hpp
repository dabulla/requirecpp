#ifndef REQUIRECPP2_HPP
#define REQUIRECPP2_HPP

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
// if multiple components are requested, and it could be, that components deregister and reregister again later
constexpr bool g_abortExistRequests { true };

// cares about all blocking requests, but not callbacks
template<typename T>
class ComponentHolder;

}

namespace requirecpp
{
class Context
{
public:
    template<typename T>
    static ComponentHolder<T> s_components;
    template<typename T>
    static std::recursive_mutex s_componentsMutex;

    std::vector<std::reference_wrapper<std::function<void()>>> m_visitExists;
    std::vector<std::reference_wrapper<std::function<void()>>> m_visitAssembled;
    std::vector<std::reference_wrapper<std::function<void()>>> m_visitFinished;
    std::mutex m_visitMutex;
};
template<typename T>
ComponentHolder<T> Context::s_components;
template<typename T>
std::recursive_mutex Context::s_componentsMutex;


template<requirecpp::Context &context, typename Self>
class DependencyReactor;
}

//template<typename T>
//using Component = typename requirecpp::Context::template s_components<T>;

namespace
{

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

// use this to disable debugging information
//template <typename Context, typename ...Deps>
//using CallbackImpl = std::function<bool()>

//template <typename Context, typename ...Deps>
//using CallbackEx = std::function<bool()>

template <typename Context, typename ...Deps>
using CallbackImpl = DependencyCallback<Context, Deps...>;

using CallbackEx = std::unique_ptr<DependencyCallbackBase>;


template < template <typename...> class Template, typename T >
struct is_specialization_of : std::false_type {};

template < template <typename...> class Template, typename... Args >
struct is_specialization_of< Template, Template<Args...> > : std::true_type {};


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
class Decoration
{
    using BaseType = T;
};
template<typename T>
class Decoration<Decoration<T>>
{
    using BaseType = typename T::BaseType;
};

template <typename T>
struct RemoveDecoration
{
    using type = typename Decoration<T>::BaseType;
};

template<typename T>
class Existing : public Decoration<T>
{
public:
    using NoRequireDecoration = std::true_type;
};

/// Decoration for require ///

template<typename T>
class ComponentStateDecoration : public Decoration<T>
{
    //static_assert(std::is_base_of<ComponentStateDecoration, T>, "Do not Decorate with multiple Component States");
};

// all calls to require have finished and no component is used
template<typename T>
class Finished : public ComponentStateDecoration<T>
{
public:
    using NoRegisterDecoration = std::true_type;
};

// all calls to require have returned, components may still be used
template<typename T>
class Assembled : public ComponentStateDecoration<T>
{
public:
    using NoRegisterDecoration = std::true_type;
};

template<typename T>
class WaitModifyDecoration : public Decoration<T>
{
    //static_assert(std::is_base_of<ComponentStateDecoration, T>, "Do not Decorate with multiple Component States");
};
// do not block and immediatly return a component, but block on first usage of the component
template<typename T>
class Lazy : public WaitModifyDecoration<T>
{
public:
    using NoRegisterDecoration = std::true_type;
};

// Try getting a component and wait a maximum duration
// Not yet implemented
template<typename T, typename Duration, Duration duration_>
class Timed : public WaitModifyDecoration<T>
{
public:
    using NoRegisterDecoration = std::true_type;
    static constexpr Duration duration = duration_;
};

template<typename T>
class FailureDecoration : public Decoration<T>
{
    //static_assert(std::is_base_of<FailureDecoration, T>, "Do not Decorate with multiple Component States");
};

// return with a component or nothing, do not throw
// Not yet implemented
template<typename T>
class OptionalPeek : public FailureDecoration<T>
{
public:
    using NoRegisterDecoration = std::true_type;
};

template<typename T>
class LockModifyDecoration : public Decoration<T>
{
    //static_assert(std::is_base_of<LockModifyDecoration, T>, "Do not Decorate with multiple Component States");
};

// return a reference to a component or throw. This is basic ServiceLocator style
template<typename T>
class Unlocked : public LockModifyDecoration<T>
{
public:
    using NoRegisterDecoration = std::true_type;
};

// every call will lock the component by overloading the -> operator.
// If the component was destroyed, the call will throw. This should not be used,
// calls to the component will be slow and it must be expected, that every call could throw
// This is discouraged for the same reasons as lock-free programming
template<typename T>
class LockFree : public LockModifyDecoration<T>
{
public:
    using NoRegisterDecoration = std::true_type;
};


template <requirecpp::Context &context, typename ...Tail>
struct Satisfied
{
    static bool satisfied() { return true; }
};
template <requirecpp::Context &context, typename Dep, typename ...Tail>
struct Satisfied<context, Assembled<Dep>, Tail...>
{
    static bool satisfied()
    {
        //std::shared_lock lk{Component<Context, Dep>::mutex};
        //const bool fin = Component<Context, Dep>::awaited == 0;
        //lk.unlock();
        return Satisfied<context, Dep>::satisfied() && true && Satisfied<context, Tail...>::satisfied();
    }
};
template <requirecpp::Context &context, typename Dep, typename ...Tail>
struct Satisfied<context, Finished<Dep>, Tail...>
{
    static bool satisfied()
    {
//        std::shared_lock lk{Component<Context, Dep>::mutex};
//        const bool fin = Component<Context, Dep>::refCount == 0;
//        lk.unlock();
        return Satisfied<context, Dep>::satisfied() && true && Satisfied<context, Tail...>::satisfied();
    }
};

// never locks
template <requirecpp::Context &context, typename Dep, typename ...Tail>
struct Satisfied<context, Unlocked<Dep>, Tail...>
{
    static bool satisfied()
    {
        return Satisfied<context, Dep, Tail...>::satisfied();
    }
};
template <requirecpp::Context &context, typename Dep, typename ...Tail>
struct Satisfied<context, Dep, Tail...>
{
    static bool satisfied()
    {
        return nullptr != requirecpp::Context::template s_components<Dep>.ptr && Satisfied<context, Tail...>::satisfied();
    }
};

template<typename T>
class ComponentHolder
{
    T* m_ptr { nullptr };
    unsigned int m_referenceCountExist { 0 };
    unsigned int m_referenceCountAssembled { 0 };
    unsigned int m_referenceCountFinished { 0 };
    std::shared_mutex mutex;
    // notified when 0 == m_ptr changed
    std::condition_variable_any m_cvExist;
    // notified when 0 == m_referenceCountExist changed
    std::condition_variable_any m_cvAssembled;
    // notified when 0 == m_referenceCountAssembled changed
    std::condition_variable_any m_cvFinished;
    // notified when 0 == m_referenceCountFinished changed
    std::condition_variable_any m_cvDeletable;

    // used to wait for finishing or aborting component usage, when Self destructs
    bool abortRequire { false };

    // if m_ptr is set, component exists
    bool exists() { return nullptr != m_ptr; }
    // if nobody is waiting for exist, component is assembled
    bool assembled() { return 0 == m_referenceCountExist; }
    // if nobody is waiting for exist or assembled, component is finished
    bool finished() { return assembled() && 0 == m_referenceCountAssembled; }
    // if nobody is waiting for anything, component can be deleted
    bool deletable() { return finished() && 0 == m_referenceCountFinished; }

    void waitForDeletable()
    {
        std::unique_lock lk{mutex};
        // ensure, everyone returned or aborted, this request should not use the abort-flag
        m_cvDeletable.wait(lk, [&]{ return deletable();});
    }
public:
    const T*& get() { return m_ptr; }
    void registerComponent(const T* t)
    {
        std::scoped_lock lk{mutex};
        m_ptr = t;
        m_cvExist.notify_all();
    }
    void deregisterComponent()
    {
        std::scoped_lock lk{mutex};
        // signal destruction
        abortRequire = true;
        // tell threads, trying to assemble to give up
        m_cvExist.notify_all();
        m_cvAssembled.notify_all();
        waitForDeletable();
        abortRequire = false;
        m_ptr = nullptr;
    }


    // only abort if dependency reactor shuts down. Not if component is deregistered (could be reregistered).
    template<typename ...Args>
    static void waitForExist( Args&... args)
    {
        std::scoped_lock lk{args.mutex...};
        (++args.m_referenceCountExist, ...);
        do
        {
            (args.m_cvExist.wait(lk, [&]{ return (args.exists() && ...);}), ...);

            // after all components have been checked, lock all mutexes again and check another time
        } while((args.exists() && ...));
        (--args.m_referenceCountExist, ...);
        ((args.assembled() ? args.m_cvAssembled.notify_all() : 0), ...);
        if(!(args.exists() && ...)) throw std::runtime_error{"Require aborted (waitForExists)"};
    }

    //template <>
    class WaitStrategyWait
    {
    public:
        WaitStrategyWait(std::condition_variable_any &cv)
            :m_cv{cv}
        {
        }
        template<typename Predicate>
        void operator()(std::unique_lock<std::shared_mutex> &lock, Predicate p)
        {
            m_cv.wait(lock, p);
        }
        ~WaitStrategyWait()
        {
            // maybe notify_all?
        }
    private:
        std::condition_variable_any &m_cv;
    };

    // Requirement:
    // 1) required, not satisfied, blocking
    // 2) required, not satisfied, callback waiting
    // 3) required, partly satisfied, blocking
    // 4) required, partly satisfied, callback waiting
    // 4) required, satisfied, callback waiting

    template <typename WaitStrategy, typename NotifyNext>
    class _BlockingOnAcquireGuard
    {
    public:
        BlockingOnAcquireGuard(std::shared_mutex &mutex, unsigned int &refCount)
            :m_lock{mutex}
            ,m_refCount{refCount}
        {
            ++m_refCount;
        }
        ~BlockingOnAcquireGuard()
        {
            --m_refCount;
            NotifyNext();
        }
        template<typename DecoratedT, typename Self, requirecpp::Context &context, requirecpp::DependencyReactor<context, Self> &reactor>
        void waitForSatisfied()
        {
            WaitStrategy(m_lock, [&]{ return Satisfied<context, DecoratedT>::satisfied() || !reactor.s_exists;} );
        }
    private:
        std::unique_lock<std::shared_mutex> m_lock;
        unsigned int &m_refCount;
    };

    template <typename DecoratedT>
    class BlockingOnAcquireGuard : private _BlockingOnAcquireGuard<WaitStrategyWait, WaitStrategyWait>
    {
    };
    template <typename DecoratedT>
    class BlockingOnAcquireGuard : private _BlockingOnAcquireGuard<>
    {
    };

    template<requirecpp::Context &context, typename ...Dep>
    static void waitForSatisfied( Dep&... args)
    {
        std::shared_lock lk{args.mutex...};
        (++args.m_referenceCountExist, ...);
        do
        {
            (args.m_cvExist.wait(lk, [&]{ return (Satisfied<context, T>::satisfied() && ...);}), ...);

            // after all components have been checked, lock all mutexes again and check another time
        } while((Satisfied<context, T>::satisfied() && ...));
        (--args.m_referenceCountExist, ...);
        ((args.assembled() ? args.m_cvAssembled.notify_all() : 0), ...);
        if(!(args.exists() && ...)) throw std::runtime_error{"Require aborted (waitForExists)"};
    }
    void waitForAssembled()
    {
        std::unique_lock lk{mutex};
        ++m_referenceCountAssembled;
        m_cvAssembled.wait(lk, [&]{ return assembled() || abortRequire;});
        --m_referenceCountAssembled;
        if(finished())
        {
            m_cvFinished.notify_all();
        }
        if(!assembled()) throw std::runtime_error{"Require aborted (waitForAssembled)"};
    }
    void waitForFinished()
    {
        std::unique_lock lk{mutex};
        ++m_referenceCountFinished;
        m_cvFinished.wait(lk, [&]{ return finished();});
        --m_referenceCountFinished;
        if(deletable())
        {
            m_cvDeletable.notify_one();
        }
        // this will block until finish, nothing we can do. Component is used and must not be deleted.
        if(!finished()) throw std::runtime_error{"Require aborted (waitForFinished), should not happen"};
    }
};




template <requirecpp::Context &context, typename Self, typename T>
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

    // register request at component. if component destructs, abort request
    // register request at self, if self destructs, abort request
    //^todo
    struct RequestGuard
    {
        RequestGuard()
        {
//            //std::cout << "Shared lock " << std::string{typeid(T).name()} << " in " << std::string{typeid(Context).name()} << " exists: " << Exists<T>::exists() << std::endl;
//            // maintain a global register of component requests. If a context tears down with the component, all requests are aborted. TODO: not needed, call <T>.unavail = true and <T>.notify()
//            auto &awaitedContext = Component<Context, T>::awaited;
//            ++awaitedContext;
//            // maintain a register of component requests per Self to know if Self is assembled.
//            if(++DependencyReactor<Context, Self>::template s_awaited<T> != 1) return;
//            DependencyReactor<Context, Self>::template s_pendingRequests.emplace(&ContextAssociated<Context>::template componentReferenceCondition<T>); // TODO: Move out of DependencyReactor class?
        }
        ~RequestGuard()
        {
//            auto &awaitedContext = Component<Context, T>::awaited;
//            --awaitedContext;
//            if(--DependencyReactor<Context, Self>::template s_awaited<T> == 1)
//            {
//                DependencyReactor<Context, Self>::template s_pendingRequests.erase(&ContextAssociated<Context>::template componentReferenceCondition<T>);
//                Component<Context, T>::cvBack.notify_all(); // notify clear
//            }
        }
    };
    // Guard Component while it is used or requested and not yet available (if non exist, Self is finished)
    // register usage at component, do not destruct component during usage
    // do not register usage at self, if self destructs during usage (something strange happens, but it should work)
    template <typename Context, typename Self2, typename T>
    struct UsageGuard
    {
        T& ref;
        UsageGuard(T& ref)
            :ref{ref}
        {
//            std::scoped_lock lk {DependencyReactor<Context, Self>::template s_components<T>::mutex};
//            ++DependencyReactor<Context, Self>::template s_components<T>::refCount;
        }
        ~UsageGuard()
        {
//            std::scoped_lock lk {DependencyReactor<Context, Self>::template s_components<T>::mutex};
//            --DependencyReactor<Context, Self>::template s_components<T>::refCount;
        }
    };



//    Requirement()
//        :m_lock{Component<Context, T>::mutex, std::defer_lock}
//    {
//    }
//    static bool satisfied()
//    {
//        //std::cout << "in Wait " << std::string{typeid(T).name()} << " in " << std::string{typeid(Context).name()} << " exists: " << Exists<T>::exists() << " unav: " <<  ContextAssociated<Context>::template unavailable<T> << " awaited " << ContextAssociated<Context>::template awaited<T> << std::endl;
//        return Satisfied<Context, T>::satisfied() || Component<Context, T>::unavailable || Component<Context, Self>::unavailable || !DependencyReactor<Context, Self>::s_exists;
//    }
//    UsageGuard wait()
//    {
//        m_lock.lock();
//        RequestGuard guard{};
//        ContextAssociated<Context>::template componentReferenceCondition<T>.wait(m_lock, &Requirement::satisfied);
//        const bool componentUnavailable = Component<Context, T>::unavailable; // store result before notifying clear
//        const bool selfUnavailable = Component<Context, Self>::unavailable;
//        const bool selfContainerExists = !DependencyReactor<Context, Self>::s_exists;
//        if(componentUnavailable) throw std::runtime_error{"Component was never registered (component shutdown)"};
//        if(selfUnavailable) throw std::runtime_error{"Component was never registered (self shutdown)"};
//        if(!selfContainerExists) throw std::runtime_error{"Component is beeing destructed (self shutdown)"};
//        return {Get<Unlocked<T>>::get()};
//    }
};

}

namespace requirecpp {

// not virtual!
template<Context &context, typename Self, typename T>
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
        //std::scoped_lock lk {DependencyReactor<Context, Self>::s_lockedComponentsMutex};
        //DependencyReactor<Context, Self>::s_lockedComponents.insert(&m_lock);
    }
    ~ComponentReference()
    {
        //unlock();
    }
    ComponentReference(const ComponentReference& other) = delete; // no copy
    ComponentReference(ComponentReference&& other) noexcept
        :m_t{other.m_t},
         m_lock{std::move(other.m_lock)}
    {};
    ComponentReference& operator=(const ComponentReference& other) = delete;  // no copy assign
    ComponentReference& operator=(ComponentReference&& other) noexcept = delete;  // no move assign
//    void unlock()
//    {
//        std::scoped_lock lk {DependencyReactor<Context, Self>::s_lockedComponentsMutex};
//        DependencyReactor<Context, Self>::s_lockedComponents.erase(&m_lock);
//        m_lock.unlock();
//    }
//protected:
//    void _lock()
//    {
//        m_lock.lock();
//    }
};

template<Context &context, typename Self>
class DependencyReactor
{
public:
    DependencyReactor()
    {
        std::unique_lock lk{s_mutex};
        if(s_exists) throw std::runtime_error{"Only one DependencyReactor per class allowed"};
        s_exists = true;
        lk.unlock();
//        std::scoped_lock lkCheckDeps{ContextAssociated<Context>::checkDependenciesMutex};
//        ContextAssociated<Context>::checkDependencies.emplace_back(std::bind(&DependencyReactor<Context, Self>::checkDependecies, this));

//        std::scoped_lock lkMiss{ContextAssociated<Context>::whatsMissingMutex};
//        ContextAssociated<Context>::whatsMissing.emplace_back(std::bind(&DependencyReactor<Context, Self>::whatsMissingLocal, this));
    }
    ~DependencyReactor()
    {
        std::unique_lock lk{s_mutex};
        s_exists = false; // in clean, all blocking require() calls have to return
        for (auto& cb : m_visitExists)
        {
            std::cout << "aborted require" << std::endl;
            cb();
            //cv->notify_all();
        }
        m_visitExists.clear();
//        for (auto&& clear : m_clear)
//        {
//            clear();
//        }
    }

    template<typename T>
    static void _clearComponent()
    {
        auto& component = Context::template s_components<T>;
        component.deregister();
        if constexpr(!is_specialization_of<Existing, T>())
        {
            std::unique_ptr<T> &ownedComp = DependencyReactor<context, Self>::s_componentsOwned<T>;
            ownedComp.reset();
        }
    }

    template<typename DecoratedT, typename ...Args,
             typename DecoratedT::NoRegisterDecoration = 0>
    void registerComponent(const Args&& ...args)
    {
        static_assert(false, "Invalid decoration for register");
    }

    template<typename DecoratedT, typename ...Args>
    void registerComponent(const Args&& ...args)
    {
        static_assert(!is_specialization_of<Finished, DecoratedT>(), "Do not decorate Component with Finished for registration");
        static_assert(!is_specialization_of<Unlocked, DecoratedT>(), "Do not decorate Component with Unlocked for registration");
        static_assert(!is_specialization_of<LockFree, DecoratedT>(), "Do not decorate Component with LockFree for registration");
        static_assert(!is_specialization_of<Lazy, DecoratedT>(), "Do not decorate Component with Lazy for registration");

        using T = typename std::conditional<is_specialization_of<Existing, DecoratedT>::value, typename DecoratedT::type, DecoratedT>::type;
        std::scoped_lock lk{s_mutex};

        if(Satisfied<context, Existing<T>>::satisfied())
        {
            throw std::runtime_error{"Called registerCompontent twice"};
        }
        std::unique_ptr<T> &ownedComp = DependencyReactor<context, Self>::s_componentsOwned<T>;
        m_clear.emplace_back(&DependencyReactor<context, Self>::_clearComponent<DecoratedT>);
        const T* component;
        if constexpr(is_specialization_of<Existing, DecoratedT>())
        {
            // use pointer to existing component
            std::tuple<Args...> a( args... );
            component = std::get<T>(&a);
        }
        else
        {
            // create a unique pointer and manage its lifecycle
            ownedComp = std::make_unique<T>(args...);
            component = ownedComp.get();
        }
        Context::s_components<T>.set(component);

        lkComp.unlock();
        Component<Context, T>::cv.notify_all();
        std::scoped_lock lkCheckDeps{ContextAssociated<Context>::checkDependenciesMutex};
        for (auto&& checkDep : ContextAssociated<Context>::checkDependencies)
        {
            checkDep();
        }
    }

private:
    std::unordered_set<std::condition_variable_any*> m_pendingRequests;
    std::unordered_set<std::condition_variable_any*> m_pendingUsages;

    std::vector<CallbackEx> m_visitExists;
    std::vector<CallbackEx> m_visitAssembled;
    std::vector<CallbackEx> m_visitFinished;
    static std::mutex s_mutex;
    static bool s_exists;

    template<typename T>
    static std::unique_ptr<T> s_componentsOwned;

    std::vector<std::function<void()>> m_clear;
};

template <Context &context, typename Self>
template<typename T>
std::unique_ptr<T> DependencyReactor<context, Self>::s_componentsOwned;

}
#endif // REQUIRECPP2_HPP
