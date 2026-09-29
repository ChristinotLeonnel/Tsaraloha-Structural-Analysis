// MOCK minimal d'OpenCASCADE, uniquement pour tester IsolationManager sans OCC.
#pragma once
#include <memory>
namespace opencascade {
template <class T> class handle {
public:
  handle() = default;
  handle(T *p) : m_p(p) {}
  template <class U> handle(const handle<U> &o) : m_p(o.m_p) {}
  bool IsNull() const { return !m_p; }
  void Nullify() { m_p.reset(); }
  T *get() const { return m_p.get(); }
  T *operator->() const { return m_p.get(); }
  bool operator==(const handle &o) const { return m_p == o.m_p; }
private:
  std::shared_ptr<T> m_p;
  template <class> friend class handle;
};
} // namespace opencascade
struct Standard_Type { const char *name; const char *Name() const { return name; } };
