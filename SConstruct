#!python
import os, subprocess

# Local dependency paths, adapt them to your setup
godot_headers_path = ARGUMENTS.get("headers", "godot_headers/")
cpp_bindings_path = ARGUMENTS.get("cpp_bindings_path", "godot-cpp/")
cpp_bindings_library_path = ARGUMENTS.get("cpp_bindings_library", "godot-cpp/bin/godot_cpp_bindings")
leapsdk_path = ARGUMENTS.get("leapsdk_path", "C:/Users/basti/Development/LeapDeveloperKit_3.2.1+45911_win/LeapSDK/")

target = ARGUMENTS.get("target", "debug")
platform = ARGUMENTS.get("platform", "windows")
bits = ARGUMENTS.get("bits", 64)

# This makes sure to keep the session environment variables on windows, 
# that way you can run scons in a vs 2017 prompt and it will find all the required tools
env = Environment()
if platform == "windows":
    env = Environment(ENV = os.environ)

if ARGUMENTS.get("use_llvm", "no") == "yes":
    env["CXX"] = "clang++"

def add_sources(sources, directory):
    for file in os.listdir(directory):
        if file.endswith('.cpp'):
            sources.append(directory + '/' + file)

if platform == "osx":
    env.Append(CCFLAGS = ['-g','-O3', '-arch', 'x86_64'])
    env.Append(LINKFLAGS = ['-arch', 'x86_64'])

if platform == "linux":
    env.Append(CCFLAGS = ['-fPIC', '-g','-O3', '-std=c++14'])

if platform == "windows":
#    env.Append(CCFLAGS = ['-DWIN32', '-D_WIN32', '-D_WINDOWS', '-W3', '-GR', '-D_CRT_SECURE_NO_WARNINGS'])
    if target == "debug":
        env.Append(CCFLAGS = ['-EHsc', '-D_DEBUG', '-MDd'])
    else:
        env.Append(CCFLAGS = ['-O2', '-EHsc', '-DNDEBUG', '-MD'])

leapsdk_lib = leapsdk_path
if bits == 64:
    leapsdk_lib = leapsdk_lib + 'lib/x64/LeapC.lib'
else:
    leapsdk_lib = leapsdk_lib + 'lib/x86/LeapC.lib'

env.Append(CPPPATH=['.', 'src/', godot_headers_path, cpp_bindings_path + 'include/', cpp_bindings_path + 'include/core/', leapsdk_path + 'include/'])
env.Append(LIBS=[cpp_bindings_library_path, leapsdk_lib])

sources = []
add_sources(sources, "src")

library = env.SharedLibrary(target='demo/bin/libgdleapmotion', source=sources)
Default(library)

# note, copy LeapC.dll into bin folder
