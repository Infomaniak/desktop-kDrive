import kchat_notification


def scope(key, counts=None, result="success"):
    return {"key": key, "title": key, "result": result, "counts": counts}


CLEAN_COUNTS = {severity: 0 for severity in kchat_notification.SEVERITIES}
ALL_SCOPES_CLEAN = [scope(key, dict(CLEAN_COUNTS)) for key in kchat_notification.EXPECTED_SCOPE_KEYS]


def test_classify_red_on_critical_or_high():
    counts = dict(CLEAN_COUNTS, Critical=2)
    assert kchat_notification.classify("success", {"scopes": [scope("windows-server", counts)]})[0] == "red"
    counts = dict(CLEAN_COUNTS, High=1)
    assert kchat_notification.classify("success", {"scopes": ALL_SCOPES_CLEAN + [scope("macos-app", counts)]})[0] == "red"


def test_classify_orange_on_low_severities_only():
    counts = dict(CLEAN_COUNTS, Medium=3, Low=1)
    assert kchat_notification.classify("success", {"scopes": ALL_SCOPES_CLEAN + [scope("linux-appimage", counts)]})[0] == "orange"


def test_classify_green_requires_every_scope_clean_and_successful():
    assert kchat_notification.classify("success", {"scopes": ALL_SCOPES_CLEAN})[0] == "green"
    # A truncated summary (a scope missing) is not a clean scan
    assert kchat_notification.classify("success", {"scopes": ALL_SCOPES_CLEAN[:-1]})[0] == "failed"
    # A scope without result counts is not a clean scan
    assert kchat_notification.classify("success", {"scopes": ALL_SCOPES_CLEAN + [scope("windows-server")]})[0] == "failed"
    # A scope whose scan job failed is not a clean scan
    failed = scope("linux-appimage", dict(CLEAN_COUNTS), result="failure")
    assert kchat_notification.classify("success", {"scopes": ALL_SCOPES_CLEAN + [failed]})[0] == "failed"


def test_classify_failed_report_or_missing_summary():
    assert kchat_notification.classify("failure", {"scopes": ALL_SCOPES_CLEAN})[0] == "failed"
    assert kchat_notification.classify("success", None)[0] == "failed"
    assert kchat_notification.classify("success", {"scopes": []})[0] == "failed"


def test_classify_grey_when_cancelled():
    counts = dict(CLEAN_COUNTS, Critical=1)
    assert kchat_notification.classify("cancelled", {"scopes": [scope("windows-server", counts)]})[0] == "grey"


def test_badge_url_escapes_dashes_and_spaces_and_requests_a_png():
    total = dict(CLEAN_COUNTS, Critical=1)
    url, right = kchat_notification.badge_url("red", total, "2026-02-10")

    assert url == ("https://raster.shields.io/badge/vulnerability_scan-1_CRITICAL-red.png"
                   "?style=for-the-badge&logo=githubactions&logoColor=white")
    assert right == "1 CRITICAL"
