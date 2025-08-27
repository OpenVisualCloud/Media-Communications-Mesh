#!/bin/bash

# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024 Intel Corporation

script_dir="$(readlink -f "$(dirname "${BASH_SOURCE[0]}")")"
repository_root="$(readlink -f "${script_dir}/../..")"

# shellcheck source="SCRIPTDIR/../../scripts/common.sh"
. "${repository_root}/scripts/common.sh"

allow_non_ascii_filenames="false"

function get_main_sha()
{
  log_info "Getting origin/main commit SHA"
  git_origin_main=$(git rev-parse --verify origin/main)
  log_info "running against origin/master=${git_origin_main}"
  echo "$git_origin_main"
}

function get_head_sha()
{
  log_info "Getting HEAD commit SHA"
  if git rev-parse --verify HEAD >/dev/null 2>&1; then
    git_current_hash=$(git rev-parse --verify HEAD)
  else
    echo "This is first commit, nothing to check, exiting"
    exit 0
  fi
  log_info "running against HEAD=${git_current_hash}"
  echo "${git_current_hash}"
}

function check_nonascii_files()
{
  local github_origin_main="$1"
  local github_current_hash="$2"
  local allow_non_ascii=${3:-false}

  if [ "$allow_non_ascii" == "false" ]
  then
    if test "$(git diff --diff-filter=AR --relative --name-only -z "${github_origin_main}" "${github_current_hash}" | LC_ALL=C tr -d '[ -~]\0' | wc -c)" != 0
    then
      cat <<EOF
Error: You attempted to add a non-ASCII file name.
       This is not allowed in Cloud Native Data Services repository.
       To be portable it is mandatory for you to rename all the file.
EOF
    fi
  fi
}

function check_file_subdir_type()
{
  local modified_file="$1"
  local fields_range=${2:-'1'}

  if [ -z "$modified_file" ]
  then
    printf "Function check_file_subdir_type requires exactly 1 valid argument." 1>&2
    exit 1
  fi
  printf "%s" "${modified_file}" | cut -d'/' "-f${fields_range}"
}

function files_subdir_types()
{
  local github_origin_main="$1"
  local github_current_hash="$2"

# diff-filter params, uppercase include, lowercase exclude:
# Added (A), Copied (C), Deleted (D), Modified (M), Renamed (R), changed (T), Unmerged (U), Unknown (X), pairing Broken (B)
  modified_file_list="$(git diff --diff-filter=dxb --relative --name-only -z "${github_origin_main}" "${github_current_hash}" | xargs -0)"

  for pt in $modified_file_list
  do
    modified_file="$(readlink -f "${pt}")"

    [ -d "$modified_file" ] && modified_dir="$modified_file" || modified_dir="$(dirname "$modified_file")";
    case $(check_file_subdir_type "${modified_file}") in
      deployment)
        deployment_subdir_check "${modified_file}" "${modified_dir}"
        ;;

      config)
        configuration_subdir_check "${modified_file}"
        ;;

      docs)
        documentation_subdir_check "${modified_file}"
        ;;

      .github)
        github_workflow_subdir_check "${modified_file}"
        ;;

      tests|scripts)
        ansible_subdir_check "${modified_file}"
        ;;

      *)
        if echo "${modified_file}" | grep --silent ".*\.sh\$"; then
          shell_script_file_check "${modified_file}"
        elif echo "${modified_file}" | grep --silent ".*\.py\$"; then
          python_script_file_check "${modified_file}"
        elif echo "${modified_file}" | grep --silent "\(\.yaml\$\)\|\(\.yml\$\)"; then
          ansible_subdir_check "${modified_file}"
        else
          other_file_check "${modified_file}"
        fi
        ;;
    esac
  done
}

function images_subdir_check {
  local filepath="$1"
  shift
  log_info "Dockerfiles images subdirectory. ${filepath}"
}

function deployment_subdir_check() {
  local filepath="$1"
  local dirpath="$2"
  shift; shift;
  log_info "Helm Charts deployment subdirectory. ${filepath}"
  helm lint "$dirpath" 1>&2 || true
}

function ansible_subdir_check() {
  local filepath="$1"
  local dirpath="$2"
  shift; shift;
  echo "ansible roles and playbooks subdirectory. ${filepath}"
  ansible-lint "$filepath" 1>&2 || true
}

function inventories_subdir_check() {
  local filepath="$1"
  shift
  log_info "inventories files subdirectory. ${filepath}"
}

function configuration_subdir_check() {
  local filepath="$1"
  shift
  log_info "configuration files subdirectory. ${filepath}"
}

function documentation_subdir_check() {
  local filepath="$1"
  shift
  log_info "documentation files and styles subdirectory. ${filepath}"
}

function github_workflow_subdir_check() {
  local filepath="$1"
  shift
  log_info "GitHub workflows subdirectory. ${filepath}"
}

function shell_script_file_check() {
  local filepath="$1"
  shift
  log_info "Shell script file path. ${filepath}"
  shellcheck -f tty "${repository_root}/$filepath" 1>&2
}

function python_script_file_check() {
  local filepath="$1"
  shift
  log_info "Python script file path. ${filepath}"
}

function other_file_check() {
  local filepath="$1"
  shift
  log_info "Other file path, not categorized. ${filepath}"
}

function start_git_head_parsing()
{
  pushd "${repository_root}" || exit 10
  git_current_hash="$(get_head_sha)"
  git_origin_main="$(get_main_sha)"
  check_nonascii_files "$git_origin_main" "$git_current_hash" "${allow_non_ascii_filenames}"
  files_subdir_types "$git_origin_main" "$git_current_hash" || true
  popd || exit 11
}

start_git_head_parsing
