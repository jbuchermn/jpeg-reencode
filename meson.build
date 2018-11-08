project(
    'jpeg-reencode',
    'c',
    version: '0.1.0',
    license: 'MIT',
    meson_version: '>=0.48.0',
    default_options : [
        'c_std=c11',
        'warning_level=2'
    ],
)

python = import('python').find_installation('python3')

m = meson.get_compiler('c').find_library('m')

deps = [
    m
]

sources = [
    'src/jpeg.c',
    'src/encode.c',
    'src/decode.c',
    'src/reencode.c',
    'src/huffman.c'
]

py_sources = [
    'py/jpeg_reencodemodule.c'
]

incs = include_directories('include')

executable(
	'jpeg-reencode',
	sources + ['src/main.c'],
    include_directories: incs,
	dependencies: [m],
    c_args: ['-Ofast']
)

python.extension_module(
    'jpeg_reencode',
    sources + py_sources,
    include_directories: incs,
    dependencies: deps + [python.dependency()],
    c_args: ['-Ofast']
)
