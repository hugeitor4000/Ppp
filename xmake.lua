add_rules("mode.debug", "mode.release", "mode.releasedbg")

add_repositories("liteldev-repo https://github.com/LiteLDev/xmake-repo.git")

add_requires("levilamina")

if not has_config("vs_runtime") then
    set_runtimes("MD")
end

target("immortal-zombie")
    set_kind("shared")
    add_files("src/**.cpp")
    add_includedirs("src")
    add_packages("levilamina")
    set_languages("c++20")
    set_exceptions("cxx")
