#!/usr/bin/env bash

program_name="$(basename "$0")"

script_directory_path="$( cd -- "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"

function get_default_git_dir {
    git_directory="$HOME/Projects/desktop-kDrive"

    if [ -n "$KD_GIT_DIR" ]; then
        git_directory=$KD_GIT_DIR
    elif [ ! -d "$git_directory" ]; then
        git_directory="$script_directory_path/../.."
    fi

    echo $git_directory
}


git_directory="$(get_default_git_dir)"

function display_help {
  echo "$program_name [-h] [-d git_directory]"
  echo "  Updates the Qt translation files of the specified git directory"
  echo "where:"
  echo "-h  Show this help text."
  echo "-d <git-directory>" 
  echo "  Set the path to the git directory. Defaults to '$git_directory'."
}

while :
do
    case "$1" in
      -d | --git-directory)
          git_directory="$2"   
          shift 2
          ;;
      -h | --help)
          display_help 
          exit 0
          ;;
      --) # End of all options
          shift
          break
          ;;
      -*)
          echo "Error: Unknown option: $1" >&2
          exit 1 
          ;;
      *)  # No more options
          break
          ;;
    esac
done


if [ ! -d "$git_directory" ]; then
    echo "Directory does not exist: '$git_directory'. Aborting."
    echo
    display_help
    exit 1
fi

sources_directory="$git_directory/src"

if [ ! -d "$sources_directory" ]; then
    echo "Directory does not exist: '$sources_directory'. Aborting."
    echo
    display_help
    exit 1
fi

translations_directory="$git_directory/translations"

if [ ! -d "$translations_directory" ]; then
    echo "Directory does not exist: '$translations_directory'. Aborting."
    echo
    display_help
    exit 1
fi


echo 
echo "Sources directory: '$sources_directory'"
echo "Translations directory: '$translations_directory'"
echo

translation_files=`find "$translations_directory" -name "*\.ts" -type f`

for file in "$translation_files"; do
    lupdate "$sources_directory" -ts $file -noobsolete
done

echo
echo "Update successfully completed."

exit 0