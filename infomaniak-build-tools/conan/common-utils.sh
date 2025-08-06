if [[ "${BASH_SOURCE[0]}" -ef "$0" ]]; then
    echo "This script must be sourced, not executed."
    exit 1
fi

find_conan_dependency_path() {
  local build_dir="$1"
  local origin_dep_name="${2}"
  local dep_name="$(echo "$origin_dep_name" | cut -c1-5)" # Crop the name of the dependency to the first 5 characters to match the Conan package naming convention (folder ex: /home/user/.conan2/p/log4c6abc1234def/p for the dependency log4cplus)

  [[ -z "$build_dir" ]] && { echo "Error: No build directory specified to the 'find_conan_dependency_path' function." >&2; exit 1; }
  [[ ! -d "$build_dir" ]] && { echo "Error: The specified build directory '$build_dir' does not exist." >&2; exit 1; }
  [[ -z "$origin_dep_name" ]] && { echo "Error: No dependency name specified to the 'find_conan_dependency_path' function." >&2; exit 1; }

  local sourced=0
  local conan_path=""

  # If the LD_LIBRARY_PATH does not contain the ".conan2" folder, we need to source the conanrun.sh script to set it up
  if [[ "$LD_LIBRARY_PATH" != *"/.conan2/"* ]]; then
    conan_path="$(dirname "$(find "$build_dir" -iname "conanrun.sh" -type f | head -n 1)")"
    if [[ -n "$conan_path" && -f "$conan_path/conanrun.sh" ]]; then
      source "$conan_path/conanrun.sh"
      sourced=1
    else
      echo "Error: Could not find 'conanrun.sh' in the build directory '$build_dir'." >&2
      exit 1
    fi
  fi

  # Special handling for Zsh to ensure proper word splitting on macOS
  if [[ -n "$ZSH_VERSION" ]]; then
    if setopt | grep -Fxq shwordsplit; then # Here we check if shwordsplit is set in Zsh because we need it to split the LD_LIBRARY_PATH correctly
      was_shwordsplit_set=1
    else
      was_shwordsplit_set=0
      setopt shwordsplit
    fi
  fi

  # Split LD_LIBRARY_PATH using colon as delimiter into an array
  local old_IFS="$IFS"
  IFS=':'
  local path_dirs=()
  path_dirs=($LD_LIBRARY_PATH)
  IFS="$old_IFS"
  [[ -n "$ZSH_VERSION" && "$was_shwordsplit_set" == 0 ]] && unsetopt shwordsplit # Restore the previous state of shwordsplit in Zsh

  # Define regex pattern to match Conan package paths in LD_LIBRARY_PATH.
  # This recognize Conan package paths that are either in the form of:
  # - /home/user/.conan2/p/log4c6abc1234def
  # - /home/user/.conan2/p/b/log4c6abc1234def
  # and ends with "/lib". The "b/" part is optional, as it is used for packages that are built on the machine
  local conan_package_folder_regex="\.conan2/p/(b/)?${dep_name}[^/]*/p/lib$"

  # Search through each directory in LD_LIBRARY_PATH for a matching Conan path
  for dir in "${path_dirs[@]}"; do
    if echo "$dir" | grep -qE "$conan_package_folder_regex"; then
      echo "${dir%/lib}" # Remove the trailing "/lib" from the path to get the full Conan package path
      # Deactivate Conan environment if it was sourced
      [[ $sourced -eq 1 ]] && [[ -f "$conan_path/deactivate_conanrun.sh" ]] && source "$conan_path/deactivate_conanrun.sh" > /dev/null 2>&1
      return 0
    fi
  done

  # If no match is found, output an error and deactivate Conan environment if necessary
  echo "Error: Could not find the $origin_dep_name($dep_name...)Conan package path in LD_LIBRARY_PATH='$LD_LIBRARY_PATH'." >&2
  [[ $sourced -eq 1 ]] && [[ -f "$conan_path/deactivate_conanrun.sh" ]] && source "$conan_path/deactivate_conanrun.sh" > /dev/null 2>&1
  return 1
}

find_qt_conan_path() {
  find_conan_dependency_path "$1" "qt"
}