#!/usr/bin/env python3

"""
CLI tool to check whether the status of a repository has changed wrt a previous reading.
Git and mercurial repositories are supported. The git commands are tried at first. If the repository is a mercurial
clone those commands will fail, and equivalent mercurial commands will be tried instead.

Usage:

check-repo-src-changed.py [-h] --status-dirpath STATUS_DIRPATH --repo-dirpath REPO_DIRPATH

where:
- STATUS_DIRPATH: is the path to the directory where the previous status of the repository
   to check against is stored. A new status is computed and stored in this directory if one of the following happens:
     * the directory does not exist, or the status is not initialized;
     * the status is corrupted (i.e. hash of the content of the status directory does not match the content of
       `hash.txt`);
     * the status of the repository has changed.
- REPO_DIRPATH: path to the Git (or mercurial) repository whose status is of interest.

The status directory is defined as 'initialized' if there is the `hash.txt` file in it.
The status directory is defined as 'corrupted' if it is initialized and the SHA1 of the content of the directory,
 except `hash.txt`, does not match the content of `hash.txt`.
The status directory has to be updated if, it is initialized, not corrupted, and one of the following is true:
- the content of the file `commit-hash.txt` is different from the HEAD commit of the target repository;
- the content of the file `diff.txt` is different from the output of `git status -vv` executed in the target repository.

REQUIREMENTS:
This script requires python >= 3.8.

Tutorial:

Let path_to_git_repo be a repository we want to monitor the status with this tool. Let path_to_status_dir be the
 chosen path of the status directory.

The script is called as follows:

    python check-repo-src-changed.py --status-dirpath ${path_to_status_dir} --repo-dirpath ${path_to_git_repo}

The directory at path path_to_git_repo is created, populated with the "${path_to_status_dir}/commit-hash.txt" file
 (the output of `git rev-parse HEAD` executed in path_to_git_repo) and the "${path_to_status_dir}/diff.txt" file (the
 output of `git status -vv` executed in path_to_git_repo). Finally, the SHA1 of the current content of the status
 directory is computed, and stored in "${path_to_status_dir}/hash.txt".

Calling again the same command will print "repository status has not changed.".

After manually removing the ${path_to_status_dir}/hash.txt file, the command will print  "repository status not
 initialized" and the computation of the status is triggered again.

After changing any of the content of the files in ${path_to_status_dir} file, the command will print "previous
 repository status is corrupted.", and the computation of the status is triggered again.

Changing the state of the target repository will print "repository status has changed.", and the computation of the
 status is triggered again.

EXIT CODES:
- 0 if the script succeeded, and no files were modified since the last execution
- 47 if the script succeeded and some modifications were detected
- other exit codes are to be interpreted as fatal errors that have to block further processing
"""
from __future__ import annotations

import argparse
import dataclasses
import functools
import hashlib
import shutil
import string
import subprocess
import sys
from pathlib import Path
from typing import Callable, Optional, Sequence, Set


if sys.version_info < (3, 8):
    raise ValueError("this script requires Python at version 3.8 or later")


def compose(*fs: Callable) -> Callable:
    """
    Compose a sequence of callables.

    :param fs: the sequence of callables.
    :return: the composition of the callables.
    """

    def _compose(f: Callable, g: Callable) -> Callable:
        return lambda *args, **kwargs: f(g(*args, **kwargs))

    return functools.reduce(_compose, fs)


def check_path_is_dir(path: Path) -> Path:
    """
    Parse path of directory.

    :param path: the path object
    :return: the argument
    """
    path = Path(path).resolve()
    if not path.is_dir():
        raise argparse.ArgumentTypeError(f"path {path} is not a directory")
    return path


def check_path_exists(path: Path) -> Path:
    """
    Check that the path exists.

    :param path: the path object
    :return: the argument
    """
    path = Path(path).resolve()
    if not path.exists():
        raise argparse.ArgumentTypeError(f"path {path} does not exist")
    return path


def check_git_installed() -> None:
    """
    Check that the 'git' executable is accessible from the system path (using shutil.which).

    It raises RuntimeError in case `which git` returns error.
    """
    if shutil.which("git") is None:
        raise RuntimeError("'git' not installed.")


def is_relative_to(this: Path, other: Path) -> bool:
    """Check 'this' path is relative to 'other' path."""
    try:
        this.relative_to(other)
        return True
    except ValueError:
        return False


def run_subprocess(command: Sequence[str], cwd: Path) -> subprocess.CompletedProcess:
    """Run a command in a subprocess and get stdout"""
    exec_result = subprocess.run(
        command,
        cwd=cwd,
        timeout=5,
        text=True,
        capture_output=True,
    )
    return exec_result


def run_git_or_mercurial_and_return_stdout(
    git_command: Sequence[str], hg_command: Sequence[str], cwd: Path
) -> str:
    """
    Tries to run git_command.
    If mercurial is installed, and the git command fails with a "not a git
    repository" error, tries to run hg_command.
    """
    git_res = run_subprocess(git_command, cwd)
    if git_res.returncode == 0:
        return git_res.stdout
    if (shutil.which("hg") is None) or git_res.returncode != 128:
        raise RuntimeError(
            f"Command {git_command} failed with code {git_res.returncode} and the following stderr: {git_res.stderr}"
        )
    if "not a git repository" not in git_res.stderr:
        raise RuntimeError(
            f"Command {git_command} failed with code {git_res.returncode}, but stderr did not contain 'not a git repository': {git_res.stderr}"
        )
    # if we arrive here, we have to try mercurial
    hg_res = run_subprocess(hg_command, cwd)
    if hg_res.returncode == 0:
        return hg_res.stdout
    raise RuntimeError(
        f"Unable to execute any of the following commands in {cwd}: {git_command}, {hg_command} (rc: {hg_res.returncode}, stderr: {hg_res.stderr})"
    )


class HashString(str):
    """A hash string."""

    def __new__(cls, s: str) -> HashString:
        string_object = super().__new__(cls, s.strip())
        if not cls.is_hash(string_object):
            raise ValueError(f"not a hash: {string_object}")
        return string_object

    @staticmethod
    def is_hash(hash_str: str) -> bool:
        """
        Return true if the input is a hexadecimal string of length 40, False otherwise.

        :param hash_str: the
        :return: Return true if the input is a hexadecimal string of length 40, False otherwise.
        """
        return len(hash_str) == 40 and all(c in string.hexdigits for c in hash_str)


def path_checksum(path: Path, skip_paths: Optional[Set] = None) -> HashString:
    """
    Recursively calculates a checksum representing the contents of all files
    found with a sequence of file and/or directory paths.

    Adapted from https://code.activestate.com/recipes/576973-getting-the-sha-1-or-md5-hash-of-a-directory/#c3
    """
    assert path.exists()
    skip_paths = set() if skip_paths is None else skip_paths

    def _update_checksum(checksum, filepath: Path):
        with filepath.open(mode="rb") as fh:
            while True:
                buf = fh.read(4096)
                if not buf:
                    break
                checksum.update(buf)
            fh.close()

    chksum = hashlib.sha1()

    for child in sorted([f.resolve() for f in path.rglob("*")]):
        if child in skip_paths:
            continue
        if child.is_file():
            _update_checksum(chksum, child)

    return HashString(chksum.hexdigest())


def parse_args() -> argparse.Namespace:
    """Parse the CLI arguments."""
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--status-dirpath",
        required=True,
        type=compose(Path.resolve, Path),
        help="Path to the directory to store the previous status of the directory",
    )
    parser.add_argument(
        "--repo-dirpath",
        required=True,
        type=compose(Path.resolve, check_path_exists, check_path_is_dir, Path),
        help="Path to the repository root directory.",
    )
    return parser.parse_args()


@dataclasses.dataclass(frozen=True)
class RepoStatus:
    """Class to represent a status directory."""

    repo_dirpath: Path
    status_dirpath: Path

    def __post_init__(self) -> None:
        """Do post-initialization checks."""
        assert self.repo_dirpath.is_absolute()
        assert self.status_dirpath.is_absolute()
        assert not is_relative_to(
            self.repo_dirpath, self.status_dirpath
        ), "repo dirpath must not be relative to status dirpath"
        assert not is_relative_to(
            self.status_dirpath, self.repo_dirpath
        ), "status dirpath must not be relative to repo dirpath"

    @property
    def commit_filepath(self) -> Path:
        """Get the path to the commit hash file."""
        return self.status_dirpath / "commit-hash.txt"

    @property
    def diff_filepath(self) -> Path:
        """Get the path to the diff file."""
        return self.status_dirpath / "diff.txt"

    @property
    def hash_filepath(self) -> Path:
        """Get the path to the hash file."""
        return self.status_dirpath / "hash.txt"

    @property
    def stored_status_dir_hash(self) -> HashString:
        """Get the content of the hash file."""
        return HashString(self.hash_filepath.read_text())

    @property
    def stored_commit(self) -> HashString:
        """Get the content of the hash file."""
        return HashString(self.commit_filepath.read_text())

    @property
    def stored_git_diff(self) -> str:
        """Get the content of the diff file."""
        return self.diff_filepath.read_text()

    def compute_hash(self) -> HashString:
        """Compute the hash of the status directory."""
        return path_checksum(self.status_dirpath, skip_paths={self.hash_filepath})

    @property
    def is_initialized(self) -> bool:
        """Return true iff hash file is in a correct state in status_dirpath."""
        if not self.hash_filepath.exists():
            return False
        hash_file_content = self.hash_filepath.read_text()
        try:
            HashString(hash_file_content)
        except ValueError:
            return False
        return True

    @property
    def is_corrupted(self) -> bool:
        """
        Return true if the status directory is corrupted.

        It checks if the hash of the status directory matches the stored hash.
        Requires the stauts directory to be initialized.

        :return: True if it is corrupted
        :raises ValueError: if it is not initialized.
        """
        if not self.is_initialized:
            raise ValueError(
                "status directory not initialized, cannot check corruption"
            )
        actual_hash = self.compute_hash()
        expected_hash = self.stored_status_dir_hash
        return actual_hash != expected_hash

    def get_current_commit_hash(self) -> HashString:
        """Get the hash of the commit currently checked out in the target repository."""
        stdout = run_git_or_mercurial_and_return_stdout(
            ["git", "rev-parse", "HEAD"],
            ["hg", "identify", "--rev", ".", "--template", "{id}\n"],
            self.repo_dirpath,
        )
        return HashString(stdout)

    def get_current_git_diff(self) -> str:
        """Get current git diff status of the target repository."""
        stdout = run_git_or_mercurial_and_return_stdout(
            ["git", "status", "-vv"],
            ["hg", "status", "--verbose"],
            self.repo_dirpath,
        )
        return stdout


def update_status(repo_status: RepoStatus) -> None:
    """
    Update the repository status in the configured status directory.

    Steps:
    - make the status directory if it does not exist
    - get the current commit hash and store it in `commit-hash.txt`
    - get the git diff and store it in `diff.txt`
    - compute the hash of the status directory content and store it in `hash.txt`

    :param repo_status: the RepoStatus object.
    :return: None
    """
    repo_status.status_dirpath.mkdir(parents=True, exist_ok=True)
    commit_hash = repo_status.get_current_commit_hash()
    git_diff = repo_status.get_current_git_diff()
    repo_status.commit_filepath.write_text(f"{commit_hash}\n")
    repo_status.diff_filepath.write_text(git_diff)

    # compute hash of status directory
    new_hash = repo_status.compute_hash()
    repo_status.hash_filepath.write_text(f"{new_hash}\n")


def is_status_the_same(repo_status: RepoStatus) -> bool:
    """
    Check whether the actual status of the repository matches the expected status.

    Steps:
    - check whether the expected commit hash is the same of the current commit hash of the repository.
       If not, then return False.
    - check whether the previously stored output of `git status -vv` matches the current one. If not, then return False.
    - else, return True

    :param repo_status: the RepoStatus object.
    :return: True if the status is the same, False otherwise.
    """
    expected_commit_hash = repo_status.stored_commit
    actual_commit_hash = repo_status.get_current_commit_hash()
    if expected_commit_hash != actual_commit_hash:
        # commit hash changed, status has (very likely) changed
        return False

    expected_diff = repo_status.stored_git_diff
    actual_diff = repo_status.get_current_git_diff()
    if expected_diff != actual_diff:
        # git-diff returned different outcomes
        return False

    return True


if __name__ == "__main__":
    arguments = parse_args()
    check_git_installed()

    repo_status = RepoStatus(arguments.repo_dirpath, arguments.status_dirpath)

    exit_code: int
    if not repo_status.is_initialized:
        print(f"repository status not initialized. Repo dir: {arguments.repo_dirpath}, status dir: {arguments.status_dirpath}")
        exit_code = 47
    elif repo_status.is_corrupted:
        print(f"previous repository status is corrupted. Repo dir: {arguments.repo_dirpath}, status dir: {arguments.status_dirpath}")
        exit_code = 47
    elif not is_status_the_same(repo_status):
        print(f"repository status has changed. Repo dir: {arguments.repo_dirpath}, status dir: {arguments.status_dirpath}")
        exit_code = 47
    else:
        print(f"repository status has not changed. Repo dir: {arguments.repo_dirpath}, status dir: {arguments.status_dirpath}")
        exit_code = 0

    if exit_code != 0:
        update_status(repo_status)

    sys.exit(exit_code)
