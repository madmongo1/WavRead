//
// Created by Richard Hodges on 08/11/2017.
//

#pragma once

#include "implement_indent_write_base.hpp"

template<class Outer>
struct ImplementIndentWrite
    : ImplementIndentWriteBase
{
    friend std::ostream& write(std::ostream& os, ImplementIndentWrite const& wf, std::size_t indent = 0)
    {
        return static_cast<Outer const&>(wf).write_impl(os, make_indent(os, indent));
    }

    friend std::ostream& operator <<(std::ostream& os, ImplementIndentWrite const& wf)
    {
        return write(os, static_cast<Outer const&>(wf));
    }

};
