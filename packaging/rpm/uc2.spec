%global upstream_version 3.0.0a13
%global upstream_tag v3.0.0-alpha.13
%if 0%{?rhel} == 9
%global test_python python3.11
%else
%global test_python python3
%endif

Name:           uc2
Version:        3.0.0~alpha.13
Release:        2%{?dist}
Summary:        Read and create UltraCompressor II archives
License:        GPL-3.0-or-later AND LGPL-3.0-only AND BSD-2-Clause
URL:            https://github.com/evvaletov/uc2
Source0:        %{url}/archive/refs/tags/%{upstream_tag}.tar.gz
BuildRequires:  gcc
BuildRequires:  cmake
BuildRequires:  make
BuildRequires:  %{test_python} >= 3.11
BuildRequires:  diffutils
Requires:       python3

%description
UC2 is a C99 revival of UltraCompressor II. It reads original DOS archives
and creates classic or modern chunked archives. This is an alpha release;
keep original archives and test new builds on copies. The package includes
the command-line tool, manual and optional Midnight Commander integration.

%prep
echo '41566267ca92d36cf181b6c4fc5878b53050b4aebf43baba7d16bdf60149ed93  %{SOURCE0}' | sha256sum -c -
%autosetup -n uc2-3.0.0-alpha.13

%build
%cmake -DUC2_VERSION_LABEL=%{upstream_version} -DUC2_BUILD_TESTS=ON -DPython3_EXECUTABLE=/usr/bin/%{test_python}
%cmake_build

%install
%cmake_install
install -m644 contrib/mc/README.md %{buildroot}%{_datadir}/uc2/mc/README.md

%check
(cd "%{_vpath_builddir}" && ctest --output-on-failure -E roundtrip_dosbox)
cli="$PWD/%{_vpath_builddir}/cli/uc2"
trial=$(mktemp -d)
trap 'rm -rf "$trial"' EXIT
printf 'UC2 RPM round trip\n' > "$trial/input.txt"
"$cli" -qw -L4 "$trial/classic.uc2" "$trial/input.txt"
"$cli" -t "$trial/classic.uc2"
"$cli" -p "$trial/classic.uc2" > "$trial/output.txt"
cmp "$trial/input.txt" "$trial/output.txt"
"$cli" -qw -L9 --chunked "$trial/modern.uc2" "$trial/input.txt"
"$cli" -t "$trial/modern.uc2"
"$cli" -p "$trial/modern.uc2" > "$trial/output.txt"
cmp "$trial/input.txt" "$trial/output.txt"

%files
%license LICENSE LICENSES/LGPL-3.0-only.txt LICENSES/BSD-2-Clause.txt
%doc README.md CREDITS.md
%{_bindir}/uc2
%{_mandir}/man1/uc2.1*
%dir %{_libexecdir}/mc
%dir %{_libexecdir}/mc/extfs.d
%{_libexecdir}/mc/extfs.d/uc2
%{_datadir}/uc2/

%changelog
* Sat Nov 21 2026 Eremey Valetov <eremeyvvaletov@gmail.com> - 3.0.0~alpha.13-2
- Extend package validation to Fedora 44/45 and AlmaLinux 9/10 on x86_64 and aarch64.

* Sat Nov 21 2026 Eremey Valetov <eremeyvvaletov@gmail.com> - 3.0.0~alpha.13-1
- Package alpha13 with archive round-trip checks and the native test suite.
