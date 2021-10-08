from conans import ConanFile, CMake
import pathlib

class FTPClientCPPConan(ConanFile):
    name = "ftpclient-cpp"
    version = "0.1.0"
    license = "MIT"
    author = "embeddedmz https://github.com/embeddedmz"
    url = "https://github.com/embeddedmz/ftpclient-cpp"
    description = "C++ client for making FTP requests"
    settings = "os", "compiler", "build_type", "arch"
    options = {"shared": [False]}
    default_options = {"shared": False}
    requires = "libcurl/7.79.0"
    generators = "cmake_find_package"
    exports_sources = "*"

    def build(self):
        cmake = self._configure()
        cmake.build()

    def _configure(self):
        cmake = CMake(self)
        cmake.configure(source_folder=".")
        return cmake

    def package(self):
        self.copy("*.h", "include", src="FTP")
        self.copy("*.a", dst="lib", src="", keep_path=False)
        self.copy("*.so", dst="lib", src="", keep_path=False)
        self.copy("*.lib", dst="lib", src="", keep_path=False)
        self.copy("*.dll", dst="lib", src="", keep_path=False)
        self.copy("LICENSE")

    def package_info(self):
        self.cpp_info.libs = ["ftpclient"]
