from conan import ConanFile


class RingBufferConan(ConanFile):
    name = "ringbuffer"
    version = "0.1.3"
    settings = "os", "arch", "compiler", "build_type"
    generators = "CMakeDeps"

    requires = (
        "boost/1.90.0",
        "catch2/3.13.0",
    )

    default_options = {
        "boost/*:without_cobalt": True,
    }
