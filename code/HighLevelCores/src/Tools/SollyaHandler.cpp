#include "flopoco/Tools/SollyaHandler.hpp"

namespace {
void delete_sollya_obj_t(sollya_obj_t managed) {
  sollya_lib_clear_obj(managed);
}
} // namespace
namespace flopoco {
detail::SollyaLifetimeManager sollya_lib_lifetime_manager{};

detail::SollyaLifetimeManager::SollyaLifetimeManager() { sollya_lib_init(); }
detail::SollyaLifetimeManager::~SollyaLifetimeManager() { sollya_lib_close(); }

SollyaHandler::SollyaHandler(sollya_obj_t managed)
    : manager{managed, delete_sollya_obj_t} {}
} // namespace archgenlib
