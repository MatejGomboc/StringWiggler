# Declaration order matters: a library must appear after every library it links.
# testing comes first (every other lib's tests/ link against it); signals before
# logging (logging links signals); logging before window (window links logging).
add_subdirectory(testing)
add_subdirectory(signals)
add_subdirectory(logging)
add_subdirectory(math)
add_subdirectory(window)
