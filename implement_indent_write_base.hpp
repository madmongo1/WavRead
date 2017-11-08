//
// Created by Richard Hodges on 08/11/2017.
//

#pragma once

#include "project.hpp"

struct ImplementIndentWriteBase
{
    using indent_object = std::string;

    static indent_object make_indent(std::ostream& os, std::size_t indent) { return indent_object(indent, os.fill()); }
};
