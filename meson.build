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

m = meson.get_compiler('c').find_library('m')

sources = [
    'src/main.c',
    'src/jpeg.c',
    'src/encode.c',
    'src/decode.c',
    'src/huffman.c'
]

incs = include_directories('include')

executable(
	'jpeg-reencode',
	sources,
    include_directories: incs,
	dependencies: [m],
    c_args: ['-O3'],
)
