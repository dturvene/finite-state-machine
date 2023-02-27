# meson build FSM project
# See $GK/docs/make.md

# https://mesonbuild.com/Builtin-options.html#core-options
# buildtype=[plain, debug, debugoptimized, release,...]
#   To see current values> meson configure
#   To change> meson setup meson_bld --wipe
project('stoplight/crosswalk FSM', 'c',
  default_options: [ 'buildtype=plain' ],
  version : '1.0',
  license : 'MIT',
  )

pthread_dep = dependency('threads')

# https://mesonbuild.com/Builtin-options.html#base-options
# https://github.com/mesonbuild/meson/discussions/9777
# default shared object link flag is -Wl,--no-undefined, remove with b_lundef
libsrc = ['fsm.c', 'evtq.c', 'timer.c', 'cli.c']
libfsm = shared_library('fsm', libsrc, override_options: ['b_lundef=false'], install : false)
message('WARN: set $LD_LIBRARY_PATH to include path')

# https://mesonbuild.com/Syntax.html#foreach-statements
progs = [['fsmdemo', ['fsmdemo.c']],
         ['evtdemo', ['evtdemo.c']]]

# build the target executables
fsmdemo = executable('fsmdemo', 'fsmdemo.c', link_with : libfsm, dependencies : pthread_dep)
evtdemo = executable('evtdemo', 'evtdemo.c', link_with : libfsm, dependencies : pthread_dep)

# https://mesonbuild.com/Unit-tests.html
# linux> meson test [--repeat=N]
test('evt demo', evtdemo, args : ['-n', '-s', '../evtdemo.script', '-t', '200'])
test('fsm demo', fsmdemo, args : ['-n', '-s', '../fsmdemo.script', '-t', '100'])
