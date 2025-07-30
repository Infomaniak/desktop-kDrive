if [ "${BASH_SOURCE[0]}" -ef "$0" ]; then
    echo "This script must be sourced, not executed."
    exit 1
fi

find_conan_dependency_path() {
  local build_dir="$1"
  local origin_dep_name="${2}"
  local dep_name="${origin_dep_name:0:5}" # Crop the name of the dependency to the first 5 characters to match the Conan package naming convention (folder ex: /home/user/.conan2/p/log4c6abc1234def/p for the dependency log4cplus)

  [[ -z "$build_dir" ]] && { echo "Error: No build directory specified to the 'find_conan_dependency_path' function." >&2; exit 1; }
  [[ ! -d "$build_dir" ]] && { echo "Error: The specified build directory '$build_dir' does not exist." >&2; exit 1; }
  [[ -z "$origin_dep_name" ]] && { echo "Error: No dependency name specified to the 'find_conan_dependency_path' function." >&2; exit 1; }

  local sourced=0
  local conan_path=""

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

  local conan_package_folder_regex="\.conan2/p/(b/)?${dep_name}[^/]*/p/lib$"
  IFS=':' read -ra path_dirs <<< "$LD_LIBRARY_PATH"
  for dir in "${path_dirs[@]}"; do
    if [[ "$dir" =~ $conan_package_folder_regex ]]; then
      echo "${dir:0:-4}" # Remove the trailing "/lib" from the path
      [[ $sourced -eq 1 ]] && [[ -f "$conan_path/deactivate_conanrun.sh" ]] && source "$conan_path/deactivate_conanrun.sh" > /dev/null 2>&1
      return 0
    fi
  done

  echo "Error: Could not find the $origin_dep_name($dep_name...)Conan package path in LD_LIBRARY_PATH='$LD_LIBRARY_PATH'." >&2
  [[ $sourced -eq 1 ]] && [[ -f "$conan_path/deactivate_conanrun.sh" ]] && source "$conan_path/deactivate_conanrun.sh" > /dev/null 2>&1
  return 1
}

find_qt_conan_path() {
  find_conan_dependency_path "$1" "qt"
}