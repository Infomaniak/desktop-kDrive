import conan_graph_to_cyclonedx


def test_conan_purl_with_user_and_channel():
    # Qualifiers are sorted and user/channel follow the purl-spec conan type
    assert conan_graph_to_cyclonedx.conan_purl("zlib", "1.3.1", "infomaniak", "stable") \
        == "pkg:conan/zlib@1.3.1?channel=stable&user=infomaniak"


def test_conan_purl_without_user_and_channel():
    assert conan_graph_to_cyclonedx.conan_purl("zlib", "1.3.1") == "pkg:conan/zlib@1.3.1"
    # conan writes "_" when a package has no user or channel
    assert conan_graph_to_cyclonedx.conan_purl("zlib", "1.3.1", "_", "_") == "pkg:conan/zlib@1.3.1"


def test_conan_purl_percent_encodes_qualifier_values():
    assert conan_graph_to_cyclonedx.conan_purl("zlib", "1.3.1", "User Name", "stable") \
        == "pkg:conan/zlib@1.3.1?channel=stable&user=User%20Name"


def test_conan_cpe_uses_product_aliases():
    assert conan_graph_to_cyclonedx.conan_cpe("zlib", "1.3.1") \
        == "cpe:2.3:a:zlib:zlib:1.3.1:*:*:*:*:*:*:*"
    # Recipe names that build a product known to NVD under another identifier
    assert conan_graph_to_cyclonedx.conan_cpe("sqlite3", "3.45.0") \
        == "cpe:2.3:a:sqlite:sqlite:3.45.0:*:*:*:*:*:*:*"
    assert conan_graph_to_cyclonedx.conan_cpe("openssl-macos", "3.2.1") \
        == "cpe:2.3:a:openssl:openssl:3.2.1:*:*:*:*:*:*:*"


def test_graph_packages_keeps_host_dependencies_only():
    graph = {
        "graph": {
            "nodes": {
                "0": {"ref": ".", "recipe": "Consumer"},
                "1": {"ref": "zlib/1.3.1", "name": "zlib", "version": "1.3.1", "context": "host",
                      "user": "infomaniak", "channel": "stable"},
                "2": {"ref": "cmake/3.28.1", "name": "cmake", "version": "3.28.1", "context": "build"},
            }
        }
    }

    packages = conan_graph_to_cyclonedx.graph_packages(graph)

    assert packages == {("zlib", "1.3.1"): ("infomaniak", "stable")}
    bom = conan_graph_to_cyclonedx.cyclonedx(packages)
    assert bom["components"] == [
        {
            "type": "library",
            "name": "zlib",
            "version": "1.3.1",
            "purl": "pkg:conan/zlib@1.3.1?channel=stable&user=infomaniak",
            "cpe": "cpe:2.3:a:zlib:zlib:1.3.1:*:*:*:*:*:*:*",
        }
    ]


def test_graph_packages_reads_user_and_channel_from_the_ref():
    graph = {"graph": {"nodes": {"1": {"ref": "zlib/1.3.1@infomaniak/stable#rrev", "context": "host"}}}}

    assert conan_graph_to_cyclonedx.graph_packages(graph) == {("zlib", "1.3.1"): ("infomaniak", "stable")}
