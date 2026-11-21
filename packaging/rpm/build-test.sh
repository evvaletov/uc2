#!/bin/bash
set -euo pipefail

# This installs and removes packages; run only in a fresh disposable container.
if [[ $# != 1 || $1 != /* || $EUID != 0 ]] ||
   [[ ! -f /.dockerenv && ! -f /run/.containerenv ]]; then
  echo 'Usage: build-test.sh /absolute/new-output-directory (root in a disposable container)' >&2
  exit 2
fi
if rpm -q uc2 >/dev/null 2>&1; then
  echo 'Refusing to replace an existing UC2 installation.' >&2
  exit 2
fi
output=$1
mkdir "$output"
repo=$(cd "$(dirname "$0")/../.." && pwd)
arch=$(uname -m)
[[ $arch == x86_64 || $arch == aarch64 ]]
[[ $arch == "${UC2_RPM_EXPECT_ARCH:-$arch}" ]]

# Request the executable so an installed curl-minimal also satisfies the dependency.
test_python=python3
if [[ $(rpm --eval '%{?rhel}') == 9 ]]; then test_python=python3.11; fi
dnf -y --setopt=install_weak_deps=False install \
  rpm-build redhat-rpm-config cmake gcc make python3 "$test_python" diffutils tar \
  /usr/bin/curl /usr/sbin/runuser shadow-utils
useradd -m uc2builder
work=$(mktemp -d /tmp/uc2-rpm.XXXXXX)
chown uc2builder:uc2builder "$work"

build() {
  local alpha=$1 top="$work/alpha$1"
  install -d -o uc2builder -g uc2builder "$top" "$top/SOURCES"
  python3 - "$repo/packaging/rpm/uc2.spec" "$top/uc2.spec" "$alpha" <<'PY'
from pathlib import Path
import sys
recipe = Path(sys.argv[1]).read_text()
if sys.argv[3] == "12":
    # The earlier version is an upgrade fixture, never a release artefact.
    recipe = recipe.replace("3.0.0a13", "3.0.0a12").replace("alpha.13", "alpha.12")
    recipe = recipe.replace("%global native_test_count 36", "%global native_test_count 35")
    recipe = recipe.replace(
        "41566267ca92d36cf181b6c4fc5878b53050b4aebf43baba7d16bdf60149ed93",
        "a596f5758c2985bf288aab487e4bd61d01a465c6a1f1c396fb491ec621c748bd")
Path(sys.argv[2]).write_text(recipe)
PY
  curl -fL "https://github.com/evvaletov/uc2/archive/refs/tags/v3.0.0-alpha.$alpha.tar.gz" \
    -o "$top/SOURCES/v3.0.0-alpha.$alpha.tar.gz"
  runuser -u uc2builder -- rpmbuild -ba --define "_topdir $top" \
    --define '_smp_build_ncpus 4' "$top/uc2.spec" 2>&1 | tee "$output/alpha$alpha-build.log"
}
build 12
build 13

mapfile -t old_packages < <(find "$work/alpha12/RPMS" -name "uc2-3.0.0~alpha.12-2.*.$arch.rpm")
mapfile -t packages < <(find "$work/alpha13/RPMS" -name "uc2-3.0.0~alpha.13-2.*.$arch.rpm")
mapfile -t sources < <(find "$work/alpha13/SRPMS" -name 'uc2-3.0.0~alpha.13-2.*.src.rpm')
[[ ${#old_packages[@]} == 1 && ${#packages[@]} == 1 && ${#sources[@]} == 1 ]]
old=${old_packages[0]}
package=${packages[0]}
source=${sources[0]}
trial=$(mktemp -d /tmp/uc2-rpm-lifecycle.XXXXXX)
# Recent RPM versions remove the build tree after a successful build.
tree="$trial/fixtures"
mkdir "$tree"
tar -xzf "$work/alpha13/SOURCES/v3.0.0-alpha.13.tar.gz" -C "$tree" --strip-components=1 \
  uc2-3.0.0-alpha.13/tests/archives/basic.uc2 uc2-3.0.0-alpha.13/tests/corpus
printf 'User archives survive package removal.\n' > "$trial/keep.txt"
cp "$trial/keep.txt" "$trial/expected.txt"

check_installed() {
  local dest="$trial/$1"
  mkdir "$dest"
  rpm -V uc2
  [[ $(rpm -q --qf '%{VERSION} %{ARCH}' uc2) == "3.0.0~alpha.13 $arch" ]]
  uc2 -h > "$dest/help.txt"
  grep -F 'UC2 3.0.0a13 ' "$dest/help.txt"
  uc2 -t "$tree/tests/archives/basic.uc2"
  mkdir "$dest/restored"
  uc2 -d "$dest/restored" "$tree/tests/archives/basic.uc2"
  for file in "$tree"/tests/corpus/*; do
    cmp "$file" "$dest/restored/${file##*/}"
  done
  /usr/libexec/mc/extfs.d/uc2 list "$tree/tests/archives/basic.uc2" > "$dest/mc-list.txt"
  grep -F 'hello.txt' "$dest/mc-list.txt"
  /usr/libexec/mc/extfs.d/uc2 copyout "$tree/tests/archives/basic.uc2" hello.txt "$dest/mc-hello.txt"
  cmp "$tree/tests/corpus/hello.txt" "$dest/mc-hello.txt"
  if [[ $test_python == python3.11 ]]; then
    head -n 1 /usr/libexec/mc/extfs.d/uc2 | grep -Fx '#!/usr/bin/python3'
    if rpm -q --requires uc2 | grep -F 'python3.11'; then
      echo 'Build interpreter leaked into runtime requirements.' >&2
      return 1
    fi
  fi
  for level in 4 9; do
    args=()
    if [[ $level == 9 ]]; then args=(--chunked); fi
    uc2 -qw -L"$level" "${args[@]}" "$dest/level$level.uc2" "$trial/keep.txt"
    uc2 -t "$dest/level$level.uc2"
    uc2 -p "$dest/level$level.uc2" > "$dest/level$level.txt"
    cmp "$trial/keep.txt" "$dest/level$level.txt"
    sha256sum "$dest/level$level.uc2" >> "$trial/archives.sha256"
  done
}

remove_package() {
  dnf -y remove uc2
  if rpm -q uc2 >/dev/null 2>&1; then
    echo 'UC2 is still registered after removal.' >&2
    return 1
  fi
  test ! -e /usr/bin/uc2
  test ! -e /usr/libexec/mc/extfs.d/uc2
  cmp "$trial/keep.txt" "$trial/expected.txt"
  sha256sum -c "$trial/archives.sha256"
}

dnf -y --setopt=localpkg_gpgcheck=0 install "$package"
check_installed fresh
remove_package
dnf -y --setopt=localpkg_gpgcheck=0 install "$old"
uc2 -h > "$trial/old-help.txt"
grep -F 'UC2 3.0.0a12 ' "$trial/old-help.txt"
dnf -y --setopt=localpkg_gpgcheck=0 upgrade "$package"
check_installed upgraded
remove_package
dnf -y --setopt=localpkg_gpgcheck=0 install "$package"
check_installed reinstalled
remove_package

python3 - "$output" "$package" "$source" "$arch" <<'PY'
from pathlib import Path
import hashlib, json, shutil, subprocess, sys
out = Path(sys.argv[1])
artifacts = []
for source in map(Path, sys.argv[2:4]):
    target = out / source.name.replace("3.0.0~alpha.13", "3.0.0a13")
    shutil.copyfile(source, target)
    artifacts.append({"name": target.name, "size": target.stat().st_size,
                      "sha256": hashlib.sha256(target.read_bytes()).hexdigest()})
report = {
    "architecture": sys.argv[4],
    "os_release": Path("/etc/os-release").read_text(),
    "rpm_version": subprocess.check_output(["rpm", "--version"], text=True).strip(),
    "source_tag": "v3.0.0-alpha.13",
    "source_sha256": "41566267ca92d36cf181b6c4fc5878b53050b4aebf43baba7d16bdf60149ed93",
    "native_builds_and_tests": "PASS for alpha12 upgrade fixture and alpha13",
    "lifecycle": "PASS: fresh install, alpha12 upgrade, removal and reinstallation",
    "validation": "rpm -V, six byte-identical fixture files, installed MC helper, L4/L9 round trips, user files preserved",
    "signature": "unsigned standalone RPMs",
    "artifacts": artifacts,
}
report['test_runs'] = {}
for alpha in (12, 13):
    lines = (out / f'alpha{alpha}-build.log').read_text().splitlines()
    results = [json.loads(line.removeprefix('UC2_NATIVE_TESTS '))
               for line in lines if line.startswith('UC2_NATIVE_TESTS ')]
    if len(results) != 1:
        raise SystemExit('Expected one native test report per build')
    report['test_runs'][f'alpha{alpha}'] = results[0]
(out / "validation.json").write_text(json.dumps(report, indent=2) + "\n")
PY
printf 'RPM_MATRIX_LIFECYCLE_PASS %s\n' "$arch"
