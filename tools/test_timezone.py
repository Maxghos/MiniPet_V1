"""Compare firmware POSIX rules against IANA America/Santiago, including DST edges."""
import datetime as dt
import os
from pathlib import Path
import re
import time
from zoneinfo import ZoneInfo

source = Path("src/NetworkService.cpp").read_text()
rules = re.search(r'santiagoTimezone = "([^"]+)"', source).group(1)
os.environ["TZ"] = rules
time.tzset()
zone = ZoneInfo("America/Santiago")
start = dt.datetime(2026, 1, 1, tzinfo=dt.timezone.utc)
checks = 0
for hours in range(0, 365 * 24):
    instant = start + dt.timedelta(hours=hours)
    expected = instant.astimezone(zone)
    actual = time.localtime(instant.timestamp())
    assert (actual.tm_year, actual.tm_mon, actual.tm_mday, actual.tm_hour) == (
        expected.year, expected.month, expected.day, expected.hour
    ), (instant, expected, actual)
    checks += 1
print(f"Timezone: {checks} hourly comparisons passed against IANA America/Santiago (2026)")
