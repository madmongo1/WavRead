if (AddBoost_included)
    return()
else ()
    set(AddBoost_included 1)
endif ()

function(AddBoost target)
    set(options)
    set(oneValueArgs)
    set(multiValueArgs COMPONENTS)
    cmake_parse_arguments(ADDBOOST "${options}" "${oneValueArgs}"
            "${multiValueArgs}" ${ARGN})

    set(componentArg)
    if (ADDBOOST_COMPONENTS)
        set(componentArg "COMPONENTS" ${ADDBOOST_COMPONENTS})
    endif ()

    hunter_add_package(Boost ${componentArg})
    set(libs "Boost::boost")
    foreach (component IN LISTS ADDBOOST_COMPONENTS)
        list(APPEND libs "Boost::${component}")
    endforeach ()

    find_package(Boost CONFIG ${componentArg})
    target_link_libraries(${target} ${libs})

endfunction()