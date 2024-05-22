#!/usr/bin/env python3

import datetime
import deepl
import os
import re
import subprocess
import sys

pattern = re.compile("^(\d+\.)?(\d+\.)?(\*|\d+)")

if len(sys.argv) < 2 or not pattern.match(sys.argv[1]):
    version = input("Enter the release version: ")
else:
    version = sys.argv[1]

if len(sys.argv) < 3:
    date = datetime.date.today().strftime('%Y%m%d')
else:
    date = sys.argv[2]

if not os.path.isfile(f"kDrive-{version}.{date}.html"):
    quit();

dirPath = "../release_notes/"
fullName = f"kDrive-{version}.{date}

translator = deepl.Translator(os.getenv("DEEPL_AUTH_KEY"))

target_lang = [
    'FR',
    'DE',
    'ES',
    'IT'
]

os.mkdir(f"{dirPath}/{fullName}", mode = 0o755)

for lang in target_lang:
    translator.translate_document_from_filepath(
        {fullName}.html",
        f"{dirPath}/{fullName}-{lang.lower()}.html",
        target_lang=lang,
    )

subprocess.run(['./encode.sh'], shell=True)

usage = translator.get_usage()
print(f"Translation done. {usage.character} characters used for the month.")
