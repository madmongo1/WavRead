//
// Created by Richard Hodges on 08/11/2017.
//

#pragma once

#include <boost/variant.hpp>
#include <boost/filesystem.hpp>
#include <boost/optional.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/iostreams/device/mapped_file.hpp>
#include <boost/utility/string_view.hpp>


namespace project {
    using boost::optional;
    namespace fs = boost::filesystem;
    namespace logging = boost::log;
    using boost::iostreams::mapped_file;
    using string_view = std::string_view;
    template<class...Ts> using variant = boost::variant<Ts...>;

    template<class...Args>
    inline decltype(auto) visit(Args&& ...args) { return boost::apply_visitor(std::forward<Args>(args)...); }

    struct empty_type
    {
        constexpr bool operator ==(empty_type const&) const { return true; }

        friend std::ostream& operator <<(std::ostream& os, empty_type const&)
        {
            return os << "{ empty }";
        }

        friend std::ostream& write(std::ostream& os, empty_type const& e, std::size_t indent)
        {
            return os << std::string(indent, ' ') << e;
        }
    };

    constexpr auto empty = empty_type();
}

#define PROJECT_LOG(x) BOOST_LOG_TRIVIAL(x)
