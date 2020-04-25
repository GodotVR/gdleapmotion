#!python
import os

# Reads variables from an optional file.
customs = ['../custom.py']
opts = Variables(customs, ARGUMENTS)

# Gets the standart flags CC, CCX, etc.
env = DefaultEnvironment()

# Define our parameters
opts.Add(EnumVariable('target', "Compilation target", 'release', ['d', 'debug', 'r', 'release']))
opts.Add(EnumVariable('platform', "Compilation platform", 'windows', ['windows', 'x11', 'linux', 'osx']))
opts.AddVariables(
    PathVariable('leapsdk_path', 'The path where the leap motion SDK is located.', 'LeapSDK/'),
    PathVariable('target_path', 'The path where the lib is installed.', 'demo/addons/gdleapmotion/bin/'),
    PathVariable('target_name', 'The library name.', 'libgdleapmotion', PathVariable.PathAccept),
)
opts.Add(BoolVariable('use_llvm', "Use the LLVM / Clang compiler", 'no'))
opts.Add(EnumVariable('bits', "CPU architecture", '64', ['32', '64']))

# Other needed paths
godot_headers_path = "godot-cpp/godot_headers/"
godot_cpp_path = "godot-cpp/"
godot_cpp_library = "libgodot-cpp"

# Updates the environment with the option variables.
opts.Update(env)

# Check some environment settings
if env['use_llvm']:
    env['CXX'] = 'clang++'

# platform dir for openvr libraries
platform_dir = ''
leapsdk_path = env['leapsdk_path']
leapsdk_lib = leapsdk_path

# Setup everything for our platform
if env['platform'] == 'windows':
    env['target_path'] += 'win' + env['bits'] + '/'
    godot_cpp_library += '.windows'
    platform_dir = 'win'

    if not env['use_llvm']:
        # This makes sure to keep the session environment variables on windows,
        # that way you can run scons in a vs 2017 prompt and it will find all the required tools
        env.Append(ENV = os.environ)

        env.Append(CCFLAGS = ['-DWIN32', '-D_WIN32', '-D_WINDOWS', '-W3', '-GR', '-D_CRT_SECURE_NO_WARNINGS'])
        if env['target'] in ('debug', 'd'):
            env.Append(CCFLAGS = ['-EHsc', '-D_DEBUG', '-MDd'])
        else:
            env.Append(CCFLAGS = ['-O2', '-EHsc', '-DNDEBUG', '-MD'])
    # untested
    else:
        if env['target'] in ('debug', 'd'):
            env.Append(CCFLAGS = ['-fPIC', '-g3','-Og', '-std=c++17'])
        else:
            env.Append(CCFLAGS = ['-fPIC', '-g','-O3', '-std=c++17'])

    if env['bits'] == '64':
        leapsdk_lib = leapsdk_lib + 'lib/x64/LeapC.lib'
    else:
        leapsdk_lib = leapsdk_lib + 'lib/x86/LeapC.lib'

# osx is not yet supported
elif env['platform'] == 'osx':
    env['target_path'] += 'osx/'
    godot_cpp_library += '.osx'
    platform_dir = 'osx'
    if env['target'] in ('debug', 'd'):
        env.Append(CCFLAGS = ['-g','-O2', '-arch', 'x86_64'])
    else:
        env.Append(CCFLAGS = ['-g','-O3', '-arch', 'x86_64'])
    env.Append(CXXFLAGS='-std=c++11')
    env.Append(LINKFLAGS = ['-arch', 'x86_64'])

# linux is not yet supported
elif env['platform'] in ('x11', 'linux'):
    env['target_path'] += 'x11/'
    godot_cpp_library += '.linux'
    platform_dir = 'linux'
    if env['target'] in ('debug', 'd'):
        env.Append(CCFLAGS = ['-fPIC', '-g3','-Og', '-std=c++17'])
    else:
        env.Append(CCFLAGS = ['-fPIC', '-g','-O3', '-std=c++17'])
    env.Append(CXXFLAGS='-std=c++0x')
    env.Append(LINKFLAGS = ['-Wl,-R,\'$$ORIGIN\''])

# Complete godot-cpp library path
if env['target'] in ('debug', 'd'):
    godot_cpp_library += '.debug'
else:
    godot_cpp_library += '.release'

godot_cpp_library += '.' + str(env['bits'])

# Update our include search path
env.Append(CPPPATH=[
    '.',
    'src/',
    godot_headers_path,
    godot_cpp_path + 'include/',
    godot_cpp_path + 'include/core/',
    godot_cpp_path + 'include/gen/',
    leapsdk_path + 'include/'
])

# Add our godot-cpp library
env.Append(LIBPATH=[godot_cpp_path + 'bin/'])
env.Append(LIBS=[godot_cpp_library, leapsdk_lib])

# Add our sources
sources = Glob('src/*.c')
sources += Glob('src/*.cpp')

# Build our library
library = env.SharedLibrary(target=env['target_path'] + env['target_name'], source=sources)

Default(library)

# Generates help for the -h scons option.
Help(opts.GenerateHelpText(env))

# note, copy LeapC.dll into bin folder
