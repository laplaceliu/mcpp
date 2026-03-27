/**
 * @file middleware.cpp
 * @brief Filter chain implementation
 */

#include "mcpp/enterprise/middleware.hpp"

namespace mcpp {
namespace enterprise {

void FilterChain::add(std::shared_ptr<Middleware> middleware) {
    middlewares_.push_back(middleware);

    // Chain the middlewares
    if (middlewares_.size() > 1) {
        middlewares_[middlewares_.size() - 2]->set_next(middleware);
    }
}

bool FilterChain::execute(RequestContext& ctx) {
    for (size_t i = 0; i < middlewares_.size(); ++i) {
        MiddlewareResult result = middlewares_[i]->process(ctx);
        if (result != MiddlewareResult::Continue) {
            return false;
        }
    }
    return true;
}

} // namespace enterprise
} // namespace mcpp