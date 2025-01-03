#!/usr/bin/env python3
#
# Infomaniak kDrive - Desktop
# Copyright (C) 2023-2024 Infomaniak Network SA
# 
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

import argparse
import datetime
import deepl
import errno
import os
import re
import shutil
import subprocess
import sys

parser = argparse.ArgumentParser(
    description="""Translater script for the desktop kDrive release notes.
The translation is done using DeepL API from English to French, German, Italian and Spanish.
Please make sure your DeepL API key is exported as DEEPL_AUTH_KEY before running the script.

A default english version of the Release Notes with all the systems combined must be created first.
This script requires a file named kDrive-template.html to exist in the current working directory.""",
    epilog="""
To add more languages in the future, edit the script to append the new language to the list.
    All the languages in the list must be in the DeepL API target language list.
    Language list : https://developers.deepl.com/docs/api-reference/languages

To add more systems, edit the script to append the new system to the list.
    System specific Release Notes entries must start with the system name.
    eg: "Windows - Added new feature" for Windows specific feature.
    """,
    formatter_class=argparse.RawTextHelpFormatter)

parser.add_argument('text')
args = parser.parse_args()

deepl_key = os.getenv("DEEPL_AUTH_KEY")
if not deepl_key:
    sys.exit("error: The DeepL API key is not set in env");
translator = deepl.Translator(deepl_key)

target_lang = [
    'DE',
    'ES',
    'FR',
    'IT'
]

try:
    for lang in target_lang:
        result = translator.translate_text(
            f"{args.text}",
            target_lang=lang,
        )
        print(f"{lang}: {result.text}")
except Exception as e:
    print(e)

usage = translator.get_usage()
print(f"Translation done. {usage.character} characters used for the month.")