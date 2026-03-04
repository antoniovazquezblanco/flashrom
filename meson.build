project('flashrom', 'c',
  version : run_command('cat', 'VERSION', check: true).stdout().strip(),
  license : 'GPL-2.0',
  meson_version : '>=0.56.0',
  default_options : [
    'warning_level=2',
    'c_std=c99',
    'werror=true',
    'optimization=s',
    'debug=false',
    'default_library=both'
  ],
)

fs = import('fs')

if get_option('classic_cli').enabled() and get_option('default_library') == 'shared'
  error('''
    Cannot build cli_classic with shared libflashrom. Use \'-Dclassic_cli=disabled\' to disable the cli,
    or use \'--default-library=both\' to also build the classic_cli
  ''')
endif

# libtool versioning
lt_current = '1'
lt_revision = '0'
lt_age = '0'
lt_version = '@0@.@1@.@2@'.format(lt_current, lt_age, lt_revision)

flashrom_version = meson.project_version()
if get_option('use_git_version').auto() or get_option('use_git_version').enabled()
  git = find_program('git', native : true, required : get_option('use_git_version').enabled())
  if git.found() and fs.is_dir(meson.project_source_root()/'.git')
    version_git = run_command('git', 'describe', check : false)
    if version_git.returncode() == 0
      flashrom_version += ' (git:@0@)'.format(version_git.stdout().strip())
    endif
  endif
endif

# hide/enable some warnings
warning_flags = [
  '-Wshadow',
  '-Wmissing-prototypes',
  '-Wwrite-strings',
  '-Wno-unused-parameter',
  '-Wno-address-of-packed-member',
  '-Wno-enum-conversion',
  '-Wno-missing-braces',
]

cc = meson.get_compiler('c')
add_project_arguments(cc.get_supported_arguments(warning_flags), language : 'c')
add_project_arguments('-D_DEFAULT_SOURCE', language : 'c')
add_project_arguments('-D_POSIX_C_SOURCE=200809L', language : 'c') # required for fileno, nanosleep, and strndup
add_project_arguments('-D_BSD_SOURCE', language : 'c') # required for glibc < v2.19
add_project_arguments('-D__BSD_VISIBLE', language : 'c') # required for u_char, u_int, u_long on FreeBSD
add_project_arguments('-D__XSI_VISIBLE', language : 'c') # required for gettimeofday() on FreeBSD
add_project_arguments('-D_NETBSD_SOURCE', language : 'c') # required for indirect include of strings.h on NetBSD
add_project_arguments('-D_DARWIN_C_SOURCE', language : 'c') # required for indirect include of strings.h on MacOS
add_project_arguments('-DFLASHROM_VERSION="' + flashrom_version + '"', language : 'c')

# Include subdirectories
subdir('doc')
subdir('src')


if get_option('ich_descriptors_tool').auto() or get_option('ich_descriptors_tool').enabled()
  subdir('util/ich_descriptors_tool')
endif
