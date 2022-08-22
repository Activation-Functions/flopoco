#ifndef SOLLYA_HANDLER_HPP
#define SOLLYA_HANDLER_HPP

#include <atomic>
#include <memory>
#include <type_traits>

#include <sollya.h>

namespace flopoco {

namespace detail {
static struct SollyaLifetimeManager {
  SollyaLifetimeManager();
  ~SollyaLifetimeManager();
} sollya_lib_lifetime_manager;
} // namespace detail

class SollyaHandler {
private:
  using manager_t = std::shared_ptr<std::remove_pointer_t<sollya_obj_t>>;
  manager_t manager;

public:
  SollyaHandler(sollya_obj_t);
  SollyaHandler() = default;
  operator sollya_obj_t() { return manager.get(); }
  operator const sollya_obj_t() const {return manager.get();}
};
} // namespace flopoco

#endif
