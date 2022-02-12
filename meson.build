project('ycbcr-profiles', 'cpp',
  version : 'v2022.02.11',
  license : 'BSD-3-Clause',
  default_options : ['warning_level=3', 'cpp_std=c++17'])

lcms2 = dependency('lcms2', version : '>=2.0.0')

commit = vcs_tag(command : ['git', 'describe', '--dirty'],
            fallback: meson.project_version(),
            input : 'version.h.in',
            output :'version.h')

y_709_4 = executable('ycbcr_709_v4',
           'ycbcr_709_v4.cpp',
           dependencies: lcms2,
           cpp_args: ['-DCMS_NO_REGISTER_KEYWORD'],
           install: false)

y_709_2 = executable('ycbcr_709_v2',
           'ycbcr_709_v2.cpp',
           dependencies: lcms2,
           cpp_args: ['-DCMS_NO_REGISTER_KEYWORD'],
           install: false)

y_601_4 = executable('ycbcr_601_v4',
           'ycbcr_601_v4.cpp',
           dependencies: lcms2,
           cpp_args: ['-DCMS_NO_REGISTER_KEYWORD'],
           install: false)

y_601_2 = executable('ycbcr_601_v2',
           'ycbcr_601_v2.cpp',
           dependencies: lcms2,
           cpp_args: ['-DCMS_NO_REGISTER_KEYWORD'],
           install: false)

y_709_1886_2 = executable('ycbcr_709_1886_v2',
           'ycbcr_709_v2.cpp',
           dependencies: lcms2,
           cpp_args: ['-DCMS_NO_REGISTER_KEYWORD', '-DBT1886'],
           install: false)

y_601_1886_2 = executable('ycbcr_601_1886_v2',
           'ycbcr_601_v2.cpp',
           dependencies: lcms2,
           cpp_args: ['-DCMS_NO_REGISTER_KEYWORD', '-DBT1886'],
           install: false)

y_709_1886_4 = executable('ycbcr_709_1886_v4',
           'ycbcr_709_v4.cpp',
           dependencies: lcms2,
           cpp_args: ['-DCMS_NO_REGISTER_KEYWORD', '-DBT1886'],
           install: false)

y_601_1886_4 = executable('ycbcr_601_1886_v4',
           'ycbcr_601_v4.cpp',
           dependencies: lcms2,
           cpp_args: ['-DCMS_NO_REGISTER_KEYWORD', '-DBT1886'],
           install: false)

custom_target('bt601_v2',
  command: y_601_2,
  output: ['bt601-7_ycbcr_v2.icc'],
  install: true,
  install_tag: 'ITU-R BT.601-7 v2',
  install_dir: 'share/color/icc')

custom_target('bt601_bt1886_v2',
  command: y_601_1886_2,
  output: ['bt601-7_bt1886_ycbcr_v2.icc'],
  install: true,
  install_tag: 'ITU-R BT.601-7 + BT.1886 v2',
  install_dir: 'share/color/icc')

custom_target('bt601_v4',
  command: y_601_4,
  output: ['bt601-7_ycbcr_v4.icc'],
  install: true,
  install_tag: 'ITU-R BT.601-7 v4',
  install_dir: 'share/color/icc')

custom_target('bt601_bt1886_v4',
  command: y_601_1886_4,
  output: ['bt601-7_bt1886_ycbcr_v4.icc'],
  install: true,
  install_tag: 'ITU-R BT.601-7 + BT.1886 v4',
  install_dir: 'share/color/icc')

custom_target('bt709_v2',
  command: y_709_2,
  output: ['bt709-6_ycbcr_v2.icc'],
  install: true,
  install_tag: 'ITU-R BT.709-6 v2',
  install_dir: 'share/color/icc')

custom_target('bt709_bt1886_v2',
  command: y_709_1886_2,
  output: ['bt709-6_bt1886_ycbcr_v2.icc'],
  install: true,
  install_tag: 'ITU-R BT.709-6 + BT.1886 v2',
  install_dir: 'share/color/icc')

custom_target('bt709_v4',
  command: y_709_4,
  output: ['bt709-6_ycbcr_v4.icc'],
  install: true,
  install_tag: 'ITU-R BT.709-6 v4',
  install_dir: 'share/color/icc')

custom_target('bt709_bt1886_v4',
  command: y_709_1886_4,
  output: ['bt709-6_bt1886_ycbcr_v4.icc'],
  install: true,
  install_tag: 'ITU-R BT.709-6 + BT.1886 v4',
  install_dir: 'share/color/icc')
