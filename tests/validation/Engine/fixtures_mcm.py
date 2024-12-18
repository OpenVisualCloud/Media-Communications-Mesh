# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024 Intel Corporation
# Intel® Media Communications Mesh
import os
import subprocess

import pytest


@pytest.fixture(scope="session")
def build_TestApp(build):
    path = os.path.join(build, "tests", "tools", "TestApp", "build")
    subprocess.run(f"rm -rf {path}", shell=True, timeout=10)
    subprocess.run(f"mkdir -p {path}", shell=True, timeout=10)
    subprocess.run("cmake ..", cwd=path, shell=True, timeout=10)
    subprocess.run("make", cwd=path, shell=True, timeout=10)


@pytest.fixture(scope="package")
def media_proxy_single():
    # Run single media proxy
    pass


@pytest.fixture(scope="package")
def media_proxy_dual():
    # Run dual media proxy
    pass
