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

def version_regex(arg_value, pattern=re.compile(r'^(\d+\.)?(\d+\.)?(\*|\d+)')):
    if not pattern.match(arg_value):
        raise argparse.ArgumentTypeError("invalid version")
    return arg_value

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

parser.add_argument('-v', '--version', metavar="", type=version_regex, help='The release version (eg: 3.6.0)', required=True)
parser.add_argument('-d', '--date', metavar="", type=int, help='The planned release date (defaults to today)', default=datetime.date.today().strftime('%Y%m%d'))
args = parser.parse_args()

if not os.path.isfile(f"kDrive-template.html"):
    sys.exit("Unable to find kDrive-template.html.");

fullName = f"kDrive-{args.version}.{args.date}"
dirPath = f"../release_notes/{fullName}"

deepl_key = os.getenv("DEEPL_AUTH_KEY")
if not deepl_key:
    sys.exit("error: The DeepL API key is not set in env");
translator = deepl.Translator(deepl_key)

target_lang = [
    'FR',
    'DE',
    'ES',
    'IT'
]

os_list = [
    'Win',
    'Linux',
    'macOS'
]

def split_os(lang):
    lang_ext = f"-{lang.lower()}" if lang != 'en' else ""

    for os_name in os_list:
        os_ext = f"-{os_name.lower()}" if os_name != 'macOS' else ""
        if os_name != 'macOS':
            shutil.copyfile(f"{fullName}{lang_ext}.html", f"{fullName}{os_ext}{lang_ext}.html")
        with open(f"{fullName}{os_ext}{lang_ext}.html", "r") as f:
            lines = f.readlines()
        with open(f"{fullName}{os_ext}{lang_ext}.html", "w") as f:
            for line in lines:
                if any(os_note in line for os_note in os_list):
                    if (f"<li>{os_name}" in line):
                        f.write(f"\t\t<li>{line[line.find('-') + 2:]}")
                else:
                    f.write(line)

print(f"Generating Release Notes for kDrive-{args.version}.{args.date}")

try:
    os.mkdir(dirPath, mode = 0o755)
except OSError as exc:
    if exc.errno != errno.EEXIST:
        raise
    pass

shutil.copyfile("kDrive-template.html", f"{dirPath}/{fullName}.html")
os.chdir(dirPath)

try:
    for lang in target_lang:
        translator.translate_document_from_filepath(
            f"{fullName}.html",
            f"{fullName}-{lang.lower()}.html",
            target_lang=lang,
        )
        split_os(lang)
except Exception as e:
    print(e)

split_os('en')

subprocess.run(['../../infomaniak-build-tools/encode.sh'], shell=True)

usage = translator.get_usage()
print(f"Translation done. {usage.character} characters used for the month.")