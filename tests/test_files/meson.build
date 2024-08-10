# copy json files for testing
files = [
    'nullbytes.c',
    'invalid_utf.c',
    'crlf.c',
]

foreach f : files
    configure_file(input : f,
        output : f,
        copy: true)
endforeach
