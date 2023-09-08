#pragma once
#include <condition_variable>
#include <memory>
#include <mutex>
#include <type_traits>

namespace requirecpp::details {

// if there are pending get() calls, make sure the object is not destroyed,
// destruction: if there are pending blocking_get() calls, they hold a
// shared_ptr.
//   blocking calls might be stopped with fail()
template <typename T>
class TrackableObject {
 public:
  TrackableObject(std::shared_ptr<T> obj) : m_object{std::move(obj)} {}
  TrackableObject(const TrackableObject&) = delete;
  TrackableObject(TrackableObject&&) = delete;
  TrackableObject& operator=(const TrackableObject&) = delete;
  TrackableObject& operator=(TrackableObject&&) = delete;
  ~TrackableObject() = default;

  void set(const std::shared_ptr<T>& obj) {
    std::unique_lock lk{m_mutex};
    m_object = obj;
    lk.unlock();
    m_cv.notify_all();
  }

  // blocking
  std::shared_ptr<T> require() {
    std::unique_lock lk{m_mutex};
    m_cv.wait(lk, [&] { return m_shutdown || m_object != nullptr; });
    if (m_shutdown)
      throw std::runtime_error{"Could not get object"};
    return m_object;
  }

  // non blocking, may return nullptr
  std::shared_ptr<T> optional() {
    std::unique_lock lk{m_mutex};
    return m_object;
  }

  std::shared_ptr<T> now_or_throw() {
    std::unique_lock lk{m_mutex};
    if (!m_object)
      throw std::runtime_error{"Could not retrieve object"};
    return m_object;
  }

  void fail() {
    {
      std::scoped_lock lk{m_mutex};
      m_shutdown = true;
    }
    m_cv.notify_all();
  }

  bool has_value() const { return m_object != nullptr; }

 private:
  std::shared_ptr<T> m_object;
  bool m_shutdown{false};
  std::mutex m_mutex;
  std::condition_variable m_cv;
};
}  // namespace requirecpp::details
