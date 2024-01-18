# requirecpp

A framework that solves the chicken egg problem.

Chicken:
```cpp
class Chicken {
 public:
  Chicken(requirecpp::Context& ctx, const std::string& name) : m_name{name} {
    ctx.require([this, &ctx](const Egg& egg) { hatched_from(egg); },
                "hatched_from");
  }
  void hatched_from(const Egg& egg) const {
    std::cout << get_name() << " hatched from " << egg.get_label() << std::endl;
  }
  std::string get_name() const { return m_name; }

 private:
  std::string m_name;
};
```
Egg:
```cpp
class Egg {
 public:
  Egg(requirecpp::Context& ctx, const std::string& label) : m_label{label} {
    ctx.require([this](const Chicken& chicken) { laid_by(chicken); },
                "laid_by");
  }
  void laid_by(const Chicken& chicken) const {
    std::cout << get_label() << " laid by " << chicken.get_name() << std::endl;
  }
  std::string get_label() const { return m_label; }

 private:
  std::string m_label;
};
```
Dependency management by requirecpp:
```cpp
int main() {
  requirecpp::Context ctx;
  ctx.emplace<Chicken>(ctx, "Chuck");
  ctx.emplace<Egg>(ctx, "Egg3000");
}
```

Output:
```
Egg3000 laid by Chuck
Chuck hatched from Egg3000
```

The above example might seem trivial, but imagine your project with components like `Database`, `UserService`, `Settings`, `InputReader`, `Webserver`, ... and they all depend upon each other.

`requirecpp` takes the burden off you and your team to care about the order of initialization and enables you to add in components easily, manage, find and debug cyclic dependecies.
Your components lifecycles can be managed cleanly in a multithreaded environment. It leverages [Inversion of Control](https://en.wikipedia.org/wiki/Inversion_of_control) and dependecy injection patterns and is inspired by [RequireJS](https://github.com/requirejs/requirejs).  

- Two Options to use it:
  - Without infecting your precious components with a dependency to `requirecpp` (see examples for a [Chicken/Egg variant](examples/chicken-egg-unaware.cpp) that are not aware of `requirecpp`)
  - Alternatively: Components can be aware of `requirecpp` and manage their dependecies on their own. No glue code required to setup and compose your components.
- Use states/tags to mark that your components reached a certain state: `require<Tagged<MyWebserver, SocketState::LISTENING>()`
- Debug dependecies by printing unmet requirements and missing components: (see [Car Assembly example](examples/car-assembly-states.cpp))
  ```
  interior(SteeringWheel [missing], Seats)
  quality_control(Car<CarState::ASSEMBLED>, Car<CarState::INTERIOR_ASSEMBLED> [missing], Car<CarState::PAINTED>)
  finish(Car<CarState::QUALITY_CONTROL_PASSED> [missing])
  ```
  (The car factory is `[missing]` a `SteeringWheel` component, so the interior cannot be assembled and quality control cannot be done. The car is already painted though.)
- threadsafe, blocking and non-blocking calls. lazy loading of components.
  - blocking: `context.get<Wheels>()->require()` (another thread has to call e.g. `context.push(summer_tires)`)
  - non-blocking: `context.get<Wheels>()->optional()`
- Remove and add objects to context whenever you want.
- Use requirement callbacks with `shared_ptr`, **references** or **copy-by-value** depending on what you need.
  ```
  context.require(
    [this](std::shared_ptr<Wheels> wheels, //< keep wheels object valid independent of requirecpp lifecycle
           const Motor& motor,             //< object guaranteed to be valid until callback leaves scope
           Seats seats) {                  //< an independent copy of seats object
        // ... do stuff with wheels, motor and seats
    }
  ```
