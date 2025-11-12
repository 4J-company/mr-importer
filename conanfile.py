from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMake, cmake_layout, CMakeDeps
from conan.tools.build import check_min_cppstd

class mr_importerRecipe(ConanFile):
    name = "mr-importer"
    version = "2.9.0"
    package_type = "library"

    license = "MIT"
    author = "Michael Tsukanov mt6@4j-company.ru"
    url = "https://github.com/4j-company/mr-importer"
    description = "Opinionated asset importer"
    topics = ("Asset", "3D", "Computer Graphics")

    settings = "os", "compiler", "build_type", "arch"

    options = {"shared": [True, False]}
    default_options = {"shared": False}

    exports_sources = "CMakeLists.txt", "src/*", "include/*", "cmake/deps.cmake", "tests/main.cpp", "examples/*"

    def requirements(self):
        self.requires("spdlog/1.14.1", override=True)

        self.requires("meshoptimizer/0.25")
        self.requires("fastgltf/0.9.0-4j")

        self.requires("ktx/4.3.2")
        self.requires("draco/1.5.7")

        self.requires("glm/1.0.1")

        self.requires("slang/2025.10.4")

        self.requires("onetbb/2022.2.0")

        self.requires("mr-math/1.1.4")
        self.requires("mr-utils/1.1.2")

        self.requires("tracy/0.12.2")

    def build_requirements(self):
        self.tool_requires("cmake/[>3.26]")
        self.tool_requires("ninja/[~1.12]")

        if self.settings.os == "Linux":
            self.tool_requires("mold/[>=2.40]")

        self.test_requires("gtest/1.14.0")

    def validate(self):
        check_min_cppstd(self, "23")

    def configure(self):
        if self.settings.os == "Linux":
            self.conf_info.append("tools.build:exelinkflags", "-fuse-ld=mold")
            self.conf_info.append("tools.build:sharedlinkflags", "-fuse-ld=mold")

    def layout(self):
        cmake_layout(self)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.generator = "Ninja"
        tc.generate()

        deps = CMakeDeps(self)
        deps.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()
