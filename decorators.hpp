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

namespace requirecpp::decorator {

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
//template <typename T>
//struct RemoveDecoration
//{
//    using type = typename Decoration<T>::BaseType;
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


//template<typename T>
//class Existing : public Decoration<T>
//{
//public:
//    using NoRequireDecoration = std::true_type;
//};

/// Decoration for require ///

template<typename T, auto ...State>
class StateDecoration : public Decoration<T>
{
    //static_assert(std::is_base_of<ComponentStateDecoration, T>, "Do not Decorate with multiple Component States");
};

//template<typename T>
//class WaitModifyDecoration : public Decoration<T>
//{
//    //static_assert(std::is_base_of<ComponentStateDecoration, T>, "Do not Decorate with multiple Component States");
//};
//// do not block and immediatly return a component, but block on first usage of the component
//template<typename T>
//class Lazy : public WaitModifyDecoration<T>
//{
//public:
//    using NoRegisterDecoration = std::true_type;
//};

//// Try getting a component and wait a maximum duration
//// Not yet implemented
//template<typename T, typename Duration, Duration duration_>
//class Timed : public WaitModifyDecoration<T>
//{
//public:
//    using NoRegisterDecoration = std::true_type;
//    static constexpr Duration duration = duration_;
//};

//template<typename T>
//class FailureDecoration : public Decoration<T>
//{
//    //static_assert(std::is_base_of<FailureDecoration, T>, "Do not Decorate with multiple Component States");
//};

// return with a component or nothing, do not throw
// Not yet implemented
//template<typename T>
//class OptionalPeek : public FailureDecoration<T>
//{
//public:
//    using NoRegisterDecoration = std::true_type;
//};

//template<typename T>
//class LockModifyDecoration : public Decoration<T>
//{
//    //static_assert(std::is_base_of<LockModifyDecoration, T>, "Do not Decorate with multiple Component States");
//};

//// return a reference to a component or throw. This is basic ServiceLocator style
//template<typename T>
//class Unlocked : public LockModifyDecoration<T>
//{
//public:
//    using NoRegisterDecoration = std::true_type;
//};

//// every call will lock the component by overloading the -> operator.
//// If the component was destroyed, the call will throw. This should not be used,
//// calls to the component will be slow and it must be expected, that every call could throw
//// This is discouraged for the same reasons as lock-free programming
//template<typename T>
//class LockFree : public LockModifyDecoration<T>
//{
//public:
//    using NoRegisterDecoration = std::true_type;
//};
}
