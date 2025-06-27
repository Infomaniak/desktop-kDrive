import argparse
import deepl
import html
import os
import sys
from pathlib import Path
from bs4 import BeautifulSoup as Soup


def get_default_git_dir_path():
    git_source_dir = os.getenv("KD_GIT_DIR")
    if not git_source_dir:
        return Path(os.getenv("HOME")) / "Projects" / "desktop-kDrive"
    
    return git_source_dir

def handle_changes(changes, soup, file_path):
    print("Change summary:")
    print(f"* [{language}]")
    for (key, value) in changes.items():
        print(f"\t [+] {key[0]} - line {key[1]} : \n\t {value}")

    print()
    answer = input(f"Saving changes to file '{str(file_path)}'? [Y/N]: ")
    if answer != "Y":
        print("Aborting")
        return
    
    with open(file_path, "w") as file:
        file.write(str(soup))


parser = argparse.ArgumentParser(description="""Translate automatically from english the newly introduced translatable strings of the desktop-kDrive GUI.
""")
parser.add_argument("--git_dir_path", help="path to the git directory of desktop-kDrive", type=str,default=str(get_default_git_dir_path()))
args = parser.parse_args()


deepl_key = os.getenv("DEEPL_AUTH_KEY")
if not deepl_key:
    sys.exit("error: The DeepL API key is not set in env");

translator = deepl.Translator(deepl_key)

def fill_missing_translations(git_dir_path, language):
    file_name = f"client_{language}.ts"
    file_path = Path(git_dir_path) / "translations" / file_name

    soup = Soup(open(file_path),"xml")
    contexts = soup.find_all("context")
    
    changes = {}

    print(f"Detecting missing translations in client_{language}.ts ...")

    for context in contexts:
        messages = context.find_all("message")
        for message in messages:
            if not message.translation.text:
                    source_text = message.source.get_text()
                    message.translation.string = html.unescape(translator.translate_text(source_text, target_lang=language).text)
                    del message.translation["type"] # Mark translation as finished
                    changes[(message.location.get("filename"), message.location.get("line"))] = message.translation

    
    if changes:
        handle_changes(changes, soup, file_path)
    else:
        print("No changes detected.")
    
    

languages = ["de", "es", "fr", "it"]

for language in languages:
    fill_missing_translations(args.git_dir_path, language)

