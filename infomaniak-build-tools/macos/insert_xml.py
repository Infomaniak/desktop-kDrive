#!/usr/bin/env python
# -*- encoding: ascii -*-
"""insert_xml.py"""

import sys
from bs4 import BeautifulSoup as Soup

filename, appname  = sys.argv[1:3]                              # "update-macos-3.7.8.2.xml", "kDrive-3.7.8.2"
version = appname.split("-")[-1]                                # "3.7.8.2"
short_version = version[:-2]                                    # "3.7.8"
semantic_version = f"{appname.split("-")[0]}-{short_version}"   # "kDrive-3.7.8"

print(f"Processing '{filename}' \n - Version '{appname}' \n - Semantic version: '{semantic_version}':")

soup = Soup(open(filename),"xml")
element = soup.find("sparkle:minimumSystemVersion")

print("  -- Inserting release notes links")
languages = ["en", "fr", "de", "es", "it"]
for lang in reversed(languages):
    new_tag = soup.new_tag('sparkle:releaseNotesLink')
    new_tag["xml:lang"] = lang
    new_tag.string = f"https://download.storage.infomaniak.com/drive/desktopclient/{semantic_version}-macos-{lang}.html"
    element.insert_after(new_tag)

print("  -- Inserting download URL")
tag = soup.enclosure
tag['downloadUrl'] = f"https://download.storage.infomaniak.com/drive/desktopclient/{appname}.pkg"

with open(filename, 'w') as xmlfile:
    xmlfile.write(soup.prettify())