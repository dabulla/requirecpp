#pragma once

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
#include "decorators.hpp"

// todo: simplify by replace some mutexes by synchronized_value, semaphore

// Rules for DependencyReactor: In the destructor, own pending require calls will throw.
// Destructor will wait for ComponentReferences to unlock

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

template <typename Context, typename ...Deps>
using CallbackImpl = DependencyCallback<Context, Deps...>;

using CallbackEx = std::unique_ptr<DependencyCallbackBase>;

template<typename T, T val>
class State
{
};

template<typename T, auto state>
class Requirement
{
};

// Context can be T
// Components have a member DependencyReactor<Themself>
// With that interface executeWith<Dep1, Dep2, Dep3>(lambda) can be used.
// As soon, as all lambdas with dependencies have been executed, onFinished<T>(lambda) is invoked,
// this could happend directly after registerComponent, if all dependecies are met or if there are no dependencies.
// Whenever a component is registered, the executeWith-lambdas that have all dependencies satisfied,
// are executed. After that onFinished is checked an might be invoked.
// executeWith<Dep1, Dep2, Dep3>(lambda) can be used with executeWith<RequireFinished<Dep1>, Dep2, Dep3>(lambda), this must not introduce circular dependencies.
class Context
{
public:
    template<class T, class Fn>
    void on_required(Fn&& callback);
    template<typename T>
    auto require() -> decltype(Get<T>::get())
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
                // @todo finished, assembled
                std::scoped_lock lkCheckDeps{ContextAssociated<Context>::checkDependenciesMutex};
                s_executeWith.emplace_back(std::make_unique<DependencyCallback<Context, Deps...>>(cb));
            }
//        }
    }
//    void checkDependecies()
//    {
//        auto end1 = std::remove_if(s_executeWith.begin(), s_executeWith.end(), [](auto& cb){ return (*cb)();});
//        s_executeWith.erase(end1, s_executeWith.end());
//        auto end2 = std::remove_if(s_executeWhenFinished.begin(), s_executeWhenFinished.end(), [](auto& cb){ return (*cb)();});
//        s_executeWhenFinished.erase(end2, s_executeWhenFinished.end());
//    }
//    template<typename DecoratedT, typename ...Args,
//             typename DecoratedT::NoRegisterDecoration = 0>
//    void registerComponent(const Args&& ...args)
//    {
//        static_assert(false, "Invalid decoration for register");
//    }

//    template<typename DecoratedT, typename ...Args>
//    void registerComponent(const Args&& ...args)
//    {
//        static_assert(!std::is_same<Context, DecoratedT>(), "Context cannot be registered");
//        static_assert(!is_specialization_of<Finished, DecoratedT>(), "Do not decorate Component with Finished for registration");
//        static_assert(!is_specialization_of<Unlocked, DecoratedT>(), "Do not decorate Component with Unlocked for registration");
//        static_assert(!is_specialization_of<LockFree, DecoratedT>(), "Do not decorate Component with LockFree for registration");
//        static_assert(!is_specialization_of<Lazy, DecoratedT>(), "Do not decorate Component with Lazy for registration");

//        using T = typename std::conditional<is_specialization_of<Existing, DecoratedT>::value, typename DecoratedT::type, DecoratedT>::type;
//        std::unique_lock lk{s_mutex}; // guard s_lockdComponents
//        //std::unique_lock lk2{ContextAssociated<Context>::componentsMutex};
//        std::unique_lock lkComp{Component<Context, T>::mutex};

//        if(Exists<T>::exists())
//        {
//            throw std::runtime_error{"Called registerCompontent twice"};
//        }
//        std::unique_ptr<T> &ownedComp = DependencyReactor<Context, Self>::s_componentsOwned<T>;
//        m_clear.emplace_back(&DependencyReactor<Context, Self>::_clearComponent<DecoratedT>);
//        auto& component = Component<Context, T>::ptr;
//        if constexpr(is_specialization_of<Existing, T>())
//        {
//            // use pointer to existing component
//            std::tuple<Args...> a( args... );
//            component = std::get<T>(&a);
//        }
//        else
//        {
//            // create a unique pointer and manage its lifecycle
//            std::unique_ptr<T> &ownedComp = DependencyReactor<Context, Self>::s_componentsOwned<T>;
//            ownedComp = std::make_unique<T>(args...);
//            component = ownedComp.get();
//        }
//        lkComp.unlock();
//        Component<Context, T>::cv.notify_all();
//        std::scoped_lock lkCheckDeps{ContextAssociated<Context>::checkDependenciesMutex};
//        for (auto&& checkDep : ContextAssociated<Context>::checkDependencies)
//        {
//            checkDep();
//        }
//    }

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
                // @todo finished, assembled
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
    static std::unordered_set<std::condition_variable_any*> s_pendingRequests;

//    struct ComponentReactorHolder
//    {
//        unsigned int awaited{0}; // nneded? component has awaited and refcount only once, globally? two wait conditions for that
//        unsigned int refCount{0};
//        std::shared_mutex mMutex;
//    };
//    template<typename T>
//    static ComponentReactorHolder s_components;
    static std::vector<CallbackEx> s_executeWith;
    static std::vector<CallbackEx> s_executeWhenAssembled;
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
std::unordered_set<std::condition_variable_any*> DependencyReactor<Context, Self>::s_pendingRequests{};

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
