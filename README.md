# requirecpp

A framework that solves the chicken egg problem:

Chicken:
```cpp
class Chicken {
 public:
  Chicken(const std::string& name) : m_name{name} {}
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
  Egg(const std::string& label) : m_label{label} {}
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
  ctx.require([](const Chicken& chicken,
                 const Egg& egg) { chicken.hatched_from(egg); },
              "hatched_from");
  ctx.require(
      [](const Chicken& chicken, const Egg& egg) { egg.laid_by(chicken); },
      "laid_by");
		
  ctx.emplace<Chicken>("Chuck");
  ctx.emplace<Egg>("Egg3000");

  return 0;
}
```

Output:
```
Chuck hatched from Egg3000
Egg3000 laid by Chuck
```

Why didn't we just create the `Chicken` and `Egg` and then just call their methods?
In bigger projects you might find more components with dependecies for the different functions they have and that's where `requirecpp` shines.
Imagine there is a project with components `Database`, `UserService`, `Settings`, `InputReader`, `Webserver`, ... and they all depend upon each other.
`requirecpp` takes the burden from you to care about the order of initialization and enables you to add in components easily, manage, find and debug cyclic dependecies.
Lifecycle management of components can be handled cleanly in a multithreaded environment.

- Can be used without infecting your precious components with a dependency to `requirecpp` (see above example, `Chicken` and `Egg` are not aware of `requirecpp`)
- Components can be aware of `requirecpp` and manage their dependecies on their own. No glue code required to setup and compose your components.
- Debug dependecies by printing unmet requirements and missing components
- Will be threadsafe
- Use requirement callbacks with `shared_ptr`, references or copy-by-value depending on what you need. Remove and add objects to context whenever you want. However, components must live in `shared_ptr`s at the moment.
- Use states/tags to mark that your components reached a certain state: `require<Tagged<MyWebserver, SocketState::LISTENING>()`
