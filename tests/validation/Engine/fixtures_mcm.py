# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024 Intel Corporation
# Intel® Media Communications Mesh
import pytest


@pytest.fixture(scope="session")
def build_TestApp():
    # Build TestApp
    pass


@pytest.fixture(scope="package")
def media_proxy_single():
    # Run single media proxy
    pass


@pytest.fixture(scope="package")
def media_proxy_dual():
    # Run dual media proxy
    pass
