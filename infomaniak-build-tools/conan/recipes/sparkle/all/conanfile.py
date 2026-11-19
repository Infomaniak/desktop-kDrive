import os
import shutil

from conan import ConanFile
from conan.errors import ConanInvalidConfiguration
from conan.tools.files import copy, get


class SparkleConan(ConanFile):
    name = "sparkle"
    description = "Software update framework for macOS"
    license = "MIT"
    homepage = "https://sparkle-project.org/"
    url = "https://github.com/sparkle-project/Sparkle"
    package_type = "shared-library"
    settings = "os"

    def validate(self):
        if self.settings.os != "Macos":
            raise ConanInvalidConfiguration("Sparkle is only available on macOS")

    def source(self):
        get(
            self,
            f"https://github.com/sparkle-project/Sparkle/releases/download/{self.version}/Sparkle-{self.version}.tar.xz",
            sha256="50612a06038abc931f16011d7903b8326a362c1074dabccb718404ce8e585f0b",
        )

    def package(self):
        copy(self, "LICENSE", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        # Preserve the framework's versioned symlinks and nested updater applications.
        shutil.copytree(
            os.path.join(self.source_folder, "Sparkle.framework"),
            os.path.join(self.package_folder, "lib", "Sparkle.framework"),
            symlinks=True,
        )
        shutil.copytree(os.path.join(self.source_folder, "bin"), os.path.join(self.package_folder, "bin"), symlinks=True)

    def package_info(self):
        self.cpp_info.frameworkdirs = ["lib"]
        self.cpp_info.frameworks = ["Sparkle"]
        self.cpp_info.includedirs = ["lib/Sparkle.framework/Headers"]
