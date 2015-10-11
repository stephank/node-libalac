{
  'targets': [
    {
      'target_name': 'binding',
      'include_dirs': [ 'ALAC/codec', "<!(node -e \"require('nan')\")" ],
      'sources': [
        'ALAC/codec/EndianPortable.c',
        'ALAC/codec/ALACBitUtilities.c',
        'ALAC/codec/ALACDecoder.cpp',
        'ALAC/codec/ALACEncoder.cpp',
        'ALAC/codec/ag_dec.c',
        'ALAC/codec/ag_enc.c',
        'ALAC/codec/dp_dec.c',
        'ALAC/codec/dp_enc.c',
        'ALAC/codec/matrix_dec.c',
        'ALAC/codec/matrix_enc.c',
        'binding.cc'
      ],
      'defines': [
        'YAML_DECLARE_STATIC'
      ]
    }
  ]
}
