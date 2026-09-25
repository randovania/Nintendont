# Updates the version/release date metadata information used by HBC.
# Called by CI during a release
# Does NOT create a new commit with the modified file; its only used for the release.

import re
from pathlib import Path
from datetime import datetime

version_regex = r"<version>(.*)<\/version>"
release_date_regex = r"<release_date>(.*)<\/release_date>"

metadata_path = Path("./nintendont/meta.xml")
nintendont_version_path = Path("./common/include/NintendontVersion.h")
major_version = 0
minor_version = 0
special_version = ""

# Get metadata xml
with open(metadata_path) as f:
    xml = f.read()

# Get Nintendont version numbers
with open(nintendont_version_path) as f:
    for line in f:
        try:
            _, name, value = line.split()
        except:
            continue

        if name == "NIN_MAJOR_VERSION":
            major_version = value
        elif name == "NIN_MINOR_VERSION":
            minor_version = value
        elif name == "NIN_SPECIAL_VERSION":
            special_version = value.strip("\"")

if not major_version and not minor_version and not special_version:
    raise ValueError("Couldn't properly parse versions from NintendontVersion.h")

# Replace version
xml = re.sub(version_regex, f"<version>{major_version}.{minor_version}{special_version}</version>", xml)

# Replace date
date = datetime.now().strftime("%Y%m%d000000")
xml = re.sub(release_date_regex, f"<release_date>{date}</release_date>", xml)

# Write metadata back
with open(metadata_path, "w") as f:
    f.write(xml)


